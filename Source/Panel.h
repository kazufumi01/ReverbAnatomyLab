#pragma once
#include "Widgets.h"

// ===========================================================================
//  Hardware faceplate: the classic unit's own controls. Moving one writes the mapped
//  ALGO / VOICE / OUT parameters (see units() in Presets.h), which light up below.
// ===========================================================================
class HardwarePanel : public juce::Component
{
public:
    HardwarePanel (ParamHelp& h) : ph (h) {}
    std::function<void (const juce::String&)> onTouch;     // "SHAPE 120 -> 191" for the notes
    bool isVertical() const { return getHeight() > getWidth() * 0.45f; }
    // height for the upright layout (right column): title + display, then up to 4 controls per row
    static int uprightHeight (int nCtl) { const int rows = (std::max (1, nCtl) + 3) / 4; return 120 + rows * 96; }

    void paint (juce::Graphics& g) override
    {
        const int u = ph.proc.unitIndex(); if (u < 0 || u >= (int) units().size()) return;
        const auto& U = units()[(size_t) u];
        const auto face = juce::Colour (U.bg), ink = juce::Colour (U.ink);
        const bool light = face.getPerceivedBrightness() > 0.5f;
        auto b = getLocalBounds().toFloat();
        const bool vert = isVertical();
        // rack ears (left / right; top / bottom when the panel stands upright in the right column)
        const juce::Colour ear = light ? face.darker (0.25f) : face.brighter (0.12f);
        g.setColour (ear); g.fillRoundedRectangle (b, 3.0f);
        if (vert)
        {
            for (float y : { b.getY() + 6.0f, b.getBottom() - 6.0f })
                for (float x : { b.getX() + 22.0f, b.getRight() - 22.0f })
                { g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (x - 6, y - 3, 12, 6, 3); }
            face_ = b.reduced (2, 12);
        }
        else
        {
            for (float x : { b.getX() + 9.0f, b.getRight() - 9.0f })
                for (float y : { b.getY() + 22.0f, b.getBottom() - 22.0f })
                { g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (x - 4, y - 6, 8, 12, 4); }
            face_ = b.reduced (20, 2);
        }
        g.setGradientFill (juce::ColourGradient (face.brighter (light ? 0.05f : 0.10f), 0, face_.getY(), face.darker (0.10f), 0, face_.getBottom(), false));
        g.fillRect (face_);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRect (face_, 1.0f);
        // title
        g.setColour (ink); g.setFont (theme::ui (12.5f, true));
        g.drawText (U.face, (int) face_.getX() + 14, (int) face_.getY() + 6, vert ? (int) face_.getWidth() - 120 : 330, 16, juce::Justification::centredLeft);
        g.setColour (ink.withAlpha (0.6f)); g.setFont (theme::ui (9.5f));
        g.drawText (juce::String (U.era),
                    (int) face_.getX() + 14, (int) face_.getY() + 21, 380, 12, juce::Justification::centredLeft);
        // display
        disp = { face_.getX() + 14, face_.getY() + 38, vert ? 168.0f : 176.0f, 44 };
        g.setColour (juce::Colour (U.dispBg)); g.fillRoundedRectangle (disp, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (disp, 3.0f, 1.5f);
        const auto din = juce::Colour (U.dispInk);
        const auto vals = ph.proc.panelValues(); const int prog = ph.proc.unitProg();
        g.setColour (din); g.setFont (theme::mono (U.disp == 1 ? 12.0f : 13.0f, true));
        g.drawText (juce::String (U.programs[(size_t) juce::jlimit (0, (int) U.programs.size() - 1, prog)]), disp.reduced (8, 4).withHeight (18).toNearestInt(), juce::Justification::centredLeft);
        if (last >= 0 && last < (int) U.ctl.size() && last < (int) vals.size())
        {
            g.setFont (theme::mono (12.0f));
            g.drawText (juce::String (U.ctl[(size_t) last].label) + " " + fmt (U.ctl[(size_t) last], vals[(size_t) last]),
                        disp.reduced (8, 4).withTrimmedTop (19).toNearestInt(), juce::Justification::centredLeft);
        }
        // program buttons (under the display; upright: beside it, two per row)
        progR.clear();
        const int np = (int) U.programs.size();
        for (int i = 0; i < np; ++i)
        {
            juce::Rectangle<float> r;
            if (vert)
            {
                const float x0p = disp.getRight() + 10, wp = (face_.getRight() - 12 - x0p) / 2.0f, hp = np > 2 ? 21.0f : 44.0f;
                r = { x0p + (i % 2) * wp, disp.getY() + (i / 2) * (hp + 2), wp - 4, hp };
            }
            else r = { disp.getX() + i * (disp.getWidth() / (float) np), disp.getBottom() + 8, disp.getWidth() / (float) np - 4, 26 };
            progR.push_back (r);
            g.setColour (light ? face.darker (0.18f) : face.brighter (0.16f)); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (i == prog ? juce::Colour (U.accent) : juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (r.getX() + 4, r.getY() + 4, 5, 5);
            g.setColour (ink.withAlpha (i == prog ? 1.0f : 0.7f)); g.setFont (theme::ui (8.5f, i == prog));
            g.drawFittedText (U.programs[(size_t) i], r.withTrimmedLeft (8).toNearestInt(), juce::Justification::centred, 2, 0.6f);
        }
        // controls
        ctlR.clear();
        const int nc = (int) U.ctl.size();
        const float x0 = vert ? face_.getX() + 12 : disp.getRight() + 24;
        const int cols = vert ? std::min (nc, 4) : nc, rows = (nc + cols - 1) / std::max (1, cols);
        const float w = (face_.getRight() - 12 - x0) / (float) std::max (1, cols);
        const float cy0 = vert ? disp.getBottom() + 10 : face_.getY() + 6, ch = vert ? (face_.getBottom() - 16 - cy0) / (float) rows : face_.getHeight() - 10;
        const auto outs = ph.proc.panelOutputs();
        for (int i = 0; i < nc; ++i)
        {
            const auto& c = U.ctl[(size_t) i];
            const float cx = x0 + w * ((i % cols) + 0.5f), v = i < (int) vals.size() ? vals[(size_t) i] : c.lo;
            const float t = norm (c, v);
            juce::Rectangle<float> r (cx - w * 0.5f + 2, cy0 + (i / cols) * ch, w - 4, ch - (vert ? 4 : 0)); ctlR.push_back (r);
            g.setColour (ink); g.setFont (theme::ui (9.5f, true));
            g.drawFittedText (c.label, r.withHeight (14).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            if (c.kind == 0)
            {
                const float R = std::min ({ 19.0f, w * 0.32f, (r.getHeight() - 36) * 0.5f }), cy = r.getY() + 20 + R + 4;
                for (int k = 0; k <= 10; ++k)
                {
                    const float a = juce::MathConstants<float>::pi * (1.25f - 1.5f * k / 10.0f);
                    g.setColour (ink.withAlpha (0.55f));
                    g.drawLine (cx + std::cos (a) * (R + 3), cy - std::sin (a) * (R + 3), cx + std::cos (a) * (R + 6), cy - std::sin (a) * (R + 6), 1.0f);
                }
                g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (cx - R + 1, cy - R + 2, 2 * R, 2 * R);
                g.setColour (light ? juce::Colour (0xff2a2a2a) : juce::Colour (0xff121212)); g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
                g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawEllipse (cx - R + 1, cy - R + 1, 2 * R - 2, 2 * R - 2, 1.0f);
                const float a = juce::MathConstants<float>::pi * (1.25f - 1.5f * t);
                g.setColour (i == last ? juce::Colour (U.accent).brighter (0.3f) : juce::Colours::white.withAlpha (0.9f));
                g.drawLine (cx + std::cos (a) * R * 0.25f, cy - std::sin (a) * R * 0.25f, cx + std::cos (a) * (R - 3), cy - std::sin (a) * (R - 3), 2.2f);
            }
            else
            {
                const float top = r.getY() + 20, bot = r.getBottom() - 30;
                g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (cx - 2.5f, top, 5, bot - top, 2.5f);
                for (int k = 0; k <= 10; ++k) { const float y = bot - (bot - top) * k / 10.0f; g.setColour (ink.withAlpha (0.4f)); g.drawLine (cx - 12, y, cx - 7, y, 1.0f); }
                const float y = bot - (bot - top) * t;
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (cx - 11, y - 6, 22, 14, 2);
                g.setColour (U.disp == 0 && ! light ? juce::Colour (0xffe8e8e8) : juce::Colour (0xffd8402a));
                if (u == 1) g.setColour (juce::Colour (0xffeeeeee));
                g.fillRoundedRectangle (cx - 11, y - 7, 22, 13, 2);
                g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawLine (cx - 9, y, cx + 9, y, 1.2f);
            }
            g.setColour (i == last ? juce::Colour (U.accent).brighter (light ? 0.0f : 0.4f) : ink.withAlpha (0.85f)); g.setFont (theme::mono (10.0f, true));
            g.drawFittedText (fmt (c, v), r.removeFromBottom (14).toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        // what the panel drives
        juce::StringArray ids; for (auto& kv : outs) ids.addIfNotAlreadyThere (kv.first);
        g.setColour (ink.withAlpha (0.55f)); g.setFont (theme::ui (9.0f));
        g.drawFittedText (ph.tr ("→ 動かす内部パラメーター: ", "-> drives: ") + ids.joinIntoString (", "),
                          (int) face_.getX() + 14, (int) face_.getBottom() - 15, vert ? (int) face_.getWidth() - 28 : (int) (disp.getRight() + 20 - face_.getX() - 14), 12, juce::Justification::centredLeft, 1, 0.7f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int u = ph.proc.unitIndex(); if (u < 0) return;
        const auto& U = units()[(size_t) u];
        const int i = hit (e.position);
        if (i >= 0) setHelp (*this, U.ctl[(size_t) i].label, U.ctl[(size_t) i].ja, U.ctl[(size_t) i].en);
        else setHelp (*this, "Faceplate", "実機の操作系を再現したパネル。つまみやフェーダーを上下にドラッグ（Shift で微調整、ダブルクリックでプログラムの初期値）。下の ALGO・VOICE の枠で光っている項目が、このパネルで動く内部パラメーターです。",
                      "The unit's own controls. Drag knobs and faders up / down (Shift = fine, double-click = the program's value). The items lit in ALGO / VOICE below are what this panel drives.");
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int u = ph.proc.unitIndex(); if (u < 0) return;
        for (int i = 0; i < (int) progR.size(); ++i)
            if (progR[(size_t) i].contains (e.position))
            {
                for (size_t k = 0; k < presets().size(); ++k) if (presets()[k].unit == u && presets()[k].prog == i) { ph.proc.applyPreset ((int) k); break; }
                repaint(); return;
            }
        drag = hit (e.position); if (drag < 0) return;
        last = drag;
        const auto& c = units()[(size_t) u].ctl[(size_t) drag];
        const auto v = ph.proc.panelValues();
        if (e.mods.isAltDown()) { resetCtl (drag); drag = -1; return; }
        startV = v[(size_t) drag]; startN = norm (c, startV); lastY = e.getScreenPosition().toFloat().y;
        ph.proc.undo.beginNewTransaction();
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int u = ph.proc.unitIndex(); if (u < 0 || drag < 0) return;
        const auto& c = units()[(size_t) u].ctl[(size_t) drag];
        const float y = e.getScreenPosition().toFloat().y;
        startN = juce::jlimit (0.0f, 1.0f, startN + (lastY - y) * (e.mods.isShiftDown() ? 0.0012f : 0.006f)); lastY = y;
        ph.proc.setPanelValue (drag, denorm (c, startN));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        const int u = ph.proc.unitIndex();
        if (u >= 0 && drag >= 0 && onTouch)
        {
            const auto& c = units()[(size_t) u].ctl[(size_t) drag];
            onTouch (juce::String (c.label) + " " + fmt (c, startV) + " -> " + fmt (c, ph.proc.panelValues()[(size_t) drag]));
        }
        drag = -1;
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override { const int i = hit (e.position); if (i >= 0) resetCtl (i); }

private:
    void resetCtl (int i)
    {
        const auto d = ph.proc.panelDefaults(); if (i >= (int) d.size()) return;
        ph.proc.undo.beginNewTransaction(); ph.proc.setPanelValue (i, d[(size_t) i]); last = i; repaint();
    }
    int hit (juce::Point<float> p) const { for (int i = 0; i < (int) ctlR.size(); ++i) if (ctlR[(size_t) i].contains (p)) return i; return -1; }
    static float norm (const PanelCtl& c, float v)
    {
        if (c.log && c.lo > 0) return (float) (std::log (v / c.lo) / std::log (c.hi / c.lo));
        return (v - c.lo) / (c.hi - c.lo);
    }
    static float denorm (const PanelCtl& c, float t)
    {
        float v = c.log && c.lo > 0 ? c.lo * std::pow (c.hi / c.lo, t) : c.lo + t * (c.hi - c.lo);
        if (c.dec == 0) v = std::round (v);
        return v;
    }
    static juce::String fmt (const PanelCtl& c, float v)
    {
        const juce::String u (c.unit);
        if (u == "m3") return "10^" + juce::String (v, 1);
        if (u == "Hz") return v >= 1000 ? juce::String (v / 1000, 1) + "k" : juce::String ((int) std::round (v));
        juce::String s = c.dec == 0 ? juce::String ((int) std::round (v)) : juce::String (v, c.dec);
        if (u == "dB" && v > 0.04f) s = "+" + s;
        return u.isEmpty() || u == "x" ? s : s + " " + u;
    }
    ParamHelp& ph; juce::Rectangle<float> face_, disp; std::vector<juce::Rectangle<float>> ctlR, progR;
    int drag = -1, last = -1; float startV = 0, startN = 0, lastY = 0;
};
