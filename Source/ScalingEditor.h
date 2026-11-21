#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// Whole-window zoom (same as the other Lab plug-ins): laid out once at the design size and drawn
// through a scale transform. Saved with the session and remembered for new instances.
struct ZoomPrefs
{
    static juce::PropertiesFile& file()
    {
        static juce::PropertiesFile f ([] { juce::PropertiesFile::Options o;
                                            o.applicationName = "JPK Anatomy Plugins"; o.filenameSuffix = ".settings";
                                            o.osxLibrarySubFolder = "Application Support"; o.folderName = "JPK";
                                            return o; }());
        return f;
    }
    static double get (const juce::String& key, double def) { return file().getDoubleValue (key, def); }
    static void set (const juce::String& key, double v) { file().setValue (key, v); file().saveIfNeeded(); }
};

inline const juce::Array<float>& zoomSteps() { static const juce::Array<float> z { 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f }; return z; }

class ZoomButton : public juce::TextButton
{
public:
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<void (int)> setSkin;
    ZoomButton() { onClick = [this] { showMenu(); }; setTooltip ("Window size and look"); }
    void refresh() { if (get) setButtonText (juce::String (juce::roundToInt (get() * 100.0f)) + "%"); }
private:
    void showMenu()
    {
        juce::PopupMenu m; const float now = get ? get() : 1.0f;
        m.addSectionHeader ("Size");
        for (int i = 0; i < zoomSteps().size(); ++i)
            m.addItem (i + 1, juce::String (juce::roundToInt (zoomSteps()[i] * 100.0f)) + "%", true, std::abs (zoomSteps()[i] - now) < 0.01f);
        m.addSectionHeader ("Look");
        for (int q = 0; q < theme::numSkins(); ++q) { const int i = theme::skinAt (q); m.addItem (100 + i, theme::skinName (i), true, i == theme::current); }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r >= 100 && setSkin) setSkin (r - 100);
            else if (r > 0 && r <= zoomSteps().size() && set) set (zoomSteps()[r - 1]);
        });
    }
};

template <class View, class Proc>
class ScalingEditor : public juce::AudioProcessorEditor
{
public:
    explicit ScalingEditor (Proc& p) : juce::AudioProcessorEditor (p), proc (p), view (p)
    {
        baseW = View::designW; baseH = View::designH;
        addAndMakeVisible (view);
        view.setBounds (0, 0, baseW, baseH);
        view.onDesignChange = [this] { applyDesign(); };
        view.zoom.get = [this] { return zoom; };
        view.zoom.set = [this] (float z) { setZoom (z); };
        float z = proc.uiState.hasProperty ("zoom") ? (float) proc.uiState.getProperty ("zoom") : (float) ZoomPrefs::get ("rvlab.zoom", 1.0);
        setResizable (true, true);
        setResizeLimits (juce::roundToInt (baseW * 0.5f), juce::roundToInt (baseH * 0.5f), juce::roundToInt (baseW * 2.5f), juce::roundToInt (baseH * 2.5f));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
        setZoom (z);
        view.initMode();
    }
    // the view's design size changed (display mode): keep the zoom, change the window
    void applyDesign()
    {
        const auto d = view.designSize(); if (d.x == baseW && d.y == baseH) return;
        const float z = zoom;                      // (changing the limits resizes the window and would change the zoom)
        baseW = d.x; baseH = d.y;
        view.setBounds (0, 0, baseW, baseH);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio (0);
        setResizeLimits (juce::roundToInt (baseW * 0.5f), juce::roundToInt (baseH * 0.5f), juce::roundToInt (baseW * 2.5f), juce::roundToInt (baseH * 2.5f));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
        setSize (juce::roundToInt (baseW * z), juce::roundToInt (baseH * z));
        resized();
    }
    void setZoom (float z) { z = juce::jlimit (0.5f, 2.5f, z); setSize (juce::roundToInt (baseW * z), juce::roundToInt (baseH * z)); }
    void resized() override
    {
        zoom = (float) getWidth() / (float) baseW;
        view.setTransform (juce::AffineTransform::scale (zoom));
        proc.uiState.setProperty ("zoom", zoom, nullptr);
        ZoomPrefs::set ("rvlab.zoom", zoom);
        view.zoom.refresh();
    }
private:
    Proc& proc; View view; int baseW = 100, baseH = 100; float zoom = 1.0f;
};
