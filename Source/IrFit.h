#pragma once
// Reverb Anatomy Lab 0.7.0: IR -> algorithm, closed loop (the light version of the offline fitter).
// The plugin renders its own IR with candidate settings, measures it like the target IR and moves the settings
// (Nelder-Mead per group of parameters). Measures: T30 / EDT per octave (Schroeder, noise-truncated), C80,
// the energy envelope 0..400 ms, the 1/3-octave spectrum (early / late), late IACC per band.
// Copyright (c) 2026 Johan Plus Kei. All rights reserved.
#include "TailDsp.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <random>

namespace fit
{
constexpr double kSR = 48000;
constexpr int kOct = 7;
inline const double* octs() { static const double o[kOct] = { 125, 250, 500, 1000, 2000, 4000, 8000 }; return o; }
constexpr int kIacc = 5;
inline const double* iaccBands() { static const double o[kIacc] = { 250, 500, 1000, 2000, 4000 }; return o; }

struct Metrics
{
    bool valid = false;
    double t30[kOct] {}, edt[kOct] {}, iacc[kIacc] {}, c80 = 0, peakMs = 0;
    std::vector<double> env, specL, specE;          // env: 40 x 10 ms (dB re total); spectra: 1/3 oct 50 Hz .. 16 kHz (dB)
};

// ---- band filter: Butterworth 4th-order high pass + 4th-order low pass (one octave)
struct Band
{
    rv::BQ s[4];
    void set (double fc, double sr)
    {
        const double lo = fc / std::sqrt (2.0), hi = std::min (fc * std::sqrt (2.0), sr * 0.45);
        s[0].pass (true, lo, 0.5412, sr); s[1].pass (true, lo, 1.3066, sr); s[2].pass (false, hi, 0.5412, sr); s[3].pass (false, hi, 1.3066, sr);
    }
    inline float run (float x) { for (auto& b : s) x = b.run (x); return x; }
};

// EDC in dB (noise floor = the last 10 %, cut where the 10 ms average meets floor + 5 dB)
inline std::vector<double> edcDb (const std::vector<double>& e, double sr)
{
    const size_t n = e.size(); if (n < 16) return {};
    double noise = 0; size_t c0 = (size_t) (n * 0.9); for (size_t i = c0; i < n; ++i) noise += e[i]; noise /= std::max<size_t> (1, n - c0);
    const int w = std::max (1, (int) (0.01 * sr));
    double run = 0; size_t cut = n - 1; std::vector<double> sm (n, 0);
    for (size_t i = 0; i < n; ++i) { run += e[i]; if (i >= (size_t) w) run -= e[i - (size_t) w]; sm[i] = run / w; }
    for (size_t i = n; i-- > 0;) if (sm[i] > noise * 3.1623) { cut = i; break; }
    std::vector<double> d (cut + 1, 0); double acc = 0;
    for (size_t i = cut + 1; i-- > 0;) { acc += std::max (0.0, e[i] - noise); d[i] = acc; }
    const double tot = d[0] + 1e-30;
    for (auto& v : d) v = 10 * std::log10 (v / tot + 1e-30);
    return d;
}
inline double fitTime (const std::vector<double>& d, double a, double b, double sr)
{
    size_t i0 = 0, i1 = 0; bool f0 = false, f1 = false;
    for (size_t i = 0; i < d.size(); ++i) { if (! f0 && d[i] <= a) { i0 = i; f0 = true; } if (! f1 && d[i] <= b) { i1 = i; f1 = true; break; } }
    if (! f0 || ! f1 || i1 <= i0 + 4) return std::nan ("");
    double sx = 0, sy = 0, sxx = 0, sxy = 0; const double m = (double) (i1 - i0);
    for (size_t i = i0; i < i1; ++i) { const double t = i / sr, y = d[i]; sx += t; sy += y; sxx += t * t; sxy += t * y; }
    const double k = (m * sxy - sx * sy) / (m * sxx - sx * sx);
    return k < 0 ? -60.0 / k : std::nan ("");
}

inline Metrics measure (const float* L, const float* R, int n, double sr = kSR)
{
    Metrics m; if (n < (int) (0.1 * sr)) return m;
    std::vector<double> e ((size_t) n);
    for (int i = 0; i < n; ++i) e[(size_t) i] = (double) L[i] * L[i] + (double) R[i] * R[i];
    double tot = 0; for (auto v : e) tot += v; tot += 1e-30;
    // per octave: T30, EDT
    std::vector<double> eb ((size_t) n);
    for (int k = 0; k < kOct; ++k)
    {
        Band bl, br; bl.set (octs()[k], sr); br.set (octs()[k], sr);
        for (int i = 0; i < n; ++i) { const float l = bl.run (L[i]), r = br.run (R[i]); eb[(size_t) i] = (double) l * l + (double) r * r; }
        const auto d = edcDb (eb, sr);
        m.t30[k] = fitTime (d, -5, -35, sr); m.edt[k] = fitTime (d, -0.01, -10, sr);
    }
    const int n80 = std::min (n, (int) (0.08 * sr));
    double a = 0, z = 0; for (int i = 0; i < n; ++i) (i < n80 ? a : z) += e[(size_t) i];
    m.c80 = 10 * std::log10 (a / (z + 1e-30) + 1e-30);
    const int w10 = (int) (0.01 * sr);
    m.env.resize (40);
    for (int b = 0; b < 40; ++b) { double s = 0; for (int i = b * w10; i < (b + 1) * w10 && i < n; ++i) s += e[(size_t) i]; m.env[(size_t) b] = 10 * std::log10 (s / tot + 1e-12); }
    { const int w5 = (int) (0.005 * sr); double run = 0, best = -1; int bi = 0;
      for (int i = 0; i < std::min (n, (int) (0.5 * sr)); ++i) { run += e[(size_t) i]; if (i >= w5) run -= e[(size_t) (i - w5)]; if (run > best) { best = run; bi = i; } }
      m.peakMs = 1000.0 * bi / sr; }
    // spectra (1/3 octave, 50 Hz .. 16 kHz): late (80 ms ..) and early (.. 80 ms)
    {
        constexpr int order = 17, N = 1 << order;
        static thread_local std::unique_ptr<juce::dsp::FFT> fft; if (! fft) fft = std::make_unique<juce::dsp::FFT> (order);
        std::vector<float> buf ((size_t) (2 * N)); std::vector<double> pw ((size_t) (N / 2));
        auto spec = [&] (int from, int to, std::vector<double>& out)
        {
            std::fill (pw.begin(), pw.end(), 0.0);
            for (const float* x : { L, R })
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int i = from; i < std::min (to, from + N); ++i) buf[(size_t) (i - from)] = x[i];
                fft->performFrequencyOnlyForwardTransform (buf.data());
                for (int b = 0; b < N / 2; ++b) pw[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b];
            }
            out.clear();
            for (double fc = 50; fc <= 16100; fc *= std::pow (2.0, 1.0 / 3.0))
            {
                const int b0 = std::max (1, (int) (fc / std::pow (2.0, 1.0 / 6.0) * N / sr)), b1 = std::max (b0, (int) (fc * std::pow (2.0, 1.0 / 6.0) * N / sr));
                double s = 0; for (int b = b0; b <= b1 && b < N / 2; ++b) s += pw[(size_t) b];
                out.push_back (10 * std::log10 (s / (b1 - b0 + 1) + 1e-30));
            }
        };
        spec (n80, n, m.specL); spec (0, n80, m.specE);
    }
    // late IACC per band (80 .. 880 ms, +-1 ms)
    {
        const int i0 = n80, i1 = std::min (n, n80 + (int) (0.8 * sr)), lag = (int) (0.001 * sr);
        std::vector<float> bl ((size_t) n), br ((size_t) n);
        for (int k = 0; k < kIacc; ++k)
        {
            Band fl, fr; fl.set (iaccBands()[k], sr); fr.set (iaccBands()[k], sr);
            for (int i = 0; i < i1; ++i) { bl[(size_t) i] = fl.run (L[i]); br[(size_t) i] = fr.run (R[i]); }
            double el = 0, er = 0; for (int i = i0; i < i1; ++i) { el += (double) bl[(size_t) i] * bl[(size_t) i]; er += (double) br[(size_t) i] * br[(size_t) i]; }
            const double den = std::sqrt (el * er) + 1e-30; double best = 0;
            for (int t = -lag; t <= lag; t += 2)
            {
                double s = 0;
                for (int i = std::max (i0, i0 - t); i < std::min (i1, i1 - t); ++i) s += (double) bl[(size_t) i] * br[(size_t) (i + t)];
                best = std::max (best, std::abs (s) / den);
            }
            m.iacc[k] = best;
        }
    }
    m.valid = true;
    return m;
}

struct Parts { double t30 = 0, edt = 0, env = 0, specL = 0, specE = 0, iacc = 0, c80 = 0; };
inline double compare (const Metrics& t, const Metrics& c, Parts* out = nullptr)
{
    if (! t.valid || ! c.valid) return 1e9;
    auto lr = [] (double a, double b) { return (std::isfinite (a) && std::isfinite (b) && a > 0 && b > 0) ? std::abs (std::log (b / a)) : 0.7; };
    Parts p; int nt = 0; double ne = 0;
    for (int k = 0; k < kOct; ++k) { if (std::isfinite (t.t30[k])) { p.t30 += lr (t.t30[k], c.t30[k]); ++nt; } if (std::isfinite (t.edt[k])) { const double we = (k == kOct - 1 && std::isfinite (t.t30[k]) && t.edt[k] < 0.5 * t.t30[k]) ? 0.2 : 1.0;   // (0.7.3: a top octave that is mostly noise floor has a meaningless EDT)
            p.edt += we * lr (t.edt[k], c.edt[k]); ne += we; } }
    p.t30 = nt ? p.t30 / nt / 0.05 : 0; p.edt = ne ? p.edt / ne / 0.08 : 0;
    { double s = 0; for (size_t i = 0; i < t.env.size() && i < c.env.size(); ++i) { const double d = juce::jlimit (-60.0, 0.0, t.env[i]) - juce::jlimit (-60.0, 0.0, c.env[i]); s += d * d; }
      p.env = std::sqrt (s / std::max<size_t> (1, t.env.size())) / 2.0; }
    auto shape = [] (const std::vector<double>& a, const std::vector<double>& b)
    {
        const size_t n = std::min (a.size(), b.size()); if (n == 0) return 0.0;
        double mean = 0; for (size_t i = 0; i < n; ++i) mean += a[i] - b[i]; mean /= n;
        double s = 0; for (size_t i = 0; i < n; ++i) { const double d = a[i] - b[i] - mean; s += d * d; }
        return std::sqrt (s / n);
    };
    p.specL = shape (t.specL, c.specL) / 1.5; p.specE = 0.5 * shape (t.specE, c.specE) / 1.5;
    for (int k = 0; k < kIacc; ++k) p.iacc += std::abs (t.iacc[k] - c.iacc[k]); p.iacc = p.iacc / kIacc / 0.08;
    p.c80 = std::abs (t.c80 - c.c80) / 1.5;
    if (out) *out = p;
    return p.t30 * p.t30 + p.edt * p.edt + p.env * p.env + p.specL * p.specL + p.specE * p.specE + p.iacc * p.iacc + p.c80 * p.c80;
}

// ---- the target: one file, or a true-stereo pair (*_L / *_R), at 48 kHz; a dry spike at the start is removed
inline bool readAt48k (const juce::File& f, std::vector<float>& L, std::vector<float>& R)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f)); if (rd == nullptr) return false;
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 20));
    juce::AudioBuffer<float> b ((int) std::max (2u, rd->numChannels), n); b.clear();
    rd->read (&b, 0, n, 0, true, true);
    if (rd->numChannels == 1) b.copyFrom (1, 0, b, 0, 0, n);
    const double ratio = rd->sampleRate / kSR; const int m = (int) std::floor ((n - 4) / ratio);
    L.assign ((size_t) std::max (0, m), 0.0f); R.assign ((size_t) std::max (0, m), 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        juce::LagrangeInterpolator ip; auto& o = c == 0 ? L : R;
        ip.process (ratio, b.getReadPointer (c), o.data(), m);
    }
    return m > 0;
}
// the start of a recording that has the dry impulse in it: that spike is the time zero, and it is taken out
inline void stripDry (std::vector<float>& L, std::vector<float>& R)
{
    const size_t n = L.size(); if (n < 2000) return;
    double pk = 0; for (size_t i = 0; i < n; ++i) pk = std::max (pk, (double) L[i] * L[i] + (double) R[i] * R[i]);
    size_t o = 0; while (o < n && (double) L[o] * L[o] + (double) R[o] * R[o] < pk * 0.00316) ++o;
    auto en = [&] (size_t a, size_t b) { double s = 0; for (size_t i = a; i < std::min (b, n); ++i) s += (double) L[i] * L[i] + (double) R[i] * R[i]; return s; };
    const size_t s0 = o > 8 ? o - 8 : 0, s1 = o + 24, q1 = o + 144, w = o + 960;
    const double spike = en (s0, s1), after = en (s1, q1), first = en (s0, w);
    if (spike >= 0.45 * first && after < 0.05 * spike)
    {
        L.erase (L.begin(), L.begin() + (long) s0); R.erase (R.begin(), R.begin() + (long) s0);
        for (size_t i = 0; i < 32 && i < L.size(); ++i) L[i] = R[i] = 0;
    }
}
inline bool loadTarget (const juce::File& f, const juce::File& pair, std::vector<float>& L, std::vector<float>& R)
{
    if (! readAt48k (f, L, R)) return false;
    stripDry (L, R);
    if (pair.existsAsFile())
    {
        std::vector<float> L2, R2; if (! readAt48k (pair, L2, R2)) return true;
        stripDry (L2, R2);
        const size_t n = std::max (L.size(), L2.size()); L.resize (n, 0.0f); R.resize (n, 0.0f);
        for (size_t i = 0; i < L2.size(); ++i) { L[i] += L2[i]; R[i] += R2[i]; }
    }
    // trailing silence off, at most 10 s
    size_t last = 0; for (size_t i = L.size(); i-- > 0;) if (std::abs (L[i]) > 1e-6f || std::abs (R[i]) > 1e-6f) { last = i; break; }
    L.resize (std::min (last + 1, (size_t) (10 * kSR))); R.resize (L.size());
    return true;
}
// "Hall_L.wav" -> "Hall_R.wav" (and the other way); empty when the name is not one of a pair
inline juce::File pairOf (const juce::File& f)
{
    const auto nm = f.getFileNameWithoutExtension();
    for (auto [a, b] : { std::pair<const char*, const char*> { "_L", "_R" }, { " L", " R" }, { "-L", "-R" }, { "_l", "_r" } })
        if (nm.endsWith (a)) return f.getSiblingFile (nm.dropLastCharacters (2) + b + f.getFileExtension());
    return {};
}
inline bool isRightOfPair (const juce::File& f)
{
    const auto nm = f.getFileNameWithoutExtension();
    for (auto [a, b] : { std::pair<const char*, const char*> { "_L", "_R" }, { " L", " R" }, { "-L", "-R" }, { "_l", "_r" } })
        if (nm.endsWith (b) && f.getSiblingFile (nm.dropLastCharacters (2) + a + f.getFileExtension()).existsAsFile()) return true;
    return false;
}
inline juce::String pairName (const juce::File& f)
{
    auto nm = f.getFileNameWithoutExtension();
    if (pairOf (f).existsAsFile()) nm = nm.dropLastCharacters (2);
    return nm;
}

// ---- the parameters it moves (id, low, high, log scale)
struct P { const char* id; double lo, hi; bool lg; };
inline std::vector<P> groupDecay() { return { { "tDecay", 0.2, 30, true }, { "rtM1", 0.15, 6, true }, { "rtM2", 0.15, 6, true }, { "rtM3", 0.15, 6, true }, { "rtM4", 0.15, 6, true }, { "rtM5", 0.15, 6, true } }; }
inline std::vector<P> groupTime() { return { { "predelay", 0, 120, false }, { "tPre", 0, 200, false }, { "tSize", 10, 300, true }, { "tDiff", 0, 100, false }, { "tLvl", -36, 12, false }, { "level", -24, 12, false }, { "tSpread", 0, 300, false }, { "tShape", -100, 100, false }, { "tSpreadHi", 20, 200, true }, { "tFill", 0, 100, false } }; }
inline std::vector<P> groupER() { return { { "first", 1, 80, true }, { "length", 8, 300, true }, { "growth", 0, 2.5, false }, { "energy", -30, 12, false }, { "decay", -40, 24, false } }; }
inline std::vector<P> groupTone() { return { { "eqG1", -12, 12, false }, { "eqG2", -12, 12, false }, { "eqG3", -12, 12, false }, { "eqG4", -12, 12, false }, { "eqG5", -12, 12, false }, { "eqG6", -12, 12, false }, { "eqLP", 2000, 20000, true }, { "damp", 1500, 16000, true } }; }
inline std::vector<P> groupStereo() { return { { "outW", 0, 200, false }, { "width", 0, 150, false }, { "tModD", 0, 100, false }, { "tBassW", 0, 100, false } }; }

using Values = std::unordered_map<std::string, float>;

// Nelder-Mead in [0,1]^n with a random look first; f is the cost; returns the best x
inline std::vector<double> nelderMead (std::function<double (const std::vector<double>&)> f, std::vector<double> x0, int maxEval, int randomFirst, unsigned seed,
                                       std::function<bool()> stop)
{
    const size_t n = x0.size(); std::mt19937 rng (seed); std::uniform_real_distribution<double> U (0, 1); std::normal_distribution<double> G (0, 0.15);
    auto clip = [] (std::vector<double> x) { for (auto& v : x) v = juce::jlimit (0.0, 1.0, v); return x; };
    auto F = [&] (const std::vector<double>& x) { return f (clip (x)); };
    double best = F (x0); int ev = 1;
    for (int k = 0; k < randomFirst && ! stop(); ++k, ++ev)
    {
        std::vector<double> x (n); for (size_t i = 0; i < n; ++i) x[i] = (k & 1) ? U (rng) : x0[i] + G (rng);
        x = clip (x); const double c = F (x); if (c < best) { best = c; x0 = x; }
    }
    for (auto& v : x0) v = juce::jlimit (0.02, 0.98, v);
    std::vector<std::vector<double>> s (n + 1, x0); std::vector<double> fs (n + 1);
    for (size_t i = 0; i < n; ++i) s[i + 1][i] = x0[i] + (x0[i] < 0.5 ? 0.12 : -0.12);
    for (size_t i = 0; i <= n; ++i) { fs[i] = F (s[i]); ++ev; }
    while (ev < maxEval && ! stop())
    {
        std::vector<size_t> id (n + 1); for (size_t i = 0; i <= n; ++i) id[i] = i;
        std::sort (id.begin(), id.end(), [&] (size_t a, size_t b) { return fs[a] < fs[b]; });
        const size_t bI = id[0], wI = id[n], sI = id[n - 1];
        if (std::abs (fs[wI] - fs[bI]) < 0.01) break;
        std::vector<double> c (n, 0); for (size_t k = 0; k < n; ++k) for (size_t i = 0; i <= n; ++i) if (i != wI) c[k] += s[i][k] / n;
        auto pt = [&] (double a) { std::vector<double> x (n); for (size_t k = 0; k < n; ++k) x[k] = c[k] + a * (s[wI][k] - c[k]); return x; };
        auto xr = pt (-1.0); const double fr = F (xr); ++ev;
        if (fr < fs[bI]) { auto xe = pt (-2.0); const double fe = F (xe); ++ev; if (fe < fr) { s[wI] = xe; fs[wI] = fe; } else { s[wI] = xr; fs[wI] = fr; } }
        else if (fr < fs[sI]) { s[wI] = xr; fs[wI] = fr; }
        else
        {
            auto xc = pt (fr < fs[wI] ? -0.5 : 0.5); const double fc = F (xc); ++ev;
            if (fc < std::min (fr, fs[wI])) { s[wI] = xc; fs[wI] = fc; }
            else for (size_t i = 0; i <= n; ++i) if (i != bI) { for (size_t k = 0; k < n; ++k) s[i][k] = s[bI][k] + 0.5 * (s[i][k] - s[bI][k]); fs[i] = F (s[i]); ++ev; }
        }
    }
    size_t b = 0; for (size_t i = 1; i <= n; ++i) if (fs[i] < fs[b]) b = i;
    return clip (fs[b] < best ? s[b] : x0);
}
} // namespace fit
