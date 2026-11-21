#pragma once
#include "Widgets.h"

using Preview = ERProcessor::Preview;

inline void panelBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
{
    g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty()) { g.setColour (theme::muted); g.setFont (theme::ui (11.0f, true)); g.drawText (title, r.reduced (12, 6).withHeight (16).toNearestInt(), juce::Justification::centredLeft); }
}

// improvement hints, largest error first (port of the web app's diagnose)
struct Hint { double w; juce::String text; };
inline std::vector<Hint> diagnose (const er::Feat& ft, const er::Feat& fe, bool ja)
{
    std::vector<Hint> h;
    auto add = [&] (double w, const char* j, const char* e, double v = 0, int dec = 1)
    { auto s = juce::String::fromUTF8 (ja ? j : e); h.push_back ({ w, s.replace ("%v", juce::String (std::abs (v), dec)) }); };
    const double lr = std::log ((fe.T1 + 0.7) / (ft.T1 + 0.7));
    if (std::abs (lr) > 0.12) { if (lr > 0) add (std::abs (lr) / 0.35 * 1.2, "初回反射が %v ms 遅い → First を下げる", "First reflection %v ms late -> lower First", fe.T1 - ft.T1);
                                else add (std::abs (lr) / 0.35 * 1.2, "初回反射が %v ms 早い → First を上げる", "First reflection %v ms early -> raise First", fe.T1 - ft.T1); }
    const double dl = fe.erDb - ft.erDb;
    if (std::abs (dl) > 1) { if (dl > 0) add (std::abs (dl) / 2, "ER全体が %v dB 大きい → Energy / ER Level を下げる", "ER %v dB too loud -> lower Energy / ER Level", dl);
                             else add (std::abs (dl) / 2, "ER全体が %v dB 小さい → Energy / ER Level を上げる", "ER %v dB too quiet -> raise Energy / ER Level", dl); }
    const double dc = fe.Tc - ft.Tc;
    if (std::abs (dc) > 0.12 * ft.Tc + 2) { if (dc > 0) add (std::abs (dc) / (0.12 * ft.Tc + 2) * 0.6, "ERの重心が %v ms 後ろ → Length / Size を縮める", "ER centre %v ms late -> shorten Length / Size", dc);
                                             else add (std::abs (dc) / (0.12 * ft.Tc + 2) * 0.6, "ERの重心が %v ms 前 → Length / Size を広げる", "ER centre %v ms early -> lengthen Length / Size", dc); }
    const double ds = fe.slope - ft.slope;
    if (std::abs (ds) > 0.9) { if (ds > 0) add (std::abs (ds) / 1.2 * 0.6, "減衰が緩い → Envelope を下げる（深く）", "Decays too slowly -> lower Envelope");
                               else add (std::abs (ds) / 1.2 * 0.6, "減衰が急 → Envelope を上げる（浅く）", "Decays too fast -> raise Envelope"); }
    const double rd = std::log ((fe.cnt[0] + fe.cnt[1] + fe.cnt[2] + 0.06) / (ft.cnt[0] + ft.cnt[1] + ft.cnt[2] + 0.06));
    if (std::abs (rd) > 0.3) { if (rd > 0) add (std::abs (rd) / 0.6 * 0.8, "反射が多い → Reflections を減らす", "Too many reflections -> fewer Reflections");
                               else add (std::abs (rd) / 0.6 * 0.8, "反射が少ない → Reflections を増やす", "Too few reflections -> more Reflections"); }
    const double hi = (fe.band[7] + fe.band[6]) - (ft.band[7] + ft.band[6]);
    if (std::abs (hi) > 4) { if (hi > 0) add (std::abs (hi) / 6 * 0.8, "後半の高域が %v dB 多い → Damping を下げる", "Late treble %v dB too high -> lower Damping", hi / 2);
                             else add (std::abs (hi) / 6 * 0.8, "後半の高域が %v dB 少ない → Damping を上げる", "Late treble %v dB too low -> raise Damping", hi / 2); }
    if (ft.stereo) { const double di = fe.iacc - ft.iacc;
        if (std::abs (di) > 0.1) { if (di > 0) add (std::abs (di) / 0.15 * 0.7, "左右の相関が高い（狭い） → Width を上げる", "Too correlated (narrow) -> raise Width");
                                   else add (std::abs (di) / 0.15 * 0.7, "左右の相関が低い（広い） → Width を下げる", "Too decorrelated (wide) -> lower Width"); } }
    std::sort (h.begin(), h.end(), [] (const Hint& a, const Hint& b) { return a.w > b.w; });
    return h;
}

// ===========================================================================
//  Echogram: direct (amber), reflections coloured by side, the rendered envelope,
//  and the target IR in violet.
// ===========================================================================
class Echogram : public juce::Component
{
public:
    Echogram (ParamHelp& h) : ph (h) {}
    const Preview* prev = nullptr; std::shared_ptr<const er::Target> tgt;
    // compact = the sub monitor: always the whole reverb (ER + tail, continuous), no view tabs
    bool compact = false;
    static constexpr int kMinFullH = 112;          // the smallest height that keeps the full echogram (tabs, spans, scale)
    std::vector<float> longL, longR;                    // the measured whole reverb (ER + tail + master EQ), direct at 0
    double tailStartMs = -1, erEndMs = 0;
    double spanAll() const
    {
        double ms = longL.empty() ? 0.0 : (double) longL.size() / er::kK;
        if (prev != nullptr) ms = std::max (ms, (double) prev->fullL.size() / er::kK - 50);
        if (ph.value ("tOn") > 0.5f && longL.empty())
        {   // before the first measurement: from the reverb time asked for
            double r = 0; for (double f : { 125.0, 500.0, 2000.0 }) r = std::max (r, ph.proc.rtAt (f));
            ms = std::max (ms, r * 1500.0 + ph.value ("predelay") + ph.value ("tPre"));
        }
        ms = std::max (ms, 300.0);
        const double steps[] = { 300, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 14000 };
        for (double st : steps) if (ms <= st) return st;
        return 14000;
    }
    double span() const
    {
        const int s = (int) ph.proc.uiState.getProperty ("span", 400);
        return compact || s <= 0 ? spanAll() : (double) s;
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        panelBox (g, b);
        const bool ja = ph.ja();
        // view tabs
        tabR.clear(); spanR.clear(); whoR.clear(); float tx = 10;
        if (compact)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true));
            const juce::String t = ph.tr ("ECHOGRAM（ER＋テール 全体）", "ECHOGRAM (ER + tail, whole)");
            g.drawText (t, 10, 5, 260, 18, juce::Justification::centredLeft);
            tx = 14 + juce::GlyphArrangement::getStringWidth (theme::ui (10.5f, true), t);
        }
        const char* tabs[] = { "ECHOGRAM", "SPECTRO", "WATERFALL" };
        for (int i = 0; i < (compact ? 0 : 3); ++i)
        {
            const float w = juce::GlyphArrangement::getStringWidth (theme::ui (10.5f, true), tabs[i]) + 16;
            juce::Rectangle<float> r (tx, 5, w, 20); tabR.push_back ({ r, i }); tx += w + 3;
            const bool on = view() == i;
            g.setColour (on ? theme::text : theme::shade (0.3f)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (on ? theme::panel : theme::muted); g.setFont (theme::ui (10.5f, true)); g.drawText (tabs[i], r, juce::Justification::centred);
        }
        // span buttons
        const int spans[] = { 50, 100, 200, 400, 1000, 3000, 0 };
        // narrow: the spans (and current / target) go to a second row
        const bool hasWho = ! compact && view() > 0 && tgt && tgt->valid;
        twoRows = ! compact && tx + 8 + 7 * 44 + (hasWho ? 132 : 0) > b.getRight() - 12;     // (0.8.0: the legend moves into the plot when the row is full)
        const float rowY = twoRows ? 29.0f : 5.0f;
        float sx = twoRows ? 10.0f : b.getRight() - 12 - 7 * 44;
        const int curSpan = (int) ph.proc.uiState.getProperty ("span", 400);
        for (int s : spans)
        {
            if (compact) break;
            juce::Rectangle<float> r (sx, rowY, 42, 20); spanR.push_back ({ r, s });
            const bool on = curSpan == s || (s == 0 && curSpan <= 0);
            g.setColour (on ? theme::accent : theme::shade (0.3f)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (on ? theme::onAccent : theme::muted); g.setFont (theme::mono (10.5f, on));
            g.drawText (s == 0 ? juce::String ("ALL") : s >= 1000 ? juce::String (s / 1000) + " s" : juce::String (s) + "ms", r, juce::Justification::centred); sx += 44;
        }
        // which signal (spectro / waterfall): yours or the target
        if (! compact && view() > 0 && tgt && tgt->valid)
        {
            float wx = twoRows ? 10 + 7 * 44 + 8 : b.getRight() - 12 - 7 * 44 - 8 - 2 * 62;
            for (int i = 0; i < 2; ++i)
            {
                juce::Rectangle<float> r (wx + i * 62, rowY, 60, 20); whoR.push_back ({ r, i });
                const bool on = showTarget() == (i == 1);
                g.setColour (on ? (i ? theme::target : theme::accent) : theme::shade (0.3f)); g.fillRoundedRectangle (r, 3.0f);
                g.setColour (on ? juce::Colours::white : theme::muted); g.setFont (theme::ui (10.0f, on));
                g.drawText (i ? ph.tr ("ターゲット", "Target") : ph.tr ("いまの音", "Current"), r, juce::Justification::centred);
            }
        }
        if (view() == 0 || compact)
        {
            g.setFont (theme::ui (10.0f)); float lx = tx + 10, ly = 6;
            if (! compact && ! twoRows && tx + 10 + 190 > b.getRight() - 12 - 7 * 44 - 8) { lx = 50; ly = 33; }       // (no room after the tabs: inside the plot, top-left)
            auto leg = [&] (juce::Colour c, const juce::String& s) { g.setColour (c); g.fillRect (lx, ly + 5, 8.0f, 8.0f); g.setColour (theme::muted); g.drawText (s, (int) lx + 11, (int) ly, 120, 18, juce::Justification::centredLeft); lx += 16 + juce::GlyphArrangement::getStringWidth (theme::ui (10.0f), s) + 8; };
            leg (theme::direct, ph.tr ("直接音", "Direct"));
            if (! compact) { leg (theme::side, ph.tr ("左寄り", "Left-ish")); leg (theme::accent, ph.tr ("右寄り", "Right-ish")); }
            if (compact) leg (theme::accent.withAlpha (0.5f), ph.tr ("リバーブ全体", "Whole reverb"));
            else
            {
                if (tgt && tgt->valid) leg (theme::target, ph.tr ("ターゲット", "Target"));
                if (ghost != nullptr && ! ghost->L.empty()) leg (theme::muted, ph.tr ("変更前（破線）", "Before (dashed)"));
            }
        }
        if (view() > 0 && ! compact) { paintSpectral (g, b); return; }
        plot = b.withTrimmedTop (compact ? 26 : twoRows ? 54 : 30).withTrimmedLeft (42).withTrimmedRight (14).withTrimmedBottom (compact ? 18 : 24);
        g.setColour (theme::well); g.fillRect (plot);
        const double S = span(); lastS = S;
        auto X = [&] (double t) { return plot.getX() + (float) (t / S) * plot.getWidth(); };
        // the whole-reverb views go down to -84 dB (the tail's decay is longer than the ER's 60 dB window)
        const double dbRange = compact || S > 1000 ? 90.0 : 60.0;
        auto Y = [&] (double db) { return plot.getY() + (float) ((6 - db) / dbRange) * plot.getHeight(); };
        // zones
        g.setColour (theme::warn.withAlpha (0.08f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (X (0), plot.getY(), X (std::min (S, 1.0)), plot.getBottom()));
        g.setColour (theme::accent.withAlpha (0.05f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (X (1), plot.getY(), X (std::min (S, 30.0)), plot.getBottom()));
        // grid
        g.setFont (theme::mono (9.5f));
        // (0.8.0) the dB step follows the plot's height: a label every 16 px at least
        int dbStep = compact ? 24 : dbRange > 60 ? 12 : 6;
        while (plot.getHeight() * dbStep / dbRange < 16 && dbStep < 48) dbStep *= 2;
        for (int db = 0; db >= 6 - (int) dbRange; db -= dbStep)
        {
            g.setColour (db == 0 ? theme::line : theme::grid); g.drawHorizontalLine ((int) Y (db), plot.getX(), plot.getRight());
            g.setColour (theme::faint); g.drawText (minus (juce::String (db)), 0, (int) Y (db) - 7, 38, 14, juce::Justification::centredRight);
        }
        const bool secs = S > 1000;
        double step = S <= 50 ? 5 : S <= 100 ? 10 : S <= 200 ? 20 : S <= 400 ? 50 : S <= 1000 ? 100 : S <= 3000 ? 500 : 1000;
        if (compact && S <= 1000) step *= 2;
        for (double t = 0; t <= S + 0.01; t += step)
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) X (t), plot.getY(), plot.getBottom());
            g.setColour (theme::faint); g.drawText (secs ? juce::String (t / 1000.0, step < 1000 ? 1 : 0) : juce::String ((int) t), (int) X (t) - 20, (int) plot.getBottom() + 3, 40, 14, juce::Justification::centred);
        }
        g.drawText (secs ? "s" : "ms", 0, (int) plot.getBottom() + 3, 38, 14, juce::Justification::centredRight);
        if (S >= 50 && S <= 1000 && ! compact)
        {
            g.setColour (theme::warn.withAlpha (0.5f));
            const float x50 = X (50); for (float y = plot.getY(); y < plot.getBottom(); y += 6) g.drawLine (x50, y, x50, y + 3, 1.0f);
            g.setFont (theme::ui (9.5f)); g.drawText (ph.tr ("エコー閾 50 ms", "echo threshold 50 ms"), (int) x50 + 4, (int) plot.getY() + 20, 140, 12, juce::Justification::centredLeft);
        }
        g.setColour (theme::muted.withAlpha (0.8f)); g.setFont (theme::ui (9.5f));
        if (! compact && S <= 400) {
        if (S <= 100) g.drawText (ph.tr ("コム", "comb"), (int) X (0) + 2, (int) plot.getBottom() - 14, 40, 12, juce::Justification::centredLeft);
        if (X (std::min (S, 30.0)) - plot.getX() > 172)
            g.drawText (ph.tr ("融合（先行音効果）1–30 ms", "fusion (precedence) 1-30 ms"), (int) X (std::min (S, 30.0)) - 170, (int) plot.getY() + 20, 166, 12, juce::Justification::centredRight);
        else if (X (std::min (S, 30.0)) - plot.getX() > 30)
            g.drawText (ph.tr ("融合", "fusion"), (int) X (0) + 3, (int) plot.getY() + 20, (int) (X (std::min (S, 30.0)) - X (0)) - 4, 12, juce::Justification::centredRight);
        }
        // where the ER sits and where the tail comes in
        if (erEndMs > 0 && erEndMs < S && S > 150)
        {
            g.setColour (theme::side.withAlpha (0.10f)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (X (0), plot.getY(), X (erEndMs), plot.getBottom()));
            if (X (erEndMs) - X (0) > 26) { g.setColour (theme::side.withAlpha (0.8f)); g.setFont (theme::ui (9.5f, true)); g.drawText ("ER", (int) X (0) + 6, (int) plot.getBottom() - 14, 30, 12, juce::Justification::centredLeft); }
        }
        g.saveState(); g.reduceClipRegion (plot.toNearestInt());
        // our own taps are drawn at the level heard: ER Level (+ Auto Level) and the ER / TAIL balance
        const double ownDb = ph.value ("level") + ph.proc.autoComp.load() + 20 * std::log10 (std::max (1e-6f, ph.proc.erBalG()));
        auto barsOf = [&] (const er::TapSet& ts, bool isTarget)
        {
            const double off = isTarget ? 0.0 : ownDb;
            for (int i = 0; i < ts.n; ++i)
            {
                const auto& t = ts.t[(size_t) i];
                const double e = 0.5 * ((double) t.gL * t.gL + (double) t.gR * t.gR); if (e <= 1e-9) continue;
                const double db = 10 * std::log10 (e) + off, tm = 0.5 * (t.tL + t.tR); if (tm > S) continue;
                const float x = X (tm);
                juce::Colour c = theme::target;
                if (! isTarget)
                {
                    const float al = std::abs (t.gL), ar = std::abs (t.gR), pan = (ar - al) / std::max (1e-6f, al + ar);
                    c = theme::side.interpolatedWith (theme::accent, juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * pan));
                }
                g.setColour (c.withAlpha (isTarget ? 0.45f : 0.9f));
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x - (isTarget ? 0.5f : 1.0f), Y (db), x + (isTarget ? 1.5f : 1.0f), plot.getBottom()));
            }
        };
        auto envOf = [&] (const std::vector<float>& L, const std::vector<float>& R, long off, double norm, juce::Colour c, float w)
        {
            juce::Path p; bool st = false; const double bin = std::max (0.25, S / plot.getWidth());
            for (double t = 0; t < S; t += bin)
            {
                const long a = off + (long) (t * er::kK), bb = off + (long) ((t + bin) * er::kK);
                float pk = 0; for (long i = a; i < bb && i < (long) L.size(); ++i) if (i >= 0) pk = std::max (pk, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
                const double db = juce::jlimit (-60.0, 12.0, 20 * std::log10 (pk * norm + 1e-9));
                const float x = X (t + bin * 0.5), y = Y (db);
                if (! st) { p.startNewSubPath (x, y); st = true; } else p.lineTo (x, y);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (w));
        };
        if (tgt && tgt->valid)
        {
            float dpk = 0; for (long i = std::max (0L, tgt->t0 - 3); i <= tgt->t0 + 3 && i < tgt->sig.n; ++i) dpk = std::max (dpk, std::max (std::abs (tgt->sig.L[(size_t) i]), std::abs (tgt->sig.R[(size_t) i])));
            if (tgt->wetOnly)        // the virtual direct sound was 2 x the strongest peak
            {
                float m = 0; for (auto v : tgt->sig.L) m = std::max (m, std::abs (v)); for (auto v : tgt->sig.R) m = std::max (m, std::abs (v));
                dpk = 2 * m;
            }
            barsOf (tgt->taps, true);
            envOf (tgt->sig.L, tgt->sig.R, tgt->t0 + (long) (0.4 * er::kK), 1.0 / std::max (1e-9f, dpk), theme::target.withAlpha (0.85f), 1.3f);
            if (tgt->mix > 0 && tgt->mix < S)
            {
                g.setColour (theme::target.withAlpha (0.7f)); const float xm = X (tgt->mix);
                for (float y = plot.getY(); y < plot.getBottom(); y += 8) g.drawLine (xm, y, xm, y + 4, 1.2f);
                g.setFont (theme::ui (9.5f)); g.drawText (ph.tr ("混合時間", "mixing time") + " " + fmtMs (tgt->mix), (int) xm + 4, (int) plot.getY() + 16, 140, 12, juce::Justification::centredLeft);
            }
        }
        if (ghost != nullptr && ! ghost->L.empty())
        {
            // the state before the last change, dashed
            juce::Path p; bool st = false; const double bin = std::max (0.5, S / plot.getWidth() * 2);
            for (double t = 0; t < S; t += bin)
            {
                const long a = (long) (t * er::kK), bb = (long) ((t + bin) * er::kK); float pk = 0;
                for (long i = a; i < bb && i < (long) ghost->L.size(); ++i) pk = std::max (pk, std::max (std::abs (ghost->L[(size_t) i]), std::abs (ghost->R[(size_t) i])));
                const float x = X (t + bin * 0.5), y = Y (juce::jlimit (-60.0, 12.0, 20 * std::log10 (pk + 1e-9)));
                if (! st) { p.startNewSubPath (x, y); st = true; } else p.lineTo (x, y);
            }
            juce::Path d; const float dl[] = { 4.0f, 3.0f }; juce::PathStrokeType (1.2f).createDashedStroke (d, p, dl, 2);
            g.setColour (theme::warn.withAlpha (0.75f)); g.fillPath (d);
        }
        const bool haveFull = prev != nullptr && ! prev->fullL.empty();
        if (haveFull || ! longL.empty())
        {   // the whole reverb (ER + tail, after the master EQ): filled peak envelope
            // (the live preview as far as it goes; past it, the background render of the whole reverb)
            const size_t nF = haveFull ? prev->fullL.size() : 0;
            juce::Path p; const double bin = std::max (0.25, S / plot.getWidth() * 1.5);
            p.startNewSubPath (X (0), plot.getBottom());
            for (double t = 0; t < S; t += bin)
            {
                const long a = (long) (t * er::kK), bb = (long) ((t + bin) * er::kK); float pk = 0;
                for (long i = a; i < bb; ++i)
                {
                    if (i < (long) nF) pk = std::max (pk, std::max (std::abs (prev->fullL[(size_t) i]), std::abs (prev->fullR[(size_t) i])));
                    else if (i < (long) longL.size()) pk = std::max (pk, std::max (std::abs (longL[(size_t) i]), std::abs (longR[(size_t) i])));
                    else break;
                }
                p.lineTo (X (t + bin * 0.5), Y (juce::jlimit (6 - dbRange, 12.0, 20 * std::log10 (pk + 1e-9))));
            }
            p.lineTo (X (S), plot.getBottom()); p.closeSubPath();
            g.setColour (theme::accent.withAlpha (compact ? 0.22f : 0.13f)); g.fillPath (p);
            g.setColour (theme::accent.withAlpha (compact ? 0.7f : 0.45f)); g.strokePath (p, juce::PathStrokeType (0.8f));
        }
        if (prev != nullptr)
        {
            if (! prev->conv && S <= 3000) barsOf (prev->taps, false);
            if (S <= 400) envOf (prev->L, prev->R, 0, 1.0, theme::text.withAlpha (0.55f), 1.1f);
        }
        // direct
        g.setColour (theme::direct); g.fillRect (X (0) - 1.5f, Y (0), 3.0f, plot.getBottom() - Y (0));
        g.restoreState();

        if (tailStartMs >= 0 && tailStartMs < S)
        {
            const float xt = X (tailStartMs); g.setColour (theme::accent.withAlpha (0.75f));
            for (float y = plot.getY(); y < plot.getBottom(); y += 7) g.drawLine (xt, y, xt, y + 3, dragTail ? 2.4f : 1.2f);
            if (! compact && onTailDrag) { g.fillEllipse (xt - 4, plot.getBottom() - 9, 8, 8); }
            g.setFont (theme::ui (9.5f, true));
            const auto tl = ph.tr ("テール開始 ", "TAIL from ") + (tailStartMs >= 1000 ? juce::String (tailStartMs / 1000.0, 2) + " s" : fmtMs ((float) tailStartMs));
            g.setColour (theme::well.withAlpha (0.85f)); g.fillRect ((float) xt + 2, plot.getBottom() - 31, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tl) + 6, 14.0f); g.setColour (theme::accent);
            g.drawText (ph.tr ("テール開始 ", "TAIL from ") + (tailStartMs >= 1000 ? juce::String (tailStartMs / 1000.0, 2) + " s" : fmtMs ((float) tailStartMs)), (int) xt + 4, (int) plot.getBottom() - 30, 120, 12, juce::Justification::centredLeft);
        }

        // ITDG marker
        if (! compact && S <= 1000 && prev != nullptr && ! prev->conv && prev->taps.n > 0)
        {
            double t1 = 1e9; for (int i = 0; i < prev->taps.n; ++i) t1 = std::min (t1, 0.5 * (double) (prev->taps.t[(size_t) i].tL + prev->taps.t[(size_t) i].tR));
            if (t1 < S)
            {
                const float y = plot.getY() + 6, x0 = X (0), x1 = X (t1);
                g.setColour (theme::direct.withAlpha (0.9f)); g.drawLine (x0, y, x1, y, 1.2f); g.drawLine (x1, y - 4, x1, y + 4, 1.2f);
                g.setFont (theme::mono (10.0f, true)); g.drawText ("ITDG " + fmtMs (t1), (int) x1 + 4, (int) y - 7, 110, 14, juce::Justification::centredLeft);
            }
        }
        // score chip
        if (! compact && prev != nullptr && prev->hasFeat)
        {
            juce::Rectangle<float> r (plot.getRight() - 112, plot.getY() + 8, 104, 40);
            g.setColour (theme::panel.withAlpha (0.92f)); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (theme::target); g.drawRoundedRectangle (r, 5.0f, 1.2f);
            g.setFont (theme::ui (9.5f)); g.setColour (theme::muted); g.drawText (ph.tr ("一致度", "MATCH"), r.withHeight (14).translated (0, 3), juce::Justification::centred);
            g.setFont (theme::mono (18.0f, true)); g.setColour (theme::text); g.drawText (juce::String (prev->d.score), r.withTrimmedTop (14), juce::Justification::centred);
        }
        // cursor read-out
        if (hover && plot.contains (mouse))
        {
            const double t = (mouse.x - plot.getX()) / plot.getWidth() * S, db = 6 - (mouse.y - plot.getY()) / plot.getHeight() * 60.0;
            g.setColour (theme::muted); g.setFont (theme::mono (10.0f));
            g.drawText ((t >= 1000 ? juce::String (t / 1000.0, 2) + " s" : fmtMs (t)) + "  " + minus (juce::String (db, 1)) + " dB", (int) plot.getX() + 6, (int) plot.getBottom() - 30, 200, 14, juce::Justification::centredLeft);
        }
        juce::ignoreUnused (ja);
    }
    // the waterfall's angle (uiState wfDx, wfDy: how far the time axis goes right and up, as parts of the plot)
    float wfDx() const { return juce::jlimit (0.0f, 0.45f, (float) (double) ph.proc.uiState.getProperty ("wfDx", 0.22)); }
    float wfDy() const { return juce::jlimit (0.1f, 0.7f, (float) (double) ph.proc.uiState.getProperty ("wfDy", 0.45)); }
    std::function<void (double)> onTailDrag;           // the echogram: drag the "tail from" line (the editor sets Tail Delay / IR start)
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragTail && onTailDrag)
        {
            const double t = juce::jlimit (0.0, lastS, (double) ((e.position.x - plot.getX()) / plot.getWidth()) * lastS);
            onTailDrag (t); repaint(); return;
        }
        if (dragWf)
        {
            ph.proc.uiState.setProperty ("wfDx", juce::jlimit (0.0, 0.45, wfStart.x + (e.position.x - e.mouseDownPosition.x) / plot.getWidth()), nullptr);
            ph.proc.uiState.setProperty ("wfDy", juce::jlimit (0.1, 0.7, wfStart.y - (e.position.y - e.mouseDownPosition.y) / plot.getHeight()), nullptr);
            repaint();
        }
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragTail) { dragTail = false; repaint(); } dragWf = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (view() == 2 && plot.contains (e.position)) { ph.proc.uiState.removeProperty ("wfDx", nullptr); ph.proc.uiState.removeProperty ("wfDy", nullptr); repaint(); }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragTail = dragWf = false;
        if (! compact && view() == 0 && onTailDrag && tailStartMs >= 0 && tailStartMs < lastS && plot.contains (e.position)
            && std::abs (e.position.x - (plot.getX() + (float) (tailStartMs / lastS) * plot.getWidth())) < 7) { dragTail = true; repaint(); return; }
        if (! compact && view() == 2 && plot.contains (e.position)) { dragWf = true; wfStart = { wfDx(), wfDy() }; }
        for (auto& [r, s] : spanR) if (r.contains (e.position)) { ph.proc.uiState.setProperty ("span", s, nullptr); repaint(); return; }
        for (auto& [r, i] : tabR) if (r.contains (e.position)) { ph.proc.uiState.setProperty ("echoView", i, nullptr); repaint(); return; }
        for (auto& [r, i] : whoR) if (r.contains (e.position)) { ph.proc.uiState.setProperty ("specTgt", i == 1, nullptr); repaint(); return; }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        mouse = e.position; hover = true;
        const bool onTail = ! compact && view() == 0 && onTailDrag && tailStartMs >= 0 && tailStartMs < lastS && plot.contains (e.position)
                            && std::abs (e.position.x - (plot.getX() + (float) (tailStartMs / lastS) * plot.getWidth())) < 7;
        setMouseCursor (onTail ? juce::MouseCursor::LeftRightResizeCursor : ! compact && view() == 2 && plot.contains (e.position) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    const Preview* ghost = nullptr;
    int view() const { return juce::jlimit (0, 2, (int) ph.proc.uiState.getProperty ("echoView", 0)); }      // (not "view": that is the display mode)
    bool showTarget() const { return tgt && tgt->valid && (bool) ph.proc.uiState.getProperty ("specTgt", false); }

private:
    // ---------- spectrogram / waterfall (generic: any L/R buffer at 48 kHz, so the tail can share it) ----------
    static juce::Colour heat (float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        const juce::Colour stops[] = { theme::well, juce::Colour (0xff1f3a7a), juce::Colour (0xff1aa0a0), juce::Colour (0xfff0b34a), juce::Colour (0xfffff3d6) };
        const float x = t * 4; const int i = std::min (3, (int) x);
        return stops[i].interpolatedWith (stops[i + 1], x - (float) i);
    }
    void signal (std::vector<float>& m) const          // mono, ER only, direct at time 0 = sample 0
    {
        m.clear();
        if (showTarget())
        {
            float dpk = 0; for (long i = std::max (0L, tgt->t0 - 3); i <= tgt->t0 + 3 && i < tgt->sig.n; ++i) dpk = std::max (dpk, std::max (std::abs (tgt->sig.L[(size_t) i]), std::abs (tgt->sig.R[(size_t) i])));
            if (tgt->wetOnly) { float mx = 0; for (auto v : tgt->sig.L) mx = std::max (mx, std::abs (v)); dpk = 2 * mx; }
            const float k = 0.5f / std::max (1e-9f, dpk); const long a = tgt->t0 + (long) (0.4 * er::kK);
            for (long i = tgt->t0; i < tgt->sig.n; ++i) m.push_back (i < a ? 0.0f : (tgt->sig.L[(size_t) i] + tgt->sig.R[(size_t) i]) * k);
        }
        else if (prev != nullptr)
        {
            const auto& A = prev->fullL.empty() ? prev->L : prev->fullL; const auto& B = prev->fullL.empty() ? prev->R : prev->fullR;
            for (size_t i = 0; i < A.size(); ++i) m.push_back (0.5f * (A[i] + B[i]));
        }
    }
    void paintSpectral (juce::Graphics& g, juce::Rectangle<float> b)
    {
        plot = b.withTrimmedTop (twoRows ? 54 : 30).withTrimmedLeft (42).withTrimmedRight (40).withTrimmedBottom (24);
        g.setColour (theme::well); g.fillRect (plot);
        const double S = span();
        const int key = (prev ? prev->serial : 0) * 7 + view() * 1000003 + (int) S * 31 + (showTarget() ? 17 : 0) + (int) plot.getWidth() * 131 + (int) plot.getHeight() * 977
                      + (view() == 2 ? (int) (wfDx() * 1000) * 7919 + (int) (wfDy() * 1000) * 104729 : 0);
        if (key != cacheKey || ! cache.isValid()) { render (S); cacheKey = key; }
        g.drawImage (cache, plot);
        // axes
        g.setFont (theme::mono (9.5f)); g.setColour (theme::faint);
        const double fLo = 50, fHi = 20000;
        auto fy = [&] (double f) { return plot.getBottom() - (float) (std::log (f / fLo) / std::log (fHi / fLo)) * plot.getHeight(); };
        if (view() == 1)
        {
            for (double f : { 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
            { g.setColour (theme::hilite (0.12f)); g.drawHorizontalLine ((int) fy (f), plot.getX(), plot.getRight());
              g.setColour (theme::faint); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), 0, (int) fy (f) - 7, 38, 14, juce::Justification::centredRight); }
            const double step = S <= 50 ? 5 : S <= 100 ? 10 : S <= 200 ? 20 : S <= 400 ? 50 : S <= 1000 ? 100 : 500;
            for (double t = 0; t <= S + 0.01; t += step)
                g.drawText (juce::String ((int) t), (int) (plot.getX() + t / S * plot.getWidth()) - 20, (int) plot.getBottom() + 3, 40, 14, juce::Justification::centred);
            g.drawText ("ms", 0, (int) plot.getBottom() + 3, 38, 14, juce::Justification::centredRight);
            g.drawText ("Hz", 0, (int) plot.getY() - 2, 38, 12, juce::Justification::centredRight);
            // colour scale
            for (int i = 0; i < (int) plot.getHeight(); ++i) { g.setColour (heat (1.0f - i / plot.getHeight())); g.drawHorizontalLine ((int) plot.getY() + i, plot.getRight() + 8, plot.getRight() + 16); }
            g.setColour (theme::faint); g.drawText ("0", (int) plot.getRight() + 18, (int) plot.getY() - 4, 22, 12, juce::Justification::centredLeft);
            g.drawText (minus ("-72"), (int) plot.getRight() + 16, (int) plot.getBottom() - 10, 26, 12, juce::Justification::centredLeft);
            g.drawText ("dB", (int) plot.getRight() + 18, (int) plot.getCentreY() - 6, 22, 12, juce::Justification::centredLeft);
        }
        else
        {   // the three axes: frequency along the front edge, time going back, level up the front-left edge
            const float W = plot.getWidth(), H = plot.getHeight(), dX = W * wfDx(), dY = H * wfDy(), bW = W - dX, bH = H - dY;
            const float x0 = plot.getX(), y0 = plot.getBottom();
            g.setColour (theme::muted.withAlpha (0.8f));
            g.drawLine (x0, y0, x0 + bW, y0, 1.0f);                       // frequency (front)
            g.drawLine (x0, y0, x0 + dX, y0 - dY, 1.0f);                  // time (going back)
            g.drawLine (x0, y0, x0, y0 - bH * 0.95f, 1.0f);              // level (front)
            g.setColour (theme::faint);
            for (double f : { 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
            { const float x = x0 + (float) (std::log (f / fLo) / std::log (fHi / fLo)) * bW;
              g.drawVerticalLine ((int) x, y0, y0 + 3);
              g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) y0 + 3, 40, 14, juce::Justification::centred); }
            g.drawText ("Hz", (int) (x0 + bW) - 2, (int) y0 + 3, 30, 14, juce::Justification::centredLeft);
            // time: along the right-hand depth edge, labels in the right margin
            g.setColour (theme::muted.withAlpha (0.8f)); g.drawLine (x0 + bW, y0, x0 + W, y0 - dY, 1.0f);
            g.setColour (theme::faint);
            const double T = S * 0.8;
            for (int k = 0; k <= 4; ++k)
            {
                const float u = k / 4.0f, x = x0 + bW + dX * u, y = y0 - dY * u;
                g.drawLine (x, y, x + 3, y, 1.0f);
                if (k > 0) g.drawText (juce::String ((int) std::round (T * u)) + (k == 4 ? "ms" : ""), (int) x + 4, (int) y - 7, 40, 14, juce::Justification::centredLeft);
            }
            // level: up the front-left edge, labels in the left margin
            for (int d = 0; d <= 60; d += 20)
            {
                const float y = y0 - (float) ((60 - d + 0.0) / 62.0) * bH * 0.95f;
                g.drawLine (x0 - 3, y, x0, y, 1.0f);
                g.drawText (d == 0 ? juce::String ("0") : minus ("-" + juce::String (d)), 0, (int) y - 7, (int) x0 - 5, 14, juce::Justification::centredRight);
            }
            g.drawText ("dB", 0, (int) (y0 - bH * 0.95f) - 22, (int) x0 - 5, 12, juce::Justification::centredRight);
            g.drawText (ph.tr ("0 dB = 最大 ", "0 dB = top ") + minus (juce::String ((int) std::round (wfTop))) + ph.tr (" dB ／ ドラッグで角度、ダブルクリックで戻す", " dB / drag: angle, double-click: reset"),
                        (int) plot.getRight() - 360, (int) plot.getY() + 4, 354, 12, juce::Justification::centredRight);
        }
        g.setColour (showTarget() ? theme::target : theme::accent); g.setFont (theme::ui (10.0f, true));
        const juce::String lab = showTarget() ? ph.tr ("ターゲット IR の ER（直接音なし）", "Target IR, ER only (no direct sound)")
                               : prev != nullptr && prev->conv ? ph.tr ("いま鳴っている ER：IR を畳み込み（直接音なし）", "ER playing now: the IR convolved (no direct sound)")
                                                               : ph.tr ("いま鳴っている ER（直接音なし）", "ER playing now (no direct sound)");
        {
            const float w = juce::GlyphArrangement::getStringWidth (theme::ui (10.0f, true), lab) + 12;
            g.setColour (theme::panel.withAlpha (0.85f)); g.fillRoundedRectangle (plot.getX() + 3, plot.getY() + 2, w, 16, 3.0f);
            g.setColour (showTarget() ? theme::target : theme::accent);
            g.drawText (lab, (int) plot.getX() + 9, (int) plot.getY() + 4, (int) w, 12, juce::Justification::centredLeft);
        }
    }
    void render (double S)
    {
        const int W = std::max (8, (int) plot.getWidth()), H = std::max (8, (int) plot.getHeight());
        cache = juce::Image (juce::Image::ARGB, W, H, true);
        std::vector<float> m; signal (m);
        const size_t need = (size_t) ((S + 60) * er::kK); if (m.size() < need) m.resize (need, 0.0f);
        const double fLo = 50, fHi = 20000;
        if (view() == 1)
        {
            const int order = 10, N = 1 << order, win = 512; juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) (2 * N)), mag ((size_t) (N / 2));
            juce::Image::BitmapData bd (cache, juce::Image::BitmapData::writeOnly);
            const int cols = std::min (W, 400);
            for (int c = 0; c < cols; ++c)
            {
                const double t = S * c / cols; const long centre = (long) (t * er::kK);
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int i = 0; i < win; ++i) { const long k = centre - win / 2 + i; if (k < 0 || k >= (long) m.size()) continue;
                    buf[(size_t) i] = m[(size_t) k] * (0.5f - 0.5f * std::cos (2 * juce::MathConstants<float>::pi * (i + 0.5f) / win)); }
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int k = 0; k < N / 2; ++k) mag[(size_t) k] = buf[(size_t) k];     // an impulse of 1.0 (= the direct sound) at the window centre -> 0 dB
                const int x0 = c * W / cols, x1 = (c + 1) * W / cols;
                for (int y = 0; y < H; ++y)
                {
                    const double f = fLo * std::pow (fHi / fLo, 1.0 - (y + 0.5) / H), bin = f * N / er::kSR;
                    const int b0 = std::min (N / 2 - 2, (int) bin); const float fr = (float) (bin - b0);
                    const float a = mag[(size_t) b0] * (1 - fr) + mag[(size_t) b0 + 1] * fr;
                    const float db = 20 * std::log10 (a + 1e-9f);
                    const auto col = heat ((db + 72) / 72);
                    for (int x = x0; x < x1; ++x) bd.setPixelColour (x, y, col);
                }
            }
        }
        else
        {
            juce::Graphics g (cache);
            const int K = 32, order = 11, N = 1 << order; juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) (2 * N));
            const float depthX = W * wfDx(), depthY = H * wfDy(), baseW = W - depthX, baseH = H - depthY;
            auto xf = [&] (double f) { return (float) (std::log (f / fLo) / std::log (fHi / fLo)) * baseW; };
            // pass 1: every slice's smoothed spectrum (cumulative spectral decay), pass 2: draw back to front
            const int P = 160; std::vector<std::vector<float>> sl ((size_t) K, std::vector<float> ((size_t) P + 1));
            float mx = -200;
            for (int k = 0; k < K; ++k)
            {
                const double t0 = S * 0.8 * k / (K - 1); const long a = (long) (t0 * er::kK);
                std::fill (buf.begin(), buf.end(), 0.0f);
                const int len = N, taper = len / 4;
                for (int i = 0; i < len; ++i) { const long j = a + i; if (j >= (long) m.size()) break;
                    float w = 1; if (i > len - taper) w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (i - (len - taper)) / (float) taper);
                    buf[(size_t) i] = m[(size_t) j] * w; }
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int q = 0; q <= P; ++q)
                {
                    const double f = fLo * std::pow (fHi / fLo, (double) q / P), lo = f / 1.06, hi = f * 1.06;     // 1/6-octave smoothing
                    const int b0 = std::max (1, (int) (lo * N / er::kSR)), b1 = std::max (b0, (int) (hi * N / er::kSR));
                    double e = 0; for (int bb = b0; bb <= b1 && bb < N / 2; ++bb) e += (double) buf[(size_t) bb] * buf[(size_t) bb];
                    sl[(size_t) k][(size_t) q] = (float) (10 * std::log10 (e / (b1 - b0 + 1) + 1e-12));
                    mx = std::max (mx, sl[(size_t) k][(size_t) q]);
                }
            }
            wfTop = mx;
            for (int k = K - 1; k >= 0; --k)
            {
                const float ox = depthX * k / (K - 1), oy = baseH + depthY - depthY * k / (K - 1);
                juce::Path p; p.startNewSubPath (ox, oy);
                for (int q = 0; q <= P; ++q)
                {
                    const double f = fLo * std::pow (fHi / fLo, (double) q / P);
                    const float h = juce::jlimit (0.0f, 1.0f, (sl[(size_t) k][(size_t) q] - (mx - 60)) / 62.0f) * baseH * 0.95f;
                    p.lineTo (ox + xf (f), oy - h);
                }
                p.lineTo (ox + baseW, oy); p.closeSubPath();
                g.setColour (theme::well); g.fillPath (p);
                const float tt = 1.0f - (float) k / (K - 1);
                g.setColour (heat (0.25f + 0.7f * tt).withAlpha (0.95f)); g.strokePath (p, juce::PathStrokeType (k == 0 ? 1.6f : 1.0f));
            }
        }
    }
    juce::Image cache; int cacheKey = -1; float wfTop = 0; double lastS = 400; bool dragTail = false, dragWf = false; juce::Point<double> wfStart;
    std::vector<std::pair<juce::Rectangle<float>, int>> tabR, whoR; bool twoRows = false;
    ParamHelp& ph; juce::Rectangle<float> plot; std::vector<std::pair<juce::Rectangle<float>, int>> spanR;
    juce::Point<float> mouse; bool hover = false;
};

// ===========================================================================
//  Room plan: drag the source (triangle) and the listener (dot)
// ===========================================================================
class RoomView : public juce::Component
{
public:
    RoomView (ParamHelp& h) : ph (h) {}
    int slot = -1;                                    // (0.9.2) -1 = the ER's room; 0 / 1 = room A / B (while its SOURCE card is being worked on)
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const bool narrow = b.getWidth() < 300;            // (0.8.1: the tall pane is a fifth of the monitors: one column of read-outs)
        const juce::String rn = slot < 0 ? juce::String() : (slot == 0 ? " A" : " B");
        panelBox (g, b, narrow ? ph.tr ("部屋", "ROOM") + rn + ph.tr ("（平面図）", " PLAN") : ph.tr ("部屋", "ROOM") + rn + ph.tr ("（平面図）— ▲音源と●リスナーをドラッグ", " PLAN - drag the source and the listener"));
        const auto R = ph.proc.roomParams (slot);
        area = b.reduced (narrow ? 10.0f : 16.0f).withTrimmedTop (22).withTrimmedBottom (narrow ? 108.0f : 78.0f);
        const float sc = (float) std::min (area.getWidth() / R.W, area.getHeight() / R.D);
        room = juce::Rectangle<float> ((float) (R.W * sc), (float) (R.D * sc)).withCentre (area.getCentre());
        const auto& M = er::materials();
        const auto poly = er::roomPoly (R);
        auto P = [&] (double xm, double ym) { return juce::Point<float> (room.getX() + (float) (xm / R.W * room.getWidth()), room.getY() + (float) (ym / R.D * room.getHeight())); };
        {   // the plan (a polygon for the hall / skewed shapes)
            juce::Path pp; for (int i = 0; i < poly.n; ++i) { const auto q = P (poly.x[i], poly.y[i]); if (i == 0) pp.startNewSubPath (q); else pp.lineTo (q); } pp.closeSubPath();
            g.setColour (theme::well); g.fillPath (pp);
            g.setColour (theme::muted); g.strokePath (pp, juce::PathStrokeType (juce::jlimit (1.0f, 5.0f, (float) (1.0 + 8 * (0.6 - M[(size_t) R.wall].alpha)))));
        }
        if (R.shape == 3)
        {   // the dome's rings seen from above (30 / 60 degrees up) and its crown
            g.setColour (theme::muted.withAlpha (0.35f));
            for (double cp : { 0.866, 0.5 }) { const auto c0 = P (R.W * 0.5, R.D * 0.5); g.drawEllipse (c0.x - (float) (cp * room.getWidth() * 0.5), c0.y - (float) (cp * room.getHeight() * 0.5), (float) (cp * room.getWidth()), (float) (cp * room.getHeight()), 1.0f); }
        }
        double sxm, sym, lxm, lym; er::roomPos (R, R.sx, R.sy, sxm, sym); er::roomPos (R, R.lx, R.ly, lxm, lym);
        const auto s = P (sxm, sym), l = P (lxm, lym);
        // first-order wall reflections
        {
            std::vector<er::Img2> w1; er::polyImages (poly, sxm, sym, lxm, lym, 1, w1);
            for (const auto& w : w1)
            {
                const auto hit = P (w.hx, w.hy);
                g.setColour (theme::accent.withAlpha (0.45f));
                g.drawLine ({ s, hit }, 1.0f); g.drawLine ({ hit, l }, 1.0f);
                g.fillEllipse (hit.x - 2, hit.y - 2, 4, 4);
            }
        }
        if (er::legClear (poly, sxm, sym, lxm, lym)) { g.setColour (theme::direct.withAlpha (0.8f)); g.drawLine ({ s, l }, 1.6f); }
        else { const float dash[] = { 4.0f, 4.0f }; g.setColour (theme::muted.withAlpha (0.6f)); g.drawDashedLine ({ s, l }, dash, 2, 1.0f); }   // the direct sound is blocked by a wall
        juce::Path tri; tri.addTriangle (s.x, s.y - 8, s.x - 7, s.y + 6, s.x + 7, s.y + 6);
        g.setColour (theme::direct); g.fillPath (tri);
        g.setColour (theme::accent); g.fillEllipse (l.x - 6, l.y - 6, 12, 12);
        g.setColour (theme::onAccent); g.fillEllipse (l.x - 2, l.y - 2, 4, 4);
        // read-outs
        const double dDir = std::hypot (sxm - lxm, sym - lym);
        double first = 1e9;
        {
            er::TapSet ts; std::vector<er::Img> v; v.reserve (1400); auto r1 = R; r1.order = 1; er::roomTaps (r1, ts, v);
            for (int i = 0; i < ts.n; ++i) first = std::min (first, 0.5 * (double) (ts.t[(size_t) i].tL + ts.t[(size_t) i].tR));
        }
        const auto GM = er::roomGeom (R); const double V = GM.V, S = GM.S;
        auto t = narrow ? b.reduced (10, 8).removeFromBottom (98) : b.reduced (16, 10).removeFromBottom (68);
        g.setFont (theme::mono (11.0f));
        auto row = [&] (const juce::String& k, const juce::String& v, int col, int r)
        {
            if (narrow) { r += col * 3; col = 0; }
            const int cw = narrow ? (int) t.getWidth() : (int) (t.getWidth() / 2), kw = narrow ? (int) (t.getWidth() * 0.52f) : juce::jlimit (60, 110, cw - 96);
            const int x = (int) t.getX() + col * cw, y = (int) t.getY() + r * 16;
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawFittedText (k, x, y, kw - 4, 15, juce::Justification::centredLeft, 1, 0.75f);
            g.setColour (theme::text); g.setFont (theme::mono (10.5f, true)); g.drawFittedText (v, x + kw, y, cw - kw - 4, 15, juce::Justification::centredLeft, 1, 0.7f);
        };
        row (ph.tr ("サイズ", "Size"), juce::String (R.shape == 1 ? "Hall " : R.shape == 2 ? "Skew " : R.shape == 3 ? "Dome " : R.shape == 4 ? "Conn " : R.shape == 5 ? "Corr " : "") + juce::String (R.W, 1) + "x" + juce::String (R.D, 1) + "x" + juce::String (R.H, 1), 0, 0);
        row (ph.tr ("直接音の距離", "Direct path"), juce::String (dDir, 2) + " m", 0, 1);
        row (ph.tr ("初回反射（ITDG）", "First (ITDG)"), first < 1e8 ? fmtMs (first) : "-", 0, 2);
        row (ph.tr ("平均自由行程", "Mean free path"), juce::String (4 * V / S, 2) + " m", 1, 0);
        row (ph.tr ("RT60（Sabine）", "RT60 (Sabine)"), juce::String (er::roomRT60 (R), 2) + " s", 1, 1);
        row (ph.tr ("容積", "Volume"), juce::String ((int) std::round (V)) + " m3", 1, 2);
        juce::ignoreUnused (S);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto R = ph.proc.roomParams (slot);
        auto P = [&] (double fx, double fy) { double xm, ym; er::roomPos (R, fx, fy, xm, ym); return juce::Point<float> (room.getX() + (float) (xm / R.W * room.getWidth()), room.getY() + (float) (ym / R.D * room.getHeight())); };
        const float ds = e.position.getDistanceFrom (P (R.sx, R.sy)), dl = e.position.getDistanceFrom (P (R.lx, R.ly));
        dragR = R;
        drag = std::min (ds, dl) > 18 ? 0 : (ds < dl ? 1 : 2);
        if (drag == 0) return;
        ph.proc.undo.beginNewTransaction();
        for (auto id : ids()) if (auto* p = ph.get (id)) p->beginChangeGesture();
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == 0 || room.isEmpty()) return;
        const float y = juce::jlimit (0.05f, 0.95f, (e.position.y - room.getY()) / room.getHeight());
        const double xm = (e.position.x - room.getX()) / room.getWidth() * dragR.W; double xl, xr; er::roomChord (dragR, y, xl, xr);
        const float x = juce::jlimit (0.05f, 0.95f, (float) ((xm - xl) / std::max (0.01, xr - xl)));
        ph.set (drag == 1 ? "srcX" : "lisX", x); ph.set (drag == 1 ? "srcY" : "lisY", y);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == 0) return;
        for (auto id : ids()) if (auto* p = ph.get (id)) p->endChangeGesture();
        drag = 0;
    }
private:
    static std::array<const char*, 4> ids() { return { "srcX", "srcY", "lisX", "lisY" }; }
    ParamHelp& ph; juce::Rectangle<float> area, room; int drag = 0; er::RoomP dragR;
};

// ===========================================================================
//  Info panel: ALGO (model text + "what other reverbs call it") / IR (analysis)
// ===========================================================================
class InfoPanel : public juce::Component
{
public:
    int focusIr = -1;                                 // (0.9.2) the IR card being worked on in SOURCE (0 A / 1 B), else -1
    InfoPanel (ParamHelp& h) : ph (h)
    {
        for (auto* b : { &fitBtn, &clearBtn, &loadBtn, &neutralBtn, &roomBtn }) { addAndMakeVisible (*b); b->getProperties().set ("fontPx", 11.5); }
        roomBtn.onClick = [this] { sendMenu (false); };
        neutralBtn.onClick = [this] { ph.proc.resetVoiceColor(); };
        fitBtn.onClick = [this] { sendMenu (true); };
        clearBtn.onClick = [this] { ph.proc.clearIR(); };
    }
    const Preview* prev = nullptr; std::shared_ptr<const er::Target> tgt; bool compare = true;
    bool forceIR = false;                         // the "IR analysis" monitor: always the IR, whatever the ER plays
    std::function<void (const juce::String&, const juce::String&)> onFlash;
    // the analysed IR to the ER and / or to the tail, each as the IR itself, an algorithm or a room
    void sendMenu (bool toER)
    {
        juce::PopupMenu m;
        if (toER)
        {
            m.addSectionHeader (ph.tr ("ER へ送る", "Send to the ER"));
            m.addItem (1, ph.tr ("IR のまま（ER＝IR）", "As the IR (ER = IR)"));
            m.addItem (2, ph.tr ("アルゴリズムで再現（ALGO にフィット）", "Rebuild with the algorithm (fit to ALGO)"), tgt && tgt->fitted);
            m.addItem (3, ph.tr ("部屋で再現（ROOM：鏡像法で部屋を推定）", "Rebuild as a room (ROOM: image-source fit)"));
        }
        else
        {
            m.addSectionHeader (ph.tr ("TAIL へ送る（この IR を使う）", "Send to the tail (this IR)"));
            m.addItem (11, ph.tr ("IR のまま（TAIL＝IR）", "As the IR (TAIL = IR)"));
            m.addItem (12, ph.tr ("アルゴリズムで再現（ALGO：帯域ごとの減衰をフィット）", "Rebuild with the algorithm (ALGO: decay per band fitted)"));
            m.addItem (13, ph.tr ("部屋で再現（ROOM：残響時間から部屋を推定）", "Rebuild as a room (ROOM: room fitted to the reverb time)"));
        }
        juce::Component::SafePointer<InfoPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (toER ? &fitBtn : &roomBtn), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& P = safe->ph.proc;
            auto flash = [safe] (const juce::String& t, const juce::String& b) { if (safe->onFlash) safe->onFlash (t, b); };
            if (r == 1) { P.undo.beginNewTransaction(); P.setParam ("src", 2); flash ("IR -> ER", safe->ph.tr ("ER は IR をそのまま鳴らします。", "The ER plays the IR as it is.")); }
            else if (r == 2) { P.startFit(); flash ("IR -> ER ALGO", safe->ph.tr ("IR の ER を ALGO の 9 つのパラメーターで描き直しました。", "The IR's ER redrawn with ALGO's nine parameters.")); }
            else if (r == 3) { P.irToRoom(); flash ("IR -> ER ROOM", safe->ph.tr ("IR の ER に近い部屋を探しています（数秒）。終わると ROOM に切り替わります。", "Searching for the room closest to the IR's ER (a few seconds); then ROOM.")); }
            else
            {
                // the tail takes this IR (not a separately loaded tail IR)
                if (P.tailIrOwn()) P.clearTailIR();
                if (r == 11) { P.undo.beginNewTransaction(); P.setParam ("tSrc", 1); flash ("IR -> TAIL", safe->ph.tr ("テールは IR をそのまま鳴らします（音量は ALGO と同じ大きさにそろえます）。", "The tail plays the IR as it is (matched to the ALGO tail's level).")); }
                else
                {
                    // the IR tail is built in the background: wait for it (up to ~2 s), then fit
                    P.setParam ("tSrc", 1);
                    const bool room = r == 13;
                    auto tryFit = std::make_shared<std::function<void (int)>>();
                    *tryFit = [safe, room, flash, tryFit] (int left)
                    {
                        if (safe == nullptr) return;
                        auto& Q = safe->ph.proc;
                        if (! Q.tailIrReady.load() && left > 0) { juce::Timer::callAfterDelay (100, [tryFit, left] { (*tryFit) (left - 1); }); return; }
                        const auto rep = room ? Q.irToTailRoom() : Q.irToFdn();
                        if (room) flash ("IR -> TAIL ROOM", rep.isEmpty() ? safe->ph.tr ("IR の残響時間が測れませんでした。", "Could not measure the IR's reverb time.")
                                                                     : safe->ph.tr ("残響時間に合う部屋：", "The room that fits the reverb time: ") + rep + safe->ph.tr ("。材質で届かない分は RT の点（倍率）に入れました。ER が ROOM のときは ER も同じ部屋になります。", ". What the materials can't reach is in the RT points (trim). With a ROOM ER, the ER is in the same room."));
                        else flash ("IR -> TAIL ALGO", safe->ph.tr ("IR の帯域ごとの減衰を ALGO に写しました：", "The IR's decay per band copied to ALGO: ") + rep);
                    };
                    (*tryFit) (25);
                }
            }
        });
    }
    juce::TextButton fitBtn, clearBtn, loadBtn, neutralBtn, roomBtn;

    void resized() override
    {
        auto all = getLocalBounds().reduced (12);
        auto r = all.removeFromBottom (28);
        all.removeFromBottom (6);
        neutralBtn.setBounds (all.removeFromBottom (24).removeFromRight (230));
        loadBtn.setBounds (r.removeFromLeft (96)); r.removeFromLeft (5);
        clearBtn.setBounds (r.removeFromLeft (52)); r.removeFromLeft (5);
        fitBtn.setBounds (r.removeFromLeft (108)); r.removeFromLeft (5);
        roomBtn.setBounds (r.removeFromLeft (108));
    }
    void refreshButtons()
    {
        clearBtn.setButtonText (ph.tr ("クリア", "Clear"));
        fitBtn.setButtonText (ph.tr ("ER へ ▾", "To ER ▾"));
        roomBtn.setButtonText (ph.proc.converting.load() == 3 ? ph.tr ("推定中…", "Fitting...") : ph.tr ("TAIL へ ▾", "To TAIL ▾"));
        loadBtn.setButtonText (ph.tr ("IR読込…", "Load IR..."));
        neutralBtn.setButtonText (ph.tr ("VOICE・COLOR を初期値に", "Reset VOICE / COLOR"));
        neutralBtn.setVisible ((int) ph.value ("src") == 2 && tgt != nullptr && ! ph.proc.nonNeutralVoiceColor().isEmpty());
        setHelp (neutralBtn, "Reset VOICE / COLOR", "VOICE と COLOR（Predelay・Size・Width・Diffusion・フィルター・ER Level・Bandwidth・Bits・Wander）を初期値に戻します。IR の ER をそのままの形で聴き、比べるときに使います（Undo で戻せます）。",
                 "Puts VOICE and COLOR (Predelay, Size, Width, Diffusion, filters, ER Level, Bandwidth, Bits, Wander) back to their defaults, to hear and compare the IR's ER as it is (undoable).");
        const bool ir = (int) ph.value ("src") == 2 || forceIR;
        loadBtn.setVisible (ir); clearBtn.setVisible (ir && tgt != nullptr);
        fitBtn.setVisible (ir && tgt != nullptr && tgt->valid);
        roomBtn.setVisible (ir && tgt != nullptr && tgt->valid);
        setHelp (fitBtn, "To ER", "解析した IR を ER へ：IR のまま／アルゴリズム（ALGO）で再現／部屋（ROOM）で再現。", "The analysed IR to the ER: as the IR / rebuilt with the algorithm (ALGO) / rebuilt as a room (ROOM).");
        setHelp (roomBtn, "To TAIL", "解析した IR をテールへ：IR のまま／アルゴリズム（ALGO）で再現／部屋（ROOM）で再現（残響時間に合う部屋の大きさと材質）。ER と TAIL は別々に選べます。", "The analysed IR to the tail: as the IR / rebuilt with the algorithm (ALGO) / rebuilt as a room (ROOM: the size and materials that fit the reverb time). ER and TAIL are chosen separately.");
        if ((false)) setHelp (roomBtn, "Fit -> ROOM", "IR の ER にいちばん近い直方体の部屋（寸法・音源とリスナーの位置・壁と床の材質）を鏡像法で探し、ROOM に切り替えます（数秒）。左右・前後を入れ替えた部屋は同じ反射になるので、答えはそのうちの一つです。プレートや実機の IR は部屋の形がないので一致度が低く出ます。Undo で戻せます。",
                 "Searches for the shoebox room (size, source / listener positions, wall and floor materials) whose image-source ER is closest to the IR's, then switches to ROOM (a few seconds). Mirrored rooms give the same reflections, so the answer is one of them. Plate or hardware IRs have no room shape and score low. Undo goes back.");
        if ((false)) setHelp (fitBtn, "Fit -> ALGO", "解析合成フィットで求めた9つのパラメーターを ALGO に書き込み、ALGO に切り替えます。IRの響きを「編集できるパラメーター」に翻訳する操作です。",
                 "Writes the nine parameters found by the analysis-by-synthesis fit into ALGO and switches to it: the IR translated into editable parameters.");
        setHelp (loadBtn, "Load IR", "WAV / AIFF / FLAC の IR を読み込みます。直接音を含む部屋の IR が基本です。リバーブ機の戻りだけの IR（直接音なし）は Wet only をオンに。",
                 "Loads a WAV / AIFF / FLAC impulse response. A room IR with its direct sound is the normal case; for a reverb return with no direct sound turn on Wet only.");
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const int src = forceIR ? 2 : (int) ph.value ("src");
        panelBox (g, b, src == 2 ? ph.tr ("IR 解析", "IR ANALYSIS") + (forceIR ? juce::String (ph.proc.erIrSlot() == 0 ? "  (IR A)" : "  (IR B)") : juce::String()) : ph.tr ("パターン / モデル", "ALGO / MODEL"));
        auto r = getLocalBounds().reduced (14).withTrimmedTop (22);
        if (src == 2 && focusIr >= 0 && focusIr != ph.proc.erIrSlot())
        {   // an IR card is being worked on, but the analysis is the ER's IR
            g.setColour (theme::warn); g.setFont (theme::ui (10.0f, true));
            const juce::String nm (focusIr == 0 ? "IR A" : "IR B");
            g.drawFittedText (ph.ja() ? nm + juce::String::fromUTF8 (" は ER に送ると解析されます（いまは ER の IR を表示）") : nm + " is analysed once it feeds the ER (showing the ER's IR)", r.removeFromTop (28), juce::Justification::topLeft, 2, 0.8f);
        }
        if (src == 2) paintIR (g, r); else paintPattern (g, r);
    }
private:
    void text (juce::Graphics& g, juce::Rectangle<int>& r, const juce::String& s, float px, juce::Colour c, int lines, bool bold = false)
    {
        g.setColour (c); g.setFont (theme::ui (px, bold));
        juce::AttributedString as; as.append (s, theme::ui (px, bold), c); as.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout tl; tl.createLayout (as, (float) r.getWidth());
        const int h = std::min ({ (int) std::ceil (tl.getHeight()), (int) (lines * px * 1.45f), r.getHeight() });
        if (r.getHeight() < (int) (px * 1.2f) || h <= 0) { r.setHeight (0); return; }    // no room left: stop rather than overlap
        g.saveState(); g.reduceClipRegion (r.withHeight (h));
        tl.draw (g, r.removeFromTop (h).toFloat()); g.restoreState(); r.removeFromTop (std::min (6, r.getHeight()));
    }
    void breakdown (juce::Graphics& g, juce::Rectangle<int>& r)
    {
        if (prev == nullptr || ! prev->hasFeat) return;
        const auto& d = prev->d;
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true));
        if (r.getHeight() < 16 + 14 * 3) return;
        g.drawText (ph.tr ("ターゲットとのずれ（1 = はっきり違う）", "Distance to target (1 = clearly different)"), r.removeFromTop (16), juce::Justification::centredLeft);
        const std::pair<const char*, double> items[] = { { "First", d.first }, { "Envelope", d.env }, { "Level", d.level }, { "Density", d.dens }, { "Tone", d.spec }, { "Width", d.width }, { "Shape", d.shape } };
        for (auto& [k, v] : items)
        {
            if (r.getHeight() < 14) return;
            auto row = r.removeFromTop (14);
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawText (k, row.removeFromLeft (64), juce::Justification::centredLeft);
            auto bar = row.removeFromLeft (row.getWidth() - 40).toFloat().reduced (0, 4);
            g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (bar, 2.0f);
            const auto c = v < 0.5 ? theme::ok : v < 1.0 ? theme::direct : theme::warn;
            g.setColour (c); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.02, 1.0, v / 2)), 2.0f);
            g.setColour (theme::text); g.setFont (theme::mono (10.0f)); g.drawText (juce::String (v, 2), row, juce::Justification::centredRight);
        }
        r.removeFromTop (4);
        if (tgt)
        {
            auto hs = diagnose (tgt->F, prev->feat, ph.ja());
            for (size_t i = 0; i < std::min<size_t> (2, hs.size()); ++i) text (g, r, juce::String::fromUTF8 ("\xe2\x80\xa2 ") + hs[i].text, 11.0f, theme::text, 2);
            if (hs.empty()) text (g, r, ph.tr ("ほぼ一致しています。最後は耳で確認を。", "Close match. Confirm by ear."), 11.0f, theme::ok, 1);
        }
    }
    void paintPattern (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const int pi = ph.proc.currentPreset;
        const auto& P = presets();
        if (pi >= 0 && pi < (int) P.size() && P[(size_t) pi].group == 1)
        {
            text (g, r, juce::String::fromUTF8 (P[(size_t) pi].name), 14.0f, theme::text, 1, true);
            text (g, r, juce::String::fromUTF8 (ph.ja() ? P[(size_t) pi].ja : P[(size_t) pi].en), 11.5f, theme::muted, 5);
        }
        else text (g, r, ph.tr ("ALGO は 9 つのパラメーターで ER を描くエンジンです。プリセットのビンテージ機モデルもこのエンジンと COLOR で作っています。",
                                "ALGO draws the ER with nine parameters. The vintage models in the presets are this engine plus COLOR."), 11.5f, theme::muted, 4);
        if (compare && tgt && tgt->valid && prev && prev->hasFeat) { breakdown (g, r); return; }
        // what other reverbs call it
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true));
        g.drawText (ph.tr ("ほかのリバーブでの呼び名（目安）", "What other reverbs call it (rough guide)"), r.removeFromTop (16), juce::Justification::centredLeft);
        const char* rows[][3] = {
            { "Predelay", "Predelay", "Predelay" }, { "First", "ER Time / Size", "ER Time / Size" }, { "Length", "ER Size / Spread", "ER Size / Spread" },
            { "Reflections", "Density", "Density" }, { "Growth", "Shape / Distribution", "Shape / Distribution" }, { "Envelope", "Shape / ER Decay / Slope", "Shape / ER Decay / Slope" },
            { "Damping", "ER High Cut / Damping", "ER High Cut / Damping" }, { "Diffusion", "Diffusion", "Diffusion" }, { "Width", "Spread / Stereo", "Spread / Stereo" } };
        for (auto& rw : rows)
        {
            auto row = r.removeFromTop (15); if (row.getBottom() > getHeight() - 14) break;
            g.setColour (theme::text); g.setFont (theme::mono (10.5f, true)); g.drawText (rw[0], row.removeFromLeft (96), juce::Justification::centredLeft);
            g.setColour (theme::muted); g.setFont (theme::ui (10.5f)); g.drawText (rw[1], row, juce::Justification::centredLeft);
        }
    }
    void paintIR (juce::Graphics& g, juce::Rectangle<int> r)
    {
        r.removeFromBottom (34);
        if (neutralBtn.isVisible()) r.removeFromBottom (30);
        if (ph.proc.analysing.load()) { text (g, r, ph.tr ("解析中…", "Analysing..."), 13.0f, theme::accent, 1, true); }
        if (! tgt || ! tgt->valid)
        {
            if (! ph.proc.analysing.load())
                text (g, r, ph.tr ("IRを読み込むと、直接音・初期反射・混合時間を検出し、反射をタップに変換します。\n\n"
                                   "・部屋のIR（直接音あり）：そのまま\n・リバーブ機の戻りのIR（直接音なし）：Wet only をオン\n\n"
                                   "Taps = 拾った反射を鳴らす（Size・Width などで編集可）\nConv = ERをそのまま鳴らす答え合わせ用\nHybrid = IR の反射の量を前半・後半で別々に合わせ、後半は隙間を補う",
                                   "Load an IR: the direct sound, early reflections and mixing time are detected and the reflections become taps.\n\n"
                                   "- Room IR (with direct sound): as is\n- Reverb return (no direct sound): turn on Wet only\n\n"
                                   "Taps = plays the picked reflections (editable with Size, Width...)\nConv = plays the ER as it is, for reference\nHybrid = IR reflections, early and later parts levelled separately, gaps filled later"),
                      11.5f, theme::muted, 12);
            return;
        }
        {
            const bool sb = ph.proc.tgtSlot == 1; const auto& nm0 = sb ? ph.proc.tIrName : ph.proc.irName;
            juce::String nm = nm0.isNotEmpty() ? nm0 : juce::String ("IR");
            if (sb && ph.proc.irListB.size() > 1) nm = juce::String (ph.proc.irListIndexB + 1) + " / " + juce::String (ph.proc.irListB.size()) + "   " + nm;
            else if (! sb && ph.proc.irList.size() > 1) nm = juce::String (ph.proc.irListIndex + 1) + " / " + juce::String (ph.proc.irList.size()) + "   " + nm;
            text (g, r, nm, 13.0f, theme::text, 1, true);
            if (! compare) text (g, r, ph.tr ("比較 OFF：一致度とずれは表示しません。", "Compare OFF: no match score or distances."), 10.5f, theme::muted, 1);
        }
        const auto& F = tgt->F;
        auto kv = [&] (const juce::String& k, const juce::String& v, int col, int row)
        {
            const int cw = r.getWidth() / 2, x = r.getX() + col * cw, y = r.getY() + row * 15, kw = std::min (96, cw * 11 / 20);     // (narrow panes: the key gives way)
            g.setColour (theme::muted); g.setFont (theme::ui (10.5f)); g.drawFittedText (k, x, y, kw - 2, 14, juce::Justification::centredLeft, 1, 0.6f);
            g.setColour (theme::text); g.setFont (theme::mono (10.5f, true)); g.drawFittedText (v, x + kw, y, cw - kw - 2, 14, juce::Justification::centredLeft, 1, 0.6f);
        };
        kv (ph.tr ("初回反射 T1", "First T1"), fmtMs (F.T1), 0, 0);
        kv (ph.tr ("ER窓", "ER window"), fmtMs (tgt->Wc), 1, 0);
        kv (ph.tr ("混合時間", "Mixing time"), tgt->mix > 0 ? fmtMs (tgt->mix) : "-", 0, 1);
        kv ("ER / Direct", fmtDb (F.erDb), 1, 1);
        kv ("C50", fmtDb (tgt->c50), 0, 2);
        kv ("IACC early", juce::String (F.iacc, 2), 1, 2);
        kv (ph.tr ("拾った反射", "Picked"), juce::String (tgt->taps.n), 0, 3);
        kv (ph.tr ("フィット一致度", "Fit score"), tgt->fitted ? juce::String (tgt->fitD.score) : "-", 1, 3);
        r.removeFromTop (66);
        if (const auto nn = ph.proc.nonNeutralVoiceColor(); ! nn.isEmpty())
        {
            text (g, r, ph.tr ("VOICE・COLOR が効いています（IR の ER とはこの分だけ違う音になり、一致度も下がります）：", "VOICE / COLOR are active (the sound differs from the IR's ER by this much, and so does the match): ")
                        + nn.joinIntoString (ph.ja() ? "、" : ", "), 10.5f, theme::direct, 3);
        }
        else text (g, r, ph.tr ("VOICE・COLOR は初期値：IR の ER をそのままの形で鳴らしています。", "VOICE / COLOR at their defaults: the IR's ER plays as it is."), 10.5f, theme::ok, 1);
        if (const float c = ph.proc.autoComp.load(); std::abs (c) > 0.05f && ph.value ("autoLvl") > 0.5f)
            text (g, r, ph.tr ("Auto Level の ", "Auto Level ") + fmtDb (c) + ph.tr (" は一致度の計算から除いています。", " is left out of the match."), 10.5f, theme::muted, 1);
        const juce::String dk = "Darkening " + juce::String (tgt->ir.darken, 2) + " oct/100 ms";
        if (! (tgt->fitted && ph.proc.nonNeutralVoiceColor().isEmpty()))
            text (g, r, ph.tr ("推定：", "Estimate: ") + dk + ph.tr ("（この値で IR どおり）", " (as the IR at this value)"), 10.5f, theme::muted, 1);
        if (tgt->fitted && ph.proc.nonNeutralVoiceColor().isEmpty())
        {
            const auto& f = tgt->fit;
            text (g, r, ph.tr ("推定：", "Fit: ") + "First " + fmtMs (f.p.first) + " / Length " + fmtMs (f.p.length) + " / Refl " + juce::String ((int) f.p.taps)
                        + " / Growth " + juce::String (f.p.growth, 2) + " / Energy " + fmtDb (f.p.energy) + " / Env " + fmtDb (f.p.decay)
                        + " / Damp " + fmtHz (f.p.damp) + " / Width " + juce::String ((int) std::round (f.width * 100)) + "% / " + dk, 10.5f, theme::muted, 3);
        }
        breakdown (g, r);
    }
    ParamHelp& ph;
};

// ===========================================================================
//  Lesson card
// ===========================================================================
class LessonCard : public juce::Component
{
public:
    LessonCard (ParamHelp& h) : ph (h)
    {
        for (auto* b : { &prevB, &nextB, &loadB, &earB }) { addAndMakeVisible (*b); b->getProperties().set ("fontPx", 12.0); }
        earB.onClick = [this] { if (onEar) onEar(); };
        prevB.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80")); nextB.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
        prevB.onClick = [this] { step (-1); }; nextB.onClick = [this] { step (1); };
        loadB.onClick = [this] { ph.proc.applyLesson (index()); };
    }
    int index() const { return juce::jlimit (0, (int) lessons().size() - 1, (int) ph.proc.uiState.getProperty ("lessonView", 0)); }
    void step (int d) { ph.proc.uiState.setProperty ("lessonView", juce::jlimit (0, (int) lessons().size() - 1, index() + d), nullptr); repaint(); }
    bool narrow() const { return getWidth() < 520; }
    void resized() override
    {
        if (narrow())
        {   // the notes column: ◀ ▶ at the top, the two buttons at the bottom
            auto r = getLocalBounds().reduced (10, 8);
            auto top = r.removeFromTop (24);
            nextB.setBounds (top.removeFromRight (28)); top.removeFromRight (4); prevB.setBounds (top.removeFromRight (28));
            earB.setBounds (r.removeFromBottom (26)); r.removeFromBottom (6); loadB.setBounds (r.removeFromBottom (26));
            return;
        }
        auto r = getLocalBounds().reduced (10, 8).removeFromTop (24);
        nextB.setBounds (r.removeFromRight (28)); r.removeFromRight (4); prevB.setBounds (r.removeFromRight (28)); r.removeFromRight (8);
        loadB.setBounds (r.removeFromRight (190)); r.removeFromRight (8);
        earB.setBounds (r.removeFromRight (110));
    }
    std::function<void()> onEar;
    void paint (juce::Graphics& g) override
    {
        loadB.setButtonText (ph.tr ("このレッスンの設定を読み込む", "Load this lesson's settings"));
        earB.setButtonText (ph.tr ("耳トレへ", "Ear training"));
        auto b = getLocalBounds().toFloat();
        panelBox (g, b);
        const auto& L = lessons()[(size_t) index()];
        auto r = getLocalBounds().reduced (14, 8);
        auto top = r.removeFromTop (24);
        g.setColour (theme::accent); g.setFont (theme::ui (11.0f, true));
        g.drawText ("LESSON " + juce::String (index() + 1) + " / " + juce::String ((int) lessons().size()), top.removeFromLeft (110), juce::Justification::centredLeft);
        g.setColour (theme::text); g.setFont (theme::ui (13.5f, true));
        if (narrow())
        {
            auto t = r.removeFromTop (40);
            g.drawFittedText (juce::String::fromUTF8 (ph.ja() ? L.titleJ : L.titleE), t, juce::Justification::topLeft, 2, 0.9f);
            r.removeFromBottom (64);
        }
        else g.drawText (juce::String::fromUTF8 (ph.ja() ? L.titleJ : L.titleE), top.withTrimmedRight (360), juce::Justification::centredLeft);
        r.removeFromTop (4);
        juce::AttributedString as; as.append (juce::String::fromUTF8 (ph.ja() ? L.ja : L.en), theme::ui (14.0f), theme::text);
        as.setWordWrap (juce::AttributedString::byWord); as.setLineSpacing (3.0f);
        juce::TextLayout tl; tl.createLayout (as, (float) r.getWidth()); tl.draw (g, r.toFloat());
    }
private:
    ParamHelp& ph; juce::TextButton prevB, nextB, loadB, earB;
};

// ===========================================================================
//  Ear training: one setting changes from a reference; which value is it?
//  The current sound is the base. The settings are put back when it ends.
// ===========================================================================
struct EarQ { const char* id; const char* ja; const char* en; float ref; float ch[4]; int kind; };   // kind: 0 ms, 1 %, 2 oct, 3 Hz, 4 bit
inline const std::vector<EarQ>& earQuestions()
{
    static const std::vector<EarQ> q {
        { "predelay",  "Predelay はどれ？",            "Which Predelay?",          0,     { 5, 15, 30, 60 }, 0 },
        { "size",      "Size はどれ？",                "Which Size?",              100,   { 60, 80, 130, 180 }, 1 },
        { "darken",    "Darkening はどれ？",           "Which Darkening?",         0,     { 0.5f, 1.5f, 3, 5 }, 2 },
        { "diffusion", "Diffusion はどれ？",           "Which Diffusion?",         0,     { 20, 45, 70, 100 }, 1 },
        { "width",     "Width はどれ？",               "Which Width?",             100,   { 0, 40, 70, 150 }, 1 },
        { "wander",    "Wander はどれ？",              "Which Wander?",            0,     { 15, 35, 60, 100 }, 1 },
        { "highcut",   "High Cut はどれ？",            "Which High Cut?",          20000, { 2500, 5000, 8000, 12000 }, 3 },
        { "bits",      "Bits はどれ？",                "Which Bits?",              24,    { 8, 10, 12, 16 }, 4 } };
    return q;
}
inline juce::String earFmt (float v, int kind)
{
    switch (kind)
    {
        case 0: return juce::String ((int) v) + " ms";
        case 1: return juce::String ((int) v) + " %";
        case 2: return juce::String (v, 1) + " oct";
        case 3: return v >= 1000 ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String ((int) v) + " Hz";
        default: return juce::String ((int) v) + " bit";
    }
}
class EarCard : public juce::Component
{
public:
    EarCard (ParamHelp& h) : ph (h)
    {
        for (auto* b : { &refB, &qB, &nextB, &endB, &ch[0], &ch[1], &ch[2], &ch[3] }) { addAndMakeVisible (*b); b->getProperties().set ("fontPx", 12.0); }
        refB.setClickingTogglesState (false); qB.setClickingTogglesState (false);
        refB.onClick = [this] { listen (false); }; qB.onClick = [this] { listen (true); };
        nextB.onClick = [this] { newQuestion(); };
        endB.onClick = [this] { stop(); };
        for (int i = 0; i < 4; ++i) ch[i].onClick = [this, i] { answer (i); };
    }
    std::function<void()> onEnd;
    void start() { ph.proc.earBegin(); correct = total = 0; newQuestion(); }
    void stop() { ph.proc.earEnd(); if (onEnd) onEnd(); }
    void newQuestion()
    {
        const auto& Q = earQuestions();
        int k; do k = rnd.nextInt ((int) Q.size()); while (k == qi && Q.size() > 1);
        qi = k; ans = rnd.nextInt (4); picked = -1;
        // the base is the sound before the training, with this one setting at its reference
        restoreBase();
        listen (true);
        repaint();
    }
    void listen (bool question)
    {
        const auto& q = earQuestions()[(size_t) qi];
        hearingQ = question;
        ph.proc.requestTransition (false);
        ph.setGesture (q.id, question ? q.ch[ans] : q.ref);
        refB.setToggleState (! question, juce::dontSendNotification); qB.setToggleState (question, juce::dontSendNotification);
        repaint();
    }
    void answer (int i)
    {
        if (picked >= 0) return;
        picked = i; ++total; if (i == ans) ++correct;
        repaint();
    }
    bool narrow() const { return getWidth() < 520; }
    void resized() override
    {
        if (narrow())
        {   // the notes column: title, question, listen buttons, the four answers (2 x 2), next / end at the bottom
            auto r = getLocalBounds().reduced (10, 8);
            r.removeFromTop (24 + 52);
            auto l = r.removeFromTop (28); const int hw = (l.getWidth() - 6) / 2;
            refB.setBounds (l.removeFromLeft (hw)); l.removeFromLeft (6); qB.setBounds (l);
            r.removeFromTop (10);
            for (int i = 0; i < 4; ++i) ch[i].setBounds (r.getX() + (i % 2) * (hw + 6), r.getY() + (i / 2) * 34, hw, 28);
            auto bot = r.removeFromBottom (26);
            endB.setBounds (bot.removeFromRight (hw)); bot.removeFromRight (6); nextB.setBounds (bot);
            return;
        }
        auto r = getLocalBounds().reduced (10, 8);
        auto top = r.removeFromTop (24);
        endB.setBounds (top.removeFromRight (90)); top.removeFromRight (8);
        nextB.setBounds (top.removeFromRight (110)); top.removeFromRight (16);
        qB.setBounds (top.removeFromRight (110)); top.removeFromRight (4);
        refB.setBounds (top.removeFromRight (110));
        r.removeFromTop (8);
        auto row = r.removeFromTop (30); row.removeFromLeft (380);
        for (int i = 0; i < 4; ++i) { ch[i].setBounds (row.removeFromLeft (150)); row.removeFromLeft (8); }
    }
    void paint (juce::Graphics& g) override
    {
        refB.setButtonText (ph.tr ("基準を聞く", "Reference")); qB.setButtonText (ph.tr ("問題を聞く", "Question"));
        nextB.setButtonText (ph.tr ("次の問題", "Next")); endB.setButtonText (ph.tr ("終了", "End"));
        const auto& q = earQuestions()[(size_t) qi];
        for (int i = 0; i < 4; ++i)
        {
            ch[i].setButtonText (earFmt (q.ch[i], q.kind));
            ch[i].setColour (juce::TextButton::buttonColourId, picked < 0 ? theme::well : i == ans ? theme::ok.withAlpha (0.55f) : i == picked ? theme::direct.withAlpha (0.55f) : theme::well);
        }
        panelBox (g, getLocalBounds().toFloat());
        auto r = getLocalBounds().reduced (14, 8);
        auto top = r.removeFromTop (24);
        g.setColour (theme::accent); g.setFont (theme::ui (11.0f, true));
        g.drawText (ph.tr ("耳トレ", "EAR TRAINING") + "   " + ph.tr ("正解 ", "Correct ") + juce::String (correct) + " / " + juce::String (total),
                    top.removeFromLeft (220), juce::Justification::centredLeft);
        g.setColour (theme::text); g.setFont (theme::ui (13.5f, true));
        const auto qt = juce::String::fromUTF8 (ph.ja() ? q.ja : q.en) + ph.tr ("（基準 = ", "  (reference = ") + earFmt (q.ref, q.kind) + ph.tr ("）", ")");
        juce::Rectangle<int> row;
        if (narrow())
        {
            g.drawFittedText (qt, r.removeFromTop (48), juce::Justification::topLeft, 2, 0.9f);
            row = r.withTrimmedTop (28 + 10 + 70).removeFromTop (60);
        }
        else
        {
            g.drawText (qt, top.withTrimmedRight (470), juce::Justification::centredLeft);
            r.removeFromTop (8);
            row = r.removeFromTop (30).removeFromLeft (370);
        }
        g.setFont (theme::ui (12.0f));
        if (picked < 0)
        {
            g.setColour (theme::muted);
            g.drawFittedText (ph.tr (hearingQ ? "いま：問題の音。基準と聞き比べて、右から選んでください。" : "いま：基準の音。",
                                     hearingQ ? "Now: the question. Compare with the reference and pick on the right." : "Now: the reference."), row, juce::Justification::centredLeft, 2);
        }
        else
        {
            g.setColour (picked == ans ? theme::ok : theme::direct);
            g.drawFittedText ((picked == ans ? ph.tr ("正解！ ", "Correct! ") : ph.tr ("ちがいます。正解は ", "Not quite: it was "))
                                  + earFmt (q.ch[ans], q.kind) + ph.tr ("。「次の問題」へ。", ". On to Next."), row, juce::Justification::centredLeft, 2);
        }
    }
    int questionIndex() const { return qi; }
    int answerIndex() const { return ans; }
private:
    void restoreBase()
    {
        // every training setting back to the value from before the training (only the asked one changes)
        auto snap = ph.proc.earSnap;
        for (auto& Q : earQuestions())
            for (int i = 0; i < snap.getNumChildren(); ++i)
                if (snap.getChild (i).getProperty ("id").toString() == Q.id) ph.set (Q.id, (float) snap.getChild (i).getProperty ("value"));
    }
    ParamHelp& ph;
    juce::TextButton refB, qB, nextB, endB, ch[4];
    juce::Random rnd;
    int qi = 0, ans = 0, picked = -1, correct = 0, total = 0; bool hearingQ = true;
};

// covers the settings while the ear training runs
class EarMask : public juce::Component
{
public:
    explicit EarMask (ParamHelp& h) : ph (h) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::bg);
        g.setColour (theme::muted); g.setFont (theme::ui (15.0f, true));
        g.drawFittedText (ph.tr ("耳トレ中：設定と表示は隠しています。\n「終了」で耳トレ前の設定に戻ります。",
                                 "Ear training: the settings and displays are hidden.\n\"End\" puts the settings back as they were."),
                          getLocalBounds(), juce::Justification::centred, 3);
    }
private:
    ParamHelp& ph;
};

// ===========================================================================
//  Help card: what the control under the mouse does
// ===========================================================================
class HelpCard : public juce::Component
{
public:
    HelpCard (ParamHelp& h) : ph (h) {}
    juce::String title, body;
    juce::String text() const
    {
        return body.isNotEmpty() ? body
            : ph.tr ("項目にマウスを乗せると説明が出ます。\nドラッグ上下で変更（Shift = 微調整）、ダブルクリックで数値入力、Option+クリックで初期値。右クリックでロック。",
                     "Hover over a control for its description.\nDrag up / down to change (Shift = fine), double-click to type, Option+click for the default. Right-click to lock.");
    }
    // (0.10.12) one sentence per line, as a list (the sentence ends of Japanese and English text; a line break already there is kept)
    juce::String listText() const
    {
        const juce::String t = text(); juce::StringArray out;
        for (auto line : juce::StringArray::fromLines (t))
        {
            line = line.trim(); if (line.isEmpty()) continue;
            juce::String cur;
            for (int i = 0; i < line.length(); ++i)
            {
                const auto c = line[i]; cur << juce::String::charToString (c);
                const bool ja = c == (juce::juce_wchar) 0x3002;                                           // 。
                const bool en = c == '.' && i + 2 < line.length() && line[i + 1] == ' ' && juce::CharacterFunctions::isUpperCase (line[i + 2]);
                if (ja || en) { out.add (cur.trim()); cur.clear(); if (en) ++i; }
            }
            if (cur.trim().isNotEmpty()) out.add (cur.trim());
        }
        if (out.size() < 2) return t;
        for (auto& l : out) l = juce::String::fromUTF8 ("\xe3\x83\xbb") + l;
        return out.joinIntoString ("\n");
    }
    juce::TextLayout layoutFor (float w) const
    {
        juce::AttributedString as; as.append (listText(), theme::ui (13.5f), theme::text);
        as.setWordWrap (juce::AttributedString::byWord); as.setLineSpacing (3.0f);
        juce::TextLayout tl; tl.createLayout (as, w); return tl;
    }
    // the height the text needs at this width (the notes column sizes the card by it)
    int neededHeight (int w) const { return 10 + 18 + 4 + (int) std::ceil (layoutFor ((float) (w - 28)).getHeight()) + 14; }
    void paint (juce::Graphics& g) override
    {
        panelBox (g, getLocalBounds().toFloat());
        auto r = getLocalBounds().reduced (14, 10);
        g.setColour (theme::accent); g.setFont (theme::ui (11.0f, true));
        g.drawText (ph.tr ("ヘルプ", "HELP") + (title.isNotEmpty() ? "  |  " + title : juce::String()), r.removeFromTop (18), juce::Justification::centredLeft);
        r.removeFromTop (4);
        layoutFor ((float) r.getWidth()).draw (g, r.toFloat());
    }
private:
    ParamHelp& ph;
};

// ===========================================================================
//  Filter graph: Low Cut and High Cut (Butterworth, chosen slopes) plus the COLOR bandwidth.
//  Drag the handles left / right to move the corner frequencies.
// ===========================================================================
class FilterGraph : public juce::Component
{
public:
    FilterGraph (ParamHelp& h) : ph (h)
    {
        setHelp (*this, "Filter", "ER にかかるフィルターの特性（Butterworth）。左がローカット、右がハイカット。縦線をドラッグで周波数、右のメニューで傾き（6〜48 dB/oct）。点線は COLOR の Bandwidth。背景はスペアナ：橙＝直接音（入力）、緑＝ER（Mix の前）。",
                 "The filters on the ER (Butterworth): low cut on the left, high cut on the right. Drag a line to move its frequency; pick the slope (6-48 dB/oct) on the right. Dotted = COLOR Bandwidth. Behind: spectra of the direct sound (amber, the input) and the ER (green, before the mix).");
    }
    static double order (int slope) { static const int o[6] = { 1, 2, 3, 4, 6, 8 }; return o[juce::jlimit (0, 5, slope)]; }
    const std::vector<float>* specD = nullptr; const std::vector<float>* specE = nullptr;     // 121 points, 20 Hz - 20 kHz, dBFS
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        plot = b.withTrimmedLeft (26).withTrimmedBottom (14).withTrimmedTop (2).withTrimmedRight (4);
        g.setColour (theme::well); g.fillRoundedRectangle (plot, 3.0f);
        const double lc = ph.value ("lowcut"), hc = ph.value ("highcut"), bw = ph.value ("bw") * 1000.0;
        const double nl = order ((int) ph.value ("lcSlope")), nh = order ((int) ph.value ("hcSlope"));
        const bool lcOn = lc > 20.5, hcOn = hc < 19900;
        g.setFont (theme::mono (8.5f));
        for (double f : { 100.0, 1000.0, 10000.0 })
        { g.setColour (theme::grid); g.drawVerticalLine ((int) X (f), plot.getY(), plot.getBottom());
          g.setColour (theme::faint); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) X (f) - 14, (int) plot.getBottom() + 1, 28, 12, juce::Justification::centred); }
        for (int db : { 0, -12, -24, -36 })
        { g.setColour (theme::grid); g.drawHorizontalLine ((int) Y (db), plot.getX(), plot.getRight());
          g.setColour (theme::faint); g.drawText (minus (juce::String (db)), 0, (int) Y (db) - 6, 24, 12, juce::Justification::centredRight); }
        auto curve = [&] (std::function<double (double)> h, juce::Colour c, float w, bool dotted)
        {
            juce::Path p;
            for (int i = 0; i <= 120; ++i)
            {
                const double f = 20 * std::pow (1000.0, i / 120.0);
                const float y = Y (juce::jlimit (-48.0, 3.0, 20 * std::log10 (h (f) + 1e-9)));
                if (i == 0) p.startNewSubPath (X (f), y); else p.lineTo (X (f), y);
            }
            g.setColour (c);
            if (dotted) { juce::Path d; const float dl[] = { 2.0f, 2.5f }; juce::PathStrokeType (w).createDashedStroke (d, p, dl, 2); g.fillPath (d); }
            else g.strokePath (p, juce::PathStrokeType (w));
        };
        // spectra behind the curves: direct (amber) and ER (accent), -90 .. -6 dBFS over the plot height
        auto spectrum = [&] (const std::vector<float>* v, juce::Colour c)
        {
            if (v == nullptr || v->size() < 2) return;
            juce::Path p; p.startNewSubPath (plot.getX(), plot.getBottom());
            for (size_t q = 0; q < v->size(); ++q)
            {
                const double f = 20 * std::pow (1000.0, (double) q / (double) (v->size() - 1));
                const float t = juce::jlimit (0.0f, 1.0f, ((*v)[q] + 90.0f) / 84.0f);
                p.lineTo (X (f), plot.getBottom() - t * plot.getHeight());
            }
            p.lineTo (plot.getRight(), plot.getBottom()); p.closeSubPath();
            g.setColour (c.withAlpha (0.22f)); g.fillPath (p);
            g.setColour (c.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (0.8f));
        };
        spectrum (specD, theme::direct); spectrum (specE, theme::accent);
        g.setFont (theme::ui (8.5f, true));
        g.setColour (theme::direct); g.drawText ("Direct", (int) plot.getRight() - 84, (int) plot.getBottom() - 13, 40, 11, juce::Justification::centredRight);
        g.setColour (theme::accent); g.drawText ("ER", (int) plot.getRight() - 40, (int) plot.getBottom() - 13, 36, 11, juce::Justification::centredRight);
        if (bw < 19900) curve ([bw] (double f) { return 1.0 / std::sqrt (1 + std::pow (f / bw, 8.0)); }, theme::muted, 1.0f, true);
        curve ([=] (double f) { double a = 1; if (lcOn) a /= std::sqrt (1 + std::pow (lc / f, 2 * nl)); if (hcOn) a /= std::sqrt (1 + std::pow (f / hc, 2 * nh)); return a; },
               theme::accent, 1.8f, false);
        auto handle = [&] (double f, bool on, const juce::String& t, bool right)
        {
            const float x = X (f);
            g.setColour (on ? theme::direct : theme::faint); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.fillEllipse (x - 3.5f, plot.getY() + 3, 7, 7);
            g.setFont (theme::mono (9.0f, true));
            g.drawText (t, right ? (int) x - 64 : (int) x + 4, (int) plot.getY() + 12, 60, 11, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
        };
        handle (lc, lcOn, lcOn ? juce::String ((int) nl * 6) + "dB" : juce::String ("off"), false);
        handle (hc, hcOn, hcOn ? juce::String ((int) nh * 6) + "dB" : juce::String ("off"), true);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float dl = std::abs (e.position.x - X (ph.value ("lowcut"))), dh = std::abs (e.position.x - X (ph.value ("highcut")));
        drag = std::min (dl, dh) > 14 ? juce::String() : juce::String (dl < dh ? "lowcut" : "highcut");
        if (drag.isEmpty()) return;
        if (e.mods.isAltDown()) { ph.resetDefault (drag); drag = {}; repaint(); return; }
        ph.proc.undo.beginNewTransaction(); if (auto* p = ph.get (drag)) p->beginChangeGesture();
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.isEmpty()) return;
        const double f = 20 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((e.position.x - plot.getX()) / plot.getWidth())));
        ph.set (drag, (float) f); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag.isNotEmpty()) if (auto* p = ph.get (drag)) p->endChangeGesture(); drag = {}; setMouseCursor (juce::MouseCursor::NormalCursor); }
private:
    float X (double f) const { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); }
    float Y (double db) const { return plot.getY() + (float) ((3 - db) / 51.0) * plot.getHeight(); }
    ParamHelp& ph; juce::Rectangle<float> plot; juce::String drag;
};

// ===========================================================================
//  "What you hear now": observations written from the rendered ER, plus the last change in words
// ===========================================================================
class NowCard : public juce::Component
{
public:
    NowCard (ParamHelp& h) : ph (h) {}
    juce::String change; juce::StringArray notes;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        panelBox (g, b);
        auto r = getLocalBounds().reduced (14, 10);
        g.setColour (theme::accent); g.setFont (theme::ui (11.0f, true));
        g.drawText (ph.tr ("リバーブ音", "REVERB SOUND"), r.removeFromTop (18), juce::Justification::centredLeft);
        r.removeFromTop (4);
        // shrink the text until it fits the card
        for (float fz = 14.5f; ; fz -= 0.5f)
        {
            juce::AttributedString as; as.setWordWrap (juce::AttributedString::byWord); as.setLineSpacing (fz > 10.5f ? 4.0f : 1.0f);
            if (change.isNotEmpty()) as.append (change + "\n\n", theme::ui (fz + 0.5f, true), theme::direct);
            for (auto& n : notes) as.append (juce::String::fromUTF8 ("\xe3\x83\xbb") + n + "\n", theme::ui (fz), theme::text.withAlpha (0.9f));
            juce::TextLayout tl; tl.createLayout (as, (float) r.getWidth());
            if (tl.getHeight() <= (float) r.getHeight() || fz <= 9.0f) { tl.draw (g, r.toFloat()); break; }
        }
    }
private:
    ParamHelp& ph;
};
