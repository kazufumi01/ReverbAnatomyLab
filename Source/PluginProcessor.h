#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "ERDsp.h"
#include "TailDsp.h"
#include "DynMod.h"
#include "IrFit.h"
#include <unordered_map>
#include "Params.h"
#include "Presets.h"
#include <atomic>
#include <thread>
#include <mutex>

class ERProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ERProcessor();
    ~ERProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "ui" };

    // (0.7.0) a fitting thread renders with its own values: tlOverride is set on that thread only
    static thread_local const std::unordered_map<std::string, float>* tlOverride;
    static thread_local double tlMaxLenS;
    // (0.7.1) the wet bus: -12 dB, so a typical hall at 100 % wet sits about 5 dB under the input (as common reverb plug-ins);
    // the presets are made for about +7 dB of wet IR energy before it (kRefWetDb)
    static constexpr float kWetCal = 0.2512f; static constexpr double kRefWetDb = 7.0;                  // (a fit thread: renders no longer than the target)
    float value (const char* id) const
    {
        if (auto* o = tlOverride) { auto it = o->find (id); if (it != o->end()) return it->second; }
        return apvts.getRawParameterValue (id)->load();
    }
    void setParam (const char* id, float v);
    // parameter lock (0.8.0)
    juce::StringArray lockedIds() const; bool isLocked (const juce::String& id) const; void setLocked (const juce::String& id, bool on);
    std::atomic<bool> snapReq { false }; int snapState = 0, lastTailKey = -1; float snapG = 1.0f; bool snapOn = false, snapEnabled = true; std::vector<float> snapBuf;     // (0.8.1) no morph on loads
    struct KeepLocked { explicit KeepLocked (ERProcessor&); ~KeepLocked(); ERProcessor& p; std::vector<std::pair<juce::String, float>> v; };
    void applyPreset (int index);
    void applyPresetPart (int index, const juce::String& section);    // only that part of a built-in preset (the rest stays)
    void requestTransition (bool isPreset, bool longHold = false) { if (isPreset) transPreset = true; if (longHold) transLong = true; ++transReq; }
    std::atomic<float> autoComp { 0 };               // dB added to the ER by Auto Level (read by the editor, saved in uiState)
    void applySourcePreset (int index);             // only the SOURCE parameters of preset[index]
    void applyColorPreset (int index);
    static const juce::StringArray& voiceColorIds();
    juce::StringArray nonNeutralVoiceColor() const;  // VOICE / COLOR values that differ from the defaults ("Predelay 8.0 ms")
    void resetVoiceColor();
    float neutralValue (const juce::String& id) const;
    int lastSrcSeen = -1;
    // ---------- IR list (several files or a folder, stepped with the arrows) ----------
    juce::Array<juce::File> irList; int irListIndex = -1;
    juce::Array<juce::File> irListB; int irListIndexB = -1;            // (0.10.10) IR B has its own list, as IR A
    void setIRList (const juce::Array<juce::File>& files, int startIndex, int slot = 0);
    juce::String stepIR (int delta, int slot = 0);  // load the previous / next file of the list
    juce::String loadIRSlot (const juce::File& f, int slot) { return slot == 1 ? loadTailIR (f) : loadIR (f); }
    static juce::Array<juce::File> irFilesIn (const juce::File& folder);
    // ---------- analyser FIFOs (direct = input, ER = wet before the mix) ----------
    static constexpr int fifoSize = 16384;
    std::array<float, fifoSize> fifoD {}, fifoE {}, fifoI {}, fifoR {}; std::atomic<int> fifoW { 0 }; int fifoW0 = 0;
    void readFifo2 (std::vector<float>& in, std::vector<float>& wet, int n) const;      // reverb input (before the input filter) and wet output
    void readFifo (std::vector<float>& d, std::vector<float>& e, int n) const;
    // ---------- hardware faceplate (classic units in ALGO) ----------
    int unitIndex() const { return (int) uiState.getProperty ("unit", -1); }
    int unitProg() const { return (int) uiState.getProperty ("prog", 0); }
    std::vector<float> panelValues() const;
    void setPanelValue (int ctl, float v);          // stores the value and writes the mapped parameters
    void applyPanel();
    ParamList panelOutputs() const;
    static void linkTail (ParamList& o);
    // ER / tail balance: unity at 0, the other side fades out linearly to the end
    float erBalG() const { const float b = value ("erBal"); return b <= 0 ? 1.0f : (100.0f - b) / 100.0f; }
    float tailBalG() const { const float b = value ("erBal"); return b >= 0 ? 1.0f : (100.0f + b) / 100.0f; }         // the faceplate's tail-related outputs -> the TAIL parameters
    std::vector<float> panelDefaults() const;       // the current program's factory values
    void applyLesson (int index);
    int currentPreset = -1;

    // ---------- IR target (message thread) ----------
    juce::String readIRFileAuto (const juce::File& f, std::vector<float>& L, std::vector<float>& R, juce::String* shownName = nullptr);
    juce::String loadIR (const juce::File& f);          // empty = ok, else the error
    juce::File lastIRFile;                              // for user presets (the IR is referred to by its file)
    // the tail's own IR (else the tail uses the ER's IR)
    juce::String loadTailIR (const juce::File& f);
    void clearTailIR();
    bool tailIrOwn() const { return tIrRawL.size() >= 256; }
    bool tailUsesOwnIR() const { return tailIrOwn() && (int) value ("tIrSel") == 1; }      // IR B loaded and chosen (else IR A)
    // (0.9.1) IR A = the ER's IR slot (irRaw), IR B = tIrRaw. The ER follows erIrSel, the TAIL tIrSel; a slot that is not loaded falls back to A
    int erIrSlot() const { return (int) value ("erIrSel") == 1 && tailIrOwn() ? 1 : 0; }
    int tailIrSlot() const { return tailUsesOwnIR() ? 1 : 0; }
    int tailSrcEff() const { return (int) value ("tSrc") == 2 && (int) value ("tBody") == 2 && tailIrOwn() && ! irRawL.empty() ? 1 : (int) value ("tSrc"); }   // T-HEAD with an IR body = one spliced IR
    bool irALoaded() const { return ! irRawL.empty(); }
    const std::vector<float>& rawOf (int slot, bool right) const { return slot == 1 ? (right ? tIrRawR : tIrRawL) : (right ? irRawR : irRawL); }
    bool erHasRaw() const { return erIrSlot() == 1 ? tailIrOwn() : ! irRawL.empty(); }
    // the sources' "-> ALGO" (the lit destinations decide what is written; the fits that need an analysis wait for it)
    // the source cards: 0 ROOM A, 1 ROOM B, 2 IR A, 3 IR B. Which destinations a card feeds, and a click on its [ER] / [TAIL] / [T-HEAD] (what 0 / 1 / 2)
    bool cardLoaded (int card) const { return card < 2 ? true : card == 2 ? irALoaded() : tailIrOwn(); }
    bool erLit (int card) const;
    bool tailLit (int card) const;
    bool headLit (int slot) const { return (int) value ("tSrc") == 2 && tailIrSlot() == slot; }
    void routeSend (int card, int what);
    void convertRoom (int slot, bool toEr, bool toTail);
    void convertIr (int slot, bool toEr, bool toTail);
    void servicePending();
    int lastErKey = -1, tgtSlot = 0; std::atomic<int> analysisSlot { 0 }; bool pendErFit = false, pendTailFit = false; int pendTgtSeen = 0, pendIrtSeen = 0, pendTries = 0;
    std::vector<float> tIrRawL, tIrRawR; std::atomic<int> tIrVersion { 0 };   // (message thread)
    juce::String tIrName; juce::File lastTailIRFile;
    static juce::String readIRFile (const juce::File& f, std::vector<float>& L, std::vector<float>& R);

    // ---------- user presets (message thread) ----------
    static juce::File userPresetDir(); static juce::File presetRoot(); static juce::File writeFactoryPresets();
    juce::Array<juce::File> userPresets() const;
    // (0.11.0) additional libraries: Libraries/<library>/[<folder>/]<name>.rvpreset. Kept apart from the user presets; read-only in the plug-in
    // (the plug-in never writes or deletes there). A library is a folder, or a .rvlib (= a zip of that folder) added through the menu.
    struct LibItem { juce::String lib, sub, name; juce::File file; };
    static juce::File libraryDir();
    std::vector<LibItem> userPresetItems() const;        // the user presets with their folder (LibItem::sub)
    std::vector<LibItem> libraryPresets() const;
    static juce::String importLibrary (const juce::File& rvlibOrFolder, int* added = nullptr);       // "" = ok, else the reason
    juce::File saveUserPreset (const juce::String& name);      // returns the file (does not exist = failed)
    // section presets: only ER / TAIL / IO / EQ parameters (User Presets/<section>/*.rvsec)
    static juce::String sectionOf (const juce::String& id);
    static juce::File sectionDir (const juce::String& sec) { return userPresetDir().getChildFile (sec); }
    juce::Array<juce::File> sectionPresets (const juce::String& sec) const;
    juce::File saveSectionPreset (const juce::String& sec, const juce::String& name);
    bool loadSectionPreset (const juce::File& f);
    bool loadUserPreset (const juce::File& f);
    juce::String currentUserPreset;
    juce::File currentPresetFile;                      // (0.11.0) the file of the loaded user / library preset (for the ◀▶ order)

    // ---------- ear training (message thread): the settings are kept and put back afterwards ----------
    void earBegin();
    void earEnd();
    bool earActive() const { return earSnap.isValid(); }
    juce::ValueTree earSnap;
    void clearIR();
    std::shared_ptr<const er::Target> target() const { std::lock_guard<std::mutex> l (tMutex); return tgt; }
    juce::String irName;
    std::atomic<int> analysing { 0 }, fitting { 0 };
    void startFit();                                    // IR -> ALGO (background), writes the parameters when done
    // ROOM -> ALGO: fit the ALGO parameters to the current room (background), then switch to ALGO
    // keepEarly: the first ms stay ROOM's own reflections (roomEarly)
    void roomToPattern (bool keepEarly = false);
    // IR -> ROOM: the shoebox whose ER looks most like the IR's (background), then switch to ROOM
    void irToRoom();
    std::atomic<int> converting { 0 }, r2pDone { 0 }, r2pScore { -1 }, r2pKind { 0 };   // kind 0 ROOM->ALGO, 1 hybrid, 2 IR->ROOM
    std::atomic<int> targetVersion { 0 };

    // ---------- IR export (message thread) ----------
    struct ExportOpts { double sr = 48000; bool direct = false, tail = true, normalize = false; int bits = 24; };
    void renderIR (const ExportOpts& o, juce::AudioBuffer<float>& out, double& gainDb);
    juce::String exportIR (const juce::File& f, const ExportOpts& o, bool& ok);      // returns a short report

    // ---------- preview for the editor (message thread) ----------
    struct Preview { std::vector<float> L, R; std::vector<float> fullL, fullR; er::TapSet taps; bool conv = false; int src = 0; er::Feat feat; bool hasFeat = false; er::Dist d; double lengthMs = 0;
                     er::Feat self; double selfWc = 80; int serial = 0; };
    void renderPreview (Preview& out, double lengthMs);
    bool previewTailStale = false;                            // the last preview reused an older tail (render again soon)
    std::vector<float> pvTailL, pvTailR; double pvTailHash = -1, pvTailAt = 0;       // ER (L / R, for the match) + the whole reverb (fullL / fullR)
    void renderPreviewER (Preview& out, double lengthMs) const;
    void erToFull (double fs, std::vector<float>& L, std::vector<float>& R) const;   // ER on/off and the input filter, offline
    er::RoomP roomParams (int slot = -1) const;             // -1 = the ER's room (A / B)
    er::RoomP tailRoomP() const { return roomParams ((int) value ("tRoomSel")); }
    void copyRoomAtoB() { copyRoom (true); }
    void copyRoom (bool aToB);
    er::PatternP patternParams() const;

    void poll() { timerCallback(); }               // tests
    // live values for the editor (LFOs, envelope, matrix sums, ducking GR per band, gate)
    std::atomic<float> mLfo1 { 0 }, mLfo2 { 0 }, mEnv { 0 }, mGate { 1 }; std::array<std::atomic<float>, dm::kDst> mSum {}; std::array<std::atomic<float>, dm::Ducker::kSpec> mDuck {}; std::atomic<int> mDuckN { 0 };
    // IR -> algorithm presets in one go: each IR is analysed, its ER rebuilt as ALGO and its tail as ALGO, and the result
    // saved as a user preset (no IR needed to load it). The settings from before the batch come back at the end.
    // applyOnly: one IR, the result stays on (no preset saved, nothing restored) - "rebuild this IR precisely"
    void startBatch (const juce::Array<juce::File>& files, const juce::String& suffix = " (Fit)", bool applyOnly = false);
    std::atomic<float> batchFitProgress { 0 };            // 0..1 within the IR being fitted
    juce::String lastFitReport;
    void cancelBatch() { batchCancel = true; }
    std::atomic<int> batchTotal { 0 }, batchDone { 0 }, batchOk { 0 }; std::atomic<bool> batchRunning { false };
    juce::String batchLog;                                  // (message thread) one line per IR

    // ---------- reverb (tail, input / output, master EQ) ----------
    rv::TailSettings tailSettings (double fs) const;
    rv::RTCurve rtCurve() const;                       // the reverb time asked for, per band
    double rtAt (double f) const;                      // the same, at any frequency (for the graph)
    bool tailFromRoom() const;                         // ROOM: the ALGO tail follows the ER's room
    bool tailEdt() const;                              // (1.1) Hall engine with an initial decay below 100 %: two slopes
    double edrAt (double f) const;                     // the initial-decay ratio (EDT / T30) at f
    bool tailDual() const;                             // (0.10.3) connected rooms: the tail has two slopes
    double roomRTAt (double f) const;                  // the room's own reverb time (Eyring + air)
    double decayBase() const;
    double hybridFdnGain() const;                      // IR + ALGO: the FDN starts B ms down a decay
    juce::String roomToAlgo();
    juce::String irToTailRoom();                       // IR -> TAIL ROOM (the room whose RT per band fits the IR tail's T30)
    float hybG = 1.0f;                                 // (the FDN's smoothed hybrid gain)
    void setInputFilter (rv::InputFilter& f, double fs) const;
    void setMasterEQ (rv::MasterEQ& e, double fs) const;
    // offline (message / worker thread): the tail's impulse response and the output stage
    void renderTail (double fs, int N, std::vector<float>& L, std::vector<float>& R, rv::Fdn& f) const;
    void finishWet (double fs, std::vector<float>& L, std::vector<float>& R) const;
    // measurement of the whole reverb (background, starts by itself when the settings settle)
    void startMeasure();
    rv::Measure measurement() const { std::lock_guard<std::mutex> l (measMutex); return meas; }
    std::atomic<int> measVersion { 0 }; std::atomic<bool> measuring { false };
    // ---------- the IR tail (TAIL source IR / IR + ALGO) ----------
    std::atomic<bool> tailIrReady { false }; std::atomic<int> irtVersion { 0 };
    void buildTailIR();                                  // background; also starts by itself when the IR or its settings change
    juce::String irToFdn();                              // sets ALGO from the IR's decay per band; returns a short report
    void tailIR (std::vector<float>& L, std::vector<float>& R, rv::Measure& m) const { std::lock_guard<std::mutex> l (irtMutex); L = irtL; R = irtR; m = irtMeas; }
    mutable std::mutex irtMutex; std::vector<float> irtL, irtR; rv::Measure irtMeas;     // processed tail IR (48 kHz, starts at IR Tail Start) and the IR's own decay
    void measuredIR (std::vector<float>& L, std::vector<float>& R) const { std::lock_guard<std::mutex> l (measMutex); L = measL; R = measR; }
    std::vector<float> measL, measR;
    // meters
    std::atomic<float> inPeak { 0 }, erPeak { 0 }, outPeak { 0 };

private:
    void timerCallback() override;
    void stepBatch();
    // 0.6.0: ducking, gate, chorus, modulation
    dm::Ducker ducker; dm::Gate gate; dm::Chorus chorus; dm::Lfo lfo1, lfo2; dm::Env modEnv;
    juce::AudioBuffer<float> keyBuf, dynBuf; float modV[dm::kDst] {}; double decayMod = 1, trebleMod = 1, decayModApplied = 1, trebleModApplied = 1;
    void runModSources (int n);
    juce::Array<juce::File> batchFiles; juce::String batchSuffix; int batchState = 0, batchWait = 0, batchIrtSeen = 0, batchTgtSeen = 0; bool batchApplyOnly = false;
    class FitJob; std::unique_ptr<FitJob> fitJob; void stopFitJob();
    std::atomic<bool> batchCancel { false }; std::unique_ptr<juce::XmlElement> batchSaved; juce::File batchIrBefore; juce::String batchNameBefore;
    void rebuildTaps (bool instant);
    void buildRaw (er::TapSet& raw, int src, const er::Target* T, std::vector<er::Img>& sc) const;
    void sourceTaps (er::TapSet& shapedOut, const er::Target* T) const;       // message thread: raw taps of the current source, shaped by VOICE
    er::Settings settings() const;
    void reanalyse();
    void installTarget (std::shared_ptr<er::Target> t);

    double sr = 48000; int maxBlock = 512;
    er::TapEngine engine; er::Post post, tailFlt; er::ColorStage tailCol;
    er::Settings tailFilterSettings() const; er::TailFDN tail;
    void colorTail (double fs, std::vector<float>& L, std::vector<float>& R) const;
    rv::Fdn fdn; rv::InputFilter inF; rv::MasterEQ meq; rv::Fdn previewFdn; bool previewFdnReady = false;
    std::vector<float> tdBuf[2]; int tdW = 0; double tdCur = 0; std::array<float, 128> revSnap {}; std::array<float, 64> filtCur {}, filtTgt {}; int filtSlopes[2] {}; bool filtInit = false;
    void glideFilters (int n, bool inFilter, bool force); float erOnG = 1, tailG = 1, outW = 1;
    mutable std::mutex measMutex; rv::Measure meas; std::thread measWorker; double measHash = 0, measSeen = -1; int measStable = 0; std::atomic<bool> measCancel { false };
    juce::dsp::Convolution conv { juce::dsp::Convolution::Latency { 0 } };
    juce::dsp::Convolution tailConv { juce::dsp::Convolution::NonUniform { 1024 } };       // the IR tail (zero latency)
    juce::AudioBuffer<float> cIn; double tdCur2 = -1; int lastTsrc = 0; bool tailIdle = false, convWarm = false, convRunning = false; int convWarmBlocks = 0;
    std::thread irtWorker; double irtHash = 0, irtSeen = -1; int irtStable = 0, irtVerLaunch = 0;
    void loadTailConv();
    juce::AudioBuffer<float> wet, monoBuf, pdBuf, tailBuf, inBuf, fBuf, tIn, tOut;
    void updateReverb (bool force);
    std::vector<er::Img> scratch;
    er::TapSet raw, shaped;
    std::array<float, 64> lastSnap {}; int sinceRebuild = 1 << 20; bool pendingRebuild = true;
    std::atomic<int> irTapIdx { 0 }; er::IrSet irTaps[2]; std::atomic<int> irTapsVersion { 0 }; int seenIrVersion = -1;
    std::atomic<bool> convReady { false };
    float dryG = 1, wetG = 1;
    // preset / source switching: the wet path fades out, holds while the new settings land, then fades in
    enum class Trans { normal, out, hold, in };
    Trans trans = Trans::normal; int tCount = 0, tLen = 1, transSeen = 0; float tGain = 1, tFrom = 1, tPre = 1; bool holdDone = false; std::vector<float> gPre, gPost;
    std::atomic<int> transReq { 0 }; std::atomic<bool> transPreset { false }, transLong { false };
    er::Settings curS; bool curConv = false; float curPreMs = 0; int curSrc = -1, curIrMode = -1;
    // automatic ER level across ROOM / ALGO / IR
    float eCur = -100;                                   // ER energy of the current source (dB re direct)
    std::atomic<float> convE { -100 };
    float tapEnergyDb (const er::TapSet& t) const;
    void finishTransition();
    juce::SmoothedValue<float> outGain;

    mutable std::mutex tMutex; std::shared_ptr<const er::Target> tgt;
    std::vector<float> irRawL, irRawR;                 // 48 kHz original (for re-analysis and the saved state)
    std::array<float, 4> lastIrOpts { -1, -1, -1, -1 };
    std::thread worker; std::atomic<bool> cancel { false };
    std::thread r2pWorker; std::mutex r2pMutex; bool r2pReady = false; er::FitP r2pFit; er::RoomP r2pRoom; int r2pResScore = 0, r2pResKind = 0; float r2pEarly = 20;
    void joinWorker() { cancel = true; if (worker.joinable()) worker.join(); cancel = false; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ERProcessor)
};
