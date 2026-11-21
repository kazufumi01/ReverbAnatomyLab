#pragma once
#include "Widgets.h"
#include "RevViews.h"

// ===========================================================================
//  Display modes besides the full lab screen:
//   SIMPLE  - one deck of knobs (grouped like a classic one-page reverb) + the graphs
//   REMOTE  - small screens: an '80s fader remote, and a modern knob row over one graph
//  Every control here is either one existing parameter, or a macro that writes several
//  (when it moves, like the faceplate). No new DSP; the full screen shows the same values.
// ===========================================================================
struct MCtl
{
    juce::String label, led;                       // knob caption, 3-4 letter LED name
    std::function<float()> getN;                   // 0..1 position
    std::function<void (float)> setN;
    std::function<juce::String()> text;            // the value as shown
    float defN = 0.5f;
    juce::StringArray ids;                         // parameters it writes (undo / host gestures)
    const char* ja = ""; const char* en = "";
    bool valid() const { return (bool) getN; }
};

// dial colours (the same as the graphs): EQ = yellow, reverb time = blue, everything else = the notes' cyan
namespace mcol
{
    inline juce::Colour eq()    { return NodeGraph::eqCol(); }
    inline juce::Colour rt()    { return NodeGraph::rtCol(); }
    inline juce::Colour other() { return theme::accent; }
    inline juce::Colour forKey (const juce::String& k)
    {
        if (k == "hicut" || k == "locut" || k == "ilc" || k == "ihc" || k == "tilt") return eq();
        if (k == "decay" || k == "hfreq" || k == "hshelf" || k == "bfreq" || k == "bmult" || k == "bright") return rt();
        return other();
    }
}

inline juce::String fmtHzS (float v) { return v >= 1000 ? juce::String (v / 1000, v >= 10000 ? 1 : 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; }

inline MCtl makeCtl (ParamHelp& ph, const juce::String& key)
{
    MCtl c;
    auto direct = [&] (const char* id, const char* label, const char* led, std::function<juce::String (float)> f, const char* ja, const char* en)
    {
        auto* p = ph.get (id); if (p == nullptr) return;
        c.label = label; c.led = led; c.ids = { id }; c.ja = ja; c.en = en;
        c.getN = [p] { return p->getValue(); };
        c.setN = [p] (float n) { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n)); };
        c.text = [p, f] { return f (p->convertFrom0to1 (p->getValue())); };
        c.defN = p->getDefaultValue();
    };
    auto pct = [] (float v) { return juce::String ((int) std::round (v)) + " %"; };
    auto ms = [] (float v) { return v < 10 ? juce::String (v, 1) + " ms" : juce::String ((int) std::round (v)) + " ms"; };
    auto db = [] (float v) { return fmtDb (v); };
    auto mulPct = [] (float v) { return juce::String ((int) std::round (v * 100)) + " %"; };
    auto hz = [] (float v) { return fmtHzS (v); };
    if (key == "mix") direct ("mix", "Mix", "MIX", pct, "ドライとリバーブの割合（50 %＝両方を最大）。", "Dry / reverb (50 % = both at full).");
    else if (key == "predelay") direct ("predelay", "PreDelay", "PDL", ms, "リバーブ全体（ER とテール）が鳴り始めるまでの遅れ。", "Delay before the whole reverb (ER and tail).");
    else if (key == "decay") direct ("tDecay", "Decay", "RTM", [] (float v) { return v < 10 ? juce::String (v, 2) + " s" : juce::String (v, 1) + " s"; },
                                     "残響時間（中域の RT60）。周波数ごとの RT はこれに対する %。", "Reverb time (mid RT60). Per-frequency RT is a % of this.");
    else if (key == "hfreq") direct ("rtF5", "HighFreq", "HFX", hz, "高域の残響を短くし始める周波数（RT 曲線の高域シェルフ）。", "Where the treble decay starts to shorten (RT curve high shelf).");
    else if (key == "hshelf") direct ("rtM5", "HighShelf", "HFR", mulPct, "高域の残響時間（Decay に対する %）。下げるほど暗く、空気に吸われた響き。", "Treble reverb time (% of Decay). Lower = darker, air-absorbed.");
    else if (key == "bfreq") direct ("rtF1", "BassFreq", "BXO", hz, "低域の残響を変え始める周波数（RT 曲線の低域シェルフ）。", "Where the bass decay starts to change (RT curve low shelf).");
    else if (key == "bmult") direct ("rtM1", "BassMult", "BAS", mulPct, "低域の残響時間（Decay に対する %）。上げると大きな部屋・温かい響き。", "Bass reverb time (% of Decay). Higher = bigger, warmer room.");
    else if (key == "early") direct ("diffusion", "Early", "EDF", pct, "ER（初期反射）のにじみ。0 % で反射が 1 本ずつ聞こえる。", "ER diffusion. 0 % = single reflections audible.");
    else if (key == "late") direct ("tDiff", "Late", "DIF", pct, "テールの拡散（密度の立ち上がり）。下げると粒が残る。", "Tail diffusion (how fast it gets dense). Lower = grainier.");
    else if (key == "rate") direct ("tModR", "Rate", "RAT", [] (float v) { return juce::String (v, 2) + " Hz"; }, "テールの揺れの速さ。", "Speed of the tail's motion.");
    else if (key == "depth") direct ("tModD", "Depth", "DEP", pct, "テールの揺れの量。金属的な鳴りをぼかし、コーラス感が出る。", "Amount of tail motion. Blurs metallic ringing, adds chorus.");
    else if (key == "hicut") direct ("eqLP", "HighCut", "HCT", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHzS (v); }, "リバーブ出力のハイカット（MASTER EQ の LP）。", "High cut on the reverb output (master EQ LP).");
    else if (key == "locut") direct ("eqHP", "LowCut", "LCT", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHzS (v); }, "リバーブ出力のローカット（MASTER EQ の HP）。", "Low cut on the reverb output (master EQ HP).");
    else if (key == "bal") direct ("erBal", "Refl / Rev", "BAL", [] (float v) { const int b = (int) std::round (v); return b == 0 ? juce::String ("Equal") : b < 0 ? "ER +" + juce::String (-b) : "TAIL +" + juce::String (b); },
                                   "ER とテールのバランス。左＝ER が前（近い）、右＝テールが前（遠い）。", "ER / tail balance. Left = ER forward (near), right = tail forward (far).");
    else if (key == "spread") direct ("length", "Spread", "SPR", ms, "ER の長さ（立ち上がりにかける時間）。", "ER length (how long the build-up takes).");
    else if (key == "density") direct ("taps", "Density", "DEN", [] (float v) { return juce::String ((int) std::round (v)); }, "ER の反射の本数。", "Number of ER reflections.");
    else if (key == "erlevel") direct ("level", "ER Level", "ELV", db, "ER の音量。", "ER level.");
    else if (key == "first") direct ("first", "First", "EFS", ms, "最初の反射までの時間（ITDG）。", "Time to the first reflection (ITDG).");
    else if (key == "erwidth") direct ("width", "ER Width", "EWD", pct, "ER の左右の広がり。", "ER stereo width.");
    else if (key == "darken") direct ("darken", "Darken", "DRK", [] (float v) { return juce::String (v, 1) + " oct"; }, "後ろの反射ほど暗くなる量。", "How much later reflections get darker.");
    else if (key == "tlvl") direct ("tLvl", "Tail Level", "TLV", db, "テールの音量。", "Tail level.");
    else if (key == "out") direct ("out", "Output", "OUT", db, "出力の音量。", "Output level.");
    else if (key == "width") direct ("outW", "Width", "WID", pct, "出力の左右の広がり。", "Output stereo width.");
    else if (key == "ilc") direct ("iLC", "In LowCut", "ILC", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHzS (v); }, "リバーブに入る前のローカット。", "Low cut before the reverb.");
    else if (key == "ihc") direct ("iHC", "In HighCut", "IHC", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHzS (v); }, "リバーブに入る前のハイカット。", "High cut before the reverb.");
    else if (key == "tilt") direct ("iTilt", "Tilt", "TLT", db, "リバーブに入る音の明るさ。", "Brightness into the reverb.");
    else if (key == "bw") direct ("bw", "Bandwidth", "BW", [] (float v) { return v >= 19.9f ? juce::String ("Off") : juce::String (v, 1) + " kHz"; },
                                   "昔のデジタル機の帯域（低いサンプリング周波数）。20 kHz = オフ。", "Band limit of old digital units (low sample rate). 20 kHz = off.");
    else if (key == "bits") direct ("bits", "Bits", "BIT", [] (float v) { return v >= 23.5f ? juce::String ("Off") : juce::String ((int) std::round (v)) + " bit"; },
                                    "量子化のビット数。24 = オフ。12〜16 bit で初期デジタルのざらつき。", "Quantisation bits. 24 = off. 12-16 bit gives early-digital grain.");
    else if (key == "wander") direct ("wander", "Wander", "WND", pct, "ER のゆらぎ。", "ER wander.");
    else if (key == "tdelay") direct ("tPre", "Tail Delay", "TDL", ms, "ER からテールまでの遅れ。", "Delay from the ER to the tail.");
    else if (key == "size")
    {   // macro: the ER's size and the tail's delay-line size together
        auto* p = ph.get ("size");
        c.label = "Size"; c.led = "SIZ"; c.ids = { "size", "tSize" };
        c.ja = "空間の大きさ（マクロ）：ER の Size と、テールの Size（遅延線の長さ）を一緒に動かします。"; c.en = "Space size (macro): the ER's Size and the tail's Size (delay-line length) together.";
        c.getN = [p] { return p->getValue(); };
        c.setN = [&ph, p] (float n) { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n)); ph.set ("tSize", juce::jlimit (10.0f, 300.0f, 80.0f * ph.value ("size") / 100.0f)); };
        c.text = [&ph] { return juce::String ((int) std::round (ph.value ("size"))) + " %"; };
        c.defN = p->getDefaultValue();
    }
    else if (key == "attack")
    {   // macro: how the reverb builds up - the ER's growth and the gap before the tail
        c.label = "Attack"; c.led = "SHP"; c.ids = { "growth", "tPre" };
        c.ja = "響きの立ち上がり（マクロ）：ER の Growth（後ろへふくらむ量）と、テールが入るまでの遅れ（Tail Delay）を一緒に。0 %＝すぐ立ち上がる、100 %＝ゆっくりふくらむ。";
        c.en = "Build-up (macro): the ER's growth and the gap before the tail together. 0 % = immediate, 100 % = slow swell.";
        c.getN = [&ph] { return juce::jlimit (0.0f, 1.0f, (ph.value ("tPre") - 5.0f) / 75.0f); };
        c.setN = [&ph] (float n) { n = juce::jlimit (0.0f, 1.0f, n); ph.set ("tPre", 5.0f + 75.0f * n); ph.set ("growth", 0.4f + 1.6f * n); };
        c.text = [&ph] { return juce::String ((int) std::round (juce::jlimit (0.0f, 1.0f, (ph.value ("tPre") - 5.0f) / 75.0f) * 100)) + " %"; };
        c.defN = 7.0f / 75.0f;
    }
    else if (key == "bright")
    {   // macro: the treble decay (points 4 and 5 of the RT curve) together
        c.label = "Brightness"; c.led = "BRT"; c.ids = { "rtM4", "rtM5" };
        c.ja = "高域の残り方（マクロ）：RT 曲線の点 4・5（高域）を一緒に上下。上げるほど明るく長い高域。"; c.en = "Treble decay (macro): RT points 4 and 5 together. Higher = brighter, longer treble.";
        auto b = [&ph] { return juce::jlimit (-1.0f, 1.0f, (float) (std::log2 (std::max (0.01f, ph.value ("rtM5")) / 0.45f) / 1.5)); };
        c.getN = [b] { return 0.5f + 0.5f * b(); };
        c.setN = [&ph] (float n) { const float x = juce::jlimit (-1.0f, 1.0f, 2 * n - 1); ph.set ("rtM5", 0.45f * std::pow (2.0f, 1.5f * x)); ph.set ("rtM4", juce::jlimit (0.2f, 1.6f, 0.75f * std::pow (2.0f, x))); };
        c.text = [b] { const int v = (int) std::round (b() * 100); return (v > 0 ? "+" : "") + juce::String (v); };
        c.defN = 0.5f;
    }
    else if (key == "distance")
    {   // macro: near = more ER and a longer predelay, far = more tail and almost no gap
        c.label = "Distance"; c.led = "DST"; c.ids = { "erBal", "predelay" };
        c.ja = "距離（マクロ）：近い＝ER が前でプリディレイ長め、遠い＝テールが前でプリディレイ短め。"; c.en = "Distance (macro): near = ER forward and a longer predelay, far = tail forward and almost no gap.";
        c.getN = [&ph] { return juce::jlimit (0.0f, 1.0f, (ph.value ("erBal") + 100.0f) / 200.0f); };
        c.setN = [&ph] (float n) { n = juce::jlimit (0.0f, 1.0f, n); ph.set ("erBal", -100.0f + 200.0f * n); ph.set ("predelay", 30.0f * (1.0f - n)); };
        c.text = [&ph] { const int d = (int) std::round ((ph.value ("erBal") + 100.0f) / 2.0f); return juce::String (d) + " %"; };
        c.defN = 0.5f;
    }
    return c;
}

// ---------------------------------------------------------------------------
//  A rotary knob over one MCtl (drag up / down, Shift fine, double-click or Option+click default, wheel)
// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (ParamHelp& h, const juce::String& key, juce::Colour c, bool big = false) : ph (h), ctl (makeCtl (h, key)), col (c), large (big)
    { if (ctl.valid()) setHelpText(); setWantsKeyboardFocus (true); }
    Knob (ParamHelp& h, const juce::String& key, bool big = false) : Knob (h, key, mcol::forKey (key), big) {}
    // up / down (and left / right) keys move it once it has been clicked; Shift = fine
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int kc = k.getKeyCode();
        if (! ctl.valid() || (kc != juce::KeyPress::upKey && kc != juce::KeyPress::downKey && kc != juce::KeyPress::leftKey && kc != juce::KeyPress::rightKey)) return false;
        const float d = (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey ? 1.0f : -1.0f) * (k.getModifiers().isShiftDown() ? 0.002f : 0.01f);
        ph.proc.undo.beginNewTransaction(); gestures (true); ctl.setN (juce::jlimit (0.0f, 1.0f, ctl.getN() + d)); gestures (false); repaint();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    MCtl ctl;
    void paint (juce::Graphics& g) override
    {
        if (! ctl.valid()) return;
        auto b = getLocalBounds().toFloat();
        // short and wide (the deck under the monitors): the dial on the left, the name and the value on its right
        const bool side = ! large && getHeight() < 64 && getWidth() > getHeight() * 1.5f;
        juce::Rectangle<float> vr;
        if (side)
        {
            auto txt = b.withTrimmedLeft (b.getHeight());
            b = b.withWidth (b.getHeight());
            g.setColour (theme::text); g.setFont (theme::ui (10.5f, true));
            g.drawFittedText (ctl.label, txt.removeFromTop (txt.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.65f);
            vr = txt;
        }
        else
        {
            g.setColour (theme::text); g.setFont (theme::ui (large ? 13.0f : 11.5f, true));
            g.drawText (ctl.label, b.removeFromTop (16), juce::Justification::centred);
            vr = b.removeFromBottom (16);
        }
        const float R = std::min (b.getWidth(), b.getHeight()) * 0.5f - 3, cx = b.getCentreX(), cy = b.getCentreY();
        const float a0 = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f, t = ctl.getN();
        juce::Path bgArc; bgArc.addCentredArc (cx, cy, R, R, 0, -a0 + juce::MathConstants<float>::pi * 0.5f * 0, -a0 + span, true);
        juce::Path track; track.addCentredArc (cx, cy, R, R, 0, -juce::MathConstants<float>::pi * 0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        g.setColour (theme::shade (0.35f)); g.strokePath (track, juce::PathStrokeType (large ? 7.0f : 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val; val.addCentredArc (cx, cy, R, R, 0, -juce::MathConstants<float>::pi * 0.75f, -juce::MathConstants<float>::pi * 0.75f + span * t, true);
        g.setColour (col); g.strokePath (val, juce::PathStrokeType (large ? 7.0f : 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ri = R - (large ? 9.0f : 6.0f);
        g.setColour (theme::lightPanels ? juce::Colour (0xff3a3d44) : juce::Colour (0xff15171b)); g.fillEllipse (cx - ri, cy - ri, 2 * ri, 2 * ri);
        g.setColour (col.withAlpha (0.35f)); g.drawEllipse (cx - ri, cy - ri, 2 * ri, 2 * ri, 1.0f);
        const float ang = -juce::MathConstants<float>::pi * 0.75f + span * t;
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (cx + std::sin (ang) * ri * 0.25f, cy - std::cos (ang) * ri * 0.25f, cx + std::sin (ang) * (ri - 3), cy - std::cos (ang) * (ri - 3), large ? 3.0f : 2.2f);
        (void) bgArc;
        g.setColour (drag || hasKeyboardFocus (false) ? col : theme::muted); g.setFont (theme::mono (large ? 13.0f : 11.0f, true));
        if (side) { g.setFont (theme::mono (10.0f, true)); g.drawFittedText (ctl.text(), vr.toNearestInt(), juce::Justification::topLeft, 1, 0.7f); }
        else g.drawText (ctl.text(), vr, juce::Justification::centred);
        if (hasKeyboardFocus (false)) { g.setColour (col.withAlpha (0.6f)); g.drawRoundedRectangle (vr.reduced (vr.getWidth() * 0.2f, 0).expanded (0, 1), 3.0f, 1.0f); }
        if (anyLocked (ph, ctl.ids)) drawLockMark (g, getLocalBounds().toFloat());
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! ctl.valid()) return;
        if (e.mods.isPopupMenu()) { showLockMenu (ph, ctl.ids, this); return; }
        grabKeyboardFocus();
        ph.proc.undo.beginNewTransaction(); gestures (true);
        if (e.mods.isAltDown() || e.getNumberOfClicks() > 1) { ctl.setN (ctl.defN); gestures (false); repaint(); return; }
        drag = true; startN = ctl.getN(); lastY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag) return;
        startN = juce::jlimit (0.0f, 1.0f, startN + (lastY - e.position.y) * (e.mods.isShiftDown() ? 0.0012f : 0.006f)); lastY = e.position.y;
        ctl.setN (startN); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag) gestures (false); drag = false; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! ctl.valid()) return;
        ph.proc.undo.beginNewTransaction(); gestures (true); ctl.setN (juce::jlimit (0.0f, 1.0f, ctl.getN() + w.deltaY * 0.05f)); gestures (false); repaint();
    }
private:
    void setHelpText() { setHelp (*this, ctl.label.toRawUTF8(), ctl.ja, ctl.en); getProperties().set ("hids", ctl.ids.joinIntoString (",")); getProperties().set ("hlabel", ctl.label); }
    void gestures (bool begin) { for (auto& id : ctl.ids) if (auto* p = ph.get (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); } }
    ParamHelp& ph; juce::Colour col; bool large, drag = false; float startN = 0, lastY = 0;
};

// ---------------------------------------------------------------------------
//  SIMPLE: one deck of grouped knobs (a classic one-page reverb layout)
// ---------------------------------------------------------------------------
class SimpleDeck : public juce::Component
{
public:
    struct Group { const char* name; juce::Colour col; std::vector<juce::String> keys; int cols; float units; };
    explicit SimpleDeck (ParamHelp& h) : ph (h) { setLayout (0); }
    // 0 SIMPLE (the classic deck, 2 rows), 1 BASIC 8 (one row: the eight everyday controls), 2 MACRO (Mix, Decay + the four macros),
    // 3 COLOR (Bandwidth, Bits, Wander: the era of the unit; sits at the deck's right end whatever the deck shows)
    void setLayout (int L)
    {
        L = juce::jlimit (0, 3, L);
        if (L == layoutId && ! knobs.isEmpty()) return;
        layoutId = L; knobs.clear();
        // frames and dials: EQ yellow, reverb time blue, the rest cyan (as on the graphs and in the notes)
        if (L == 0)
            groups = {
                { "", mcol::other(), { "mix", "predelay" }, 1, 1.0f },
                { "", mcol::rt(), { "decay" }, 1, 1.5f },
                { "DAMPING", mcol::rt(), { "bfreq", "hfreq", "bmult", "hshelf" }, 2, 2.0f },       // (0.10.16) left: BassFreq / BassMult, right: HighFreq / HighShelf
                { "SHAPE", mcol::other(), { "size", "attack" }, 1, 1.0f },
                { "DIFF", mcol::other(), { "early", "late" }, 1, 1.0f },
                { "MOD", mcol::other(), { "rate", "depth" }, 1, 1.0f },
                { "EQ", mcol::eq(), { "hicut", "locut" }, 1, 1.0f },
                { "OUT", mcol::other(), { "bal", "width" }, 1, 1.0f } };
        else if (L == 1)
            groups = {
                { "", mcol::other(), { "mix", "predelay" }, 2, 2.0f },
                { "", mcol::rt(), { "decay" }, 1, 1.1f },
                { "SPACE", mcol::other(), { "size", "late" }, 2, 2.0f },
                { "DAMPING", mcol::rt(), { "bmult", "hshelf" }, 2, 2.0f },
                { "", mcol::other(), { "bal" }, 1, 1.0f } };
        else if (L == 3)
            groups = { { "COLOR", mcol::other(), { "bw", "bits", "wander" }, 3, 1.0f } };
        else
            groups = {
                { "", mcol::other(), { "mix" }, 1, 1.0f },
                { "", mcol::rt(), { "decay" }, 1, 1.1f },
                { "MACRO", mcol::other(), { "size", "attack", "bright", "distance" }, 4, 4.0f } };
        for (auto& gr : groups)
            for (auto& k : gr.keys) { auto* kn = knobs.add (new Knob (ph, k, k == "decay")); addAndMakeVisible (kn); }
        resized(); repaint();
    }
    int layout() const { return layoutId; }
    // the group whose heading is at p (local), for the help
    juce::String groupAt (juce::Point<int> p) const
    {
        for (size_t i = 0; i < groups.size() && i < boxes.size(); ++i)
            if (groups[i].name[0] && boxes[i].withHeight (20).contains (p.toFloat())) return groups[i].name;
        return {};
    }
    void repaintKnobs() { for (auto* k : knobs) k->repaint(); }
    void resized() override
    {
        boxes.clear();
        if (groups.empty()) return;
        const float W = (float) getWidth(), H = (float) getHeight();
        float tot = 0; for (auto& gr : groups) tot += gr.units;
        const float gap = 8, unitW = (W - gap * (groups.size() - 1)) / tot;
        float x = 0; int k = 0;
        for (size_t i = 0; i < groups.size(); ++i)
        {
            const float w = groups[i].units * unitW; juce::Rectangle<float> r (x, 0, w, H); boxes.push_back (r);
            auto in = r.reduced (6, 4).withTrimmedTop (groups[i].name[0] ? 18 : 2).withTrimmedBottom ((float) bottomPad);
            const int n = (int) groups[i].keys.size(), cols = std::max (1, groups[i].cols), rows = (n + cols - 1) / cols;
            if (n == 1) knobs[k++]->setBounds (in.reduced (2, H > 150 ? 6 : 1).toNearestInt());
            else
            {
                const float cw = in.getWidth() / cols, ch = in.getHeight() / rows;
                for (int j = 0; j < n; ++j) knobs[k++]->setBounds (juce::Rectangle<float> (in.getX() + (j % cols) * cw, in.getY() + (j / cols) * ch, cw, ch).reduced (2, 1).toNearestInt());
            }
            x += w + gap;
        }
    }
    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < groups.size() && i < boxes.size(); ++i)
        {
            if (! groups[i].name[0]) continue;
            auto r = boxes[i];
            const auto col = (i == 0 && frameFn) ? frameFn() : groups[i].col;
            g.setColour (col.withAlpha (0.08f)); g.fillRoundedRectangle (r, 8.0f);
            g.setColour (col.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1), 8.0f, 2.0f);
            g.setFont (theme::ui (11.0f, true));
            if (i == 0 && frameFn && ! subFn) g.drawText (groups[i].name, r.withHeight (18).translated (8, 2), juce::Justification::centredLeft);
            else if (i == 0 && subFn) { g.drawText (groups[i].name, r.withHeight (18).translated (8, 2), juce::Justification::centredLeft); g.setFont (theme::ui (9.5f, true)); g.drawText (subFn(), r.withHeight (18).translated (-8, 2), juce::Justification::centredRight); }
            else g.drawText (groups[i].name, r.withHeight (18).translated (0, 2), juce::Justification::centred);
        }
    }
private:
public:
    std::function<juce::Colour()> frameFn; std::function<juce::String()> subFn;      // the first group's frame colour / right-hand caption (COLOR: the era)
    int bottomPad = 0;                                                                   // room left under the knobs (COLOR: the preset box)
private:
    ParamHelp& ph; std::vector<Group> groups; juce::OwnedArray<Knob> knobs; std::vector<juce::Rectangle<float>> boxes; int layoutId = -1;
};

// ---------------------------------------------------------------------------
//  REMOTE 1: an '80s fader remote. Top: a red screen with the page's graph (reverb envelope,
//  decay vs frequency, filters, ER taps, motion) and its readings, plus in / out LED meters.
//  Then the program name, page keys, the six-slot LED readout, six faders, six soft keys
//  (tap = slot default). Click a fader / slot to select it: up / down keys move it, left / right
//  select the neighbour (Shift = fine).
// ---------------------------------------------------------------------------
class FaderRemote : public juce::Component
{
public:
    explicit FaderRemote (ParamHelp& h) : ph (h)
    {
        pages = { { "QUICK", { "decay", "attack", "spread", "size", "hicut", "predelay" } },
                  { "TIME",  { "decay", "bmult", "bfreq", "hshelf", "hfreq", "tdelay" } },
                  { "SHAPE", { "attack", "spread", "size", "late", "early", "density" } },
                  { "FILTER",{ "locut", "hicut", "ilc", "ihc", "tilt", "bright" } },
                  { "MOD",   { "rate", "depth", "wander", "", "", "" } },
                  { "ER",    { "erlevel", "first", "spread", "erwidth", "darken", "early" } },
                  { "OUT",   { "mix", "bal", "tlvl", "erlevel", "width", "out" } } };
        setPage ((int) ph.proc.uiState.getProperty ("rmPage", 0));
        setWantsKeyboardFocus (true);
    }
    // what the editor hands over for the screen: the whole reverb's envelope (dB, 0 = direct) over envMs,
    // the ER taps (ms, dB), and the latest measurement
    std::vector<float> envDb; double envMs = 0;
    std::vector<std::pair<float, float>> taps;
    rv::Measure meas;
    void setPage (int p)
    {
        page = juce::jlimit (0, (int) pages.size() - 1, p); ph.proc.uiState.setProperty ("rmPage", page, nullptr);
        ctls.clear(); for (auto& k : pages[(size_t) page].second) ctls.push_back (k.isEmpty() ? MCtl() : makeCtl (ph, k));
        if (sel >= 0 && ! ctls[(size_t) sel].valid()) sel = 0;
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int kc = k.getKeyCode();
        if (kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey)
        {
            int i = sel < 0 ? 0 : sel;
            for (int n = 0; n < 6; ++n) { i = (i + (kc == juce::KeyPress::rightKey ? 1 : 5)) % 6; if (ctls[(size_t) i].valid()) break; }
            sel = i; updateHelp (sel); repaint(); return true;
        }
        if ((kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey) && sel >= 0 && ctls[(size_t) sel].valid())
        {
            const float d = (kc == juce::KeyPress::upKey ? 1.0f : -1.0f) * (k.getModifiers().isShiftDown() ? 0.002f : 0.01f);
            ph.proc.undo.beginNewTransaction(); gest (sel, true); ctls[(size_t) sel].setN (juce::jlimit (0.0f, 1.0f, ctls[(size_t) sel].getN() + d)); gest (sel, false);
            repaint(); return true;
        }
        if (kc == juce::KeyPress::pageUpKey || kc == juce::KeyPress::pageDownKey) { setPage ((page + (kc == juce::KeyPress::pageDownKey ? 1 : (int) pages.size() - 1)) % (int) pages.size()); return true; }
        return false;
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto body = juce::Colour (0xffe4e0d6), ink = juce::Colour (0xff2b2a27), led = juce::Colour (0xffff3b2f), ledBg = juce::Colour (0xff160606);
        g.setColour (body); g.fillRoundedRectangle (b, 16.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawRoundedRectangle (b.reduced (1), 16.0f, 2.0f);
        // ---- the screen: graph + readings + meters (in a grey bezel)
        auto bez = juce::Rectangle<float> (b.getX() + 14, b.getY() + 12, b.getWidth() - 28, 262);
        g.setColour (juce::Colour (0xffc9c4b8)); g.fillRoundedRectangle (bez, 10.0f);
        auto scr = bez.reduced (12, 12);
        g.setColour (ledBg); g.fillRoundedRectangle (scr, 8.0f);
        // tabs across the top of the screen (the page's view)
        const char* view = page == 1 ? "TIME/FREQ" : page == 3 ? "FILTER/FREQ" : page == 5 ? "EARLY REF" : page == 4 ? "MOTION" : page == 6 ? "LEVELS" : "REVERB";
        g.setColour (led); g.fillRoundedRectangle (scr.getX() + 8, scr.getY() + 6, 110, 18, 3.0f);
        g.setColour (ledBg); g.setFont (theme::mono (11.0f, true)); g.drawText (view, (int) scr.getX() + 8, (int) scr.getY() + 6, 110, 18, juce::Justification::centred);
        g.setColour (led.withAlpha (0.85f)); g.setFont (theme::mono (11.0f, true));
        g.drawText (progName().toUpperCase().substring (0, 22), (int) scr.getX() + 126, (int) scr.getY() + 6, (int) scr.getWidth() - 180, 18, juce::Justification::centredLeft);
        // meters (right edge, vertical)
        auto lvl = [] (float pk) { return juce::jlimit (0.0f, 1.0f, (20 * std::log10 (pk + 1e-9f) + 48) / 48); };
        const float mi = lvl (ph.proc.inPeak.load()), mo = lvl (ph.proc.erPeak.load());
        auto mr = juce::Rectangle<float> (scr.getRight() - 42, scr.getY() + 30, 34, scr.getHeight() - 52);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 14; ++i)
            {
                const float y = mr.getBottom() - (i + 1) * mr.getHeight() / 14.0f, v = c == 0 ? mi : mo;
                g.setColour (v > (float) i / 14.0f ? led : led.withAlpha (0.12f)); g.fillEllipse (mr.getX() + 4 + c * 18, y + 2, 7, 7);
            }
        g.setColour (led.withAlpha (0.6f)); g.setFont (theme::mono (8.5f));
        g.drawText ("IN", (int) mr.getX() - 2, (int) mr.getBottom() + 2, 18, 10, juce::Justification::centred);
        g.drawText ("ER", (int) mr.getX() + 16, (int) mr.getBottom() + 2, 18, 10, juce::Justification::centred);
        // graph
        auto gr = juce::Rectangle<float> (scr.getX() + 30, scr.getY() + 34, scr.getWidth() - 86, scr.getHeight() - 92);
        g.setColour (led.withAlpha (0.18f));
        for (int i = 0; i <= 8; ++i) g.drawVerticalLine ((int) (gr.getX() + gr.getWidth() * i / 8), gr.getY(), gr.getBottom());
        for (int i = 0; i <= 4; ++i) g.drawHorizontalLine ((int) (gr.getY() + gr.getHeight() * i / 4), gr.getX(), gr.getRight());
        g.setColour (led.withAlpha (0.5f)); g.drawRect (gr, 1.0f);
        paintGraph (g, gr, led);
        // readings under the graph
        g.setColour (led); g.setFont (theme::mono (11.0f, true));
        g.drawFittedText (readings(), juce::Rectangle<float> (scr.getX() + 10, gr.getBottom() + 18, scr.getWidth() - 64, 34).toNearestInt(), juce::Justification::centredLeft, 2, 0.7f);
        // ---- name plate
        g.setColour (ink); g.setFont (theme::ui (13.0f, true));
        g.drawText ("REVERB  REMOTE", (int) b.getX(), (int) bez.getBottom() + 8, (int) b.getWidth(), 18, juce::Justification::centred);
        // ---- page keys (two rows)
        pageR.clear();
        const float kx = b.getX() + 18, kw = (b.getWidth() - 36) / 4.0f, ky = bez.getBottom() + 34;
        for (int i = 0; i < (int) pages.size(); ++i)
        {
            juce::Rectangle<float> r (kx + (i % 4) * kw, ky + (i / 4) * 34.0f, kw - 6, 30); pageR.push_back (r);
            const bool on = i == page;
            g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r.translated (0, 2), 5.0f);
            g.setColour (on ? juce::Colour (0xff2f6fa8) : juce::Colour (0xfff7f5ef)); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? juce::Colours::white : ink); g.setFont (theme::ui (12.0f, true)); g.drawText (pages[(size_t) i].first, r, juce::Justification::centred);
        }
        // ---- six-slot LED readout
        auto rd = juce::Rectangle<float> (b.getX() + 18, ky + 76, b.getWidth() - 36, 52);
        g.setColour (ledBg); g.fillRoundedRectangle (rd, 4.0f);
        const float sw = rd.getWidth() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            if (! ctls[(size_t) i].valid()) continue;
            auto c = juce::Rectangle<float> (rd.getX() + i * sw, rd.getY(), sw, rd.getHeight()).reduced (3, 4);
            if (i == sel) { g.setColour (led.withAlpha (0.18f)); g.fillRoundedRectangle (c.expanded (2, 3), 3.0f); }
            g.setColour (led); g.setFont (theme::mono (15.0f, true));
            g.drawText (ctls[(size_t) i].led, c.removeFromTop (22).toNearestInt(), juce::Justification::centred);
            g.setFont (theme::mono (11.0f, true));
            g.drawFittedText (ctls[(size_t) i].text(), c.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        // ---- faders
        fadR.clear(); capR.clear();
        const float fy0 = rd.getBottom() + 22, fy1 = b.getBottom() - 78;
        for (int i = 0; i < 6; ++i)
        {
            const float cx = rd.getX() + (i + 0.5f) * sw;
            juce::Rectangle<float> slot (cx - 4, fy0, 8, fy1 - fy0); fadR.push_back (slot.expanded (sw * 0.5f - 6, 12));
            g.setColour (juce::Colour (0xff1a1a1a)); g.fillRoundedRectangle (slot, 4.0f);
            for (int k = 0; k <= 8; ++k) { const float y = fy0 + (fy1 - fy0) * k / 8.0f; g.setColour (ink.withAlpha (0.55f)); g.drawLine (cx + 10, y, cx + sw * 0.5f - 8, y, 1.5f); }
            if (! ctls[(size_t) i].valid()) { capR.push_back ({}); continue; }
            const float y = fy1 - (fy1 - fy0) * ctls[(size_t) i].getN();
            juce::Rectangle<float> cap (cx - sw * 0.36f, y - 14, sw * 0.72f, 28); capR.push_back (cap);
            g.setColour (juce::Colours::black.withAlpha (0.28f)); g.fillRoundedRectangle (cap.translated (0, 4), 3.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbf8f0), 0, cap.getY(), juce::Colour (0xffd9d3c4), 0, cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (i == sel ? led : juce::Colours::black.withAlpha (0.35f)); g.drawLine (cap.getX() + 4, y, cap.getRight() - 4, y, i == sel ? 2.0f : 1.5f);
        }
        // ---- soft keys: tap = the slot's default
        softR.clear();
        for (int i = 0; i < 6; ++i)
        {
            juce::Rectangle<float> r (rd.getX() + i * sw + 5, b.getBottom() - 62, sw - 10, 44); softR.push_back (r);
            g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r.translated (0, 3), 6.0f);
            g.setColour (juce::Colour (0xfff7f5ef)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRoundedRectangle (r, 6.0f, 2.0f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        for (int i = 0; i < (int) pageR.size(); ++i) if (pageR[(size_t) i].contains (e.position)) { setPage (i); return; }
        for (int i = 0; i < (int) softR.size(); ++i)
            if (softR[(size_t) i].contains (e.position) && ctls[(size_t) i].valid())
            { ph.proc.undo.beginNewTransaction(); gest (i, true); ctls[(size_t) i].setN (ctls[(size_t) i].defN); gest (i, false); sel = i; repaint(); return; }
        for (int i = 0; i < (int) fadR.size(); ++i)
            if (fadR[(size_t) i].contains (e.position) && ctls[(size_t) i].valid())
            {
                drag = sel = i; ph.proc.undo.beginNewTransaction(); gest (i, true);
                startN = ctls[(size_t) i].getN(); lastY = e.position.y;
                if (! capR[(size_t) i].contains (e.position))
                {   // click on the slot: jump there
                    const auto& f = fadR[(size_t) i];
                    startN = juce::jlimit (0.0f, 1.0f, (f.getBottom() - 12 - e.position.y) / (f.getHeight() - 24));
                    ctls[(size_t) i].setN (startN);
                }
                updateHelp (i); repaint(); return;
            }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        const float span = fadR[(size_t) drag].getHeight() - 24;
        startN = juce::jlimit (0.0f, 1.0f, startN + (lastY - e.position.y) / span * (e.mods.isShiftDown() ? 0.2f : 1.0f)); lastY = e.position.y;
        ctls[(size_t) drag].setN (startN); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag >= 0) gest (drag, false); drag = -1; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < (int) fadR.size(); ++i) if (fadR[(size_t) i].contains (e.position) || (i < (int) softR.size() && softR[(size_t) i].contains (e.position))) { updateHelp (i); return; }
    }
private:
    // the page's graph on the red screen
    void paintGraph (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour led)
    {
        g.saveState(); g.reduceClipRegion (r.toNearestInt());
        auto fx = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        g.setFont (theme::mono (8.5f)); 
        auto axisF = [&]
        {
            for (double f : { 100.0, 1000.0, 10000.0 }) { g.setColour (led.withAlpha (0.55f)); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) fx (f) + 2, (int) r.getBottom() - 12, 30, 10, juce::Justification::centredLeft); }
        };
        juce::Path p; bool st = false;
        auto pt = [&] (float x, float y) { if (! st) { p.startNewSubPath (x, y); st = true; } else p.lineTo (x, y); };
        if (page == 1)
        {   // reverb time vs frequency (0 .. 2 x Decay), with the measured T30
            const double D = std::max (0.1, ph.proc.decayBase()), top = 2.2 * D;
            auto ty = [&] (double t) { return r.getBottom() - (float) juce::jlimit (0.0, 1.0, t / top) * r.getHeight(); };
            for (float x = r.getX(); x <= r.getRight(); x += 2) pt (x, ty (ph.proc.rtAt (20.0 * std::pow (1000.0, (x - r.getX()) / r.getWidth()))));
            g.setColour (led); g.strokePath (p, juce::PathStrokeType (2.0f));
            if (meas.valid) for (int k = 0; k < rv::kMBands; ++k) if (meas.t30[(size_t) k] > 0) { const float x = fx (rv::mBandFc (k)), y = ty (meas.t30[(size_t) k]); g.fillRect (x - 2.5f, y - 2.5f, 5.0f, 5.0f); }
            g.setColour (led.withAlpha (0.4f)); g.drawHorizontalLine ((int) ty (D), r.getX(), r.getRight());
            axisF();
        }
        else if (page == 3)
        {   // the input filter (dim) and the master EQ (bright), +-18 dB
            auto yd = [&] (double db) { return r.getCentreY() - (float) (juce::jlimit (-24.0, 24.0, db) / 21.0) * r.getHeight() * 0.5f; };
            rv::InputFilter fi; ph.proc.setInputFilter (fi, 48000); rv::MasterEQ eq; ph.proc.setMasterEQ (eq, 48000);
            juce::Path q; bool sq = false;
            for (float x = r.getX(); x <= r.getRight(); x += 2)
            {
                const double f = 20.0 * std::pow (1000.0, (x - r.getX()) / r.getWidth());
                pt (x, yd (eq.magDb (f, 48000)));
                const float y2 = yd (fi.magDb (f, 48000)); if (! sq) { q.startNewSubPath (x, y2); sq = true; } else q.lineTo (x, y2);
            }
            g.setColour (led.withAlpha (0.45f)); g.strokePath (q, juce::PathStrokeType (1.5f));
            g.setColour (led); g.strokePath (p, juce::PathStrokeType (2.0f));
            g.setColour (led.withAlpha (0.4f)); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
            axisF();
        }
        else if (page == 5)
        {   // ER taps: time (0 .. ER end) vs level
            float tEnd = 20, top = -90; for (auto& t : taps) { tEnd = std::max (tEnd, t.first * 1.1f); top = std::max (top, t.second); }
            for (auto& t : taps)
            {   // 36 dB under the loudest reflection at the bottom
                const float x = r.getX() + t.first / tEnd * r.getWidth(), h = juce::jlimit (0.0f, 1.0f, (t.second - top + 36) / 38) * r.getHeight();
                g.setColour (led); g.drawLine (x, r.getBottom(), x, r.getBottom() - h, 1.5f);
            }
            g.setColour (led.withAlpha (0.55f));
            g.drawText ("0", (int) r.getX() + 2, (int) r.getBottom() - 12, 20, 10, juce::Justification::centredLeft);
            g.drawText (juce::String ((int) tEnd) + "ms", (int) r.getRight() - 48, (int) r.getBottom() - 12, 46, 10, juce::Justification::centredRight);
        }
        else if (page == 4)
        {   // the tail's motion: a sine at the rate, as deep as the depth (2 s across)
            const double rate = ph.value ("tModR"), dep = ph.value ("tModD") / 100.0;
            for (float x = r.getX(); x <= r.getRight(); x += 1.5f)
                pt (x, r.getCentreY() - (float) (std::sin (2 * juce::MathConstants<double>::pi * rate * 2.0 * (x - r.getX()) / r.getWidth()) * dep * 0.45) * r.getHeight());
            g.setColour (led); g.strokePath (p, juce::PathStrokeType (2.0f));
            g.setColour (led.withAlpha (0.55f)); g.drawText ("2 s", (int) r.getRight() - 30, (int) r.getBottom() - 12, 28, 10, juce::Justification::centredRight);
        }
        else
        {   // REVERB / LEVELS: the whole reverb's envelope (0 .. -72 dB) with the build and the tail
            if (envDb.size() > 1 && envMs > 0)
            {
                p.startNewSubPath (r.getX(), r.getBottom()); st = true;
                for (size_t k = 0; k < envDb.size(); ++k)
                    p.lineTo (r.getX() + (float) k / (float) (envDb.size() - 1) * r.getWidth(), r.getY() + juce::jlimit (0.0f, 1.0f, -envDb[k] / 72.0f) * r.getHeight());
                p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath();
                g.setColour (led.withAlpha (0.25f)); g.fillPath (p);
                g.setColour (led); g.strokePath (p, juce::PathStrokeType (1.6f));
                const double st2 = envMs > 4000 ? 1000 : 500;
                g.setColour (led.withAlpha (0.55f));
                for (double t = 0; t <= envMs; t += st2)
                    g.drawText (t >= 1000 ? juce::String (t / 1000.0, 1) + "s" : juce::String ((int) t) + "ms", (int) (r.getX() + t / envMs * r.getWidth()) + 2, (int) r.getBottom() - 12, 40, 10, juce::Justification::centredLeft);
            }
            else { g.setColour (led.withAlpha (0.6f)); g.setFont (theme::mono (11.0f)); g.drawText ("MEASURING...", r, juce::Justification::centred); }
        }
        g.restoreState();
    }
    // the page's readings (measured where it helps)
    juce::String readings() const
    {
        auto s2 = [] (double v) { return v > 0 ? juce::String (v, 2) + "s" : juce::String ("--"); };
        auto hz = [] (float v) { return v >= 1000 ? juce::String (v / 1000, 1) + "k" : juce::String ((int) v); };
        switch (page)
        {
            case 1: return "T30 LO " + s2 (meas.valid ? meas.t30[1] : 0) + "  MID " + s2 (meas.valid ? meas.t30[4] : 0) + "  HI " + s2 (meas.valid ? meas.t30[6] : 0)
                           + "\nSET  LO " + s2 (ph.proc.rtAt (125)) + "  MID " + s2 (ph.proc.rtAt (1000)) + "  HI " + s2 (ph.proc.rtAt (4000));
            case 3: return "OUT LC " + (ph.value ("eqHP") <= 20.5f ? juce::String ("OFF") : hz (ph.value ("eqHP"))) + "  HC " + (ph.value ("eqLP") >= 19900 ? juce::String ("OFF") : hz (ph.value ("eqLP")))
                           + "\nIN  LC " + (ph.value ("iLC") <= 20.5f ? juce::String ("OFF") : hz (ph.value ("iLC"))) + "  HC " + (ph.value ("iHC") >= 19900 ? juce::String ("OFF") : hz (ph.value ("iHC")))
                           + "  TILT " + juce::String (ph.value ("iTilt"), 1);
            case 4: return "RATE " + juce::String (ph.value ("tModR"), 2) + "HZ  DEPTH " + juce::String ((int) ph.value ("tModD")) + "%\nER WANDER " + juce::String ((int) ph.value ("wander")) + "%";
            case 5: return "FIRST " + juce::String (ph.value ("first"), 1) + "MS  TAPS " + juce::String ((int) taps.size()) + "\nER LVL " + juce::String (ph.value ("level"), 1) + "DB  WIDTH " + juce::String ((int) ph.value ("width")) + "%";
            case 6: return "MIX " + juce::String ((int) ph.value ("mix")) + "%  BAL " + juce::String ((int) ph.value ("erBal")) + "  OUT " + juce::String (ph.value ("out"), 1) + "DB"
                           + "\nC80 " + (meas.valid ? juce::String (meas.c80, 1) + "DB" : juce::String ("--")) + "  D50 " + (meas.valid ? juce::String ((int) std::round (meas.d50 * 100)) + "%" : juce::String ("--"));
            default: return "T30 " + s2 (meas.valid ? meas.t30Wide : 0) + "  EDT " + s2 (meas.valid ? meas.edtWide : 0)
                            + "\nPRE " + juce::String ((int) ph.value ("predelay")) + "MS  C80 " + (meas.valid ? juce::String (meas.c80, 1) + "DB" : juce::String ("--"));
        }
    }
    juce::String progName() const
    {
        if (ph.proc.currentPreset >= 0 && ph.proc.currentPreset < (int) presets().size()) return juce::String::fromUTF8 (presets()[(size_t) ph.proc.currentPreset].name);
        return ph.proc.currentUserPreset.isNotEmpty() ? ph.proc.currentUserPreset : juce::String ("USER");
    }
    void updateHelp (int i) { if (i >= 0 && i < (int) ctls.size() && ctls[(size_t) i].valid()) { setHelp (*this, ctls[(size_t) i].label.toRawUTF8(), ctls[(size_t) i].ja, ctls[(size_t) i].en);
                                getProperties().set ("hids", ctls[(size_t) i].ids.joinIntoString (",")); getProperties().set ("hlabel", ctls[(size_t) i].label); } }
    void gest (int i, bool b) { for (auto& id : ctls[(size_t) i].ids) if (auto* p = ph.get (id)) { if (b) p->beginChangeGesture(); else p->endChangeGesture(); } }
    ParamHelp& ph; std::vector<std::pair<juce::String, std::vector<juce::String>>> pages; std::vector<MCtl> ctls;
    std::vector<juce::Rectangle<float>> pageR, fadR, capR, softR;
    int page = 0, sel = 0, drag = -1; float startN = 0, lastY = 0;
};

// ---------------------------------------------------------------------------
//  REMOTE 2: modern - one row of macro knobs (the big decay in the middle); the editor puts the
//  RT (%) + EQ (dB) graph under it
// ---------------------------------------------------------------------------
class ModernRemote : public juce::Component
{
public:
    explicit ModernRemote (ParamHelp& h) : ph (h)
    {
        const char* keys[] = { "predelay", "attack", "size", "distance", "decay", "bright", "width", "bal", "mix" };
        for (auto* k : keys) { auto* kn = knobs.add (new Knob (ph, k, juce::String (k) == "decay")); addAndMakeVisible (kn); }
    }
    void repaintKnobs() { for (auto* k : knobs) k->repaint(); }
    void resized() override
    {
        auto r = getLocalBounds();
        const int big = 150, n = knobs.size(), small = (r.getWidth() - big) / (n - 1);
        int x = 0;
        for (int i = 0; i < n; ++i)
        {
            const bool isBig = i == 4; const int w = isBig ? big : small;
            knobs[i]->setBounds (isBig ? juce::Rectangle<int> (x, 0, w, r.getHeight()) : juce::Rectangle<int> (x, 18, w, r.getHeight() - 26).reduced (4, 0));
            x += w;
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (theme::panel.brighter (0.06f), 0, 0, theme::panel, 0, b.getHeight(), false));
        g.fillRoundedRectangle (b, 10.0f);
    }
private:
    ParamHelp& ph; juce::OwnedArray<Knob> knobs;
};
