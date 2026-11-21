#pragma once
#include "RevViews.h"
#include <set>
#include "Panel.h"
#include "Modes.h"
#include "ScalingEditor.h"

class MainView : public juce::Component, private juce::Timer
{
public:
    // EDIT: the reverb (kRevW wide) + the notes column on the right (kInfoW, can be hidden: the reverb part stays put)
    static constexpr int kRevW = 1000, kInfoW = 280, kEditH = 876, kEditClosedH = 650, kDeckH = 228, kColorW = 186;     // 1280 wide at 100 % with the notes
    // the parameter grid (compact): pitch, knob size, row pitch, group title
    // (0.7.9: smaller, so INPUT | ER | TAIL | OUTPUT fit side by side under the deck)
    static constexpr int kP = 53, kKW = 50, kKH = 34, kRP = 40, kGT = 20;
    static juce::Rectangle<int> cell (juce::Rectangle<int> g, int col, int row) { return { g.getX() + 8 + col * kP, g.getY() + kGT + row * kRP, kKW, kKH }; }
    static juce::Rectangle<int> segAt (juce::Rectangle<int> g, int col, int row, int span) { return { g.getX() + 8 + col * kP, g.getY() + kGT + row * kRP + 14, span * kP - 4, 20 }; }
    static constexpr int designW = kRevW + kInfoW, designH = kEditH;
    explicit MainView (ERProcessor& p);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    ZoomButton zoom;
    // display modes: 0 full lab, 1 simple, 2 remote (fader), 3 remote (modern); each has its own design size
    juce::Point<int> designSize() const;
    std::function<void()> onDesignChange;
    void initMode();

private:
    int viewMode() const { return 0; }          // (the EDIT screen only: BASIC and the remotes are gone)
    bool helpOn() const { return (bool) proc.uiState.getProperty ("simpleHelp", true); }
    void setViewMode (int m); void layoutMode(); void enforceMode(); void showViewMenu();
    std::vector<juce::Component*> fullVisible; std::set<juce::Component*> modeAllowed; bool modeInit = false;
    void timerCallback() override;
    void layoutSource();
    void updateHelp();
    void applySkin (int i); void applyLookAdjust(); void restoreLastView(); void saveLastView(); int saveTick = 0; juce::TextButton lookBtn;
    void chooseIR (bool folder, int slot = 0);
    void loadIRFolder (const juce::File& d, int slot = 0);
    void addIRFolder (const juce::File& d);
    void showLoadMenu (int slot = 0);
    // (0.10.10) IR A and IR B: the same row (arrows, load, clear), compare per card: which IR, and ER / TAIL / whole
    TriButton irBPrev, irBNext; juce::TextButton irAClear, cmpB; UiSeg cmpScope;
    int cmpSlot() const { return (int) proc.uiState.getProperty ("cmpSlot", 0) > 0 ? 1 : 0; }
    int cmpScopeV() const { return juce::jlimit (0, 2, (int) proc.uiState.getProperty ("cmpScope", 0)); }
    void toggleCompare (int slot); rv::Measure irM[2]; juce::String irMKey[2]; const rv::Measure& irMeasOf (int slot);
    juce::String compareText (int slot); void nowInfo (int part, juce::String& what, juce::String& how) const;
    void fillSourcePresets();
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String keyPid; int srcPresetFor = -2; float lastComp = 0;
    std::vector<float> fD, fE, specD, specE;
    void updateSpectra(); int specTick = 0, specQuiet = 0; double specSr = 0; std::vector<int> specB0, specB1; std::vector<float> specBuf;
    
    bool cmpOn() const { return (bool) proc.uiState.getProperty ("cmpOn", true); }
    void showExportMenu();
    void exportTo();
    void flash (const juce::String& title, const juce::String& body);
    juce::String msgT, msgB; double msgUntil = 0;
    DragValue& dv (const char* id, const char* caption, std::function<juce::String (float)> fmt);

    ERProcessor& proc; ParamHelp ph;
    theme::LookAndFeel lnf;
    juce::ComboBox presetBox, srcPresetBox, colorPresetBox; juce::TextButton cmpBtn; TriButton irPrev, irNext, prevP, nextP; juce::TextButton loadBtn, exportBtn, langBtn, undoBtn, redoBtn;
    Seg srcSeg, irMode, irAuto, irWet, tailSeg, autoSeg;
    Seg erRoomSeg, tRoomSegAB, erIrSeg, tIrSegAB;
    UiAB srcRoomAB, srcIrAB; bool cardOn[4] { true, false, true, false }; juce::Rectangle<int> gNow;
    int srcShow (int kind) const; juce::String roomSummary (const er::RoomP& T, const juce::String& tag, bool dual) const;                       // (0.10.5) which room the EDIT panel's ER / TAIL column edits
    void layoutEditRoom();
    ParamCombo wallBox, floorBox, slopeBox, lcSlopeBox;
    juce::TextButton panelBtn, r2pBtn, r2pHybBtn; int lastR2p = 0;
    void startEar(); void endEar();
    // reverb pages
    PageTabs tabs; DetailSwitch detSwitch; ParamToggle colErBtn, colTailBtn; juce::Rectangle<int> gEr, gTl, sideR, gCO, gCol, colorBox; RTGraph rtGraph; CurveGraph inGraph, eqGraph, erfGraph, tfGraph; ComboGraph combo; MonitorTools tools; void flatRT(); void flatEQ(); MeasurePanel measure; LayerBar layerBar; juce::TextButton swapBtn, eqFlatBtn;
    juce::Rectangle<int> mainR, subR, specBox; bool specMain() const; NodeGraph* activeGraph(); void layoutMonitors(); void layoutRight(); static int layerFor (int page);
    struct RightClickClose : juce::MouseListener { std::function<void()> fn; void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && fn) fn(); } } popCloser; NodePopup popup; IrTailView irtView;
    TailSourceSeg tSrcSeg; Seg irtRevSeg, irtAutoSeg; juce::TextButton fdnBtn, hybBtn, roomAlgoBtn; juce::Rectangle<int> gTView; void layoutTail(); int lastTailMode = -1; juce::String lastStatus; int lastIrtV = -1, popTick = 0;
    std::vector<juce::String> popIds; void updatePopup(); std::vector<float> fI, fR, specI, specR;
    Seg tEngSeg, rtModeSeg, freezeSeg, erOnSeg, tOnSeg, outModeSeg, iToSeg, eqOnSeg;
    ParamCombo inModeBox, iLCsBox, iHCsBox, tLCsBox, tHCsBox;
    // 0.6.0: DYN (ducking, gate) and MOD (LFOs, envelope, chorus, matrix)
    Seg dkModeSeg, dkToSeg, gtOnSeg, gtKeySeg; std::vector<std::unique_ptr<ParamCombo>> modBoxes; DynMeter dynMeter; ModScope modScope;
    juce::Rectangle<int> gDuck, gGate, gDynV, gLfo, gMat, gScope; void layoutDynMod(); ParamCombo& mbox (const char* id);
    juce::Rectangle<int> gTail, gRT, gIO, gInF, gEQ, gMeas;
    std::vector<juce::Component*> pageComps[7], erComps; int lastMeasV = -1;
    int page() const; void applyPage(); void layoutReverb(); void paintReverbPages (juce::Graphics& g, int page);
    void showPresetMenuExtras (int id);
    void fillPresetBox(); void syncPresetBox();
    Echogram echo; RoomView roomView; InfoPanel info; LessonCard lesson; HelpCard help; EarCard ear; EarMask earMask;
    HardwarePanel panel; FilterGraph fgraph; NowCard now;
    std::map<juce::String, std::unique_ptr<DragValue>> vals;
    std::vector<juce::Component*> roomSet, patSet, irSet;
    juce::Rectangle<int> gSrc, gVoice, gColor, gOut, gFilter;
    Preview prev, ghost; std::vector<float> snap, settledSnap; juce::String settledPnl, lastPnl; double lastChange = 0; bool showPanel() const;
    void describeChange(); void describeNow(); void updateRings(); int lastTargetVer = -1, settle = 0; bool dirty = true;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastLang;
    juce::TextButton secBtn, tIrBtn, tIrClearBtn; void showSectionMenu (int forPage = -1, juce::Rectangle<int> at = {}); juce::String pageSection() const; void loadTailIrFile();
    juce::TextButton viewBtn, helpBtn, editBtn, basicBtn, remoteBtn; void showRemoteMenu(); void chooseBatch (bool folder); void startBatchFor (const juce::Array<juce::File>& files); bool batchWatch = false, batchApply = false; juce::String lastTabStatus; void syncViewButtons(); void placeViewButtons (int x, int y, int h); SimpleDeck deck, colorDeck; FaderRemote fader; ModernRemote modern;
    // monitors: 1-4 panes, each showing one view (0 spectrum + edit, 1 echogram, 2 measurement, 3 room / info); uiState monLay, monV0..3 (-1 auto)
    juce::TextButton infoBtn, paneBtn[4], deckSel[4]; HoldButton infoTab[3];
    juce::TextButton detEditBtn, detDynBtn, detModBtn, lockBtn;
    // SOURCE screen (0.9.1): four source cards (ROOM A | ROOM B | IR A | IR B); each one sends to the ER and / or the TAIL, converts itself to ALGO,
    // and an IR can also be the TAIL's head (T-HEAD: its first part; what follows comes from another source)
    std::map<juce::String, std::unique_ptr<DragValue>> srcVals;
    ParamCombo wallA2, floorA2, wallB2, floorB2, shapeA2, shapeB2;
    juce::TextButton sendEr[4], sendTail[4], sendHead[2]; HoldButton toAlgo[4];
    juce::TextButton detSrcBtn, copyABBtn, copyBABtn, ir2Load, ir2Clear, dmPreBtn;
    int deckView() const;             // (0.10.9) what the deck area shows: 0 controls, 1 SOURCE, 2 DYN, 3 MOD
    void setDeckView (int v); juce::Rectangle<int> gDynInfo;
    std::vector<juce::Component*> srcComps; juce::Rectangle<int> gCard[4];
    std::vector<float> irEnv[2]; juce::String irEnvKey[2]; double irEnvT0[2] { 0, 0 }, irEnvLen[2] { 0, 0 };
    void layoutSourceScreen(); void syncSourceButtons(); void showLockMenuAll(); void showLessonList();
    // (0.9.2) working on a card (a click or a drag in it) makes the right-hand monitor show it: the room plan of A / B, or the IR analysis
    int srcFocus = -1; struct FocusL : juce::MouseListener { MainView* o = nullptr; void mouseDown (const juce::MouseEvent& e) override; } focusL;
    int cardAt (juce::Point<int> p) const;
    void sendTo (int card, int what);                    // what: 0 ER, 1 TAIL, 2 T-HEAD
    void convertCard (int card, int which);              // which: 0 the lit destinations (both when none), 1 ER, 2 TAIL, 3 both
    bool erLit (int card) const; bool tailLit (int card) const; bool headLit (int slot) const; bool cardLoaded (int card) const;
    const std::vector<float>& envOf (int slot);
    juce::Rectangle<int> paneR[4], monArea, deckR, colorR, gIn; int paneView[4] { 0, 1, 2, 3 }, nPanes = 1;
    // the splits between the panes (drag to resize): which ratio, its axis, the span it moves in
    struct Split { juce::Rectangle<int> hit; int which; bool vertical; juce::Rectangle<int> span; };
    std::vector<Split> splits; int dragSplit = -1;
    void mouseDrag (const juce::MouseEvent&) override; void mouseUp (const juce::MouseEvent&) override; void mouseMove (const juce::MouseEvent&) override;
    int monLayout() const; double roomPaneWidth (int paneH) const; bool shows (int id) const; void applyDetailVisibility(); int deckMode() const; bool infoOn() const; int infoSel() const;
    void layoutPanes(); void showPaneMenu (int pane); void layoutInfo(); void layoutDeck(); void setInfoSel (int i);
    static const char* viewName (int v, bool ja);
};

using EditorShell = ScalingEditor<MainView, ERProcessor>;
