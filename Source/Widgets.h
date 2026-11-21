#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Theme.h"

struct ParamHelp
{
    explicit ParamHelp (ERProcessor& p) : proc (p) {}
    ERProcessor& proc;
    juce::RangedAudioParameter* get (const juce::String& id) const { return proc.apvts.getParameter (id); }
    float value (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const juce::String& id, float v) const { if (auto* p = get (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    void setGesture (const juce::String& id, float v) const { if (auto* p = get (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } }
    bool resetDefault (const juce::String& id) const
    {
        auto* p = get (id); if (p == nullptr) return false;
        proc.undo.beginNewTransaction();
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
        return true;
    }
    bool ja() const { return proc.uiState.getProperty ("lang", "ja").toString() == "ja"; }
    juce::String tr (const char* j, const char* e) const { return juce::String::fromUTF8 (ja() ? j : e); }
};

// parameter lock (0.8.0): a small padlock on a locked control, and the right-click menu that locks / unlocks it
inline void drawLockMark (juce::Graphics& g, juce::Rectangle<float> box)
{
    const float w = 7.0f, h = 6.0f, x = box.getRight() - w - 3, y = box.getY() + 6;
    g.setColour (theme::warn); g.fillRoundedRectangle (x, y, w, h, 1.0f);
    juce::Path sh; sh.addCentredArc (x + w * 0.5f, y, w * 0.32f, 3.2f, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.strokePath (sh, juce::PathStrokeType (1.3f));
}
inline bool anyLocked (const ParamHelp& ph, const juce::StringArray& ids) { for (auto& id : ids) if (ph.proc.isLocked (id)) return true; return false; }
inline void showLockMenu (ParamHelp& ph, const juce::StringArray& ids, juce::Component* target)
{
    if (ids.isEmpty()) return;
    const bool locked = anyLocked (ph, ids);
    juce::PopupMenu m;
    m.addItem (1, ph.tr ("ロック（プリセットを読んでも変えない）", "Lock (presets leave it as it is)"), true, locked);
    m.addItem (2, ph.tr ("ロックをすべて解除", "Unlock all"), ! ph.proc.lockedIds().isEmpty());
    juce::Component::SafePointer<juce::Component> safe (target);
    ParamHelp* p = &ph;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [p, ids, locked, safe] (int r)
    {
        if (r == 1) for (auto& id : ids) p->proc.setLocked (id, ! locked);
        if (r == 2) p->proc.uiState.setProperty ("locks", "", nullptr);
        if (r > 0 && safe != nullptr) { if (auto* top = safe->getTopLevelComponent()) top->repaint(); }
    });
}

// a button with a drawn triangle (the font's ◀ ▶ differ in size): dir -1 = left, +1 = right; the same triangle size everywhere
class TriButton : public juce::TextButton
{
public:
    int dir = 1;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, findColour (juce::TextButton::buttonColourId), over, down);
        const auto c = getLocalBounds().toFloat().getCentre(); const float h = 4.5f, w = 3.8f;
        juce::Path t; if (dir > 0) t.addTriangle (c.x - w + 0.8f, c.y - h, c.x - w + 0.8f, c.y + h, c.x + w + 0.8f, c.y); else t.addTriangle (c.x + w - 0.8f, c.y - h, c.x + w - 0.8f, c.y + h, c.x - w - 0.8f, c.y);
        g.setColour (findColour (juce::TextButton::textColourOffId).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f)); g.fillPath (t);
    }
};

// a button that also reacts to a long press (or a right-click): onHold, and then no click
class HoldButton : public juce::TextButton, private juce::Timer
{
public:
    std::function<void()> onHold;
    void mouseDown (const juce::MouseEvent& e) override
    {
        held = false;
        if (e.mods.isPopupMenu() && onHold) { held = true; onHold(); return; }
        startTimer (450); juce::TextButton::mouseDown (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        stopTimer();
        if (held) { held = false; setState (juce::Button::buttonNormal); return; }
        juce::TextButton::mouseUp (e);
    }
private:
    void timerCallback() override { stopTimer(); if (onHold && isMouseButtonDown()) { held = true; onHold(); } }
    bool held = false;
};

// hover help: title + Japanese + English, read by the editor from the component under the mouse
inline void setHelp (juce::Component& c, const char* title, const char* ja, const char* en)
{
    auto& p = c.getProperties();
    p.set ("helpT", juce::String::fromUTF8 (title)); p.set ("helpJ", juce::String::fromUTF8 (ja)); p.set ("helpE", juce::String::fromUTF8 (en));
}

inline juce::String minus (juce::String s) { return s.replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")); }
inline juce::String fmtHz (double f) { return f >= 1000 ? juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + " kHz" : juce::String ((int) std::round (f)) + " Hz"; }
inline juce::String fmtDb (double g, int d = 1) { return minus ((g > 0.04 ? "+" : "") + (d == 0 ? juce::String ((int) std::lround (g)) : juce::String (g, d))) + " dB"; }
inline juce::String fmtMs (double v) { return (v < 10 ? juce::String (v, 1) : juce::String ((int) std::lround (v))) + " ms"; }

// ---------------------------------------------------------------------------
//  DragValue: drag up / down (Shift = fine), wheel, double-click to type, Option+click = default
// ---------------------------------------------------------------------------
class DragValue : public juce::Component
{
public:
    DragValue (ParamHelp& h, juce::String id, juce::String captionText, std::function<juce::String (float)> fmt)
        : ph (h), paramID (std::move (id)), caption (std::move (captionText)), format (std::move (fmt)) { getProperties().set ("pid", paramID); }
    std::function<float (const juce::String&)> parse = [] (const juce::String& s) { auto t = s.trim(); float v = t.getFloatValue(); if (t.containsIgnoreCase ("k")) v *= 1000.0f; return v; };
    std::function<bool()> dimmed = [] { return false; };
    juce::Colour ring = juce::Colours::transparentBlack;      // driven by the hardware faceplate
    float flash = 0;                                         // 1 = just changed by the faceplate, fades out
    bool selected = false;                                   // the arrow keys move this one
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    bool fader = false;                                      // drawn as a slim vertical fader (INPUT / OUTPUT columns)
    void paintFader (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        const bool dim = dimmed();
        g.setColour (dim ? theme::faint : theme::muted); g.setFont (theme::ui (9.5f));
        g.drawFittedText (caption, 0, 0, getWidth(), 13, juce::Justification::centred, 1, 0.7f);
        g.setColour (dim ? theme::faint : (over || dragging ? theme::accent : theme::text)); g.setFont (theme::mono (10.0f, true));
        if (editor == nullptr) g.drawFittedText (format (ph.value (paramID)), 0, getHeight() - 14, getWidth(), 14, juce::Justification::centred, 1, 0.6f);
        auto tr = r.withTrimmedTop (17).withTrimmedBottom (18);
        const float cx = tr.getCentreX();
        g.setColour (theme::shade (0.35f)); g.fillRoundedRectangle (cx - 2.5f, tr.getY(), 5.0f, tr.getHeight(), 2.5f);
        if (auto* p = ph.get (paramID))
        {
            const float v = p->getValue(), y = tr.getBottom() - v * tr.getHeight();
            g.setColour (theme::accent.withAlpha (dim ? 0.25f : 0.8f)); g.fillRoundedRectangle (cx - 2.5f, y, 5.0f, tr.getBottom() - y, 2.5f);
            g.setColour (ring.getAlpha() > 0 ? ring : (dim ? theme::faint : theme::text)); g.fillRoundedRectangle (cx - 9, y - 3, 18.0f, 6.0f, 2.0f);
        }
        if (selected) { g.setColour (theme::accent); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
    }
    void paint (juce::Graphics& g) override
    {
        if (fader) { paintFader (g); return; }
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::hilite (over || dragging ? 0.16f : 0.07f)); g.fillRoundedRectangle (r, 4.0f);
        if (flash > 0.01f) { g.setColour (ring.withAlpha (0.35f * flash)); g.fillRoundedRectangle (r, 4.0f); }
        g.setColour (ring.getAlpha() > 0 ? ring : theme::line); g.drawRoundedRectangle (r, 4.0f, ring.getAlpha() > 0 ? 2.0f : 1.0f);
        if (selected) { g.setColour (theme::accent); juce::Path t; t.addTriangle (r.getX() + 3, r.getY() + 3, r.getX() + 11, r.getY() + 3, r.getX() + 3, r.getY() + 11); g.fillPath (t); }
        if (ph.proc.isLocked (paramID)) drawLockMark (g, r);
        if (editor != nullptr) return;
        const bool dim = dimmed();
        g.setColour (dim ? theme::faint : theme::text); g.setFont (theme::mono (getHeight() < 38 ? 11.5f : getHeight() < 46 ? 12.5f : 14.0f, true));
        g.drawFittedText (format (ph.value (paramID)), 2, 3, getWidth() - 4, getHeight() - 19, juce::Justification::centred, 1, 0.7f);
        g.setColour (dim ? theme::faint : theme::muted); g.setFont (theme::ui (getHeight() < 38 ? 9.0f : 10.0f));
        g.drawFittedText (caption, 2, getHeight() - (getHeight() < 38 ? 14 : 16), getWidth() - 4, 14, juce::Justification::centred, 1, 0.8f);
        // value bar
        if (auto* p = ph.get (paramID))
        {
            const float v = p->getValue();
            g.setColour (theme::accent.withAlpha (dim ? 0.2f : 0.55f));
            g.fillRect (r.getX() + 4, r.getBottom() - 2.5f, (r.getWidth() - 8) * v, 1.5f);
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showLockMenu (ph, { paramID }, this); return; }
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }
        if (auto* p = ph.get (paramID)) { ph.proc.undo.beginNewTransaction(); start = p->getValue(); lastY = e.getScreenPosition().toFloat().y; p->beginChangeGesture(); dragging = true; }
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* p = ph.get (paramID); if (p == nullptr || ! dragging) return;
        const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;
        const float y = e.getScreenPosition().toFloat().y;
        start = juce::jlimit (0.0f, 1.0f, start + (lastY - y) * sens); lastY = y;
        p->setValueNotifyingHost (start); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        if (auto* p = ph.get (paramID)) p->endChangeGesture();
        dragging = false; setMouseCursor (juce::MouseCursor::NormalCursor); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* p = ph.get (paramID); if (p == nullptr || w.isInertial || std::abs (w.deltaY) < 0.01f) return;
        const float step = (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
        p->beginChangeGesture(); p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + step)); p->endChangeGesture(); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        editor = std::make_unique<juce::TextEditor>();
        editor->setFont (theme::mono (13.0f, true)); editor->setJustification (juce::Justification::centred);
        editor->setText (format (ph.value (paramID)).upToFirstOccurrenceOf (" ", false, false).replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-"), false);
        editor->selectAll(); editor->setBounds (getLocalBounds().withTrimmedBottom (14));
        editor->onReturnKey = [this] { commit (true); };
        editor->onEscapeKey = [this] { commit (false); };
        editor->onFocusLost = [this] { commit (true); };
        addAndMakeVisible (*editor); editor->grabKeyboardFocus();
    }
    const juce::String paramID;
private:
    void commit (bool apply)
    {
        if (editor == nullptr) return;
        const auto txt = editor->getText();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<DragValue> (this)] { if (sp != nullptr) { sp->editor.reset(); sp->repaint(); } });
        if (apply && txt.trim().isNotEmpty())
            if (auto* p = ph.get (paramID)) ph.setGesture (paramID, p->getNormalisableRange().getRange().clipValue (parse (txt)));
    }
    ParamHelp& ph; juce::String caption; std::function<juce::String (float)> format;
    float start = 0, lastY = 0; bool over = false, dragging = false;
    std::unique_ptr<juce::TextEditor> editor;
};

// ---------------------------------------------------------------------------
//  Seg: segmented switch for a choice / bool / int parameter
// ---------------------------------------------------------------------------
// an on / off switch for a bool parameter, drawn as a small lit button with its name
class ParamToggle : public juce::Component
{
public:
    ParamToggle (ParamHelp& h, juce::String id, juce::String label) : paramID (std::move (id)), ph (h), text (std::move (label)) { getProperties().set ("pid", paramID); }
    void paint (juce::Graphics& g) override
    {
        const bool on = ph.value (paramID) > 0.5f; auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::accent : theme::shade (0.3f)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? theme::accent : theme::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (on ? theme::onAccent : theme::muted); g.setFont (theme::ui (9.5f, true)); g.drawText (text, r, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { ph.proc.undo.beginNewTransaction(); ph.setGesture (paramID, ph.value (paramID) > 0.5f ? 0.0f : 1.0f); repaint(); }
    const juce::String paramID;
private:
    ParamHelp& ph; juce::String text;
};

class Seg : public juce::Component
{
public:
    Seg (ParamHelp& h, juce::String id, juce::StringArray labels, int firstValue = 0) : ph (h), paramID (std::move (id)), items (std::move (labels)), base (firstValue) { getProperties().set ("pid", paramID); }
    void setLabels (juce::StringArray l) { items = std::move (l); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const int sel = (int) std::lround (ph.value (paramID)) - base;
        const float w = r.getWidth() / (float) items.size();
        for (int i = 0; i < items.size(); ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (theme::accent); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : theme::muted);
            g.setFont (theme::ui (11.0f, i == sel));
            g.drawFittedText (items[i], c.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }
        const int i = juce::jlimit (0, items.size() - 1, (int) (e.x * items.size() / std::max (1, getWidth())));
        ph.proc.undo.beginNewTransaction();
        ph.setGesture (paramID, (float) (i + base)); repaint();
        if (onChange) onChange();
    }
    std::function<void()> onChange;
    const juce::String paramID;
private:
    ParamHelp& ph; juce::StringArray items; int base;
};

// choice parameter as a menu
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (ParamHelp& h, juce::String id) : ph (h), paramID (std::move (id))
    {
        getProperties().set ("pid", paramID);
        onChange = [this] { if (! syncing) { ph.proc.undo.beginNewTransaction(); ph.setGesture (paramID, (float) (getSelectedId() - 1)); } };
    }
    void setItems (const juce::StringArray& s) { syncing = true; clear (juce::dontSendNotification); addItemList (s, 1); sync(); syncing = false; }
    void sync() { syncing = true; setSelectedId ((int) ph.value (paramID) + 1, juce::dontSendNotification); syncing = false; }
    const juce::String paramID;
private:
    ParamHelp& ph; bool syncing = false;
};

// (0.10.8) a small A | B switch for what a panel shows (not a parameter)
class UiAB : public juce::Component
{
public:
    std::function<int()> get; std::function<void (int)> set;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const int sel = get ? get() : 0;
        for (int i = 0; i < 2; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * r.getWidth() * 0.5f, r.getY(), r.getWidth() * 0.5f, r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (theme::accent); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : theme::muted); g.setFont (theme::ui (11.0f, i == sel));
            g.drawText (i == 0 ? "A" : "B", c.toNearestInt(), juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { const int i = e.x * 2 / std::max (1, getWidth()) > 0 ? 1 : 0; if (set) set (i); repaint(); }
};

// (0.10.10) a small segmented switch for UI state (not a parameter)
class UiSeg : public juce::Component
{
public:
    juce::StringArray items; std::function<int()> get; std::function<void (int)> set;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.3f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const int sel = get ? get() : 0; const float w = r.getWidth() / (float) std::max (1, items.size());
        for (int i = 0; i < items.size(); ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (isEnabled() ? theme::accent : theme::faint); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : theme::muted); g.setFont (theme::ui (10.5f, i == sel));
            g.drawFittedText (items[i], c.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { if (items.isEmpty()) return; const int i = juce::jlimit (0, items.size() - 1, e.x * items.size() / std::max (1, getWidth())); if (set) set (i); repaint(); }
};
