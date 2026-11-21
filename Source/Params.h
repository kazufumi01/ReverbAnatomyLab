#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ERCore.h"

// Parameter IDs. New parameters go at the end with a default that keeps the old sound.
namespace pid
{
inline const char* const ids[] = {
    "src",
    "roomW", "roomD", "roomH", "wall", "floor", "order", "srcX", "srcY", "lisX", "lisY",
    "first", "length", "taps", "growth", "energy", "decay", "damp", "vari",
    "irMode", "irWin", "irAuto", "irThr", "irWet",
    "predelay", "size", "width", "diffusion", "lowcut", "highcut", "level",
    "bw", "bits", "wander",
    "mix", "out", "tail", "tailRT", "tailLvl",
    "hcSlope", "lcSlope", "autoLvl", "darken", "irSplit", "irFill", "roomEarly",
    // Reverb Anatomy Lab
    "tOn", "erOn", "tDecay", "tSize", "tPre", "tDiff", "tModD", "tModR", "tLvl", "tFreeze", "rtMode", "rtF1", "rtF2", "rtF3", "rtF4", "rtF5", "rtM1", "rtM2", "rtM3", "rtM4", "rtM5", "mbN", "mbX1", "mbX2", "mbX3", "mbX4", "mbM1", "mbM2", "mbM3", "mbM4", "mbM5", "inMode", "inW", "outMode", "outW", "iLC", "iHC", "iLCs", "iHCs", "iTilt", "iTo", "eqOn", "eqHP", "eqLP", "eqF1", "eqG1", "eqQ1", "eqF2", "eqG2", "eqQ2", "eqF3", "eqG3", "eqQ3", "eqF4", "eqG4", "eqQ4", "eqF5", "eqG5", "eqQ5", "eqF6", "eqG6", "eqQ6",
    "rtQ1", "rtQ2", "rtQ3", "rtQ4", "rtQ5", "eqB1", "eqB2", "eqB3", "eqB4", "eqB5", "eqB6",
    "tSrc", "irtX", "irtLen", "irtSt", "irtLvl", "irtDL", "irtDM", "irtDH", "irtRev", "irtB",
    "erBal", "tEng", "tRoom", "aGrav", "aFb", "irtAuto",
    // 0.6.0: ducking, gate, chorus, LFO x2, envelope, mod matrix
    "dkMode", "dkDepth", "dkThr", "dkAtk", "dkRel", "dkBands", "dkTo",
    "gtOn", "gtThr", "gtHold", "gtRel", "gtKey",
    "chMix", "chRate", "chDepth",
    "l1Rate", "l1Shape", "l1Sync", "l2Rate", "l2Shape", "l2Sync", "enAtk", "enRel", "enSens",
    "m1Src", "m1Dst", "m1Amt", "m2Src", "m2Dst", "m2Amt", "m3Src", "m3Dst", "m3Amt", "m4Src", "m4Dst", "m4Amt",
    // 0.6.1: build-up and the tail's low width
    "tSpread", "tShape", "tBassW",
    // 0.7.2: the Hall engine's onset fill
    "tFill",
    // 0.7.3: build-up per band
    "tSpreadHi", "colEr", "colTail", "tLC", "tHC", "tLCs", "tHCs", "rbW", "rbD", "rbH", "rbWall", "rbFloor", "erRoomSel", "tRoomSel", "tIrSel", "erIrSel", "tBody", "shape", "splay", "rbShape", "rbSplay", "backAbs", "rbBackAbs", "clutter", "rbClutter" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, float centre = -1, const char* unit = "")
    {
        NormalisableRange<float> r (lo, hi);
        if (centre > 0) r.setSkewForCentre (centre);
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto it = [&] (const char* id, const char* name, int lo, int hi, int def)
    { L.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto ch = [&] (const char* id, const char* name, StringArray items, int def)
    { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto bo = [&] (const char* id, const char* name, bool def)
    { L.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    StringArray mats; for (auto& m : er::materials()) mats.add (m.en);

    ch ("src", "Source", { "Room", "Algo", "IR" }, 0);
    // ROOM
    fl ("roomW", "Room Width", 1.5f, 40.0f, 6.0f, 8.0f, "m");
    fl ("roomD", "Room Depth", 1.5f, 50.0f, 8.0f, 10.0f, "m");
    fl ("roomH", "Room Height", 2.0f, 20.0f, 3.2f, 5.0f, "m");
    ch ("wall", "Walls", mats, 2);
    ch ("floor", "Floor", mats, 3);
    it ("order", "Reflection Order", 1, 5, 3);
    fl ("srcX", "Source X", 0.05f, 0.95f, 0.5f);
    fl ("srcY", "Source Y", 0.05f, 0.95f, 0.3f);
    fl ("lisX", "Listener X", 0.05f, 0.95f, 0.5f);
    fl ("lisY", "Listener Y", 0.05f, 0.95f, 0.7f);
    // ALGO
    fl ("first", "First Reflection", 1.0f, 80.0f, 6.0f, 12.0f, "ms");
    fl ("length", "ER Length", 8.0f, 300.0f, 50.0f, 60.0f, "ms");
    it ("taps", "Reflections", 4, 120, 24);
    fl ("growth", "Growth", 0.0f, 2.5f, 1.0f);
    fl ("energy", "ER Energy", -30.0f, 12.0f, -8.0f, -1, "dB");
    fl ("decay", "Envelope", -40.0f, 24.0f, -14.0f, -1, "dB");
    fl ("damp", "Damping", 1500.0f, 16000.0f, 7000.0f, 5000.0f, "Hz");
    it ("vari", "Variation", 1, 16, 1);
    // IR
    ch ("irMode", "IR Mode", { "Taps", "Convolution", "Hybrid" }, 0);      // Hybrid added in 0.5.0 (appended: old indices stay)
    fl ("irWin", "IR Window", 20.0f, 300.0f, 80.0f, 80.0f, "ms");
    bo ("irAuto", "IR Window Auto", true);
    fl ("irThr", "IR Threshold", -60.0f, -10.0f, -40.0f, -1, "dB");
    bo ("irWet", "IR Wet Only", false);
    // VOICE (applies to every source)
    fl ("predelay", "Predelay", 0.0f, 200.0f, 0.0f, 30.0f, "ms");
    fl ("size", "Size", 50.0f, 200.0f, 100.0f, 100.0f, "%");
    fl ("width", "Width", 0.0f, 150.0f, 100.0f, -1, "%");
    fl ("diffusion", "Diffusion", 0.0f, 100.0f, 0.0f, -1, "%");
    fl ("lowcut", "Low Cut", 20.0f, 1000.0f, 20.0f, 150.0f, "Hz");
    fl ("highcut", "High Cut", 1000.0f, 20000.0f, 20000.0f, 6000.0f, "Hz");
    fl ("level", "ER Level", -24.0f, 12.0f, 0.0f, -1, "dB");
    // COLOUR
    fl ("bw", "Bandwidth", 4.0f, 20.0f, 20.0f, 10.0f, "kHz");
    fl ("bits", "Bits", 8.0f, 24.0f, 24.0f, -1, "bit");
    fl ("wander", "Wander", 0.0f, 100.0f, 0.0f, -1, "%");
    // OUT
    fl ("mix", "Mix", 0.0f, 100.0f, 50.0f, -1, "%");
    fl ("out", "Output", -24.0f, 12.0f, 0.0f, -1, "dB");
    bo ("tail", "Tail Preview", false);
    fl ("tailRT", "Tail Decay", 0.2f, 6.0f, 1.2f, 1.2f, "s");
    fl ("tailLvl", "Tail Level", -30.0f, 6.0f, -6.0f, -1, "dB");
    // added in 0.1.1 (default = the 12 dB/oct of 0.1.0)
    ch ("hcSlope", "High Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    // added in 0.2.0 (default = the 12 dB/oct of 0.1.x)
    ch ("lcSlope", "Low Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    // added in 0.4.0: keep the ER level when switching ROOM / ALGO / IR
    bo ("autoLvl", "Auto Level", true);
    // added in 0.5.0: treble lost per 100 ms of arrival time (every source but Convolution), and the HYBRID split
    fl ("darken", "Darkening", 0.0f, 6.0f, 0.0f, 1.5f, "oct");
    fl ("irSplit", "Hybrid Split", 3.0f, 300.0f, 35.0f, 40.0f, "ms");
    // added in 0.5.1: fill reflections after the split
    fl ("irFill", "Hybrid Fill", 0.0f, 100.0f, 55.0f, -1, "%");      // default 55 % by ear (11-14)
    // added in 0.6.0: ALGO takes its first ms from ROOM (0 = off)
    fl ("roomEarly", "ROOM Early", 0.0f, 80.0f, 0.0f, 20.0f, "ms");
    // ===== Reverb Anatomy Lab 0.1.0 =====
    // late reverb (FDN)
    bo ("tOn", "Tail On", true);
    bo ("erOn", "ER On", true);
    fl ("tDecay", "Decay", 0.1f, 30.0f, 2.0f, 2.0f, "s");
    fl ("tSize", "Tail Size", 10.0f, 300.0f, 80.0f, 80.0f, "ms");
    fl ("tPre", "Tail Delay", 0.0f, 300.0f, 12.0f, 30.0f, "ms");
    fl ("tDiff", "Tail Diffusion", 0.0f, 100.0f, 70.0f, -1, "%");
    fl ("tModD", "Mod Depth", 0.0f, 100.0f, 20.0f, -1, "%");
    fl ("tModR", "Mod Rate", 0.05f, 5.0f, 0.6f, 0.8f, "Hz");
    fl ("tLvl", "Tail Level", -36.0f, 12.0f, 0.0f, -1, "dB");
    bo ("tFreeze", "Freeze", false);
    // reverb time per frequency: EQ nodes or multiband
    ch ("rtMode", "RT Mode", { "EQ", "Multiband" }, 0);
    {
        const float f[5] = { 80, 250, 1000, 4000, 12000 }, m[5] = { 1.3f, 1.1f, 1.0f, 0.75f, 0.5f };
        const char* fid[5] = { "rtF1", "rtF2", "rtF3", "rtF4", "rtF5" }; const char* mid[5] = { "rtM1", "rtM2", "rtM3", "rtM4", "rtM5" };
        const char* fn[5] = { "RT Node 1 Freq", "RT Node 2 Freq", "RT Node 3 Freq", "RT Node 4 Freq", "RT Node 5 Freq" };
        const char* mn[5] = { "RT Node 1 Mult", "RT Node 2 Mult", "RT Node 3 Mult", "RT Node 4 Mult", "RT Node 5 Mult" };
        for (int i = 0; i < 5; ++i) { fl (fid[i], fn[i], 20.0f, 20000.0f, f[i], 1000.0f, "Hz"); fl (mid[i], mn[i], 0.1f, 10.0f, m[i], 1.0f, "x"); }
    }
    it ("mbN", "MB Bands", 2, 5, 3);
    {
        const float x[4] = { 250, 2500, 6000, 12000 }, m[5] = { 1.3f, 1.0f, 0.6f, 0.5f, 0.4f };
        const char* xid[4] = { "mbX1", "mbX2", "mbX3", "mbX4" }; const char* mid[5] = { "mbM1", "mbM2", "mbM3", "mbM4", "mbM5" };
        const char* xn[4] = { "MB Crossover 1", "MB Crossover 2", "MB Crossover 3", "MB Crossover 4" };
        const char* mn[5] = { "MB Band 1 Mult", "MB Band 2 Mult", "MB Band 3 Mult", "MB Band 4 Mult", "MB Band 5 Mult" };
        for (int i = 0; i < 4; ++i) fl (xid[i], xn[i], 40.0f, 16000.0f, x[i], 1000.0f, "Hz");
        for (int i = 0; i < 5; ++i) fl (mid[i], mn[i], 0.1f, 10.0f, m[i], 1.0f, "x");
    }
    // input / output
    ch ("inMode", "Input", { "Stereo", "Mono (L+R)", "Left only", "Right only" }, 0);
    fl ("inW", "Input Width", 0.0f, 200.0f, 100.0f, -1, "%");
    ch ("outMode", "Output", { "Stereo", "Mono" }, 0);
    fl ("outW", "Output Width", 0.0f, 200.0f, 100.0f, -1, "%");
    // input filter
    fl ("iLC", "Input Low Cut", 20.0f, 2000.0f, 20.0f, 200.0f, "Hz");
    fl ("iHC", "Input High Cut", 1000.0f, 20000.0f, 20000.0f, 6000.0f, "Hz");
    ch ("iLCs", "Input Low Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    ch ("iHCs", "Input High Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    fl ("iTilt", "Input Tilt", -6.0f, 6.0f, 0.0f, -1, "dB");
    ch ("iTo", "Input Filter On", { "ER + Tail", "Tail only", "ER only" }, 0);
    // master EQ (reverb output)
    bo ("eqOn", "EQ On", true);
    fl ("eqHP", "EQ High Pass", 20.0f, 1000.0f, 20.0f, 150.0f, "Hz");
    fl ("eqLP", "EQ Low Pass", 1000.0f, 20000.0f, 20000.0f, 6000.0f, "Hz");
    {
        const float f[6] = { 100, 300, 800, 2000, 5000, 10000 }, q[6] = { 0.7f, 1, 1, 1, 1, 0.7f };
        for (int i = 0; i < 6; ++i)
        {
            const String n (i + 1);
            fl (("eqF" + n).toRawUTF8(), ("EQ " + n + " Freq").toRawUTF8(), 20.0f, 20000.0f, f[i], 1000.0f, "Hz");
            fl (("eqG" + n).toRawUTF8(), ("EQ " + n + " Gain").toRawUTF8(), -18.0f, 18.0f, 0.0f, -1, "dB");
            fl (("eqQ" + n).toRawUTF8(), ("EQ " + n + " Q").toRawUTF8(), 0.3f, 10.0f, q[i], 1.0f, "");
        }
    }
    // 0.2.0: Q of the reverb-time nodes (1 / 5 = shelves, 2-4 = bells), EQ bands on / off
    for (int i = 1; i <= 5; ++i) fl (("rtQ" + String (i)).toRawUTF8(), ("RT Node " + String (i) + " Q").toRawUTF8(), 0.3f, 4.0f, 0.7f, 1.0f, "");
    for (int i = 1; i <= 6; ++i) bo (("eqB" + String (i)).toRawUTF8(), ("EQ " + String (i) + " On").toRawUTF8(), true);
    // 0.2.0: the tail from an IR
    ch ("tSrc", "Tail Source", { "ALGO", "IR", "IR + ALGO" }, 0);
    fl ("irtX", "IR Tail Start", 5.0f, 300.0f, 80.0f, 60.0f, "ms");
    fl ("irtLen", "IR Tail Length", 0.2f, 6.0f, 6.0f, 2.0f, "s");
    fl ("irtSt", "IR Stretch", 50.0f, 200.0f, 100.0f, 100.0f, "%");
    fl ("irtLvl", "IR Tail Level", -24.0f, 12.0f, 0.0f, -1, "dB");
    fl ("irtDL", "IR Decay Low", 0.25f, 4.0f, 1.0f, 1.0f, "x");
    fl ("irtDM", "IR Decay Mid", 0.25f, 4.0f, 1.0f, 1.0f, "x");
    fl ("irtDH", "IR Decay High", 0.25f, 4.0f, 1.0f, 1.0f, "x");
    bo ("irtRev", "IR Reverse", false);
    fl ("irtB", "IR -> ALGO Time", 20.0f, 2000.0f, 250.0f, 300.0f, "ms");
    // 0.3.0: the ER / tail balance on OUTPUT (0 = both as set; -100 = ER only, +100 = tail only)
    fl ("erBal", "ER / Tail Balance", -100.0f, 100.0f, 0.0f, -1, "");
    // 0.4.0: the tail's engine
    ch ("tEng", "Tail Engine", { "Room / Hall (FDN)", "Plate", "Spring", "Ambient" }, 0);     // (0.4.2: Spring, 0.4.3: Ambient appended)
    // 0.4.1: the ALGO tail follows the ER's room (its reverb time per band from the room's size and surfaces)
    bo ("tRoom", "Tail From Room", false);
    // 0.4.3: the ambient engine's own controls
    fl ("aGrav", "Gravity", -100.0f, 100.0f, 0.0f, -1, "%");
    fl ("aFb", "Feedback", 0.0f, 90.0f, 0.0f, -1, "%");
    // 0.4.4: the IR tail is matched to the ALGO tail's loudness (else: as loud as in the IR, against its direct sound)
    bo ("irtAuto", "IR Tail Match", true);
    // ===== 0.6.0 (stage 4) =====
    // ducking: the reverb steps back while the dry plays (keyed by the dry input)
    ch ("dkMode", "Ducking", { "Off", "Single", "Multiband", "Spectral" }, 0);
    fl ("dkDepth", "Duck Depth", 0.0f, 24.0f, 6.0f, -1, "dB");
    fl ("dkThr", "Duck Threshold", -60.0f, 0.0f, -30.0f, -1, "dB");
    fl ("dkAtk", "Duck Attack", 1.0f, 200.0f, 10.0f, 20.0f, "ms");
    fl ("dkRel", "Duck Release", 20.0f, 2000.0f, 250.0f, 250.0f, "ms");
    it ("dkBands", "Duck Bands", 2, 5, 3);
    ch ("dkTo", "Duck Applies To", { "ER + Tail", "Tail only" }, 0);
    // gate: the reverb is cut once the key falls below the threshold (and the hold has passed)
    bo ("gtOn", "Gate", false);
    fl ("gtThr", "Gate Threshold", -80.0f, 0.0f, -40.0f, -1, "dB");
    fl ("gtHold", "Gate Hold", 0.0f, 2000.0f, 250.0f, 300.0f, "ms");
    fl ("gtRel", "Gate Release", 5.0f, 1000.0f, 60.0f, 80.0f, "ms");
    ch ("gtKey", "Gate Key", { "Input", "Reverb" }, 0);
    // output chorus (on the reverb)
    fl ("chMix", "Chorus Mix", 0.0f, 100.0f, 0.0f, -1, "%");
    fl ("chRate", "Chorus Rate", 0.05f, 5.0f, 0.6f, 0.8f, "Hz");
    fl ("chDepth", "Chorus Depth", 0.0f, 10.0f, 2.5f, -1, "ms");
    // modulation sources
    const StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth random" };
    const StringArray syncs { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/4 D", "1/8 D" };
    fl ("l1Rate", "LFO 1 Rate", 0.01f, 20.0f, 0.5f, 1.0f, "Hz"); ch ("l1Shape", "LFO 1 Shape", shapes, 0); ch ("l1Sync", "LFO 1 Sync", syncs, 0);
    fl ("l2Rate", "LFO 2 Rate", 0.01f, 20.0f, 0.13f, 1.0f, "Hz"); ch ("l2Shape", "LFO 2 Shape", shapes, 5); ch ("l2Sync", "LFO 2 Sync", syncs, 0);
    fl ("enAtk", "Envelope Attack", 1.0f, 200.0f, 10.0f, 20.0f, "ms");
    fl ("enRel", "Envelope Release", 10.0f, 2000.0f, 200.0f, 250.0f, "ms");
    fl ("enSens", "Envelope Sensitivity", -24.0f, 24.0f, 0.0f, -1, "dB");
    // the matrix: 4 slots (source -> destination, amount)
    const StringArray srcs { "Off", "LFO 1", "LFO 2", "Envelope" };
    const StringArray dsts { "Mix", "Output Width", "Output Level", "Tail Level", "ER Level", "Decay", "Treble Decay", "Chorus Depth", "Duck Depth", "Gate Threshold" };
    for (int i = 1; i <= 4; ++i)
    {
        const String n (i);
        ch (("m" + n + "Src").toRawUTF8(), ("Mod " + n + " Source").toRawUTF8(), srcs, 0);
        ch (("m" + n + "Dst").toRawUTF8(), ("Mod " + n + " Destination").toRawUTF8(), dsts, i == 1 ? 0 : i == 2 ? 5 : i == 3 ? 1 : 3);
        fl (("m" + n + "Amt").toRawUTF8(), ("Mod " + n + " Amount").toRawUTF8(), -100.0f, 100.0f, 0.0f, -1, "%");
    }
    // ===== 0.6.1 =====
    fl ("tSpread", "Spread", 0.0f, 300.0f, 0.0f, 60.0f, "ms");
    fl ("tShape", "Shape", -100.0f, 100.0f, 0.0f, -1, "%");
    fl ("tBassW", "Bass Width", 0.0f, 100.0f, 100.0f, -1, "%");
    // ===== 0.7.2 =====
    fl ("tFill", "Onset Fill", 0.0f, 100.0f, 0.0f, -1, "%");
    // ===== 0.7.3 =====
    fl ("tSpreadHi", "Spread High", 20.0f, 200.0f, 100.0f, -1, "%");
    // added in 0.7.9: where COLOR (Bandwidth / Bits / Wander) acts: the ER, the tail, or both (Wander moves the ER's taps only)
    bo ("colEr", "Color on ER", true);
    bo ("colTail", "Color on Tail", true);
    // added in 0.8.0: the tail's own low cut / high cut (after the tail, before COLOR)
    fl ("tLC", "Tail Low Cut", 20.0f, 2000.0f, 20.0f, 200.0f, "Hz");
    fl ("tHC", "Tail High Cut", 1000.0f, 20000.0f, 20000.0f, 6000.0f, "Hz");
    ch ("tLCs", "Tail Low Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    // added in 0.9.0: a second room (B) and which room / IR the ER and the TAIL are made from
    fl ("rbW", "Room B Width", 1.5f, 40.0f, 18.0f, 8.0f, "m");
    fl ("rbD", "Room B Depth", 1.5f, 50.0f, 26.0f, 10.0f, "m");
    fl ("rbH", "Room B Height", 2.0f, 20.0f, 10.0f, 5.0f, "m");
    ch ("rbWall", "Room B Walls", mats, 2);
    ch ("rbFloor", "Room B Floor", mats, 3);
    ch ("erRoomSel", "ER Room", { "A", "B" }, 0);
    ch ("tRoomSel", "Tail Room", { "A", "B" }, 0);
    ch ("tIrSel", "Tail IR", { "IR A", "IR B" }, 1);
    ch ("tHCs", "Tail High Cut Slope", { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1);
    // added in 0.9.1: which IR the ER follows; with T-HEAD (Tail Source IR + ALGO) what plays after the hand-over
    ch ("erIrSel", "ER IR", { "IR A", "IR B" }, 0);
    ch ("tBody", "Tail Body", { "ALGO", "ROOM", "IR" }, 0);
    // added in 0.10.0: the room's shape (A / B) and how far its walls open out
    ch ("shape", "Room Shape", { "Shoebox", "Hall", "Skewed", "Dome", "Connected", "Corridor" }, 0);
    fl ("splay", "Room Splay", 0.0f, 100.0f, 30.0f, -1, "%");
    fl ("clutter", "Room Clutter", 0.0f, 100.0f, 0.0f, -1, "%");
    fl ("rbClutter", "Room B Clutter", 0.0f, 100.0f, 0.0f, -1, "%");
    fl ("backAbs", "Back Room Absorb", -100.0f, 100.0f, 0.0f, -1, "%");
    fl ("rbBackAbs", "Room B Back Absorb", -100.0f, 100.0f, 0.0f, -1, "%");
    ch ("rbShape", "Room B Shape", { "Shoebox", "Hall", "Skewed", "Dome", "Connected", "Corridor" }, 0);
    fl ("rbSplay", "Room B Splay", 0.0f, 100.0f, 30.0f, -1, "%");
    return L;
}
