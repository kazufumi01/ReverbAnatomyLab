#pragma once
// =============================================================================
//  ER Anatomy Lab — core (no JUCE GUI dependency)
//  Copyright (c) 2026 Johan Plus Kei. All rights reserved.
//
//  Three ways to make an early-reflection tap set:
//    ROOM     image-source method in a shoebox room (physical)
//    ALGO  9-parameter algorithmic generator (also used by the vintage models)
//    IR       reflections picked from a loaded impulse response
//  Plus the IR analysis / analysis-by-synthesis fit ported from the ER Match Lab web app.
//  All analysis runs at a fixed 48 kHz.
// =============================================================================
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <complex>

namespace er
{
constexpr double kSR = 48000.0, kK = kSR / 1000.0, kC = 343.0;
constexpr int kMaxTaps = 160;
constexpr float kMaxTapMs = 900.0f;
constexpr double kPi = 3.14159265358979323846;

inline double db2g (double d) { return std::pow (10.0, d / 20.0); }
inline double clampd (double x, double a, double b) { return std::min (b, std::max (a, x)); }

// same generator as the web app (mulberry32), so seeds give the same patterns
struct Mulberry
{
    uint32_t a;
    explicit Mulberry (uint32_t s) : a (s) {}
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

// One reflection. Times in ms from the direct sound (before predelay / size scaling)
struct Tap { float tL = 0, tR = 0, gL = 0, gR = 0, fc = 20000; };
struct TapSet { std::array<Tap, kMaxTaps> t {}; int n = 0; };

// --------------------------------------------------------------------------
//  ALGO
// --------------------------------------------------------------------------
struct PatternP { double first = 6, length = 50, taps = 24, growth = 1, energy = -8, decay = -14, damp = 7000; int seed = 1; };

inline void panTap (Tap& tp, double t, double g, double pan, double fc)
{
    const double gl = std::cos ((pan + 1) * kPi / 4) * std::sqrt (2.0), gr = std::sin ((pan + 1) * kPi / 4) * std::sqrt (2.0);
    const double itd = 0.45 * pan;
    tp.tL = (float) std::max (t + itd / 2, 0.9); tp.tR = (float) std::max (t - itd / 2, 0.9);
    tp.gL = (float) (g * gl); tp.gR = (float) (g * gr); tp.fc = (float) fc;
}

inline void patternTaps (const PatternP& p, TapSet& out)
{
    Mulberry rnd ((uint32_t) (p.seed + 7919));
    const int N = std::max (2, std::min (kMaxTaps, (int) std::lround (p.taps)));
    const double t1 = p.first, tE = std::max (p.length, p.first + 3);
    double E = 0; std::array<double, kMaxTaps> g {};
    out.n = N;
    for (int k = 0; k < N; ++k)
    {
        double q = (double) k / (N - 1);
        if (k > 0 && k < N - 1) q = clampd (q + (rnd.next() - 0.5) * 0.7 / (N - 1), 0, 1);
        const double x = std::pow (q, 1.0 / (1.0 + p.growth)), t = t1 + (tE - t1) * x;
        const double dbv = p.decay * x + (k ? (rnd.next() - 0.5) * 4 : 0);
        const double mag = 0.35 + 0.65 * rnd.next();
        const double sgn = ((k & 1) ? 1.0 : -1.0) * (rnd.next() < 0.2 ? -1.0 : 1.0);
        g[(size_t) k] = db2g (dbv); E += g[(size_t) k] * g[(size_t) k];
        panTap (out.t[(size_t) k], t, 1.0, clampd (sgn * mag, -1, 1), 16000.0 * std::pow (p.damp / 16000.0, x));
    }
    const double sc = std::sqrt (std::pow (10.0, p.energy / 10.0) / std::max (E, 1e-12));
    for (int k = 0; k < N; ++k) { auto& tp = out.t[(size_t) k]; tp.gL *= (float) (g[(size_t) k] * sc); tp.gR *= (float) (g[(size_t) k] * sc); }
}

// --------------------------------------------------------------------------
//  ROOM: image-source method in a shoebox. Gains are relative to the direct sound.
// --------------------------------------------------------------------------
struct Material { const char* ja; const char* en; double alpha, hf; };
inline const std::array<Material, 7>& materials()
{
    static const std::array<Material, 7> m { {
        { "タイル・ガラス", "Tile / Glass",       0.03, 0.010 },
        { "コンクリート",   "Concrete",           0.05, 0.025 },
        { "しっくい",       "Plaster",            0.10, 0.045 },
        { "木",             "Wood",               0.15, 0.065 },
        { "吸音パネル",     "Studio panels",      0.35, 0.120 },
        { "カーテン・絨毯", "Curtain / Carpet",   0.55, 0.160 },
        { "客席",           "Audience / Seats",   0.70, 0.050 } } };
    return m;
}
// shape (0.10.0): 0 shoebox, 1 hall (a fan: narrow at the stage end, wide at the back), 2 skewed (the left wall leans); splay 0..0.8 = how much
struct RoomP { double W = 6, D = 8, H = 3.2, sx = 0.5, sy = 0.3, lx = 0.5, ly = 0.7; int wall = 2, floor = 3, order = 3; int shape = 0; double splay = 0, backAbs = 0, clutter = 0; };   // clutter 0..1: furniture / objects / rough surfaces (scatter + soft absorption)   // backAbs (connected rooms): the back room's walls more absorbent (+1: like audience seating) or more reflective (-1: like tile) than the front's

// the floor plan as a polygon (x across, y = depth; stage end at y = 0). The first / last pair of vertices are the stage-end / back walls.
struct Poly { int n = 4; double x[16] {}, y[16] {}; };
inline Poly roomPoly (const RoomP& R)
{
    Poly p;
    if (R.shape == 3)
    {   // the dome's plan: a 16-gon inscribed in the ellipse and scaled so that it spans exactly W x D (flat edges at the stage / back end)
        constexpr int N = 16; const double k = 1.0 / std::cos (kPi / N);
        p.n = N; for (int i = 0; i < N; ++i) { const double th = (i + 0.5) * 2 * kPi / N; p.x[i] = R.W * 0.5 * (1 + k * std::cos (th)); p.y[i] = R.D * 0.5 * (1 + k * std::sin (th)); }
        return p;
    }
    if (R.shape == 4)
    {   // two rooms joined by a door: the front room (full width, half the depth), a partition t thick with a door, the back room (70 % of the width, to the right)
        const double W = R.W, D = R.D, t = std::min (0.4, 0.03 * D), y1 = 0.5 * D, x2 = 0.3 * W, hw = 0.5 * (0.08 + 0.42 * std::clamp (R.splay, 0.0, 1.0)) * W, xc = 0.62 * W;
        const double xd0 = xc - hw, xd1 = xc + hw;
        const double vx[12] = { 0, W, W, xd1, xd1, W, W, x2, x2, xd0, xd0, 0 }, vy[12] = { 0, 0, y1, y1, y1 + t, y1 + t, D, D, y1 + t, y1 + t, y1, y1 };
        p.n = 12; for (int i = 0; i < 12; ++i) { p.x[i] = vx[i]; p.y[i] = vy[i]; }
        return p;
    }
    if (R.shape == 5)
    {   // an L-shaped corridor: one arm along the depth, then a turn to the right along the back
        const double W = R.W, D = R.D, w = std::min (std::min (W, D) * 0.9, (0.04 + 0.26 * std::clamp (R.splay, 0.0, 1.0)) * std::min (W, D) * 2.0);
        const double vx[6] = { 0, w, w, W, W, 0 }, vy[6] = { 0, 0, D - w, D - w, D, D };
        p.n = 6; for (int i = 0; i < 6; ++i) { p.x[i] = vx[i]; p.y[i] = vy[i]; }
        return p;
    }
    const double sp = std::min (R.splay, 0.85);
    const double a = R.shape == 1 ? sp * R.W * 0.5 : R.shape == 2 ? sp * R.W : 0.0;
    const double xl0 = a, xr0 = R.shape == 2 ? R.W : R.W - a;
    p.x[0] = xl0; p.y[0] = 0; p.x[1] = xr0; p.y[1] = 0; p.x[2] = R.W; p.y[2] = R.D; p.x[3] = 0; p.y[3] = R.D;
    if (R.shape == 0) { p.x[0] = 0; p.x[1] = R.W; }
    return p;
}
// a position given as fractions of the width / depth -> metres inside the plan
inline void roomChord (const RoomP& R, double fy, double& xl, double& xr)   // the plan's extent across, at this depth fraction
{
    const auto p = roomPoly (R); const double y = fy * R.D; xl = 1e9; xr = -1e9;
    for (int i = 0; i < p.n; ++i)
    {
        const int j = (i + 1) % p.n; const double y0 = p.y[i], y1 = p.y[j];
        if ((y0 - y) * (y1 - y) > 0 || std::abs (y1 - y0) < 1e-12) continue;
        const double xx = p.x[i] + (y - y0) / (y1 - y0) * (p.x[j] - p.x[i]); xl = std::min (xl, xx); xr = std::max (xr, xx);
    }
    if (xl > xr) { xl = 0; xr = R.W; }
}
inline void roomPos (const RoomP& R, double fx, double fy, double& x, double& y)
{
    double xl, xr; roomChord (R, fy, xl, xr); y = fy * R.D; x = xl + fx * (xr - xl);
}
inline double polyArea (const Poly& p) { double a = 0; for (int i = 0; i < p.n; ++i) { const int j = (i + 1) % p.n; a += p.x[i] * p.y[j] - p.x[j] * p.y[i]; } return std::abs (a) * 0.5; }
inline double polyPerim (const Poly& p) { double a = 0; for (int i = 0; i < p.n; ++i) { const int j = (i + 1) % p.n; a += std::hypot (p.x[j] - p.x[i], p.y[j] - p.y[i]); } return a; }
struct RoomGeom { double area, perim, V, Sw, Sf, S; };       // walls + ceiling (Sw), floor (Sf)
// the dome: a half-ellipsoid on the plan, its rise = splay x the smaller half-axis (100 % = a hemisphere on a circle)
inline double domeRise (const RoomP& R) { return std::clamp (R.splay, 0.0, 1.0) * std::min (R.W, R.D) * 0.5; }
inline RoomGeom roomGeom (const RoomP& R)
{
    const auto p = roomPoly (R); RoomGeom g; g.area = polyArea (p); g.perim = polyPerim (p); g.V = g.area * R.H; g.Sf = g.area; g.Sw = g.perim * R.H + g.area;
    if (R.shape == 3)
    {
        const double a = R.W * 0.5, b = R.D * 0.5, c = domeRise (R), f = g.area / (kPi * a * b), q = 1.6075;
        g.V += 2.0 / 3.0 * kPi * a * b * c * f;
        const double S = 4 * kPi * std::pow ((std::pow (a * b, q) + std::pow (a * c, q) + std::pow (b * c, q)) / 3.0, 1.0 / q) * 0.5;   // half an ellipsoid (Knud Thomsen)
        g.Sw = g.perim * R.H + S * f;
    }
    g.S = g.Sw + g.Sf; return g;
}

// 2-D image sources in a polygon (vertical walls): every wall sequence up to maxN bounces whose mirrored path really hits those walls.
struct Img2 { double ix, iy, hx, hy; int n; };
inline bool segHit (double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy, double& t, double& u)   // a-b with c-d
{
    const double r1 = bx - ax, r2 = by - ay, s1 = dx - cx, s2 = dy - cy, den = r1 * s2 - r2 * s1;
    if (std::abs (den) < 1e-12) return false;
    t = ((cx - ax) * s2 - (cy - ay) * s1) / den; u = ((cx - ax) * r2 - (cy - ay) * r1) / den; return true;
}
inline bool polyInside (const Poly& P, double x, double y)                            // even-odd test
{
    bool in = false;
    for (int i = 0, j = P.n - 1; i < P.n; j = i++)
        if (((P.y[i] > y) != (P.y[j] > y)) && (x < (P.x[j] - P.x[i]) * (y - P.y[i]) / (P.y[j] - P.y[i]) + P.x[i])) in = ! in;
    return in;
}
inline bool legClear (const Poly& P, double ax, double ay, double bx, double by)      // no wall crossed between a and b, and the leg runs inside the plan
{
    if (! polyInside (P, 0.5 * (ax + bx), 0.5 * (ay + by))) return false;             // (a leg through a partition's thickness touches walls only at its ends)
    for (int e = 0; e < P.n; ++e)
    {
        const int f = (e + 1) % P.n; double t, u;
        if (segHit (ax, ay, bx, by, P.x[e], P.y[e], P.x[f], P.y[f], t, u) && t > 1e-6 && t < 1 - 1e-6 && u > 1e-6 && u < 1 - 1e-6) return false;
    }
    return true;
}
inline void polyImagesRec (const Poly& P, double lx, double ly, int depth, int maxN, int* seq, double* ix, double* iy, std::vector<Img2>& out)
{
    if (depth > 0)
    {   // trace back from the listener: each leg must hit the wall it reflects off, inside the wall
        double cx = lx, cy = ly, hx0 = 0, hy0 = 0; bool ok = true;
        for (int m = depth; m >= 1 && ok; --m)
        {
            const int e = seq[m - 1], f = (e + 1) % P.n; double t, u;
            if (! segHit (cx, cy, ix[m], iy[m], P.x[e], P.y[e], P.x[f], P.y[f], t, u) || t <= 1e-6 || t >= 1 + 1e-9 || u < -1e-9 || u > 1 + 1e-9) { ok = false; break; }
            const double hx = cx + (ix[m] - cx) * t, hy = cy + (iy[m] - cy) * t;
            if (! legClear (P, cx, cy, hx, hy)) { ok = false; break; }
            if (m == depth) { hx0 = hx; hy0 = hy; }
            cx = hx; cy = hy;
        }
        if (ok && legClear (P, cx, cy, ix[0], iy[0])) out.push_back ({ ix[depth], iy[depth], hx0, hy0, depth });
    }
    if (depth >= maxN) return;
    for (int e = 0; e < P.n; ++e)
    {
        if (depth > 0 && seq[depth - 1] == e) continue;
        const int f = (e + 1) % P.n; const double ex = P.x[f] - P.x[e], ey = P.y[f] - P.y[e], L2 = ex * ex + ey * ey;
        const double d = ((ix[depth] - P.x[e]) * ex + (iy[depth] - P.y[e]) * ey) / L2;
        const double px = P.x[e] + d * ex, py = P.y[e] + d * ey;
        seq[depth] = e; ix[depth + 1] = 2 * px - ix[depth]; iy[depth + 1] = 2 * py - iy[depth];
        polyImagesRec (P, lx, ly, depth + 1, maxN, seq, ix, iy, out);
    }
}
inline void polyImages (const Poly& P, double sx, double sy, double lx, double ly, int maxN, std::vector<Img2>& out)
{
    int seq[8] {}; double ix[8] {}, iy[8] {}; ix[0] = sx; iy[0] = sy; out.clear();
    polyImagesRec (P, lx, ly, 0, std::min (maxN, 6), seq, ix, iy, out);
}

// a = one-pole coefficient (web app's kernAbs) -> cutoff in Hz
inline double aToFc (double a) { return a <= 1e-4 ? 20000.0 : std::min (20000.0, -std::log (a) * kSR / (2 * kPi)); }

// --------------------------------------------------------------------------
//  3-D image sources in a convex polyhedron (the dome: a 16-sided drum + a ring-faceted half-ellipsoid)
// --------------------------------------------------------------------------
struct Face3 { double nx = 0, ny = 0, nz = 1, d = 0; int nv = 0; double vx[16] {}, vy[16] {}, vz[16] {}; bool floor = false; };
inline void faceFinish (Face3& f, double ix, double iy, double iz)      // normal pointing into the room
{
    const double ax = f.vx[1] - f.vx[0], ay = f.vy[1] - f.vy[0], az = f.vz[1] - f.vz[0], bx = f.vx[2] - f.vx[0], by = f.vy[2] - f.vy[0], bz = f.vz[2] - f.vz[0];
    double nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx; const double L = std::sqrt (nx * nx + ny * ny + nz * nz) + 1e-300;
    nx /= L; ny /= L; nz /= L;
    if (nx * (ix - f.vx[0]) + ny * (iy - f.vy[0]) + nz * (iz - f.vz[0]) < 0) { nx = -nx; ny = -ny; nz = -nz; }
    f.nx = nx; f.ny = ny; f.nz = nz; f.d = -(nx * f.vx[0] + ny * f.vy[0] + nz * f.vz[0]);
}
inline bool inFace (const Face3& f, double px, double py, double pz)
{
    double sgn = 0;
    for (int i = 0; i < f.nv; ++i)
    {
        const int j = (i + 1) % f.nv;
        const double ex = f.vx[j] - f.vx[i], ey = f.vy[j] - f.vy[i], ez = f.vz[j] - f.vz[i], qx = px - f.vx[i], qy = py - f.vy[i], qz = pz - f.vz[i];
        const double c = (ey * qz - ez * qy) * f.nx + (ez * qx - ex * qz) * f.ny + (ex * qy - ey * qx) * f.nz;
        if (sgn == 0 && std::abs (c) > 1e-9) sgn = c > 0 ? 1 : -1;
        if (sgn * c < -1e-7) return false;
    }
    return true;
}
constexpr int kDomeFaces = 80;
inline int domeFaces (const RoomP& R, Face3* F)
{
    constexpr int N = 16; const double a = R.W * 0.5, b = R.D * 0.5, k = 1.0 / std::cos (kPi / N), c = domeRise (R), H = R.H;
    static const double ph[4] = { 0.0, kPi / 6, kPi / 3, kPi / 2 };
    auto vtx = [&] (int i, int r, double& x, double& y, double& z)
    { const double th = (i % N + 0.5) * 2 * kPi / N, cp = std::cos (ph[r]); x = a + a * k * std::cos (th) * cp; y = b + b * k * std::sin (th) * cp; z = H + c * std::sin (ph[r]); };
    const double ix = a, iy = b, iz = H * 0.5; int n = 0;
    { Face3& f = F[n++]; f = {}; f.nv = N; f.floor = true; for (int i = 0; i < N; ++i) { vtx (i, 0, f.vx[i], f.vy[i], f.vz[i]); f.vz[i] = 0; } faceFinish (f, ix, iy, iz); }
    for (int i = 0; i < N; ++i)
    {   // the drum
        Face3& f = F[n++]; f = {}; f.nv = 4; double x0, y0, z0, x1, y1, z1; vtx (i, 0, x0, y0, z0); vtx (i + 1, 0, x1, y1, z1);
        f.vx[0] = x0; f.vy[0] = y0; f.vz[0] = 0; f.vx[1] = x1; f.vy[1] = y1; f.vz[1] = 0; f.vx[2] = x1; f.vy[2] = y1; f.vz[2] = H; f.vx[3] = x0; f.vy[3] = y0; f.vz[3] = H;
        faceFinish (f, ix, iy, iz);
    }
    for (int r = 0; r < 3; ++r)
        for (int i = 0; i < N; ++i)
        {   // the dome's bands (trapezoids) and its top (triangles)
            Face3& f = F[n++]; f = {};
            if (r < 2) { f.nv = 4; vtx (i, r, f.vx[0], f.vy[0], f.vz[0]); vtx (i + 1, r, f.vx[1], f.vy[1], f.vz[1]); vtx (i + 1, r + 1, f.vx[2], f.vy[2], f.vz[2]); vtx (i, r + 1, f.vx[3], f.vy[3], f.vz[3]); }
            else { f.nv = 3; vtx (i, 2, f.vx[0], f.vy[0], f.vz[0]); vtx (i + 1, 2, f.vx[1], f.vy[1], f.vz[1]); f.vx[2] = a; f.vy[2] = b; f.vz[2] = H + c; }
            faceFinish (f, ix, iy, iz);
        }
    return n;
}
// a box as faces (to check the 3-D method against the shoebox)
inline int boxFaces (double W, double D, double H, Face3* F)
{
    int n = 0; const double ix = W * 0.5, iy = D * 0.5, iz = H * 0.5;
    auto quad = [&] (double x0, double y0, double z0, double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3, bool fl)
    { Face3& f = F[n++]; f = {}; f.nv = 4; f.floor = fl; f.vx[0] = x0; f.vy[0] = y0; f.vz[0] = z0; f.vx[1] = x1; f.vy[1] = y1; f.vz[1] = z1; f.vx[2] = x2; f.vy[2] = y2; f.vz[2] = z2; f.vx[3] = x3; f.vy[3] = y3; f.vz[3] = z3; faceFinish (f, ix, iy, iz); };
    quad (0, 0, 0, W, 0, 0, W, D, 0, 0, D, 0, true); quad (0, 0, H, W, 0, H, W, D, H, 0, D, H, false);
    quad (0, 0, 0, W, 0, 0, W, 0, H, 0, 0, H, false); quad (0, D, 0, W, D, 0, W, D, H, 0, D, H, false);
    quad (0, 0, 0, 0, D, 0, 0, D, H, 0, 0, H, false); quad (W, 0, 0, W, D, 0, W, D, H, W, 0, H, false);
    return n;
}
struct Img3 { double ix, iy, d; int nw, nf; };
inline void domeRec (const Face3* F, int nF, const double* L, int depth, int maxN, int* seq, double (*img)[3], std::vector<Img3>& out)
{
    if (depth > 0)
    {   // from the listener back to the source: each leg must hit the face it bounces off, inside that face
        double cur[3] = { L[0], L[1], L[2] }; bool ok = true;
        for (int m = depth; m >= 1 && ok; --m)
        {
            const Face3& f = F[seq[m - 1]];
            const double dc = f.nx * cur[0] + f.ny * cur[1] + f.nz * cur[2] + f.d, di = f.nx * img[m][0] + f.ny * img[m][1] + f.nz * img[m][2] + f.d;
            if (dc < -1e-9 || di > 1e-9 || dc - di < 1e-12) { ok = false; break; }
            const double t = dc / (dc - di); double h[3];
            for (int q = 0; q < 3; ++q) h[q] = cur[q] + t * (img[m][q] - cur[q]);
            if (! inFace (f, h[0], h[1], h[2])) { ok = false; break; }
            cur[0] = h[0]; cur[1] = h[1]; cur[2] = h[2];
        }
        if (ok)
        {
            int nf = 0; for (int m = 0; m < depth; ++m) nf += F[seq[m]].floor ? 1 : 0;
            out.push_back ({ img[depth][0], img[depth][1], std::sqrt ((img[depth][0] - L[0]) * (img[depth][0] - L[0]) + (img[depth][1] - L[1]) * (img[depth][1] - L[1]) + (img[depth][2] - L[2]) * (img[depth][2] - L[2])), depth - nf, nf });
        }
    }
    if (depth >= maxN) return;
    for (int e = 0; e < nF; ++e)
    {
        const Face3& f = F[e];
        const double dist = f.nx * img[depth][0] + f.ny * img[depth][1] + f.nz * img[depth][2] + f.d;
        if (dist <= 1e-9) continue;                          // the image must be in front of the face it bounces off
        seq[depth] = e;
        img[depth + 1][0] = img[depth][0] - 2 * dist * f.nx; img[depth + 1][1] = img[depth][1] - 2 * dist * f.ny; img[depth + 1][2] = img[depth][2] - 2 * dist * f.nz;
        domeRec (F, nF, L, depth + 1, maxN, seq, img, out);
    }
}
inline void polyhedronImages (const Face3* F, int nF, const double* S, const double* L, int maxN, std::vector<Img3>& out)
{
    int seq[6] {}; double img[6][3] {}; img[0][0] = S[0]; img[0][1] = S[1]; img[0][2] = S[2]; out.clear();
    domeRec (F, nF, L, 0, std::min (maxN, 5), seq, img, out);
}

struct Img { double t, g, pan, fc; };
// v: scratch (reserve 1400 once so the audio thread never allocates)
inline void roomTaps (const RoomP& R, TapSet& out, std::vector<Img>& v)
{
    v.clear();
    const auto& M = materials();
    const auto& mw = M[(size_t) std::clamp (R.wall, 0, 6)];
    const auto& mf = M[(size_t) std::clamp (R.floor, 0, 6)];
    const double rw = std::sqrt (1 - mw.alpha), rf = std::sqrt (1 - mf.alpha);
    double sx = R.sx * R.W, sy = R.sy * R.D, lx = R.lx * R.W, ly = R.ly * R.D; const double SZ = std::min (1.6, R.H * 0.6), LZ = std::min (1.3, R.H * 0.45);
    if (R.shape != 0) { roomPos (R, R.sx, R.sy, sx, sy); roomPos (R, R.lx, R.ly, lx, ly); }
    const double dDir = std::max (0.3, std::hypot (std::hypot (sx - lx, sy - ly), SZ - LZ));
    auto img = [] (int n, double p, double D) { return n * D + ((n % 2 == 0) ? p : D - p); };
    const int N = std::clamp (R.order, 1, 5);
    if (R.shape == 3)
    {   // the dome: 3-D images (every face bounce counts once), order <= 3
        static thread_local std::vector<Img3> w3; w3.clear(); if (w3.capacity() < 1024) w3.reserve (1024);
        static thread_local Face3 F[kDomeFaces];
        const int nF = domeFaces (R, F); const double S3[3] = { sx, sy, SZ }, L3[3] = { lx, ly, LZ };
        polyhedronImages (F, nF, S3, L3, std::min (N, 3), w3);
        for (const auto& w : w3)
        {
            const double g = std::pow (rw, w.nw) * std::pow (rf, w.nf) * dDir / w.d, t = (w.d - dDir) / kC * 1000.0;
            if (t > kMaxTapMs || t < 0) continue;
            const double hf = mw.hf * w.nw + mf.hf * w.nf, air = 20000.0 * std::exp (-w.d / 140.0);
            v.push_back ({ t, g, clampd (std::sin (std::atan2 (w.ix - lx, -(w.iy - ly))), -1, 1), std::min (aToFc (std::min (0.94, hf)), air) });
        }
    }
    else if (R.shape != 0)
    {   // a polygon plan: the 2-D images (n wall bounces) x the floor / ceiling images (k), n + |k| <= N
        static thread_local std::vector<Img2> w2; w2.clear(); if (w2.capacity() < 512) w2.reserve (512);
        const auto plan = roomPoly (R);
        polyImages (plan, sx, sy, lx, ly, plan.n > 6 ? std::min (N, 4) : N, w2);
        for (int q = -1; q < (int) w2.size(); ++q)
        {
            const Img2 w = q < 0 ? Img2 { sx, sy, sx, sy, 0 } : w2[(size_t) q];
            for (int k = -(N - w.n); k <= N - w.n; ++k)
            {
                if (w.n == 0 && k == 0) continue;
                const double iz = img (k, SZ, R.H), dxy = std::hypot (w.ix - lx, w.iy - ly), d = std::hypot (dxy, iz - LZ);
                const int ak = std::abs (k), nf = k < 0 ? (ak + 1) / 2 : ak / 2, nc = ak - nf;
                const double g = std::pow (rw, w.n + nc) * std::pow (rf, nf) * dDir / d;
                const double t = (d - dDir) / kC * 1000.0;
                if (t > kMaxTapMs) continue;
                const double hf = mw.hf * (w.n + nc) + mf.hf * nf, air = 20000.0 * std::exp (-d / 140.0);
                v.push_back ({ t, g, clampd (std::sin (std::atan2 (w.ix - lx, -(w.iy - ly))), -1, 1), std::min (aToFc (std::min (0.94, hf)), air) });
            }
        }
    }
    else
    for (int i = -N; i <= N; ++i)
        for (int j = -N; j <= N; ++j)
            for (int k = -N; k <= N; ++k)
            {
                const int o = std::abs (i) + std::abs (j) + std::abs (k);
                if (o == 0 || o > N) continue;
                const double ix = img (i, sx, R.W), iy = img (j, sy, R.D), iz = img (k, SZ, R.H);
                const double d = std::hypot (std::hypot (ix - lx, iy - ly), iz - LZ);
                const int ak = std::abs (k), nf = k < 0 ? (ak + 1) / 2 : ak / 2, nc = ak - nf;   // floor / ceiling bounces
                const double g = std::pow (rw, std::abs (i) + std::abs (j) + nc) * std::pow (rf, nf) * dDir / d;
                const double t = (d - dDir) / kC * 1000.0;
                if (t > kMaxTapMs) continue;
                const double hf = mw.hf * (std::abs (i) + std::abs (j) + nc) + mf.hf * nf;
                const double air = 20000.0 * std::exp (-d / 140.0);                         // air absorption, a few kHz over 100 m
                v.push_back ({ t, g, clampd (std::sin (std::atan2 (ix - lx, -(iy - ly))), -1, 1), std::min (aToFc (std::min (0.94, hf)), air) });
            }
    if ((int) v.size() > kMaxTaps)
    {
        std::nth_element (v.begin(), v.begin() + kMaxTaps, v.end(), [] (const Img& a, const Img& b) { return a.g > b.g; });
        v.resize (kMaxTaps);
    }
    std::sort (v.begin(), v.end(), [] (const Img& a, const Img& b) { return a.t < b.t; });
    if (R.clutter > 0.001)
    {   // clutter: furniture, objects and rough surfaces scatter the specular reflections - part of each tap's energy goes to a few
        // jittered children (a denser, less regular pattern), the taps themselves move a little in time and pan, and the soft
        // furnishings take the highs. Deterministic (a hash of the tap's index), so the sound does not change from run to run.
        const double c = std::clamp (R.clutter, 0.0, 1.0);
        auto hsh = [] (uint32_t n) { n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU; n ^= n >> 16; return n / 4294967295.0 * 2 - 1; };   // -1..1
        const int n0 = (int) v.size(); std::vector<Img> kids; kids.reserve ((size_t) n0 * 2);
        for (int i = 0; i < n0; ++i)
        {
            Img& a = v[(size_t) i];
            const double s = c * 0.55 * std::min (1.0, (a.t + 4.0) / 40.0);                      // the scattered share of the energy
            const double jt = c * (0.25 + 0.03 * a.t) * hsh ((uint32_t) i * 3 + 1);
            const double t0 = a.t, g0 = a.g;
            a.t = std::max (0.0, a.t + jt); a.pan = clampd (a.pan + c * 0.22 * hsh ((uint32_t) i * 3 + 2), -1, 1);
            a.g = g0 * std::sqrt (1 - s);
            a.fc = std::min (a.fc, std::max (2500.0, 20000.0 / (1.0 + 3.0 * c * (1.0 + t0 / 25.0))));
            if (s > 0.02) for (int k = 0; k < 2; ++k)
            {
                const double h = hsh ((uint32_t) i * 7 + 11 + (uint32_t) k * 5);
                kids.push_back ({ t0 + 0.6 + (1.2 + 0.06 * t0) * (h * 0.5 + 0.5) * (k + 1), g0 * std::sqrt (s * 0.5) * (h > 0 ? 1.0 : -1.0), clampd (a.pan + 0.7 * hsh ((uint32_t) i * 7 + 13 + (uint32_t) k), -1, 1), (double) a.fc * 0.7 });
            }
        }
        for (const auto& k : kids) { if ((int) v.size() >= kMaxTaps) break; v.push_back (k); }
        if ((int) v.size() > kMaxTaps) v.resize (kMaxTaps);
        std::sort (v.begin(), v.end(), [] (const Img& a, const Img& b) { return a.t < b.t; });
    }
    out.n = (int) v.size();
    for (int n = 0; n < out.n; ++n) panTap (out.t[(size_t) n], v[(size_t) n].t, v[(size_t) n].g, v[(size_t) n].pan, v[(size_t) n].fc);
}

// furnishings (sofas, curtains, people, books...): an extra absorbing area of up to ~0.5 x the floor area, little in the bass, much in the highs
inline double clutterArea (const RoomP& R, double Sf, double f)
{
    const double a = std::clamp (0.12 + 0.2 * std::log2 (std::clamp (f, 125.0, 8000.0) / 250.0), 0.05, 0.75);      // 0.12 at 250 Hz .. ~0.75 at 4 kHz+
    return std::clamp (R.clutter, 0.0, 1.0) * 0.5 * Sf * a;
}
inline double roomRT60 (const RoomP& R)             // Sabine, for the read-out
{
    const auto& M = materials();
    const auto G = roomGeom (R); const double V = G.V, Sw = G.Sw, Sf = G.Sf;
    const double A = Sw * M[(size_t) std::clamp (R.wall, 0, 6)].alpha + Sf * M[(size_t) std::clamp (R.floor, 0, 6)].alpha + clutterArea (R, Sf, 1000.0);
    return 0.161 * V / std::max (A, 0.01);
}

// The late reverb a room of this size and these surfaces would have (Eyring + air absorption),
// per frequency. The materials' alpha is the mid value; hf (the treble lost per bounce) raises the
// absorption above 1 kHz, and soft (porous) materials absorb less in the bass. An approximation for learning.
inline double materialAlpha (const Material& m, double f)
{
    const double oct = std::log2 (f / 1000.0);
    double a = m.alpha;
    if (oct > 0) a += m.hf * oct * 0.8;
    else { const double porous = clampd ((m.alpha - 0.1) / 0.5, 0, 1); a *= 1 - 0.25 * porous * std::min (3.0, -oct); }
    return clampd (a, 0.01, 0.95);
}
inline double airM (double f)                       // energy attenuation per metre (about 20 C, 50 % RH)
{
    static const double fs[] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static const double ms[] = { 0.00005, 0.0001, 0.0003, 0.0006, 0.0012, 0.0025, 0.0065, 0.022, 0.07 };
    if (f <= fs[0]) return ms[0];
    for (int i = 0; i < 8; ++i)
        if (f <= fs[i + 1]) { const double t = std::log (f / fs[i]) / std::log (fs[i + 1] / fs[i]); return std::exp (std::log (ms[i]) + t * (std::log (ms[i + 1]) - std::log (ms[i]))); }
    return ms[8];
}
struct RoomAcoustics { double V, S, mfp, abar, rt; };
inline RoomAcoustics roomAcoustics (const RoomP& R, double f)
{
    const auto& M = materials();
    const auto G = roomGeom (R); const double V = G.V, Sf = G.Sf, Sw = G.Sw, S = G.S;
    const double aw = materialAlpha (M[(size_t) std::clamp (R.wall, 0, 6)], f), af = materialAlpha (M[(size_t) std::clamp (R.floor, 0, 6)], f);
    const double abar = std::min (0.95, (Sw * aw + Sf * af + clutterArea (R, Sf, f)) / S);
    const double rt = 0.161 * V / (-S * std::log (1 - abar) + 4 * airM (f) * V);
    return { V, S, 4 * V / S, abar, clampd (rt, 0.1, 30.0) };
}
// --------------------------------------------------------------------------
//  Connected rooms (shape 4): the two rooms as coupled energy reservoirs (the statistical model of coupled spaces).
//  dE1/dt = -c (A1 + Sd) / 4V1 E1 + c Sd / 4V1 E2,  dE2/dt = -c (A2 + Sd) / 4V2 E2 + c Sd / 4V2 E1   (A = absorption area, Sd = the door)
//  Its two decay rates give a fast and a slow slope; where the source and the listener are decides how the two add.
// --------------------------------------------------------------------------
struct CoupledGeom { double V1, Sw1, Sf1, V2, Sw2, Sf2, Sd, y1; };
inline CoupledGeom coupledGeom (const RoomP& R)
{
    const double W = R.W, D = R.D, H = R.H, t = std::min (0.4, 0.03 * D), y1 = 0.5 * D, x2 = 0.3 * W, doorW = (0.08 + 0.42 * std::clamp (R.splay, 0.0, 1.0)) * W;
    const double A1 = W * y1, A2 = (W - x2) * std::max (0.5, D - y1 - t), P1 = 2 * (W + y1), P2 = 2 * ((W - x2) + std::max (0.5, D - y1 - t)), Sd = doorW * H;
    return { A1 * H, std::max (0.0, P1 * H - Sd) + A1, A1, A2 * H, std::max (0.0, P2 * H - Sd) + A2, A2, Sd, y1 };
}
struct Coupled { double Ta = 1, Tb = 1, wa = 1, wb = 0, peakMs = 0, peak = 1; bool sameRoom = true; };      // Ta fast, Tb slow (RT60, s); wa + wb = 1 (same room)
inline Coupled coupledDecay (const RoomP& R, double f, int srcRoom, int lisRoom)
{
    const auto G = coupledGeom (R); const auto& M = materials();
    const double aw = materialAlpha (M[(size_t) std::clamp (R.wall, 0, 6)], f), af = materialAlpha (M[(size_t) std::clamp (R.floor, 0, 6)], f), air = 4 * airM (f);
    const double ba = std::clamp (R.backAbs, -1.0, 1.0), aw2 = ba >= 0 ? aw + (0.7 - aw) * ba : aw + (0.03 - aw) * -ba;
    const double A1 = G.Sw1 * aw + G.Sf1 * af + air * G.V1, A2 = G.Sw2 * aw2 + G.Sf2 * af + air * G.V2;
    const double p1 = kC * (A1 + G.Sd) / (4 * G.V1), q1 = kC * G.Sd / (4 * G.V1), p2 = kC * (A2 + G.Sd) / (4 * G.V2), q2 = kC * G.Sd / (4 * G.V2);
    const double dsc = std::sqrt ((p1 - p2) * (p1 - p2) + 4 * q1 * q2), la = -0.5 * ((p1 + p2) + dsc), lb = -0.5 * ((p1 + p2) - dsc);   // la: the faster decay
    Coupled c; c.Ta = 13.8155 / std::abs (la); c.Tb = 13.8155 / std::abs (lb); c.sameRoom = srcRoom == lisRoom;
    const double va[2] = { q1, p1 + la }, vb[2] = { q1, p1 + lb };
    const double e[2] = { srcRoom == 0 ? 1.0 : 0.0, srcRoom == 0 ? 0.0 : 1.0 }, det = va[0] * vb[1] - vb[0] * va[1];
    if (std::abs (det) < 1e-12) { c.Tb = c.Ta; return c; }
    const double alA = (e[0] * vb[1] - vb[0] * e[1]) / det, alB = (va[0] * e[1] - e[0] * va[1]) / det;
    c.wa = alA * va[lisRoom]; c.wb = alB * vb[lisRoom];
    if (c.sameRoom) { const double s = std::max (1e-9, c.wa + c.wb); c.wa = std::max (0.0, c.wa) / s; c.wb = std::max (0.0, c.wb) / s; }
    else if (c.wa < 0 && c.wb > 0 && la != lb)
    {   // the other room: its energy rises and then falls at the slow rate; the peak sets the level and (about) when the tail starts
        const double ts = std::log (std::max (1e-9, -c.wb * lb / (c.wa * la))) / (la - lb);
        c.peakMs = std::max (0.0, ts) * 1000.0; c.peak = std::max (1e-4, c.wa * std::exp (la * ts) + c.wb * std::exp (lb * ts));
    }
    return c;
}
inline void coupledSizesMs (const RoomP& R, double& ms1, double& ms2)
{
    const auto G = coupledGeom (R);
    auto one = [] (double V, double Sw, double Sf) { return clampd (2.5 * (4 * V / (Sw + Sf)) / kC * 1000.0, 10.0, 300.0); };
    ms1 = one (G.V1, G.Sw1, G.Sf1); ms2 = one (G.V2, G.Sw2, G.Sf2);
}
// which room (0 = the front one, 1 = the back one) a position (fractions of the plan) is in
inline int coupledRoomOf (const RoomP& R, double fx, double fy) { double x, y; roomPos (R, fx, fy, x, y); return y < coupledGeom (R).y1 ? 0 : 1; }

// the FDN's size for this room: a few mean free paths (10-300 ms)
inline double roomTailSizeMs (const RoomP& R)
{
    const auto a = roomAcoustics (R, 1000);
    return clampd (2.5 * a.mfp / kC * 1000.0, 10.0, 300.0);
}

// --------------------------------------------------------------------------
//  Width / size / predelay applied to any tap set (the common "voice" layer)
// --------------------------------------------------------------------------
// darken: treble lost per 100 ms of arrival time, in octaves of the tap's cut-off (0 = off)
inline void shapeTaps (const TapSet& in, TapSet& out, double predelay, double size, double width, double darken = 0)
{
    out.n = in.n;
    for (int i = 0; i < in.n; ++i)
    {
        const auto& a = in.t[(size_t) i]; auto& b = out.t[(size_t) i];
        const double c = 0.5 * (a.tL + a.tR), h = 0.5 * (a.tL - a.tR) * std::min (width, 1.5);
        b.tL = (float) std::min ((double) kMaxTapMs, predelay + size * std::max (0.05, c + h));
        b.tR = (float) std::min ((double) kMaxTapMs, predelay + size * std::max (0.05, c - h));
        const double m = 0.5 * (a.gL + a.gR), s = 0.5 * (a.gL - a.gR) * width;
        b.gL = (float) (m + s); b.gR = (float) (m - s); b.fc = a.fc;
        if (darken > 0) b.fc = (float) std::max (300.0, a.fc * std::pow (2.0, -darken * size * std::max (0.0, c) / 100.0));
    }
}

// one-pole impulse into a buffer (same kernel as the realtime tap filter)
inline void kernAbs (std::vector<float>& buf, long n, double amp, double a)
{
    if (n < 0 || n >= (long) buf.size()) return;
    double c = amp * (1 - a); const double lim = 1e-5 * std::abs (amp);
    for (int m = 0; m < 400 && n + m < (long) buf.size() && std::abs (c) > lim; ++m) { buf[(size_t) (n + m)] += (float) c; c *= a; }
}
inline double fcToA (double fc, double sr) { return std::exp (-2 * kPi * std::min (fc, sr * 0.45) / sr); }

// static render (48 kHz) with a unit direct sound at sample 0: used by the fit
inline void renderTaps (const TapSet& ts, std::vector<float>& L, std::vector<float>& R, bool direct)
{
    double tEnd = 0; for (int i = 0; i < ts.n; ++i) tEnd = std::max (tEnd, (double) std::max (ts.t[(size_t) i].tL, ts.t[(size_t) i].tR));
    const size_t len = (size_t) std::ceil (kK * (tEnd + 10)) + 420;
    L.assign (len, 0.0f); R.assign (len, 0.0f);
    if (direct) { L[0] = 1; R[0] = 1; }
    for (int i = 0; i < ts.n; ++i)
    {
        const auto& tp = ts.t[(size_t) i]; const double a = fcToA (tp.fc, kSR);
        kernAbs (L, std::lround (tp.tL * kK), tp.gL, a);
        kernAbs (R, std::lround (tp.tR * kK), tp.gR, a);
    }
}

// --------------------------------------------------------------------------
//  Analysis (port of the ER Match Lab web app)
// --------------------------------------------------------------------------
struct Sig
{
    std::vector<float> L, R, e; std::vector<double> c; long n = 0;
    void prep()
    {
        n = (long) L.size(); e.assign ((size_t) n, 0); c.assign ((size_t) n + 1, 0);
        for (long i = 0; i < n; ++i) { const double v = 0.5 * ((double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]); e[(size_t) i] = (float) v; c[(size_t) i + 1] = c[(size_t) i] + v; }
    }
    double eSum (double a, double b) const
    {
        const long A = (long) clampd (std::round (a), 0, (double) n), B = (long) clampd (std::round (b), 0, (double) n);
        return B > A ? c[(size_t) B] - c[(size_t) A] : 0;
    }
};

inline long detectT0 (const Sig& P, double nudgeMs)
{
    const long n = std::min (P.n, (long) kSR);
    auto mx = [&] (long i) { return std::max (std::abs (P.L[(size_t) i]), std::abs (P.R[(size_t) i])); };
    float m = 0; for (long i = 0; i < n; ++i) m = std::max (m, mx (i));
    long on = 0; for (long i = 0; i < n; ++i) if (mx (i) >= 0.25f * m) { on = i; break; }
    long best = on; float bv = 0;
    for (long i = on; i < std::min (P.n, on + 48); ++i) if (mx (i) > bv) { bv = mx (i); best = i; }
    return std::max (6L, best + std::lround (nudgeMs * kK));
}
inline double noiseFloorDb (const Sig& P, long t0)
{
    if (P.n < kSR * 0.4) return -70;
    const double w = std::max (std::round (P.n * 0.1), std::round (0.05 * kSR));
    const double nd = P.eSum (P.n - w, P.n) / w * kK, Ed = std::max (P.eSum (t0 - 5, t0 + 14), 1e-12);
    return clampd (10 * std::log10 (nd / Ed + 1e-12) + 3, -70, -30);
}
struct Pick { double t, db; };
inline std::vector<Pick> pickTaps (const Sig& P, long t0, double W, double thrDb)
{
    const int H = 6, Rr = 17; std::vector<Pick> out;
    auto avg = [&] (long i) { return (P.c[(size_t) std::min (P.n, i + H)] - P.c[(size_t) std::max (0L, i - H)]) / (2.0 * H); };
    double dEnv = 1e-12; for (long i = t0 - 6; i <= t0 + 6; ++i) if (i >= 0) dEnv = std::max (dEnv, avg (i));
    const long a = t0 + 34, b = std::min (P.n - 1, t0 + std::lround (W * kK)); if (b <= a) return out;
    std::vector<double> env ((size_t) (b - a + 2 * Rr + 1));
    for (size_t i = 0; i < env.size(); ++i) { const long ii = a - Rr + (long) i; env[i] = (ii >= 0 && ii < P.n) ? avg (ii) : 0; }
    for (size_t i = Rr; i + Rr < env.size(); ++i)
    {
        const double v = env[i]; if (v <= 0) continue;
        const double db = 10 * std::log10 (v / dEnv); if (db < thrDb) continue;
        bool ok = true;
        for (int j = -Rr; j <= Rr && ok; ++j) { if (! j) continue; const double u = env[(size_t) ((long) i + j)]; if (u > v || (j < 0 && u == v)) ok = false; }
        if (ok) out.push_back ({ (double) (a - Rr + (long) i - t0) / kK, db });
    }
    return out;
}

inline void fft (std::vector<std::complex<double>>& x)
{
    const size_t n = x.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (x[i], x[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPi / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1, 0);
            for (size_t k = 0; k < len / 2; ++k) { auto u = x[i + k], v = x[i + k + len / 2] * w; x[i + k] = u + v; x[i + k + len / 2] = u - v; w *= wl; }
        }
    }
}
inline std::array<double, 8> bandFeat (const Sig& P, long t0, double Wc)
{
    std::array<double, 8> out {}; const double ed[5] = { 100, 500, 2000, 6000, 16000 }; const size_t FN = 8192;
    std::vector<std::complex<double>> X (FN);
    int o = 0;
    for (auto [lo, hi] : { std::pair<double, double> { 0.5, Wc / 2 }, std::pair<double, double> { Wc / 2, Wc } })
    {
        const long a = t0 + std::lround (lo * kK); const long len = std::min ((long) FN, std::lround ((hi - lo) * kK));
        std::fill (X.begin(), X.end(), std::complex<double> (0, 0));
        for (long i = 0; i < len; ++i) { const long idx = a + i; if (idx >= P.n) break;
            const double w = 0.5 - 0.5 * std::cos (2 * kPi * (i + 0.5) / len); X[(size_t) i] = { P.L[(size_t) idx] * w, P.R[(size_t) idx] * w }; }
        fft (X);
        double bp[4] = { 0, 0, 0, 0 };
        for (size_t k = 1; k < FN / 2; ++k)
        {
            const double f = (double) k * kSR / FN; if (f < ed[0] || f >= ed[4]) continue;
            int b = 0; while (f >= ed[b + 1]) ++b;
            bp[b] += (std::norm (X[k]) + std::norm (X[FN - k])) / 2;
        }
        const double tot = bp[0] + bp[1] + bp[2] + bp[3] + 1e-20;
        for (double v : bp) out[(size_t) o++] = std::max (10 * std::log10 (v / tot + 1e-9), -40.0);
    }
    return out;
}
inline double iacc (const Sig& P, long t0, double W)
{
    const long a = t0 + std::lround (0.5 * kK), b = std::min (P.n, t0 + std::lround (W * kK)); double sx = 0, sy = 0;
    for (long i = a; i < b; ++i) { sx += (double) P.L[(size_t) i] * P.L[(size_t) i]; sy += (double) P.R[(size_t) i] * P.R[(size_t) i]; }
    if (sx * sy < 1e-20) return 1;
    double best = 0;
    for (int lag = -48; lag <= 48; lag += 2)
    {
        double s = 0;
        for (long i = a; i < b; ++i) { const long j = i + lag; if (j < 0 || j >= P.n) continue; s += (double) P.L[(size_t) i] * P.R[(size_t) j]; }
        best = std::max (best, std::abs (s));
    }
    return std::min (1.0, best / std::sqrt (sx * sy));
}

struct Feat
{
    std::vector<double> cum; double erDb = -60, Tc = 0, slope = 0, T1 = 0, iacc = 1; std::array<double, 3> cnt {}; std::array<double, 8> band {};
    std::vector<Pick> taps; bool stereo = true;
};
inline Feat features (const Sig& P, long t0, double Wc, double floorDb, bool stereo)
{
    static const double CUM[] = { 2, 3, 4, 6, 8, 10, 13, 16, 20, 25, 32, 40, 50, 65, 80, 100, 125, 160, 200, 250, 300 };
    Feat F; F.stereo = stereo;
    const double Ed = std::max (P.eSum (t0 - 5, t0 + 14), 1e-12);
    const double a = t0 + std::round (0.4 * kK), b = t0 + std::round (Wc * kK);
    F.erDb = 10 * std::log10 (P.eSum (a, b) / Ed + 1e-12);
    double sw = 0, sx = 0; for (long i = (long) a; i < (long) b && i < P.n; ++i) { sw += P.e[(size_t) i]; sx += P.e[(size_t) i] * (double) (i - t0); }
    F.Tc = sw > 0 ? sx / sw / kK : 0;
    double qd[4], qx[4];
    for (int i = 0; i < 4; ++i) { const double lo = std::max (Wc * i / 4, 0.4), hi = Wc * (i + 1) / 4;
        qd[i] = std::max (10 * std::log10 (P.eSum (t0 + lo * kK, t0 + hi * kK) / (hi - lo) / Ed + 1e-12), floorDb); qx[i] = (lo + hi) / 2; }
    double Sx = 0, Sy = 0, Sxy = 0, Sxx = 0; for (int i = 0; i < 4; ++i) { Sx += qx[i]; Sy += qd[i]; Sxy += qx[i] * qd[i]; Sxx += qx[i] * qx[i]; }
    F.slope = 10 * (4 * Sxy - Sx * Sy) / (4 * Sxx - Sx * Sx);
    for (double ed : CUM) { if (ed > Wc) break; F.cum.push_back (std::max (10 * std::log10 (P.eSum (a, t0 + ed * kK) / Ed + 1e-12), -40.0)); }
    F.taps = pickTaps (P, t0, Wc, std::max (floorDb + 6, -40.0));
    F.T1 = F.taps.empty() ? Wc : F.taps.front().t;
    const double w1 = Wc * 0.25, w2 = Wc * 0.6;
    for (auto& t : F.taps) { if (t.t < w1) F.cnt[0]++; else if (t.t < w2) F.cnt[1]++; else F.cnt[2]++; }
    F.cnt[0] /= w1; F.cnt[1] /= (w2 - w1); F.cnt[2] /= (Wc - w2);
    F.band = bandFeat (P, t0, Wc);
    F.iacc = iacc (P, t0, std::min (Wc, 80.0));
    return F;
}

struct Dist { double first = 0, env = 0, level = 0, dens = 0, spec = 0, width = 0, shape = 0, total = 0; int score = 0; };
inline Dist dist (const Feat& ft, const Feat& fe)
{
    Dist e;
    e.first = std::abs (std::log ((fe.T1 + 0.7) / (ft.T1 + 0.7))) / 0.35;
    double s = 0; int n = 0; for (size_t i = 0; i < std::min (ft.cum.size(), fe.cum.size()); ++i) { const double d = ft.cum[i] - fe.cum[i]; s += d * d; ++n; }
    e.env = std::sqrt (s / std::max (n, 1)) / 3;
    e.level = std::abs (fe.erDb - ft.erDb) / 2;
    double dn = 0; for (int i = 0; i < 3; ++i) dn += std::abs (std::log ((fe.cnt[(size_t) i] + 0.02) / (ft.cnt[(size_t) i] + 0.02)));
    e.dens = dn / 3 / 0.6;
    double sp = 0; for (int i = 0; i < 8; ++i) sp += std::abs (ft.band[(size_t) i] - fe.band[(size_t) i]); e.spec = sp / 8 / 3;
    e.width = ft.stereo ? std::abs (fe.iacc - ft.iacc) / 0.15 : 0;
    e.shape = (std::abs (fe.Tc - ft.Tc) / (0.12 * ft.Tc + 2) + std::abs (fe.slope - ft.slope) / 1.2) / 2;
    const double W[7] = { 1.2, 1.5, 1, 0.8, 0.8, 0.7, 0.6 }; const double v[7] = { e.first, e.env, e.level, e.dens, e.spec, e.width, e.shape };
    double num = 0, den = 0; for (int i = 0; i < 7; ++i) { if (i == 5 && ! ft.stereo) continue; num += W[i] * v[i] * v[i]; den += W[i]; }
    e.total = std::sqrt (num / den);
    e.score = (int) std::lround (100 * std::exp (-0.55 * e.total));
    return e;
}

// normalised echo density (Abel & Huang): the mixing time is where it reaches 1
inline double mixingTime (const Sig& P, long t0)
{
    const long w = std::lround (0.02 * kSR), half = w / 2;
    for (int t = 10; t <= 300; ++t)
    {
        const long ctr = t0 + std::lround (t * kK), a = ctr - half, b = ctr + half; if (b >= P.n) break;
        double s = 0; for (long i = a; i <= b; ++i) s += (double) P.L[(size_t) i] * P.L[(size_t) i];
        const double sd = std::sqrt (s / (w + 1)); long cnt = 0;
        for (long i = a; i <= b; ++i) if (std::abs (P.L[(size_t) i]) > sd) ++cnt;
        if ((double) cnt / (w + 1) / 0.3173 >= 1) return t;
    }
    return -1;
}

// --------------------------------------------------------------------------
//  Analysis-by-synthesis fit: ALGO params (+ width, predelay)
// --------------------------------------------------------------------------
struct FitP { PatternP p; double width = 1, predelay = 0; };
inline Feat engineFeat (const FitP& f, double Wc, double floorDb, bool stereo)
{
    TapSet raw, sh; patternTaps (f.p, raw); shapeTaps (raw, sh, f.predelay, 1.0, f.width);
    Sig S; renderTaps (sh, S.L, S.R, true); S.prep();
    return features (S, 0, Wc, floorDb, stereo);
}
inline FitP initFit (const Feat& ft, double Wc)
{
    FitP f; const double first = clampd (ft.T1, 1, 80), size = clampd (2 * ft.Tc - first, first + 4, 300);
    const double nT = ft.cnt[0] * Wc * 0.25 + ft.cnt[1] * Wc * 0.35 + ft.cnt[2] * Wc * 0.4;
    f.p.first = first; f.p.length = size; f.p.taps = clampd (nT * 0.7, 6, 120); f.p.growth = 1;
    f.p.energy = clampd (ft.erDb, -30, 12); f.p.decay = clampd (ft.slope * size / 10, -40, 0); f.p.damp = 7000;
    f.width = clampd ((1 - ft.iacc) * 1.8, 0, 1.5);
    return f;
}
inline void clampFit (FitP& f)
{
    auto& p = f.p;
    p.first = clampd (p.first, 1, 80); p.length = clampd (p.length, 8, 300); p.taps = std::round (clampd (p.taps, 4, 120));
    p.growth = clampd (p.growth, 0, 2.5); p.energy = clampd (p.energy, -30, 12); p.decay = clampd (p.decay, -40, 24);
    p.damp = clampd (p.damp, 1500, 16000); f.width = clampd (f.width, 0, 1.5); f.predelay = clampd (f.predelay, 0, 80);
    if (p.length < p.first + 3) p.length = std::min (300.0, p.first + 3);
}
// coordinate descent from f; feat(q) gives the features of what candidate q would play
template <class FeatFn, class Cancel>
inline FitP descendFit (const Feat& ft, FitP f, FeatFn feat, Cancel cancelled, Dist* outD, bool voice)
{
    auto cost = [&] (const FitP& q) { return dist (ft, feat (q)).total + 0.015 * q.predelay / 10; };
    double best = cost (f);
    enum K { first, length, energy, decay, taps, growth, damp, width, pre };
    const struct { K k; double step; bool log; } specs[] = { { first, 0, true }, { length, 0, true }, { energy, 4, false }, { decay, 8, false },
        { taps, 0, true }, { growth, 0.6, false }, { damp, 0, true }, { width, 0.3, false }, { pre, 6, false } };
    auto ref = [] (FitP& q, K k) -> double& {
        switch (k) { case first: return q.p.first; case length: return q.p.length; case energy: return q.p.energy; case decay: return q.p.decay;
                     case taps: return q.p.taps; case growth: return q.p.growth; case damp: return q.p.damp; case width: return q.width; default: return q.predelay; } };
    for (int pass = 0; pass < 7; ++pass)
    {
        const double fct = std::pow (0.6, pass);
        for (auto& s : specs)
        {
            if (cancelled()) return f;
            if (! voice && (s.k == width || s.k == pre)) continue;
            std::vector<double> cands; const double v = ref (f, s.k);
            if (s.log) for (double m : { 1 + 0.6 * fct, 1 + 0.2 * fct }) { cands.push_back (v * m); cands.push_back (v / m); }
            else for (double st : { s.step * fct, s.step * fct * 0.35 }) { cands.push_back (v + st); cands.push_back (v - st); }
            for (double c : cands) { FitP q = f; ref (q, s.k) = c; clampFit (q); const double cc = cost (q); if (cc < best - 1e-6) { best = cc; f = q; } }
        }
    }
    if (outD) *outD = dist (ft, feat (f));
    return f;
}

template <class Cancel>
inline FitP fitPattern (const Feat& ft, double Wc, double floorDb, int seed, Cancel cancelled, Dist* outD = nullptr, bool voice = true)
{
    FitP f = initFit (ft, Wc); f.p.seed = seed; if (! voice) { f.width = 1; f.predelay = 0; } clampFit (f);
    auto feat = [&] (const FitP& q) { return engineFeat (q, Wc, floorDb, ft.stereo); };
    return descendFit (ft, f, feat, cancelled, outD, voice);
}

// a tap set (e.g. ROOM) -> ALGO. Width / predelay stay at 1 / 0: VOICE applies to both sources alike.
inline void splicePatternRoom (const TapSet& pat, const TapSet& room, double early, TapSet& out);
template <class Cancel>
inline FitP fitToTaps (const TapSet& raw, int seed, Cancel cancelled, Dist* outD, double* outWc = nullptr, double early = 0)
{
    double tEnd = 0; for (int i = 0; i < raw.n; ++i) tEnd = std::max (tEnd, 0.5 * (raw.t[(size_t) i].tL + raw.t[(size_t) i].tR));
    const double Wc = clampd (tEnd + 5, 20, 300);
    Sig S; renderTaps (raw, S.L, S.R, true);
    const size_t need = (size_t) ((Wc + 20) * kK); if (S.L.size() < need) { S.L.resize (need, 0.0f); S.R.resize (need, 0.0f); }
    S.prep();
    const Feat ft = features (S, 0, Wc, -60, true);
    if (outWc) *outWc = Wc;
    FitP f = fitPattern (ft, Wc, -60, seed, cancelled, outD, false);
    if (early <= 0) return f;
    // hybrid: refine with the room's first `early` ms in front of the ALGO (what will actually play)
    auto feat = [&] (const FitP& q)
    {
        TapSet pt, sp; patternTaps (q.p, pt); splicePatternRoom (pt, raw, early, sp);
        Sig S2; renderTaps (sp, S2.L, S2.R, true);
        if (S2.L.size() < need) { S2.L.resize (need, 0.0f); S2.R.resize (need, 0.0f); }
        S2.prep(); return features (S2, 0, Wc, -60, true);
    };
    return descendFit (ft, f, feat, cancelled, outD, false);
}

// IR -> ROOM: a shoebox whose image-source ER looks like the target's ER (analysis by synthesis).
// Mirror images of a room give the same reflections, so the answer is one of several equivalent rooms.
inline Feat roomFeat (const RoomP& R, double Wc, double floorDb, bool stereo, std::vector<Img>& sc)
{
    TapSet raw; roomTaps (R, raw, sc);
    Sig S; renderTaps (raw, S.L, S.R, true);
    const size_t need = (size_t) ((Wc + 20) * kK); if (S.L.size() < need) { S.L.resize (need, 0.0f); S.R.resize (need, 0.0f); }
    S.prep();
    return features (S, 0, Wc, floorDb, stereo);
}
inline void clampRoom (RoomP& R)
{
    R.W = clampd (R.W, 1.5, 40); R.D = clampd (R.D, 1.5, 50); R.H = clampd (R.H, 2, 20);
    R.sx = clampd (R.sx, 0.05, 0.95); R.sy = clampd (R.sy, 0.05, 0.95); R.lx = clampd (R.lx, 0.05, 0.95); R.ly = clampd (R.ly, 0.05, 0.95);
    R.wall = std::clamp (R.wall, 0, 6); R.floor = std::clamp (R.floor, 0, 6); R.order = std::clamp (R.order, 1, 5);
}
template <class Cancel>
inline RoomP fitRoom (const Feat& ft, double Wc, double floorDb, Cancel cancelled, Dist* outD = nullptr)
{
    std::vector<Img> sc; sc.reserve (1400);
    auto cost = [&] (const RoomP& R) { return dist (ft, roomFeat (R, Wc, floorDb, ft.stereo, sc)).total; };
    // starting rooms: sizes from a closet to a hall, two layouts, two wall materials
    const double dims[][3] = { { 2.5, 3.2, 2.4 }, { 4, 5, 2.7 }, { 6, 8, 3.2 }, { 9, 12, 4.5 }, { 14, 20, 7 }, { 22, 32, 11 } };
    const double lay[][4] = { { 0.5, 0.3, 0.5, 0.7 }, { 0.3, 0.25, 0.65, 0.75 } };
    struct Cand { RoomP R; double c; };
    std::vector<Cand> cs;
    for (auto& d : dims) for (auto& l : lay) for (int w : { 2, 4 })
    {
        if (cancelled()) return {};
        RoomP R; R.W = d[0]; R.D = d[1]; R.H = d[2]; R.sx = l[0]; R.sy = l[1]; R.lx = l[2]; R.ly = l[3]; R.wall = w; R.floor = 3; R.order = 4;
        cs.push_back ({ R, cost (R) });
    }
    std::sort (cs.begin(), cs.end(), [] (const Cand& a, const Cand& b) { return a.c < b.c; });
    RoomP bestR = cs[0].R; double bestC = cs[0].c;
    for (int start = 0; start < 3; ++start)
    {
        RoomP f = cs[(size_t) start].R; double best = cs[(size_t) start].c;
        for (int pass = 0; pass < 7; ++pass)
        {
            const double fct = std::pow (0.6, pass);
            auto tryR = [&] (RoomP q) { clampRoom (q); const double c = cost (q); if (c < best - 1e-6) { best = c; f = q; } };
            for (int k = 0; k < 3; ++k)
                for (double mul : { 1 + 0.35 * fct, 1 / (1 + 0.35 * fct), 1 + 0.1 * fct, 1 / (1 + 0.1 * fct) })
                {
                    if (cancelled()) return bestR;
                    RoomP q = f; (k == 0 ? q.W : k == 1 ? q.D : q.H) *= mul; tryR (q);
                }
            {   // the whole room larger / smaller
                for (double mul : { 1 + 0.25 * fct, 1 / (1 + 0.25 * fct) }) { RoomP q = f; q.W *= mul; q.D *= mul; q.H *= mul; tryR (q); }
            }
            for (int k = 0; k < 4; ++k)
                for (double st : { 0.18 * fct, -0.18 * fct, 0.06 * fct, -0.06 * fct })
                {
                    if (cancelled()) return bestR;
                    RoomP q = f; (k == 0 ? q.sx : k == 1 ? q.sy : k == 2 ? q.lx : q.ly) += st; tryR (q);
                }
            for (int m = 0; m < 7; ++m) { RoomP q = f; q.wall = m; tryR (q); }
            for (int m = 0; m < 7; ++m) { RoomP q = f; q.floor = m; tryR (q); }
            for (int o : { 3, 5 }) if (pass >= 2) { RoomP q = f; q.order = o; tryR (q); }
        }
        if (best < bestC) { bestC = best; bestR = f; }
    }
    if (outD) *outD = dist (ft, roomFeat (bestR, Wc, floorDb, ft.stereo, sc));
    return bestR;
}

inline Feat tapsFeat (const TapSet& raw, double Wc)
{
    Sig S; renderTaps (raw, S.L, S.R, true);
    const size_t need = (size_t) ((Wc + 20) * kK); if (S.L.size() < need) { S.L.resize (need, 0.0f); S.R.resize (need, 0.0f); }
    S.prep();
    return features (S, 0, Wc, -60, true);
}

// ALGO with the first ms taken from ROOM (ROOM -> ALGO, hybrid): ROOM's reflections before `early`,
// the ALGO's from `early` on
inline void splicePatternRoom (const TapSet& pat, const TapSet& room, double early, TapSet& out)
{
    out.n = 0;
    for (int i = 0; i < room.n && out.n < kMaxTaps; ++i)
        if (0.5 * (room.t[(size_t) i].tL + room.t[(size_t) i].tR) < early) out.t[(size_t) out.n++] = room.t[(size_t) i];
    for (int i = 0; i < pat.n && out.n < kMaxTaps; ++i)
        if (0.5 * (pat.t[(size_t) i].tL + pat.t[(size_t) i].tR) >= early) out.t[(size_t) out.n++] = pat.t[(size_t) i];
}

// --------------------------------------------------------------------------
//  Target IR: analysis result + extracted reflections
// --------------------------------------------------------------------------
// what the audio thread needs of a target: the picked reflections, the fitted ALGO and the ER energy curve
struct IrSet
{
    TapSet taps;                         // picked reflections, tone per tap, scaled to the ER energy
    float scale = 1;                     // the energy scale applied to taps (raw = taps / scale)
    PatternP fit; float fitWidth = 1, fitPre = 0; bool fitted = false;
    std::array<float, 302> cum {};       // ER energy re direct from 0.4 ms to n ms
    float Wc = 80;
    float darken = 0;                    // treble lost over time, taken out of the taps' tone (oct / 100 ms)
};
inline double tapEnergy (const Tap& tp)
{
    const double a = fcToA (tp.fc, kSR);
    return 0.5 * ((double) tp.gL * tp.gL + (double) tp.gR * tp.gR) * (1 - a) / (1 + a);
}
// HYBRID: up to split the picked reflections at the energy of that stretch; after it the picked reflections
// keep their places, a few fill reflections go into the gaps (fill 0..1), and every 4 ms stretch is set to the IR's
// own energy there. (0.5.0 used a fitted ALGO after split: its growth crowded the reflections into a block.)
inline void hybridTaps (const IrSet& I, double split, double fill, TapSet& out)
{
    out.n = 0;
    const double Wc = std::max (2.0, (double) I.Wc); split = clampd (split, 1, Wc);
    auto cumAt = [&] (double ms) { const double x = clampd (ms, 0, 301); const int a = (int) x; const double f = x - a; return (double) I.cum[(size_t) a] * (1 - f) + (double) I.cum[(size_t) std::min (301, a + 1)] * f; };
    auto ctr = [] (const Tap& t) { return 0.5 * (t.tL + t.tR); };
    double eE = 0;
    for (int i = 0; i < I.taps.n && out.n < kMaxTaps; ++i)
    {
        const auto& tp = I.taps.t[(size_t) i];
        if (ctr (tp) >= split) continue;
        auto& o = out.t[(size_t) out.n++]; o = tp; o.gL /= I.scale; o.gR /= I.scale; eE += tapEnergy (o);
    }
    if (out.n > 0 && eE > 1e-12)
    {
        const float k = (float) clampd (std::sqrt (cumAt (split) / eE), 0.7, 2.8);
        for (int i = 0; i < out.n; ++i) { out.t[(size_t) i].gL *= k; out.t[(size_t) i].gR *= k; }
    }
    const int nEarly = out.n;
    if (Wc - split < 1 || out.n >= kMaxTaps) return;
    // late: the picked reflections as they are
    std::array<int, kMaxTaps> lateIdx {}; int nL = 0;
    for (int i = 0; i < I.taps.n && out.n < kMaxTaps; ++i)
    {
        const auto& tp = I.taps.t[(size_t) i]; const double c = ctr (tp);
        if (c < split || c > Wc) continue;
        lateIdx[(size_t) nL++] = out.n; auto& o = out.t[(size_t) out.n++]; o = tp; o.gL /= I.scale; o.gR /= I.scale;
    }
    // fill: up to twice as many again, more of them later (as the echo density of a room grows with t^2)
    const int nF = std::min (kMaxTaps - out.n, (int) std::lround (clampd (fill, 0, 1) * 2.0 * std::max (nL, 8)));
    Mulberry rnd (977u);
    double gRef = 0; for (int i = 0; i < nL; ++i) { const auto& t = out.t[(size_t) lateIdx[(size_t) i]]; gRef += 0.5 * (t.gL * t.gL + t.gR * t.gR); }
    gRef = nL > 0 ? std::sqrt (gRef / nL) : 0.05;
    for (int k = 0; k < nF; ++k)
    {
        const double t = split + (Wc - split) * std::sqrt (rnd.next());
        // tone and width from the nearest picked reflection
        double fc = 8000, pan = (rnd.next() * 2 - 1) * 0.8, bd = 1e9;
        for (int i = 0; i < nL; ++i) { const auto& q = out.t[(size_t) lateIdx[(size_t) i]]; const double d = std::abs (ctr (q) - t);
            if (d < bd) { bd = d; fc = q.fc; const double m = std::abs (q.gL) + std::abs (q.gR); pan = m > 1e-9 ? (std::abs (q.gR) - std::abs (q.gL)) / m : 0; } }
        pan = clampd ((rnd.next() < 0.5 ? -1 : 1) * std::max (std::abs (pan), 0.25 + 0.5 * rnd.next()), -1, 1);
        const double sgn = rnd.next() < 0.5 ? -1.0 : 1.0;
        panTap (out.t[(size_t) out.n++], t, sgn * gRef * (0.4 + 0.4 * rnd.next()), pan, fc * 0.85);
    }
    // every 4 ms stretch at the IR's energy; gains interpolated between the stretch centres (no steps)
    const double B = 4.0; const int nb = std::max (1, (int) std::ceil ((Wc - split) / B));
    std::vector<double> have ((size_t) nb, 0.0), kb ((size_t) nb, 1.0);
    auto bin = [&] (double c) { return std::clamp ((int) ((c - split) / B), 0, nb - 1); };
    for (int i = nEarly; i < out.n; ++i) have[(size_t) bin (ctr (out.t[(size_t) i]))] += tapEnergy (out.t[(size_t) i]);
    for (int b = 0; b < nb; ++b)
    {
        const double want = cumAt (std::min (Wc, split + (b + 1) * B)) - cumAt (split + b * B);
        kb[(size_t) b] = have[(size_t) b] > 1e-14 ? clampd (std::sqrt (want / have[(size_t) b]), 0.3, 3.0) : 1.0;
    }
    for (int i = nEarly; i < out.n; ++i)
    {
        auto& t = out.t[(size_t) i];
        const double x = (ctr (t) - split) / B - 0.5; const int b0 = std::clamp ((int) std::floor (x), 0, nb - 1), b1 = std::min (nb - 1, b0 + 1);
        const double f = clampd (x - b0, 0, 1), k = kb[(size_t) b0] * (1 - f) + kb[(size_t) b1] * f;
        t.gL *= (float) k; t.gR *= (float) k;
    }
    // the stretches with no reflection at all lose their energy: give it back over the whole late part (at most +4 dB)
    double eWant = cumAt (Wc) - cumAt (split), eHave = 0; for (int i = nEarly; i < out.n; ++i) eHave += tapEnergy (out.t[(size_t) i]);
    if (eHave > 1e-14) { const float k = (float) clampd (std::sqrt (eWant / eHave), 0.6, 1.6); for (int i = nEarly; i < out.n; ++i) { out.t[(size_t) i].gL *= k; out.t[(size_t) i].gR *= k; } }
    // time order (the engine does not need it, the displays like it)
    std::sort (out.t.begin(), out.t.begin() + out.n, [&] (const Tap& x, const Tap& y) { return ctr (x) < ctr (y); });
}

struct Target
{
    bool valid = false, wetOnly = false;
    double t0ms = 0, Wc = 80, floorDb = -60, mix = -1, c50 = 0, nativeSR = 48000, lengthMs = 0;
    Feat F;
    TapSet taps;                         // reflections (times re direct, gains re direct amplitude)
    std::vector<float> erL, erR;         // ER window at 48 kHz, direct removed, scaled to direct = 1 (for the convolution mode)
    Sig sig;                             // whole IR at 48 kHz (for drawing)
    long t0 = 0;
    FitP fit; Dist fitD; bool fitted = false;
    IrSet ir;                            // taps / fit / energy for the audio thread
};

// tone of one picked reflection: a one-pole low-pass of coefficient a scales the energy of the first
// difference by (1 - a) relative to the unfiltered pulse, so  a = 1 - M(tap) / M(reference).
inline double hfRatio (const Sig& P, long c, const std::vector<float>& x)
{
    double e = 0, d = 0;
    for (long i = c - 8; i <= c + 26; ++i)
    {
        if (i < 1 || i >= P.n) continue;
        const double v = x[(size_t) i], u = x[(size_t) i - 1];
        e += v * v; d += (v - u) * (v - u);
    }
    return e > 1e-20 ? d / e : 0;
}

// L, R at 48 kHz. wetOnly = a reverb return with no direct sound: time 0 = file start and the
// strongest peak + 6 dB stands in for the missing direct sound.
inline void analyseTarget (Target& T, std::vector<float> L, std::vector<float> R, bool wetOnly, double winMs, bool autoWin, double thrDb)
{
    T.valid = false; T.wetOnly = wetOnly;
    // 1 ms of silence in front: room for the analysis windows when the direct sound sits at sample 0
    // (e.g. an IR exported by this plug-in), and for the virtual direct sound of a wet-only IR
    const long pad = 48; float vdir = 0;
    {
        float m = 0; for (size_t i = 0; i < L.size(); ++i) m = std::max (m, std::max (std::abs (L[i]), std::abs (R[i])));
        L.insert (L.begin(), (size_t) pad, 0.0f); R.insert (R.begin(), (size_t) pad, 0.0f);
        if (wetOnly) { vdir = 2.0f * std::max (m, 1e-6f); L[(size_t) pad] += vdir; R[(size_t) pad] += vdir; }   // removed again below
    }
    T.sig.L = std::move (L); T.sig.R = std::move (R); T.sig.prep();
    if (T.sig.n < 200) return;
    const long t0 = wetOnly ? pad : detectT0 (T.sig, 0);
    T.t0 = t0; T.t0ms = (double) (t0 - pad) / kK; T.lengthMs = (double) (T.sig.n - t0) / kK;
    T.floorDb = noiseFloorDb (T.sig, t0);
    T.mix = mixingTime (T.sig, t0);
    T.Wc = autoWin && T.mix > 0 ? clampd (T.mix, 20, winMs) : winMs;
    T.Wc = std::min (T.Wc, std::max (10.0, T.lengthMs - 1));
    const double e50 = T.sig.eSum (t0, t0 + 50 * kK), tot = std::max (T.sig.eSum (t0, T.sig.n), 1e-15);
    T.c50 = 10 * std::log10 (e50 / std::max (tot - e50, 1e-15));
    T.F = features (T.sig, t0, T.Wc, T.floorDb, true);

    // direct amplitude (for normalising), then the reflections
    float dpk = 0; for (long i = std::max (0L, t0 - 3); i <= std::min (T.sig.n - 1, t0 + 3); ++i) dpk = std::max (dpk, std::max (std::abs (T.sig.L[(size_t) i]), std::abs (T.sig.R[(size_t) i])));
    const double nrm = 1.0 / std::max (1e-9f, dpk);
    auto picks = pickTaps (T.sig, t0, T.Wc, std::max (thrDb, T.floorDb + 6));
    if ((int) picks.size() > kMaxTaps)
    {
        std::nth_element (picks.begin(), picks.begin() + kMaxTaps, picks.end(), [] (const Pick& a, const Pick& b) { return a.db > b.db; });
        picks.resize (kMaxTaps);
        std::sort (picks.begin(), picks.end(), [] (const Pick& a, const Pick& b) { return a.t < b.t; });
    }
    T.taps.n = (int) picks.size();
    for (size_t k = 0; k < picks.size(); ++k)
    {
        // the signed peak of each channel within ±0.4 ms of the picked time
        const long c = t0 + std::lround (picks[k].t * kK);
        float bl = 0, br = 0; long il = c, ir = c;
        for (long i = c - 19; i <= c + 19; ++i) { if (i < 0 || i >= T.sig.n) continue;
            if (std::abs (T.sig.L[(size_t) i]) > std::abs (bl)) { bl = T.sig.L[(size_t) i]; il = i; }
            if (std::abs (T.sig.R[(size_t) i]) > std::abs (br)) { br = T.sig.R[(size_t) i]; ir = i; } }
        auto& tp = T.taps.t[k];
        tp.tL = (float) std::max (0.9, (double) (il - t0) / kK); tp.tR = (float) std::max (0.9, (double) (ir - t0) / kK);
        tp.gL = (float) (bl * nrm); tp.gR = (float) (br * nrm);
        // tone from the waveform around the peak (louder channel)
        const bool useL = std::abs (bl) >= std::abs (br);
        tp.fc = (float) hfRatio (T.sig, useL ? il : ir, useL ? T.sig.L : T.sig.R);          // the raw measure for now
    }
    // the reference: the direct sound, or (wet only) the brightest reflection
    {
        double ref = 0;
        if (! wetOnly) ref = 0.5 * (hfRatio (T.sig, t0, T.sig.L) + hfRatio (T.sig, t0, T.sig.R));
        if (ref < 0.2) for (int i = 0; i < T.taps.n; ++i) ref = std::max (ref, (double) T.taps.t[(size_t) i].fc);
        ref = std::max (ref, 1e-6);
        for (int i = 0; i < T.taps.n; ++i)
        {
            auto& tp = T.taps.t[(size_t) i];
            const double r = clampd (tp.fc / ref, 0.04, 1.0), a = std::max (fcToA (20000, kSR), 1 - r);
            tp.fc = (float) clampd (a <= 1e-4 ? 20000.0 : -std::log (a) * kSR / (2 * kPi), 1000, 20000);
            // the measured peak is the filtered one: the filter's own gain comes back out (peak = g (1 - a))
            const float k = (float) (1.0 / std::max (0.05, 1 - fcToA (tp.fc, kSR)));
            tp.gL *= k; tp.gR *= k;
        }
        // time darkening: energy-weighted line through log2 (fc) over time. It moves to the Darkening control:
        // each tap keeps its base tone, and shapeTaps puts the trend back (so the sound is unchanged at the estimate).
        double sw = 0, st = 0, sy = 0, stt = 0, sty = 0;
        for (int i = 0; i < T.taps.n; ++i)
        {
            const auto& tp = T.taps.t[(size_t) i];
            const double t = 0.5 * (tp.tL + tp.tR), y = std::log2 ((double) tp.fc);
            const double w = std::sqrt (0.5 * ((double) tp.gL * tp.gL + (double) tp.gR * tp.gR));
            sw += w; st += w * t; sy += w * y; stt += w * t * t; sty += w * t * y;
        }
        double D = 0;
        if (T.taps.n >= 6 && sw > 0)
        {
            const double den = sw * stt - st * st;
            if (den > 1e-9) D = clampd (-100.0 * (sw * sty - st * sy) / den, 0.0, 6.0);
        }
        D = std::round (D * 20.0) / 20.0;                     // 0.05 oct steps
        T.ir.darken = (float) D;
        if (D > 0)
            for (int i = 0; i < T.taps.n; ++i)
            {
                auto& tp = T.taps.t[(size_t) i];
                tp.fc = (float) std::min (24000.0, tp.fc * std::pow (2.0, D * 0.5 * (tp.tL + tp.tR) / 100.0));
            }
    }
    // the picked peaks miss the energy between them: scale them so the ER energy matches (at most +9 dB)
    {
        double e = 0; for (int i = 0; i < T.taps.n; ++i) e += tapEnergy (T.taps.t[(size_t) i]);
        const double want = std::pow (10.0, T.F.erDb / 10.0);
        T.ir.scale = 1;
        if (e > 1e-12) { const float k = (float) clampd (std::sqrt (want / e), 0.7, 2.8); T.ir.scale = k; for (int i = 0; i < T.taps.n; ++i) { T.taps.t[(size_t) i].gL *= k; T.taps.t[(size_t) i].gR *= k; } }
    }
    // the ER energy curve (re the direct-sound window, as in features())
    {
        const double Ed = std::max (T.sig.eSum (t0 - 5, t0 + 14), 1e-12), a = t0 + std::round (0.4 * kK);
        for (int m = 0; m < 302; ++m) T.ir.cum[(size_t) m] = (float) (m * kK <= 0.4 * kK ? 0.0 : std::min (T.sig.eSum (a, t0 + m * kK), T.sig.eSum (a, t0 + T.Wc * kK)) / Ed);
        T.ir.Wc = (float) T.Wc; T.ir.taps = T.taps;
    }
    // ER window for the convolution mode: 0.4 ms after the direct sound to Wc, 3 ms fade out
    const long a = t0 + std::lround (0.4 * kK), b = std::min (T.sig.n, t0 + std::lround (T.Wc * kK)), fade = std::lround (3 * kK);
    T.erL.assign ((size_t) std::max (1L, b - t0), 0.0f); T.erR.assign (T.erL.size(), 0.0f);
    for (long i = a; i < b; ++i)
    {
        double w = 1; if (i - a < 10) w = (double) (i - a) / 10.0; if (b - i < fade) w *= (double) (b - i) / fade;
        T.erL[(size_t) (i - t0)] = (float) (T.sig.L[(size_t) i] * nrm * w);
        T.erR[(size_t) (i - t0)] = (float) (T.sig.R[(size_t) i] * nrm * w);
    }
    if (wetOnly) { T.sig.L[(size_t) pad] -= vdir; T.sig.R[(size_t) pad] -= vdir; T.sig.prep(); }
    T.valid = true; T.fitted = false;
}
// tone of the picked reflections from the fitted damping
// after the fit: hand it to the HYBRID mode (the tone of each picked reflection is measured, not taken from the fit)
inline void applyFit (Target& T)
{
    T.ir.fit = T.fit.p; T.ir.fitWidth = (float) T.fit.width; T.ir.fitPre = (float) T.fit.predelay; T.ir.fitted = T.fitted;
    T.ir.taps = T.taps;
}
} // namespace er
