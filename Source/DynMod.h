#pragma once
// Reverb Anatomy Lab 0.6.0 (stage 4): ducking (single / multiband / spectral), gate, output chorus,
// LFO x 2, envelope follower and the 4-slot modulation matrix.
// Copyright (c) 2026 Johan Plus Kei. All rights reserved.
#include "TailDsp.h"

namespace dm
{
using rv::BQ; using er::kPi;

inline float db2g (float d) { return std::exp (d * 0.11512925f); }
inline float g2db (float g) { return 8.6858896f * std::log (std::max (1e-9f, g)); }

// peak envelope (attack / release, one pole each), on |x|
struct Env
{
    float a = 0, r = 0, v = 0;
    void set (double atkMs, double relMs, double sr) { a = (float) std::exp (-1.0 / (std::max (0.05, atkMs) * 0.001 * sr)); r = (float) std::exp (-1.0 / (std::max (0.5, relMs) * 0.001 * sr)); }
    inline float run (float x) { x = std::abs (x); v = x > v ? x + a * (v - x) : x + r * (v - x); return v; }
};

// how much a band is turned down: 0 below the threshold, the full depth 12 dB above it (smooth in between)
inline float duckDb (float envDb, float thr, float depth)
{
    const float u = std::clamp ((envDb - thr) / 12.0f, 0.0f, 1.0f);
    return -depth * u * u * (3 - 2 * u);
}

// ---------------------------------------------------------------------------
//  Linkwitz-Riley 4 crossover (LP / HP of two Butterworth sections each)
// ---------------------------------------------------------------------------
struct LR4
{
    BQ lp[2], hp[2];
    void set (double fc, double sr) { for (auto& b : lp) b.pass (false, fc, 0.70710678, sr); for (auto& b : hp) b.pass (true, fc, 0.70710678, sr); }
    void reset() { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }
    inline void split (float x, float& lo, float& hi) { lo = lp[1].run (lp[0].run (x)); hi = hp[1].run (hp[0].run (x)); }
};

// ---------------------------------------------------------------------------
//  Ducker
// ---------------------------------------------------------------------------
struct DuckSettings { int mode = 0; float depth = 6, thr = -30, atkMs = 10, relMs = 250; int bands = 3; };

class Ducker
{
public:
    static constexpr int kSpec = 17;                       // spectral: half-octave bands, 63 Hz .. 16 kHz
    void prepare (double s) { sr = s; configured = -1; reset(); }
    void reset()
    {
        for (auto& e : env) e.v = 0;
        for (auto& c : xo) for (auto& x : c) x.reset();
        for (auto& b : keyBp) b.reset();
        for (auto& c : peq) for (auto& b : c) b.reset();
        for (auto& g : gCur) g = 0; for (auto& g : gLin) g = 1; for (auto& g : gr) g = 0; for (auto& g : gSpec) g = 0;
    }
    // GR per band (dB, <= 0) for the meter
    float gr[kSpec] {}; int nShown = 0;
    void process (const float* key, float* L, float* R, int n, const DuckSettings& d)
    {
        if (d.mode <= 0) { nShown = 0; return; }
        configure (d);
        if (d.mode == 1) single (key, L, R, n, d);
        else if (d.mode == 2) multi (key, L, R, n, d);
        else spectral (key, L, R, n, d);
    }
private:
    void configure (const DuckSettings& d)
    {
        const int key = d.mode * 10 + d.bands;
        for (auto& e : env) e.set (0.3, 20, sr);                  // the detector is quick; Attack / Release shape the gain
        ga = (float) std::exp (-8.0 / (std::max (0.5f, d.atkMs) * 0.001 * sr)); gr8 = (float) std::exp (-8.0 / (std::max (5.0f, d.relMs) * 0.001 * sr));
        if (key == configured) return;
        configured = key; reset();
        if (d.mode == 2)
        {
            static const float xf[4][4] = { { 800 }, { 250, 2500 }, { 200, 800, 3200 }, { 150, 500, 1600, 5000 } };
            nb = std::clamp (d.bands, 2, 5);
            for (int c = 0; c < 3; ++c) for (int i = 0; i < nb - 1; ++i) xo[c][i].set (xf[nb - 2][i], sr);
        }
        if (d.mode == 3)
            for (int k = 0; k < kSpec; ++k)
            {
                const double f = 63.0 * std::pow (2.0, k * 0.5);
                keyBp[k].set (0, 0, 0, 1, 0, 0);
                { const double w = 2 * kPi * std::min (f, sr * 0.45) / sr, al = std::sin (w) / (2 * 2.87);
                  keyBp[k].set (al, 0, -al, 1 + al, -2 * std::cos (w), 1 - al); }                       // band-pass, half an octave
                const double w = 2 * kPi * std::min (f, sr * 0.45) / sr; pc[k] = std::cos (w); pal[k] = std::sin (w) / (2 * 1.41);
                for (int c = 0; c < 2; ++c) setPeak (c, k, 0.0f);
            }
    }
    void setPeak (int c, int k, float gdB)
    {
        auto& f = peq[c][k]; const float z1 = f.z1, z2 = f.z2;
        const double A = std::pow (10.0, gdB / 40.0), a0 = 1 + pal[k] / A;
        f.b0 = (float) ((1 + pal[k] * A) / a0); f.b1 = f.a1 = (float) (-2 * pc[k] / a0); f.b2 = (float) ((1 - pal[k] * A) / a0); f.a2 = (float) ((1 - pal[k] / A) / a0);
        f.z1 = z1; f.z2 = z2;
    }
    void single (const float* key, float* L, float* R, int n, const DuckSettings& d)
    {
        float g = db2g (gCur[0]);
        for (int i = 0; i < n; ++i)
        {
            const float e = env[0].run (key[i]);
            if ((i & 7) == 0) { gCur[0] = ballistic (gCur[0], duckDb (g2db (e), d.thr, d.depth)); g = db2g (gCur[0]); }
            gLin[0] += 0.05f * (g - gLin[0]);                   // (smooths the 8-sample steps)
            L[i] *= gLin[0]; R[i] *= gLin[0];
        }
        gr[0] = gCur[0]; nShown = 1;
    }
    void multi (const float* key, float* L, float* R, int n, const DuckSettings& d)
    {
        float tg[5] = { 1, 1, 1, 1, 1 };
        for (int i = 0; i < n; ++i)
        {
            float kb[5], lb[5], rb[5];
            float kx = key[i], lx = L[i], rx = R[i];
            for (int b = 0; b < nb - 1; ++b)
            {   // a tree: band b = the low part of what is left
                float lo, hi;
                xo[0][b].split (kx, lo, hi); kb[b] = lo; kx = hi;
                xo[1][b].split (lx, lo, hi); lb[b] = lo; lx = hi;
                xo[2][b].split (rx, lo, hi); rb[b] = lo; rx = hi;
            }
            kb[nb - 1] = kx; lb[nb - 1] = lx; rb[nb - 1] = rx;
            float ol = 0, orr = 0;
            for (int b = 0; b < nb; ++b)
            {
                const float e = env[b].run (kb[b]);
                if ((i & 7) == 0) { gCur[b] = ballistic (gCur[b], duckDb (g2db (e) + 3.0f, d.thr, d.depth)); tg[b] = db2g (gCur[b]); }  // (+3 dB: a band holds part of the level)
                else tg[b] = db2g (gCur[b]);
                gLin[b] += 0.05f * (tg[b] - gLin[b]);
                ol += lb[b] * gLin[b]; orr += rb[b] * gLin[b];
            }
            L[i] = ol; R[i] = orr;
        }
        for (int b = 0; b < nb; ++b) gr[b] = gCur[b];
        nShown = nb;
    }
    void spectral (const float* key, float* L, float* R, int n, const DuckSettings& d)
    {
        for (int i0 = 0; i0 < n; i0 += 32)
        {
            const int m = std::min (32, n - i0);
            // the key's level per band over this step
            for (int k = 0; k < kSpec; ++k)
            {
                auto& bp = keyBp[k]; auto& e = env[k];
                for (int i = 0; i < m; ++i) e.run (bp.run (key[i0 + i]));
                const float t = ballistic (gSpec[k], duckDb (g2db (e.v) + 6.0f, d.thr, d.depth), 4.0f);   // (+6 dB: half an octave holds little of the level)
                gSpec[k] = t;
                if (std::abs (t - gCur[k]) > 0.05f) { gCur[k] = t; setPeak (0, k, t); setPeak (1, k, t); }
            }
            for (int i = i0; i < i0 + m; ++i)
            {
                float l = L[i], r = R[i];
                for (int k = 0; k < kSpec; ++k) { l = peq[0][k].run (l); r = peq[1][k].run (r); }
                L[i] = l; R[i] = r;
            }
        }
        for (int k = 0; k < kSpec; ++k) gr[k] = gCur[k];
        nShown = kSpec;
    }
    // the gain (dB) moves towards its target: down at the attack rate, back up at the release rate (per 8 samples)
    inline float ballistic (float cur, float tgt, float k = 1.0f) const { const float c = tgt < cur ? std::pow (ga, k) : std::pow (gr8, k); return tgt + c * (cur - tgt); }
    float ga = 0.9f, gr8 = 0.99f;
    double sr = 48000; int configured = -1, nb = 3;
    Env env[kSpec]; float gCur[kSpec] {}, gLin[kSpec] {}, gSpec[kSpec] {};
    LR4 xo[3][4];
    BQ keyBp[kSpec], peq[2][kSpec]; double pc[kSpec] {}, pal[kSpec] {};
};

// ---------------------------------------------------------------------------
//  Gate
// ---------------------------------------------------------------------------
class Gate
{
public:
    void prepare (double s) { sr = s; env.set (0.5, 40, sr); reset(); }
    void reset() { env.v = 0; g = 1; hold = 0; open = true; }
    bool open = true; float g = 1;
    void process (const float* key, float* L, float* R, int n, float thrDb, float holdMs, float relMs)
    {
        const float thr = db2g (thrDb), relStep = (float) (1.0 / (std::max (5.0f, relMs) * 0.001 * sr)), atkStep = (float) (1.0 / (0.001 * sr));
        const int holdN = (int) (holdMs * 0.001 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float e = env.run (key[i]);
            if (e > thr) { open = true; hold = holdN; }
            else if (hold > 0) --hold;
            else open = false;
            g = open ? std::min (1.0f, g + atkStep) : std::max (0.0f, g - relStep);
            const float c = g * g * (3 - 2 * g);                  // an S-curve: no corner at either end
            L[i] *= c; R[i] *= c;
        }
    }
private:
    double sr = 48000; Env env; int hold = 0;
};

// ---------------------------------------------------------------------------
//  Output chorus: one modulated delay per side (quadrature), mixed with the input
// ---------------------------------------------------------------------------
class Chorus
{
public:
    void prepare (double s) { sr = s; size = 1; while (size < (int) (0.04 * sr) + 8) size <<= 1; buf[0].assign ((size_t) size, 0.0f); buf[1].assign ((size_t) size, 0.0f); w = 0; ph = 0; depthCur = -1; }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
    void process (float* L, float* R, int n, float mix, float rateHz, float depthMs)
    {
        if (mix <= 0 && mixCur <= 0) return;
        const int mask = size - 1; const float inc = (float) (2 * kPi * rateHz / sr);
        const float dTgt = (float) (depthMs * 0.001 * sr); if (depthCur < 0) depthCur = dTgt;
        const float base = (float) (0.007 * sr);
        for (int i = 0; i < n; ++i)
        {
            mixCur += 0.002f * (mix - mixCur); depthCur += 0.001f * (dTgt - depthCur);
            ph += inc; if (ph > (float) kPi) ph -= (float) (2 * kPi);
            float q = ph + (float) (kPi / 2); if (q > (float) kPi) q -= (float) (2 * kPi);
            const float s0 = rv::fastSin (ph), c0 = rv::fastSin (q);
            float x[2] = { L[i], R[i] };
            buf[0][(size_t) w] = x[0]; buf[1][(size_t) w] = x[1];
            for (int c = 0; c < 2; ++c)
            {
                const float d = base + depthCur * 0.5f * (1 + (c == 0 ? s0 : c0));
                const int di = (int) d; const float fr = d - (float) di;
                const float a = buf[(size_t) c][(size_t) ((w - di) & mask)], b = buf[(size_t) c][(size_t) ((w - di - 1) & mask)];
                const float y = a + fr * (b - a);
                x[c] += mixCur * 0.5f * (y - x[c]);                   // 100 % = the classic half dry, half moving copy
            }
            L[i] = x[0]; R[i] = x[1];
            w = (w + 1) & mask;
        }
    }
private:
    double sr = 48000; std::vector<float> buf[2]; int size = 1, w = 0; float ph = 0, depthCur = -1, mixCur = 0;
};

// ---------------------------------------------------------------------------
//  LFO (bipolar -1..1): sine, triangle, saw, square, sample & hold, smooth random
// ---------------------------------------------------------------------------
class Lfo
{
public:
    float value = 0;
    void reset() { ph = 0; held = 0; from = 0; to = 0; }
    // advance by dt seconds at rateHz; returns the value
    float step (double dt, double rateHz, int shape)
    {
        const double p0 = ph; ph += dt * rateHz;
        const bool wrapped = ph >= 1.0; ph -= std::floor (ph);
        if (wrapped || p0 == 0) { from = to; to = rnd.nextFloat() * 2 - 1; held = to; }
        const float x = (float) ph;
        switch (shape)
        {
            case 0: value = rv::fastSin ((float) (2 * kPi) * x - (float) kPi) * -1.0f; break;
            case 1: value = x < 0.5f ? 4 * x - 1 : 3 - 4 * x; break;
            case 2: value = 2 * x - 1; break;
            case 3: value = x < 0.5f ? 1.0f : -1.0f; break;
            case 4: value = held; break;
            default: { const float u = x * x * (3 - 2 * x); value = from + (to - from) * u; break; }
        }
        return value;
    }
    void setPhase (double p) { ph = p - std::floor (p); }
    double phase() const { return ph; }
private:
    double ph = 0; float held = 0, from = 0, to = 0; juce::Random rnd { 1234 };
};

// beats per cycle for the sync choices ("Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/4 D", "1/8 D")
inline double syncBeats (int s)
{
    static const double b[] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.5, 0.75 };
    return s > 0 && s < 12 ? b[s] : 0;
}

// the destinations of the matrix
enum Dst { dMix, dWidth, dOut, dTail, dER, dDecay, dTreble, dChorus, dDuck, dGate, kDst };
} // namespace dm
