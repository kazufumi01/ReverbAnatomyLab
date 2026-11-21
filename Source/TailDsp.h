#pragma once
// Reverb Anatomy Lab: late reverb (16-line FDN with a per-band reverb time), input / output stages,
// master EQ, the reverb-time curve and the IR measurement (T30 / EDT / C80 / D50 / IACC).
// Copyright (c) 2026 Johan Plus Kei. All rights reserved.
#include "ERCore.h"
#include <complex>

namespace rv
{
using er::clampd; using er::kPi;

// ---------------------------------------------------------------------------
//  Biquad (float state, transposed direct form II)
// ---------------------------------------------------------------------------
struct BQ
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0); }
    void peak (double fc, double q, double gdB, double sr)
    {
        const double A = std::pow (10.0, gdB / 40), w = 2 * kPi * std::min (fc, sr * 0.49) / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void shelf (bool high, double fc, double gdB, double sr, double S = 1.0)
    {
        const double A = std::pow (10.0, gdB / 40), w = 2 * kPi * std::min (fc, sr * 0.49) / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt (A) * al;
        if (! high) set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                         (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
        else set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                  (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void pass (bool high, double fc, double q, double sr)
    {
        const double w = 2 * kPi * std::min (fc, sr * 0.49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        if (high) set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        else set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void pass1 (bool high, double fc, double sr)
    {
        const double K = std::tan (kPi * std::min (fc, sr * 0.49) / sr);
        if (high) set (1, -1, 0, 1 + K, K - 1, 0); else set (K, K, 0, 1 + K, K - 1, 0);
    }
    double magDb (double f, double sr) const
    {
        const double w = 2 * kPi * f / sr; const std::complex<double> z = std::polar (1.0, -w), z2 = z * z;
        const auto h = ((double) b0 + (double) b1 * z + (double) b2 * z2) / (1.0 + (double) a1 * z + (double) a2 * z2);
        return 20 * std::log10 (std::max (1e-12, std::abs (h)));
    }
};

// ---------------------------------------------------------------------------
//  Reverb time per octave band: 31.25 Hz .. 16 kHz (10 bands)
// ---------------------------------------------------------------------------
constexpr int kBands = 10;
// a cheap sine for modulation (|error| < 0.001): x in [-pi, pi]
inline float fastSin (float x)
{
    const float B = 4.0f / 3.14159265f, C = -4.0f / (3.14159265f * 3.14159265f);
    float y = B * x + C * x * std::abs (x);
    return 0.225f * (y * std::abs (y) - y) + y;
}
inline double bandFc (int k) { return 31.25 * std::pow (2.0, k); }
struct RTCurve { std::array<double, kBands> rt {}; };

// EQ mode: an "EQ" on the reverb time. ln RT = ln Decay + sum of ln(mult) x shape: node 1 low shelf, node 5 high shelf,
// nodes 2-4 bells (in octaves). Q sets the width (bells) or the steepness (shelves).
inline double rtShape (int idx, double f, double fc, double q)
{
    const double x = std::log2 (f / fc), qq = clampd (q, 0.3, 4.0);
    if (idx == 0) return 1.0 / (1.0 + std::exp (4.0 * qq * x));
    if (idx == 4) return 1.0 / (1.0 + std::exp (-4.0 * qq * x));
    const double sg = 0.6 / qq; return std::exp (-0.5 * (x / sg) * (x / sg));
}
inline double rtAtEQ (double f, double decay, const double* nf, const double* nm, const double* nq)
{
    double l = std::log (decay);
    for (int i = 0; i < 5; ++i) l += std::log (clampd (nm[i], 0.05, 20)) * rtShape (i, f, nf[i], nq[i]);
    return std::exp (l);
}
// multiband mode: n bands split at n - 1 crossovers; the multiplier blends over +-1/2 octave around each crossover
inline double rtAtMB (double f, double decay, const double* xo, const double* m, int nBands)
{
    double lm = std::log (m[0]);
    for (int i = 0; i + 1 < nBands; ++i)
    {
        const double x = clampd (std::log2 (f / xo[i]) + 0.5, 0, 1), s = x * x * (3 - 2 * x);
        lm += s * (std::log (m[i + 1]) - std::log (m[i]));
    }
    return decay * std::exp (lm);
}

// ---------------------------------------------------------------------------
//  Graphic EQ in the loop: 10 peaking filters at the band centres. The gains come from the
//  interaction matrix (each filter's response at every centre), inverted once per sample rate,
//  so the attenuation at each centre is what the reverb time asks for.
// ---------------------------------------------------------------------------
struct GeqDesign
{
    double sr = 0, q = 0.92;                                   // 1.5-octave bandwidth
    std::array<std::array<double, kBands>, kBands> inv {};
    void prepare (double s)
    {
        if (s == sr) return;
        sr = s;
        const double Gp = 6.0;                                 // a prototype gain where the filters are still about linear
        double B[kBands][kBands];
        for (int j = 0; j < kBands; ++j)
        {
            BQ f; make (f, j, Gp);
            for (int i = 0; i < kBands; ++i) B[i][j] = f.magDb (bandFc (i), sr) / Gp;
        }
        // Gauss-Jordan inverse
        double M[kBands][2 * kBands];
        for (int i = 0; i < kBands; ++i) for (int j = 0; j < 2 * kBands; ++j) M[i][j] = j < kBands ? B[i][j] : (j - kBands == i ? 1 : 0);
        for (int c = 0; c < kBands; ++c)
        {
            int p = c; for (int r = c + 1; r < kBands; ++r) if (std::abs (M[r][c]) > std::abs (M[p][c])) p = r;
            for (int j = 0; j < 2 * kBands; ++j) std::swap (M[c][j], M[p][j]);
            const double d = M[c][c]; for (int j = 0; j < 2 * kBands; ++j) M[c][j] /= d;
            for (int r = 0; r < kBands; ++r) if (r != c) { const double f = M[r][c]; for (int j = 0; j < 2 * kBands; ++j) M[r][j] -= f * M[c][j]; }
        }
        for (int i = 0; i < kBands; ++i) for (int j = 0; j < kBands; ++j) inv[(size_t) i][(size_t) j] = M[i][kBands + j];
    }
    // the end bands are shelves, so below 31 Hz and above 16 kHz decay too (DC must not ring forever)
    void make (BQ& f, int j, double gDb) const
    {
        if (j == 0) f.shelf (false, bandFc (0) * 1.41, gDb, sr, 0.7);
        else if (j == kBands - 1) f.shelf (true, bandFc (kBands - 1) / 1.41, gDb, sr, 0.7);
        else f.peak (bandFc (j), q, gDb, sr);
    }
    // correct the gains with the cascade's actual response at the centres (the filters are not quite linear in dB)
    void refine (const double* targetDb, double* g, int passes) const
    {
        for (int p = 0; p < passes; ++p)
        {
            double act[kBands] {}, err[kBands], dg[kBands];
            for (int j = 0; j < kBands; ++j) { BQ f; make (f, j, g[j]); for (int i = 0; i < kBands; ++i) act[i] += f.magDb (bandFc (i), sr); }
            for (int i = 0; i < kBands; ++i) err[i] = targetDb[i] - act[i];
            gains (err, dg);
            for (int i = 0; i < kBands; ++i) g[i] += dg[i];
        }
    }
    void gains (const double* targetDb, double* g) const
    {
        for (int i = 0; i < kBands; ++i) { double s = 0; for (int j = 0; j < kBands; ++j) s += inv[(size_t) i][(size_t) j] * targetDb[j]; g[i] = s; }
    }
};

// ---------------------------------------------------------------------------
//  Late reverb: 16-line FDN (Hadamard), input diffusion, modulated delay lines, per-band decay, freeze
// ---------------------------------------------------------------------------
struct TailSettings
{
    double sizeMs = 80, diffusion = 0.7, modDepthMs = 0.3, modRate = 0.6;
    RTCurve rt; bool freeze = false;
    int engine = 0;                                            // 0 = FDN (rooms / halls), 1 = plate, 2 = spring, 3 = ambient
    double gravity = 0, feedback = 0;                          // (ambient) -1 swell .. 0 .. +1 front-heavy; 0 .. 0.9 back into the input
    double spreadMs = 0, shape = 0;                            // 0.6.1: build-up (the input smeared over Spread ms, envelope by Shape -1..+1)
    double bassWidth = 1;                                      // 0.6.1: the tail's width below ~250 Hz (0 = mono .. 1 = as is)
    double spreadHi = 1;                                       // 0.7.3: the build-up time of the band above ~1.5 kHz, as a ratio of Spread (1 = the same)
    double fill = 0;                                           // 0.7.2: onset fill (Hall engine): extra outputs part-way along each line, 0..1
    // 0.10.3: two slopes (connected rooms): a second FDN with its own reverb time and size, mixed in (levels are amplitudes: sqrt of the energy shares)
    bool dual = false; RTCurve rt2; double sizeMs2 = 80, gain1 = 1, gain2 = 0, delay2Ms = 0;
};

// ---------------------------------------------------------------------------
//  0.6.1: build-up ("Spread" / "Shape"): the tail's input goes through a sparse random FIR (velvet noise, ~1 tap per ms)
//  whose envelope over Spread ms is rising (Shape > 0), flat (0) or falling (< 0). Energy-normalised, different noise
//  per side; a change crossfades the old and new tap sets over 20 ms. Spread 0 = bypass.
// ---------------------------------------------------------------------------
class Spreader
{
public:
    void prepare (double s) { sr = s; size = 1; while (size < (int) (0.32 * sr) + 8192) size <<= 1; for (auto& b : buf) b.assign ((size_t) size, 0.0f); w = 0; cur[0].clear(); cur[1].clear(); nxt[0].clear(); nxt[1].clear(); x = 1; }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
    bool active() const { return ! cur[0].empty() || ! nxt[0].empty(); }
    void set (double spreadMs, double shape)
    {
        spreadMs = std::clamp (spreadMs, 0.0, 300.0); shape = std::clamp (shape, -1.0, 1.0);
        if (std::abs (spreadMs - sMs) < 0.05 && std::abs (shape - sSh) < 0.002) return;
        sMs = spreadMs; sSh = shape;
        Taps t[2];
        if (spreadMs >= 1.0)
        {
            const int segN = std::max (1, (int) (0.001 * sr)), n = std::max (1, (int) (spreadMs * 0.001 * sr / segN));
            juce::Random rnd (777);
            for (int c = 0; c < 2; ++c)
            {
                double e = 0;
                for (int k = 0; k < n; ++k)
                {
                    const double u = (k + 0.5) / n;
                    const double env = shape >= 0 ? std::pow (u, shape * 4.0) : std::pow (1.0 - u, -shape * 4.0);
                    const int d = k * segN + rnd.nextInt (segN);
                    const float g = (float) ((rnd.nextBool() ? 1.0 : -1.0) * env);
                    t[c].push_back ({ d, g }); e += env * env;
                }
                const float nrm = (float) (1.0 / std::sqrt (e + 1e-12));
                for (auto& tp : t[c]) tp.g *= nrm;
            }
        }
        if (cur[0].empty() && nxt[0].empty() && ! t[0].empty() && firstSet) { cur[0] = t[0]; cur[1] = t[1]; x = 1; }
        else { nxt[0] = t[0]; nxt[1] = t[1]; x = 0; fadeOn = true; }
        firstSet = false;
    }
    void setInstant() { if (fadeOn) { cur[0] = nxt[0]; cur[1] = nxt[1]; nxt[0].clear(); nxt[1].clear(); fadeOn = false; x = 1; } }
    // in place. Block form: the block goes into the history first, then each tap adds its delayed copy over the whole block
    void process (float* L, float* R, int n)
    {
        if (! active() && ! fadeOn) return;
        const int mask = size - 1; const float st = (float) (1.0 / (0.02 * sr));
        float* io[2] = { L, R };
        if ((int) acc.size() < n) { acc.resize ((size_t) n); acc2.resize ((size_t) n); }
        for (int c = 0; c < 2; ++c)
        {
            auto& b = buf[(size_t) c];
            for (int i = 0; i < n; ++i) b[(size_t) ((w + i) & mask)] = io[c][i];
            auto run = [&] (const Taps& taps, float* out)
            {
                if (taps.empty()) { for (int i = 0; i < n; ++i) out[i] = b[(size_t) ((w + i) & mask)]; return; }
                std::fill (out, out + n, 0.0f);
                for (const auto& tp : taps)
                {
                    const int i0 = (w - tp.d) & mask; const int seg = std::min (n, size - i0);
                    const float* src = b.data() + i0; const float g = tp.g;
                    for (int i = 0; i < seg; ++i) out[i] += src[i] * g;
                    const float* src2 = b.data() - seg;                              // (wrapped: continues from index 0)
                    for (int i = seg; i < n; ++i) out[i] += src2[i] * g;
                }
            };
            run (cur[c], acc.data());
            if (fadeOn)
            {
                run (nxt[c], acc2.data());
                float xx = x;
                for (int i = 0; i < n; ++i) { xx = std::min (1.0f, xx + st); acc[(size_t) i] += xx * (acc2[(size_t) i] - acc[(size_t) i]); }
                if (c == 1) x = xx;
            }
            std::copy (acc.begin(), acc.begin() + n, io[c]);
        }
        w = (w + n) & mask;
        if (fadeOn && x >= 1.0f) { cur[0] = nxt[0]; cur[1] = nxt[1]; nxt[0].clear(); nxt[1].clear(); fadeOn = false; }
    }
private:
    struct Tap { int d; float g; }; using Taps = std::vector<Tap>;
    double sr = 48000, sMs = -1, sSh = 0; std::vector<float> buf[2], acc, acc2; int size = 1, w = 0; Taps cur[2], nxt[2]; float x = 1; bool fadeOn = false, firstSet = true;
};

class FdnCore
{
public:
    static constexpr int N = 16;
    static constexpr double kMaxMs = 2500;                     // the delay lines can be seconds long (for the ambient engine later)
    void prepare (double sampleRate, double maxMs = kMaxMs)
    {
        sr = sampleRate; geq.prepare (sr);
        for (int k = 0; k < kBands; ++k) { const double w = 2 * kPi * std::min (bandFc (k), sr * 0.49) / sr; pkC[k] = std::cos (w); pkAl[k] = std::sin (w) / (2 * geq.q); }
        size_t len = 1; while ((double) len < maxMs * sr / 1000.0 + 8) len <<= 1;
        mask = (int) len - 1;
        for (auto& b : buf) b.assign (len, 0.0f);
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) { apBuf[c][k].assign ((size_t) (0.02 * sr) + 4, 0.0f); apPos[c][k] = 0; }
        reset(); configured = false;
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& f : filt) for (auto& q : f) q.reset();
        for (auto& a : sz1) for (auto& v : a) v = 0; for (auto& a : sz2) for (auto& v : a) v = 0;
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) std::fill (apBuf[c][k].begin(), apBuf[c][k].end(), 0.0f);
        w = 0; inGain = freezeIn ? 0.0f : 1.0f;
    }
    // control rate (when the settings change)
    void setup (const TailSettings& t, bool instant = false)
    {
        // line lengths: spread over 0.45..1.0 of the size, mutually prime-ish ratios
        static const double ratio[N] = { 0.451, 0.487, 0.521, 0.559, 0.593, 0.631, 0.667, 0.703, 0.739, 0.773, 0.811, 0.847, 0.883, 0.919, 0.957, 1.0 };
        modTarget = (float) (clampd (t.modDepthMs, 0, 20) * sr / 1000.0); if (! configured) modDepth = modTarget;
        for (int i = 0; i < N; ++i)
        {
            target[i] = std::round (clampd (t.sizeMs * ratio[i] * sr / 1000.0, 8.0, (double) mask - 0.03 * sr - 8));
            if (instant || ! configured) cur[i] = target[i];
            rate[i] = (float) (2 * kPi * t.modRate * (0.83 + 0.034 * i) / sr);
        }
        // absorption per line: the attenuation per pass that gives the reverb time in every band
        for (int i = 0; i < N; ++i)
        {
            double tgt[kBands], g[kBands];
            for (int k = 0; k < kBands; ++k) tgt[k] = t.freeze ? 0.0 : -60.0 * target[i] / (clampd (t.rt.rt[(size_t) k], 0.05, 60) * sr);
            geq.gains (tgt, g);
            geq.refine (tgt, g, 2);
            for (int k = 0; k < kBands; ++k)
            {
                gTgt[i][k] = (float) g[k];
                if (instant || ! configured) { gCur[i][k] = gTgt[i][k]; applyGain (i, k); }
            }
        }
        // input diffusion: 4 allpasses per channel
        static const double apMs[2][4] = { { 1.31, 2.27, 3.71, 5.33 }, { 1.43, 2.41, 3.97, 5.71 } };
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k)
            apLen[c][k] = std::max (1, std::min ((int) apBuf[c][k].size() - 1, (int) std::lround (apMs[c][k] * sr / 1000.0)));
        apTarget = (float) (clampd (t.diffusion, 0, 1) * 0.72); if (! configured) apG = apTarget;
        fillTarget = (float) clampd (t.fill, 0, 1); if (instant || ! configured) fillCur = fillTarget;
        freezeIn = t.freeze;
        configured = true;
    }
    // the loop gains glide (a jump in a recursive filter's coefficients clicks): every 16 samples, ~50 ms
    void applyGain (int i, int k)
    {
        auto& f = filt[(size_t) i][(size_t) k]; const float z1 = f.z1, z2 = f.z2;
        if (k == 0 || k == kBands - 1) geq.make (f, k, gCur[i][k]);
        else
        {   // peaking: only A changes (cos / alpha are fixed per band)
            const double A = std::pow (10.0, gCur[i][k] / 40.0), a0 = 1 + pkAl[k] / A;
            f.b0 = (float) ((1 + pkAl[k] * A) / a0); f.b1 = f.a1 = (float) (-2 * pkC[k] / a0); f.b2 = (float) ((1 - pkAl[k] * A) / a0); f.a2 = (float) ((1 - pkAl[k] / A) / a0);
        }
        f.z1 = z1; f.z2 = z2;
        // the loop runs the 16 lines' filters side by side (structure of arrays, so the compiler can vectorise)
        sb0[k][i] = f.b0; sb1[k][i] = f.b1; sb2[k][i] = f.b2; sa1[k][i] = f.a1; sa2[k][i] = f.a2;
    }
    void glideGains()
    {
        const float k16 = (float) (1.0 - std::exp (-16.0 / (0.05 * sr)));
        for (int i = 0; i < N; ++i)
            for (int k = 0; k < kBands; ++k)
            {
                const float d = gTgt[i][k] - gCur[i][k];
                if (std::abs (d) < 1e-6f) continue;
                gCur[i][k] = std::abs (d) < 1e-4f ? gTgt[i][k] : gCur[i][k] + d * k16;
                applyGain (i, k);
            }
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float glide = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
        const float inTarget = freezeIn ? 0.0f : 1.0f, inStep = (float) (1.0 / (0.03 * sr));
        for (int s = 0; s < n; ++s)
        {
            if ((++gCount & 15) == 0) glideGains();
            modDepth += (modTarget - modDepth) * 0.0005f;          // depth glides (~40 ms): a jump would move the read position
            apG += (apTarget - apG) * 0.0005f;                     // so does the diffusion (an allpass coefficient jump clicks)
            // input diffusion
            float x[2] = { inL[s], inR[s] };
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k)
                {
                    auto& b = apBuf[c][k]; int& p = apPos[c][k];
                    const float d = b[(size_t) p], v = x[c] + apG * d;
                    b[(size_t) p] = v; if (++p >= apLen[c][k]) p = 0;
                    x[c] = d - apG * v;
                }
            inGain += inGain < inTarget ? std::min (inStep, inTarget - inGain) : -std::min (inStep, inGain - inTarget);
            // read the lines (fractional, modulated), absorb
            float y[N];
            for (int i = 0; i < N; ++i)
            {
                cur[i] += std::clamp ((target[i] - cur[i]) * glide, -0.05, 0.05);      // at most 5 % pitch while the size moves
                ph[i] += rate[i]; if (ph[i] > (float) (2 * kPi)) ph[i] -= (float) (2 * kPi);
                const double d = cur[i] + (modDepth > 0 ? modDepth * (1.0 - fastSin (ph[i] - (float) kPi)) * 0.5 : 0.0);
                const int di = (int) d; const float fr = (float) (d - di);
                const auto& b = buf[(size_t) i];
                // 3rd-order Lagrange: linear interpolation would be a low-pass on every pass round the loop
                // (-1.2 dB at 8 kHz each time = a much shorter treble decay)
                const float sm = b[(size_t) ((w - di + 1) & mask)], s0 = b[(size_t) ((w - di) & mask)],
                            s1 = b[(size_t) ((w - di - 1) & mask)], s2 = b[(size_t) ((w - di - 2) & mask)];
                const float xm = fr + 1, x1 = fr - 1, x2 = fr - 2;
                y[i] = -fr * x1 * x2 * (1.0f / 6.0f) * sm + xm * x1 * x2 * 0.5f * s0 - xm * fr * x2 * 0.5f * s1 + xm * fr * x1 * (1.0f / 6.0f) * s2;
            }
            // absorption: band by band, all 16 lines at once
            for (int k = 0; k < kBands; ++k)
            {
                float* z1 = sz1[k]; float* z2 = sz2[k];
                const float *b0 = sb0[k], *b1 = sb1[k], *b2 = sb2[k], *a1 = sa1[k], *a2 = sa2[k];
                for (int i = 0; i < N; ++i)
                {
                    const float x = y[i], o = b0[i] * x + z1[i];
                    z1[i] = b1[i] * x - a1[i] * o + z2[i]; z2[i] = b2[i] * x - a2[i] * o; y[i] = o;
                }
            }
            float oL = 0, oR = 0;
            for (int i = 0; i < N; ++i) { oL += (i & 1) ? -y[i] : y[i]; oR += ((i >> 1) & 1) ? -y[i] : y[i]; }
            // onset fill: the same lines read a third and two thirds of the way along. What went in arrives there sooner,
            // so the tail is dense from its start (no gap between the ER and the tail); the decay is unchanged (not fed back)
            fillCur += (fillTarget - fillCur) * 0.0005f;
            if (fillCur > 0.001f)
            {
                float fL = 0, fR = 0;
                for (int i = 0; i < N; ++i)
                {
                    const int d1 = (int) (cur[i] * 0.333), d2 = (int) (cur[i] * 0.667);
                    const auto& b = buf[(size_t) i];
                    const float v = b[(size_t) ((w - d1) & mask)] - b[(size_t) ((w - d2) & mask)];
                    fL += ((i >> 2) & 1) ? -v : v; fR += ((i >> 3) & 1) ? -v : v;
                }
                oL = oL * (1.0f - 0.35f * fillCur) + fL * 0.45f * fillCur; oR = oR * (1.0f - 0.35f * fillCur) + fR * 0.45f * fillCur;
            }
            // Hadamard 16 (orthonormal: / 4)
            for (int h = 1; h < N; h <<= 1) for (int k = 0; k < N; k += h << 1) for (int j = k; j < k + h; ++j) { const float u = y[j], v = y[j + h]; y[j] = u + v; y[j + h] = u - v; }
            const float iL = x[0] * inGain, iR = x[1] * inGain;
            for (int i = 0; i < N; ++i) buf[(size_t) i][(size_t) w] = y[i] * 0.25f + ((i & 1) ? iR : iL) * ((i & 2) ? -1.0f : 1.0f);
            w = (w + 1) & mask;
            outL[s] = oL * 0.35f; outR[s] = oR * 0.35f;
        }
    }
    double sampleRate() const { return mask > 0 ? sr : 0.0; }       // 0 until prepared
private:
    double sr = 48000; GeqDesign geq;
    std::array<std::vector<float>, N> buf; int mask = 0, w = 0;
    std::array<std::array<BQ, kBands>, N> filt;
    alignas (32) float sb0[kBands][N] {}, sb1[kBands][N] {}, sb2[kBands][N] {}, sa1[kBands][N] {}, sa2[kBands][N] {}, sz1[kBands][N] {}, sz2[kBands][N] {};
    double cur[N] {}, target[N] {}; float ph[N] {}, rate[N] {}, modDepth = 0, modTarget = 0;
    std::vector<float> apBuf[2][4]; int apPos[2][4] {}, apLen[2][4] {}; float apG = 0.5f, apTarget = 0.5f;
    float inGain = 1, fillCur = 0, fillTarget = 0; bool freezeIn = false, configured = false;
    float gTgt[N][kBands] {}, gCur[N][kBands] {}; double pkC[kBands] {}, pkAl[kBands] {}; unsigned gCount = 0;
};

// ---------------------------------------------------------------------------
//  Plate: the classic figure-of-eight tank (input diffusers, two cross-fed halves of
//  modulated allpass - delay - absorption - allpass - delay; output taps from both halves).
//  The absorption is the same graphic EQ as the FDN's, so the RT curve (per band) holds here too.
// ---------------------------------------------------------------------------
class Plate
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; geq.prepare (sr);
        const double k = sr / 29761.0 * 3.2 + 8;                 // room for sizes up to 3x the original tank
        auto mk = [&] (VD& d, double len) { size_t n = 1; while ((double) n < len * k + 64) n <<= 1; d.b.assign (n, 0.0f); d.mask = (int) n - 1; d.w = 0; };
        for (int i = 0; i < 4; ++i) mk (inAp[i], kIn[i] / 3.2 + 1);
        for (int h = 0; h < 2; ++h) { mk (ap1[h], (kAp1[h] + 40) / 3.2); mk (dl1[h], kDl1[h] / 3.2); mk (ap2[h], kAp2[h] / 3.2); mk (dl2[h], kDl2[h] / 3.2); }
        reset(); configured = false;
    }
    void reset()
    {
        for (auto* d : all()) std::fill (d->b.begin(), d->b.end(), 0.0f);
        for (auto& h : filt) for (auto& f : h) f.reset();
        fb[0] = fb[1] = 0; inGain = freezeIn ? 0.0f : 1.0f;
    }
    void setup (const TailSettings& t, bool instant = false)
    {
        // size: 80 ms of the FDN = the original tank; glide like the FDN (<= 5 % pitch)
        scaleT = clampd (t.sizeMs / 80.0, 0.3, 3.0); if (instant || ! configured) scale = scaleT;
        difT = clampd (t.diffusion, 0, 1); if (instant || ! configured) dif = difT;
        excT = clampd (t.modDepthMs, 0, 2.0) * sr / 1000.0 * 0.5; if (instant || ! configured) exc = excT;
        rateW = 2 * kPi * clampd (t.modRate, 0.05, 5.0) / sr;
        freezeIn = t.freeze;
        // absorption per half-loop: the attenuation per pass that gives the reverb time in every band
        for (int h = 0; h < 2; ++h)
        {
            // (x 1.1: the allpasses' stored energy makes the loop act about 10 % longer than its delays add up to)
            const double Lsec = (kAp1[h] + kDl1[h] + kAp2[h] + kDl2[h]) / 29761.0 * scaleT * 1.1;
            double tgt[kBands], g[kBands];
            for (int b = 0; b < kBands; ++b) tgt[b] = t.freeze ? 0.0 : -60.0 * Lsec / clampd (t.rt.rt[(size_t) b], 0.05, 60);
            geq.gains (tgt, g); geq.refine (tgt, g, 2);
            for (int b = 0; b < kBands; ++b) { gTgt[h][b] = (float) g[b]; if (instant || ! configured) { gCur[h][b] = gTgt[h][b]; geq.make (filt[h][(size_t) b], b, gCur[h][b]); } }
        }
        configured = true;
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float inTarget = freezeIn ? 0.0f : 1.0f, inStep = (float) (1.0 / (0.03 * sr));
        const double ks = sr / 29761.0;
        for (int s = 0; s < n; ++s)
        {
            if ((++cnt & 15) == 0)
            {   // control rate: size, modulation depth, diffusion (an allpass coefficient jump clicks) and the absorption glide
                // size: at most ~1.5 % pitch on the longest line while it moves (a faster change is heard as a swoop / click)
                const double lim = 0.015 * 16 / (kDl1[0] * ks);
                scale += std::clamp ((scaleT - scale) * 0.01, -lim, lim);
                dif += (difT - dif) * 0.01;
                exc += (excT - exc) * 0.01;
                for (int h = 0; h < 2; ++h) for (int b = 0; b < kBands; ++b)
                {
                    const float d = gTgt[h][b] - gCur[h][b]; if (std::abs (d) < 1e-6f) continue;
                    gCur[h][b] = std::abs (d) < 1e-4f ? gTgt[h][b] : gCur[h][b] + d * 0.015f;
                    geq.make (filt[h][(size_t) b], b, gCur[h][b]);
                }
            }
            inGain += inGain < inTarget ? std::min (inStep, inTarget - inGain) : -std::min (inStep, inGain - inTarget);
            const float g1 = (float) (0.75 * dif), g2 = (float) (0.625 * dif), dd1 = (float) (0.7 * dif), dd2 = 0.5f;
            // input: mono, four diffusers
            float x = 0.5f * (inL[s] + inR[s]) * inGain;
            for (int i = 0; i < 4; ++i) x = allpass (inAp[i], kIn[i] * ks, i < 2 ? g1 : g2, x);
            ph += rateW; if (ph > 2 * kPi) ph -= 2 * kPi;
            float half[2];
            for (int h = 0; h < 2; ++h)
            {
                float v = x + fb[1 - h];
                const double m = exc * std::sin (ph + h * 1.6);
                v = allpass (ap1[h], kAp1[h] * ks * scale + m, -dd1, v);
                dl1[h].write (v); v = dl1[h].read (kDl1[h] * ks * scale);
                for (auto& f : filt[h]) v = f.run (v);
                v = allpass (ap2[h], kAp2[h] * ks * scale, dd2, v);
                dl2[h].write (v); half[h] = dl2[h].read (kDl2[h] * ks * scale);
            }
            fb[0] = half[0]; fb[1] = half[1];
            // output taps (left from both halves, right mirrored)
            const double q = ks * scale;
            auto tA = [&] (VD& d, double t) { return d.read (t * q); };
            const float yl = tA (dl1[1], 266) + tA (dl1[1], 2974) - tA (ap2[1], 1913) + tA (dl2[1], 1996) - tA (dl1[0], 1990) - tA (ap2[0], 187) - tA (dl2[0], 1066);
            const float yr = tA (dl1[0], 353) + tA (dl1[0], 3627) - tA (ap2[0], 1228) + tA (dl2[0], 2673) - tA (dl1[1], 2111) - tA (ap2[1], 335) - tA (dl2[1], 121);
            outL[s] = yl * kOut; outR[s] = yr * kOut;
        }
    }
    double sampleRate() const { return configured || ! inAp[0].b.empty() ? sr : 0.0; }
private:
    struct VD
    {
        std::vector<float> b; int mask = 0, w = 0;
        void write (float v) { b[(size_t) w] = v; w = (w + 1) & mask; }
        float read (double d) const       // d samples back from the last write (cubic)
        {
            d = std::max (1.0, std::min (d, (double) mask - 4));
            const int di = (int) d; const float fr = (float) (d - di);
            const int i0 = (w - 1 - di) & mask;
            const float sm = b[(size_t) ((i0 + 1) & mask)], s0 = b[(size_t) i0], s1 = b[(size_t) ((i0 - 1) & mask)], s2 = b[(size_t) ((i0 - 2) & mask)];
            const float xm = fr + 1, x1 = fr - 1, x2 = fr - 2;
            return -fr * x1 * x2 * (1.0f / 6.0f) * sm + xm * x1 * x2 * 0.5f * s0 - xm * fr * x2 * 0.5f * s1 + xm * fr * x1 * (1.0f / 6.0f) * s2;
        }
    };
    static float allpass (VD& d, double len, float g, float x)
    {
        const float dl = d.read (len - 1); const float v = x + g * dl; d.write (v); return dl - g * v;
    }
    std::vector<VD*> all() { std::vector<VD*> v; for (auto& d : inAp) v.push_back (&d); for (int h = 0; h < 2; ++h) { v.push_back (&ap1[h]); v.push_back (&dl1[h]); v.push_back (&ap2[h]); v.push_back (&dl2[h]); } return v; }
    // the tank's lengths at its original 29761 Hz
    static constexpr double kIn[4] = { 142, 107, 379, 277 };
    static constexpr double kAp1[2] = { 672, 908 }, kDl1[2] = { 4453, 4217 }, kAp2[2] = { 1800, 2656 }, kDl2[2] = { 3720, 3163 };
    static constexpr float kOut = 1.30f;          // about the FDN's level
    double sr = 48000; GeqDesign geq;
    VD inAp[4], ap1[2], dl1[2], ap2[2], dl2[2];
    std::array<std::array<BQ, kBands>, 2> filt;
    float gTgt[2][kBands] {}, gCur[2][kBands] {}, fb[2] {}, inGain = 1;
    double scale = 1, scaleT = 1, dif = 0.7, difT = 0.7, exc = 0, excT = 0, rateW = 0, ph = 0; unsigned cnt = 0;
    bool freezeIn = false, configured = false;
};

// ---------------------------------------------------------------------------
//  Spring: three springs, each a feedback loop of a dispersive allpass chain (the "chirp":
//  low frequencies travel slower along a spring, so a click comes back as a falling "boing"),
//  a transit delay and the per-band absorption. The output is band-limited like a real tank.
//  (After the stretched-allpass spring models in the literature; an approximation for learning.)
// ---------------------------------------------------------------------------
class Spring
{
public:
    static constexpr int kSprings = 3, kStages = 48;
    void prepare (double sampleRate)
    {
        sr = sampleRate; geq.prepare (sr);
        K = std::max (1, (int) std::round (sr / (2.0 * kChirpHz)));
        for (auto& sp : sp)
        {
            size_t n = 1; while ((double) n < kMaxDelayMs * 3.0 * sr / 1000.0 + 64) n <<= 1;
            sp.dl.assign (n, 0.0f); sp.mask = (int) n - 1; sp.w = 0;
            for (auto& st : sp.ap) { st.x.assign ((size_t) K, 0.0f); st.y.assign ((size_t) K, 0.0f); }
        }
        for (auto& f : outLP) f.pass (false, 5200, 0.6, sr);
        for (auto& f : outHP) f.pass1 (true, 90, sr);
        reset(); configured = false;
    }
    void reset()
    {
        for (auto& p : sp) { std::fill (p.dl.begin(), p.dl.end(), 0.0f); p.w = 0; p.fb = 0; for (auto& st : p.ap) { std::fill (st.x.begin(), st.x.end(), 0.0f); std::fill (st.y.begin(), st.y.end(), 0.0f); } p.k = 0; for (auto& f : p.filt) f.reset(); }
        for (auto& f : outLP) f.reset(); for (auto& f : outHP) f.reset();
        inGain = freezeIn ? 0.0f : 1.0f;
    }
    // the chain's group delay in samples at f (all stages): a stretched first-order allpass, K x (1 - a^2) / (1 + 2a cos(K w) + a^2)
    double chainDelay (double f, double a) const
    {
        const double w = 2 * kPi * f / sr;
        return kStages * K * (1 - a * a) / (1 + 2 * a * std::cos (K * w) + a * a);
    }
    void setup (const TailSettings& t, bool instant = false)
    {
        scaleT = clampd (t.sizeMs / 80.0, 0.4, 3.0); if (instant || ! configured) scale = scaleT;
        // diffusion = how strongly the spring disperses (how much "boing")
        aT = -(0.35 + 0.35 * clampd (t.diffusion, 0, 1)); if (instant || ! configured) a = aT;
        excT = clampd (t.modDepthMs, 0, 2.0) * sr / 1000.0 * 0.4; if (instant || ! configured) exc = excT;
        rateW = 2 * kPi * clampd (t.modRate, 0.05, 5.0) / sr;
        freezeIn = t.freeze;
        for (int i = 0; i < kSprings; ++i)
        {
            const double D = kDelayMs[i] * scaleT * sr / 1000.0;
            double tgt[kBands], g[kBands];
            for (int b = 0; b < kBands; ++b)
            {
                // the loop is longer where the chirp is slow: the absorption per pass follows the real loop time in each band
                const double Ls = (D + chainDelay (bandFc (b), aT)) / sr;
                tgt[b] = t.freeze ? 0.0 : -60.0 * Ls / clampd (t.rt.rt[(size_t) b], 0.05, 60);
            }
            geq.gains (tgt, g); geq.refine (tgt, g, 2);
            for (int b = 0; b < kBands; ++b) { gTgt[i][b] = (float) g[b]; if (instant || ! configured) { gCur[i][b] = gTgt[i][b]; geq.make (sp[i].filt[(size_t) b], b, gCur[i][b]); } }
        }
        configured = true;
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float inTarget = freezeIn ? 0.0f : 1.0f, inStep = (float) (1.0 / (0.03 * sr));
        for (int s = 0; s < n; ++s)
        {
            if ((++cnt & 15) == 0)
            {
                const double lim = 0.015 * 16 / (kDelayMs[2] * sr / 1000.0);
                scale += std::clamp ((scaleT - scale) * 0.01, -lim, lim);
                a += (aT - a) * 0.005; exc += (excT - exc) * 0.01;
                for (int i = 0; i < kSprings; ++i) for (int b = 0; b < kBands; ++b)
                {
                    const float d = gTgt[i][b] - gCur[i][b]; if (std::abs (d) < 1e-6f) continue;
                    gCur[i][b] = std::abs (d) < 1e-4f ? gTgt[i][b] : gCur[i][b] + d * 0.015f;
                    geq.make (sp[i].filt[(size_t) b], b, gCur[i][b]);
                }
            }
            inGain += inGain < inTarget ? std::min (inStep, inTarget - inGain) : -std::min (inStep, inGain - inTarget);
            const float x = 0.5f * (inL[s] + inR[s]) * inGain;
            ph += rateW; if (ph > 2 * kPi) ph -= 2 * kPi;
            const float af = (float) a;
            float y[kSprings];
            for (int i = 0; i < kSprings; ++i)
            {
                auto& p = sp[i];
                float v = x * kIn[i] + p.fb;
                // the dispersive chain (stretched allpasses: y[n] = a x[n] + x[n-K] - a y[n-K])
                for (auto& st : p.ap)
                {
                    const float xk = st.x[(size_t) p.k], yk = st.y[(size_t) p.k];
                    const float o = af * v + xk - af * yk;
                    st.x[(size_t) p.k] = v; st.y[(size_t) p.k] = o; v = o;
                }
                if (++p.k >= K) p.k = 0;
                // transit delay (slightly wobbling: a spring is never still)
                p.dl[(size_t) p.w] = v;
                const double d = kDelayMs[i] * scale * sr / 1000.0 + exc * std::sin (ph + i * 2.1);
                const float o = readCubic (p, d);
                p.w = (p.w + 1) & p.mask;
                float r = o; for (auto& f : p.filt) r = f.run (r);
                p.fb = r; y[i] = o;
            }
            float l = y[0] + 0.6f * y[2] - 0.25f * y[1], rr = y[1] + 0.6f * y[2] - 0.25f * y[0];
            l = outHP[0].run (outLP[0].run (l)); rr = outHP[1].run (outLP[1].run (rr));
            outL[s] = l * kOut; outR[s] = rr * kOut;
        }
    }
private:
    struct Stage { std::vector<float> x, y; };
    struct One { std::vector<float> dl; int mask = 0, w = 0, k = 0; float fb = 0; std::array<Stage, kStages> ap; std::array<BQ, kBands> filt; };
    static float readCubic (const One& p, double d)
    {
        d = std::max (1.0, std::min (d, (double) p.mask - 4));
        const int di = (int) d; const float fr = (float) (d - di);
        const int i0 = (p.w - di) & p.mask;
        const float sm = p.dl[(size_t) ((i0 + 1) & p.mask)], s0 = p.dl[(size_t) i0], s1 = p.dl[(size_t) ((i0 - 1) & p.mask)], s2 = p.dl[(size_t) ((i0 - 2) & p.mask)];
        const float xm = fr + 1, x1 = fr - 1, x2 = fr - 2;
        return -fr * x1 * x2 * (1.0f / 6.0f) * sm + xm * x1 * x2 * 0.5f * s0 - xm * fr * x2 * 0.5f * s1 + xm * fr * x1 * (1.0f / 6.0f) * s2;
    }
    static constexpr double kChirpHz = 4300;                         // where the chirp folds over (a typical tank)
    static constexpr double kDelayMs[kSprings] = { 31.0, 37.0, 43.0 }, kMaxDelayMs = 43.0;
    static constexpr float kIn[kSprings] = { 1.0f, -0.9f, 0.8f };
    static constexpr float kOut = 1.8f;           // about the FDN's level
    double sr = 48000; int K = 6; GeqDesign geq;
    std::array<One, kSprings> sp; BQ outLP[2], outHP[2];
    float gTgt[kSprings][kBands] {}, gCur[kSprings][kBands] {}, inGain = 1;
    double scale = 1, scaleT = 1, a = -0.6, aT = -0.6, exc = 0, excT = 0, rateW = 0, ph = 0; unsigned cnt = 0;
    bool freezeIn = false, configured = false;
};

// ---------------------------------------------------------------------------
//  Ambient: a space bigger than any room. 8 long lines (about 0.1-1.3 s, by Size) with an allpass
//  in each loop (the smear), a Householder mix, the per-band absorption, strong slow modulation.
//  Before it: GRAVITY, a bank of taps over up to 3 s whose weights rise (swell, < 0) or fall
//  (front-heavy, > 0); after it: FEEDBACK, the output fed back into the input through a soft limiter
//  (the tail grows longer than Decay says and blooms).
// ---------------------------------------------------------------------------
class Ambient
{
public:
    static constexpr int N = 8, kTaps = 16;
    void prepare (double sampleRate)
    {
        sr = sampleRate; geq.prepare (sr);
        auto pow2 = [] (double n) { size_t m = 1; while ((double) m < n) m <<= 1; return m; };
        for (int i = 0; i < N; ++i)
        {
            auto& l = ln[i];
            l.dl.assign (pow2 (kDelayMs[i] * kMaxScale * sr / 1000.0 + 64 + 8 * sr / 1000.0), 0.0f); l.mask = (int) l.dl.size() - 1;
            l.ap.assign (pow2 (kApMs[i] * kMaxScale * sr / 1000.0 + 8), 0.0f); l.apMask = (int) l.ap.size() - 1;
        }
        sw.assign (pow2 (3.2 * sr + 8), 0.0f); swMask = (int) sw.size() - 1;
        for (int i = 0; i < 4; ++i) { inAp[i].assign (pow2 (kInMs[i] * sr / 1000.0 + 4), 0.0f); }
        fbd.assign (pow2 (0.6 * sr + 8), 0.0f); fbMask = (int) fbd.size() - 1;
        reset(); configured = false;
    }
    void reset()
    {
        for (auto& l : ln) { std::fill (l.dl.begin(), l.dl.end(), 0.0f); std::fill (l.ap.begin(), l.ap.end(), 0.0f); l.w = l.aw = 0; l.v = 0; for (auto& f : l.filt) f.reset(); }
        std::fill (sw.begin(), sw.end(), 0.0f); sww = 0;
        for (auto& a : inAp) std::fill (a.begin(), a.end(), 0.0f); for (auto& w : inW) w = 0;
        std::fill (fbd.begin(), fbd.end(), 0.0f); fbw = 0; fbLp = 0;
        inGain = freezeIn ? 0.0f : 1.0f;
    }
    void setup (const TailSettings& t, bool instant = false)
    {
        scaleT = clampd (t.sizeMs / 80.0 * 1.5, 0.25, kMaxScale); if (instant || ! configured) scale = scaleT;
        // a big jump (a preset): gliding there would sweep the pitch for seconds; fade out, start the space anew, fade in
        else if (std::abs (std::log (scaleT / scale)) > 0.25) jump = true;
        dif = 0.45 + 0.3 * clampd (t.diffusion, 0, 1);
        excT = clampd (t.modDepthMs, 0, 2.0) * 4.0 * sr / 1000.0; if (instant || ! configured) exc = excT;
        rateW = 2 * kPi * clampd (t.modRate, 0.05, 5.0) * 0.35 / sr;
        freezeIn = t.freeze;
        fbT = clampd (t.feedback, 0, 0.9); if (instant || ! configured) fb = fbT;
        // gravity: tap weights over Ts (a part of the 1 kHz decay, up to 3 s)
        const double g = clampd (t.gravity, -1, 1), Ts = clampd (0.45 * t.rt.rt[4], 0.15, 3.0);
        double e = 0, wv[kTaps];
        for (int k = 0; k < kTaps; ++k)
        {
            const double x = (k + 0.5) / kTaps;                                  // 0..1 over Ts
            wv[k] = std::abs (g) * (g < 0 ? std::exp (3.5 * (x - 1)) : std::exp (-5.0 * x));
            e += wv[k] * wv[k];
        }
        const double dry = 1 - std::abs (g);
        const double norm = 1.0 / std::sqrt (dry * dry + e + 1e-12);
        tapDry = (float) (dry * norm);
        for (int k = 0; k < kTaps; ++k) { tapW[k] = (float) (wv[k] * norm); tapD[k] = (float) (((k + 0.5) / kTaps + 0.013 * ((k * 7) % 5)) * Ts * sr); }
        for (int i = 0; i < N; ++i)
        {
            const double Ls = (kDelayMs[i] + kApMs[i]) * scaleT / 1000.0 * 1.06;      // (x 1.06: the allpass's delay, measured)
            double tgt[kBands], gg[kBands];
            for (int b = 0; b < kBands; ++b) tgt[b] = t.freeze ? 0.0 : -60.0 * Ls / clampd (t.rt.rt[(size_t) b], 0.05, 120);
            geq.gains (tgt, gg); geq.refine (tgt, gg, 2);
            for (int b = 0; b < kBands; ++b) { gTgt[i][b] = (float) gg[b]; if (instant || ! configured) { gCur[i][b] = gTgt[i][b]; geq.make (ln[i].filt[(size_t) b], b, gCur[i][b]); } }
        }
        configured = true;
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float inTarget = freezeIn ? 0.0f : 1.0f, inStep = (float) (1.0 / (0.05 * sr));
        for (int s = 0; s < n; ++s)
        {
            if (jump)
            {   // 20 ms out, then the new size from silence
                duck -= (float) (1.0 / (0.02 * sr));
                if (duck <= 0) { duck = 0; const bool fz = freezeIn; reset(); freezeIn = fz; inGain = fz ? 0.0f : 1.0f; scale = scaleT; jump = false; }
            }
            else if (duck < 1) duck = std::min (1.0f, duck + (float) (1.0 / (0.02 * sr)));
            if ((++cnt & 15) == 0)
            {
                const double lim = 0.03 * 16 / (kDelayMs[N - 1] * sr / 1000.0);       // (at most a 3 % pitch drift while Size moves)
                scale += std::clamp ((scaleT - scale) * 0.01, -lim, lim);
                exc += (excT - exc) * 0.01; fb += (fbT - fb) * 0.01;
                for (int i = 0; i < N; ++i) for (int b = 0; b < kBands; ++b)
                {
                    const float d = gTgt[i][b] - gCur[i][b]; if (std::abs (d) < 1e-6f) continue;
                    gCur[i][b] = std::abs (d) < 1e-4f ? gTgt[i][b] : gCur[i][b] + d * 0.015f;
                    geq.make (ln[i].filt[(size_t) b], b, gCur[i][b]);
                }
            }
            inGain += inGain < inTarget ? std::min (inStep, inTarget - inGain) : -std::min (inStep, inGain - inTarget);
            // FEEDBACK: the output, 0.5 s later, darker, through a soft limiter
            const float fbo = fbd[(size_t) ((fbw - (int) (0.5 * sr)) & fbMask)];
            fbLp += 0.35f * (fbo - fbLp);
            float x = 0.5f * (inL[s] + inR[s]) * inGain + (float) fb * std::tanh (fbLp);
            // GRAVITY: the tap bank (swell / front-heavy)
            sw[(size_t) sww] = x;
            float g = tapDry * x;
            if (tapDry < 0.999f) for (int k = 0; k < kTaps; ++k) g += tapW[k] * sw[(size_t) ((sww - (int) tapD[k]) & swMask)];
            sww = (sww + 1) & swMask;
            // input diffusion (4 allpasses)
            for (int i = 0; i < 4; ++i)
            {
                auto& a = inAp[i]; const int len = (int) (kInMs[i] * sr / 1000.0); auto& w = inW[i];
                const float d = a[(size_t) ((w - len) & (int) (a.size() - 1))], v = g + 0.6f * d;
                a[(size_t) w] = v; w = (w + 1) & (int) (a.size() - 1); g = d - 0.6f * v;
            }
            ph += rateW; if (ph > 2 * kPi) ph -= 2 * kPi;
            // the lines
            float o[N], sum = 0;
            for (int i = 0; i < N; ++i)
            {
                auto& l = ln[i];
                const double d = kDelayMs[i] * scale * sr / 1000.0 + exc * (1 + std::sin (ph * (1 + 0.13 * i) + i * 0.9));
                float v = readCubic (l.dl, l.mask, l.w, d);
                // the allpass in the loop (smear)
                // (a fractional read: its length glides with Size, an integer jump would click)
                const float ad = readCubic (l.ap, l.apMask, l.aw, kApMs[i] * scale * sr / 1000.0), av = v + (float) dif * ad;
                l.ap[(size_t) l.aw] = av; l.aw = (l.aw + 1) & l.apMask; v = ad - (float) dif * av;
                for (auto& f : l.filt) v = f.run (v);
                o[i] = v; sum += v;
            }
            const float hh = sum * (2.0f / N);
            float L = 0, R = 0;
            for (int i = 0; i < N; ++i)
            {
                auto& l = ln[i];
                l.dl[(size_t) l.w] = o[i] - hh + g * kInSign[i];
                l.w = (l.w + 1) & l.mask;
                (i & 1 ? R : L) += o[i] * kOutSign[i];
            }
            fbd[(size_t) fbw] = 0.35f * (L + R); fbw = (fbw + 1) & fbMask;          // (taken before the output gain: the loop's gain does not depend on it)
            L *= kOut * duck; R *= kOut * duck;
            outL[s] = L; outR[s] = R;
        }
    }
private:
    struct Line { std::vector<float> dl, ap; int mask = 0, apMask = 0, w = 0, aw = 0; float v = 0; std::array<BQ, kBands> filt; };
    static float readCubic (const std::vector<float>& b, int mask, int w, double d)
    {
        d = std::max (1.0, std::min (d, (double) mask - 4));
        const int di = (int) d; const float fr = (float) (d - di);
        const int i0 = (w - di) & mask;
        const float sm = b[(size_t) ((i0 + 1) & mask)], s0 = b[(size_t) i0], s1 = b[(size_t) ((i0 - 1) & mask)], s2 = b[(size_t) ((i0 - 2) & mask)];
        const float xm = fr + 1, x1 = fr - 1, x2 = fr - 2;
        return -fr * x1 * x2 * (1.0f / 6.0f) * sm + xm * x1 * x2 * 0.5f * s0 - xm * fr * x2 * 0.5f * s1 + xm * fr * x1 * (1.0f / 6.0f) * s2;
    }
    static constexpr double kMaxScale = 5.0;
    static constexpr double kDelayMs[N] = { 61, 79, 97, 113, 131, 149, 167, 191 };
    static constexpr double kApMs[N] = { 7.1, 11.3, 13.7, 17.9, 19.3, 23.1, 29.3, 31.7 };
    static constexpr double kInMs[4] = { 4.77, 3.59, 12.73, 9.31 };
    static constexpr float kInSign[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
    static constexpr float kOutSign[N] = { 1, 1, -1, 1, -1, 1, 1, -1 };
    static constexpr float kOut = 1.5f;            // about the FDN's level (for the same decay)
    double sr = 48000; GeqDesign geq;
    std::array<Line, N> ln; std::vector<float> sw, fbd; std::array<std::vector<float>, 4> inAp; int inW[4] {}; int swMask = 0, sww = 0, fbMask = 0, fbw = 0;
    float gTgt[N][kBands] {}, gCur[N][kBands] {}, tapW[kTaps] {}, tapD[kTaps] {}, tapDry = 1, inGain = 1, fbLp = 0;
    double scale = 1, scaleT = 1, dif = 0.6, exc = 0, excT = 0, rateW = 0, ph = 0, fb = 0, fbT = 0; unsigned cnt = 0;
    bool freezeIn = false, configured = false, jump = false; float duck = 1;
};

// ---------------------------------------------------------------------------
//  The tail: the FDN or the plate (30 ms crossfade when the engine changes)
// ---------------------------------------------------------------------------
class Fdn
{
public:
    static constexpr double kMaxMs = FdnCore::kMaxMs;
    static constexpr int kEngines = 4;                         // 0 FDN (rooms / halls), 1 plate, 2 spring, 3 ambient
    void prepare (double s) { fdn.prepare (s); fdn2.prepare (s, 400.0); d2L.assign ((size_t) (0.25 * s) + 8, 0.0f); d2R.assign (d2L.size(), 0.0f); d2w = 0; plate.prepare (s); spring.prepare (s); amb.prepare (s); sr = s; spread.prepare (s); spreadH.prepare (s); for (auto& l : xLp) for (auto& b : l) { b.pass (false, kSplitHz, 0.70710678, s); b.reset(); } for (auto& l : xHp) for (auto& b : l) { b.pass (true, kSplitHz, 0.70710678, s); b.reset(); } for (auto& b : bwLp) { b.pass (false, 250.0, 0.70710678, s); b.reset(); } for (auto& b : bwHp) { b.pass (true, 250.0, 0.70710678, s); b.reset(); } }
    void reset() { fdn.reset(); fdn2.reset(); std::fill (d2L.begin(), d2L.end(), 0.0f); std::fill (d2R.begin(), d2R.end(), 0.0f); plate.reset(); spring.reset(); amb.reset(); for (auto& r : ramp) r = 1; spread.reset(); spreadH.reset(); for (auto& l : xLp) for (auto& b : l) b.reset(); for (auto& l : xHp) for (auto& b : l) b.reset(); for (auto& b : bwLp) b.reset(); for (auto& b : bwHp) b.reset(); }
    void rampIn() { for (auto& r : ramp) r = 0; }               // after a reset in the running signal: the input fades in
    void setup (const TailSettings& t, bool instant = false)
    {
        fdn.setup (t, instant); plate.setup (t, instant); spring.setup (t, instant); amb.setup (t, instant);
        {   // two slopes: the second FDN, its input delay and the two levels
            if (t.dual)
            {
                TailSettings t2 = t; t2.rt = t.rt2; t2.sizeMs = t.sizeMs2; t2.fill = 0; t2.modDepthMs = t.modDepthMs;
                if (! dual2 || instant) fdn2.reset ();
                fdn2.setup (t2, instant || ! dual2);
                d2len = std::clamp ((int) std::lround (t.delay2Ms * sr / 1000.0), 0, (int) d2L.size() - 2);
                g1t = (float) t.gain1; g2t = (float) t.gain2;
                if (instant) { g1c = g1t; g2c = g2t; }                     // (otherwise the levels glide from where they are: a step in a ringing tail would click)
            }
            else { g1t = 1.0f; g2t = 0.0f; if (instant) { g1c = 1.0f; g2c = 0.0f; } }
            dual2 = t.dual;
        }
        splitOn = t.spreadMs >= 1.0;                                 // (always split while Spread is on: a change of the ratio then only crossfades the taps, no path switch)
        spread.set (t.spreadMs, t.shape); if (instant) spread.setInstant();
        spreadH.set (splitOn ? t.spreadMs * t.spreadHi : 0.0, t.shape); if (instant) spreadH.setInstant();
        bassW = (float) std::clamp (t.bassWidth, 0.0, 1.0);
        const int e = juce::jlimit (0, kEngines - 1, t.engine);
        if (instant || ! configured) { eng = e; for (int i = 0; i < kEngines; ++i) w[i] = i == e ? 1.0f : 0.0f; if (e == 3) amb.reset(); }
        else if (e != eng)
        {   // an engine that was silent starts clean (only the engine that is heard, and one fading out, run)
            if (w[e] == 0)
            {   // (its glides jump straight to the settings: it was not heard)
                if (e == 0) { fdn.reset(); fdn.setup (t, true); } else if (e == 1) { plate.reset(); plate.setup (t, true); }
                else if (e == 2) { spring.reset(); spring.setup (t, true); } else { amb.reset(); amb.setup (t, true); }
                ramp[e] = 0;                                    // and its input fades in (a running signal starting at once would click)
            }
            eng = e;
        }
        configured = true;
    }
    void process (const float* inL0, const float* inR0, float* outL, float* outR, int n)
    {
        // build-up: the input smeared first (Spread / Shape)
        const float* inL = inL0; const float* inR = inR0;
        if (spread.active() && splitOn)
        {   // 0.7.3: split at ~1.5 kHz (Linkwitz-Riley), each band built up over its own time, then added back
            if ((int) sL.size() < n) { sL.resize ((size_t) n); sR.resize ((size_t) n); }
            if ((int) hL.size() < n) { hL.resize ((size_t) n); hR.resize ((size_t) n); }
            for (int i = 0; i < n; ++i)
            {
                sL[(size_t) i] = xLp[0][1].run (xLp[0][0].run (inL0[i])); hL[(size_t) i] = xHp[0][1].run (xHp[0][0].run (inL0[i]));
                sR[(size_t) i] = xLp[1][1].run (xLp[1][0].run (inR0[i])); hR[(size_t) i] = xHp[1][1].run (xHp[1][0].run (inR0[i]));
            }
            spread.process (sL.data(), sR.data(), n); spreadH.process (hL.data(), hR.data(), n);
            for (int i = 0; i < n; ++i) { sL[(size_t) i] += hL[(size_t) i]; sR[(size_t) i] += hR[(size_t) i]; }
            inL = sL.data(); inR = sR.data();
        }
        else if (spread.active())
        {
            if ((int) sL.size() < n) { sL.resize ((size_t) n); sR.resize ((size_t) n); }
            std::copy (inL0, inL0 + n, sL.begin()); std::copy (inR0, inR0 + n, sR.begin());
            spread.process (sL.data(), sR.data(), n); inL = sL.data(); inR = sR.data();
        }
        core (inL, inR, outL, outR, n);
        if (bassW < 0.999f)
        {   // the side signal split at 250 Hz (Linkwitz-Riley 24 dB/oct, the halves add back flat); its low half turned down:
            // a narrower low end, as in real rooms
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (outL[i] + outR[i]), sd = 0.5f * (outL[i] - outR[i]);
                const float lo = bwLp[1].run (bwLp[0].run (sd)), hi = bwHp[1].run (bwHp[0].run (sd));
                const float s2 = hi + bassW * lo;
                outL[i] = m + s2; outR[i] = m - s2;
            }
        }
    }
private:
    static constexpr double kSplitHz = 1500.0;
    Spreader spread, spreadH; std::vector<float> sL, sR, hL, hR; float bassW = 1; BQ bwLp[2], bwHp[2], xLp[2][2], xHp[2][2]; bool splitOn = false;
    void core (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        if ((int) tmpL.size() < n) for (auto* v : { &tmpL, &tmpR, &tmp2L, &tmp2R, &tmp3L, &tmp3R }) v->resize ((size_t) n);
        // CPU: each engine runs only while it is heard (selected, or fading out after a switch)
        const bool plOn = eng == 1 || w[1] > 0, spOn = eng == 2 || w[2] > 0, ambOn = eng == 3 || w[3] > 0;
        if ((int) rL.size() < n) { rL.resize ((size_t) n); rR.resize ((size_t) n); }
        // the input of an engine that has just started: ramped in over 20 ms
        auto feed = [&] (int e, const float*& a, const float*& b)
        {
            a = inL; b = inR; if (ramp[e] >= 1.0f) return;
            const float st = (float) (1.0 / (0.02 * sr));
            for (int i = 0; i < n; ++i) { ramp[e] = std::min (1.0f, ramp[e] + st); rL[(size_t) i] = inL[i] * ramp[e]; rR[(size_t) i] = inR[i] * ramp[e]; }
            a = rL.data(); b = rR.data();
        };
        const float *xa, *xb;
        if (plOn) { feed (1, xa, xb); plate.process (xa, xb, tmpL.data(), tmpR.data(), n); } else { std::fill (tmpL.begin(), tmpL.begin() + n, 0.0f); std::fill (tmpR.begin(), tmpR.begin() + n, 0.0f); }
        if (spOn) { feed (2, xa, xb); spring.process (xa, xb, tmp2L.data(), tmp2R.data(), n); } else { std::fill (tmp2L.begin(), tmp2L.begin() + n, 0.0f); std::fill (tmp2R.begin(), tmp2R.begin() + n, 0.0f); }
        if (ambOn) { feed (3, xa, xb); amb.process (xa, xb, tmp3L.data(), tmp3R.data(), n); }
        const bool settled = w[eng] == 1.0f;
        if (settled && eng != 0)
        {
            const auto& L = eng == 1 ? tmpL : eng == 2 ? tmp2L : tmp3L; const auto& R = eng == 1 ? tmpR : eng == 2 ? tmp2R : tmp3R;
            std::copy (L.begin(), L.begin() + n, outL); std::copy (R.begin(), R.begin() + n, outR); return;
        }
        if (w[0] > 0 || eng == 0)
        {
            feed (0, xa, xb); fdn.process (xa, xb, outL, outR, n);
            if (dual2 || g2c > 1e-4f)
            {   // the second slope: the same input (a little later), its own FDN, added by its level
                if ((int) o2L.size() < n) { o2L.resize ((size_t) n); o2R.resize ((size_t) n); dIL.resize ((size_t) n); dIR.resize ((size_t) n); }
                const int ln = (int) d2L.size();
                for (int i = 0; i < n; ++i)
                {
                    d2L[(size_t) d2w] = xa[i]; d2R[(size_t) d2w] = xb[i];
                    const int r = (d2w - d2len + ln) % ln; dIL[(size_t) i] = d2L[(size_t) r]; dIR[(size_t) i] = d2R[(size_t) r];
                    if (++d2w >= ln) d2w = 0;
                }
                fdn2.process (dIL.data(), dIR.data(), o2L.data(), o2R.data(), n);
                for (int i = 0; i < n; ++i)
                {
                    g1c += (g1t - g1c) * 0.0005f; g2c += (g2t - g2c) * 0.0005f;
                    outL[i] = outL[i] * g1c + o2L[(size_t) i] * g2c; outR[i] = outR[i] * g1c + o2R[(size_t) i] * g2c;
                }
            }
        }
        else { std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f); }
        if (settled) return;                                   // (FDN alone)
        // crossfade, 30 ms
        const float st = (float) (1.0 / (0.03 * sr));
        for (int i = 0; i < n; ++i)
        {
            for (int e = 0; e < kEngines; ++e) { const float tg = e == eng ? 1.0f : 0.0f; w[e] += w[e] < tg ? std::min (st, tg - w[e]) : -std::min (st, w[e] - tg); }
            const float a3 = ambOn ? w[3] : 0.0f;
            outL[i] = outL[i] * w[0] + tmpL[(size_t) i] * w[1] + tmp2L[(size_t) i] * w[2] + (ambOn ? tmp3L[(size_t) i] * a3 : 0.0f);
            outR[i] = outR[i] * w[0] + tmpR[(size_t) i] * w[1] + tmp2R[(size_t) i] * w[2] + (ambOn ? tmp3R[(size_t) i] * a3 : 0.0f);
        }
    }
public:
    double sampleRate() const { return fdn.sampleRate(); }
private:
    FdnCore fdn, fdn2; std::vector<float> d2L, d2R, o2L, o2R, dIL, dIR; int d2w = 0, d2len = 0; bool dual2 = false; float g1c = 1, g2c = 0, g1t = 1, g2t = 0;
    Plate plate; Spring spring; Ambient amb; double sr = 48000; int eng = 0; float w[kEngines] { 1, 0, 0, 0 }; bool configured = false;
    std::vector<float> tmpL, tmpR, tmp2L, tmp2R, tmp3L, tmp3R, rL, rR; float ramp[kEngines] { 1, 1, 1, 1 };
};

// ---------------------------------------------------------------------------
//  Butterworth low / high cut with a slope choice (6 / 12 / 18 / 24 / 36 / 48 dB/oct), stereo
// ---------------------------------------------------------------------------
struct SlopeFilter
{
    std::array<BQ, 4> st[2]; int n = 0; bool on = false;
    float mix = 0, step = 0.004f;                  // switching on / off crossfades (5 ms): a filter starting from silence would click
    void set (bool high, double fc, int slopeIdx, double sr, bool enabled)
    {
        static const int orders[6] = { 1, 2, 3, 4, 6, 8 };
        const int N = orders[std::clamp (slopeIdx, 0, 5)];
        step = (float) (1.0 / (0.005 * sr));
        if (enabled && ! on && mix <= 0) for (auto& c : st) for (auto& b : c) b.reset();
        on = enabled;
        if (! on && mix <= 0) return;                   // keep the sections while fading out
        BQ proto[4]; int m = 0;
        if (N & 1) proto[m++].pass1 (high, fc, sr);
        for (int k = 1; k <= N / 2; ++k)
        {
            const double th = (N & 1) ? kPi * k / N : kPi * (2 * k - 1) / (2 * N);
            proto[m++].pass (high, fc, 1.0 / (2 * std::cos (th)), sr);
        }
        for (int c = 0; c < 2; ++c) for (int i = 0; i < m; ++i) { const float z1 = st[c][(size_t) i].z1, z2 = st[c][(size_t) i].z2; st[c][(size_t) i] = proto[i]; st[c][(size_t) i].z1 = z1; st[c][(size_t) i].z2 = z2; }
        n = m;
    }
    inline float run (float x, int c)
    {
        if (c == 0) { if (on) mix = std::min (1.0f, mix + step); else mix = std::max (0.0f, mix - step); }
        if (mix <= 0) return x;
        float y = x; for (int i = 0; i < n; ++i) y = st[c][(size_t) i].run (y);
        return mix >= 1 ? y : x + mix * (y - x);
    }
    void reset() { for (auto& s : st) for (auto& b : s) b.reset(); mix = on ? 1.0f : 0.0f; }
    int sections() const { return on ? n : 0; }
};

// input filter: low cut, high cut, tilt (pivot 1 kHz)
struct InputFilter
{
    SlopeFilter lc, hc; BQ tilt[2][2]; bool tiltOn = false;
    void set (double lcHz, int lcS, double hcHz, int hcS, double tiltDb, double sr)
    {
        lc.set (true, lcHz, lcS, sr, lcHz > 20.5); hc.set (false, hcHz, hcS, sr, hcHz < 19900);
        tiltOn = std::abs (tiltDb) > 0.05;
        for (int c = 0; c < 2; ++c)
        {
            BQ a, b; a.shelf (false, 250, -tiltDb / 2, sr, 0.6); b.shelf (true, 4000, tiltDb / 2, sr, 0.6);
            for (int i = 0; i < 2; ++i) { auto& t = tilt[c][i]; const float z1 = t.z1, z2 = t.z2; t = i == 0 ? a : b; t.z1 = z1; t.z2 = z2; }
        }
    }
    inline float run (float x, int c) { x = lc.run (x, c); x = hc.run (x, c); if (tiltOn) { x = tilt[c][0].run (x); x = tilt[c][1].run (x); } return x; }
    void reset() { lc.reset(); hc.reset(); for (auto& t : tilt) for (auto& b : t) b.reset(); }
    double magDb (double f, double sr) const
    {
        double d = 0;
        for (int i = 0; i < lc.sections(); ++i) d += lc.st[0][(size_t) i].magDb (f, sr);
        for (int i = 0; i < hc.sections(); ++i) d += hc.st[0][(size_t) i].magDb (f, sr);
        if (tiltOn) d += tilt[0][0].magDb (f, sr) + tilt[0][1].magDb (f, sr);
        return d;
    }
};

// master EQ on the reverb output: HPF, low shelf, 4 peaks, high shelf, LPF
struct EqBand { double f = 1000, g = 0, q = 1; };
struct MasterEQ
{
    static constexpr int NB = 6;
    BQ b[2][NB]; SlopeFilter hp, lp; bool on = true;
    void set (const EqBand* bands, double hpHz, double lpHz, double sr, bool enabled)
    {
        on = enabled;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < NB; ++i)
            {
                BQ t; const auto& e = bands[i];
                if (i == 0) t.shelf (false, e.f, e.g, sr, clampd (e.q, 0.3, 1.0));
                else if (i == NB - 1) t.shelf (true, e.f, e.g, sr, clampd (e.q, 0.3, 1.0));
                else t.peak (e.f, e.q, e.g, sr);
                t.z1 = b[c][i].z1; t.z2 = b[c][i].z2; b[c][i] = t;
            }
        hp.set (true, hpHz, 1, sr, hpHz > 20.5); lp.set (false, lpHz, 1, sr, lpHz < 19900);
    }
    inline float run (float x, int c)
    {
        if (! on) return x;
        x = hp.run (x, c); x = lp.run (x, c);
        for (int i = 0; i < NB; ++i) x = b[c][i].run (x);
        return x;
    }
    void reset() { for (auto& c : b) for (auto& f : c) f.reset(); hp.reset(); lp.reset(); }
    double magDb (double f, double sr) const
    {
        if (! on) return 0;
        double d = 0;
        for (int i = 0; i < NB; ++i) d += b[0][i].magDb (f, sr);
        for (int i = 0; i < hp.sections(); ++i) d += hp.st[0][(size_t) i].magDb (f, sr);
        for (int i = 0; i < lp.sections(); ++i) d += lp.st[0][(size_t) i].magDb (f, sr);
        return d;
    }
};

// ---------------------------------------------------------------------------
//  Measurement of an impulse response (Schroeder integration per octave band)
// ---------------------------------------------------------------------------
constexpr int kMBands = 8;                                     // 63 Hz .. 8 kHz
inline double mBandFc (int k) { return 62.5 * std::pow (2.0, k); }
struct Measure
{
    std::array<double, kMBands> t30 {}, edt {};               // seconds (0 = not measurable)
    double t30Wide = 0, edtWide = 0, c80 = 0, d50 = 0, iacc = 0;
    bool valid = false;
};
// decay time from the backward-integrated energy: fit between a dB and b dB, scaled to 60 dB
inline double decayFit (const std::vector<double>& edc, double sr, double a, double b)
{
    const double e0 = edc.empty() ? 0 : edc[0]; if (e0 <= 0) return 0;
    int i0 = -1, i1 = -1;
    for (size_t i = 0; i < edc.size(); ++i)
    {
        const double d = 10 * std::log10 (std::max (edc[i] / e0, 1e-30));
        if (i0 < 0 && d <= a) i0 = (int) i;
        if (d <= b) { i1 = (int) i; break; }
    }
    if (i0 < 0 || i1 <= i0 + 2) return 0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
    const int step = std::max (1, (i1 - i0) / 400);
    for (int i = i0; i <= i1; i += step)
    {
        const double x = i / sr, y = 10 * std::log10 (std::max (edc[(size_t) i] / e0, 1e-30));
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++m;
    }
    const double den = m * sxx - sx * sx; if (den <= 0) return 0;
    const double slope = (m * sxy - sx * sy) / den;           // dB per second
    return slope < -1e-6 ? -60.0 / slope : 0;
}
inline std::vector<double> schroeder (const std::vector<float>& L, const std::vector<float>& R)
{
    std::vector<double> e (L.size(), 0.0); double acc = 0;
    for (size_t i = L.size(); i-- > 0;) { acc += (double) L[i] * L[i] + (double) R[i] * R[i]; e[i] = acc; }
    return e;
}
// IR starts at sample 0 with the direct sound (or the first energy)
inline Measure measureIR (const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    Measure m; if (L.size() < 256) return m;
    for (int k = 0; k < kMBands; ++k)
    {
        // octave band: 4th-order band pass (2 x high pass + 2 x low pass at the band edges)
        BQ f[4]; const double fc = mBandFc (k), lo = fc / std::sqrt (2.0), hi = std::min (fc * std::sqrt (2.0), sr * 0.45);
        std::vector<float> bl (L.size()), br (R.size());
        for (int c = 0; c < 2; ++c)
        {
            f[0].pass (true, lo, 0.7071, sr); f[1].pass (true, lo, 0.7071, sr); f[2].pass (false, hi, 0.7071, sr); f[3].pass (false, hi, 0.7071, sr);
            const auto& src = c == 0 ? L : R; auto& dst = c == 0 ? bl : br;
            for (size_t i = 0; i < src.size(); ++i) { float x = src[i]; for (auto& q : f) x = q.run (x); dst[i] = x; }
        }
        const auto edc = schroeder (bl, br);
        double t = decayFit (edc, sr, -5, -35); if (t <= 0) t = decayFit (edc, sr, -5, -25);
        m.t30[(size_t) k] = t; m.edt[(size_t) k] = decayFit (edc, sr, -0.01, -10);
    }
    const auto edc = schroeder (L, R);
    m.t30Wide = decayFit (edc, sr, -5, -35); m.edtWide = decayFit (edc, sr, -0.01, -10);
    auto eSum = [&] (size_t a, size_t b) { double s = 0; for (size_t i = a; i < std::min (b, L.size()); ++i) s += (double) L[i] * L[i] + (double) R[i] * R[i]; return s; };
    const size_t n50 = (size_t) (0.05 * sr), n80 = (size_t) (0.08 * sr);
    const double eAll = eSum (0, L.size()), e50 = eSum (0, n50), e80 = eSum (0, n80);
    m.c80 = 10 * std::log10 (std::max (e80, 1e-30) / std::max (eAll - e80, 1e-30));
    m.d50 = eAll > 0 ? e50 / eAll : 0;
    {   // early IACC: max normalised cross-correlation within +-1 ms over 0..80 ms
        const int lag = (int) (0.001 * sr); double el = 0, er2 = 0, best = 0;
        for (size_t i = 0; i < std::min (n80, L.size()); ++i) { el += (double) L[i] * L[i]; er2 += (double) R[i] * R[i]; }
        const double nrm = std::sqrt (el * er2);
        for (int d = -lag; d <= lag; ++d)
        {
            double s = 0;
            for (size_t i = 0; i < std::min (n80, L.size()); ++i) { const long j = (long) i + d; if (j < 0 || j >= (long) R.size()) continue; s += (double) L[i] * R[(size_t) j]; }
            best = std::max (best, std::abs (s));
        }
        m.iacc = nrm > 0 ? best / nrm : 0;
    }
    m.valid = true;
    return m;
}
}
