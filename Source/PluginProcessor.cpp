#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>

static const char* const kTapIds[] = { "src", "erRoomSel", "rbW", "rbD", "rbH", "rbWall", "rbFloor", "roomW", "roomD", "roomH", "wall", "floor", "order", "srcX", "srcY", "lisX", "lisY", "shape", "splay", "rbShape", "rbSplay", "backAbs", "rbBackAbs", "clutter", "rbClutter",
                                       "first", "length", "taps", "growth", "energy", "decay", "damp", "vari", "predelay", "size", "width", "irMode", "darken", "irSplit", "irFill", "roomEarly" };
static const char* const kIrOpts[] = { "irWin", "irAuto", "irThr", "irWet" };
static constexpr int kNumTapIds = (int) (sizeof (kTapIds) / sizeof (kTapIds[0]));

ERProcessor::ERProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", makeLayout())
{
    scratch.reserve (1400);
    lastSnap.fill (-1e9f);
    startTimerHz (10);
}

ERProcessor::~ERProcessor() { stopTimer(); stopFitJob(); joinWorker(); if (r2pWorker.joinable()) r2pWorker.join(); if (measWorker.joinable()) measWorker.join(); if (irtWorker.joinable()) irtWorker.join(); }

bool ERProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void ERProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = std::max (1, samplesPerBlock);
    engine.prepare (sr); post.prepare (sr); tail.prepare (sr); tailCol.prepare (sr); tailFlt.prepare (sr); tailFlt.set (tailFilterSettings()); tailFlt.reset();
    conv.prepare ({ sr, (juce::uint32) maxBlock, 2 });
    wet.setSize (2, maxBlock); gPre.assign ((size_t) maxBlock, 1.0f); gPost.assign ((size_t) maxBlock, 1.0f); monoBuf.setSize (1, maxBlock); pdBuf.setSize (2, maxBlock); tailBuf.setSize (2, maxBlock);
    inBuf.setSize (2, maxBlock); fBuf.setSize (2, maxBlock); tIn.setSize (2, maxBlock); tOut.setSize (2, maxBlock); snapBuf.assign ((size_t) maxBlock + 16, 1.0f);
    fdn.prepare (sr); inF.reset(); meq.reset();
    for (auto& b : tdBuf) b.assign ((size_t) (2.6 * sr) + 8, 0.0f);
    tdW = 0; tdCur = -1; tdCur2 = -1; filtInit = false;
    cIn.setSize (2, maxBlock);
    keyBuf.setSize (1, maxBlock); dynBuf.setSize (2, maxBlock);
    ducker.prepare (sr); gate.prepare (sr); chorus.prepare (sr); lfo1.reset(); lfo2.reset(); modEnv.v = 0;
    tailConv.prepare ({ sr, (juce::uint32) maxBlock, 2 });
    if (tailIrReady.load()) loadTailConv(); revSnap.fill (-1e9f);
    updateReverb (true);
    erOnG = value ("erOn") > 0.5f ? erBalG() : 0.0f; tailG = value ("tOn") > 0.5f ? (float) er::db2g (value ("tLvl")) * tailBalG() : 0.0f; outW = value ("outW") / 100.0f;
    outGain.reset (sr, 0.03); outGain.setCurrentAndTargetValue ((float) er::db2g (value ("out")));
    const float m = value ("mix") / 100.0f; dryG = std::min (1.0f, 2 * (1 - m)); wetG = std::min (1.0f, 2 * m);
    curS = settings(); post.set (curS); post.reset();
    curSrc = (int) value ("src"); curIrMode = (int) value ("irMode"); curConv = curSrc == 2 && curIrMode == 1; curPreMs = value ("predelay");
    trans = Trans::normal; tGain = 1; transSeen = transReq.load();
    lastSnap.fill (-1e9f);
    rebuildTaps (true);
    eCur = curConv ? convE.load() : tapEnergyDb (shaped);
    if (auto t = target(); t && t->valid)
    {
        juce::AudioBuffer<float> b (2, (int) t->erL.size());
        b.copyFrom (0, 0, t->erL.data(), (int) t->erL.size()); b.copyFrom (1, 0, t->erR.data(), (int) t->erR.size());
        conv.loadImpulseResponse (std::move (b), er::kSR, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        convReady = true;
    }
}

// the rooms: A (the original parameters) and B (0.9.0); slot -1 = the ER's room. Both share the positions and the order.
er::RoomP ERProcessor::roomParams (int slot) const
{
    if (slot < 0) slot = (int) value ("erRoomSel");
    er::RoomP R;
    if (slot == 1) { R.W = value ("rbW"); R.D = value ("rbD"); R.H = value ("rbH"); R.wall = (int) value ("rbWall"); R.floor = (int) value ("rbFloor"); R.shape = (int) value ("rbShape"); R.splay = value ("rbSplay") / 100.0; R.backAbs = value ("rbBackAbs") / 100.0; R.clutter = value ("rbClutter") / 100.0; }
    else { R.W = value ("roomW"); R.D = value ("roomD"); R.H = value ("roomH"); R.wall = (int) value ("wall"); R.floor = (int) value ("floor"); R.shape = (int) value ("shape"); R.splay = value ("splay") / 100.0; R.backAbs = value ("backAbs") / 100.0; R.clutter = value ("clutter") / 100.0; }
    R.order = (int) value ("order");
    R.sx = value ("srcX"); R.sy = value ("srcY"); R.lx = value ("lisX"); R.ly = value ("lisY");
    return R;
}
er::PatternP ERProcessor::patternParams() const
{
    er::PatternP p;
    p.first = value ("first"); p.length = value ("length"); p.taps = value ("taps"); p.growth = value ("growth");
    p.energy = value ("energy"); p.decay = value ("decay"); p.damp = value ("damp"); p.seed = (int) value ("vari");
    return p;
}
er::Settings ERProcessor::settings() const
{
    er::Settings s;
    s.diffusion = value ("diffusion") / 100.0; s.sizeScale = value ("size") / 100.0;
    s.lowCut = value ("lowcut"); s.highCut = value ("highcut"); s.hcSlope = (int) value ("hcSlope"); s.lcSlope = (int) value ("lcSlope"); s.levelDb = value ("level");
    s.bwKHz = value ("bw"); s.bits = value ("bits"); s.wander = value ("wander") / 100.0;
    if (value ("colEr") < 0.5f) { s.bwKHz = 20; s.bits = 24; s.wander = 0; }        // COLOR off for the ER
    s.width = value ("width") / 100.0;
    s.convWidth = (int) value ("src") == 2 && (int) value ("irMode") == 1;
    s.mix = value ("mix") / 100.0; s.outDb = value ("out");
    s.tail = value ("tail") > 0.5f; s.tailRT = value ("tailRT"); s.tailDb = value ("tailLvl");
    return s;
}

void ERProcessor::buildRaw (er::TapSet& out, int src, const er::Target* T, std::vector<er::Img>& sc) const
{
    if (src == 0) er::roomTaps (roomParams(), out, sc);
    else if (src == 1)
    {
        const double early = value ("roomEarly");
        if (early < 0.5) er::patternTaps (patternParams(), out);
        else { er::TapSet pt, rt; er::patternTaps (patternParams(), pt); er::roomTaps (roomParams(), rt, sc); er::splicePatternRoom (pt, rt, early, out); }
    }
    else
    {
        const auto& I = T != nullptr ? T->ir : irTaps[irTapIdx.load()];
        if ((int) value ("irMode") == 2) er::hybridTaps (I, value ("irSplit"), value ("irFill") / 100.0, out);
        else out = I.taps;
    }
}

void ERProcessor::sourceTaps (er::TapSet& sh, const er::Target* T) const
{
    const int src = (int) value ("src");
    er::TapSet r;
    if (src == 2 && (T == nullptr || ! T->valid)) r.n = 0;
    else { std::vector<er::Img> sc; sc.reserve (1400); buildRaw (r, src, T, sc); }
    er::shapeTaps (r, sh, value ("predelay"), value ("size") / 100.0, value ("width") / 100.0, value ("darken"));
}

void ERProcessor::rebuildTaps (bool instant)
{
    const int src = (int) value ("src");
    if (src == 2 && (int) value ("irMode") == 1) shaped.n = 0;
    else
    {
        buildRaw (raw, src, nullptr, scratch);
        er::shapeTaps (raw, shaped, value ("predelay"), value ("size") / 100.0, value ("width") / 100.0, value ("darken"));
    }
    if (engine.setTaps (shaped, instant)) { pendingRebuild = false; sinceRebuild = 0; }
}

// ---------------------------------------------------------------------------
//  Reverb: settings
// ---------------------------------------------------------------------------
bool ERProcessor::tailFromRoom() const { const int ts = (int) value ("tSrc"); return (ts == 0 && value ("tRoom") > 0.5f) || (ts == 2 && (int) value ("tBody") == 1); }
bool ERProcessor::tailDual() const { return tailFromRoom() && (int) value ("tEng") == 0 && value ("tFreeze") < 0.5f && tailRoomP().shape == 4; }
double ERProcessor::roomRTAt (double f) const { return er::roomAcoustics (tailRoomP(), f).rt; }
void ERProcessor::copyRoom (bool aToB) { for (auto [a, b] : { std::pair { "roomW", "rbW" }, { "roomD", "rbD" }, { "roomH", "rbH" }, { "wall", "rbWall" }, { "floor", "rbFloor" }, { "shape", "rbShape" }, { "splay", "rbSplay" }, { "backAbs", "rbBackAbs" }, { "clutter", "rbClutter" } }) { if (aToB) setParam (b, value (a)); else setParam (a, value (b)); } }
// the tail's base reverb time: Decay, or (ROOM) the room's at 1 kHz
// IR + ALGO: the FDN's level at the hand-over (B ms into a decay of the 1 kHz reverb time); 1 when the IR is not matched
double ERProcessor::hybridFdnGain() const
{
    if (value ("irtAuto") < 0.5f) return 1.0;
    return std::exp (-6.91 * value ("irtB") / 1000.0 / std::max (0.1, rtAt (1000.0)));
}
double ERProcessor::decayBase() const { return tailFromRoom() ? roomRTAt (1000.0) : (double) value ("tDecay"); }
double ERProcessor::rtAt (double f) const
{
    // ROOM: the room's own curve, times the edited curve (as a trim: 100 % everywhere = the room as it is)
    const bool room = tailFromRoom();
    const double decay = room ? 1.0 : (double) value ("tDecay");
    const double base = room ? roomRTAt (f) : 1.0;
    if ((int) value ("rtMode") == 0)
    {
        double nf[5], nm[5], nq[5];
        for (int i = 0; i < 5; ++i) { const juce::String n (i + 1); nf[i] = value (("rtF" + n).toRawUTF8()); nm[i] = value (("rtM" + n).toRawUTF8()); nq[i] = value (("rtQ" + n).toRawUTF8()); }
        return base * rv::rtAtEQ (f, decay, nf, nm, nq);
    }
    const int nb = std::clamp ((int) value ("mbN"), 2, 5);
    double xo[4], m[5]; const char* xid[4] = { "mbX1", "mbX2", "mbX3", "mbX4" }; const char* mid[5] = { "mbM1", "mbM2", "mbM3", "mbM4", "mbM5" };
    for (int i = 0; i < 4; ++i) xo[i] = value (xid[i]);
    for (int i = 1; i < 4; ++i) xo[i] = std::max (xo[i], xo[i - 1] * 1.2);           // keep the crossovers in order
    for (int i = 0; i < 5; ++i) m[i] = value (mid[i]);
    return base * rv::rtAtMB (f, decay, xo, m, nb);
}
// ROOM -> ALGO: the room's curve (with the trim) goes into Decay and the five points, so it can be edited freely
juce::String ERProcessor::roomToAlgo()
{
    // the target: the curve heard now (room x trim), on 1/3 octaves 63 Hz - 12.5 kHz
    std::vector<double> fs, lt;
    for (double f = 63; f < 10500; f *= std::pow (2.0, 1.0 / 3)) { fs.push_back (f); lt.push_back (std::log (std::max (0.05, rtAt (f)))); }
    const double size = er::roomTailSizeMs (tailRoomP());
    const double decay = juce::jlimit (0.1, 30.0, rtAt (1000));
    // log RT = log Decay + sum log(m_i) * shape_i(f): linear in log(m), so least squares for a given layout of the points;
    // a few layouts (where the shelves sit, how steep) are tried and the closest kept
    auto solve = [&] (const double* f, const double* q, double* lm)
    {
        double A[5][6] {};
        for (size_t k = 0; k < fs.size(); ++k)
        {
            double sh[5]; for (int i = 0; i < 5; ++i) sh[i] = rv::rtShape (i, fs[k], f[i], q[i]);
            const double y = lt[k] - std::log (decay);
            for (int i = 0; i < 5; ++i) { for (int j = 0; j < 5; ++j) A[i][j] += sh[i] * sh[j]; A[i][5] += sh[i] * y; }
        }
        for (int i = 0; i < 5; ++i) A[i][i] += 0.01;
        for (int c = 0; c < 5; ++c)
        {
            int pv = c; for (int r = c + 1; r < 5; ++r) if (std::abs (A[r][c]) > std::abs (A[pv][c])) pv = r;
            for (int j = 0; j < 6; ++j) std::swap (A[c][j], A[pv][j]);
            for (int r = 0; r < 5; ++r) if (r != c) { const double t = A[r][c] / A[c][c]; for (int j = c; j < 6; ++j) A[r][j] -= t * A[c][j]; }
        }
        double worst = 0;
        for (int i = 0; i < 5; ++i) lm[i] = std::log (juce::jlimit (0.1, 10.0, std::exp (A[i][5] / A[i][i])));
        for (size_t k = 0; k < fs.size(); ++k)
        {
            double l = std::log (decay); for (int i = 0; i < 5; ++i) l += lm[i] * rv::rtShape (i, fs[k], f[i], q[i]);
            worst += (l - lt[k]) * (l - lt[k]);
        }
        return worst;                                    // (squared error)
    };
    double f[5] = { 125, 400, 1000, 2500, 6000 }, q[5] = { 0.7, 0.9, 0.9, 0.9, 0.7 }, lm[5] {}, best = 1e9;
    for (double f1 : { 100.0, 160.0, 250.0 }) for (double q1 : { 0.5, 0.8, 1.3 })
        for (double f5 : { 3000.0, 5000.0, 8000.0 }) for (double q5 : { 0.5, 0.8, 1.3 }) for (double f4 : { 1800.0, 2800.0 })
        {
            const double ff[5] = { f1, 400, 1000, f4, f5 }, qq[5] = { q1, 0.9, 0.9, 0.9, q5 };
            double l[5]; const double w = solve (ff, qq, l);
            if (w < best) { best = w; for (int i = 0; i < 5; ++i) { f[i] = ff[i]; q[i] = qq[i]; lm[i] = l[i]; } }
        }
    undo.beginNewTransaction();
    auto setG = [this] (const juce::String& id, float v) { if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    const double t125 = rtAt (125), t8k = rtAt (8000);
    setG ("tRoom", 0); setG ("rtMode", 0); setG ("tDecay", (float) decay); setG ("tSize", (float) size);
    for (int i = 0; i < 5; ++i)
    {
        const juce::String n (i + 1);
        setG ("rtF" + n, (float) f[i]); setG ("rtQ" + n, (float) q[i]); setG ("rtM" + n, (float) std::exp (lm[i]));
    }
    uiState.setProperty ("rtAdd", 31, nullptr);          // all five points in use: show them
    return juce::String (t125, 2) + " / " + juce::String (decay, 2) + " / " + juce::String (t8k, 2) + " s  (125 Hz / 1 kHz / 8 kHz)";
}
rv::RTCurve ERProcessor::rtCurve() const
{
    rv::RTCurve c; for (int k = 0; k < rv::kBands; ++k) c.rt[(size_t) k] = rtAt (rv::bandFc (k)); return c;
}
rv::TailSettings ERProcessor::tailSettings (double) const
{
    rv::TailSettings t;
    t.sizeMs = tailFromRoom() ? er::roomTailSizeMs (tailRoomP()) : (double) value ("tSize"); t.diffusion = value ("tDiff") / 100.0;
    t.modDepthMs = value ("tModD") / 100.0 * 1.2; t.modRate = value ("tModR");
    t.rt = rtCurve(); t.freeze = value ("tFreeze") > 0.5f;
    t.engine = (int) value ("tEng");
    t.gravity = value ("aGrav") / 100.0; t.feedback = value ("aFb") / 100.0;
    t.spreadMs = value ("tSpread"); t.shape = value ("tShape") / 100.0; t.bassWidth = value ("tBassW") / 100.0; t.fill = value ("tFill") / 100.0; t.spreadHi = value ("tSpreadHi") / 100.0;
    if (tailDual())
    {   // (0.10.3) connected rooms: two slopes. Fast and slow reverb times per band from the coupled-room model, each with the edited curve as a trim
        const auto R = tailRoomP(); const int sr0 = er::coupledRoomOf (R, R.sx, R.sy), lr = er::coupledRoomOf (R, R.lx, R.ly);
        double ms1 = 0, ms2 = 0; er::coupledSizesMs (R, ms1, ms2);
        for (int k = 0; k < rv::kBands; ++k)
        {
            const double f = rv::bandFc (k), trim = rtAt (f) / std::max (0.05, roomRTAt (f)); const auto c = er::coupledDecay (R, f, sr0, lr);
            t.rt.rt[(size_t) k] = juce::jlimit (0.05, 60.0, c.Ta * trim); t.rt2.rt[(size_t) k] = juce::jlimit (0.05, 60.0, c.Tb * trim);
        }
        const auto c = er::coupledDecay (R, 1000.0, sr0, lr);
        t.dual = true; t.sizeMs = lr == 0 ? ms1 : ms2; t.sizeMs2 = std::max (ms1, ms2);
        if (c.sameRoom) { t.gain1 = std::sqrt (c.wa); t.gain2 = std::sqrt (c.wb); t.delay2Ms = 0; }
        else { t.gain1 = 0; t.gain2 = std::sqrt (c.peak); t.delay2Ms = juce::jlimit (0.0, 200.0, 0.6 * c.peakMs); }
    }
    return t;
}
void ERProcessor::setInputFilter (rv::InputFilter& f, double fs) const
{
    f.set (value ("iLC"), (int) value ("iLCs"), value ("iHC"), (int) value ("iHCs"), value ("iTilt"), fs);
}
void ERProcessor::setMasterEQ (rv::MasterEQ& e, double fs) const
{
    rv::EqBand b[6];
    for (int i = 0; i < 6; ++i)
    {
        const juce::String n (i + 1);
        b[i].f = value (("eqF" + n).toRawUTF8()); b[i].g = value (("eqB" + n).toRawUTF8()) > 0.5f ? value (("eqG" + n).toRawUTF8()) : 0.0; b[i].q = value (("eqQ" + n).toRawUTF8());
    }
    e.set (b, value ("eqHP"), value ("eqLP"), fs, value ("eqOn") > 0.5f);
}
// the reverb's own parameters changed? (compared once per block; the FDN takes them at control rate)
void ERProcessor::updateReverb (bool force)
{
    static const char* const ids[] = { "tDecay", "tSize", "tDiff", "tModD", "tModR", "tFreeze", "rtMode", "tEng", "tRoom", "tSrc", "roomW", "roomD", "roomH", "wall", "floor", "shape", "splay", "backAbs", "rbW", "rbD", "rbH", "rbWall", "rbFloor", "rbShape", "rbSplay", "rbBackAbs", "clutter", "rbClutter", "tRoomSel", "tBody", "aGrav", "aFb", "tSpread", "tShape", "tBassW", "tFill", "tSpreadHi",
        "rtF1", "rtF2", "rtF3", "rtF4", "rtF5", "rtM1", "rtM2", "rtM3", "rtM4", "rtM5", "rtQ1", "rtQ2", "rtQ3", "rtQ4", "rtQ5", "mbN", "mbX1", "mbX2", "mbX3", "mbX4", "mbM1", "mbM2", "mbM3", "mbM4", "mbM5",
        "iLC", "iHC", "iLCs", "iHCs", "iTilt", "eqOn", "eqHP", "eqLP",
        "eqF1", "eqG1", "eqQ1", "eqF2", "eqG2", "eqQ2", "eqF3", "eqG3", "eqQ3", "eqF4", "eqG4", "eqQ4", "eqF5", "eqG5", "eqQ5", "eqF6", "eqG6", "eqQ6", "eqB1", "eqB2", "eqB3", "eqB4", "eqB5", "eqB6" };
    constexpr int nT = 62, nF = 5, nAll = (int) (sizeof (ids) / sizeof (ids[0]));
    bool chT = force, chF = force, chE = force;
    for (int i = 0; i < nAll; ++i)
    {
        const float v = value (ids[i]);
        if (v != revSnap[(size_t) i]) { revSnap[(size_t) i] = v; if (i < nT) chT = true; else if (i < nT + nF) chF = true; else chE = true; }
    }
    const bool modCh = std::abs (decayMod / decayModApplied - 1) > 0.01 || std::abs (trebleMod / trebleModApplied - 1) > 0.01;
    if (chT || modCh)
    {   // (the matrix's Decay / Treble Decay scale the reverb time per band here; offline renders take the plain settings)
        auto t = tailSettings (sr);
        if (decayMod != 1 || trebleMod != 1)
            for (int k = 0; k < rv::kBands; ++k)
            {
                const double w = juce::jlimit (0.0, 1.0, (k - 5) / 2.0);       // 1 kHz: none, 4 kHz and up: all of it
                t.rt.rt[(size_t) k] *= decayMod * std::pow (trebleMod, w); t.rt2.rt[(size_t) k] *= decayMod * std::pow (trebleMod, w);
            }
        fdn.setup (t, force); decayModApplied = decayMod; trebleModApplied = trebleMod;
    }
    // the filters' targets (they glide in 32-sample steps inside processBlock: a jump in a filter's coefficients clicks)
    {
        int n = 0;
        for (auto* id : { "iLC", "iHC", "iTilt" }) filtTgt[(size_t) n++] = value (id);
        const bool eqOn = value ("eqOn") > 0.5f;
        filtTgt[(size_t) n++] = eqOn ? value ("eqHP") : 20.0f; filtTgt[(size_t) n++] = eqOn ? value ("eqLP") : 20000.0f;
        for (int i = 1; i <= 6; ++i) { const juce::String k (i); filtTgt[(size_t) n++] = value (("eqF" + k).toRawUTF8()); filtTgt[(size_t) n++] = value (("eqB" + k).toRawUTF8()) > 0.5f && eqOn ? value (("eqG" + k).toRawUTF8()) : 0.0f; filtTgt[(size_t) n++] = value (("eqQ" + k).toRawUTF8()); }
        filtSlopes[0] = (int) value ("iLCs"); filtSlopes[1] = (int) value ("iHCs");
        if (force || ! filtInit) { filtCur = filtTgt; filtInit = true; glideFilters (0, true, true); glideFilters (0, false, true); }
        else if (chF) glideFilters (0, true, true);         // a slope change applies at once
    }
}

// move the input filter (inFilter) or the master EQ a step of n samples towards their settings (~30 ms)
void ERProcessor::glideFilters (int n, bool inFilter, bool force)
{
    const int i0 = inFilter ? 0 : 3, i1 = inFilter ? 3 : 23;
    const float a = (float) (1.0 - std::exp (-n / (0.06 * sr)));
    bool moved = force;
    for (int i = i0; i < i1; ++i)
    {
        float& c = filtCur[(size_t) i]; const float t = filtTgt[(size_t) i];
        if (c == t) continue;
        if (std::abs (t - c) < 1e-4f * (1 + std::abs (t))) c = t;
        else
        {
            const bool logScale = i == 0 || i == 1 || i == 3 || i == 4 || (i >= 5 && (i - 5) % 3 != 1);
            c = logScale ? c * std::pow (t / c, a) : c + (t - c) * a;
        }
        moved = true;
    }
    if (! moved) return;
    if (inFilter) inF.set (filtCur[0], filtSlopes[0], filtCur[1], filtSlopes[1], filtCur[2], sr);
    else
    {
        rv::EqBand b[6];
        for (int i = 0; i < 6; ++i) { b[i].f = filtCur[(size_t) (5 + 3 * i)]; b[i].g = filtCur[(size_t) (6 + 3 * i)]; b[i].q = filtCur[(size_t) (7 + 3 * i)]; }
        meq.set (b, filtCur[3], filtCur[4], sr, true);
    }
}

// offline: impulse -> (input filter) -> tail delay -> FDN, at the tail level
void ERProcessor::renderTail (double fs, int N, std::vector<float>& L, std::vector<float>& R, rv::Fdn& f) const
{
    juce::ScopedNoDenormals noDenormals;                        // (0.7.3) an offline render on a fit thread: the filters' tails must not slow it down
    L.assign ((size_t) N, 0.0f); R.assign ((size_t) N, 0.0f);
    if (value ("tOn") < 0.5f || N <= 0) return;
    const int tsrc = tailSrcEff();
    std::vector<float> iL, iR; rv::Measure im; if (tsrc != 0) tailIR (iL, iR, im);      // (ALGO alone does not need it: and a fit thread must not touch it)
    const bool irOk = ! iL.empty();
    rv::InputFilter iflt; setInputFilter (iflt, fs);
    const bool filt = (int) value ("iTo") != 2;
    if (irOk && tsrc != 0)
    {   // the IR tail at Predelay + IR Tail Start (resampled to fs), through the input filter
        const double off = (value ("predelay") + value ("irtX")) * fs / 1000.0, ratio = er::kSR / fs;
        rv::InputFilter f2; setInputFilter (f2, fs);
        for (int i = (int) off; i < N; ++i)
        {
            const double x = (i - off) * ratio; const size_t k = (size_t) x; if (k + 1 >= iL.size()) break;
            const float fr = (float) (x - k); float l = iL[k] + fr * (iL[k + 1] - iL[k]), r = iR[k] + fr * (iR[k + 1] - iR[k]);
            if (filt) { l = f2.run (l, 0); r = f2.run (r, 1); }
            L[(size_t) i] += l; R[(size_t) i] += r;
        }
        if (tsrc == 1)
        {
            const float g = (float) er::db2g (value ("tLvl")) * tailBalG();
            for (auto& v : L) v *= g; for (auto& v : R) v *= g;
            colorTail (fs, L, R);
            return;
        }
    }
    std::vector<float> irPartL = L, irPartR = R;
    std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
    if (f.sampleRate() != fs) f.prepare (fs); else f.reset();
    f.setup (tailSettings (fs), true);
    const double fdnMs = tsrc == 2 && irOk ? value ("irtX") + value ("irtB") : value ("tPre");
    const int D = (int) std::lround ((value ("predelay") + fdnMs) * fs / 1000.0);
    std::vector<float> inL ((size_t) N, 0.0f), inR ((size_t) N, 0.0f);
    if (D < N)
        for (int i = D; i < N; ++i)
        {
            const float x = i == D ? 1.0f : 0.0f;
            inL[(size_t) i] = filt ? iflt.run (x, 0) : x; inR[(size_t) i] = filt ? iflt.run (x, 1) : x;
            if (i > D + (int) (0.5 * fs)) break;
        }
    const int B = 1024;
    for (int p = 0; p < N; p += B) { const int n = std::min (B, N - p); f.process (inL.data() + p, inR.data() + p, L.data() + p, R.data() + p, n); }
    if (tsrc == 2 && irOk) { const float hg = (float) hybridFdnGain(); for (size_t i = 0; i < L.size(); ++i) { L[i] *= hg; R[i] *= hg; } }
    for (size_t i = 0; i < L.size(); ++i) { L[i] += irPartL[i]; R[i] += irPartR[i]; }
    const float g = (float) er::db2g (value ("tLvl")) * tailBalG();
    for (auto& v : L) v *= g; for (auto& v : R) v *= g;
    colorTail (fs, L, R);
}
// COLOR on the tail: only Bandwidth and Bits (Wander moves the ER's taps); everything else in the stage is neutral
er::Settings ERProcessor::tailFilterSettings() const
{
    er::Settings s; s.lowCut = value ("tLC"); s.highCut = value ("tHC"); s.lcSlope = (int) value ("tLCs"); s.hcSlope = (int) value ("tHCs"); return s;
}
void ERProcessor::colorTail (double fs, std::vector<float>& L, std::vector<float>& R) const
{
    if (L.empty()) return;
    {   // the tail's filters
        const auto fs2 = tailFilterSettings();
        if (fs2.lowCut > 20.5 || fs2.highCut < 19900) { er::Post p; p.prepare (fs); p.set (fs2); p.reset(); p.process (L.data(), R.data(), (int) L.size()); }
    }
    if (value ("colTail") < 0.5f) return;
    er::ColorStage c; c.prepare (fs); c.snap (true);
    c.process (L.data(), R.data(), (int) L.size(), value ("bw"), value ("bits"), true);
}
// offline: master EQ and output width on the wet signal
void ERProcessor::finishWet (double fs, std::vector<float>& L, std::vector<float>& R) const
{
    rv::MasterEQ e; setMasterEQ (e, fs);
    const float w = value ("outW") / 100.0f; const bool mono = (int) value ("outMode") == 1;
    for (size_t i = 0; i < L.size(); ++i)
    {
        float l = e.run (L[i], 0), r = e.run (R[i], 1);
        const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * w;
        l = m + sd; r = m - sd;
        if (mono) l = r = 0.5f * (l + r);
        L[i] = l; R[i] = r;
    }
}

// ---------------------------------------------------------------------------
//  IR tail: the IR from IR Tail Start on (scaled like the ER: direct = 1), with length, per-band decay,
//  stretch, reverse, level; for IR + ALGO it fades out where the FDN takes over
// ---------------------------------------------------------------------------
void ERProcessor::buildTailIR()
{
    // (0.9.1) the source of a slot (0 = IR A, 1 = IR B): the ER's analysed target when the ER follows that slot,
    // else the raw IR (onset = the first sample at 25 % of the peak)
    struct Src { std::vector<float> L, R; long t0 = 0; bool wetOnly = false, ok = false; };
    auto sourceOf = [this] (int slot) -> Src
    {
        Src o;
        if (slot == erIrSlot() && slot == tgtSlot)          // (the target is the ER's analysis of that slot: while it is still being replaced, the raw IR is used)
        {
            auto T = target();
            if (! T || ! T->valid) return o;
            o.L = T->sig.L; o.R = T->sig.R; o.t0 = T->t0; o.wetOnly = T->wetOnly; o.ok = o.L.size() >= 256; return o;
        }
        o.L = slot == 1 ? tIrRawL : irRawL; o.R = slot == 1 ? tIrRawR : irRawR;
        float pk = 0; for (size_t i = 0; i < o.L.size(); ++i) pk = std::max (pk, std::max (std::abs (o.L[i]), std::abs (o.R[i])));
        for (size_t i = 0; i < o.L.size(); ++i) if (std::max (std::abs (o.L[i]), std::abs (o.R[i])) >= 0.25f * pk) { o.t0 = (long) i; break; }
        o.ok = o.L.size() >= 256; return o;
    };
    auto clearIt = [this] { tailIrReady = false; std::lock_guard<std::mutex> l (irtMutex); irtL.clear(); irtR.clear(); irtMeas = {}; ++irtVersion; };
    const int hs = tailIrSlot();
    Src S = sourceOf (hs);
    if (! S.ok) { clearIt(); return; }
    std::vector<float> SL = std::move (S.L), SR = std::move (S.R); long t0 = S.t0; bool wetOnly = S.wetOnly;
    const double X = value ("irtX"), len = value ("irtLen"), st = value ("irtSt") / 100.0, lvl = value ("irtLvl");
    const double dS[3] = { value ("irtDL"), value ("irtDM"), value ("irtDH") };
    const bool rev = value ("irtRev") > 0.5f; const int src = tailSrcEff(); const double B = value ("irtB");
    const bool autoM = value ("irtAuto") > 0.5f;
    float presetNrm = 0.0f;
    if ((int) value ("tSrc") == 2 && src == 1)
    {   // T-HEAD with an IR body: this IR's first X + B ms, then (60 ms cross-fade) the other IR on the same time axis, each scaled to its direct sound = 1
        Src Bd = sourceOf (1 - hs);
        if (Bd.ok)
        {
            auto dpkOf = [] (const Src& q, const std::vector<float>& L, const std::vector<float>& R)
            {
                float d = 0;
                if (q.wetOnly) { for (size_t i = 0; i < L.size(); ++i) d = std::max (d, std::abs (L[i])); return std::max (1e-9f, 2 * d); }
                for (long i = std::max (0L, q.t0 - 3); i <= std::min ((long) L.size() - 1, q.t0 + 3); ++i) d = std::max (d, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
                return std::max (1e-9f, d);
            };
            Src Hd; Hd.t0 = t0; Hd.wetOnly = wetOnly;
            const float nh = 1.0f / dpkOf (Hd, SL, SR), nb = 1.0f / dpkOf (Bd, Bd.L, Bd.R);
            const long kH = (long) std::lround ((X + B) * er::kK), xf = (long) std::lround (0.060 * er::kSR);
            const long total = std::max ((long) SL.size() - t0, (long) Bd.L.size() - Bd.t0);
            std::vector<float> CL ((size_t) (t0 + total), 0.0f), CR ((size_t) (t0 + total), 0.0f);
            for (long i = 0; i < t0 + total; ++i)
            {
                const long tau = i - t0;
                const float hl = i < (long) SL.size() ? SL[(size_t) i] * nh : 0.0f, hr = i < (long) SL.size() ? SR[(size_t) i] * nh : 0.0f;
                const long j = Bd.t0 + tau; const bool bo = tau >= 0 && j >= 0 && j < (long) Bd.L.size();
                const float bl = bo ? Bd.L[(size_t) j] * nb : 0.0f, br = bo ? Bd.R[(size_t) j] * nb : 0.0f;
                const float w = tau < kH ? 0.0f : tau >= kH + xf ? 1.0f : (float) (tau - kH) / (float) xf;
                CL[(size_t) i] = hl * (1 - w) + bl * w; CR[(size_t) i] = hr * (1 - w) + br * w;
            }
            SL = std::move (CL); SR = std::move (CR); wetOnly = false; presetNrm = 1.0f;
        }
    }
    if (irtWorker.joinable()) irtWorker.join();
    irtVerLaunch = irtVersion.load();                       // (after the join: the finishing build of the older settings has counted already)
    auto refTs = tailSettings (er::kSR);
    irtWorker = std::thread ([this, SL = std::move (SL), SR = std::move (SR), t0, wetOnly, X, len, st, lvl, dS, rev, src, B, autoM, refTs, presetNrm]
    {
        const double fs = er::kSR; const long n = (long) SL.size();
        float dpk = 0; for (long i = std::max (0L, t0 - 3); i <= std::min (n - 1, t0 + 3); ++i) dpk = std::max (dpk, std::max (std::abs (SL[(size_t) i]), std::abs (SR[(size_t) i])));
        if (wetOnly) { float mx = 0; for (auto v : SL) mx = std::max (mx, std::abs (v)); dpk = 2 * mx; }
        const float nrm = presetNrm > 0.0f ? presetNrm : 1.0f / std::max (1e-9f, dpk);
        // the IR from the direct sound (direct removed), for the measurement of its own decay
        std::vector<float> fL, fR;
        for (long i = t0; i < n; ++i) { const bool d = i < t0 + (long) (0.4 * er::kK); fL.push_back (d ? 0.0f : SL[(size_t) i] * nrm); fR.push_back (d ? 0.0f : SR[(size_t) i] * nrm); }
        const auto meas = rv::measureIR (fL, fR, fs);
        // the segment
        const long a = (long) std::lround (X * er::kK), b = std::min ((long) fL.size(), a + (long) (len * fs));
        std::vector<float> L, R;
        if (b - a > 64)
        {
            L.assign (fL.begin() + a, fL.begin() + b); R.assign (fR.begin() + a, fR.begin() + b);
            // decay per band (low < 300 Hz < mid < 3 kHz < high): multiply by the envelope that turns T into s x T
            if (std::abs (dS[0] - 1) > 1e-3 || std::abs (dS[1] - 1) > 1e-3 || std::abs (dS[2] - 1) > 1e-3)
            {
                const double Tb[3] = { meas.t30[1] > 0 ? 0.5 * (meas.t30[1] + meas.t30[2]) : meas.t30Wide, meas.t30[4] > 0 ? meas.t30[4] : meas.t30Wide,
                                       meas.t30[6] > 0 ? 0.5 * (meas.t30[6] + meas.t30[7]) : meas.t30Wide };
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& x = ch == 0 ? L : R;
                    // Linkwitz-Riley (24 dB/oct) 3-band split; the low band gets the 3 kHz allpass so the bands add back flat
                    rv::BQ a[2], b[2], c[2], d[2], ap[2][2];
                    for (int k = 0; k < 2; ++k) { a[k].pass (false, 300, 0.7071, fs); b[k].pass (true, 300, 0.7071, fs); c[k].pass (false, 3000, 0.7071, fs); d[k].pass (true, 3000, 0.7071, fs);
                                                  ap[0][k].pass (false, 3000, 0.7071, fs); ap[1][k].pass (true, 3000, 0.7071, fs); }
                    for (size_t i = 0; i < x.size(); ++i)
                    {
                        const float v = x[i];
                        float lo = a[1].run (a[0].run (v)); const float rest = b[1].run (b[0].run (v));
                        const float mi = c[1].run (c[0].run (rest)), hi = d[1].run (d[0].run (rest));
                        lo = ap[0][1].run (ap[0][0].run (lo)) + ap[1][1].run (ap[1][0].run (lo));          // LR4 low + high at 3 kHz = allpass
                        const double t = (double) i / fs; float y = 0; const float bands[3] = { lo, mi, hi };
                        for (int k = 0; k < 3; ++k)
                        {
                            const double Tk = std::max (0.05, Tb[k]); const double alpha = 6.91 / Tk;
                            y += bands[k] * (float) std::min (31.6, std::exp (-alpha * (1.0 / dS[k] - 1.0) * t));
                        }
                        x[i] = y;
                    }
                }
            }
            // stretch (resample: longer = bigger and lower)
            if (std::abs (st - 1) > 1e-3)
            {
                const size_t n2 = (size_t) (L.size() * st); std::vector<float> L2 (n2), R2 (n2);
                for (size_t i = 0; i < n2; ++i) { const double x = i / st; const size_t k = (size_t) x; const float f = (float) (x - k); const size_t k1 = std::min (k + 1, L.size() - 1);
                                                  L2[i] = L[k] + f * (L[k1] - L[k]); R2[i] = R[k] + f * (R[k1] - R[k]); }
                L.swap (L2); R.swap (R2);
            }
            // loudness: the IR's own (against its direct sound) differs from IR to IR; matched, the IR tail starts as loud
            // as the ALGO tail would with the same decay (energy per second of decay, from the ALGO engine itself)
            float match = 1.0f;
            if (autoM)
            {
                double eI = 0; for (size_t i = 0; i < L.size(); ++i) eI += (double) L[i] * L[i] + (double) R[i] * R[i];
                const double T = std::max (0.1, (meas.t30[4] > 0 ? meas.t30[4] : meas.t30Wide > 0 ? meas.t30Wide : 1.5) * st * dS[1]);
                // the reference: the ALGO engine with a flat 2 s, its energy per second of decay
                auto ts = refTs; for (auto& r : ts.rt.rt) r = 2.0; ts.freeze = false; ts.feedback = 0; ts.gravity = 0;
                rv::Fdn f; f.prepare (fs); f.setup (ts, true);
                const int N = (int) (3.5 * fs), Bk = 1024; std::vector<float> in ((size_t) N, 0.0f), oL ((size_t) N), oR ((size_t) N); in[0] = 1.0f;
                for (int p = 0; p < N; p += Bk) { const int nn = std::min (Bk, N - p); f.process (in.data() + p, in.data() + p, oL.data() + p, oR.data() + p, nn); }
                double eF = 0; for (int i = 0; i < N; ++i) eF += (double) oL[(size_t) i] * oL[(size_t) i] + (double) oR[(size_t) i] * oR[(size_t) i];
                // (the 3.5 s window holds about all of a 2 s decay's energy)
                if (eI > 1e-12 && eF > 1e-12) match = (float) juce::jlimit (0.03, 30.0, std::sqrt ((eF / 2.0) * T / eI));
            }
            // IR + ALGO: the IR hands over to the FDN after B ms (60 ms fade)
            if (src == 2) { const size_t e = std::min (L.size(), (size_t) ((B + 60) * er::kK)); L.resize (e); R.resize (e);
                            const size_t f0 = (size_t) (B * er::kK); for (size_t i = f0; i < e; ++i) { const float g = 1.0f - (float) (i - f0) / (float) std::max ((size_t) 1, e - f0); L[i] *= g; R[i] *= g; } }
            if (rev) { std::reverse (L.begin(), L.end()); std::reverse (R.begin(), R.end()); }
            // fades: 3 ms in, 50 ms out; level
            const float g = (float) er::db2g (lvl) * match; const size_t fi = (size_t) (0.003 * fs), fo = std::min (L.size(), (size_t) (0.05 * fs));
            for (size_t i = 0; i < L.size(); ++i)
            {
                float w = g; if (i < fi) w *= (float) i / fi; if (i + fo >= L.size()) w *= (float) (L.size() - i) / fo;
                L[i] *= w; R[i] *= w;
            }
        }
        { std::lock_guard<std::mutex> l (irtMutex); irtL = L; irtR = R; irtMeas = meas; }
        if (! L.empty()) { loadTailConv(); tailIrReady = true; } else tailIrReady = false;
        ++irtVersion;
    });
}
void ERProcessor::loadTailConv()
{
    std::vector<float> L, R; rv::Measure m; tailIR (L, R, m);
    if (L.empty()) return;
    juce::AudioBuffer<float> b (2, (int) L.size());
    b.copyFrom (0, 0, L.data(), (int) L.size()); b.copyFrom (1, 0, R.data(), (int) R.size());
    tailConv.loadImpulseResponse (std::move (b), er::kSR, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

// IR -> FDN: the ALGO tail takes the IR's decay per band (EQ mode, 5 points fitted to the octave T30s), its size from the
// mixing time and its level from the energy of the IR tail
juce::String ERProcessor::irToFdn()
{
    setParam ("autoLvl", 0);                                    // (0.7.1) a rebuild of an IR keeps the IR's own level: Auto Level off
    std::vector<float> L, R; rv::Measure m; tailIR (L, R, m);
    if (! m.valid) return "no IR";
    auto t30 = [&] (int k) { double v = m.t30[(size_t) k]; return v > 0.05 ? v : (m.t30Wide > 0.05 ? m.t30Wide : 1.5); };
    const double decay = t30 (4);
    requestTransition (false); apvts.copyState(); undo.beginNewTransaction();
    setParam ("rtMode", 0); setParam ("tDecay", (float) juce::jlimit (0.1, 30.0, decay));
    const float nf[5] = { 100, 250, 1000, 4000, 10000 };
    for (int i = 0; i < 5; ++i) { const juce::String n (i + 1); setParam (("rtF" + n).toRawUTF8(), nf[i]); setParam (("rtQ" + n).toRawUTF8(), 0.7f); setParam (("rtM" + n).toRawUTF8(), 1.0f); }
    // fit the 5 multipliers to the 8 octave T30s (a few passes of a simple correction)
    const int bandOf[5] = { 1, 2, 4, 6, 7 };
    for (int pass = 0; pass < 6; ++pass)
        for (int i = 0; i < 5; ++i)
        {
            const juce::String id = "rtM" + juce::String (i + 1);
            const double f = rv::mBandFc (bandOf[i]), want = t30 (bandOf[i]), got = rtAt (f);
            setParam (id.toRawUTF8(), (float) juce::jlimit (0.1, 10.0, value (id.toRawUTF8()) * std::pow (want / std::max (0.05, got), 0.8)));
        }
    if (auto T = target(); T && T->mix > 0 && tailIrSlot() == erIrSlot()) setParam ("tSize", (float) juce::jlimit (25.0, 250.0, T->mix * 1.3));
    setParam ("tPre", (float) juce::jlimit (0.0, 300.0, value ("irtX") * 0.5));
    // level: the FDN's energy after the IR tail start = the IR tail's
    setParam ("tSrc", 0); setParam ("tLvl", 0);
    {
        const double fs = er::kSR; const int N = (int) (fs * std::min (8.0, decay * 1.5 + 0.5));
        rv::Fdn f; std::vector<float> fL, fR; renderTail (fs, N, fL, fR, f);
        const size_t a = (size_t) ((value ("predelay") + value ("irtX")) * er::kK);
        double eF = 0, eI = 0; for (size_t i = a; i < fL.size(); ++i) eF += (double) fL[i] * fL[i] + (double) fR[i] * fR[i];
        for (size_t i = 0; i < L.size(); ++i) eI += (double) L[i] * L[i] + (double) R[i] * R[i];
        if (eF > 1e-12 && eI > 1e-12) setParam ("tLvl", (float) juce::jlimit (-36.0, 12.0, 10 * std::log10 (eI / eF)));
    }
    apvts.copyState();
    juce::String rep; for (int k = 1; k < rv::kMBands - 1; ++k) rep << juce::String ((int) rv::mBandFc (k)) << " Hz " << juce::String (t30 (k), 2) << " / " << juce::String (rtAt (rv::mBandFc (k)), 2) << " s   ";
    return rep;
}

// IR -> TAIL ROOM: the shoebox (size and materials, the current proportions kept) whose reverb time per band (Eyring + air)
// is closest to the IR tail's measured T30; what the materials cannot reach goes into the trim points. Then TAIL = ROOM.
juce::String ERProcessor::irToTailRoom()
{
    setParam ("autoLvl", 0);                                    // (0.7.1) a rebuild of an IR keeps the IR's own level: Auto Level off
    std::vector<float> L, R; rv::Measure m; tailIR (L, R, m);
    if (! m.valid) return {};
    std::vector<double> fs, tg;
    for (int k = 1; k < rv::kMBands - 1; ++k) if (m.t30[(size_t) k] > 0.05) { fs.push_back (rv::mBandFc (k)); tg.push_back (m.t30[(size_t) k]); }
    if (fs.size() < 3) return {};
    auto tgtAt = [&] (double f)
    {   // the measured T30, interpolated in log frequency (held at the ends)
        if (f <= fs.front()) return tg.front(); if (f >= fs.back()) return tg.back();
        for (size_t i = 0; i + 1 < fs.size(); ++i)
            if (f <= fs[i + 1]) { const double t = std::log (f / fs[i]) / std::log (fs[i + 1] / fs[i]); return std::exp (std::log (tg[i]) + t * (std::log (tg[i + 1]) - std::log (tg[i]))); }
        return tg.back();
    };
    const double t1k = tgtAt (1000.0);
    auto R0 = roomParams(); R0.shape = 0;
    const double c0 = std::cbrt (R0.W * R0.D * R0.H);
    auto withScale = [&] (er::RoomP r, double s) { r.W = juce::jlimit (1.5, 40.0, R0.W / c0 * s); r.D = juce::jlimit (1.5, 50.0, R0.D / c0 * s); r.H = juce::jlimit (2.0, 20.0, R0.H / c0 * s); return r; };
    er::RoomP best = R0; double bestE = 1e18;
    double vMix = 0; if (auto T = target(); T && T->valid && T->mix > 5) vMix = juce::jlimit (20.0, 40000.0, T->mix * T->mix);
    for (int w = 0; w < 7; ++w)
        for (int fl = 0; fl < 7; ++fl)
        {
            er::RoomP r = R0; r.wall = w; r.floor = fl;
            // the size where 1 kHz matches (the reverb time grows with the size): bisection on the cube-root scale
            double lo = 1.5, hi = 40.0;
            for (int it = 0; it < 40; ++it) { const double mid = std::sqrt (lo * hi); (er::roomAcoustics (withScale (r, mid), 1000).rt < t1k ? lo : hi) = mid; }
            const auto rr = withScale (r, std::sqrt (lo * hi));
            double e = 0; for (size_t i = 0; i < fs.size(); ++i) { const double d = std::log (er::roomAcoustics (rr, fs[i]).rt / tg[i]); e += d * d; }
            // a weak pull toward the volume the IR's mixing time suggests (sqrt(V) ms, Polack): many rooms fit the decay alone
            const double vv = rr.W * rr.D * rr.H;
            if (vMix > 0) { const double d = std::log (vv / vMix); e += 0.04 * d * d * (double) fs.size(); }
            if (e < bestE) { bestE = e; best = rr; }
        }
    requestTransition (false); apvts.copyState(); undo.beginNewTransaction();
    setParam ("shape", 0); setParam ("roomW", (float) best.W); setParam ("roomD", (float) best.D); setParam ("roomH", (float) best.H);
    setParam ("wall", (float) best.wall); setParam ("floor", (float) best.floor);
    setParam ("tSrc", 0); setParam ("tRoom", 1); setParam ("rtMode", 0);
    // the rest (what the two materials cannot shape): the five trim points, a few passes
    for (int i = 1; i <= 5; ++i) setParam (("rtM" + juce::String (i)).toRawUTF8(), 1.0f);
    for (int pass = 0; pass < 6; ++pass)
        for (int i = 1; i <= 5; ++i)
        {
            const juce::String id = "rtM" + juce::String (i);
            const double f = value (("rtF" + juce::String (i)).toRawUTF8()), want = tgtAt (f), got = rtAt (f);
            setParam (id.toRawUTF8(), (float) juce::jlimit (0.25, 4.0, value (id.toRawUTF8()) * std::pow (want / std::max (0.05, got), 0.8)));
        }
    uiState.setProperty ("rtAdd", 31, nullptr);
    apvts.copyState();
    const auto& M = er::materials();
    return juce::String (best.W, 1) + " x " + juce::String (best.D, 1) + " x " + juce::String (best.H, 1) + " m, "
         + juce::String::fromUTF8 (M[(size_t) best.wall].ja) + " / " + juce::String::fromUTF8 (M[(size_t) best.floor].ja)
         + "  (RMS " + juce::String (std::sqrt (bestE / (double) fs.size()) * 100, 0) + " %)";
}

void ERProcessor::startMeasure()
{
    if (measuring.exchange (true)) return;
    if (measWorker.joinable()) measWorker.join();
    measWorker = std::thread ([this]
    {
        double rmax = 0; for (auto r : rtCurve().rt) rmax = std::max (rmax, r);
        const bool frz = value ("tFreeze") > 0.5f;
        const double fs = er::kSR, lenS = juce::jlimit (1.0, 14.0, (frz ? 3.0 : rmax * 1.6) + (value ("predelay") + value ("tPre")) / 1000.0 + 0.3);
        const int N = (int) (lenS * fs);
        Preview pv; renderPreviewER (pv, lenS * 1000.0);
        std::vector<float> L = pv.L, R = pv.R; L.resize ((size_t) N, 0.0f); R.resize ((size_t) N, 0.0f);
        erToFull (fs, L, R);
        rv::Fdn f; std::vector<float> tL, tR; renderTail (fs, N, tL, tR, f);
        for (int i = 0; i < N; ++i) { L[(size_t) i] += tL[(size_t) i]; R[(size_t) i] += tR[(size_t) i]; }
        finishWet (fs, L, R);
        // the measurement starts at the direct sound (time 0)
        auto m = rv::measureIR (L, R, fs);
        { std::lock_guard<std::mutex> l (measMutex); meas = m; measL = std::move (L); measR = std::move (R); }
        ++measVersion;
        measuring = false;
    });
}

void ERProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if (n > maxBlock) { prepareToPlay (sr, n); }
    const int nIn = getTotalNumInputChannels();
    if (nIn == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // a preset, a source switch or a new IR: fade the ER out, swap everything while it is silent, fade back in
    const int srcNow = (int) value ("src"), irmNow = (int) value ("irMode");
    if (const int r = transReq.load(); r != transSeen || (trans == Trans::normal && curSrc >= 0 && (srcNow != curSrc || irmNow != curIrMode)))
    {
        transSeen = r;
        if (trans == Trans::normal || trans == Trans::in) { trans = Trans::out; tFrom = tGain; tLen = tCount = std::max (1, (int) (sr * 0.010)); }   // tPre stays where it is
        else if (trans == Trans::hold) { tCount = std::max (tCount, (int) (sr * 0.030)); holdDone = false; }      // more changes are still arriving
    }
    // the swap happens at a block boundary while the ER is silent, so the whole next block is rendered with the new settings
    if (trans == Trans::hold && holdDone) { holdDone = false; finishTransition(); trans = Trans::in; tLen = tCount = (int) (sr * 0.025); tGain = 1; tPre = 0; }
    if (trans == Trans::normal)
    {
        // tap-relevant parameters changed? (rate-limited, one cross-fade at a time)
        bool changed = false;
        for (int i = 0; i < kNumTapIds; ++i) { const float v = value (kTapIds[i]); if (v != lastSnap[(size_t) i]) { lastSnap[(size_t) i] = v; changed = true; } }
        if (const int v = irTapsVersion.load(); v != seenIrVersion) { seenIrVersion = v; changed = true; }
        if (changed) pendingRebuild = true;
        sinceRebuild += n;
        if (pendingRebuild && sinceRebuild >= (int) (sr * 0.015) && ! engine.busy()) { rebuildTaps (false); eCur = curConv ? convE.load() : tapEnergyDb (shaped); }
        curS = settings(); curConv = srcNow == 2 && irmNow == 1; curPreMs = value ("predelay"); curSrc = srcNow; curIrMode = irmNow;
        post.set (curS); engine.setWander (curS.wander);
        tail.setup (curS.tailRT, curS.sizeScale);
    }
    {   // the dry key (ducking, gate, envelope) and the modulation sources, once per block
        auto* k = keyBuf.getWritePointer (0); const auto* iL = buffer.getReadPointer (0); const auto* iR = buffer.getReadPointer (1);
        for (int i = 0; i < n; ++i) k[i] = 0.5f * (iL[i] + iR[i]);
        runModSources (n);
    }
    outGain.setTargetValue ((float) er::db2g (value ("out") + 12.0f * modV[dm::dOut]));
    updateReverb (false);

    auto* L = buffer.getWritePointer (0); auto* R = buffer.getWritePointer (1);
    auto* mono = monoBuf.getWritePointer (0);
    float ip = 0;
    // ---- reverb input: mode and width (the dry signal stays as it is) ----
    auto* iL = inBuf.getWritePointer (0); auto* iR = inBuf.getWritePointer (1);
    {
        const int im = (int) value ("inMode"); const float iw = value ("inW") / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (im == 1) l = r = 0.5f * (l + r); else if (im == 2) r = l; else if (im == 3) l = r;
            else { const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * iw; l = m + sd; r = m - sd; }
            iL[i] = l; iR[i] = r;
        }
    }
    // ---- input filter (to the ER, the tail or both) ----
    const int iTo = (int) value ("iTo");
    auto* fL = fBuf.getWritePointer (0); auto* fR = fBuf.getWritePointer (1);
    for (int i = 0; i < n; ++i) { if ((i & 31) == 0) glideFilters (std::min (32, n - i), true, false); fL[i] = inF.run (iL[i], 0); fR[i] = inF.run (iR[i], 1); }
    {
        const float* eL = iTo != 1 ? fL : iL; const float* eR = iTo != 1 ? fR : iR;
        for (int i = 0; i < n; ++i) { mono[i] = 0.5f * (eL[i] + eR[i]); ip = std::max (ip, std::abs (mono[i])); }
    }
    // ---- tail: delay -> FDN (ALGO) and / or the IR tail's convolution ----
    {
        const float* sL = iTo != 2 ? fL : iL; const float* sR = iTo != 2 ? fR : iR;
        const int len = (int) tdBuf[0].size();
        const int tsrc = tailSrcEff(); const bool irOk = tailIrReady.load();
        const double pre = value ("predelay"), X = value ("irtX");
        const double fdnMs = tsrc == 2 && irOk ? X + value ("irtB") : value ("tPre");
        const double want = juce::jlimit (0.0, (double) len - 4, (pre + fdnMs) * sr / 1000.0);
        const double want2 = juce::jlimit (0.0, (double) len - 4, (pre + X) * sr / 1000.0);
        // (0.8.1) a preset / the tail's type or source changed: the tail fades out (6 ms), every glide jumps to the new settings, and it fades back in
        {
            const int key = (int) value ("tEng") + 8 * tsrc + 64 * (value ("tRoom") > 0.5f ? 1 : 0) + 128 * (int) value ("tBody") + 512 * (int) value ("tIrSel") + 1024 * (tailDual() ? 1 : 0);
            if (key != lastTailKey) { if (lastTailKey >= 0) snapReq = true; lastTailKey = key; }
            if (snapReq.exchange (false) && snapState == 0 && snapEnabled) snapState = 1;
            if (snapState == 1 && snapG <= 0.0f)
            {
                fdn.reset(); fdn.setup (tailSettings (sr), true); tdCur = want; tdCur2 = want2; snapState = 2;      // (a jump inside the loop would ring on: start it clean)
            }
            // this block's dip (applied to the tail's input and its output: the input's read position jumps while both are silent)
            snapOn = snapState != 0;
            if (snapOn)
            {
                if ((int) snapBuf.size() < n) snapBuf.resize ((size_t) n);
                const float st = (float) (1.0 / (0.02 * sr)), st2 = (float) (1.0 / (0.006 * sr));
                for (int i = 0; i < n; ++i)
                {
                    if (snapState == 1) snapG = std::max (0.0f, snapG - st);
                    else if (snapState == 2) { snapG = std::min (1.0f, snapG + st2); if (snapG >= 1.0f) snapState = 0; }
                    snapBuf[(size_t) i] = snapG;
                }
            }
        }
        if (tdCur < 0) tdCur = want; if (tdCur2 < 0) tdCur2 = want2;
        const double gl = 1.0 - std::exp (-1.0 / (0.05 * sr));
        auto* tiL = tIn.getWritePointer (0); auto* tiR = tIn.getWritePointer (1);
        auto* ciL = cIn.getWritePointer (0); auto* ciR = cIn.getWritePointer (1);
        auto rd = [&] (double d, int c) { const int di = (int) d; const float fr = (float) (d - di); const int a = (tdW - di + len) % len, b = (a - 1 + len) % len;
                                          return tdBuf[c][(size_t) a] + fr * (tdBuf[c][(size_t) b] - tdBuf[c][(size_t) a]); };
        for (int i = 0; i < n; ++i)
        {
            tdBuf[0][(size_t) tdW] = sL[i]; tdBuf[1][(size_t) tdW] = sR[i];
            tdCur += std::clamp ((want - tdCur) * gl, -0.05, 0.05); tdCur2 += std::clamp ((want2 - tdCur2) * gl, -0.05, 0.05);     // at most 5 % pitch
            tiL[i] = rd (tdCur, 0); tiR[i] = rd (tdCur, 1); ciL[i] = rd (tdCur2, 0); ciR[i] = rd (tdCur2, 1);
            if (++tdW >= len) tdW = 0;
        }
        if (snapOn) for (int i = 0; i < n; ++i) { const float g = snapBuf[(size_t) i]; tiL[i] *= g; tiR[i] *= g; ciL[i] *= g; ciR[i] *= g; }
        auto* oL = tOut.getWritePointer (0); auto* oR = tOut.getWritePointer (1);
        // CPU: TAIL off and faded out -> the tail engines rest (they start clean when switched back on)
        const bool tailWanted = value ("tOn") > 0.5f || tailG > 1e-5f;
        if (! tailWanted) { if (! tailIdle) { fdn.reset(); fdn.rampIn(); tailConv.reset(); tailIdle = true; } tOut.clear (0, n); lastTsrc = 0; }
        else { tailIdle = false; }
        if (! tailWanted) {}
        else if (tsrc != 1 || ! irOk)
        {
            fdn.process (tiL, tiR, oL, oR, n);
            // IR + ALGO: the FDN takes over B ms into a decay, so it starts that much down (it would jump up at the hand-over)
            const float hg = (float) (tsrc == 2 && irOk ? hybridFdnGain() : 1.0);
            for (int i = 0; i < n; ++i) { hybG += (hg - hybG) * 0.002f; oL[i] *= hybG; oR[i] *= hybG; }
        }
        else { tOut.clear (0, n); }
        if (tailWanted && irOk && tsrc != 0)
        {   // the IR tail only runs while it is used (a long IR costs); it starts clean when switched on
            if (lastTsrc == 0) tailConv.reset();
            juce::dsp::AudioBlock<float> blk (cIn.getArrayOfWritePointers(), 2, (size_t) n);
            tailConv.process (juce::dsp::ProcessContextReplacing<float> (blk));
            tOut.addFrom (0, 0, cIn, 0, 0, n); tOut.addFrom (1, 0, cIn, 1, 0, n);
        }
        if (tailWanted) lastTsrc = irOk ? tsrc : 0;
        if (snapOn) for (int i = 0; i < n; ++i) { oL[i] *= snapBuf[(size_t) i]; oR[i] *= snapBuf[(size_t) i]; }
        // COLOR on the tail (0.7.9): the same band limit and bit depth as the ER's
        // the tail's filters (0.8.0), then COLOR
        if (tailWanted) { tailFlt.set (tailFilterSettings()); tailFlt.process (oL, oR, n); }
        tailCol.process (oL, oR, n, value ("bw"), value ("bits"), tailWanted && value ("colTail") > 0.5f);
    }
    wet.clear (0, n);
    auto* wL = wet.getWritePointer (0); auto* wR = wet.getWritePointer (1);
    const bool convMode = curConv;
    // the convolution always runs once an IR is loaded, so switching to it never starts from stale history
    engine.process (mono, wL, wR, n, pdBuf.getWritePointer (0), (float) (curPreMs * sr / 1000.0));
    // CPU: the ER convolution runs only while the IR source is in use; switching to it goes through the silent hold,
    // where it is cleared (finishTransition), so it never plays stale history
    const bool convRun = convReady.load() && (convMode || srcNow == 2);
    if (! convRun) { convWarm = false; convWarmBlocks = 0; convRunning = false; }
    if (convRun)
    {
        if (! convRunning) { conv.reset(); convRunning = true; }         // stale history from before it rested: start clean
        pdBuf.copyFrom (1, 0, pdBuf, 0, 0, n);
        juce::dsp::AudioBlock<float> blk (pdBuf.getArrayOfWritePointers(), 2, (size_t) n);
        conv.process (juce::dsp::ProcessContextReplacing<float> (blk));
        if (convMode) { wet.addFrom (0, 0, pdBuf, 0, 0, n); wet.addFrom (1, 0, pdBuf, 1, 0, n); }
        if (! convWarm && ++convWarmBlocks * n > (int) (sr * 0.35)) convWarm = true;
    }
    // transition envelopes: the fade out is applied after the post chain (so the diffusion rings out under it), the fade in
    // before it (so nothing abrupt enters the diffusion or the tail while the ER comes back)
    for (int i = 0; i < n; ++i)
    {
        if (trans == Trans::out)
        {
            tGain = tFrom * (0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - (float) tCount / (float) tLen)));
            if (--tCount <= 0)
            {   // the ER convolution was resting: hold until its history is full (its ER is at most ~0.35 s), so nothing starts abruptly
                const bool warm = (int) value ("src") == 2 && ! convWarm;
                trans = Trans::hold; tCount = (int) (sr * (warm ? 0.36 : transLong.exchange (false) ? 0.150 : 0.030)); tGain = 0; holdDone = false;
            }   // a new IR: give the convolution time to switch
        }
        else if (trans == Trans::hold) { tGain = 0; if (tCount > 0 && --tCount <= 0) holdDone = true; }
        else if (trans == Trans::in)
        {
            tPre = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - (float) tCount / (float) tLen));
            if (--tCount <= 0) { trans = Trans::normal; tPre = 1; }
        }
        else { tGain = 1; tPre = 1; }
        gPre[(size_t) i] = tPre; gPost[(size_t) i] = tGain;
        wL[i] *= tPre; wR[i] *= tPre;
    }
    post.process (wL, wR, n);
    {   // analyser: direct (input) and ER (before the tail and the mix)
        int w = fifoW.load();
        fifoW0 = w;
        for (int i = 0; i < n; ++i) { fifoD[(size_t) w] = mono[i]; fifoE[(size_t) w] = 0.5f * (wL[i] + wR[i]); fifoI[(size_t) w] = 0.5f * (iL[i] + iR[i]); w = (w + 1) & (fifoSize - 1); }
    }
    const float m = juce::jlimit (0.0f, 1.0f, value ("mix") / 100.0f + 0.5f * modV[dm::dMix]), dT = std::min (1.0f, 2 * (1 - m)), wT = std::min (1.0f, 2 * m);
    const float comp = (float) er::db2g (autoComp.load());
    const float erT = value ("erOn") > 0.5f ? erBalG() * dm::db2g (12.0f * modV[dm::dER]) : 0.0f,
                tT = value ("tOn") > 0.5f ? (float) er::db2g (value ("tLvl") + 12.0f * modV[dm::dTail]) * tailBalG() : 0.0f,
                wT2 = juce::jlimit (0.0f, 2.0f, value ("outW") / 100.0f + modV[dm::dWidth]);
    const bool monoOut = (int) value ("outMode") == 1;
    // ---- dynamics: the settings (with the matrix) ----
    const int dkMode = (int) value ("dkMode"), dkTo = (int) value ("dkTo");
    dm::DuckSettings ds; ds.mode = dkMode; ds.depth = juce::jlimit (0.0f, 24.0f, value ("dkDepth") + 12.0f * modV[dm::dDuck]); ds.thr = value ("dkThr");
    ds.atkMs = value ("dkAtk"); ds.relMs = value ("dkRel"); ds.bands = (int) value ("dkBands");
    const auto* key = keyBuf.getReadPointer (0);
    if (dkMode > 0 && dkTo == 1) ducker.process (key, tOut.getWritePointer (0), tOut.getWritePointer (1), n, ds);     // the tail only
    const auto* toL = tOut.getReadPointer (0); const auto* toR = tOut.getReadPointer (1);
    auto* dL = dynBuf.getWritePointer (0); auto* dR = dynBuf.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        erOnG += 0.002f * (erT - erOnG); tailG += 0.002f * (tT - tailG);
        // ER (with its transition envelope and Auto Level) + tail -> master EQ
        if ((i & 31) == 0) glideFilters (std::min (32, n - i), false, false);
        const float ge = gPost[(size_t) i] * comp * erOnG;
        dL[i] = meq.run (wL[i] * ge + toL[i] * tailG, 0); dR[i] = meq.run (wR[i] * ge + toR[i] * tailG, 1);
    }
    if (dkMode > 0 && dkTo == 0) ducker.process (key, dL, dR, n, ds);
    if (dkMode > 0) { for (int b = 0; b < ducker.nShown; ++b) mDuck[(size_t) b] = ducker.gr[b]; mDuckN = ducker.nShown; } else mDuckN = 0;
    if (value ("gtOn") > 0.5f)
    {
        const float thr = juce::jlimit (-80.0f, 0.0f, value ("gtThr") + 24.0f * modV[dm::dGate]);
        const float* gk = key;
        if ((int) value ("gtKey") == 1) { auto* k2 = monoBuf.getWritePointer (0); for (int i = 0; i < n; ++i) k2[i] = 0.5f * (dL[i] + dR[i]); gk = k2; }
        gate.process (gk, dL, dR, n, thr, value ("gtHold"), value ("gtRel"));
        mGate = gate.g;
    }
    else { gate.reset(); mGate = 1.0f; }
    chorus.process (dL, dR, n, value ("chMix") / 100.0f, value ("chRate"), juce::jlimit (0.0f, 10.0f, value ("chDepth") + 5.0f * modV[dm::dChorus]));
    // ---- output: width, level, dry / wet ----
    float ep = 0, op = 0;
    for (int i = 0; i < n; ++i)
    {
        dryG += 0.003f * (dT - dryG); wetG += 0.003f * (wT - wetG); outW += 0.002f * (wT2 - outW);
        float l = dL[i] * kWetCal, r = dR[i] * kWetCal;           // (0.7.1) the wet bus at the level of common reverb plug-ins
        { const float mm = 0.5f * (l + r), sd = 0.5f * (l - r) * outW; l = mm + sd; r = mm - sd; }
        const float g = outGain.getNextValue();
        ep = std::max (ep, std::max (std::abs (l), std::abs (r)));
        fifoR[(size_t) ((fifoW0 + i) & (fifoSize - 1))] = 0.5f * (l + r);
        float oL = (L[i] * dryG + l * wetG) * g, oR = (R[i] * dryG + r * wetG) * g;
        if (monoOut) oL = oR = 0.5f * (oL + oR);
        L[i] = oL; R[i] = oR; op = std::max (op, std::max (std::abs (oL), std::abs (oR)));
    }
    fifoW.store ((fifoW0 + n) & (fifoSize - 1));
    inPeak = std::max (inPeak.load() * 0.9f, ip); erPeak = std::max (erPeak.load() * 0.9f, ep); outPeak = std::max (outPeak.load() * 0.9f, op);
}

float ERProcessor::tapEnergyDb (const er::TapSet& t) const
{
    // power gain for a wide-band signal: each tap's one-pole filter passes (1 - a) / (1 + a) of white noise
    double e = 0;
    for (int i = 0; i < t.n; ++i)
    {
        const auto& tp = t.t[(size_t) i]; const double a = er::fcToA (tp.fc, er::kSR);
        e += 0.5 * ((double) tp.gL * tp.gL + (double) tp.gR * tp.gR) * (1 - a) / (1 + a);
    }
    return e > 1e-10 ? (float) (10 * std::log10 (e)) : -100.0f;
}

// called on the audio thread while the ER is silent: take the new settings, then match the ER level if the source changed
void ERProcessor::finishTransition()
{
    const int srcNow = (int) value ("src"), irmNow = (int) value ("irMode");
    curS = settings(); curConv = srcNow == 2 && irmNow == 1; curPreMs = value ("predelay");
    post.set (curS); post.reset(); tail.reset(); engine.setWander (curS.wander); tail.setup (curS.tailRT, curS.sizeScale);
    lastSnap.fill (-1e9f);
    for (int i = 0; i < kNumTapIds; ++i) lastSnap[(size_t) i] = value (kTapIds[i]);
    seenIrVersion = irTapsVersion.load();
    rebuildTaps (true);
    const float eNew = curConv ? convE.load() : tapEnergyDb (shaped);
    float c = autoComp.load();
    if (transPreset.exchange (false)) c = 0;                        // a full preset or lesson: its own level
    else if (value ("autoLvl") > 0.5f && srcNow != curSrc && curSrc >= 0 && eNew > -90 && eCur > -90)
        c = juce::jlimit (-30.0f, 30.0f, eCur + c - eNew);            // keep the level that was heard before the switch
    if (value ("autoLvl") < 0.5f) c = 0;
    autoComp = c;
    eCur = eNew; curSrc = srcNow; curIrMode = irmNow;
}

// ---------------------------------------------------------------------------
void ERProcessor::setParam (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

// ---- parameter lock (0.8.0): the ids kept in uiState "locks" stay as they are when presets, section presets or lessons load
juce::StringArray ERProcessor::lockedIds() const { juce::StringArray a; a.addTokens (uiState.getProperty ("locks").toString(), ",", ""); a.removeEmptyStrings(); return a; }
bool ERProcessor::isLocked (const juce::String& id) const { return lockedIds().contains (id); }
void ERProcessor::setLocked (const juce::String& id, bool on)
{
    auto a = lockedIds(); if (on) a.addIfNotAlreadyThere (id); else a.removeString (id);
    uiState.setProperty ("locks", a.joinIntoString (","), nullptr);
}
ERProcessor::KeepLocked::KeepLocked (ERProcessor& pp) : p (pp) { p.snapReq = true;        /* (0.8.1) a load jumps the tail to the new settings (no morph) */ for (auto& id : p.lockedIds()) if (p.apvts.getParameter (id) != nullptr) v.push_back ({ id, p.value (id.toRawUTF8()) }); }
ERProcessor::KeepLocked::~KeepLocked() { for (auto& [id, x] : v) p.setParam (id.toRawUTF8(), x); }

void ERProcessor::applyPreset (int index)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    currentUserPreset = {};
    const auto& P = presets(); if (index < 0 || index >= (int) P.size()) return;
    requestTransition (true);
    undo.beginNewTransaction();
    static const juce::StringArray keep { "irMode", "irWin", "irAuto", "irThr", "irWet", "irSplit", "irFill", "mix", "out", "tail", "tailRT", "tailLvl", "autoLvl" };
    for (auto* id : pid::ids)
        if (! keep.contains (id))
            if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    for (auto& kv : P[(size_t) index].v) setParam (kv.first, kv.second);
    // (0.7.1) Auto Level: on for the starting points (exploring sources), off for a designed preset (its own level)
    setParam ("autoLvl", P[(size_t) index].group == 2 ? 1.0f : 0.0f);
    currentPreset = index;
    uiState.setProperty ("preset", index, nullptr);
    uiState.setProperty ("eqAdd", 0, nullptr); uiState.setProperty ("rtAdd", 0, nullptr);      // the graphs show only the points in use
    const auto& d = P[(size_t) index];
    uiState.setProperty ("unit", d.unit, nullptr); uiState.setProperty ("prog", d.prog, nullptr);
    if (d.unit >= 0)
    {
        juce::StringArray sa; for (float v : d.panel) sa.add (juce::String (v));
        uiState.setProperty ("pnl", sa.joinIntoString (","), nullptr);
        applyPanel();
    }
}

void ERProcessor::readFifo2 (std::vector<float>& in, std::vector<float>& wet, int n) const
{
    in.resize ((size_t) n); wet.resize ((size_t) n);
    const int w = fifoW.load();
    for (int i = 0; i < n; ++i) { const int k = (w - n + i + fifoSize) & (fifoSize - 1); in[(size_t) i] = fifoI[(size_t) k]; wet[(size_t) i] = fifoR[(size_t) k]; }
}
void ERProcessor::readFifo (std::vector<float>& d, std::vector<float>& e, int n) const
{
    d.resize ((size_t) n); e.resize ((size_t) n);
    const int w = fifoW.load();
    for (int i = 0; i < n; ++i) { const int k = (w - n + i + fifoSize) & (fifoSize - 1); d[(size_t) i] = fifoD[(size_t) k]; e[(size_t) i] = fifoE[(size_t) k]; }
}

static bool isSourceId (const juce::String& id, int src)
{
    static const juce::StringArray room { "roomW", "roomD", "roomH", "wall", "floor", "order", "srcX", "srcY", "lisX", "lisY", "shape", "splay", "backAbs", "clutter" };
    static const juce::StringArray pat { "first", "length", "taps", "growth", "energy", "decay", "damp", "vari" };
    return src == 0 ? room.contains (id) : src == 1 ? pat.contains (id) : false;
}

void ERProcessor::applySourcePreset (int index)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    const auto& P = presets(); if (index < 0 || index >= (int) P.size()) return;
    const auto& d = P[(size_t) index];
    const int src = d.group == 0 ? 0 : 1;
    // the preset's full parameter set, then keep only this source's parameters
    std::map<juce::String, float> m;
    for (auto& kv : d.v) m[kv.first] = kv.second;
    if (d.unit >= 0) { ParamList o; units()[(size_t) d.unit].map (d.prog, d.panel, o); for (auto& kv : o) m[kv.first] = kv.second; }
    requestTransition (false);
    undo.beginNewTransaction();
    for (auto* id : pid::ids)
        if (isSourceId (id, src))
            if (auto* p = apvts.getParameter (id))
            {
                const float v = m.count (id) ? m[id] : p->convertFrom0to1 (p->getDefaultValue());
                setParam (id, v);
            }
    setParam ("src", (float) src);
    currentPreset = -1; uiState.setProperty ("preset", -1, nullptr); uiState.setProperty ("unit", -1, nullptr);
    uiState.setProperty ("srcPreset", index, nullptr);
}

const juce::StringArray& ERProcessor::voiceColorIds()
{
    static const juce::StringArray ids { "predelay", "size", "width", "diffusion", "darken", "lowcut", "highcut", "lcSlope", "hcSlope", "level", "bw", "bits", "wander" };
    return ids;
}
juce::StringArray ERProcessor::nonNeutralVoiceColor() const
{
    juce::StringArray out;
    for (auto& id : voiceColorIds())
        if (auto* p = apvts.getParameter (id))
            if (std::abs (p->getValue() - neutralValue (id)) > 1e-4f)
            {
                if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr) { out.add (p->getName (20) + " " + p->getCurrentValueAsText()); continue; }
                const float v = value (id.toRawUTF8()), a = std::abs (v);
                const juce::String num = a >= 1000 ? juce::String (v / 1000.0f, 1) + " k" : a >= 100 ? juce::String ((int) std::round (v)) + " " : a >= 10 ? juce::String (v, 1) + " " : juce::String (v, 2) + " ";
                out.add (p->getName (20) + " " + (num + p->getLabel()).trim());
            }
    return out;
}
void ERProcessor::resetVoiceColor()
{
    requestTransition (false);
    undo.beginNewTransaction();
    for (auto& id : voiceColorIds())
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (neutralValue (id)); p->endChangeGesture(); }
}
// the neutral value (normalised): the default, except Darkening with an IR source = the IR's own estimate
float ERProcessor::neutralValue (const juce::String& id) const
{
    auto* p = apvts.getParameter (id); if (p == nullptr) return 0;
    if (id == "darken" && (int) value ("src") == 2)
        if (auto T = target(); T && T->valid)
            return apvts.getParameterRange (id).convertTo0to1 (T->ir.darken);
    return p->getDefaultValue();
}

void ERProcessor::applyColorPreset (int index)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    const auto& C = colorPresets(); if (index < 0 || index >= (int) C.size()) return;
    requestTransition (false);
    undo.beginNewTransaction();
    setParam ("bw", C[(size_t) index].bw); setParam ("bits", C[(size_t) index].bits); setParam ("wander", C[(size_t) index].wander);
}

juce::Array<juce::File> ERProcessor::irFilesIn (const juce::File& folder)
{
    auto a = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.WAV;*.AIF;*.AIFF;*.FLAC");
    struct Cmp { int compareElements (const juce::File& x, const juce::File& y) const { return x.getFileName().compareNatural (y.getFileName()); } } cmp;
    a.sort (cmp);
    return a;
}

void ERProcessor::setIRList (const juce::Array<juce::File>& files, int startIndex, int slot)
{
    auto& L = slot == 1 ? irListB : irList; auto& ix = slot == 1 ? irListIndexB : irListIndex;
    L = files; ix = juce::jlimit (-1, files.size() - 1, startIndex);
    juce::StringArray sa; for (auto& f : files) sa.add (f.getFullPathName());
    uiState.setProperty (slot == 1 ? "irListB" : "irList", sa.joinIntoString ("\n"), nullptr);
    uiState.setProperty (slot == 1 ? "irIndexB" : "irIndex", ix, nullptr);
}

juce::String ERProcessor::stepIR (int delta, int slot)
{
    auto& L = slot == 1 ? irListB : irList; auto& ix = slot == 1 ? irListIndexB : irListIndex;
    const char* kL = slot == 1 ? "irListB" : "irList"; const char* kI = slot == 1 ? "irIndexB" : "irIndex";
    if (L.isEmpty())
    {
        juce::StringArray sa; sa.addLines (uiState.getProperty (kL, "").toString());
        for (auto& p : sa) if (p.isNotEmpty()) L.add (juce::File (p));
        ix = (int) uiState.getProperty (kI, 0);
    }
    if (L.isEmpty()) return "No IR list";
    for (int tries = 0; tries < L.size(); ++tries)
    {
        ix = ((ix + delta) % L.size() + L.size()) % L.size();
        uiState.setProperty (kI, ix, nullptr);
        if (L[ix].existsAsFile()) return loadIRSlot (L[ix], slot);
    }
    return "Files not found";
}

std::vector<float> ERProcessor::panelDefaults() const
{
    const int u = unitIndex(), pr = unitProg();
    for (auto& d : presets()) if (d.unit == u && d.prog == pr) return d.panel;
    std::vector<float> v; if (u >= 0) for (auto& c : units()[(size_t) u].ctl) v.push_back (0.5f * (c.lo + c.hi));
    return v;
}
std::vector<float> ERProcessor::panelValues() const
{
    const int u = unitIndex(); if (u < 0 || u >= (int) units().size()) return {};
    const auto& U = units()[(size_t) u];
    juce::StringArray sa; sa.addTokens (uiState.getProperty ("pnl", "").toString(), ",", "");
    auto v = panelDefaults();
    for (int i = 0; i < (int) U.ctl.size() && i < sa.size(); ++i) v[(size_t) i] = juce::jlimit (U.ctl[(size_t) i].lo, U.ctl[(size_t) i].hi, sa[i].getFloatValue());
    return v;
}
ParamList ERProcessor::panelOutputs() const
{
    ParamList o; const int u = unitIndex(); if (u < 0 || u >= (int) units().size()) return o;
    auto v = panelValues(); if (v.size() < units()[(size_t) u].ctl.size()) return o;
    units()[(size_t) u].map (unitProg(), v, o);
    linkTail (o);
    return o;
}
// the faceplate drives the tail too: its reverb time, damping, size, diffusion and motion
// (unless the unit's map already sets the tail parameter itself)
void ERProcessor::linkTail (ParamList& o)
{
    auto has = [&] (const char* id) { for (auto& kv : o) if (juce::String (kv.first) == id) return true; return false; };
    auto get = [&] (const char* id) { for (auto& kv : o) if (juce::String (kv.first) == id) return kv.second; return 0.0f; };
    ParamList add;
    if (has ("tailRT") && ! has ("tDecay")) add.push_back ({ "tDecay", juce::jlimit (0.1f, 30.0f, get ("tailRT")) });
    if (has ("damp") && ! has ("rtF5"))
    {   // a loop damping filter shortens the reverb time above it: the high shelf of the RT curve sits under the damping frequency
        const float d = get ("damp");
        add.push_back ({ "rtF5", juce::jlimit (1000.0f, 12000.0f, d * 0.5f) });
        add.push_back ({ "rtM5", 0.45f });
        add.push_back ({ "rtM4", juce::jlimit (0.5f, 1.0f, 0.55f + 0.45f * (float) (std::log (d / 1500.0f) / std::log (16000.0f / 1500.0f))) });
    }
    if (has ("size") && ! has ("tSize")) add.push_back ({ "tSize", juce::jlimit (10.0f, 300.0f, 80.0f * get ("size") / 100.0f) });
    if (has ("diffusion") && ! has ("tDiff")) add.push_back ({ "tDiff", juce::jlimit (0.0f, 100.0f, get ("diffusion")) });
    if (has ("wander") && ! has ("tModD")) add.push_back ({ "tModD", juce::jlimit (0.0f, 100.0f, get ("wander") * 0.6f) });
    for (auto& kv : add) o.push_back (kv);
}
void ERProcessor::applyPanel() { for (auto& kv : panelOutputs()) setParam (kv.first, kv.second); }
void ERProcessor::setPanelValue (int ctl, float v)
{
    const int u = unitIndex(); if (u < 0 || u >= (int) units().size()) return;
    const auto& U = units()[(size_t) u]; if (ctl < 0 || ctl >= (int) U.ctl.size()) return;
    auto vals = panelValues();
    vals[(size_t) ctl] = juce::jlimit (U.ctl[(size_t) ctl].lo, U.ctl[(size_t) ctl].hi, v);
    juce::StringArray sa; for (float x : vals) sa.add (juce::String (x));
    uiState.setProperty ("pnl", sa.joinIntoString (","), nullptr);
    applyPanel();
}

void ERProcessor::applyLesson (int index)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    const auto& Ls = lessons(); if (index < 0 || index >= (int) Ls.size()) return;
    const auto& l = Ls[(size_t) index];
    requestTransition (true);
    if (const int pi = presetIndex (l.preset); pi >= 0) applyPreset (pi);
    else
    {
        undo.beginNewTransaction();
        static const juce::StringArray keep { "irMode", "irWin", "irAuto", "irThr", "irWet", "irSplit", "irFill", "out", "autoLvl" };
        for (auto* id : pid::ids)
            if (! keep.contains (id))
                if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
        currentPreset = -1; uiState.setProperty ("preset", -1, nullptr);
    }
    for (auto& kv : l.v) setParam (kv.first, kv.second);
    uiState.setProperty ("lesson", index, nullptr);
}

// ---------------------------------------------------------------------------
//  IR
// ---------------------------------------------------------------------------
juce::File ERProcessor::presetRoot()
{
    // (0.7.1) macOS: ~/Library/Application Support/JPK/Reverb Anatomy Lab (the usual place; before 0.7.1 it was ~/Library/JPK/...)
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    const auto old = base.getChildFile ("JPK").getChildFile ("Reverb Anatomy Lab");
    base = base.getChildFile ("Application Support");
   #endif
    const auto root = base.getChildFile ("JPK").getChildFile ("Reverb Anatomy Lab");
   #if JUCE_MAC
    static bool moved = false;
    if (! moved)
    {   // once: what was saved in the old place comes over (nothing is overwritten, the old folder stays)
        moved = true;
        if (old.isDirectory())
            for (auto& f : old.findChildFiles (juce::File::findFiles, true, "*.rvpreset"))
            {
                const auto dst = root.getChildFile (f.getRelativePathFrom (old));
                if (! dst.existsAsFile()) { dst.getParentDirectory().createDirectory(); f.copyFileTo (dst); }
            }
    }
   #endif
    return root;
}
juce::File ERProcessor::userPresetDir() { return presetRoot().getChildFile ("User Presets"); }

// (0.7.1) the built-in presets as files (to look at, copy or start from): Factory Presets/<kind>/<name>.rvpreset
juce::File ERProcessor::writeFactoryPresets()
{
    const auto dir = presetRoot().getChildFile ("Factory Presets");
    const auto& P = presets();
    for (int i = 0; i < (int) P.size(); ++i)
    {
        const auto& d = P[(size_t) i];
        juce::String kind = d.group == 0 ? "ER - Real spaces" : d.group == 1 ? "ER - Classic units" : d.group == 2 ? "Starting points" : "Reverb";
        if (d.group == 3) if (auto* c = presetCat (d.name)) kind = juce::String ("Reverb - ") + c->type;
        const auto f = dir.getChildFile (juce::File::createLegalFileName (kind)).getChildFile (juce::File::createLegalFileName (juce::String::fromUTF8 (d.name)) + ".rvpreset");
        f.getParentDirectory().createDirectory();
        juce::XmlElement x ("RVPRESET");
        x.setAttribute ("name", juce::String::fromUTF8 (d.name)); x.setAttribute ("version", JucePlugin_VersionString); x.setAttribute ("factory", 1);
        x.setAttribute ("note", juce::String::fromUTF8 (d.ja));
        for (auto& kv : d.v) { auto* e = x.createNewChildElement ("P"); e->setAttribute ("id", kv.first); e->setAttribute ("v", (double) kv.second); }
        x.writeTo (f);
    }
    return dir;
}
juce::Array<juce::File> ERProcessor::userPresets() const
{
    auto a = userPresetDir().findChildFiles (juce::File::findFiles, false, "*.rvpreset");
    std::sort (a.begin(), a.end(), [] (const juce::File& x, const juce::File& y) { return x.getFileName().compareNatural (y.getFileName()) < 0; });
    return a;
}
juce::File ERProcessor::saveUserPreset (const juce::String& nameIn)
{
    const auto name = juce::File::createLegalFileName (nameIn.trim().isEmpty() ? juce::String ("User Preset") : nameIn.trim());
    auto dir = userPresetDir(); dir.createDirectory();
    juce::XmlElement x ("RVPRESET");
    x.setAttribute ("name", nameIn.trim()); x.setAttribute ("version", JucePlugin_VersionString);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "autoLvl") continue;            // a working aid, not a sound
            auto* e = x.createNewChildElement ("P"); e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("v", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if ((int) value ("src") == 2 && lastIRFile.existsAsFile()) x.setAttribute ("ir", lastIRFile.getFullPathName());
    if (tailIrOwn() && lastTailIRFile.existsAsFile()) x.setAttribute ("tir", lastTailIRFile.getFullPathName());
    const auto f = dir.getChildFile (name + ".rvpreset");
    x.writeTo (f);
    currentUserPreset = nameIn.trim();
    return f;
}
// a built-in preset, but only its part (the ER presets from the ER tab): everything else keeps its value
void ERProcessor::applyPresetPart (int index, const juce::String& section)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    std::vector<std::pair<juce::String, float>> keep;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (sectionOf (rp->getParameterID()) != section) keep.push_back ({ rp->getParameterID(), value (rp->getParameterID().toRawUTF8()) });
    const int cur = currentPreset; const auto pv = uiState.getProperty ("preset"), ea = uiState.getProperty ("eqAdd"), ra = uiState.getProperty ("rtAdd");
    applyPreset (index);
    for (auto& [id, v] : keep) setParam (id.toRawUTF8(), v);
    if (section == "ER") setParam ("erOn", 1);
    if (section == "TAIL") setParam ("tOn", 1);
    currentPreset = cur; uiState.setProperty ("preset", pv, nullptr); uiState.setProperty ("eqAdd", ea, nullptr); uiState.setProperty ("rtAdd", ra, nullptr);
}
juce::String ERProcessor::sectionOf (const juce::String& id)
{
    static const juce::StringArray io { "inMode", "inW", "outMode", "outW", "iLC", "iHC", "iLCs", "iHCs", "iTilt", "iTo" };
    static const juce::StringArray tail { "rbW", "rbD", "rbH", "rbWall", "rbFloor", "rbShape", "rbSplay", "tRoomSel", "tIrSel", "tBody", "tLC", "tHC", "tLCs", "tHCs", "tDecay", "tSize", "tPre", "tDiff", "tModD", "tModR", "tLvl", "tFreeze", "rtMode", "mbN", "tSrc", "tEng", "tRoom", "aGrav", "aFb", "tSpread", "tShape", "tBassW", "tFill", "tSpreadHi" };
    static const juce::StringArray global { "mix", "out", "erOn", "tOn", "erBal", "autoLvl", "colEr", "colTail" };
    if (id.startsWith ("dk") || id.startsWith ("gt")) return "DYN";
    if (id.startsWith ("ch") || id.startsWith ("l1") || id.startsWith ("l2") || id == "enAtk" || id == "enRel" || id == "enSens" || (id.length() == 5 && id[0] == 'm' && juce::CharacterFunctions::isDigit (id[1]))) return "MOD";
    if (io.contains (id)) return "IO";
    if (tail.contains (id) || id.startsWith ("rt") || id.startsWith ("mb") || id.startsWith ("irt")) return "TAIL";
    if (id.startsWith ("eq")) return "EQ";
    if (global.contains (id)) return {};
    return "ER";
}
juce::Array<juce::File> ERProcessor::sectionPresets (const juce::String& sec) const
{
    auto a = sectionDir (sec).findChildFiles (juce::File::findFiles, false, "*.rvsec");
    std::sort (a.begin(), a.end(), [] (const juce::File& x, const juce::File& y) { return x.getFileName().compareNatural (y.getFileName()) < 0; });
    return a;
}
juce::File ERProcessor::saveSectionPreset (const juce::String& sec, const juce::String& nameIn)
{
    const auto name = juce::File::createLegalFileName (nameIn.trim().isEmpty() ? sec + " preset" : nameIn.trim());
    auto dir = sectionDir (sec); dir.createDirectory();
    juce::XmlElement x ("RVSECTION");
    x.setAttribute ("section", sec); x.setAttribute ("name", nameIn.trim()); x.setAttribute ("version", JucePlugin_VersionString);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && sectionOf (rp->getParameterID()) == sec)
        {
            auto* e = x.createNewChildElement ("P"); e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("v", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (sec == "ER" && (int) value ("src") == 2 && lastIRFile.existsAsFile()) x.setAttribute ("ir", lastIRFile.getFullPathName());
    if (sec == "TAIL" && tailIrOwn() && lastTailIRFile.existsAsFile()) x.setAttribute ("tir", lastTailIRFile.getFullPathName());
    const auto f = dir.getChildFile (name + ".rvsec");
    x.writeTo (f);
    return f;
}
bool ERProcessor::loadSectionPreset (const juce::File& f)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    auto x = juce::XmlDocument::parse (f);
    if (x == nullptr || ! x->hasTagName ("RVSECTION")) return false;
    const auto sec = x->getStringAttribute ("section");
    requestTransition (true);
    undo.beginNewTransaction();
    for (auto* e : x->getChildWithTagNameIterator ("P"))
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (e->getStringAttribute ("id"))); rp != nullptr && sectionOf (rp->getParameterID()) == sec)
        { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("v"))); rp->endChangeGesture(); }
    if (const juce::File ir (x->getStringAttribute ("ir")); x->hasAttribute ("ir") && ir.existsAsFile()) loadIR (ir);
    if (const juce::File ir (x->getStringAttribute ("tir")); x->hasAttribute ("tir") && ir.existsAsFile()) loadTailIR (ir);
    if (sec == "ER") { uiState.setProperty ("unit", -1, nullptr); }
    return true;
}
bool ERProcessor::loadUserPreset (const juce::File& f)
{
    const KeepLocked keepLocked (*this);          // (0.8.0) locked parameters keep their values
    auto x = juce::XmlDocument::parse (f);
    if (x == nullptr || ! x->hasTagName ("RVPRESET")) return false;
    requestTransition (true);
    apvts.copyState();
    undo.beginNewTransaction();
    // a parameter the file does not have (a preset from an older version) goes to its default, not to whatever was set before
    std::set<juce::String> inFile;
    for (auto* e : x->getChildWithTagNameIterator ("P")) inFile.insert (e->getStringAttribute ("id"));
    for (auto* pr : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (pr))
            if (rp->getParameterID() != "autoLvl" && inFile.count (rp->getParameterID()) == 0 && rp->getValue() != rp->getDefaultValue())
            { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture(); }
    for (auto* e : x->getChildWithTagNameIterator ("P"))
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (e->getStringAttribute ("id"))))
        { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("v"))); rp->endChangeGesture(); }
    if (const juce::File ir (x->getStringAttribute ("ir")); x->hasAttribute ("ir") && ir.existsAsFile()) loadIR (ir);
    if (const juce::File ir (x->getStringAttribute ("tir")); x->hasAttribute ("tir") && ir.existsAsFile()) loadTailIR (ir);
    currentPreset = -1; uiState.setProperty ("preset", -1, nullptr); uiState.setProperty ("unit", -1, nullptr);
    autoComp = 0; setParam ("autoLvl", 0);                    // (0.7.1) a saved sound: its own level
    currentUserPreset = x->getStringAttribute ("name", f.getFileNameWithoutExtension());
    return true;
}

void ERProcessor::earBegin()
{
    if (earSnap.isValid()) return;
    earSnap = apvts.copyState();
}
void ERProcessor::earEnd()
{
    if (! earSnap.isValid()) return;
    requestTransition (true);
    auto snap = earSnap; earSnap = juce::ValueTree();
    for (int i = 0; i < snap.getNumChildren(); ++i)
    {
        auto c = snap.getChild (i);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (c.getProperty ("id").toString())))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) c.getProperty ("value")));
    }
    undo.clearUndoHistory();
}

juce::String ERProcessor::loadTailIR (const juce::File& f)
{
    std::vector<float> L, R;
    juce::String shown; const auto err = readIRFileAuto (f, L, R, &shown); if (err.isNotEmpty()) return err;
    tIrRawL = std::move (L); tIrRawR = std::move (R); tIrName = shown; lastTailIRFile = f;
    uiState.setProperty ("tIrName", tIrName, nullptr);
    ++tIrVersion;
    return {};
}
void ERProcessor::clearTailIR()
{
    tIrRawL.clear(); tIrRawR.clear(); tIrName = {}; lastTailIRFile = juce::File();
    uiState.setProperty ("tIrName", "", nullptr);
    ++tIrVersion;
}
// (0.10.16) an L / R set (name_L + name_R, also " L", "-L", "_l") is found by itself: two mono files = the left and right ears, two stereo files = a true-stereo pair (added together)
juce::String ERProcessor::readIRFileAuto (const juce::File& fIn, std::vector<float>& outL, std::vector<float>& outR, juce::String* shownName)
{
    juce::File f = fIn, partner = fit::pairOf (f);
    if (! partner.existsAsFile())
    {
        const auto left = [&] { const auto nm = f.getFileNameWithoutExtension();
            for (auto [a, b] : { std::pair<const char*, const char*> { "_L", "_R" }, { " L", " R" }, { "-L", "-R" }, { "_l", "_r" } })
                if (nm.endsWith (b)) { const auto c = f.getSiblingFile (nm.dropLastCharacters (2) + a + f.getFileExtension()); if (c.existsAsFile()) return c; }
            return juce::File(); }();
        if (left.existsAsFile()) { partner = f; f = left; } else partner = juce::File();
    }
    if (auto err = readIRFile (f, outL, outR); err.isNotEmpty()) return err;
    if (shownName != nullptr) *shownName = f.getFileName();
    if (! partner.existsAsFile()) return {};
    std::vector<float> L2, R2; if (readIRFile (partner, L2, R2).isNotEmpty()) return {};
    const auto chans = [] (const juce::File& x) { juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (x)); return r ? (int) r->numChannels : 0; };
    const size_t n = std::min ((size_t) (er::kSR * 6.0), std::max (outL.size(), L2.size()));
    outL.resize (n, 0.0f); outR.resize (n, 0.0f);
    if (chans (f) == 1 && chans (partner) == 1) { for (size_t i = 0; i < L2.size() && i < n; ++i) outR[i] = L2[i]; }       // left ear file + right ear file
    else for (size_t i = 0; i < L2.size() && i < n; ++i) { outL[i] += L2[i]; outR[i] += R2[i]; }                            // true stereo: L-in + R-in
    if (shownName != nullptr) *shownName = fit::pairName (f) + juce::String (" (L+R)");
    return {};
}
// any audio file -> 48 kHz stereo, at most 6 s
juce::String ERProcessor::readIRFile (const juce::File& f, std::vector<float>& outL, std::vector<float>& outR)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr) return "Unsupported file";
    const double fsr = rd->sampleRate > 0 ? rd->sampleRate : 48000.0;
    const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (fsr * 6.0));
    if (len < 64) return "File too short";
    juce::AudioBuffer<float> b ((int) std::max (1u, std::min (2u, rd->numChannels)), len);
    rd->read (&b, 0, len, 0, true, b.getNumChannels() > 1);
    const double ratio = fsr / er::kSR; const int outLen = (int) std::floor ((len - 2) / ratio);
    outL.assign ((size_t) outLen, 0.0f); outR.assign ((size_t) outLen, 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        const float* src = b.getReadPointer (std::min (c, b.getNumChannels() - 1));
        auto& dst = c == 0 ? outL : outR;
        if (std::abs (ratio - 1.0) < 1e-9) { std::copy (src, src + outLen, dst.begin()); continue; }
        juce::LagrangeInterpolator li; li.process (ratio, src, dst.data(), outLen);
    }
    const size_t keep = std::min (outL.size(), (size_t) (er::kSR * 6.0));
    outL.resize (keep); outR.resize (keep);
    return {};
}

juce::String ERProcessor::loadIR (const juce::File& f)
{
    lastIRFile = f;
    std::vector<float> L, R; juce::String shown;
    if (const auto err = readIRFileAuto (f, L, R, &shown); err.isNotEmpty()) return err;
    if (L.size() < 64) return "File too short";
    irRawL = std::move (L); irRawR = std::move (R);
    irName = shown;
    uiState.setProperty ("irName", irName, nullptr);
    reanalyse();
    return {};
}

void ERProcessor::clearIR()
{
    setIRList ({}, -1);
    joinWorker();
    irRawL.clear(); irRawR.clear(); irName = {};
    tailIrReady = false; { std::lock_guard<std::mutex> l (irtMutex); irtL.clear(); irtR.clear(); irtMeas = {}; } ++irtVersion;
    uiState.setProperty ("irName", "", nullptr);
    { std::lock_guard<std::mutex> l (tMutex); tgt.reset(); }
    convReady = false;
    irTaps[1 - irTapIdx.load()] = er::IrSet(); irTaps[1 - irTapIdx.load()].taps.n = 0; irTapIdx = 1 - irTapIdx.load(); ++irTapsVersion;
    ++targetVersion;
}

static std::mutex pendMutex; static std::shared_ptr<er::Target> pendTarget; static void* pendOwner = nullptr;

void ERProcessor::reanalyse()
{
    if (! erHasRaw()) return;
    joinWorker();
    lastIrOpts = { value ("irWin"), value ("irAuto"), value ("irThr"), value ("irWet") };
    auto L = erIrSlot() == 1 ? tIrRawL : irRawL, R = erIrSlot() == 1 ? tIrRawR : irRawR;      // (0.9.1) IR A or IR B
    const bool wetOnly = value ("irWet") > 0.5f, autoW = value ("irAuto") > 0.5f;
    const double win = value ("irWin"), thr = value ("irThr"); const int seed = (int) value ("vari");
    analysing = 1; analysisSlot = erIrSlot();
    worker = std::thread ([this, L = std::move (L), R = std::move (R), wetOnly, autoW, win, thr, seed]() mutable
    {
        auto T = std::make_shared<er::Target>();
        er::analyseTarget (*T, std::move (L), std::move (R), wetOnly, win, autoW, thr);
        if (T->valid && ! cancel)
        {
            T->fit = er::fitPattern (T->F, T->Wc, T->floorDb, seed, [this] { return cancel.load(); }, &T->fitD);
            T->fitted = ! cancel;
            er::applyFit (*T);
        }
        if (! cancel) { std::lock_guard<std::mutex> l (pendMutex); pendTarget = T; pendOwner = this; }
        analysing = 0;
    });
}

void ERProcessor::installTarget (std::shared_ptr<er::Target> t)
{
    requestTransition (false, true);
    { std::lock_guard<std::mutex> l (tMutex); tgt = t; tgtSlot = analysisSlot.load(); }
    if (t && t->valid)
    {
        { double e = 0; for (size_t i = 0; i < t->erL.size(); ++i) e += 0.5 * ((double) t->erL[i] * t->erL[i] + (double) t->erR[i] * t->erR[i]);
          convE = e > 1e-10 ? (float) (10 * std::log10 (e)) : -100.0f; }
        const int nx = 1 - irTapIdx.load(); irTaps[nx] = t->ir; irTapIdx = nx; ++irTapsVersion;
        juce::AudioBuffer<float> b (2, (int) t->erL.size());
        b.copyFrom (0, 0, t->erL.data(), (int) t->erL.size()); b.copyFrom (1, 0, t->erR.data(), (int) t->erR.size());
        conv.loadImpulseResponse (std::move (b), er::kSR, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        convReady = true;
        // the IR's own darkening goes to the control (the taps keep their base tone): only while IR is the source
        if ((int) value ("src") == 2) setParam ("darken", t->ir.darken);
    }
    ++targetVersion;
}

void ERProcessor::startFit()
{
    setParam ("autoLvl", 0);                                    // (0.7.1) a rebuild of an IR keeps the IR's own level: Auto Level off
    auto t = target(); if (! t || ! t->fitted) return;
    requestTransition (false);
    undo.beginNewTransaction();
    const auto& f = t->fit;
    setParam ("first", (float) f.p.first); setParam ("length", (float) f.p.length); setParam ("taps", (float) f.p.taps);
    setParam ("growth", (float) f.p.growth); setParam ("energy", (float) f.p.energy); setParam ("decay", (float) f.p.decay);
    setParam ("damp", (float) f.p.damp); setParam ("width", (float) (f.width * 100)); setParam ("predelay", (float) f.predelay);
    setParam ("size", 100); setParam ("diffusion", 0); setParam ("level", 0);
    setParam ("src", 1);
    currentPreset = -1; uiState.setProperty ("preset", -1, nullptr);
}

void ERProcessor::roomToPattern (bool keepEarly)
{
    if (converting.load()) return;
    if (r2pWorker.joinable()) r2pWorker.join();
    er::TapSet raw; std::vector<er::Img> sc; sc.reserve (1400); er::roomTaps (roomParams(), raw, sc);
    const int seed = (int) value ("vari");
    converting = keepEarly ? 2 : 1;
    r2pWorker = std::thread ([this, raw, seed, keepEarly]
    {
        er::Dist d; double Wc = 80; er::FitP f = er::fitToTaps (raw, seed, [] { return false; }, &d, &Wc);
        float early = 0;
        if (keepEarly)          // the room's first 20 / 30 / 40 ms: keep the length that matches best
            for (double e : { 20.0, 30.0, 40.0 })
            {
                er::Dist de; const auto fe = er::fitToTaps (raw, seed, [] { return false; }, &de, &Wc, e);
                if (early == 0 || de.score > d.score) { d = de; f = fe; early = (float) e; }
            }
        { std::lock_guard<std::mutex> l (r2pMutex); r2pFit = f; r2pResScore = d.score; r2pResKind = keepEarly ? 1 : 0; r2pEarly = early; r2pReady = true; }
    });
}

void ERProcessor::irToRoom()
{
    setParam ("autoLvl", 0);                                    // (0.7.1) a rebuild of an IR keeps the IR's own level: Auto Level off
    auto T = target(); if (! T || ! T->valid || converting.load()) return;
    if (r2pWorker.joinable()) r2pWorker.join();
    converting = 3;
    r2pWorker = std::thread ([this, T]
    {
        er::Dist d; const auto R = er::fitRoom (T->F, T->Wc, T->floorDb, [] { return false; }, &d);
        { std::lock_guard<std::mutex> l (r2pMutex); r2pRoom = R; r2pResScore = d.score; r2pResKind = 2; r2pReady = true; }
    });
}

thread_local const std::unordered_map<std::string, float>* ERProcessor::tlOverride = nullptr;
thread_local double ERProcessor::tlMaxLenS = 0;

// ---------------------------------------------------------------------------
//  0.7.0: the closed-loop fit, on its own thread: render (with its own values) -> measure -> compare -> move
// ---------------------------------------------------------------------------
// (the destructor stops a running fit first: the thread uses this object)
class ERProcessor::FitJob : public juce::Thread
{
public:
    FitJob (ERProcessor& p, fit::Metrics t, fit::Values v, double len) : juce::Thread ("RvLab fit"), vals (std::move (v)), lenS (len), proc (p), tgt (std::move (t)) {}
    ~FitJob() override { stopThread (4000); }
    std::atomic<float> progress { 0 }; std::atomic<bool> finished { false };
    fit::Values vals; double lenS = 0; double cost0 = 0, cost1 = 0; fit::Parts parts0, parts1; int evals = 0;
    double eval (fit::Parts* pp = nullptr)
    {
        ERProcessor::ExportOpts o; o.sr = fit::kSR; o.bits = 32; o.tail = true; o.normalize = false;
        juce::AudioBuffer<float> b; double g = 0; proc.renderIR (o, b, g); ++evals;
        progress = juce::jlimit (0.0f, 0.99f, evals / (float) kBudget);
        const auto m = fit::measure (b.getReadPointer (0), b.getReadPointer (1), b.getNumSamples());
        return fit::compare (tgt, m, pp);
    }
    void group (const std::vector<fit::P>& ps, int maxEval, int randomFirst, unsigned seed)
    {
        auto toX = [&] (const fit::P& p) { const double v = vals[p.id]; return p.lg ? (std::log (std::max (1e-6, v)) - std::log (p.lo)) / (std::log (p.hi) - std::log (p.lo)) : (v - p.lo) / (p.hi - p.lo); };
        auto put = [&] (const std::vector<double>& x) { for (size_t i = 0; i < ps.size(); ++i) { const auto& p = ps[i]; vals[p.id] = (float) (p.lg ? std::exp (std::log (p.lo) + x[i] * (std::log (p.hi) - std::log (p.lo))) : p.lo + x[i] * (p.hi - p.lo)); } };
        std::vector<double> x0; for (auto& p : ps) x0.push_back (juce::jlimit (0.0, 1.0, toX (p)));
        auto best = fit::nelderMead ([&] (const std::vector<double>& x) { put (x); return eval(); }, x0, maxEval, randomFirst, seed, [this] { return threadShouldExit(); });
        put (best);
    }
    void run() override
    {
        ERProcessor::tlOverride = &vals; ERProcessor::tlMaxLenS = lenS;
        cost0 = eval (&parts0);
        // order: the time shape first (it moves the decay), then the decay, ER, tone, width; a second, shorter pass; the decay last
        group (fit::groupTime(), 70, 16, 21);
        group (fit::groupDecay(), 45, 0, 11);
        group (fit::groupER(), 45, 10, 31);
        group (fit::groupTone(), 40, 0, 41);
        group (fit::groupStereo(), 20, 0, 51);
        group (fit::groupDecay(), 30, 0, 12);
        group (fit::groupTime(), 35, 0, 22);
        group (fit::groupER(), 20, 0, 32);
        group (fit::groupDecay(), 30, 0, 13);
        {   // the level: the shape is matched, the loudness is set to the plug-in's reference (an IR file's own level is arbitrary)
            ERProcessor::ExportOpts o; o.sr = fit::kSR; o.bits = 32; o.tail = true; juce::AudioBuffer<float> b; double g = 0; proc.renderIR (o, b, g);
            double e = 0; for (int c = 0; c < 2; ++c) for (int k = 0; k < b.getNumSamples(); ++k) e += 0.5 * (double) b.getSample (c, k) * b.getSample (c, k);
            const float d = (float) juce::jlimit (-30.0, 30.0, kRefWetDb - 10 * std::log10 (e + 1e-20));
            vals["level"] = juce::jlimit (-24.0f, 12.0f, vals["level"] + d); vals["tLvl"] = juce::jlimit (-36.0f, 12.0f, vals["tLvl"] + d);
        }
        cost1 = eval (&parts1);
        ERProcessor::tlOverride = nullptr; ERProcessor::tlMaxLenS = 0;
        progress = 1.0f; finished = true;
    }
    static constexpr int kBudget = 400;
private:
    ERProcessor& proc; fit::Metrics tgt;
};

void ERProcessor::stopFitJob() { if (fitJob) { fitJob->stopThread (60000); fitJob.reset(); } }

void ERProcessor::startBatch (const juce::Array<juce::File>& filesIn, const juce::String& suffix, bool applyOnly)
{
    if (batchRunning.load() || filesIn.isEmpty()) return;
    juce::Array<juce::File> files;                        // a true-stereo pair (*_L / *_R) counts once (the L file)
    for (auto& f : filesIn) if (! fit::isRightOfPair (f)) files.add (f);
    batchFiles = files; batchSuffix = suffix; batchTotal = files.size(); batchDone = 0; batchOk = 0; batchCancel = false; batchLog = {};
    batchApplyOnly = applyOnly; batchFitProgress = 0;
    if (! applyOnly)
    {
        juce::MemoryBlock mb; getStateInformation (mb);
        batchSaved = getXmlFromBinary (mb.getData(), (int) mb.getSize());
    }
    batchIrBefore = lastIRFile; batchState = 1; batchRunning = true;
}

void ERProcessor::runModSources (int n)
{
    const double dt = n / sr;
    double bpm = 120, ppq = -1; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    auto lfo = [&] (dm::Lfo& L, const char* rate, const char* shape, const char* sync)
    {
        const double beats = dm::syncBeats ((int) value (sync));
        if (beats > 0)
        {   // synced: the phase follows the song position while it plays
            if (playing && ppq >= 0) L.setPhase (ppq / beats - dt * bpm / 60.0 / beats);
            return L.step (dt, bpm / 60.0 / beats, (int) value (shape));
        }
        return L.step (dt, value (rate), (int) value (shape));
    };
    const float v1 = lfo (lfo1, "l1Rate", "l1Shape", "l1Sync"), v2 = lfo (lfo2, "l2Rate", "l2Shape", "l2Sync");
    modEnv.set (value ("enAtk"), value ("enRel"), sr);
    const auto* k = keyBuf.getReadPointer (0);
    for (int i = 0; i < n; ++i) modEnv.run (k[i]);
    const float ev = juce::jlimit (0.0f, 1.0f, (dm::g2db (modEnv.v) + value ("enSens") + 48.0f) / 48.0f);    // -48 dB .. 0 dB -> 0 .. 1
    mLfo1 = v1; mLfo2 = v2; mEnv = ev;
    for (auto& m : modV) m = 0;
    static const char* const ids[4][3] = { { "m1Src", "m1Dst", "m1Amt" }, { "m2Src", "m2Dst", "m2Amt" }, { "m3Src", "m3Dst", "m3Amt" }, { "m4Src", "m4Dst", "m4Amt" } };
    for (auto& sl : ids)
    {
        const int src = (int) value (sl[0]); if (src <= 0) continue;
        const float a = value (sl[2]) / 100.0f; if (a == 0) continue;
        const float x = src == 1 ? v1 : src == 2 ? v2 : ev;
        modV[juce::jlimit (0, dm::kDst - 1, (int) value (sl[1]))] += a * x;
    }
    for (int d = 0; d < dm::kDst; ++d) { modV[d] = juce::jlimit (-1.0f, 1.0f, modV[d]); mSum[(size_t) d] = modV[d]; }
    decayMod = std::pow (2.0, (double) modV[dm::dDecay]); trebleMod = std::pow (2.0, 1.5 * (double) modV[dm::dTreble]);
}

void ERProcessor::stepBatch()
{
    if (! batchRunning.load()) return;
    auto finish = [this]
    {
        batchRunning = false; batchState = 0; fitJob.reset(); batchFitProgress = 0;
        if (batchApplyOnly) return;
        if (batchIrBefore.existsAsFile()) loadIR (batchIrBefore); else clearIR();
        if (batchSaved != nullptr) { juce::MemoryBlock mb; copyXmlToBinary (*batchSaved, mb); setStateInformation (mb.getData(), (int) mb.getSize()); }
        batchSaved.reset();
    };
    if (batchCancel.load()) { batchLog << juce::String::fromUTF8 ("中止\n"); finish(); return; }
    if (batchState == 3)
    {   // the closed-loop fit is running: wait, then take its values and save
        batchFitProgress = fitJob ? fitJob->progress.load() : 1.0f;
        if (fitJob && ! fitJob->finished.load()) return;
        const auto f = batchFiles[batchDone.load()];
        if (fitJob)
        {
            for (auto* gp : { &fit::groupDecay, &fit::groupTime, &fit::groupER, &fit::groupTone, &fit::groupStereo })
                for (auto& p : (*gp)()) setParam (p.id, fitJob->vals[p.id]);
            setParam ("autoLvl", 0);
            const auto& a = fitJob->parts0; const auto& b = fitJob->parts1;
            batchLog << fit::pairName (f) << juce::String::fromUTF8 ("：誤差 ") << juce::String (fitJob->cost0, 0) << " -> " << juce::String (fitJob->cost1, 1)
                     << juce::String::fromUTF8 ("（T30 ") << juce::String (a.t30 * 5, 1) << " -> " << juce::String (b.t30 * 5, 1)
                     << juce::String::fromUTF8 (" %、EDT ") << juce::String (a.edt * 8, 1) << " -> " << juce::String (b.edt * 8, 1)
                     << juce::String::fromUTF8 (" %、C80 ") << juce::String (a.c80 * 1.5, 1) << " -> " << juce::String (b.c80 * 1.5, 1) << " dB, "
                     << fitJob->evals << juce::String::fromUTF8 (" 回）\n");
            lastFitReport = batchLog;
        }
        fitJob.reset();
        if (batchApplyOnly) { ++batchOk; ++batchDone; batchState = 1; return; }
        const auto name = fit::pairName (f) + batchSuffix;
        const auto saved = saveUserPreset (name);
        if (saved.existsAsFile()) ++batchOk; else batchLog << name << juce::String::fromUTF8 (" : 保存できませんでした\n");
        ++batchDone; batchState = 1;
        return;
    }
    if (batchState == 1)
    {   // load the next IR
        if (batchDone.load() >= batchFiles.size()) { finish(); return; }
        const auto f = batchFiles[batchDone.load()];
        batchTgtSeen = targetVersion.load(); batchIrtSeen = irtVersion.load();
        if (const auto err = loadIR (f); err.isNotEmpty()) { batchLog << f.getFileName() << " : " << err << "\n"; ++batchDone; return; }
        setParam ("tSrc", 1);                                   // the IR tail is built from it (it is measured for ALGO)
        batchState = 2; batchWait = 0;
        return;
    }
    if (batchState == 2)
    {   // wait for the analysis (ER fit) and the IR tail
        const auto f = batchFiles[batchDone.load()];
        auto T = target();
        const bool ready = analysing.load() == 0 && targetVersion.load() != batchTgtSeen && T && T->valid && T->fitted && irtVersion.load() != batchIrtSeen && tailIrReady.load();
        if (! ready)
        {
            if (++batchWait > 400 || (analysing.load() == 0 && targetVersion.load() != batchTgtSeen && (! T || ! T->valid)))
            { batchLog << f.getFileName() << juce::String::fromUTF8 (" : 解析できませんでした\n"); ++batchDone; batchState = 1; }
            return;
        }
        startFit();                                             // ER -> ALGO (+ COLOR stays neutral)
        const auto rep = irToFdn();                             // TAIL -> ALGO (decay per band, size, level)
        setParam ("src", 1); setParam ("tSrc", 0); setParam ("tRoom", 0); setParam ("tEng", 0);
        juce::ignoreUnused (rep);
        // then the closed loop: render, measure, compare with the IR itself (and its true-stereo partner), move
        std::vector<float> tL, tR;
        if (fit::loadTarget (f, fit::pairOf (f), tL, tR))
        {
            auto tm = fit::measure (tL.data(), tR.data(), (int) tL.size());
            fit::Values v;
            for (auto* pr : getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (pr)) v[rp->getParameterID().toStdString()] = rp->convertFrom0to1 (rp->getValue());
            v["tSpread"] = (float) juce::jlimit (5.0, 250.0, tm.peakMs * 0.8); v["tShape"] = 40.0f; v["tBassW"] = 100.0f;
            fitJob = std::make_unique<FitJob> (*this, std::move (tm), std::move (v), juce::jlimit (0.5, 8.0, tL.size() / fit::kSR + 0.2));
            fitJob->startThread (juce::Thread::Priority::low);
            batchState = 3; return;
        }
        batchLog << f.getFileName() << juce::String::fromUTF8 (" : 読めないので一括作成の結果のまま\n");
        if (batchApplyOnly) { ++batchOk; ++batchDone; batchState = 1; return; }
        const auto name = fit::pairName (f) + batchSuffix;
        if (saveUserPreset (name).existsAsFile()) ++batchOk;
        ++batchDone; batchState = 1;
    }
}

// ---------------------------------------------------------------------------
//  (0.9.1) the source cards' destinations. One source per destination; a lit button pressed again goes back to ALGO
// ---------------------------------------------------------------------------
bool ERProcessor::erLit (int card) const
{
    const int es = (int) value ("src");
    return card < 2 ? es == 0 && (int) value ("erRoomSel") == card : es == 2 && erIrSlot() == card - 2;
}
bool ERProcessor::tailLit (int card) const
{
    const int ts = (int) value ("tSrc"), bd = (int) value ("tBody");
    if (card < 2) return (ts == 0 && value ("tRoom") > 0.5f && (int) value ("tRoomSel") == card) || (ts == 2 && bd == 1 && (int) value ("tRoomSel") == card);
    const int s = card - 2;
    return (ts == 1 && tailIrSlot() == s) || (ts == 2 && bd == 2 && tailIrSlot() == 1 - s);
}
void ERProcessor::routeSend (int card, int what)
{
    const int ts = (int) value ("tSrc"), bd = (int) value ("tBody"); const bool room = card < 2; const int s = room ? card : card - 2;
    if (! cardLoaded (card)) return;
    auto set = [this] (const char* id, int v) { setParam (id, (float) v); };
    if (what == 0)
    {
        if (erLit (card)) set ("src", 1);
        else if (room) { set ("erRoomSel", s); set ("src", 0); }
        else { set ("erIrSel", s); set ("src", 2); }
    }
    else if (what == 1)
    {
        if (tailLit (card))
        {
            if (ts == 2) set ("tBody", 0); else { set ("tSrc", 0); set ("tRoom", 0); }
        }
        else if (room) { set ("tRoomSel", s); if (ts == 2) set ("tBody", 1); else { set ("tSrc", 0); set ("tRoom", 1); } }
        else
        {
            if (ts == 2 && tailIrSlot() != s) set ("tBody", 2);                         // the other IR is the head: this one is the body
            else { set ("tIrSel", s); set ("tSrc", 1); set ("tBody", 0); }              // (a head on the same IR: the head goes, this IR plays whole)
        }
    }
    else if (! room)
    {
        if (headLit (s))
        {   // the head off: the body plays whole
            if (bd == 2) { set ("tIrSel", 1 - s); set ("tSrc", 1); set ("tBody", 0); }
            else { set ("tSrc", 0); set ("tRoom", bd == 1 ? 1 : 0); set ("tBody", 0); }
        }
        else
        {
            int body = 0;
            if (ts == 0) body = value ("tRoom") > 0.5f ? 1 : 0;
            else if (ts == 1) body = tailIrSlot() == s ? 0 : (cardLoaded (2 + (1 - s)) ? 2 : 0);
            else body = bd;
            if (body == 2 && ! (cardLoaded (2) && cardLoaded (3))) body = 0;
            set ("tIrSel", s); set ("tBody", body); set ("tSrc", 2);
        }
    }
}

// ---------------------------------------------------------------------------
//  (0.9.1) "-> ALGO" on a source card: what the lit destinations (ER / TAIL) use is written as ALGO parameters
// ---------------------------------------------------------------------------
void ERProcessor::convertRoom (int slot, bool toEr, bool toTail)
{
    if (! toEr && ! toTail) toEr = toTail = true;
    if (toTail)
    {
        const int ts = (int) value ("tSrc");
        if (ts == 2) setParam ("tBody", 1); else { setParam ("tSrc", 0); setParam ("tRoom", 1); }      // so the curve read is the room's
        setParam ("tRoomSel", (float) slot);
        roomToAlgo();                                                   // (Decay, the five points and Size; tRoom -> 0)
        if (ts == 2) setParam ("tBody", 0);
    }
    if (toEr) { setParam ("erRoomSel", (float) slot); roomToPattern (false); }          // (background; writes ALGO and switches when done)
}
void ERProcessor::convertIr (int slot, bool toEr, bool toTail)
{
    if (! toEr && ! toTail) toEr = toTail = true;
    if (slot == 1 ? ! tailIrOwn() : irRawL.empty()) return;
    pendTries = 0;
    if (toEr)
    {
        const auto T = target();
        if (erIrSlot() == slot && T && T->valid && T->fitted && analysing.load() == 0) startFit();
        else { pendErFit = true; pendTgtSeen = targetVersion.load(); if (erIrSlot() != slot) setParam ("erIrSel", (float) slot); }
    }
    if (toTail)
    {   // the whole IR of that slot as the tail (it is measured for ALGO), rebuilt now
        setParam ("tIrSel", (float) slot); setParam ("tSrc", 1); setParam ("tBody", 0);
        pendTailFit = true; irtSeen = -1; irtHash = -2;
    }
}
void ERProcessor::servicePending()
{
    if (! pendErFit && ! pendTailFit) return;
    if (++pendTries > 600) { pendErFit = pendTailFit = false; return; }
    if (pendErFit)
    {
        const auto T = target();
        if (analysing.load() == 0 && targetVersion.load() != pendTgtSeen)
        {
            if (T && T->valid && T->fitted) startFit();
            pendErFit = false;
        }
    }
    if (pendTailFit && ! pendErFit && irtSeen == irtHash && irtVersion.load() != irtVerLaunch && tailIrReady.load())
    {
        irToFdn(); pendTailFit = false;
    }
}

void ERProcessor::timerCallback()
{
    stepBatch();
    {   // (0.9.1) the ER's IR follows its slot (IR A / IR B) and the slot's file
        const int k = erIrSlot() + 2 * (erIrSlot() == 1 ? tIrVersion.load() : 0);
        if (k != lastErKey)
        {
            const bool first = lastErKey < 0; lastErKey = k;
            if (! first)
            {
                if (erHasRaw()) reanalyse();
                else { joinWorker(); { std::lock_guard<std::mutex> l (tMutex); tgt.reset(); } convReady = false; ++targetVersion; }
            }
        }
    }
    servicePending();
    {   // the IR tail follows the IR and its settings (built in the background once they settle ~0.2 s)
        double h = targetVersion.load() * 7.31;
        for (auto* id : { "irtX", "irtLen", "irtSt", "irtLvl", "irtDL", "irtDM", "irtDH", "irtRev", "irtB", "tSrc", "irtAuto", "tEng", "tBody", "erIrSel" }) h = h * 1.000123 + value (id);
        h += tIrVersion.load() * 3.77; h += value ("tIrSel") * 17.3;
        if (h != irtHash) { irtHash = h; irtStable = 0; }
        else if (++irtStable >= 2 && irtSeen != h) { irtSeen = h; buildTailIR(); }
    }
    {   // measure the reverb again once the settings have stopped moving for ~0.4 s
        double h = 0; int k = 0;
        for (auto* p : getParameters()) h += (double) p->getValue() * (1.0 + 0.0137 * (k++ % 97));
        if (h != measHash) { measHash = h; measStable = 0; }
        else if (++measStable >= 4 && measSeen != h && ! measuring.load()) { measSeen = h; startMeasure(); }
    }
    {   // switching to IR with Darkening still at 0: put the IR's own darkening there
        const int sNow = (int) value ("src");
        if (sNow == 2 && lastSrcSeen != 2 && value ("darken") < 0.001f)
            if (auto T = target(); T && T->valid && T->ir.darken > 0) setParam ("darken", T->ir.darken);
        lastSrcSeen = sNow;
    }
    {   // ROOM -> ALGO finished: write the ALGO parameters (one undo step) and switch
        bool ready = false; er::FitP f; er::RoomP R; int sc = 0, kind = 0; float early = 0;
        { std::lock_guard<std::mutex> l (r2pMutex); if (r2pReady) { ready = true; f = r2pFit; R = r2pRoom; sc = r2pResScore; kind = r2pResKind; early = r2pEarly; r2pReady = false; } }
        if (ready && kind == 2)
        {
            if (r2pWorker.joinable()) r2pWorker.join();
            requestTransition (false);
            apvts.copyState();                       // flushes earlier changes into their own undo step first
            undo.beginNewTransaction();
            setParam ("shape", 0); setParam ("roomW", (float) R.W); setParam ("roomD", (float) R.D); setParam ("roomH", (float) R.H);
            setParam ("wall", (float) R.wall); setParam ("floor", (float) R.floor); setParam ("order", (float) R.order);
            setParam ("srcX", (float) R.sx); setParam ("srcY", (float) R.sy); setParam ("lisX", (float) R.lx); setParam ("lisY", (float) R.ly);
            setParam ("src", 0);
            currentPreset = -1; uiState.setProperty ("preset", -1, nullptr); uiState.setProperty ("unit", -1, nullptr);
            apvts.copyState();                       // and these into this one
            r2pScore = sc; r2pKind = 2; converting = 0; ++r2pDone;
            ready = false;
        }
        if (ready)
        {
            if (r2pWorker.joinable()) r2pWorker.join();
            requestTransition (false);
            apvts.copyState();                       // flushes earlier changes into their own undo step first
            undo.beginNewTransaction();
            setParam ("roomEarly", kind == 1 ? early : 0.0f);
            setParam ("first", (float) f.p.first); setParam ("length", (float) f.p.length); setParam ("taps", (float) f.p.taps);
            setParam ("growth", (float) f.p.growth); setParam ("energy", (float) f.p.energy); setParam ("decay", (float) f.p.decay);
            setParam ("damp", (float) f.p.damp); setParam ("vari", (float) f.p.seed);
            setParam ("src", 1);
            currentPreset = -1; uiState.setProperty ("preset", -1, nullptr); uiState.setProperty ("unit", -1, nullptr);
            apvts.copyState();
            r2pScore = sc; r2pKind = kind; converting = 0; ++r2pDone;
        }
    }
    std::shared_ptr<er::Target> got;
    { std::lock_guard<std::mutex> l (pendMutex); if (pendOwner == this && pendTarget) { got = pendTarget; pendTarget.reset(); pendOwner = nullptr; } }
    if (got) { if (worker.joinable()) worker.join(); installTarget (got); }
    if (erHasRaw() && analysing.load() == 0)
    {
        const std::array<float, 4> o { value ("irWin"), value ("irAuto"), value ("irThr"), value ("irWet") };
        if (o != lastIrOpts) reanalyse();
    }
}

// ---------------------------------------------------------------------------
void ERProcessor::erToFull (double fs, std::vector<float>& L, std::vector<float>& R) const
{
    const float g = value ("erOn") > 0.5f ? erBalG() : 0.0f;
    rv::InputFilter f; setInputFilter (f, fs); const bool filt = (int) value ("iTo") != 1;
    for (size_t i = 0; i < L.size(); ++i) { L[i] = (filt ? f.run (L[i], 0) : L[i]) * g; R[i] = (filt ? f.run (R[i], 1) : R[i]) * g; }
}
void ERProcessor::renderPreview (Preview& out, double lengthMs)
{
    renderPreviewER (out, lengthMs);
    const int N = (int) out.L.size();
    out.fullL = out.L; out.fullR = out.R;
    erToFull (er::kSR, out.fullL, out.fullR);
    if (! previewFdnReady) { previewFdn.prepare (er::kSR); previewFdnReady = true; }
    // CPU: the tail is rendered again only when something that shapes it moved (an ER-only change reuses it), and at most
    // ~6 times a second while it is being dragged (the last state is always rendered: previewTailStale tells the editor)
    double h = N * 1.31 + irtVersion.load() * 7.7 + tIrVersion.load() * 3.1;
    {
        const bool roomTail = value ("tRoom") > 0.5f || tailFromRoom(); int k = 0;
        for (auto* pr : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (pr))
            {
                ++k; const auto sec = sectionOf (rp->getParameterID());
                if (sec == "ER" && ! roomTail && rp->getParameterID() != "predelay" && rp->getParameterID() != "bw" && rp->getParameterID() != "bits") continue;
                if (sec == "EQ") continue;
                h += (double) rp->getValue() * (1.0 + 0.0137 * (k % 97));
            }
    }
    const double now = juce::Time::getMillisecondCounterHiRes();
    previewTailStale = false;
    if (h != pvTailHash || (int) pvTailL.size() != N)
    {
        if ((int) pvTailL.size() == N && now - pvTailAt < 160) previewTailStale = true;     // keep the last one for now
        else { renderTail (er::kSR, N, pvTailL, pvTailR, previewFdn); pvTailHash = h; pvTailAt = now; }
    }
    for (int i = 0; i < N; ++i) { out.fullL[(size_t) i] += pvTailL[(size_t) i]; out.fullR[(size_t) i] += pvTailR[(size_t) i]; }
    finishWet (er::kSR, out.fullL, out.fullR);
}
void ERProcessor::renderPreviewER (Preview& out, double lengthMs) const
{
    auto T = target();
    const int src = (int) value ("src");
    const bool convM = src == 2 && (int) value ("irMode") == 1;
    out.taps.n = 0; out.conv = convM; out.src = src; out.lengthMs = lengthMs;
    if (! convM) sourceTaps (out.taps, T.get());
    auto s = settings(); s.tail = false; s.levelDb += autoComp.load();
    er::renderPreview (convM ? nullptr : &out.taps, convM && T ? &T->erL : nullptr, convM && T ? &T->erR : nullptr, value ("predelay"), s, lengthMs, out.L, out.R);
    {   // the ER's own features (for the "what you hear now" notes)
        double tEnd = 0; for (int i = 0; i < out.taps.n; ++i) tEnd = std::max (tEnd, (double) std::max (out.taps.t[(size_t) i].tL, out.taps.t[(size_t) i].tR));
        if (convM && T) tEnd = value ("predelay") + T->Wc;
        out.selfWc = juce::jlimit (20.0, 300.0, tEnd + 5);
        er::Sig S; S.L = out.L; S.R = out.R;
        const size_t need = (size_t) ((out.selfWc + 20) * er::kK);
        if (S.L.size() < need) { S.L.resize (need, 0.0f); S.R.resize (need, 0.0f); }
        S.L[0] += 1.0f; S.R[0] += 1.0f; S.prep();
        out.self = er::features (S, 0, out.selfWc, -60, true);
    }
    ++out.serial;
    out.hasFeat = false;
    if (T && T->valid)
    {
        // the match leaves out Auto Level: it is a listening convenience, the comparison is at the source's own level
        er::Sig S; S.L = out.L; S.R = out.R;
        const float unComp = (float) er::db2g (-autoComp.load());
        for (auto& v : S.L) v *= unComp; for (auto& v : S.R) v *= unComp;
        const size_t need = (size_t) ((T->Wc + 20) * er::kK);
        if (S.L.size() < need) { S.L.resize (need, 0.0f); S.R.resize (need, 0.0f); }
        S.L[0] += 1.0f; S.R[0] += 1.0f; S.prep();
        out.feat = er::features (S, 0, T->Wc, T->floorDb, true);
        out.d = er::dist (T->F, out.feat); out.hasFeat = true;
    }
}

// ---------------------------------------------------------------------------
//  IR export: an impulse through the same engine, offline, at any sample rate
// ---------------------------------------------------------------------------
void ERProcessor::renderIR (const ExportOpts& o, juce::AudioBuffer<float>& out, double& gainDb)
{
    const double fs = o.sr;
    auto T = target();
    const int src = (int) value ("src");
    const bool convM = src == 2 && (int) value ("irMode") == 1;
    auto s = settings(); s.tail = false; s.convWidth = convM; s.levelDb += tlOverride != nullptr ? 0.0f : autoComp.load();
    const double pre = value ("predelay");
    double rmax = 0; for (auto r : rtCurve().rt) rmax = std::max (rmax, r);
    const bool withTail = o.tail && value ("tOn") > 0.5f;

    auto eng = std::make_unique<er::TapEngine>(); eng->prepare (fs); eng->setWander (s.wander);
    double tEnd = pre;
    if (! convM)
    {
        er::TapSet sh; sourceTaps (sh, T.get());
        eng->setTaps (sh, true);
        for (int i = 0; i < sh.n; ++i) tEnd = std::max (tEnd, (double) std::max (sh.t[(size_t) i].tL, sh.t[(size_t) i].tR));
    }
    // conv mode: the ER window resampled to fs (an impulse in = the IR itself)
    std::vector<float> cL, cR;
    if (convM && T && T->valid)
    {
        const double ratio = er::kSR / fs; const size_t n = (size_t) std::floor ((double) (T->erL.size() - 1) / ratio);
        cL.resize (n); cR.resize (n);
        for (size_t i = 0; i < n; ++i)
        {
            const double x = (double) i * ratio; const size_t a = (size_t) x; const float f = (float) (x - (double) a);
            cL[i] = T->erL[a] + f * (T->erL[a + 1] - T->erL[a]); cR[i] = T->erR[a] + f * (T->erR[a + 1] - T->erR[a]);
        }
        tEnd = pre + 1000.0 * (double) n / fs;
    }
    er::Post post2; post2.prepare (fs); post2.set (s); post2.reset();
    const double lenS = std::min (tlMaxLenS > 0 ? tlMaxLenS : 15.0, std::max (tEnd / 1000.0 + 0.15, withTail ? (pre + value ("tPre")) / 1000.0 + rmax * 1.4 : 0.0));
    const int N = (int) (lenS * fs);
    out.setSize (2, N); out.clear();
    const int B = 512; std::vector<float> in ((size_t) B), wL ((size_t) B), wR ((size_t) B), tL ((size_t) B), tR ((size_t) B);
    const size_t off = (size_t) std::lround (pre * fs / 1000.0);
    for (int p0 = 0; p0 < N; p0 += B)
    {
        const int n = std::min (B, N - p0);
        std::fill (in.begin(), in.end(), 0.0f); if (p0 == 0) in[0] = 1.0f;
        std::fill (wL.begin(), wL.end(), 0.0f); std::fill (wR.begin(), wR.end(), 0.0f);
        eng->process (in.data(), wL.data(), wR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const long k = (long) (p0 + i) - (long) off;
            if (k >= 0 && (size_t) k < cL.size()) { wL[(size_t) i] += cL[(size_t) k]; wR[(size_t) i] += cR[(size_t) k]; }
        }
        post2.process (wL.data(), wR.data(), n);
        out.copyFrom (0, p0, wL.data(), n); out.copyFrom (1, p0, wR.data(), n);
    }
    {   // the reverb: ER (on / off, input filter) + tail, then master EQ and output width
        std::vector<float> L (out.getReadPointer (0), out.getReadPointer (0) + N), R (out.getReadPointer (1), out.getReadPointer (1) + N);
        erToFull (fs, L, R);
        if (withTail) { rv::Fdn f; std::vector<float> tL, tR; renderTail (fs, N, tL, tR, f); for (int i = 0; i < N; ++i) { L[(size_t) i] += tL[(size_t) i]; R[(size_t) i] += tR[(size_t) i]; } }
        finishWet (fs, L, R);
        out.copyFrom (0, 0, L.data(), N); out.copyFrom (1, 0, R.data(), N);
    }
    if (o.direct) { out.addSample (0, 0, 1.0f); out.addSample (1, 0, 1.0f); }
    // trim below -100 dB, 10 ms fade out
    int last = 0;
    for (int c = 0; c < 2; ++c) { auto* d = out.getReadPointer (c); for (int i = N - 1; i > last; --i) if (std::abs (d[i]) > 1e-5f) { last = i; break; } }
    const int len = std::min (N, last + (int) (0.01 * fs) + 1), fade = std::min (len, (int) (0.01 * fs));
    out.setSize (2, std::max (1, len), true);
    for (int c = 0; c < 2; ++c) out.applyGainRamp (c, len - fade, fade, 1.0f, 0.0f);
    // fixed-point files: keep 0.3 dB of headroom instead of clipping
    gainDb = 0;
    const float pk = out.getMagnitude (0, out.getNumSamples());
    if (o.normalize && pk > 1e-9f) { const float g = 0.891f / pk; out.applyGain (g); gainDb = 20 * std::log10 (g); }      // peak -1 dBFS
    else if (o.bits < 32 && pk > 0.966f) { const float g = 0.966f / pk; out.applyGain (g); gainDb = 20 * std::log10 (g); }
}

juce::String ERProcessor::exportIR (const juce::File& f, const ExportOpts& o, bool& ok)
{
    ok = false;
    juce::AudioBuffer<float> b; double gainDb = 0;
    renderIR (o, b, gainDb);
    if (! o.normalize) b.applyGain (kWetCal);                  // as heard (the wet bus calibration)
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return "Cannot write " + f.getFullPathName();
    juce::WavAudioFormat wav;
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (o.sr).withNumChannels (2).withBitsPerSample (o.bits)
                    .withSampleFormat (o.bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint : juce::AudioFormatWriterOptions::SampleFormat::integral);
    auto w = wav.createWriterFor (os, opts);
    if (w == nullptr) return "WAV writer failed";
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    w.reset();
    ok = true;
    const float pk = b.getMagnitude (0, b.getNumSamples());
    juce::String r = f.getFileName() + "  |  " + juce::String (b.getNumSamples() / o.sr, 2) + " s, " + juce::String (o.sr / 1000.0, 1) + " kHz, "
                     + (o.bits == 32 ? "32-bit float" : juce::String (o.bits) + "-bit") + ", peak " + juce::String (20 * std::log10 (pk + 1e-9f), 1) + " dBFS";
    if (o.normalize) r << "  (normalised " << (gainDb > 0 ? "+" : "") << juce::String (gainDb, 1) << " dB)";
    else if (gainDb < -0.01) r << "  (scaled " << juce::String (gainDb, 1) << " dB to avoid clipping)";
    return r;
}

// ---------------------------------------------------------------------------
void ERProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    uiState.setProperty ("autoComp", autoComp.load(), nullptr);
    auto st = earSnap.isValid() ? earSnap.createCopy() : apvts.copyState();     // during ear training: the settings from before it
    st.removeChild (st.getChildWithName ("ui"), nullptr);
    st.appendChild (uiState.createCopy(), nullptr);
    if (! irRawL.empty())
    {
        juce::ValueTree ir ("IR");
        ir.setProperty ("name", irName, nullptr);
        juce::MemoryBlock mL (irRawL.data(), irRawL.size() * sizeof (float)), mR (irRawR.data(), irRawR.size() * sizeof (float));
        ir.setProperty ("L", mL.toBase64Encoding(), nullptr); ir.setProperty ("R", mR.toBase64Encoding(), nullptr);
        st.removeChild (st.getChildWithName ("IR"), nullptr);
        st.appendChild (ir, nullptr);
    }
    if (! tIrRawL.empty())
    {   // the tail's own IR
        juce::ValueTree ir ("TIR");
        ir.setProperty ("name", tIrName, nullptr);
        juce::MemoryBlock mL (tIrRawL.data(), tIrRawL.size() * sizeof (float)), mR (tIrRawR.data(), tIrRawR.size() * sizeof (float));
        ir.setProperty ("L", mL.toBase64Encoding(), nullptr); ir.setProperty ("R", mR.toBase64Encoding(), nullptr);
        st.removeChild (st.getChildWithName ("TIR"), nullptr);
        st.appendChild (ir, nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void ERProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size); if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml); if (! st.isValid()) return;
    if (auto u = st.getChildWithName ("ui"); u.isValid()) uiState.copyPropertiesFrom (u, nullptr);
    auto ir = st.getChildWithName ("IR"), tir = st.getChildWithName ("TIR");
    st.removeChild (st.getChildWithName ("ui"), nullptr); st.removeChild (ir, nullptr); st.removeChild (tir, nullptr);
    if (tir.isValid())
    {
        juce::MemoryBlock mL, mR;
        if (mL.fromBase64Encoding (tir["L"].toString()) && mR.fromBase64Encoding (tir["R"].toString()) && mL.getSize() == mR.getSize() && mL.getSize() >= 256)
        {
            const size_t n = mL.getSize() / sizeof (float);
            tIrRawL.assign ((const float*) mL.getData(), (const float*) mL.getData() + n);
            tIrRawR.assign ((const float*) mR.getData(), (const float*) mR.getData() + n);
            tIrName = tir["name"].toString(); ++tIrVersion;
        }
    }
    apvts.replaceState (st);
    currentPreset = (int) uiState.getProperty ("preset", -1);
    autoComp = (float) uiState.getProperty ("autoComp", 0.0f);
    irList.clear();
    { juce::StringArray sa; sa.addLines (uiState.getProperty ("irList", "").toString()); for (auto& p : sa) if (p.isNotEmpty()) irList.add (juce::File (p)); }
    irListIndex = (int) uiState.getProperty ("irIndex", -1);
    irListB.clear();
    { juce::StringArray sa; sa.addLines (uiState.getProperty ("irListB", "").toString()); for (auto& p : sa) if (p.isNotEmpty()) irListB.add (juce::File (p)); }
    irListIndexB = (int) uiState.getProperty ("irIndexB", -1);
    if (ir.isValid())
    {
        juce::MemoryBlock mL, mR;
        if (mL.fromBase64Encoding (ir["L"].toString()) && mR.fromBase64Encoding (ir["R"].toString()) && mL.getSize() == mR.getSize() && mL.getSize() >= 256)
        {
            const size_t n = mL.getSize() / sizeof (float);
            irRawL.assign ((const float*) mL.getData(), (const float*) mL.getData() + n);
            irRawR.assign ((const float*) mR.getData(), (const float*) mR.getData() + n);
            irName = ir["name"].toString();
            reanalyse();
        }
    }
}

juce::AudioProcessorEditor* ERProcessor::createEditor() { return new EditorShell (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ERProcessor(); }
