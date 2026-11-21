#pragma once
// Reverb Anatomy Lab: page tabs, reverb-time graph, curve graphs (input filter / master EQ), measurement table.
// Copyright (c) 2026 Johan Plus Kei. All rights reserved.
#include "Views.h"

// ===========================================================================
//  Page tabs (ER / TAIL / I/O / EQ), kept in uiState "page"
// ===========================================================================
// each part's colour (the detail panel's frames and the titles' underline)
inline juce::Colour sectionColour (int id)
{
    switch (id) { case 3: return juce::Colour (0xff7d8fae); case 0: return juce::Colour (0xffd0893a); case 1: return juce::Colour (0xff3d7fd6);
                  case 7: return juce::Colour (0xff9a6cc8); case 8: return juce::Colour (0xffb07a3a); case 9: return juce::Colour (0xff5d7fb0); case 10: return juce::Colour (0xff4d9a9a); case 11: return juce::Colour (0xff6a9ab0); case 4: return juce::Colour (0xff3fa36a); case 5: return juce::Colour (0xffc65a50); default: return juce::Colour (0xff7a5cc0); }
}
class PageTabs : public juce::Component, private juce::Timer
{
public:
    PageTabs (ParamHelp& h) : ph (h) {}
    std::function<void()> onChange;
    std::function<void (int page, juce::Rectangle<int> tab)> onLongPress;       // the section's presets
    std::function<void()> onDoubleClick;                                         // a title double-clicked: the detail panel opens / closes
    // ids: 0 ER, 1 TAIL, 3 INPUT, 4 OUTPUT, 5 DYN, 6 MOD.
    // page() = the part the monitors follow (ER or TAIL: a click on its title)
    int page() const { const int p = (int) ph.proc.uiState.getProperty ("page", 1); return (p == 1 || p == 2) ? 1 : 0; }
    // the detail panel: 0 = closed, 1 = EDIT (INPUT | ER | TAIL | COLOR / OUTPUT, all at once), 2 = DYN + MOD
    int shown() const { return (int) ph.proc.uiState.getProperty ("detShow", 1) > 0 ? 1 : 0; }      // (0.10.9) EDIT only: SOURCE / DYN / MOD swap the deck        // 3 = SOURCE (0.9.0), 4 = MOD (0.10.8; 2 = DYN)
    // the EDIT / DYN+MOD buttons: a click opens that screen, the same button again closes the panel
    void toggle (int s) { ph.proc.uiState.setProperty ("detShow", shown() == s ? 0 : s, nullptr); repaint(); }
    bool open() const { return shown() != 0; }
    int mode() const { return 0; }           // which titles the row shows (closed: EDIT's)
    void setMode (int m) { ph.proc.uiState.setProperty ("detShow", m == 1 ? 2 : 1, nullptr); repaint(); }
    bool shows (int id) const { return open() && (mode() == 0 ? (id == 0 || id == 1 || id == 3 || id == 4) : mode() == 1 ? ((id == 5 && shown() == 2) || (id == 6 && shown() == 4)) : id >= 8); }
    // the titles sit over their columns (x in this component's coordinates), per screen
    void setColumns (int m, std::vector<std::pair<int, juce::Rectangle<int>>> c) { cols[juce::jlimit (0, 2, m)] = std::move (c); repaint(); }
    int count() const { return (int) cols[mode()].size(); }
    int idOf (int k) const { return cols[mode()][(size_t) juce::jlimit (0, count() - 1, k)].first; }
    juce::Rectangle<float> tabRectF (int k) const
    {
        if (count() == 0) return getLocalBounds().toFloat();
        const auto r = cols[mode()][(size_t) juce::jlimit (0, count() - 1, k)].second; return { (float) r.getX(), 0.0f, (float) r.getWidth(), (float) getHeight() };
    }
    juce::Rectangle<int> tabRect (int k) const { return tabRectF (k).toNearestInt(); }
    int tabAt (float x) const { for (int k = 0; k < count(); ++k) if (x < tabRectF (k).getRight() + 3) return k; return count() - 1; }
    // a click on a title: ER / TAIL -> the monitors follow that part (the others have nothing to select)
    void choose (int k)
    {
        const int id = idOf (k);
        if (id == 0 || id == 1) ph.proc.uiState.setProperty ("page", id, nullptr);
    }
    void paint (juce::Graphics& g) override
    {
        auto name = [this] (int i) -> juce::String
        {
            switch (i) { case 0: return ph.tr ("ER（初期反射）", "ER (early)"); case 1: return ph.tr ("TAIL（後部残響）", "TAIL (late)"); case 3: return "INPUT"; case 4: return "COLOR / OUTPUT";
                         case 8: return ph.tr ("ROOM A", "ROOM A"); case 9: return ph.tr ("ROOM B", "ROOM B"); case 10: return ph.tr ("IR A", "IR A");
                         case 11: return ph.tr ("IR B", "IR B"); case 12: return ph.tr ("いま鳴っているもの", "NOW PLAYING");
                         case 5: return ph.tr ("DYN（ダッキング・ゲート）", "DYN (ducking, gate)"); default: return ph.tr ("MOD（LFO・エンベロープ・マトリクス）", "MOD (LFO, envelope, matrix)"); }
        };
        for (int k = 0; k < count(); ++k)
        {
            const int i = idOf (k); const bool main = i == 0 || i == 1;
            auto r = tabRectF (k).reduced (0, 1);
            const bool on = main && page() == i, held = pressK == k && pressing;
            // (0.10.12) drawn like the preset box: its ground, frame, font and arrow; the part the monitors follow has an accent frame and tint
            g.setColour (theme::comboBg()); g.fillRoundedRectangle (r, 3.0f);
            if (on) { g.setColour (theme::accent.withAlpha (0.14f)); g.fillRoundedRectangle (r, 3.0f); }
            g.setColour (on ? theme::accent : main ? theme::accent.withAlpha (0.5f) : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, on ? 1.8f : 1.0f);
            if (held) { g.setColour (theme::text); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.6f); }
            if (! main) { g.setColour (sectionColour (i).withAlpha (0.10f)); g.fillRoundedRectangle (r, 3.0f); }
            g.setColour (sectionColour (i)); g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 3.0f).reduced (4.0f, 0), 1.5f);
            if (i == 4) { g.setColour (sectionColour (7)); g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 3.0f).withWidth (r.getWidth() * 0.4f).reduced (2.0f, 0), 1.5f); }
            // ER / TAIL: a power switch at the left of the title
            const char* pid = powerId (i); const bool pwr = pid == nullptr || ph.value (pid) > 0.5f;
            if (pid != nullptr)
            {
                auto pr = powerRect (k);
                g.setColour (pwr ? juce::Colour (0xff3fae6a) : theme::faint.withAlpha (0.35f)); g.fillEllipse (pr);
                g.setColour (theme::text); g.drawEllipse (pr, 1.3f);
                g.setColour (pwr ? juce::Colours::white : theme::muted); g.drawLine (pr.getCentreX(), pr.getY() + 3.5f, pr.getCentreX(), pr.getCentreY() - 0.5f, 1.6f);
            }
            const juce::String st = status (i);
            auto tr = r.withTrimmedLeft (pid != nullptr ? 26.0f : 10.0f).withTrimmedRight (20.0f);
            if (st.isNotEmpty())
            {
                g.setFont (theme::ui (9.5f, true));
                const float sw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), st) + 12;
                auto chip = juce::Rectangle<float> (tr.getRight() - sw, r.getCentreY() - 8, sw, 16);
                g.setColour (theme::accent.withAlpha (0.16f)); g.fillRoundedRectangle (chip, 3.0f);
                g.setColour (theme::accent); g.drawText (st, chip, juce::Justification::centred);
                tr = tr.withRight (chip.getX() - 4);
            }
            g.setColour (theme::text); g.setFont (theme::ui (12.0f));
            g.drawFittedText (name (i), tr.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
            if (! pwr) { g.setColour (theme::comboBg().withAlpha (0.55f)); g.fillRoundedRectangle (r.withTrimmedLeft (26).withTrimmedRight (18), 2.0f); }
            // this part's presets: the mark on the right (click it, or long-press / right-click the title)
            if (i < 8)
            {   // the preset box's arrow
                const auto pr = presetRect (k); const float cx = pr.getCentreX(), cy = pr.getCentreY();
                juce::Path a; a.startNewSubPath (cx - 4, cy - 2); a.lineTo (cx, cy + 2); a.lineTo (cx + 4, cy - 2);
                g.setColour (theme::muted); g.strokePath (a, juce::PathStrokeType (1.6f));
            }
        }
    }
    static const char* powerId (int id) { return id == 0 ? "erOn" : id == 1 ? "tOn" : nullptr; }
    juce::Rectangle<float> powerRect (int k) const { auto r = tabRectF (k).reduced (0, 1); return { r.getX() + 8, r.getCentreY() - 7, 14, 14 }; }
    juce::Rectangle<float> presetRect (int k) const { auto r = tabRectF (k); return r.withTrimmedLeft (r.getWidth() - 20); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int pk = tabAt (e.position.x); if (pk < 0) return;
        const char* pid = powerId (idOf (pk));
        if (pid != nullptr && ! e.mods.isPopupMenu() && powerRect (pk).expanded (4).contains (e.position))
        {
            ph.proc.undo.beginNewTransaction();
            ph.setGesture (pid, ph.value (pid) > 0.5f ? 0.0f : 1.0f);
            pressing = false; repaint(); if (onChange) onChange(); return;
        }
        if (e.getNumberOfClicks() == 2 && ! e.mods.isPopupMenu() && ! presetRect (pk).contains (e.position))
        { pressing = false; stopTimer(); repaint(); if (onDoubleClick) onDoubleClick(); return; }
        pressK = pk; pressing = true; repaint();
        if (e.mods.isPopupMenu() || presetRect (pk).contains (e.position)) { fire(); return; }     // right-click / the mark: this title's presets at once
        startTimer (450);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (pressing && e.getDistanceFromDragStart() > 8) { pressing = false; stopTimer(); repaint(); } }
    void mouseUp (const juce::MouseEvent&) override
    {
        const bool click = pressing; const int k = pressK; pressing = false; stopTimer(); repaint();
        if (click && k >= 0) { choose (k); repaint(); if (onChange) onChange(); }
    }
    juce::String status (int pg) const
    {
        if (pg == 0)
        {
            if (ph.value ("erOn") < 0.5f) return "OFF";
            const int s = (int) ph.value ("src");
            if (s == 2) { const int m = (int) ph.value ("irMode"); return juce::String (ph.value ("erIrSel") > 0.5f ? "IR B" : "IR A") + (m == 1 ? " CONV" : m == 2 ? " HYBRID" : " TAPS"); }
            return s == 1 ? juce::String ("ALGO") : juce::String (ph.value ("erRoomSel") > 0.5f ? "ROOM B" : "ROOM A");
        }
        if (pg == 1)
        {
            if (ph.value ("tOn") < 0.5f) return "OFF";
            const int ts = (int) ph.value ("tSrc"); const bool room = ts == 0 && ph.value ("tRoom") > 0.5f;
            const char* eng[] = { "HALL", "PLATE", "SPRING", "AMBIENT" };
            const juce::String e = eng[juce::jlimit (0, 3, (int) ph.value ("tEng"))];
            const juce::String ir = ph.value ("tIrSel") > 0.5f ? "IR B" : "IR A";
            return ts == 1 ? ir : ts == 2 ? ir + "+" + e : (room ? juce::String (ph.value ("tRoomSel") > 0.5f ? "ROOM B " : "ROOM A ") : juce::String()) + e;
        }
        if (pg == 5)
        {
            const int d = (int) ph.value ("dkMode"); const bool gt = ph.value ("gtOn") > 0.5f;
            const char* dn[] = { "", "DUCK", "DUCK MB", "DUCK SP" };
            juce::String s = dn[juce::jlimit (0, 3, d)];
            if (gt) s << (s.isEmpty() ? "" : "+") << "GATE";
            return s.isEmpty() ? juce::String ("OFF") : s;
        }
        if (pg == 6)
        {
            int k = 0; for (int i = 1; i <= 4; ++i) { const juce::String n (i); if ((int) ph.value (("m" + n + "Src").toRawUTF8()) > 0 && ph.value (("m" + n + "Amt").toRawUTF8()) != 0) ++k; }
            juce::String s = k > 0 ? juce::String (k) + " MOD" : juce::String();
            if (ph.value ("chMix") > 0) s << (s.isEmpty() ? "" : "+") << "CHO";
            return s.isEmpty() ? juce::String ("OFF") : s;
        }
        return {};
    }
private:
    void timerCallback() override { stopTimer(); if (pressing) fire(); }
    void fire()
    {
        pressing = false; repaint();
        if (onLongPress && pressK >= 0 && idOf (pressK) < 8) onLongPress (idOf (pressK), tabRect (pressK));
    }
    ParamHelp& ph; int pressK = -1; bool pressing = false;
    std::vector<std::pair<int, juce::Rectangle<int>>> cols[3];
};

// ===========================================================================
//  The detail panel's two screens, as tabs standing on its left edge:
//  REVERB (INPUT | ER | TAIL | OUTPUT) and DYN / MOD
// ===========================================================================
class DetailSwitch : public juce::Component
{
public:
    DetailSwitch (ParamHelp& h, PageTabs& t) : ph (h), tabs (t) {}
    std::function<void()> onChange;
    juce::Rectangle<float> half (int i) const { auto r = getLocalBounds().toFloat(); const float h = r.getHeight() * 0.5f; return { r.getX(), r.getY() + i * h, r.getWidth(), h }; }
    void paint (juce::Graphics& g) override
    {
        const juce::String t[2] = { ph.tr ("リバーブ", "REVERB"), "DYN / MOD" };
        for (int i = 0; i < 2; ++i)
        {
            auto r = half (i).reduced (0, 1.5f); const bool on = tabs.mode() == i;
            g.setColour (on ? theme::accent : theme::panel); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? theme::accent : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getCentreX(), r.getCentreY()));
            auto rr = juce::Rectangle<float> (r.getHeight(), r.getWidth()).withCentre (r.getCentre());
            g.setColour (on ? theme::onAccent : theme::muted); g.setFont (theme::ui (11.0f, true));
            g.drawText (t[i], rr, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = e.position.y < getHeight() * 0.5f ? 0 : 1;
        if (i == tabs.mode()) return;
        tabs.setMode (i); repaint(); if (onChange) onChange();
    }
private:
    ParamHelp& ph; PageTabs& tabs;
};

// ===========================================================================
//  The tail's source, laid out like the ER's: ROOM | ALGO | IR | IR+ALGO
//  (ROOM = the ALGO tail driven by the ER's room: tSrc 0 + tRoom 1)
// ===========================================================================
class TailSourceSeg : public juce::Component
{
public:
    TailSourceSeg (ParamHelp& h) : ph (h) {}
    static int modeOf (ParamHelp& ph) { const int s = (int) ph.value ("tSrc"); return s == 0 ? (ph.value ("tRoom") > 0.5f ? 0 : 1) : s + 1; }
    void paint (juce::Graphics& g) override
    {
        const char* items[] = { "ROOM", "ALGO", "IR", "+ALGO" };          // (0.10.8) IR and +ALGO share one slot's width: the whole is as wide as the ER's ROOM | ALGO | IR
        static const float edge[5] = { 0.0f, 1.0f / 3, 2.0f / 3, 5.0f / 6, 1.0f };
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const int sel = modeOf (ph);
        for (int i = 0; i < 4; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + edge[i] * r.getWidth(), r.getY(), (edge[i + 1] - edge[i]) * r.getWidth(), r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (theme::accent); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : theme::muted); g.setFont (theme::ui (i >= 2 ? 9.0f : 11.0f, i == sel));
            g.drawFittedText (items[i], c.toNearestInt(), juce::Justification::centred, 1, 0.55f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const float fx = (float) e.x / (float) std::max (1, getWidth());
        const int i = fx < 1.0f / 3 ? 0 : fx < 2.0f / 3 ? 1 : fx < 5.0f / 6 ? 2 : 3;
        if (i == modeOf (ph)) return;
        ph.proc.undo.beginNewTransaction();
        ph.setGesture ("tRoom", i == 0 ? 1.0f : 0.0f);
        ph.setGesture ("tSrc", (float) (i <= 1 ? 0 : i - 1));
        repaint(); if (onChange) onChange();
    }
    std::function<void()> onChange;
private:
    ParamHelp& ph;
};

// ===========================================================================
//  The spectrum monitor's bar: which curve is edited over the spectra
//  (reverb time, master EQ, input filter, ER filter), kept in uiState "layer"
// ===========================================================================
class LayerBar : public juce::Component
{
public:
    LayerBar (ParamHelp& h) : ph (h) {}
    std::function<void()> onChange;
    static constexpr int kLayers = 6;
    int layer() const { const int l = juce::jlimit (0, 5, (int) ph.proc.uiState.getProperty ("layer", 5)); return l == 0 || l == 1 ? 0 : 5; }   // 0 (and the old 1) = RT + EQ overlaid, anything else = the six panels
    void setLayer (int l) { ph.proc.uiState.setProperty ("layer", l, nullptr); repaint(); if (onChange) onChange(); }
    void paint (juce::Graphics& g) override
    {
        const bool small = getWidth() < 430;
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true));
        float x = 0;
        if (getWidth() >= 640) { g.drawText (ph.tr ("スペアナ＋編集", "SPECTRUM + EDIT"), 0, 0, 110, getHeight(), juce::Justification::centredLeft); x = 112; }
        const char* ja[] = { "RT（青）＋ MASTER EQ（黄）", "", "", "", "", "6 面：フィルター・RT・EQ" };
        const char* en[] = { "RT (blue) + MASTER EQ (yellow)", "", "", "", "", "6 panels: filters, RT, EQ" };
        const char* sh[] = { "RT+EQ", "", "", "", "", "6 PANELS" };
        r.clear();
        // signal order on the bar too: input filter, ER filter, RT, master EQ
        for (int i : { 5, 0 })
        {
            const juce::String t = small ? juce::String (sh[i]) : ph.tr (ja[i], en[i]);
            const float w = juce::GlyphArrangement::getStringWidth (theme::ui (10.5f, true), t) + 18;
            juce::Rectangle<float> b (x, 2, w, (float) getHeight() - 4); r.push_back ({ b, i }); x += w + 3;
            const bool on = layer() == i;
            g.setColour (on ? theme::text : theme::shade (0.3f)); g.fillRoundedRectangle (b, 3.0f);
            g.setColour (on ? theme::panel : theme::muted); g.setFont (theme::ui (10.5f, true)); g.drawText (t, b, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { for (auto& [b, i] : r) if (b.contains (e.position)) { setLayer (i); return; } }
private:
    ParamHelp& ph; std::vector<std::pair<juce::Rectangle<float>, int>> r;
};

// helpers: log-frequency axis
struct FreqAxis
{
    juce::Rectangle<float> r; double lo = 20, hi = 20000;
    float x (double f) const { return r.getX() + (float) (std::log (f / lo) / std::log (hi / lo)) * r.getWidth(); }
    double f (float px) const { return lo * std::pow (hi / lo, juce::jlimit (0.0, 1.0, (double) ((px - r.getX()) / r.getWidth()))); }
    void grid (juce::Graphics& g) const
    {
        g.setFont (theme::mono (9.0f));
        for (double v : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) x (v), r.getY(), r.getBottom());
            g.setColour (theme::faint); g.drawText (v >= 1000 ? juce::String ((int) (v / 1000)) + "k" : juce::String ((int) v), (int) x (v) - 16, (int) r.getBottom() + 2, 32, 12, juce::Justification::centred);
        }
    }
};
// a parameter dragged by a graph: one undo step per drag, host gestures
struct GraphDrag
{
    static void begin (ParamHelp& ph, std::initializer_list<const char*> ids) { ph.proc.undo.beginNewTransaction(); for (auto id : ids) if (auto* p = ph.get (id)) p->beginChangeGesture(); }
    static void end (ParamHelp& ph, std::initializer_list<const char*> ids) { for (auto id : ids) if (auto* p = ph.get (id)) p->endChangeGesture(); }
};

// ===========================================================================
//  Graphs with draggable points, operated like EQ Anatomy Lab:
//  drag = frequency / level (Shift = fine), wheel = Q, arrow keys move the selected point
//  (left / right = frequency, up / down = level, Shift = fine), Option+click = back to the default,
//  double-click = band on / off (EQ) or back to x1 (reverb time). The selected point's values
//  appear in a small window next to it (the editor places it: selIds() / nodePos()).
//  Behind the curves: the reverb input before the input filter (amber) and the wet output (cyan).
// ===========================================================================
class NodeGraph : public juce::Component
{
public:
    NodeGraph (ParamHelp& h) : ph (h) { setWantsKeyboardFocus (true); }
    const std::vector<float>* specIn = nullptr; const std::vector<float>* specWet = nullptr;     // 121 points, 20 Hz - 20 kHz, dBFS
    std::function<void()> onSelect;
    int sel = -1;
    virtual int count() const = 0;
    virtual juce::Point<float> pos (int i) const = 0;
    virtual juce::StringArray ids (int i) const = 0;          // the selected point's parameters (shown in the small window)
    virtual juce::String title (int i) const = 0;
    juce::Point<float> nodePos (int i) const { return pos (i); }
    juce::Rectangle<float> plotArea() const { return ax.r; }
    void setPlot (juce::Rectangle<float> r) { ax.r = r; }
    virtual juce::StringArray gestureIds (int i) const { return ids (i); }
    // points not in use are hidden until added (double-click on an empty spot); addAt returns the new point or -1
    virtual bool shown (int) const { return true; }
    virtual int addAt (juce::Point<float>) { return -1; }
    virtual bool clickExtra (juce::Point<float>, juce::Component*) { return false; }      // e.g. the EQ scale button
    virtual void emptyClick (juce::Point<float>) {}                                        // a click on no point (e.g. on a curve: choose it)
    // the "added" marks (a point at 0 dB / 100 % still shows once added), bit i per point
    bool bit (const char* key, int i) const { return (((int) ph.proc.uiState.getProperty (key, 0)) >> i) & 1; }
    void setBit (const char* key, int i, bool on) const
    { int m = (int) ph.proc.uiState.getProperty (key, 0); m = on ? (m | (1 << i)) : (m & ~(1 << i)); ph.proc.uiState.setProperty (key, m, nullptr); }
    // reverb time = blue, EQ = yellow; the input spectrum stays out of both (violet)
    static juce::Colour rtCol() { return theme::lightPanels ? juce::Colour (0xff1f6fd1) : juce::Colour (0xff5aa9ff); }
    static juce::Colour eqCol() { return theme::lightPanels ? juce::Colour (0xffc08a00) : juce::Colour (0xffffcc4d); }
    static juce::Colour inCol() { return theme::lightPanels ? juce::Colour (0xff8a64b8) : juce::Colour (0xffb592e0); }
    virtual void dragTo (int i, juce::Point<float> p, juce::Point<float> start, bool fine) = 0;
    virtual void qStep (int i, float steps) = 0;
    virtual void resetPoint (int i) = 0;
    virtual void dbl (int i) = 0;
    virtual void nudge (int i, int dx, int dy, bool fine) = 0;
    void drawNode (juce::Graphics& g, int i, juce::Colour c, const juce::String& label, bool on = true) const
    {
        const auto p = pos (i); const float r = i == sel ? 8.0f : 6.5f;
        g.setColour (on ? c : c.withAlpha (0.35f)); g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
        if (i == sel) { g.setColour (theme::text); g.drawEllipse (p.x - r - 2, p.y - r - 2, 2 * r + 4, 2 * r + 4, 1.5f); }
        g.setColour (theme::lightPanels ? juce::Colours::white : juce::Colour (0xff101418)); g.setFont (theme::ui (9.0f, true)); g.drawText (label, (int) (p.x - r), (int) (p.y - r), (int) (2 * r), (int) (2 * r), juce::Justification::centred);
    }
protected:
    void drawSpectra (juce::Graphics& g) const
    {
        auto one = [&] (const std::vector<float>* v, juce::Colour c)
        {
            if (v == nullptr || v->size() < 2) return;
            juce::Path p; p.startNewSubPath (ax.r.getX(), ax.r.getBottom());
            for (size_t q = 0; q < v->size(); ++q)
            {
                const double f = 20 * std::pow (1000.0, (double) q / (double) (v->size() - 1));
                const float t = juce::jlimit (0.0f, 1.0f, ((*v)[q] + 90.0f) / 84.0f);
                p.lineTo (ax.x (f), ax.r.getBottom() - t * ax.r.getHeight());
            }
            p.lineTo (ax.r.getRight(), ax.r.getBottom()); p.closeSubPath();
            g.setColour (c.withAlpha (0.16f)); g.fillPath (p);
            g.setColour (c.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (0.8f));
        };
        one (specIn, inCol()); one (specWet, theme::accent);
        g.setFont (theme::ui (9.0f, true));
        g.setColour (inCol()); g.drawText (ph.tr ("入力（フィルター前）", "Input (pre-filter)"), (int) ax.r.getRight() - 210, (int) ax.r.getBottom() - 14, 120, 12, juce::Justification::centredRight);
        g.setColour (theme::accent); g.drawText (ph.tr ("リバーブ", "Reverb"), (int) ax.r.getRight() - 86, (int) ax.r.getBottom() - 14, 80, 12, juce::Justification::centredRight);
    }
    int pick (juce::Point<float> p) const { int best = -1; float d0 = 14; for (int i = 0; i < count(); ++i) { if (! shown (i)) continue; const float d = pos (i).getDistanceFrom (p); if (d < d0) { d0 = d; best = i; } } return best; }
    void select (int i) { if (i != sel) { sel = i; if (onSelect) onSelect(); } repaint(); }
    void gesture (int i, bool begin) { for (auto& id : gestureIds (i)) if (auto* p = ph.get (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); } }
public:
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.mods.isPopupMenu()) { dragging = -1; select (-1); return; }      // right-click: close the small window
        if (clickExtra (e.position, this)) { dragging = -1; return; }
        int i = pick (e.position); dragging = i;
        if (i < 0 && e.getNumberOfClicks() == 1) emptyClick (e.position);
        if (i < 0 && e.getNumberOfClicks() > 1 && ax.r.contains (e.position))
        {   // double-click on an empty spot: add a point there
            ph.proc.undo.beginNewTransaction();
            const int n = addAt (e.position);
            if (n >= 0) { select (n); dragging = n; gesture (n, true); startPos = pos (n); repaint(); if (onSelect) onSelect(); }
            return;
        }
        if (i < 0) { select (-1); return; }
        select (i);
        ph.proc.undo.beginNewTransaction(); gesture (i, true);
        if (e.mods.isAltDown()) { resetPoint (i); dragging = -1; gesture (i, false); }
        else if (e.getNumberOfClicks() > 1) { dbl (i); dragging = -1; gesture (i, false); }
        startPos = pos (i); repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        const bool fine = e.mods.isShiftDown();
        const auto target = fine ? startPos + (e.position - e.mouseDownPosition) * 0.2f : e.position;
        dragTo (dragging, target, startPos, fine); repaint(); if (onSelect) onSelect();
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging >= 0) gesture (dragging, false); dragging = -1; repaint(); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        int i = pick (e.position); if (i < 0) i = sel; if (i < 0 || ! shown (i)) return;
        select (i); ph.proc.undo.beginNewTransaction(); gesture (i, true); qStep (i, w.deltaY * 4); gesture (i, false); repaint(); if (onSelect) onSelect();
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (sel < 0) return false;
        const int code = k.getKeyCode(); const bool fine = k.getModifiers().isShiftDown();
        int dx = 0, dy = 0;
        if (code == juce::KeyPress::leftKey) dx = -1; else if (code == juce::KeyPress::rightKey) dx = 1;
        else if (code == juce::KeyPress::upKey) dy = 1; else if (code == juce::KeyPress::downKey) dy = -1;
        else return false;
        ph.proc.undo.beginNewTransaction(); gesture (sel, true); nudge (sel, dx, dy, fine); gesture (sel, false); repaint(); if (onSelect) onSelect();
        return true;
    }
protected:
    ParamHelp& ph; FreqAxis ax; int dragging = -1; juce::Point<float> startPos;
    static float stepFreq (float f, int dx, bool fine) { return (float) juce::jlimit (20.0, 20000.0, f * std::pow (2.0, dx * (fine ? 1.0 / 48 : 1.0 / 12))); }
};

// ===========================================================================
//  Reverb time per frequency: the curve set, the measured T30 / EDT,
//  EQ mode (5 points: low shelf, 3 bells, high shelf; up / down = multiple of Decay, wheel = Q)
//  or Multiband (bars = multiple per band, orange marks = crossovers)
// ===========================================================================
class RTGraph : public NodeGraph, public juce::SettableTooltipClient
{
public:
    bool tiled = false;      // (0.10.18) in the six panels: the same plot margins as the filter graphs (50 Hz / 10 kHz line up)
    RTGraph (ParamHelp& h) : NodeGraph (h) {}
    rv::Measure meas, irM;                 // (0.10.10) irM: the compared IR (TAIL / ALL), drawn as target diamonds
    int count() const override { return mode() == 0 ? 5 : nBands() + nBands() - 1; }
    juce::Point<float> pos (int i) const override
    {
        if (mode() == 0) return { ax.x (ph.value (fid (i))), ty (baseAt (ph.value (fid (i))) * ph.value (mid (i))) };
        const int nb = nBands();
        if (i < nb)
        {
            const double a = i == 0 ? 20.0 : xo (i - 1), z = i == nb - 1 ? 20000.0 : xo (i);
            return { 0.5f * (ax.x (a) + ax.x (z)), ty (baseAt (std::sqrt (a * z)) * ph.value (mbm (i))) };
        }
        return { ax.x (xo (i - nb)), ax.r.getBottom() - 8 };
    }
    juce::StringArray ids (int i) const override
    {
        if (i < 0) return {};
        if (mode() == 0) return { fid (i), mid (i), qid (i) };
        return i < nBands() ? juce::StringArray { mbm (i) } : juce::StringArray { xid (i - nBands()) };
    }
    juce::String title (int i) const override
    {
        if (mode() == 0) return ph.tr ("点 ", "Point ") + juce::String (i + 1) + (i == 0 ? ph.tr ("（低域シェルフ）", " (low shelf)") : i == 4 ? ph.tr ("（高域シェルフ）", " (high shelf)") : ph.tr ("（ベル）", " (bell)"));
        return i < nBands() ? ph.tr ("帯域 ", "Band ") + juce::String (i + 1) : ph.tr ("クロスオーバー ", "Crossover ") + juce::String (i - nBands() + 1);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (b, 4.0f);
        ax.r = tiled ? b.withTrimmedLeft (30).withTrimmedBottom (16).withTrimmedTop (18).withTrimmedRight (8) : b.withTrimmedLeft (40).withTrimmedBottom (16).withTrimmedTop (18).withTrimmedRight (10);
        drawSpectra (g);
        ax.grid (g);
        paintAxis (g);
        paintCurves (g);
        paintLegend (g, ax.r.getX() + 6);
    }
    // left axis: reverb time as a percentage of Decay (100 % a quarter down from the top; one octave per quarter)
    void paintAxis (juce::Graphics& g) const
    {
        g.setFont (theme::mono (9.0f));
        for (double m : { 2.0, 1.5, 1.0, 0.7, 0.5, 0.35, 0.25, 0.125 })
        {
            const float y = tm (m);
            g.setColour (m == 1.0 ? rtCol().withAlpha (0.45f) : theme::grid); g.drawHorizontalLine ((int) y, ax.r.getX(), ax.r.getRight());
            g.setColour (m == 1.0 ? rtCol() : rtCol().withAlpha (0.7f));
            if (tiled && m == 0.125) continue;                  // (narrow margin: the bottom 12.5 is left out)
            g.drawText (m == 0.125 ? juce::String ("12.5") : juce::String ((int) std::round (m * 100)), tiled ? (int) ax.r.getX() - 29 : (int) ax.r.getX() - 38, (int) y - 6, tiled ? 26 : 33, 12, juce::Justification::centredRight);
        }
        g.setColour (rtCol()); g.setFont (theme::ui (9.0f, true));
        g.drawText ("RT %", tiled ? (int) ax.r.getX() - 32 : (int) ax.r.getX() - 40, (int) ax.r.getY() - 16, tiled ? 34 : 40, 12, juce::Justification::centredRight);
        g.setFont (theme::mono (8.5f)); g.setColour (rtCol().withAlpha (0.8f));
        g.drawText ("= " + juce::String (decay(), 2) + " s" + (ph.proc.tailFromRoom() ? ph.tr ("（部屋 1 kHz）", " (room, 1 kHz)") : juce::String()), (int) ax.r.getX() + 4, (int) tm (1.0) - 13, 150, 12, juce::Justification::centredLeft);
    }
    void paintCurves (juce::Graphics& g) const
    {
        const auto col = rtCol();
        if (ph.proc.tailFromRoom())
        {   // ROOM: the room's own curve (dashed); the solid line = the room times the trim points
            juce::Path rp; bool s0 = false;
            for (float px = ax.r.getX(); px <= ax.r.getRight(); px += 2)
            { const float y = ty (ph.proc.roomRTAt (ax.f (px))); if (! s0) { rp.startNewSubPath (px, y); s0 = true; } else rp.lineTo (px, y); }
            juce::Path d; const float dl[] = { 5.0f, 4.0f }; juce::PathStrokeType (1.3f).createDashedStroke (d, rp, dl, 2);
            g.setColour (col.withAlpha (0.55f)); g.fillPath (d);
        }
        juce::Path p; bool st = false;
        for (float px = ax.r.getX(); px <= ax.r.getRight(); px += 2)
        {
            const float y = ty (ph.proc.rtAt (ax.f (px)));
            if (! st) { p.startNewSubPath (px, y); st = true; } else p.lineTo (px, y);
        }
        g.setColour (col); g.strokePath (p, juce::PathStrokeType (2.0f));
        if (meas.valid)
            for (int k = 0; k < rv::kMBands; ++k)
            {
                const float x = ax.x (rv::mBandFc (k));
                if (meas.t30[(size_t) k] > 0) { g.setColour (col.brighter (0.6f)); g.fillRect (x - 3.5f, ty (meas.t30[(size_t) k]) - 3.5f, 7.0f, 7.0f); }
                if (meas.edt[(size_t) k] > 0) { g.setColour (theme::text.withAlpha (0.7f)); g.drawEllipse (x - 3.5f, ty (meas.edt[(size_t) k]) - 3.5f, 7, 7, 1.2f); }
            }
        if (irM.valid)
            for (int k = 0; k < rv::kMBands; ++k)
                if (irM.t30[(size_t) k] > 0)
                {
                    const float x = ax.x (rv::mBandFc (k)), y = ty (irM.t30[(size_t) k]);
                    juce::Path d; d.addQuadrilateral (x, y - 5, x + 5, y, x, y + 5, x - 5, y);
                    g.setColour (theme::target); g.strokePath (d, juce::PathStrokeType (1.6f));
                }
        if (mode() == 0) { for (int i = 0; i < 5; ++i) if (shown (i)) drawNode (g, i, col, juce::String (i + 1)); }
        else
        {
            const int nb = nBands();
            for (int i = 0; i + 1 < nb; ++i) { g.setColour (col.withAlpha (0.4f)); g.drawVerticalLine ((int) ax.x (xo (i)), ax.r.getY(), ax.r.getBottom()); }
            for (int i = 0; i < count(); ++i)
            {
                const auto c = pos (i);
                if (i < nb) { g.setColour (i == sel ? theme::text : col); g.fillRoundedRectangle (c.x - 11, c.y - 4.5f, 22, 9, 3.0f); }
                else { g.setColour (i == sel ? theme::text : col.darker (0.3f)); g.fillRect (c.x - 4, c.y - 6, 8.0f, 12.0f); }
            }
        }
    }
    float paintLegend (juce::Graphics& g, float lx) const
    {
        g.setFont (theme::ui (9.5f)); const float ly = ax.r.getY() - 16;
        auto leg = [&] (juce::Colour c, const juce::String& s, int kind) {
            g.setColour (c); if (kind == 2) g.drawEllipse (lx, ly + 3, 7, 7, 1.2f); else if (kind == 1) g.fillRect (lx, ly + 3, 7.0f, 7.0f); else g.fillEllipse (lx, ly + 3, 7, 7);
            g.setColour (theme::muted); g.drawText (s, (int) lx + 10, (int) ly, 160, 13, juce::Justification::centredLeft); lx += 18 + juce::GlyphArrangement::getStringWidth (theme::ui (9.5f), s); };
        leg (rtCol(), ph.tr ("設定した RT", "RT set"), 0);
        leg (rtCol().brighter (0.6f), ph.tr ("測定 T30", "measured T30"), 1);
        leg (theme::text.withAlpha (0.7f), "EDT", 2);
        if (irM.valid) leg (theme::target, ph.tr ("比較 IR の T30（◇）", "compared IR T30 (diamond)"), 1);
        if (ph.proc.measuring.load()) { g.setColour (theme::faint); g.drawText (ph.tr ("測定中…", "measuring..."), (int) ax.r.getRight() - 90, (int) ly, 86, 13, juce::Justification::centredRight); }
        return lx;
    }
protected:
    void dragTo (int i, juce::Point<float> p, juce::Point<float>, bool) override
    {
        if (mode() == 0)
        {
            const double f = juce::jlimit (20.0, 20000.0, ax.f (p.x));
            ph.set (fid (i), (float) f); ph.set (mid (i), (float) juce::jlimit (0.1, 10.0, tyInv (p.y) / std::max (0.05, baseAt (f))));
        }
        else if (i < nBands())
        {
            const int nb = nBands(); const double a = i == 0 ? 20.0 : xo (i - 1), z = i == nb - 1 ? 20000.0 : xo (i);
            ph.set (mbm (i), (float) juce::jlimit (0.1, 10.0, tyInv (p.y) / std::max (0.05, baseAt (std::sqrt (a * z)))));
        }
        else ph.set (xid (i - nBands()), (float) juce::jlimit (40.0, 16000.0, ax.f (p.x)));
    }
    void qStep (int i, float st) override { if (mode() == 0) ph.set (qid (i), (float) juce::jlimit (0.3, 4.0, ph.value (qid (i)) * std::pow (1.12, st))); }
    void resetPoint (int i) override { for (auto& id : ids (i)) if (! id.startsWith ("rtF") && ! id.startsWith ("mbX")) ph.set (id, ph.get (id)->convertFrom0to1 (ph.get (id)->getDefaultValue())); }
    // double-click a point: back to 100 % (EQ mode: and it disappears until added again)
    void dbl (int i) override { for (auto& id : ids (i)) if (id.startsWith ("rtM") || id.startsWith ("mbM")) ph.set (id, 1.0f); if (mode() == 0) setBit ("rtAdd", i, false); }
    bool shown (int i) const override { return mode() != 0 || std::abs (ph.value (mid (i)) - 1.0f) > 0.004f || bit ("rtAdd", i); }
    int addAt (juce::Point<float> p) override
    {
        if (mode() != 0) return -1;
        const double f = ax.f (p.x);
        auto freeN = [&] (int i) { return ! shown (i); };
        int n = -1;
        if (f < 150 && freeN (0)) n = 0; else if (f > 6000 && freeN (4)) n = 4;
        if (n < 0) { double best = 1e9; for (int i : { 1, 2, 3 }) if (freeN (i)) { const double d = std::abs (std::log (f / ph.value (fid (i)))); if (d < best) { best = d; n = i; } } }
        if (n < 0) for (int i : { 0, 4 }) if (freeN (i)) { n = i; break; }
        if (n < 0) return -1;
        for (auto* id : { fid (n), mid (n) }) if (auto* pr = ph.get (id)) pr->beginChangeGesture();
        ph.set (fid (n), (float) f);
        ph.set (mid (n), (float) juce::jlimit (0.1, 10.0, tyInv (p.y) / decay()));
        for (auto* id : { fid (n), mid (n) }) if (auto* pr = ph.get (id)) pr->endChangeGesture();
        setBit ("rtAdd", n, true);
        return n;
    }
    void nudge (int i, int dx, int dy, bool fine) override
    {
        const auto I = ids (i);
        for (auto& id : I)
        {
            if (dx != 0 && (id.startsWith ("rtF") || id.startsWith ("mbX"))) ph.set (id, juce::jlimit (id.startsWith ("mbX") ? 40.0f : 20.0f, id.startsWith ("mbX") ? 16000.0f : 20000.0f, stepFreq (ph.value (id), dx, fine)));
            if (dy != 0 && (id.startsWith ("rtM") || id.startsWith ("mbM"))) ph.set (id, (float) juce::jlimit (0.1, 10.0, ph.value (id) * std::pow (fine ? 1.01 : 1.05, dy)));
        }
    }
private:
    int mode() const { return (int) ph.value ("rtMode"); }
    int nBands() const { return juce::jlimit (2, 5, (int) ph.value ("mbN")); }
    double xo (int i) const { double v = ph.value (xid (0)); for (int k = 1; k <= i; ++k) v = std::max ((double) ph.value (xid (k)), v * 1.2); return v; }
    static const char* fid (int i) { static const char* s[5] = { "rtF1", "rtF2", "rtF3", "rtF4", "rtF5" }; return s[i]; }
    static const char* mid (int i) { static const char* s[5] = { "rtM1", "rtM2", "rtM3", "rtM4", "rtM5" }; return s[i]; }
    static const char* qid (int i) { static const char* s[5] = { "rtQ1", "rtQ2", "rtQ3", "rtQ4", "rtQ5" }; return s[i]; }
    static const char* mbm (int i) { static const char* s[5] = { "mbM1", "mbM2", "mbM3", "mbM4", "mbM5" }; return s[i]; }
    static const char* xid (int i) { static const char* s[4] = { "mbX1", "mbX2", "mbX3", "mbX4" }; return s[i]; }
    double decay() const { return std::max (0.05, ph.proc.decayBase()); }
    // the reverb time a point at f multiplies: Decay, or (ROOM) the room's own at f
    double baseAt (double f) const { return ph.proc.tailFromRoom() ? std::max (0.05, ph.proc.roomRTAt (f)) : decay(); }
    // multiple of Decay -> y: 100 % at a quarter from the top, one octave per quarter (200 % top .. 12.5 % bottom)
    float tm (double m) const { return ax.r.getY() + ax.r.getHeight() * (float) (0.25 - std::log2 (juce::jlimit (0.125, 2.0, m)) * 0.25); }
    float ty (double t) const { return tm (t / decay()); }
public:
    float curveY (double f) const { return ty (ph.proc.rtAt (f)); }
private:
    double tyInv (float y) const { return decay() * std::pow (2.0, (0.25 - juce::jlimit (0.0, 1.0, (double) ((y - ax.r.getY()) / ax.r.getHeight()))) * 4.0); }
};

// ===========================================================================
//  Frequency response: input filter (curve only) or master EQ (6 bands + HP / LP points)
// ===========================================================================
class CurveGraph : public NodeGraph
{
public:
    enum Mode { input = 0, masterEq = 1, erFilter = 2, tailFilter = 3 };
    bool tiled = false;      // (0.10.18) six panels: the EQ graph takes the filter graphs' plot margins, its dB scale moves to the left
    CurveGraph (ParamHelp& h, int m) : NodeGraph (h), mode (m) {}
    int count() const override { return mode == masterEq ? 8 : mode == input ? 3 : 2; }
    juce::Point<float> pos (int i) const override
    {
        if (mode == input)
        {
            if (i == 2) return { ax.x (1000), yd (ph.value ("iTilt")) };
            const char* id = i == 0 ? "iLC" : "iHC"; const double f = ph.value (id);
            return { ax.x (f), yd (juce::jlimit (-24.0, 24.0, curveDb (f))) };
        }
        if (mode == erFilter || mode == tailFilter)
        {
            const char* id = fqId (i); const double f = ph.value (id);
            return { ax.x (f), yd (juce::jlimit (-24.0, 24.0, curveDb (f))) };
        }
        if (i < 6) return { ax.x (ph.value (fid (i))), yd (ph.value (gid (i))) };
        return { ax.x (ph.value (i == 6 ? "eqHP" : "eqLP")), yd (0) };
    }
    // the input / ER filters keep their value boxes in their groups: no small window, the values are written next to the points
    juce::StringArray ids (int i) const override
    {
        if (i < 0 || mode != masterEq) return {};
        if (i < 6) return { fid (i), gid (i), qid (i) };
        return { i == 6 ? "eqHP" : "eqLP" };
    }
    // master EQ: a band shows when it does something (on and not 0 dB) or has been added; LP / HP always
    bool shown (int i) const override
    {
        if (mode != masterEq || i >= 6) return true;
        return (ph.value (bid (i)) > 0.5f && std::abs (ph.value (gid (i))) > 0.01f) || bit ("eqAdd", i);
    }
    int addAt (juce::Point<float> p) override
    {
        if (mode != masterEq) return -1;
        const double f = ax.f (p.x);
        auto freeN = [&] (int i) { return ! shown (i); };
        int n = -1;
        if (f < 120 && freeN (0)) n = 0; else if (f > 8000 && freeN (5)) n = 5;
        if (n < 0) { double best = 1e9; for (int i : { 1, 2, 3, 4 }) if (freeN (i)) { const double d = std::abs (std::log (f / ph.value (fid (i)))); if (d < best) { best = d; n = i; } } }
        if (n < 0) for (int i : { 0, 5 }) if (freeN (i)) { n = i; break; }
        if (n < 0) return -1;
        for (auto* id : { fid (n), gid (n), qid (n), bid (n) }) if (auto* pr = ph.get (id)) pr->beginChangeGesture();
        ph.set (fid (n), (float) f); ph.set (gid (n), (float) juce::jlimit (-18.0, 18.0, dbAt (p.y))); ph.set (qid (n), n == 0 || n == 5 ? 0.7f : 1.0f);
        if (auto* pr = ph.get (bid (n))) pr->setValueNotifyingHost (1.0f);
        for (auto* id : { fid (n), gid (n), qid (n), bid (n) }) if (auto* pr = ph.get (id)) pr->endChangeGesture();
        setBit ("eqAdd", n, true);
        return n;
    }
    juce::StringArray gestureIds (int i) const override
    {
        if (mode == masterEq) return ids (i);
        if (mode == input && i == 2) return { "iTilt" };
        return { fqId (i) };
    }
    juce::String title (int i) const override
    {
        if (mode == input) return i == 0 ? "LOW CUT" : i == 1 ? "HIGH CUT" : "TILT";
        if (mode == erFilter) return i == 0 ? "ER LOW CUT" : "ER HIGH CUT";
        if (mode == tailFilter) return i == 0 ? "TAIL LOW CUT" : "TAIL HIGH CUT";
        if (i == 6) return "HIGH PASS"; if (i == 7) return "LOW PASS";
        return (i == 0 ? juce::String ("LOW SHELF") : i == 5 ? juce::String ("HIGH SHELF") : "BAND " + juce::String (i + 1))
               + (ph.value (bid (i)) > 0.5f ? juce::String() : ph.tr ("（オフ）", " (off)"));
    }
    double curveDb (double f) const
    {
        if (mode == erFilter || mode == tailFilter)
        {   // the ER's / tail's Butterworth low / high cut (as in their own filters)
            static const int o[6] = { 1, 2, 3, 4, 6, 8 };
            const double lc = ph.value (fqId (0)), hc = ph.value (fqId (1));
            const double nl = o[juce::jlimit (0, 5, (int) ph.value (slId (0)))], nh = o[juce::jlimit (0, 5, (int) ph.value (slId (1)))];
            double h = 1;
            if (lc > 20.5) h /= std::sqrt (1 + std::pow (lc / f, 2 * nl));
            if (hc < 19900) h /= std::sqrt (1 + std::pow (f / hc, 2 * nh));
            return 20 * std::log10 (h + 1e-9);
        }
        if (mode == masterEq) { rv::MasterEQ e; ph.proc.setMasterEQ (e, 48000); return e.magDb (f, 48000); }
        rv::InputFilter fl; ph.proc.setInputFilter (fl, 48000); return fl.magDb (f, 48000);
    }
    // the master EQ's scale: +-3 / 6 / 12 / 18 / 30 dB (click the top value to choose)
    int eqRange() const { const int r = (int) ph.proc.uiState.getProperty ("eqRange", 18); return r == 3 || r == 6 || r == 12 || r == 30 ? r : 18; }
    double span() const { return mode == masterEq ? eqRange() * 7.0 / 6.0 : 21.0; }
    double dbAt (float y) const { return (ax.r.getCentreY() - y) / (ax.r.getHeight() / 2) * span(); }
    // right axis: dB, 0 dB in the middle
    void paintAxisRight (juce::Graphics& g) const
    {
        const int R = eqRange(), st = R == 3 ? 1 : R / 3;
        for (int d = -R; d <= R; d += st)
        {
            g.setColour (d == 0 ? eqCol().withAlpha (0.35f) : theme::grid.withAlpha (0.6f));
            for (float x = ax.r.getX(); x < ax.r.getRight(); x += 6) g.drawHorizontalLine ((int) yd (d), x, x + 3);
            g.setColour (eqCol().withAlpha (d == 0 || d == R ? 1.0f : 0.75f)); g.setFont (theme::mono (9.0f, d == R));
            if (tiled) g.drawText (minus ((d > 0 ? "+" : "") + juce::String (d)), (int) ax.r.getX() - 29, (int) yd (d) - 6, 26, 12, juce::Justification::centredRight);
            else g.drawText (minus ((d > 0 ? "+" : "") + juce::String (d)), (int) ax.r.getRight() + 3, (int) yd (d) - 6, 30, 12, juce::Justification::centredLeft);
        }
        // the top value is a button: the scale
        rangeR = tiled ? juce::Rectangle<float> (ax.r.getX() - 29, yd (R) - 8, 28, 16) : juce::Rectangle<float> (ax.r.getRight() + 1, yd (R) - 8, 32, 16);
        g.setColour (eqCol().withAlpha (0.6f)); g.drawRoundedRectangle (rangeR, 3.0f, 1.0f);
        g.setColour (eqCol()); g.setFont (theme::ui (9.0f, true));
        if (tiled) g.drawText ("EQ dB", (int) ax.r.getX() - 29, (int) ax.r.getY() - 16, 60, 12, juce::Justification::centredLeft);
        else g.drawText ("EQ dB", (int) ax.r.getRight() - 4, (int) ax.r.getY() - 16, 40, 12, juce::Justification::centredLeft);
    }
    bool clickExtra (juce::Point<float> p, juce::Component* host) override
    {
        if (mode != masterEq || ! rangeR.contains (p)) return false;
        juce::PopupMenu m; m.addSectionHeader (ph.tr ("EQ の目盛り", "EQ scale"));
        for (int r : { 3, 6, 12, 18, 30 }) m.addItem (r, juce::String::fromUTF8 ("\xc2\xb1") + juce::String (r) + " dB", true, r == eqRange());
        juce::Component::SafePointer<juce::Component> sp (host);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (host->localAreaToGlobal (rangeR.toNearestInt())), [this, sp] (int r)
        { if (r > 0) { ph.proc.uiState.setProperty ("eqRange", r, nullptr); if (sp != nullptr) sp->repaint(); } });
        return true;
    }
    float curveY (double f) const { rv::MasterEQ e; ph.proc.setMasterEQ (e, 48000); return yd (juce::jlimit (-24.0, 24.0, e.magDb (f, 48000))); }
    void paintEqCurves (juce::Graphics& g) const
    {
        rv::MasterEQ e; ph.proc.setMasterEQ (e, 48000);
        juce::Path p; bool st = false;
        g.saveState(); g.reduceClipRegion (ax.r.expanded (0, 2).toNearestInt());
        for (float px = ax.r.getX(); px <= ax.r.getRight(); px += 2)
        {
            const float y = yd (juce::jlimit (-24.0, 24.0, e.magDb (ax.f (px), 48000)));
            if (! st) { p.startNewSubPath (px, y); st = true; } else p.lineTo (px, y);
        }
        const bool on = ph.value ("eqOn") > 0.5f;
        g.setColour (on ? eqCol() : eqCol().withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (2.0f));
        g.restoreState();
        for (int i = 0; i < 6; ++i) if (shown (i)) drawNode (g, i, eqCol().withAlpha (on ? 1.0f : 0.4f), juce::String (i + 1), ph.value (bid (i)) > 0.5f);
        for (int i = 6; i < 8; ++i) { const auto c = pos (i); g.setColour (i == sel ? theme::text : eqCol().darker (0.35f)); g.fillRect (c.x - 4, c.y - 7, 8.0f, 14.0f); }
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const bool small = getHeight() < 200 || (mode != masterEq && getWidth() < 330);
        g.setColour (theme::well); g.fillRoundedRectangle (b, 4.0f);
        if (mode == masterEq)
        {
            ax.r = tiled ? b.withTrimmedLeft (30).withTrimmedRight (8).withTrimmedBottom (16).withTrimmedTop (18) : b.withTrimmedLeft (12).withTrimmedRight (36).withTrimmedBottom (16).withTrimmedTop (18);
            drawSpectra (g); ax.grid (g); paintAxisRight (g); paintEqCurves (g);
            return;
        }
        ax.r = b.withTrimmedLeft (30).withTrimmedBottom (16).withTrimmedTop (6).withTrimmedRight (8);
        drawSpectra (g);
        ax.grid (g);
        for (int d = -18; d <= 18; d += small ? 12 : 6)
        {
            g.setColour (d == 0 ? theme::line : theme::grid); g.drawHorizontalLine ((int) yd (d), ax.r.getX(), ax.r.getRight());
            g.setColour (theme::faint); g.setFont (theme::mono (9.0f)); g.drawText (minus (juce::String (d)), 0, (int) yd (d) - 6, 26, 12, juce::Justification::centredRight);
        }
        // the curve (computed once per pixel pair; the filter objects are made once per paint)
        rv::MasterEQ e; rv::InputFilter fl;
        if (mode == masterEq) ph.proc.setMasterEQ (e, 48000); else if (mode == input) ph.proc.setInputFilter (fl, 48000);
        juce::Path p; bool st = false;
        g.saveState(); g.reduceClipRegion (ax.r.expanded (0, 2).toNearestInt());
        for (float px = ax.r.getX(); px <= ax.r.getRight(); px += 2)
        {
            const double fr = ax.f (px), db = mode == masterEq ? e.magDb (fr, 48000) : mode == input ? fl.magDb (fr, 48000) : curveDb (fr);
            const float y = yd (juce::jlimit (-24.0, 24.0, db));
            if (! st) { p.startNewSubPath (px, y); st = true; } else p.lineTo (px, y);
        }
        const bool on = mode != masterEq || ph.value ("eqOn") > 0.5f;
        const auto col = mode == erFilter ? theme::side : mode == tailFilter ? theme::target : theme::accent;
        g.setColour (on ? col : theme::faint); g.strokePath (p, juce::PathStrokeType (2.0f));
        g.restoreState();
        g.setFont (theme::ui (9.5f));
        if (mode == masterEq)
        {
            for (int i = 0; i < 6; ++i) drawNode (g, i, theme::accent.withAlpha (on ? 1.0f : 0.4f), juce::String (i + 1), ph.value (bid (i)) > 0.5f);
            for (int i = 6; i < 8; ++i) { const auto c = pos (i); g.setColour (i == sel ? theme::text : theme::warn); g.fillRect (c.x - 4, c.y - 7, 8.0f, 14.0f); }
            if (! small)
            {
                g.setColour (theme::faint);
                g.drawText (ph.tr ("ドラッグ：周波数・ゲイン（Shift 微調整）／ホイール：Q／←→↑↓キー／Option+クリック：0 dB／ダブルクリック：オン・オフ",
                                   "Drag: freq / gain (Shift fine), wheel: Q, arrow keys, Option+click: 0 dB, double-click: on / off"), ax.r.withHeight (14).translated (4, 2), juce::Justification::centredLeft);
            }
            return;
        }
        if (getWidth() < 330)
        {   // (all three filters on one screen) which one this is
            g.setColour (col); g.setFont (theme::ui (10.5f, true));
            g.drawText (mode == input ? "INPUT" : mode == erFilter ? "ER" : "TAIL", ax.r.toNearestInt().withHeight (14).translated (0, 2), juce::Justification::centred);
        }
        // input / ER filter: the points and their values
        const char* lcId = fqId (0); const char* hcId = fqId (1);
        const char* lsId = slId (0); const char* hsId = slId (1);
        static const char* slopes[6] = { "6", "12", "18", "24", "36", "48" };
        auto tag = [&] (int i, const juce::String& t)
        {   // LC / HC above their points, the tilt below its own
            const auto c = pos (i); g.setColour (theme::text.withAlpha (0.85f)); g.setFont (theme::mono (9.5f, i == sel));
            const bool right = c.x > ax.r.getCentreX();
            const float x = right ? c.x - 156 : c.x + 12, y = (i == 2 || (small && i == 1)) ? c.y + 10 : c.y - 22;
            g.drawText (t, (int) x, (int) y, 144, 13, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
        };
        const double lc = ph.value (lcId), hc = ph.value (hcId);
        drawNode (g, 0, col, "L", lc > 20.5); drawNode (g, 1, col, "H", hc < 19900);
        tag (0, lc > 20.5 ? "LC " + fmtHz ((float) lc) + (small ? juce::String() : juce::String ("  ") + slopes[juce::jlimit (0, 5, (int) ph.value (lsId))] + " dB/oct") : juce::String ("LC Off"));
        tag (1, hc < 19900 ? "HC " + fmtHz ((float) hc) + (small ? juce::String() : juce::String ("  ") + slopes[juce::jlimit (0, 5, (int) ph.value (hsId))] + " dB/oct") : juce::String ("HC Off"));
        if (mode == input) { drawNode (g, 2, theme::direct, "T", std::abs (ph.value ("iTilt")) > 0.05f); tag (2, "Tilt " + fmtDb (ph.value ("iTilt")) + " (1 kHz)"); }
        if (! small)
        {
            g.setColour (theme::faint); g.setFont (theme::ui (9.5f));
            g.drawText (mode == input ? ph.tr ("L・H を左右にドラッグ＝周波数、ホイール＝傾き（dB/oct）、T を上下＝Tilt、Option+クリック＝オフ",
                                               "Drag L / H sideways = frequency, wheel = slope (dB/oct), T up / down = tilt, Option+click = off")
                                      : mode == tailFilter ? ph.tr ("TAIL のフィルター：L・H を左右にドラッグ＝周波数、ホイール＝傾き", "The tail's filter: drag L / H sideways = frequency, wheel = slope")
                                      : ph.tr ("ER のフィルター：L・H を左右にドラッグ＝周波数、ホイール＝傾き（dB/oct）、Option+クリック＝オフ",
                                               "The ER's filter: drag L / H sideways = frequency, wheel = slope (dB/oct), Option+click = off"),
                        ax.r.withHeight (14).translated (4, 2), juce::Justification::centredLeft);
        }
    }
protected:
    void dragTo (int i, juce::Point<float> p, juce::Point<float>, bool) override
    {
        if (mode == masterEq)
        {
            if (i < 6) { ph.set (fid (i), (float) ax.f (p.x)); ph.set (gid (i), (float) juce::jlimit (-18.0, 18.0, dbAt (p.y))); }
            else ph.set (i == 6 ? "eqHP" : "eqLP", (float) (i == 6 ? juce::jlimit (20.0, 1000.0, ax.f (p.x)) : juce::jlimit (1000.0, 20000.0, ax.f (p.x))));
            return;
        }
        if (mode == input && i == 2) { ph.set ("iTilt", (float) juce::jlimit (-6.0, 6.0, (ax.r.getCentreY() - p.y) / (ax.r.getHeight() / 2) * 21.0)); return; }
        const auto r = range (i); ph.set (fqId (i), (float) juce::jlimit (r.getStart(), r.getEnd(), ax.f (p.x)));
    }
    void qStep (int i, float st) override
    {
        if (mode == masterEq) { if (i < 6) ph.set (qid (i), (float) juce::jlimit (0.3, 10.0, ph.value (qid (i)) * std::pow (1.12, st))); return; }
        if (i > 1) return;
        const char* sid = slId (i);
        if (auto* pr = ph.get (sid)) { const int v = juce::jlimit (0, 5, (int) ph.value (sid) + (st > 0 ? 1 : st < 0 ? -1 : 0)); pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 ((float) v)); pr->endChangeGesture(); }
    }
    void resetPoint (int i) override
    {
        if (mode == masterEq) { if (i < 6) ph.set (gid (i), 0.0f); else ph.set (i == 6 ? "eqHP" : "eqLP", i == 6 ? 20.0f : 20000.0f); return; }
        if (mode == input && i == 2) { ph.set ("iTilt", 0.0f); return; }
        ph.set (fqId (i), i == 0 ? 20.0f : 20000.0f);
    }
    void dbl (int i) override
    {
        if (mode == masterEq)
        {   // double-click a band: remove it (0 dB, hidden until added again); HP / LP: off
            if (i < 6) { ph.set (gid (i), 0.0f); setBit ("eqAdd", i, false); }
            else ph.set (i == 6 ? "eqHP" : "eqLP", i == 6 ? 20.0f : 20000.0f);
            return;
        }
        resetPoint (i);
    }
    void nudge (int i, int dx, int dy, bool fine) override
    {
        if (mode == masterEq)
        {
            if (i < 6)
            {
                if (dx != 0) ph.set (fid (i), stepFreq (ph.value (fid (i)), dx, fine));
                if (dy != 0) ph.set (gid (i), juce::jlimit (-18.0f, 18.0f, ph.value (gid (i)) + dy * (fine ? 0.1f : 0.5f)));
            }
            else if (dx != 0) { const char* id = i == 6 ? "eqHP" : "eqLP"; ph.set (id, juce::jlimit (i == 6 ? 20.0f : 1000.0f, i == 6 ? 1000.0f : 20000.0f, stepFreq (ph.value (id), dx, fine))); }
            return;
        }
        if (mode == input && i == 2) { if (dy != 0) ph.set ("iTilt", juce::jlimit (-6.0f, 6.0f, ph.value ("iTilt") + dy * (fine ? 0.1f : 0.5f))); return; }
        if (dx != 0) { const auto r = range (i); ph.set (fqId (i), juce::jlimit ((float) r.getStart(), (float) r.getEnd(), stepFreq (ph.value (fqId (i)), dx, fine))); }
    }
private:
    const char* fqId (int i) const { return mode == input ? (i == 0 ? "iLC" : "iHC") : mode == tailFilter ? (i == 0 ? "tLC" : "tHC") : (i == 0 ? "lowcut" : "highcut"); }
    const char* slId (int i) const { return mode == input ? (i == 0 ? "iLCs" : "iHCs") : mode == tailFilter ? (i == 0 ? "tLCs" : "tHCs") : (i == 0 ? "lcSlope" : "hcSlope"); }
    juce::Range<double> range (int i) const
    {
        if (mode == input || mode == tailFilter) return i == 0 ? juce::Range<double> (20, 2000) : juce::Range<double> (1000, 20000);
        return i == 0 ? juce::Range<double> (20, 1000) : juce::Range<double> (1000, 20000);
    }
    float yd (double db) const { return ax.r.getCentreY() - (float) (db / span()) * ax.r.getHeight() / 2; }
    mutable juce::Rectangle<float> rangeR;
    static const char* fid (int i) { static const char* s[6] = { "eqF1", "eqF2", "eqF3", "eqF4", "eqF5", "eqF6" }; return s[i]; }
    static const char* gid (int i) { static const char* s[6] = { "eqG1", "eqG2", "eqG3", "eqG4", "eqG5", "eqG6" }; return s[i]; }
    static const char* qid (int i) { static const char* s[6] = { "eqQ1", "eqQ2", "eqQ3", "eqQ4", "eqQ5", "eqQ6" }; return s[i]; }
    static const char* bid (int i) { static const char* s[6] = { "eqB1", "eqB2", "eqB3", "eqB4", "eqB5", "eqB6" }; return s[i]; }
    int mode;
};

// ===========================================================================
//  Reverb time and master EQ on one plot over the spectra:
//  left axis = RT as % of Decay (blue), right axis = EQ dB (yellow, 0 dB in the middle).
//  Points 0..nR-1 are the RT's, the rest the EQ's; both stay editable.
// ===========================================================================
class ComboGraph : public NodeGraph
{
public:
    ComboGraph (ParamHelp& h, RTGraph& r, CurveGraph& e) : NodeGraph (h), rt (r), eq (e) {}
    int nR() const { return rt.count(); }
    int count() const override { return nR() + eq.count(); }
    juce::Point<float> pos (int i) const override { sync(); return i < nR() ? rt.pos (i) : eq.pos (i - nR()); }
    juce::StringArray ids (int i) const override { if (i < 0) return {}; return i < nR() ? rt.ids (i) : eq.ids (i - nR()); }
    juce::StringArray gestureIds (int i) const override { return i < nR() ? rt.ids (i) : static_cast<NodeGraph&> (eq).gestureIds (i - nR()); }
    juce::String title (int i) const override { return i < nR() ? "RT  " + rt.title (i) : "EQ  " + eq.title (i - nR()); }
    void resized() override { layoutPlot(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (b, 4.0f);
        layoutPlot();
        rt.sel = sel >= 0 && sel < nR() ? sel : -1; eq.sel = sel >= nR() ? sel - nR() : -1;
        drawSpectra (g);
        ax.grid (g);
        rt.paintAxis (g);
        eq.paintAxisRight (g);
        eq.paintEqCurves (g);
        rt.paintCurves (g);
        float lx = rt.paintLegend (g, ax.r.getX() + 6);
        g.setColour (eqCol()); g.fillRect (lx, ax.r.getY() - 12, 10.0f, 3.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (9.5f)); g.drawText (ph.tr ("MASTER EQ（右の目盛り）", "MASTER EQ (right scale)"), (int) lx + 14, (int) ax.r.getY() - 16, 160, 13, juce::Justification::centredLeft);
        if (getHeight() >= 200)
        {
            g.setColour (theme::faint); g.setFont (theme::ui (9.5f));
            g.drawText (ph.tr ("青＝RT（左 %）、黄＝EQ（右 dB）。空いた所をダブルクリック：点を追加（上の「追加」で EQ／RT）／点をダブルクリック：消す／ドラッグ（Shift 微調整）／ホイール：Q／←→↑↓／右クリック：小窓を閉じる",
                               "Blue = RT (left %), yellow = EQ (right dB). Double-click an empty spot: add a point (EQ / RT as chosen above); double-click a point: remove it; drag (Shift fine), wheel = Q, arrows, right-click: close the small window"),
                        ax.r.withTrimmedTop (ax.r.getHeight() - 30).withHeight (14).translated (4, 0), juce::Justification::centredLeft);
        }
    }
    bool shown (int i) const override { return i < nR() ? static_cast<const NodeGraph&> (rt).shown (i) : static_cast<const NodeGraph&> (eq).shown (i - nR()); }
    bool clickExtra (juce::Point<float> p, juce::Component* host) override { sync(); return as (eq).clickExtra (p, host); }
    // a click on (near) the blue or the yellow line chooses what a double-click adds (RT / EQ), like the buttons above
    std::function<void()> onAddType;
    void emptyClick (juce::Point<float> p) override
    {
        sync(); if (! ax.r.contains (p)) return;
        const double f = ax.f (p.x);
        const float dR = std::abs (rt.curveY (f) - p.y), dE = std::abs (eq.curveY (f) - p.y);
        if (std::min (dR, dE) > 10) return;
        const int t = dR <= dE ? 1 : 0;
        if ((int) ph.proc.uiState.getProperty ("addType", 0) != t) { ph.proc.uiState.setProperty ("addType", t, nullptr); repaint(); if (onAddType) onAddType(); }
    }
    // double-click on an empty spot adds an EQ band or an RT point (uiState "addType": 0 EQ, 1 RT)
    int addAt (juce::Point<float> p) override
    {
        sync();
        if ((int) ph.proc.uiState.getProperty ("addType", 0) == 1) return as (rt).addAt (p);
        const int n = as (eq).addAt (p); return n < 0 ? -1 : n + nR();
    }
    void dragTo (int i, juce::Point<float> p, juce::Point<float> st, bool f) override { sync(); if (i < nR()) as (rt).dragTo (i, p, st, f); else as (eq).dragTo (i - nR(), p, st, f); }
    void qStep (int i, float s) override { if (i < nR()) as (rt).qStep (i, s); else as (eq).qStep (i - nR(), s); }
    void resetPoint (int i) override { if (i < nR()) as (rt).resetPoint (i); else as (eq).resetPoint (i - nR()); }
    void dbl (int i) override { if (i < nR()) as (rt).dbl (i); else as (eq).dbl (i - nR()); }
    void nudge (int i, int dx, int dy, bool f) override { if (i < nR()) as (rt).nudge (i, dx, dy, f); else as (eq).nudge (i - nR(), dx, dy, f); }
private:
    static NodeGraph& as (NodeGraph& n) { return n; }
    void layoutPlot()
    {
        ax.r = getLocalBounds().toFloat().withTrimmedLeft (40).withTrimmedRight (36).withTrimmedBottom (16).withTrimmedTop (18);
        sync();
    }
    void sync() const { rt.setPlot (ax.r); eq.setPlot (ax.r); }
    RTGraph& rt; CurveGraph& eq;
};

// the small window next to the selected point: a title; the editor puts the point's value boxes on top of it
class NodePopup : public juce::Component
{
public:
    juce::String title;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme::panel.withAlpha (0.94f)); g.fillRoundedRectangle (b, 5.0f);
        g.setColour (theme::accent.withAlpha (0.8f)); g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (theme::accent); g.setFont (theme::ui (9.0f, true)); g.drawText (title, b.reduced (6, 2).withHeight (12), juce::Justification::centredLeft);
    }
    bool hitTest (int, int) override { return false; }
};

// ===========================================================================
//  IR tail: the IR's envelope from the direct sound (grey), the tail as it will play (cyan),
//  where it starts and (IR + ALGO) where the FDN takes over
// ===========================================================================
class IrTailView : public juce::Component
{
public:
    IrTailView (ParamHelp& h) : ph (h) {}
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (b, 4.0f);
        auto r = b.withTrimmedLeft (34).withTrimmedBottom (16).withTrimmedTop (6).withTrimmedRight (8);
        auto T = ph.proc.target();
        std::vector<float> L, R; rv::Measure m; ph.proc.tailIR (L, R, m);
        // the source: the tail's own IR, or the ER's IR
        const bool own = ph.proc.tailIrOwn();
        const std::vector<float>* pL = nullptr; const std::vector<float>* pR = nullptr; long t0 = 0, n = 0; bool wet = false;
        if (own)
        {
            pL = &ph.proc.tIrRawL; pR = &ph.proc.tIrRawR; n = (long) pL->size();
            float gp = 0; for (long i = 0; i < n; ++i) gp = std::max (gp, std::max (std::abs ((*pL)[(size_t) i]), std::abs ((*pR)[(size_t) i])));
            for (long i = 0; i < n; ++i) if (std::max (std::abs ((*pL)[(size_t) i]), std::abs ((*pR)[(size_t) i])) >= 0.25f * gp) { t0 = i; break; }
        }
        else if (T && T->valid) { pL = &T->sig.L; pR = &T->sig.R; t0 = T->t0; n = T->sig.n; wet = T->wetOnly; }
        if (pL == nullptr)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (12.0f));
            g.drawFittedText (ph.tr ("IR を読み込むと、その残響（IR の Tail Start 以降）をテールに使えます。ヘッダーの「Load IR」（ER と同じ IR）か、このページの「テール用 IR」（テールだけ別の IR）。",
                                     "Load an IR to use its reverb (from IR Tail Start on) as the tail: \"Load IR\" in the header (the ER's IR) or \"Tail IR\" on this page (a separate IR for the tail)."),
                              r.reduced (20).toNearestInt(), juce::Justification::centred, 3);
            return;
        }
        const auto& SL = *pL; const auto& SR = *pR;
        const double S = std::max (1.0, (double) (n - t0) / 48000.0);
        auto X = [&] (double t) { return r.getX() + (float) (t / S) * r.getWidth(); };
        auto Y = [&] (double db) { return r.getY() + (float) ((0 - db) / 80.0) * r.getHeight(); };
        g.setFont (theme::mono (9.0f));
        for (int db = 0; db >= -80; db -= 20) { g.setColour (theme::grid); g.drawHorizontalLine ((int) Y (db), r.getX(), r.getRight()); g.setColour (theme::faint); g.drawText (minus (juce::String (db)), 0, (int) Y (db) - 6, 30, 12, juce::Justification::centredRight); }
        for (double t = 0; t <= S + 1e-6; t += S > 3 ? 1.0 : 0.5) { g.setColour (theme::grid); g.drawVerticalLine ((int) X (t), r.getY(), r.getBottom()); g.setColour (theme::faint); g.drawText (juce::String (t, 1) + " s", (int) X (t) - 20, (int) r.getBottom() + 2, 40, 12, juce::Justification::centred); }
        // the IR (normalised to its peak)
        float pk = 1e-9f; for (long i = t0; i < n; ++i) pk = std::max (pk, std::max (std::abs (SL[(size_t) i]), std::abs (SR[(size_t) i])));
        auto env = [&] (auto get, size_t nn, double t0s, float norm, juce::Colour c, bool fill)
        {
            juce::Path p; const double bin = std::max (1.0 / 48000, S / r.getWidth() * 1.5); bool st = false;
            if (fill) { p.startNewSubPath (X (t0s), r.getBottom()); st = true; }
            for (double t = 0; t0s + t < S; t += bin)
            {
                const size_t a = (size_t) (t * 48000), z = std::min (nn, (size_t) ((t + bin) * 48000)); if (a >= nn) break;
                float m2 = 0; for (size_t i = a; i < z; ++i) m2 = std::max (m2, get (i));
                const float x = X (t0s + t + bin * 0.5), y = Y (juce::jlimit (-80.0, 0.0, 20 * std::log10 (m2 * norm + 1e-9)));
                if (! st) { p.startNewSubPath (x, y); st = true; } else p.lineTo (x, y);
            }
            if (fill) { p.lineTo (p.getCurrentPosition().x, r.getBottom()); p.closeSubPath(); g.setColour (c.withAlpha (0.2f)); g.fillPath (p); }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.0f));
        };
        env ([&] (size_t i) { return std::max (std::abs (SL[(size_t) t0 + i]), std::abs (SR[(size_t) t0 + i])); }, (size_t) (n - t0), 0.0, 1.0f / pk, theme::muted.withAlpha (0.8f), false);
        float dpk = 0; for (long i = std::max (0L, t0 - 3); i <= t0 + 3 && i < n; ++i) dpk = std::max (dpk, std::max (std::abs (SL[(size_t) i]), std::abs (SR[(size_t) i])));
        const float k = wet ? 1.0f / pk : dpk / pk;       // the tail is scaled to direct = 1; draw it on the IR's scale
        g.setColour (own ? theme::direct : theme::faint); g.setFont (theme::ui (10.0f, true));
        g.drawText (own ? ph.tr ("テール用 IR：", "Tail IR: ") + ph.proc.tIrName : ph.tr ("ER と同じ IR：", "Same IR as the ER: ") + ph.proc.irName,
                    r.withHeight (14).translated (0, 16).toNearestInt(), juce::Justification::centredRight);
        const double x0 = ph.value ("irtX") / 1000.0;
        if (! L.empty()) env ([&] (size_t i) { return std::max (std::abs (L[i]), std::abs (R[i])); }, L.size(), x0, k, theme::accent, true);
        auto mark = [&] (double t, juce::Colour c, const juce::String& s)
        {
            g.setColour (c.withAlpha (0.7f)); for (float y = r.getY(); y < r.getBottom(); y += 6) g.drawLine (X (t), y, X (t), y + 3, 1.0f);
            g.setFont (theme::ui (9.5f)); g.drawText (s, (int) X (t) + 4, (int) r.getY() + 2, 160, 12, juce::Justification::centredLeft);
        };
        mark (x0, theme::accent, "Tail Start " + fmtMs (ph.value ("irtX")));
        if ((int) ph.value ("tSrc") == 2) mark (x0 + ph.value ("irtB") / 1000.0 * ph.value ("irtSt") / 100.0, theme::direct, ph.tr ("ここから ALGO", "ALGO from here"));
        if (m.valid)
        {
            g.setColour (theme::text); g.setFont (theme::mono (10.0f));
            g.drawText (ph.tr ("IR の T30  ", "IR T30  ") + "125 Hz " + juce::String (m.t30[1], 2) + " s   1 kHz " + juce::String (m.t30[4], 2) + " s   4 kHz " + juce::String (m.t30[6], 2) + " s",
                        r.withTrimmedTop (r.getHeight() - 16).translated (6, -2).toNearestInt(), juce::Justification::centredLeft);
        }
    }
private:
    ParamHelp& ph;
};

// ===========================================================================
//  Measurement of the whole reverb: T30 / EDT per octave, C80, D50, IACC
// ===========================================================================
class MeasurePanel : public juce::Component
{
public:
    MeasurePanel (ParamHelp& h) : ph (h) {}
    rv::Measure m, irM;                    // (0.10.10) irM: the compared IR (TAIL / ALL)
    std::vector<float> edc; double edcMs = 0;           // broadband energy decay (Schroeder), dB, from the direct sound; length in ms
    void setIR (const std::vector<float>& L, const std::vector<float>& R, double fs)
    {
        edc.clear(); edcMs = 0; if (L.empty()) return;
        const size_t n = L.size(); std::vector<double> e (n); double acc = 0;
        for (size_t i = n; i-- > 0;) { acc += (double) L[i] * L[i] + (double) R[i] * R[i]; e[i] = acc; }
        const int P = 400; edc.resize ((size_t) P); edcMs = 1000.0 * (double) n / fs;
        for (int k = 0; k < P; ++k) edc[(size_t) k] = (float) (10 * std::log10 (e[(size_t) ((double) k / P * (double) (n - 1))] / std::max (1e-30, e[0]) + 1e-12));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool big = getHeight() > 250;
        const float rowH = big ? 16.0f : 13.0f, fz = big ? 11.0f : 10.0f, x0 = r.getX(), w = r.getWidth();
        const float cw[4] = { 0.25f, 0.25f, 0.25f, 0.25f };
        auto cell = [&] (int col, float y, const juce::String& s, juce::Colour c, bool bold = false)
        {
            float x = x0; for (int i = 0; i < col; ++i) x += cw[i] * w;
            g.setColour (c); g.setFont (col == 0 ? theme::ui (fz, bold) : theme::mono (fz, bold));
            g.drawFittedText (s, (int) x, (int) y, (int) (cw[col] * w) - 2, (int) rowH, col == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight, 1, 0.7f);
        };
        float y = r.getY();
        auto sec0 = [] (double v) { return v > 0 ? juce::String (v, v < 10 ? 2 : 1) + " s" : juce::String ("-"); };
        if (getHeight() < 170)
        {   // under the faceplate: T30 per band in two columns, then the figures
            g.setFont (theme::mono (10.0f));
            for (int k = 0; k < rv::kMBands; ++k)
            {
                const double f = rv::mBandFc (k); const float cx = x0 + (k / 4) * w * 0.5f, cy = y + (k % 4) * 14.0f;
                g.setColour (theme::muted); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) cx, (int) cy, 34, 13, juce::Justification::centredLeft);
                g.setColour (theme::accent); g.drawText (sec0 (ph.proc.rtAt (f)), (int) cx + 34, (int) cy, 64, 13, juce::Justification::centredRight);
                g.setColour (theme::text); g.drawText (m.valid ? sec0 (m.t30[(size_t) k]) : "-", (int) cx + 98, (int) cy, 66, 13, juce::Justification::centredRight);
            }
            y += 4 * 14.0f + 4;
            g.setColour (theme::faint); g.setFont (theme::ui (9.0f)); g.drawText (ph.tr ("帯域  設定 RT  測定 T30", "band  RT set  T30"), (int) x0, (int) y, (int) w, 12, juce::Justification::centredLeft); y += 13;
            g.setColour (theme::text); g.setFont (theme::mono (10.0f));
            g.drawText ("T30 " + sec0 (m.t30Wide) + "  EDT " + sec0 (m.edtWide) + "  C80 " + (m.valid ? fmtDb (m.c80) : juce::String ("-")), (int) x0, (int) y, (int) w, 13, juce::Justification::centredLeft); y += 14;
            if (irM.valid) { g.setColour (theme::target); g.drawText ("IR  T30 " + sec0 (irM.t30Wide) + "  EDT " + sec0 (irM.edtWide) + "  C80 " + fmtDb (irM.c80), (int) x0, (int) y + 14, (int) w, 13, juce::Justification::centredLeft); g.setColour (theme::text); }
            g.drawText ("D50 " + (m.valid ? juce::String ((int) std::round (m.d50 * 100)) + "%" : juce::String ("-")) + "  IACC " + (m.valid ? juce::String (m.iacc, 2) : juce::String ("-")), (int) x0, (int) y, (int) w, 13, juce::Justification::centredLeft);
            return;
        }
        cell (0, y, ph.tr ("帯域", "Band"), theme::muted, true); cell (1, y, ph.tr ("設定 RT", "RT set"), theme::accent, true);
        cell (2, y, "T30", theme::direct, true); cell (3, y, irM.valid ? juce::String ("IR T30") : juce::String ("EDT"), irM.valid ? theme::target : theme::muted, true);
        y += rowH + 2;
        auto sec = [] (double v) { return v > 0 ? juce::String (v, v < 10 ? 2 : 1) + " s" : juce::String ("-"); };
        for (int k = 0; k < rv::kMBands; ++k)
        {
            const double f = rv::mBandFc (k);
            cell (0, y, f >= 1000 ? juce::String ((int) (f / 1000)) + " kHz" : juce::String ((int) f) + " Hz", theme::text);
            cell (1, y, sec (ph.proc.rtAt (f)), theme::accent);
            cell (2, y, m.valid ? sec (m.t30[(size_t) k]) : "-", theme::direct);
            if (irM.valid) cell (3, y, sec (irM.t30[(size_t) k]), theme::target);
            else cell (3, y, m.valid ? sec (m.edt[(size_t) k]) : "-", theme::muted);
            y += rowH;
        }
        y += 3;
        g.setColour (theme::line); g.drawHorizontalLine ((int) y - 2, x0, x0 + w);
        g.setFont (theme::mono (fz)); g.setColour (theme::text);
        const juce::String s1 = "T30 " + sec (m.t30Wide) + "   EDT " + sec (m.edtWide);
        const juce::String s2 = "C80 " + (m.valid ? fmtDb (m.c80) : juce::String ("-")) + "   D50 " + (m.valid ? juce::String ((int) std::round (m.d50 * 100)) + "%" : juce::String ("-"))
                                + "   IACC " + (m.valid ? juce::String (m.iacc, 2) : juce::String ("-"));
        g.drawFittedText (s1, (int) x0, (int) y, (int) w, (int) rowH, juce::Justification::centredLeft, 1, 0.7f); y += rowH;
        g.drawFittedText (s2, (int) x0, (int) y, (int) w, (int) rowH, juce::Justification::centredLeft, 1, 0.7f); y += rowH;
        if (irM.valid)
        {
            g.setColour (theme::target);
            g.drawFittedText ("IR  T30 " + sec (irM.t30Wide) + "   EDT " + sec (irM.edtWide) + "   C80 " + fmtDb (irM.c80), (int) x0, (int) y, (int) w, (int) rowH, juce::Justification::centredLeft, 1, 0.7f);
            y += rowH; g.setColour (theme::text);
        }
        y += 8;
        if (! big || edc.size() < 2 || r.getBottom() - y < 60) return;
        // the energy decay curve (Schroeder): T30 comes from its -5 .. -35 dB part, EDT from 0 .. -10 dB
        g.setColour (theme::muted); g.setFont (theme::ui (10.0f, true));
        g.drawText (ph.tr ("減衰曲線（シュレーダー積分、全帯域）", "ENERGY DECAY (Schroeder, broadband)"), (int) x0, (int) y, (int) w, 14, juce::Justification::centredLeft);
        g.setColour (theme::direct.withAlpha (0.8f)); g.setFont (theme::ui (9.0f));
        const bool stack = w < 360;      // (0.10.12) narrow: the note goes on its own line (no overlap with the title)
        if (stack) y += 14;
        g.drawText (ph.tr ("橙の線＝T30 の区間（−5〜−35 dB）", "orange lines = T30 range (-5..-35 dB)"), (int) x0, (int) y, (int) w, 14, juce::Justification::centredRight); y += 16;
        auto pr = juce::Rectangle<float> (x0 + 28, y, w - 30, r.getBottom() - y - 14);
        g.setColour (theme::well); g.fillRect (pr);
        auto X = [&] (double ms) { return pr.getX() + (float) (ms / edcMs) * pr.getWidth(); };
        auto Y = [&] (double db) { return pr.getY() + (float) (-db / 70.0) * pr.getHeight(); };
        g.setFont (theme::mono (9.0f));
        for (int db : { 0, -5, -10, -20, -35, -60 })
        {
            g.setColour (db == -5 || db == -35 ? theme::direct.withAlpha (0.35f) : db == -10 ? theme::text.withAlpha (0.2f) : theme::grid);
            g.drawHorizontalLine ((int) Y (db), pr.getX(), pr.getRight());
            if (db == -5 || db == -10) continue;
            g.setColour (theme::faint); g.drawText (minus (juce::String (db)), (int) x0, (int) Y (db) - 6, 24, 12, juce::Justification::centredRight);
        }
        const double st = edcMs > 6000 ? 2000 : edcMs > 2500 ? 1000 : 500;
        for (double t = 0; t <= edcMs; t += st)
        { g.setColour (theme::grid); g.drawVerticalLine ((int) X (t), pr.getY(), pr.getBottom());
          g.setColour (theme::faint); g.drawText (juce::String (t / 1000.0, st < 1000 ? 1 : 0) + " s", (int) X (t) - 18, (int) pr.getBottom() + 1, 36, 12, juce::Justification::centred); }
        juce::Path p;
        for (size_t k = 0; k < edc.size(); ++k)
        {
            const float xx = X (edcMs * (double) k / (double) edc.size()), yy = Y (juce::jlimit (-70.0, 0.0, (double) edc[k]));
            if (k == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
        }
        g.saveState(); g.reduceClipRegion (pr.toNearestInt());
        g.setColour (theme::accent); g.strokePath (p, juce::PathStrokeType (1.6f));
        if (m.t30Wide > 0)
        {   // the straight line T30 implies: -60 dB at T30
            g.setColour (theme::direct.withAlpha (0.8f));
            const float dl[] = { 4.0f, 3.0f }; juce::Path ln, d; ln.startNewSubPath (X (0), Y (0)); ln.lineTo (X (m.t30Wide * 1000.0), Y (-60));
            juce::PathStrokeType (1.0f).createDashedStroke (d, ln, dl, 2); g.fillPath (d);
        }
        g.restoreState();
        g.setColour (theme::faint); g.setFont (theme::ui (9.0f));
        g.drawText (ph.tr ("破線＝T30 の傾き（−60 dB までの時間＝RT）", "Dashed = T30 slope (time to -60 dB = RT)"), pr.reduced (6, 4), juce::Justification::bottomLeft);
    }
private:
    ParamHelp& ph;
};


// ===========================================================================
//  Tools on the spectrum monitor (RT + EQ): what a double-click adds, flat RT / flat EQ, undo / redo
// ===========================================================================
class MonitorTools : public juce::Component
{
public:
    MonitorTools (ParamHelp& h) : ph (h) {}
    std::function<void()> onRtFlat, onEqFlat;
    bool live = true;            // (0.8.0) off the RT + EQ layer: shown the same, greyed (undo / redo still work)
    void paint (juce::Graphics& g) override
    {
        r.clear(); float x = 0; const float H = (float) getHeight();
        auto btn = [&] (int id, const juce::String& t, float w, bool on, juce::Colour c)
        {
            juce::Rectangle<float> b (x, 1, w, H - 2); r.push_back ({ b, id }); x += w + 3;
            const bool dim = ! live && id <= 4; if (dim) { g.beginTransparencyLayer (0.35f); on = false; }
            g.setColour (on ? c : theme::shade (0.3f)); g.fillRoundedRectangle (b, 3.0f);
            g.setColour (on ? theme::panel : c); g.setFont (theme::ui (10.5f, true)); g.drawText (t, b, juce::Justification::centred);
            if (dim) g.endTransparencyLayer();
        };
        const int add = (int) ph.proc.uiState.getProperty ("addType", 0);
        g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
        const juce::String lab = ph.tr ("追加", "Add");
        g.drawText (lab, 0, 0, 30, (int) H, juce::Justification::centredLeft); x = 32;
        btn (1, "EQ", 34, add == 0, NodeGraph::eqCol());
        btn (2, "RT", 34, add == 1, NodeGraph::rtCol());
        x += 6;
        btn (3, ph.tr ("RT フラット", "RT flat"), 76, false, NodeGraph::rtCol());
        btn (4, ph.tr ("EQ フラット", "EQ flat"), 76, false, NodeGraph::eqCol());
        x += 6;
        btn (5, juce::String::fromUTF8 ("\xe2\x86\xb6"), 26, false, theme::text);
        btn (6, juce::String::fromUTF8 ("\xe2\x86\xb7"), 26, false, theme::text);
    }
    static int naturalWidth() { return 32 + 37 + 37 + 6 + 79 + 79 + 6 + 29 + 29; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& [b, id] : r)
            if (b.contains (e.position))
            {
                if (! live && id <= 4) return;
                if (id == 1 || id == 2) { ph.proc.uiState.setProperty ("addType", id - 1, nullptr); repaint(); }
                else if (id == 3 && onRtFlat) onRtFlat();
                else if (id == 4 && onEqFlat) onEqFlat();
                else if (id == 5) ph.proc.undo.undo();
                else if (id == 6) ph.proc.undo.redo();
                return;
            }
    }
private:
    ParamHelp& ph; std::vector<std::pair<juce::Rectangle<float>, int>> r;
};

// ===========================================================================
//  0.6.0: the DYN page's meter (gain reduction per band, the gate) and the MOD page's scope (LFO 1 / 2, envelope)
// ===========================================================================
class DynMeter : public juce::Component
{
public:
    explicit DynMeter (ParamHelp& h) : ph (h) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (r, 4.0f);
        auto& P = ph.proc;
        const int mode = (int) ph.value ("dkMode"), n = mode > 0 ? P.mDuckN.load() : 0;
        auto gate = r.removeFromBottom (34).reduced (6, 4);
        auto plot = r.reduced (6, 4).withTrimmedTop (14).withTrimmedBottom (14);
        g.setFont (theme::ui (9.5f, true)); g.setColour (theme::muted);
        g.drawText (ph.tr ("ダッキング量（dB）", "Ducking (dB)"), r.reduced (6, 2).withHeight (14), juce::Justification::centredLeft);
        const float depth = std::max (6.0f, ph.value ("dkDepth"));
        auto Y = [&] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, -db / depth) * plot.getHeight(); };
        g.setColour (theme::grid); for (int d = 0; d <= (int) depth; d += 6) g.drawHorizontalLine ((int) Y ((float) -d), plot.getX(), plot.getRight());
        g.setColour (theme::faint); g.setFont (theme::mono (8.5f));
        g.drawText ("0", plot.getX(), plot.getY() - 11, 20, 10, juce::Justification::centredLeft);
        g.drawText (minus ("-" + juce::String ((int) depth)), plot.getX(), plot.getBottom() + 1, 30, 10, juce::Justification::centredLeft);
        if (n <= 0) { g.setColour (theme::faint); g.setFont (theme::ui (10.0f)); g.drawText ("OFF", plot, juce::Justification::centred); }
        else
        {
            const float w = plot.getWidth() / (float) n;
            for (int b = 0; b < n; ++b)
            {
                const float db = std::min (0.0f, P.mDuck[(size_t) b].load());
                auto bar = juce::Rectangle<float>::leftTopRightBottom (plot.getX() + b * w + 1, plot.getY(), plot.getX() + (b + 1) * w - 1, Y (db));
                g.setColour (theme::accent.withAlpha (0.85f)); g.fillRect (bar);
            }
            if (mode == 2 || mode == 3)
            {
                g.setColour (theme::faint); g.setFont (theme::mono (8.0f));
                g.drawText (ph.tr ("低", "lo"), plot.getX(), (int) plot.getBottom() + 1, 20, 10, juce::Justification::centredLeft);
                g.drawText (ph.tr ("高", "hi"), (int) plot.getRight() - 20, (int) plot.getBottom() + 1, 20, 10, juce::Justification::centredRight);
            }
        }
        // the gate: a lamp and how open it is
        const bool on = ph.value ("gtOn") > 0.5f; const float gg = on ? P.mGate.load() : 1.0f;
        g.setColour (theme::muted); g.setFont (theme::ui (9.5f, true)); g.drawText ("GATE", gate.removeFromLeft (34), juce::Justification::centredLeft);
        auto lamp = gate.removeFromLeft (14).withSizeKeepingCentre (10, 10);
        g.setColour (! on ? theme::shade (0.3f) : gg > 0.5f ? theme::ok : theme::warn); g.fillEllipse (lamp);
        gate.removeFromLeft (6);
        g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (gate.withSizeKeepingCentre (gate.getWidth(), 8), 3.0f);
        if (on) { g.setColour (theme::accent); g.fillRoundedRectangle (gate.withSizeKeepingCentre (gate.getWidth(), 8).withWidth (gate.getWidth() * gg), 3.0f); }
    }
private:
    ParamHelp& ph;
};

class ModScope : public juce::Component
{
public:
    explicit ModScope (ParamHelp& h) : ph (h) {}
    void push()
    {
        auto& P = ph.proc;
        hist[0][(size_t) w] = P.mLfo1.load(); hist[1][(size_t) w] = P.mLfo2.load(); hist[2][(size_t) w] = P.mEnv.load() * 2 - 1;
        w = (w + 1) % kN; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::well); g.fillRoundedRectangle (r, 4.0f);
        const juce::Colour col[3] = { theme::accent, theme::target, theme::warn };
        const char* nm[3] = { "LFO 1", "LFO 2", "ENV" };
        const float rowH = (r.getHeight() - 4) / 3;
        for (int s = 0; s < 3; ++s)
        {
            auto row = juce::Rectangle<float> (r.getX() + 4, r.getY() + 2 + s * rowH, r.getWidth() - 8, rowH - 2);
            g.setColour (theme::grid); g.drawHorizontalLine ((int) row.getCentreY(), row.getX(), row.getRight());
            g.setColour (theme::faint); g.setFont (theme::ui (9.0f, true)); g.drawText (nm[s], row.withHeight (11), juce::Justification::centredLeft);
            juce::Path p;
            for (int k = 0; k < kN; ++k)
            {
                const float v = hist[s][(size_t) ((w + k) % kN)];
                const float x = row.getX() + row.getWidth() * k / (kN - 1), y = row.getCentreY() - v * (row.getHeight() * 0.42f);
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col[s]); g.strokePath (p, juce::PathStrokeType (1.4f));
        }
    }
private:
    static constexpr int kN = 90;
    ParamHelp& ph; std::array<std::array<float, kN>, 3> hist {}; int w = 0;
};
