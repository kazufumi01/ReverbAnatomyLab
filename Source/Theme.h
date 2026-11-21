#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Looks shared with the other Lab plug-ins (same palettes as Compressor Anatomy Lab 2),
// plus the ER colours: direct (amber), target (violet), left / right reflections.
namespace theme
{
inline juce::Colour bg, header, panel, well, line, grid, text, muted, faint, accent, onAccent, ok, side, popup, direct, target, warn;
inline bool lightPanels = false;
inline int current = 0;

inline int numSkins() { return 10; }
// (0.10.12) the menu order: Soft Dusk (dim) first
inline int skinAt (int menuPos) { return menuPos == 0 ? 9 : menuPos - 1; }
// (0.10.12) the user's adjustments (-1..1, 0 = as the look is): text weight, overall brightness, contrast
inline float adjText = 0.0f, adjBright = 0.0f, adjContrast = 0.0f;
inline const char* skinName (int i)
{
    static const char* n[] = { "Studio", "Lab Light", "Studio Night", "Blueprint", "Lesson Warm", "Soft Grey", "Soft Dusk", "Study Note (Paper)", "Study Note (Dark)", "Soft Dusk (dim)" };
    return n[juce::jlimit (0, 9, i)];
}
inline void setSkin (int i)
{
    current = juce::jlimit (0, 9, i);
    struct P { juce::uint32 bg, header, panel, well, line, grid, text, muted, faint, accent, onAccent, ok, side, popup, direct, target, warn; bool light; };
    static const P p[] = {
        { 0xff121417, 0xff191c21, 0xff191c21, 0xff0d0f12, 0xff2a2e35, 0xff1d2126, 0xffe4e1da, 0xff8d929a, 0xff6c727b, 0xff5ccfc4, 0xff06201d, 0xff8fc779, 0xff6fa8e6, 0xff1c2026, 0xfff0b34a, 0xffa592f0, 0xffef7a6e, false },
        { 0xfff6f7f9, 0xffe9edf1, 0xffffffff, 0xfffbfcfd, 0xffc8cfd7, 0xffe6e9ed, 0xff16202b, 0xff566271, 0xff7a8592, 0xff0d7a72, 0xffffffff, 0xff3c8a3a, 0xff2f6fb0, 0xffffffff, 0xffb9740e, 0xff6a52c4, 0xffb83f36, true },
        { 0xff0b0c0e, 0xff121418, 0xff15181c, 0xff07080a, 0xff23272e, 0xff15191e, 0xffeceae4, 0xff8d929a, 0xff646a73, 0xff63ddd1, 0xff06201d, 0xff8fd27a, 0xff79b4f0, 0xff121418, 0xffffbf47, 0xffb3a2ff, 0xffff7f70, false },
        { 0xffe1e6eb, 0xffd3dae2, 0xffeef1f4, 0xfff6f7f9, 0xffb9c1ca, 0xffdfe3e8, 0xff16202b, 0xff566271, 0xff7a8592, 0xff8a5a1c, 0xfffbf3e8, 0xff3c8a3a, 0xff0d6575, 0xffffffff, 0xffc47410, 0xff6a52c4, 0xffb83f36, true },
        { 0xff15140f, 0xff1d1b15, 0xff1f1d17, 0xff100f0b, 0xff34302a, 0xff1e1c16, 0xffece5d4, 0xff9a9282, 0xff746d60, 0xff7cc0c9, 0xff0b1a1c, 0xff8fc779, 0xff8aa0d8, 0xff221f18, 0xffe8a33a, 0xffb39be8, 0xffef7a6e, false },
        { 0xffc9ced4, 0xffd2d6db, 0xffdadee2, 0xffe1e4e7, 0xffa8b0b9, 0xffcfd3d8, 0xff1b232c, 0xff4f5a66, 0xff6b7581, 0xff0d6575, 0xffffffff, 0xff357a33, 0xff2a5f9a, 0xffe4e7ea, 0xffad640d, 0xff5b46b0, 0xffa5382f, true },
        { 0xffbcc5d0, 0xffc7cfd8, 0xffd1d8e0, 0xffd9dfe6, 0xff98a5b4, 0xffc5cdd7, 0xff18212d, 0xff47546a, 0xff657187, 0xff355e9c, 0xffffffff, 0xff357a33, 0xff1f6f8a, 0xffdde3e9, 0xffad640d, 0xff5b46b0, 0xffa5382f, true },
        // the web Study Notes: paper (note-kit: paper, page, ink, pencil, blue ink, red) and their dark page
        { 0xffefe8d8, 0xffe5dbc8, 0xfff8f3e8, 0xfffbf8f1, 0xffd2c6af, 0xffe9e0cd, 0xff2b2722, 0xff776d60, 0xffaea493, 0xff2c4a6e, 0xfff8f3e8, 0xff2f7d74, 0xff2869b0, 0xfff8f3e8, 0xffb0741c, 0xff6a52a4, 0xffa5472a, true },
        { 0xff0f1318, 0xff161c24, 0xff161c24, 0xff0b0e12, 0xff28323f, 0xff1c2430, 0xffe4e8ee, 0xff8c97a8, 0xff566172, 0xff5ccfc4, 0xff06201d, 0xff8fc779, 0xff5aa4e6, 0xff1c2430, 0xfff0b44c, 0xff9c86ea, 0xffc76a7e, false },
    };
    const bool dim = current == 9;                 // Soft Dusk with the surfaces' brightness down by 20 %
    const auto& s = p[dim ? 6 : current];
    auto C = [] (juce::uint32 v) { return juce::Colour (v); };
    auto Sf = [dim] (juce::uint32 v) { auto c = juce::Colour (v); return dim ? c.withMultipliedBrightness (0.8f) : c; };
    bg = Sf (s.bg); header = Sf (s.header); panel = Sf (s.panel); well = Sf (s.well); line = Sf (s.line); grid = Sf (s.grid);
    text = C (s.text); muted = C (s.muted); faint = C (s.faint); accent = C (s.accent); onAccent = C (s.onAccent); ok = C (s.ok);
    side = C (s.side); popup = Sf (s.popup); direct = C (s.direct); target = C (s.target); warn = C (s.warn); lightPanels = s.light;
    // (0.10.12) text a little stronger than the palettes (muted and faint most), then the user's three adjustments
    const auto ext = lightPanels ? juce::Colours::black : juce::Colours::white;
    auto toward = [] (juce::Colour c, juce::Colour t, float a) { return c.interpolatedWith (t, juce::jlimit (0.0f, 0.95f, a)); };
    text = toward (text, ext, 0.20f + 0.40f * adjText + 0.25f * adjContrast);
    muted = toward (muted, ext, 0.38f + 0.40f * adjText + 0.25f * adjContrast);
    faint = toward (faint, ext, 0.36f + 0.40f * adjText + 0.25f * adjContrast);
    auto bright = [] (juce::Colour c, float b) { return b >= 0 ? c.interpolatedWith (juce::Colours::white, b * 0.25f) : c.interpolatedWith (juce::Colours::black, -b * 0.25f); };
    auto spread = [&] (juce::Colour c, float k) { const auto b = bg; auto f = [k] (float v, float r) { return juce::jlimit (0.0f, 1.0f, r + (v - r) * k); };
                                                  return juce::Colour::fromFloatRGBA (f (c.getFloatRed(), b.getFloatRed()), f (c.getFloatGreen(), b.getFloatGreen()), f (c.getFloatBlue(), b.getFloatBlue()), c.getFloatAlpha()); };
    const float k = 1.0f + 0.9f * adjContrast;
    header = spread (header, k); panel = spread (panel, k); well = spread (well, k); line = spread (line, k); grid = spread (grid, k); popup = spread (popup, k);
    bg = bright (bg, adjBright); header = bright (header, adjBright); panel = bright (panel, adjBright); well = bright (well, adjBright);
    line = bright (line, adjBright); grid = bright (grid, adjBright); popup = bright (popup, adjBright);
}
inline juce::Colour comboBg() { return lightPanels ? juce::Colours::white : panel.brighter (0.06f); }
inline juce::Colour shade (float a)  { return lightPanels ? juce::Colours::black.withAlpha (a * 0.25f) : juce::Colours::black.withAlpha (a); }
inline juce::Colour hilite (float a) { return lightPanels ? juce::Colours::black.withAlpha (a * 0.6f) : juce::Colours::white.withAlpha (a); }

inline juce::Font ui (float size, bool bold = false)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Hiragino Sans", size, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
   #endif
}
inline juce::Font mono (float size, bool bold = false)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Menlo", size, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
   #endif
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel() { apply(); }
    void apply()
    {
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, onAccent);
        setColour (juce::ComboBox::backgroundColourId, comboBg());
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, muted);
        setColour (juce::PopupMenu::backgroundColourId, popup);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, muted);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, onAccent);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, accent);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if ((bool) b.getProperties().getWithDefault ("comboLook", false))
        {   // (0.10.12) drawn like the preset box: the same ground, frame and arrow
            g.setColour (comboBg()); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (over || down ? accent : line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
            juce::Path a; const float cx = r.getRight() - 12, cy = r.getCentreY(); a.startNewSubPath (cx - 4, cy - 2); a.lineTo (cx, cy + 2); a.lineTo (cx + 4, cy - 2);
            g.setColour (muted); g.strokePath (a, juce::PathStrokeType (1.6f)); return;
        }
        auto col = b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
        if (! on && (over || down)) col = lightPanels ? col.darker (0.06f) : col.brighter (0.12f);
        g.setColour (col); g.fillRoundedRectangle (r, 4.0f);
        if (! on) { g.setColour (line); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        if (! (bool) b.getProperties().getWithDefault ("comboLook", false)) { juce::LookAndFeel_V4::drawButtonText (g, b, over, down); return; }
        g.setColour (text); g.setFont (ui (12.0f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (8).withTrimmedRight (22), juce::Justification::centredLeft, 1, 0.8f);
    }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        const float px = (float) (double) b.getProperties().getWithDefault ("fontPx", 12.0);
        return ui (juce::jmin (px, h * 0.5f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui (12.0f); }
    juce::Font getPopupMenuFont() override { return ui (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

inline const bool initialised = [] { setSkin (0); return true; }();
}
