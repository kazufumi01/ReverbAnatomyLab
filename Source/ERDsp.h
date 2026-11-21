#pragma once
// Realtime early-reflection engine: multi-tap delay with per-tap one-pole tone,
// cross-faded tap sets, diffusion, vintage colour, wet filters and a small tail preview.
// Copyright (c) 2026 Johan Plus Kei. All rights reserved.
#include "ERCore.h"
#include <juce_dsp/juce_dsp.h>

namespace er
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; double z1[2] {}, z2[2] {};
    void lp (double fc, double sr, double q = 0.7071) { calc (fc, sr, q, true); }
    void hp (double fc, double sr, double q = 0.7071) { calc (fc, sr, q, false); }
    void hp1 (double fc, double sr)          // first-order high pass
    {
        const double K = std::tan (kPi * std::min (fc, sr * 0.49) / sr);
        b0 = 1 / (1 + K); b1 = -b0; b2 = 0; a1 = (K - 1) / (K + 1); a2 = 0;
    }
    void lp1 (double fc, double sr)          // first-order low pass (6 dB/oct) in the same structure
    {
        const double K = std::tan (kPi * std::min (fc, sr * 0.49) / sr);
        b0 = b1 = K / (1 + K); b2 = 0; a1 = (K - 1) / (K + 1); a2 = 0;
    }
    void calc (double fc, double sr, double q, bool low)
    {
        fc = std::min (fc, sr * 0.49); const double w = 2 * kPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        if (low) { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; } else { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float run (float x, int ch) { const double y = b0 * x + z1[ch]; z1[ch] = b1 * x - a1 * y + z2[ch]; z2[ch] = b2 * x - a2 * y; return (float) y; }
};

// a 2-pole low pass as a state-variable filter (topology-preserving): its cutoff can move while it runs without clicks (0.8.0, Bandwidth)
struct Svf
{
    double a1 = 1, a2 = 0, a3 = 0, ic1[2] {}, ic2[2] {};
    void lp (double fc, double sr, double q)
    {
        const double g = std::tan (kPi * std::min (fc, sr * 0.49) / sr), k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
    inline float run (float x, int ch)
    {
        const double v3 = x - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2 * v1 - ic1[ch]; ic2[ch] = 2 * v2 - ic2[ch]; return (float) v2;
    }
};

struct Settings                       // everything except the tap set itself
{
    int hcSlope = 1, lcSlope = 1;     // 0..5 = 6 / 12 / 18 / 24 / 36 / 48 dB/oct
    double diffusion = 0, sizeScale = 1, lowCut = 20, highCut = 20000, levelDb = 0, bwKHz = 20, bits = 24, wander = 0;
    double width = 1; bool convWidth = false;     // conv mode: width as M/S on the wet signal
    double mix = 0.5, outDb = 0; bool tail = false; double tailRT = 1.2, tailDb = -6;
};

// ---------------------------------------------------------------------------
//  Post chain (wet): diffusion -> bandwidth -> bits -> low / high cut -> width (conv) -> level
//  Also used offline for the echogram preview, so the picture is the sound.
// ---------------------------------------------------------------------------
// the wet signal sits well below full scale, so a true 16-bit step would stay far below hearing: the step is that many bits coarser (Bits 24 stays off)
constexpr double kBitsHeadroom = 3.0;
class Post
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : ap) v.assign ((size_t) (sr * 0.016) + 4, 0.0f);
        reset(); dirty = true;
    }
    void reset()
    {
        for (auto& v : ap) std::fill (v.begin(), v.end(), 0.0f);
        for (auto& row : apPos) for (auto& p : row) p = 0;
        bw1.reset(); bw2.reset(); for (auto& f : lc) f.reset(); for (auto& f : hc) f.reset();
        lvl = (float) db2g (s.levelDb);
        bwCur = -1; bwG = s.bwKHz < 19.9 ? 1.0f : 0.0f;          // (offline: no glide)
    }
    void set (const Settings& n)
    {
        if (n.lowCut != s.lowCut || n.highCut != s.highCut || n.hcSlope != s.hcSlope || n.lcSlope != s.lcSlope || n.sizeScale != s.sizeScale) dirty = true;      // (Bandwidth glides in process)
        s = n;
        if (dirty)
        {
            // Butterworth filters of order N as cascaded sections (odd N: one first-order section)
            auto build = [&] (std::array<Biquad, 5>& f, int& cnt, int slope, double fc, bool low)
            {
                static const int orders[6] = { 1, 2, 3, 4, 6, 8 };
                const int N = orders[std::clamp (slope, 0, 5)]; cnt = 0;
                if (N % 2 == 1) { if (low) f[(size_t) cnt++].lp1 (fc, sr); else f[(size_t) cnt++].hp1 (fc, sr); }
                for (int k = 1; k <= N / 2; ++k)
                {
                    const double q = N % 2 == 1 ? 1.0 / (2 * std::cos (k * kPi / N)) : 1.0 / (2 * std::cos ((2 * k - 1) * kPi / (2 * N)));
                    if (low) f[(size_t) cnt++].lp (fc, sr, q); else f[(size_t) cnt++].hp (fc, sr, q);
                }
            };
            build (hc, nHc, s.hcSlope, s.highCut, true);
            build (lc, nLc, s.lcSlope, std::max (10.0, s.lowCut), false);
            static const double base[2][4] = { { 1.13, 2.37, 3.71, 5.27 }, { 1.31, 2.59, 3.97, 5.63 } };
            const double sc = clampd (s.sizeScale, 0.5, 2.0);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 4; ++i)
                apLen[c][i] = std::min ((int) ap[(size_t) (c * 4 + i)].size() - 1, std::max (8, (int) std::lround (base[c][i] * sc * sr / 1000.0)));
            dirty = false;
        }
    }
    void process (float* L, float* R, int n)
    {
        const float d = (float) clampd (s.diffusion, 0, 1), g = 0.62f;
        const bool useBw = s.bwKHz < 19.9, useBits = s.bits < 23.5, useLc = s.lowCut > 20.5, useHc = s.highCut < 19900;
        const float q = (float) std::pow (2.0, s.bits - 1 - kBitsHeadroom), tgt = (float) db2g (s.levelDb);
        // (0.8.0) Bandwidth: the filter always runs and its cutoff glides; it fades in / out at the 20 kHz end (a jump in the
        // coefficients, or a cold filter switched in, clicked)
        const double bwT = std::min (s.bwKHz * 1000.0, 0.45 * sr);
        if (bwCur < 0) { bwCur = bwT; bw1.lp (bwCur, sr, 0.54); bw2.lp (bwCur, sr, 1.31); }
        const float bwTg = useBw ? 1.0f : 0.0f, bwK = (float) (1.0 / (0.015 * sr));
        const float wm = (float) s.width;
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0 && std::abs (bwT / bwCur - 1.0) > 1e-4) { bwCur *= std::pow (bwT / bwCur, 0.06); bw1.lp (bwCur, sr, 0.54); bw2.lp (bwCur, sr, 1.31); }
            bwG += std::clamp (bwTg - bwG, -bwK, bwK);
            float x[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                float v = x[c];
                if (d > 0.0005f)
                {
                    float y = v;
                    for (int k = 0; k < 4; ++k)
                    {
                        auto& buf = ap[(size_t) (c * 4 + k)]; int& p = apPos[c][k]; const int len = apLen[c][k];
                        const float dl = buf[(size_t) p];
                        const float in = y + g * dl; y = dl - g * in;
                        buf[(size_t) p] = in; if (++p >= len) p = 0;
                    }
                    v = (1 - d) * v + d * y;
                }
                { const float y = bw2.run (bw1.run (v, c), c); v += bwG * (y - v); }
                if (useBits) v = std::floor (v * q + 0.5f) / q;
                if (useLc) for (int k = 0; k < nLc; ++k) v = lc[(size_t) k].run (v, c);
                if (useHc) for (int k = 0; k < nHc; ++k) v = hc[(size_t) k].run (v, c);
                x[c] = v;
            }
            if (s.convWidth && std::abs (wm - 1.0f) > 1e-3f) { const float m = 0.5f * (x[0] + x[1]), sd = 0.5f * (x[0] - x[1]) * wm; x[0] = m + sd; x[1] = m - sd; }
            lvl += 0.002f * (tgt - lvl);
            L[i] = x[0] * lvl; R[i] = x[1] * lvl;
        }
    }
private:
    double sr = 48000; Settings s; bool dirty = true;
    std::array<std::vector<float>, 8> ap; int apPos[2][4] {}, apLen[2][4] {};
    Svf bw1, bw2; std::array<Biquad, 5> hc, lc; int nHc = 1, nLc = 1; float lvl = 1; double bwCur = -1; float bwG = 0;
};

// ---------------------------------------------------------------------------
//  COLOR on the tail (0.7.9): Bandwidth + Bits only. The band-limit filters always run (warm), and the stage
//  fades in / out over ~15 ms, so a preset or the switch turning it on never starts a cold filter on a running tail.
// ---------------------------------------------------------------------------
class ColorStage
{
public:
    void prepare (double sampleRate) { sr = sampleRate; fc = -1; reset(); }
    void reset() { b1.reset(); b2.reset(); g = 0; }
    void snap (bool on) { g = on ? 1.0f : 0.0f; }          // offline: no fade
    void process (float* L, float* R, int n, double bwKHz, double bits, bool on)
    {
        // the cutoff glides (a preset that jumps from 15 to 8 kHz on a running tail would click)
        const double f = std::min (bwKHz * 1000.0, 0.45 * sr);
        if (fc < 0) { fc = f; b1.lp (fc, sr, 0.54); b2.lp (fc, sr, 1.31); }
        const bool useBw = bwKHz < 19.9, useBits = bits < 23.5;
        const float tgt = on && (useBw || useBits) ? 1.0f : 0.0f, k = (float) (1.0 / (0.015 * sr));
        const float q = (float) std::pow (2.0, bits - 1 - kBitsHeadroom);
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0 && std::abs (f / fc - 1.0) > 1e-4) { fc *= std::pow (f / fc, 0.06); b1.lp (fc, sr, 0.54); b2.lp (fc, sr, 1.31); }
            g += std::clamp (tgt - g, -k, k);
            float* ch[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                float y = b2.run (b1.run (x, c), c);
                if (! useBw) y = x;
                if (useBits) y = std::floor (y * q + 0.5f) / q;
                ch[c][i] = x + g * (y - x);
            }
        }
    }
private:
    double sr = 48000, fc = -1; Svf b1, b2; float g = 0;
};

// ---------------------------------------------------------------------------
//  Tail preview: 8-line FDN fed by the ER (the way a full reverb will be built later)
// ---------------------------------------------------------------------------
class TailFDN
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.assign ((size_t) (sr * 0.16) + 8, 0.0f);
        reset(); setup (1.2, 1.0);
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); for (auto& p : pos) p = 0; for (auto& z : lp) z = 0; }
    void setup (double rt, double size)
    {
        static const double ms[8] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.1, 73.7 };
        const double sc = clampd (0.6 + 0.4 * size, 0.6, 1.4);
        for (int i = 0; i < 8; ++i)
        {
            len[i] = std::min ((int) buf[(size_t) i].size() - 1, (int) std::lround (ms[i] * sc * sr / 1000.0));
            fb[i] = (float) std::pow (10.0, -3.0 * (len[i] / sr) / std::max (0.1, rt));
        }
        a = (float) fcToA (5500.0, sr);
    }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, float gain)
    {
        for (int i = 0; i < n; ++i)
        {
            float y[8];
            for (int k = 0; k < 8; ++k) y[k] = buf[(size_t) k][(size_t) pos[k]];
            // Hadamard 8
            for (int h = 1; h < 8; h <<= 1) for (int k = 0; k < 8; k += h << 1) for (int j = k; j < k + h; ++j) { const float u = y[j], v = y[j + h]; y[j] = u + v; y[j + h] = u - v; }
            const float in = 0.5f * (inL[i] + inR[i]);
            float oL = 0, oR = 0;
            for (int k = 0; k < 8; ++k)
            {
                float v = y[k] * 0.35355339f * fb[k];
                lp[k] = v + a * (lp[k] - v);                     // damping in the loop
                const float w = lp[k] + in * ((k & 1) ? 0.5f : -0.5f);
                buf[(size_t) k][(size_t) pos[k]] = w; if (++pos[k] >= len[k]) pos[k] = 0;
                if (k & 1) oR += y[k]; else oL += y[k];
            }
            outL[i] += oL * 0.25f * gain; outR[i] += oR * 0.25f * gain;
        }
    }
private:
    double sr = 48000; std::array<std::vector<float>, 8> buf; int pos[8] {}, len[8] {}; float fb[8] {}, lp[8] {}, a = 0;
};

// ---------------------------------------------------------------------------
//  Multi-tap engine with cross-faded tap sets
// ---------------------------------------------------------------------------
class TapEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        int size = 1; while (size < (int) (sr * (kMaxTapMs + 60) / 1000.0) + 8) size <<= 1;
        line.assign ((size_t) size, 0.0f); mask = size - 1; w = 0;
        nCur = nOld = 0; fade = 0;
        fadeLen = std::max (1, (int) (sr * 0.02));
    }
    void reset() { std::fill (line.begin(), line.end(), 0.0f); for (auto& t : cur) t.zL = t.zR = 0; nOld = 0; fade = 0; }
    bool busy() const { return fade > 0; }
    // new taps (times include predelay and size). Returns false when the previous fade is still running.
    bool setTaps (const TapSet& ts, bool instant = false)
    {
        if (fade > 0 && ! instant) return false;
        old = cur; nOld = instant ? 0 : nCur;
        nCur = ts.n;
        for (int i = 0; i < ts.n; ++i)
        {
            auto& d = cur[(size_t) i]; const auto& s = ts.t[(size_t) i];
            d.dL = (float) (s.tL * sr / 1000.0); d.dR = (float) (s.tR * sr / 1000.0);
            d.gL = s.gL; d.gR = s.gR; d.a = (float) fcToA (s.fc, sr);
            d.iL = (int) std::max (0.0f, d.dL); d.fL = std::max (0.0f, d.dL) - (float) d.iL; d.iR = (int) std::max (0.0f, d.dR); d.fR = std::max (0.0f, d.dR) - (float) d.iR;
            d.zL = i < nOld ? old[(size_t) i].zL : 0; d.zR = i < nOld ? old[(size_t) i].zR : 0;
        }
        fade = nOld > 0 ? fadeLen : 0;
        return true;
    }
    void setWander (double amt) { wander = (float) clampd (amt, 0, 1); }
    // mono in, adds the reflections into outL / outR (cleared by the caller)
    void process (const float* in, float* outL, float* outR, int n, float* pdOut = nullptr, float pdSamples = 0)
    {
        const float depth = wander * (float) (0.0009 * sr);          // up to ±0.9 ms
        const float inc = (float) (2 * kPi * 0.31 / sr);
        for (int i = 0; i < n; ++i)
        {
            line[(size_t) w] = in[i];
            if (pdOut != nullptr) pdOut[i] = rd (pdSamples);
            float m[4] = { 0, 0, 0, 0 };
            if (depth > 0) { for (int k = 0; k < 4; ++k) { ph[k] += inc * (1.0f + 0.37f * (float) k); if (ph[k] > 6.2831853f) ph[k] -= 6.2831853f; m[k] = depth * 0.5f * std::sin (ph[k]); } }   // centred: Wander does not move the ER later
            float yL = 0, yR = 0;
            const float fo = fade > 0 ? (float) fade / (float) fadeLen : 0.0f, fi = 1.0f - fo;
            run (cur.data(), nCur, depth > 0 ? m : nullptr, fade > 0 ? fi : 1.0f, yL, yR);
            if (fade > 0) { run (old.data(), nOld, depth > 0 ? m : nullptr, fo, yL, yR); if (--fade == 0) nOld = 0; }
            outL[i] += yL; outR[i] += yR;
            w = (w + 1) & mask;
        }
    }
    float readDelay (float d) const          // used for the predelayed convolution input
    {
        float pos = (float) w - d; while (pos < 0) pos += (float) line.size();
        const int i0 = (int) pos; const float fr = pos - (float) i0;
        return line[(size_t) (i0 & mask)] * (1 - fr) + line[(size_t) ((i0 + 1) & mask)] * fr;
    }
    void writeOnly (const float* in, int n) { for (int i = 0; i < n; ++i) { line[(size_t) w] = in[i]; w = (w + 1) & mask; } }
    int writePos() const { return w; }
    const std::vector<float>& buffer() const { return line; }
    int bufMask() const { return mask; }

private:
    struct Live { float dL = 0, dR = 0, gL = 0, gR = 0, a = 0, zL = 0, zR = 0, fL = 0, fR = 0; int iL = 0, iR = 0; };
    inline float rd (float d) const
    {
        float pos = (float) w - d; if (pos < 0) pos += (float) line.size();
        const int i0 = (int) pos; const float fr = pos - (float) i0;
        return line[(size_t) (i0 & mask)] + fr * (line[(size_t) ((i0 + 1) & mask)] - line[(size_t) (i0 & mask)]);
    }
    inline void run (Live* t, int n, const float* m, float gain, float& yL, float& yR)
    {
        if (m == nullptr)
        {   // CPU: no Wander -> the read positions are fixed; integer / fraction worked out once per tap set
            const float* L = line.data(); float sL = 0, sR = 0;
            for (int i = 0; i < n; ++i)
            {
                auto& p = t[i];
                const int aL = (w - p.iL) & mask, aR = (w - p.iR) & mask;
                const float l0 = L[aL], r0 = L[aR];
                const float xl = l0 + p.fL * (L[(aL - 1) & mask] - l0), xr = r0 + p.fR * (L[(aR - 1) & mask] - r0);
                p.zL = xl + p.a * (p.zL - xl); p.zR = xr + p.a * (p.zR - xr);
                sL += p.zL * p.gL; sR += p.zR * p.gR;
            }
            yL += sL * gain; yR += sR * gain;
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            auto& p = t[i];
            const float o = m ? m[i & 3] : 0.0f;
            const float xl = rd (std::max (0.0f, p.dL + o)), xr = rd (std::max (0.0f, p.dR + o));
            p.zL = xl + p.a * (p.zL - xl); p.zR = xr + p.a * (p.zR - xr);
            yL += p.zL * p.gL * gain; yR += p.zR * p.gR * gain;
        }
    }
    double sr = 48000; std::vector<float> line; int mask = 0, w = 0;
    std::array<Live, kMaxTaps> cur, old; int nCur = 0, nOld = 0, fade = 0, fadeLen = 960;
    float wander = 0, ph[4] { 0, 1.7f, 3.1f, 4.6f };
};

// offline preview at 48 kHz: the shaped tap set (or the conv ER) through the same post chain
inline void renderPreview (const TapSet* taps, const std::vector<float>* convL, const std::vector<float>* convR, double predelayMs,
                           const Settings& s, double lengthMs, std::vector<float>& L, std::vector<float>& R)
{
    const size_t len = (size_t) (lengthMs * kK);
    L.assign (len, 0.0f); R.assign (len, 0.0f);
    if (taps != nullptr)
        for (int i = 0; i < taps->n; ++i)
        {
            const auto& tp = taps->t[(size_t) i]; const double a = fcToA (tp.fc, kSR);
            kernAbs (L, std::lround (tp.tL * kK), tp.gL, a); kernAbs (R, std::lround (tp.tR * kK), tp.gR, a);
        }
    if (convL != nullptr && convR != nullptr)
    {
        const size_t off = (size_t) std::lround (predelayMs * kK);
        for (size_t i = 0; i < convL->size() && i + off < len; ++i) { L[i + off] += (*convL)[i]; R[i + off] += (*convR)[i]; }
    }
    Post p; p.prepare (kSR); Settings ss = s; p.set (ss); p.reset();
    p.process (L.data(), R.data(), (int) len);
}
} // namespace er
