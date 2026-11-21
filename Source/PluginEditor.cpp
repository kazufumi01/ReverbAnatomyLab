#include "PluginEditor.h"
#include "Glossary.h"

namespace
{
// (0.10.12) the look adjustments: three knobs (text weight, brightness, contrast), shown from the header's button
struct LookPanel : juce::Component
{
    std::function<void()> onChange;
    juce::Slider s[3]; juce::Label l[3]; juce::TextButton reset;
    LookPanel (bool ja)
    {
        const char* jn[3] = { "文字の濃さ", "明るさ", "コントラスト" }; const char* en[3] = { "Text", "Brightness", "Contrast" };
        float* v[3] = { &theme::adjText, &theme::adjBright, &theme::adjContrast };
        for (int i = 0; i < 3; ++i)
        {
            s[i].setSliderStyle (juce::Slider::RotaryVerticalDrag); s[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
            s[i].setRange (-1.0, 1.0, 0.01); s[i].setValue (*v[i], juce::dontSendNotification); s[i].setDoubleClickReturnValue (true, 0.0);
            s[i].textFromValueFunction = [] (double x) { const int q = juce::roundToInt (x * 100); return (q > 0 ? juce::String ("+") : juce::String()) + juce::String (q); };
            s[i].onValueChange = [this, i, v] { *v[i] = (float) s[i].getValue(); if (onChange) onChange(); };
            addAndMakeVisible (s[i]);
            l[i].setText (juce::String::fromUTF8 (ja ? jn[i] : en[i]), juce::dontSendNotification); l[i].setJustificationType (juce::Justification::centred);
            l[i].setFont (theme::ui (12.0f)); addAndMakeVisible (l[i]);
        }
        reset.setButtonText (ja ? juce::String::fromUTF8 ("戻す") : juce::String ("Reset"));
        reset.onClick = [this] { for (auto& k : s) k.setValue (0.0, juce::sendNotificationSync); };
        addAndMakeVisible (reset);
        setSize (3 * 92 + 16, 150);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto bottom = r.removeFromBottom (24); reset.setBounds (bottom.withSizeKeepingCentre (70, 22));
        for (int i = 0; i < 3; ++i) { auto c = r.removeFromLeft (92); l[i].setBounds (c.removeFromTop (18)); s[i].setBounds (c); }
    }
};

struct HelpText { const char* id; const char* t; const char* ja; const char* en; };
const HelpText kHelp[] = {
    { "tFill", "Onset Fill", "Hall エンジンの立ち上がりを埋める量。遅延線の途中からも出力を取り出すので、テールが鳴り始めから密になり、ER が終わってからテールが育つまでの「くぼみ」がなくなります。減衰の長さは変わりません。0 % ＝従来どおり（線の端だけ）。エコーグラムで ER の終わりとテールの間を見ながら上げます。",
      "Fills the Hall engine's onset. Outputs are also taken part-way along the delay lines, so the tail is dense from its first moment and the dip between the end of the ER and the growing tail disappears. The decay stays the same. 0 % = as before (line ends only). Raise it while watching the gap after the ER in the echogram." },
    { "tSpreadHi", "Spread High", "Spread（立ち上がりの長さ）を高域（約 1.5 kHz 以上）だけ何倍にするか。100 % ＝低域と同じ。ホールや教会では高域は早く立ち上がり低域は遅れて膨らむので、100 % より小さくすると実際に近づきます。逆に大きくすると高域がゆっくり膨らむ暗い立ち上がり。Spread が 0 のときは効きません。",
      "How much longer (or shorter) the build-up is for the highs (above about 1.5 kHz), as a % of Spread. 100 % = the same as the lows. In halls and churches the highs arrive early and the lows swell later, so below 100 % is closer to real rooms; above 100 % the highs swell slowly (a darker onset). No effect with Spread at 0." },
    { "tSpread", "Spread", "テールの立ち上がり（ビルドアップ）の長さ。入力をこの時間に散らしてから残響に入れます。0 でオフ。往年のデジタルホールの「ふわっと膨らむ」立ち上がりはここ（80〜200 ms）。EDT（初期の減衰）が長くなり、C80（明瞭度）が下がります。", "The tail's build-up time: the input is spread over this long before it enters the reverb. 0 = off. The swelling onset of the classic digital halls lives here (80-200 ms). It lengthens the EDT and lowers the C80." },
    { "tShape", "Shape", "Spread の間の形。＋＝後ろほど大きい（ゆっくり膨らむ）、0＝平ら、−＝先が大きい（すぐ鳴ってから減る）。", "The envelope over the spread: + = louder towards the end (a slow swell), 0 = flat, - = loud first, then less." },
    { "tBassW", "Bass Width", "テールの 250 Hz 以下の左右の広がり。実際の部屋は低域ほど左右がそろいます（100 % ＝そのまま、0 % ＝低域はモノ）。低音の締まりとモノ互換にも効きます。", "The tail's width below 250 Hz. Real rooms are more alike left and right in the lows (100 % = as is, 0 % = mono lows). It also tightens the low end and helps mono." },
    // 0.6.0: DYN
    { "dkMode", "Ducking", "ドライ（入力）が鳴っている間だけリバーブを下げます。Single＝全体を一緒に、Multi＝2〜5 帯域ごと、Spectral＝半オクターブ 17 帯域ごと（ドライと重なる帯域だけ下がるので、残響の量感を保ったまま声の明瞭さが上がる）。", "Lowers the reverb while the dry (input) plays. Single: all together; Multi: per band (2-5); Spectral: per half-octave (17 bands; only where the dry overlaps, so the reverb keeps its size while the vocal stays clear)." },
    { "dkDepth", "Duck Depth", "いちばん下げる量（dB）。ドライが閾値より 12 dB 以上大きいとき、この量まで下がります。6〜10 dB が自然、15 dB 以上は効果として聞こえます。", "The most it turns down (dB): reached when the dry is 12 dB or more above the threshold. 6-10 dB sounds natural; 15 dB and more is heard as an effect." },
    { "dkThr", "Duck Threshold", "ここを超えたドライで下がり始めます。下げるほど小さな音でも反応します。", "The dry above this starts the ducking. Lower = it reacts to quieter playing." },
    { "dkAtk", "Duck Attack", "ドライが来てから下がりきるまでの速さ。短い＝すぐ引っ込む、長い＝頭の残響が少し残る。", "How fast it goes down once the dry comes in. Short: steps back at once; long: a little reverb stays on the attacks." },
    { "dkRel", "Duck Release", "ドライが止んでから残響が戻る速さ。テンポに合わせると自然（例：120 BPM の 8 分＝250 ms）。", "How fast the reverb comes back once the dry stops. Matching the tempo sounds natural (an eighth at 120 BPM = 250 ms)." },
    { "dkBands", "Duck Bands", "Multi の帯域数（2〜5）。クロスオーバーは 800 Hz（2）／250・2.5k（3）／200・800・3.2k（4）／150・500・1.6k・5k（5）。", "Multi's number of bands (2-5). Crossovers: 800 Hz (2) / 250, 2.5k (3) / 200, 800, 3.2k (4) / 150, 500, 1.6k, 5k (5)." },
    { "dkTo", "Duck Applies To", "ER＋テール全体を下げるか、テールだけを下げるか。テールだけにすると、近さ（ER）は残したまま濁りだけ減らせます。", "Duck the whole reverb or the tail only. Tail only keeps the closeness (the ER) and removes only the wash." },
    { "gtOn", "Gate", "残響をキーの大きさで切ります（80 年代のゲートリバーブ）。", "Cuts the reverb by the key's level (the '80s gated reverb)." },
    { "gtThr", "Gate Threshold", "キーがこれを下回ると閉じ始めます（Hold のあと）。", "When the key falls below this, the gate closes (after the hold)." },
    { "gtHold", "Gate Hold", "キーが下回ってから閉じるまで開けておく時間。ゲートリバーブの「長さ」はここで決まります。", "How long it stays open after the key falls below the threshold: the gated reverb's length." },
    { "gtRel", "Gate Release", "閉じる速さ。短いほどバッサリ切れる（20〜80 ms が定番）。", "How fast it closes. Short = an abrupt cut (20-80 ms is the classic)." },
    { "gtKey", "Gate Key", "Input＝ドライの大きさで開閉（スネアのたびに開く）、Reverb＝残響そのものの大きさで閉じる。", "Input: opens and closes with the dry (each snare hit opens it); Reverb: closes on the reverb's own level." },
    // 0.6.0: MOD
    { "chMix", "Chorus", "リバーブの出力にかけるコーラスの量。0 % でオフ。残響に揺らぎと広がりを足します（80 年代のデジタルリバーブの質感）。", "The chorus on the reverb's output. 0 % = off. Adds motion and width (the '80s digital reverb sound)." },
    { "chRate", "Chorus Rate", "コーラスの揺れの速さ。", "The chorus's speed." },
    { "chDepth", "Chorus Depth", "コーラスの揺れの深さ（ms）。深いほどピッチが揺れて聞こえます。", "The chorus's depth (ms). Deeper = you hear the pitch move." },
    { "l1Rate", "LFO 1 Rate", "LFO 1 の速さ（Sync が Free のとき）。", "LFO 1's speed (when Sync is Free)." },
    { "l2Rate", "LFO 2 Rate", "LFO 2 の速さ（Sync が Free のとき）。", "LFO 2's speed (when Sync is Free)." },
    { "l1Shape", "LFO 1 Shape", "波形：Sine／Triangle／Saw／Square／S&H（段階的にランダム）／Random（なめらかにランダム）。", "Shape: sine / triangle / saw / square / S&H (stepped random) / random (smooth)." },
    { "l2Shape", "LFO 2 Shape", "波形：Sine／Triangle／Saw／Square／S&H（段階的にランダム）／Random（なめらかにランダム）。", "Shape: sine / triangle / saw / square / S&H (stepped random) / random (smooth)." },
    { "l1Sync", "LFO 1 Sync", "テンポに同期（小節・音符）。再生中は曲の位置に合わせて動きます。", "Locks to the tempo (bars, notes). While playing it follows the song position." },
    { "l2Sync", "LFO 2 Sync", "テンポに同期（小節・音符）。再生中は曲の位置に合わせて動きます。", "Locks to the tempo (bars, notes). While playing it follows the song position." },
    { "enAtk", "Envelope Attack", "エンベロープ（入力の大きさ）が上がる速さ。", "How fast the envelope (the input's level) rises." },
    { "enRel", "Envelope Release", "エンベロープが下がる速さ。", "How fast the envelope falls." },
    { "enSens", "Envelope Sensitivity", "エンベロープの感度。−48〜0 dB の入力を 0〜1 にします。小さな素材は上げる。", "The envelope's sensitivity: -48..0 dB of input becomes 0..1. Raise it for quiet material." },
    { "m1Src", "Mod 1 Source", "このスロットのソース（LFO 1／LFO 2／エンベロープ）。", "This slot's source (LFO 1 / LFO 2 / envelope)." },
    { "m1Dst", "Mod 1 Destination", "行き先：Mix、Width、Out Level（±12 dB）、Tail Level／ER Level（±12 dB）、Decay（×½〜×2）、Treble Decay（高域の残響時間）、Chorus Depth、Duck Depth、Gate Thr。例：Env → Decay −40 %＝大きく弾くと短く、静かだと長く。", "Destination: mix, width, output level (+/-12 dB), tail / ER level (+/-12 dB), decay (x0.5-x2), treble decay, chorus depth, duck depth, gate threshold. E.g. Env -> Decay -40 %: loud playing = shorter, quiet = longer." },
    { "m1Amt", "Mod 1 Amount", "送る量（±100 %）。マイナスで逆向き。", "How much (+/-100 %). Negative = the other way." },
    { "m2Src", "Mod 2 Source", "このスロットのソース（LFO 1／LFO 2／エンベロープ）。", "This slot's source (LFO 1 / LFO 2 / envelope)." },
    { "m2Dst", "Mod 2 Destination", "行き先：Mix、Width、Out Level（±12 dB）、Tail Level／ER Level（±12 dB）、Decay（×½〜×2）、Treble Decay（高域の残響時間）、Chorus Depth、Duck Depth、Gate Thr。例：Env → Decay −40 %＝大きく弾くと短く、静かだと長く。", "Destination: mix, width, output level (+/-12 dB), tail / ER level (+/-12 dB), decay (x0.5-x2), treble decay, chorus depth, duck depth, gate threshold. E.g. Env -> Decay -40 %: loud playing = shorter, quiet = longer." },
    { "m2Amt", "Mod 2 Amount", "送る量（±100 %）。マイナスで逆向き。", "How much (+/-100 %). Negative = the other way." },
    { "m3Src", "Mod 3 Source", "このスロットのソース（LFO 1／LFO 2／エンベロープ）。", "This slot's source (LFO 1 / LFO 2 / envelope)." },
    { "m3Dst", "Mod 3 Destination", "行き先：Mix、Width、Out Level（±12 dB）、Tail Level／ER Level（±12 dB）、Decay（×½〜×2）、Treble Decay（高域の残響時間）、Chorus Depth、Duck Depth、Gate Thr。例：Env → Decay −40 %＝大きく弾くと短く、静かだと長く。", "Destination: mix, width, output level (+/-12 dB), tail / ER level (+/-12 dB), decay (x0.5-x2), treble decay, chorus depth, duck depth, gate threshold. E.g. Env -> Decay -40 %: loud playing = shorter, quiet = longer." },
    { "m3Amt", "Mod 3 Amount", "送る量（±100 %）。マイナスで逆向き。", "How much (+/-100 %). Negative = the other way." },
    { "m4Src", "Mod 4 Source", "このスロットのソース（LFO 1／LFO 2／エンベロープ）。", "This slot's source (LFO 1 / LFO 2 / envelope)." },
    { "m4Dst", "Mod 4 Destination", "行き先：Mix、Width、Out Level（±12 dB）、Tail Level／ER Level（±12 dB）、Decay（×½〜×2）、Treble Decay（高域の残響時間）、Chorus Depth、Duck Depth、Gate Thr。例：Env → Decay −40 %＝大きく弾くと短く、静かだと長く。", "Destination: mix, width, output level (+/-12 dB), tail / ER level (+/-12 dB), decay (x0.5-x2), treble decay, chorus depth, duck depth, gate threshold. E.g. Env -> Decay -40 %: loud playing = shorter, quiet = longer." },
    { "m4Amt", "Mod 4 Amount", "送る量（±100 %）。マイナスで逆向き。", "How much (+/-100 %). Negative = the other way." },
    { "src", "Source", "ER の作り方。ROOM = 実際の部屋を鏡像法で計算。ALGO = 9つのパラメーターで描く（ビンテージ機モデルもここ）。IR = 読み込んだ IR の反射を使う。",
      "How the ER is made. ROOM = a real room by the image-source method. ALGO = drawn with nine parameters (the vintage models too). IR = the reflections of a loaded IR." },
    { "roomW", "Width", "部屋の幅（左右）。広いほど側壁の反射が遅れ、左右の広がりが増します。", "Room width (left-right). Wider = later side reflections and more width." },
    { "roomD", "Depth", "部屋の奥行き（前後）。前後の壁の反射の時間を決めます。", "Room depth (front-back). Sets the timing of the front and back walls." },
    { "roomH", "Height", "天井の高さ。床と天井の反射の時間です。低い天井は初回反射が早く、こもりやすい。", "Ceiling height: the floor and ceiling bounces. A low ceiling gives an early first reflection and a boxy sound." },
    { "order", "Order", "何回まで壁に当たった反射を計算するか（1〜5）。上げるほど反射が増え、ERが密になります。", "How many wall bounces are traced (1-5). Higher = more reflections and a denser ER." },
    { "wall", "Walls", "壁と天井の材質。吸音が大きいほど反射が小さく、高域が早く減ります。", "Wall and ceiling material. More absorption = quieter reflections and faster treble loss." },
    { "floor", "Floor", "床の材質。客席や絨毯は床の反射を吸います（ホールで重要）。", "Floor material. Seats or carpet absorb the floor bounce (important in halls)." },
    { "first", "First", "直接音から最初の反射までの空白（ITDG）。部屋の大きさと壁までの距離の手がかり。1 ms 前後は反射でなくコムフィルターの音色になります。",
      "The gap from the direct sound to the first reflection (ITDG): the cue for room size and distance. Around 1 ms it becomes comb-filter colour instead of a reflection." },
    { "length", "Length", "ER が終わる時刻（直接音から）。反射が分布する時間幅。", "When the ER ends (from the direct sound): the spread of the reflections in time." },
    { "taps", "Reflections", "反射の本数。少ないと一つずつ聞こえて金属的、多いとなめらか。", "Number of reflections. Few = audible and metallic, many = smooth." },
    { "growth", "Growth", "時間とともに反射が密になる度合い。0 = 均等、2 = 実際の部屋に近い（後半に集まる）。", "How the reflections crowd towards the end. 0 = even, 2 = like a real room." },
    { "energy", "Energy", "ER 全体のエネルギー（直接音比）。ALGO の基本の量です。", "Total ER energy relative to the direct sound: ALGO's basic amount." },
    { "decay", "Envelope", "ER 内の音量変化。マイナス = 減衰（自然な部屋）、0 = 平ら（ノンリニア・ゲート）、プラス = ビルドアップ（リバース型）。",
      "Level change across the ER. Negative = decay (a natural room), 0 = flat (non-linear, gated), positive = build-up (reverse)." },
    { "damp", "Damping", "ER の終わりでの高域カットオフ。後ろの反射ほど高域が削られます。低いほど柔らかい部屋。", "Treble cut-off at the end of the ER; later reflections lose more treble. Lower = softer room." },
    { "vari", "Variation", "反射の並び（乱数の種）。同じ設定のまま別の並びを試せます。", "The reflection layout (random seed). Try another layout with the same settings." },
    { "irWin", "IR Window", "IR から ER として切り出す長さの上限。Auto のときは混合時間（反射がノイズ状に密になる時刻）で打ち切ります。",
      "Longest ER taken from the IR. With Auto it stops at the mixing time (where the reflections become noise-like)." },
    { "irThr", "Threshold", "反射として拾うレベルの下限（直接音比）。下げるほど小さな反射まで拾います。", "Lowest level picked as a reflection (relative to the direct sound). Lower = more small reflections." },
    { "irMode", "IR Mode", "Taps = 拾った反射をタップで鳴らす（反射ごとの明るさも IR から測る。Size・Width・Diffusion などで編集可）。Conv = ER 部分をそのまま畳み込む答え合わせ用（Size・Darkening は効きません）。Hybrid = 拾った反射の位置はそのままに、Split の前後で量を別々に IR に合わせ、Split 以降は隙間を補いの反射で埋める（前半の量が正確、後半はなめらか）。",
      "Taps = plays the picked reflections (the brightness of each is measured from the IR; editable with Size, Width, Diffusion...). Conv = convolves the ER as it is, for reference (Size and Darkening have no effect). Hybrid = the picked reflections stay where they are; the parts before and after Split are each set to the IR's level, and after Split the gaps get fill reflections (accurate early level, smoother later part)." },
    { "irSplit", "Hybrid Split", "Hybrid で前半と後半を分ける時刻（直接音から）。Split までは拾った反射だけ、そのあとは補いの反射を足して 4 ms ごとに IR の量に合わせます。目安は混合時間の半分くらい（20〜40 ms）。",
      "In Hybrid, the time that divides the early and later parts (from the direct sound). Up to Split only the picked reflections; after it fill reflections are added and every 4 ms is set to the IR's level. About half the mixing time (20-40 ms)." },
    { "irFill", "Hybrid Fill", "Hybrid で Split 以降に足す補いの反射の量。0% = 拾った反射だけ（Taps に近い）、100% = 拾った本数の 2 倍まで足す（なめらか・密）。詰まって聞こえるときは下げます。",
      "In Hybrid, how many fill reflections are added after Split. 0% = the picked reflections only (close to Taps), 100% = up to twice as many again (smooth, dense). Lower it if the later part sounds crowded." },
    { "roomEarly", "ROOM Early", "ALGO の最初の何 ms を ROOM の反射にするか（ROOM の設定がそのまま使われる）。0 = Off（ALGO だけ）。「ROOM → ALGO（前半 ROOM）」は 20〜40 ms のうち一致度が最も高い長さに設定します。部屋らしい最初の数本と、ALGO の後半を組み合わせられます。",
      "How many ms at the start of the ALGO are ROOM's reflections (as set in ROOM). 0 = Off (ALGO only). \"ROOM -> ALGO (early ROOM)\" sets whichever of 20-40 ms matches best. Combines the room's first few reflections with the ALGO's later part." },
    { "darken", "Darkening", "後ろの反射ほど高域を削る量（100 ms あたり何オクターブ下げるか）。どのソースにも効きます（Conv 以外）。実際の部屋で後ろの反射が暗くなる（壁の吸収・空気吸収）変化を足します。0 = オフ。IR がソースのときは、読み込んだ IR から推定した値が入ります（その値で IR どおりの音）。",
      "How much treble the later reflections lose (octaves of cut-off per 100 ms). Works on every source but Conv. Adds the way later reflections grow darker in real rooms (wall and air absorption). 0 = off. With an IR as the source, the IR's own darkening is estimated on loading and set here (the sound does not change at that value)." },
    { "irAuto", "Window", "Fixed = IR Window の長さで固定。Auto = 混合時間で自動的に打ち切り。", "Fixed = use the IR Window length. Auto = stop at the mixing time." },
    { "irWet", "IR Type", "Direct = 直接音を含む部屋の IR。Wet only = リバーブ機の戻りなど直接音がない IR（ファイル先頭を 0 ms とし、最大ピーク +6 dB を仮の直接音にします）。",
      "Direct = a room IR with its direct sound. Wet only = no direct sound, e.g. a reverb return (time 0 = file start; the strongest peak + 6 dB stands in for the direct sound)." },
    { "predelay", "Predelay", "ER 全体を後ろへ平行移動。直接音との間が空き、輪郭が前に出ます。", "Moves the whole ER later. Opens a gap after the direct sound so the attack stays clear." },
    { "size", "Size", "すべての反射の時間を伸縮（%）。どのソースにも効く「部屋の大きさ」のつまみ。", "Stretches all reflection times (%). The 'room size' control for every source." },
    { "width", "Width", "反射の左右の広がり（M/S）。0% = モノ、150% = 最大。広げすぎるとモノで打ち消し合います。", "Stereo width of the reflections (M/S). 0% = mono, 150% = maximum. Too wide can cancel in mono." },
    { "diffusion", "Diffusion", "反射をオールパスでにじませる量。0% = 一つずつの反射、100% = 密な響き（プレート的）。", "Smears the reflections with allpasses. 0% = discrete reflections, 100% = dense (plate-like)." },
    { "lowcut", "Low Cut", "ER の低域カット。低域の濁りを取り、直接音の芯を残します。傾きは FILTER の LC Slope で選びます。", "Low cut on the ER. Removes low-end mud and keeps the direct sound's body. Pick the slope with LC Slope in FILTER." },
    { "highcut", "High Cut", "ER の高域カット。距離感と柔らかさ。傾きは FILTER の HC Slope で選びます。", "High cut on the ER: distance and softness. Pick the slope with HC Slope in FILTER." },
    { "lcSlope", "Low Cut Slope", "ローカットの傾き。6 dB/oct はゆるやか、12〜24 は一般的、36〜48 は低域をはっきり切る。Butterworth 特性。",
      "Low-cut slope. 6 dB/oct is gentle, 12-24 typical, 36-48 cuts the lows off sharply. Butterworth response." },
    { "hcSlope", "High Cut Slope", "ハイカットの傾き。6 dB/oct はゆるやかで自然な空気吸収、12〜24 は一般的なフィルター、36〜48 は帯域をはっきり切る（ビンテージ機の帯域制限のような切れ方）。Butterworth 特性。",
      "Low-pass slope. 6 dB/oct is gentle, like air absorption; 12-24 is a typical filter; 36-48 cuts the band off sharply (like the band limit of a vintage unit). Butterworth response." },
    { "level", "ER Level", "ER 全体の音量の調整（どのソースにも効くトリム）。", "Overall ER level trim (works for every source)." },
    { "bw", "Bandwidth", "昔のデジタル機の帯域（低いサンプリング周波数）。20 kHz = オフ。8〜10 kHz で70年代、15 kHz で80年代の高級機の手触り。",
      "The bandwidth of old digital units (low sample rates). 20 kHz = off. 8-10 kHz for the 70s, 15 kHz for an 80s flagship." },
    { "bits", "Bits", "量子化のビット数。24 = オフ。12〜16 bit で初期デジタルのざらつき。", "Quantisation bits. 24 = off. 12-16 bits for early digital grain." },
    { "wander", "Wander", "反射の時間をゆっくり揺らす量。にじみと「コーラス」感（ビンテージ機のランダム変調）。", "Slow random movement of the reflection times: smear and chorus (the random modulation of vintage units)." },
    { "mix", "Mix", "50% = 直接音 1：ER は設定どおりの比（部屋の中で聴く状態）。100% = ER だけ（センドリターン用）。0% = ドライ。",
      "50% = direct sound at unity with the ER at its set ratio (as heard in the room). 100% = ER only (for a send). 0% = dry." },
    { "out", "Output", "出力レベル。", "Output level." },
    { "autoLvl", "Auto Level", "ROOM・ALGO・IR を切り替えたとき、切り替える直前と同じ ER の量になるよう自動で補正します（右の数値が補正量）。プリセットやレッスンを読み込むと 0 に戻り、そのプリセット本来の量になります。",
      "When you switch ROOM / ALGO / IR, the ER is corrected to the level heard just before the switch (the figure shows the correction). Loading a preset or lesson resets it to 0, the preset's own level." },
    // ===== Reverb Anatomy Lab =====
    { "tDecay", "Decay", "テール（後部残響）の残響時間 RT60（音が 60 dB 下がるまでの時間）。周波数ごとの長さは下のグラフで、この値に対する倍率として決めます。小部屋 0.3〜0.8 s、ホール 1.8〜2.5 s、大聖堂 4 s 以上が目安。",
      "Reverb time (RT60) of the tail: the time for a 60 dB drop. The graph sets each frequency as a multiple of this. Small room 0.3-0.8 s, hall 1.8-2.5 s, cathedral 4 s+." },
    { "tSize", "Size", "FDN の遅延線の長さ（いちばん長い線）。大きいほど空間が大きく、響きの粒が粗くなります（エコー密度が下がる）。動かすと音程が少し揺れながら移ります（最大 5 %）。部屋 30〜60 ms、ホール 80〜150 ms。",
      "Length of the FDN's delay lines (the longest one). Larger = a bigger space with a coarser grain (lower echo density). It glides when moved (pitch shifts by at most 5 %). Room 30-60 ms, hall 80-150 ms." },
    { "tPre", "Tail Delay", "テールが ER からどれだけ遅れて始まるか（VOICE の Predelay にさらに足す）。ER と後部残響の間の時間で、空間の大きさの手がかりになります。小部屋 5〜15 ms、ホール 20〜40 ms。",
      "How much later than the ER the tail starts (added to VOICE Predelay). The gap between early reflections and late reverb is a cue for the size of the space. Small room 5-15 ms, hall 20-40 ms." },
    { "tDiff", "Diffusion", "テールに入る前の拡散（オールパス 4 段）。上げるほど立ち上がりがなめらかに、下げると粒が聞こえる硬い響き。スネアやパーカッションで違いがよく分かります。",
      "Diffusion before the tail (4 allpasses). Higher = a smoother onset, lower = a grainy, harder sound. Easiest to hear on snare and percussion." },
    { "tModD", "Mod Depth", "遅延線の長さをゆっくり揺らす量（最大 1.2 ms）。金属的な鳴り（モード）をぼかし、コーラスのような広がりを足します。上げすぎるとピアノやギターの音程が揺れて聞こえます。",
      "How much the delay lines slowly move (up to 1.2 ms). Blurs metallic ringing (modes) and adds a chorus-like width. Too much makes pitched sources (piano, guitar) waver." },
    { "tModR", "Mod Rate", "揺れの速さ。線ごとに少しずつ違う速さで動きます。遅い（0.2〜0.8 Hz）ほど自然、速いとコーラス・ビブラート的。", "Speed of the movement; each line moves at a slightly different rate. Slow (0.2-0.8 Hz) = natural, fast = chorus / vibrato." },
    { "tEng", "Tail Engine", "テールを作る仕組み。Room / Hall＝16 本の遅延線を混ぜ合わせる FDN（部屋やホールの自然な密度）。Plate＝鉄板リバーブの古典的な「8 の字」タンク（入力の拡散器 4 段＋2 つの半分が交差して戻るループ）：立ち上がりからすぐ密で、明るく、なめらか。Ambient＝どんな部屋より大きい空間（0.1〜1 秒の長い遅延線 8 本、ループ内の拡散、ゆっくり深い揺れ）＋Gravity・Feedback。Spring＝ギターアンプなどのばねリバーブ：低い音ほど遅れて返る「ビヨーン」、数十 ms ごとの繰り返し、5 kHz までの狭い帯域（Diffusion＝ビヨーンの強さ、Size＝ばねの長さ）。どれも周波数ごとの RT・Size・Diffusion・Mod が効きます。",
      "How the tail is made. Room / Hall = a 16-line FDN (the natural density of rooms and halls). Plate = the classic figure-of-eight plate tank (4 input diffusers + two cross-fed halves): dense from the start, bright and smooth. Ambient = a space bigger than any room (8 long lines of 0.1-1 s, diffusion in the loop, slow deep motion) + Gravity and Feedback. Spring = the spring reverb of guitar amps: low notes come back later (the \"boing\"), repeats every few tens of ms, a narrow band up to 5 kHz (Diffusion = how much boing, Size = spring length). RT per frequency, Size, Diffusion and Mod work on all three." },
    { "erBal", "ER / TAIL", "ER とテールのバランス（Cinematic Rooms の Refl / Rev Mix に当たる）。0＝Equal（どちらも設定どおり）、左へ回すほどテールが下がり（ER が前に＝近い）、右へ回すほど ER が下がります（テールが前に＝遠い）。端で片方だけ。",
      "The ER / tail balance (like a Refl / Rev mix). 0 = Equal (both as set); turning left lowers the tail (ER forward = closer), right lowers the ER (tail forward = farther). At the ends only one is left." },
    { "tLvl", "Tail Level", "テールの音量（ER は VOICE の ER Level）。ER とテールのバランスで、近さ（ER が強い）と遠さ（テールが強い）が変わります。", "Tail level (the ER has VOICE's ER Level). The ER / tail balance moves the source near (more ER) or far (more tail)." },
    { "tFreeze", "Freeze", "テールを鳴らしたまま止めます（減衰なし、新しい入力は入らない）。パッドやドローンを作る用途。", "Holds the tail as it is (no decay, no new input). For pads and drones." },
    { "rtMode", "RT Mode", "周波数ごとのリバーブタイムの決め方。EQ = 5 つの点で曲線を描く（なめらかな変化、細かく作れる）。Multiband = 帯域ごとの倍率（実機のクロスオーバー＋倍率型、考え方が分かりやすい）。",
      "How the reverb time per frequency is set. EQ = a curve through 5 points (smooth, detailed). Multiband = a multiple per band (the crossover + multiplier style of many units, easy to reason about)." },
    { "mbN", "MB Bands", "Multiband の帯域数（2〜5）。", "Number of bands in Multiband (2-5)." },
    { "erOn", "ER", "ER（初期反射）のオン／オフ。オフでテールだけを聞けます。", "ER on / off. Off = the tail alone." },
    { "tOn", "TAIL", "テール（後部残響）のオン／オフ。オフで ER だけを聞けます。", "Tail on / off. Off = the ER alone." },
    { "inMode", "Input", "リバーブに入れる信号。Stereo = そのまま（Input Width で幅を変えられる）、Mono = L+R、Left / Right = 片側だけ。モノラル音源とステレオ音源でリバーブの広がりがどう変わるかを比べられます。",
      "What goes into the reverb. Stereo = as it is (Input Width changes the width), Mono = L+R, Left / Right = one side. Compare how mono and stereo sources change the reverb's width." },
    { "inW", "Input Width", "リバーブに入る信号のステレオ幅（0 % = モノ、100 % = そのまま、200 % = 2 倍）。", "Stereo width of the signal going into the reverb (0 % = mono, 100 % = as is, 200 % = double)." },
    { "outMode", "Output", "出力。Mono はドライも含めて L+R にまとめます（モノラル再生での聞こえ方の確認）。", "Output. Mono folds everything, dry included, to L+R (to check mono playback)." },
    { "outW", "Width", "リバーブ（ER＋テール）の出力の幅。0 % = モノ、200 % = 2 倍。ドライは変わりません。", "Width of the reverb output (ER + tail). 0 % = mono, 200 % = double. The dry is not changed." },
    { "iLC", "Input Low Cut", "リバーブに入る前の低域カット。低音がリバーブで濁るのを防ぐ定番の処理（ボーカル 150〜300 Hz、全体 100〜200 Hz）。", "Low cut before the reverb: the usual way to keep the low end from muddying it (vocals 150-300 Hz, mix 100-200 Hz)." },
    { "iHC", "Input High Cut", "リバーブに入る前の高域カット。歯擦音やシンバルがリバーブで目立つのを抑えます（6〜10 kHz）。", "High cut before the reverb: keeps sibilance and cymbals from standing out in it (6-10 kHz)." },
    { "iLCs", "LC Slope", "インプット Low Cut の傾き（6〜48 dB/oct）。", "Slope of the input low cut (6-48 dB/oct)." },
    { "iHCs", "HC Slope", "インプット High Cut の傾き（6〜48 dB/oct）。", "Slope of the input high cut (6-48 dB/oct)." },
    { "iTilt", "Tilt", "リバーブに入る音の明るさを傾けます（250 Hz と 4 kHz のシェルフを逆向きに）。+ で明るく、− で暗く。", "Tilts the brightness of what goes into the reverb (opposite shelves at 250 Hz and 4 kHz). + brighter, - darker." },
    { "iTo", "Applies to", "インプットフィルターを ER とテールの両方にかけるか、片方だけか。例：テールだけ低域を切ると、ER の厚みを残したまま残響の濁りを取れます。",
      "Whether the input filter feeds the ER and the tail, or one of them. E.g. cutting lows on the tail only keeps the ER's body while clearing the reverb's mud." },
    { "eqOn", "EQ", "マスター EQ のオン／オフ（リバーブの出力だけにかかる。ドライには効きません）。", "Master EQ on / off (on the reverb output only, not the dry)." },
    { "eqHP", "High Pass", "マスター EQ のハイパス（12 dB/oct）。", "Master EQ high pass (12 dB/oct)." },
    { "eqLP", "Low Pass", "マスター EQ のローパス（12 dB/oct）。", "Master EQ low pass (12 dB/oct)." },
    { "eqF", "EQ Freq", "このバンドの周波数（1 = ローシェルフ、6 = ハイシェルフ、2〜5 = ピーク）。グラフの点をドラッグしても動かせます。", "This band's frequency (1 = low shelf, 6 = high shelf, 2-5 = peaks). You can also drag the point in the graph." },
    { "eqG", "EQ Gain", "このバンドのゲイン（±18 dB）。リバーブの出力だけを補正します。", "This band's gain (+-18 dB), on the reverb output only." },
    { "tSrc", "Tail Source", "テールの作り方（ER の SOURCE と同じ並び）。ROOM＝ER の部屋（大きさ・壁と床の材質）から帯域ごとの残響時間を計算し、ALGO を鳴らす（Eyring の式＋空気の吸収）。ALGO＝Decay と曲線を自由に決めるアルゴリズム（FDN／プレート）。IR＝読み込んだ IR の残響を畳み込み。IR+ALGO＝はじめは IR、IR -> ALGO の時間から ALGO に引き継ぐ（IR の質感と、アルゴリズムの揺れ・長さの自由を両立）。",
      "How the tail is made (in the same order as the ER's SOURCE). ROOM = the reverb time per band computed from the ER's room (size, wall and floor materials), played by the ALGO tail (Eyring's formula + air absorption). ALGO = an algorithm whose Decay and curve you set freely (FDN / plate). IR = the loaded IR's reverb, convolved. IR+ALGO = the IR first, ALGO takes over after IR -> ALGO (the IR's texture with the algorithm's motion and freedom of length)." },
    { "aGrav", "Gravity", "AMBIENT のみ。響きの重心。マイナス＝後ろほど大きい（逆再生のように膨らむ「スウェル」）、0＝普通、プラス＝前に寄った密な立ち上がり。仕組み：最長 3 秒にわたるタップの重みを、後ろ上がり／前上がりにしてから空間に入れる（時間の長さは Decay の約 45 %）。",
      "AMBIENT only. Where the weight of the sound sits. Minus = louder later (a swell, like reversed), 0 = normal, plus = a dense front. How: a bank of taps over up to 3 s, weighted rising or falling, before the space (its length about 45 % of Decay)." },
    { "aFb", "Feedback", "AMBIENT のみ。出力を 0.5 秒遅らせて少し暗くし、柔らかく抑えて入力に戻す量。上げるほど Decay より長く、花が開くように育つ（90 % まで、発振しないよう抑えている）。",
      "AMBIENT only. How much of the output goes back into the input (0.5 s later, a little darker, softly limited). The higher, the longer than Decay and the more it blooms (up to 90 %, held below oscillation)." },
    { "tRoom", "Tail From Room", "ROOM：テールの長さを ER の部屋から計算します。", "ROOM: the tail's length is computed from the ER's room." },
    { "irtX", "Tail Start", "IR のどこからをテールに使うか（直接音から）。ER の窓の終わり（ER Window）に合わせると、ER と重なりません。", "Where in the IR the tail starts (from the direct sound). Matching the ER window's end avoids overlap with the ER." },
    { "irtLen", "Length", "テールに使う長さ（最後 50 ms でフェードアウト）。", "How much of the IR the tail uses (50 ms fade at the end)." },
    { "irtSt", "Stretch", "IR を伸び縮みさせる（再生速度を変える）。長くすると空間が大きく、音程は下がる（テープの速度と同じ）。", "Stretches the IR (changes its playback speed). Longer = a bigger space with a lower pitch, like tape speed." },
    { "irtLvl", "IR Level", "IR テールの音量（音量合わせの上に足す微調整）。", "Level of the IR tail (a trim on top of the match)." },
    { "irtAuto", "IR Tail Match", "On：IR テールを、同じ長さの ALGO テールと同じ大きさにそろえる（ソースを切り替えても音量が変わらない。IR+ALGO では受け渡しの時点で ALGO が同じ大きさから続く）。Off：IR に録られていた大きさのまま（直接音に対する響きの比＝その場所の残響感）。",
      "On: the IR tail is as loud as an ALGO tail of the same length (switching the source keeps the level; with IR+ALGO the ALGO goes on from the same level at the hand-over). Off: as loud as in the IR (its reverb against its direct sound = how reverberant that place is)." },
    { "irtDL", "Decay Low", "300 Hz 以下の残響時間を何倍にするか（エンベロープを掛け直す）。", "Multiplies the reverb time below 300 Hz (by reshaping the envelope)." },
    { "irtDM", "Decay Mid", "300 Hz〜3 kHz の残響時間を何倍にするか。", "Multiplies the reverb time from 300 Hz to 3 kHz." },
    { "irtDH", "Decay High", "3 kHz 以上の残響時間を何倍にするか。高域だけ短くすると、暗く柔らかい響きに。", "Multiplies the reverb time above 3 kHz. Shorter highs = a darker, softer reverb." },
    { "irtRev", "Reverse", "IR テールを逆再生（だんだん大きくなって止まる、リバース・リバーブ）。", "Plays the IR tail backwards (swells then stops: reverse reverb)." },
    { "irtB", "IR -> ALGO", "IR+ALGO のとき、Tail Start から何 ms 後に FDN へ引き継ぐか（60 ms のクロスフェード）。短いほど ALGO 寄り。", "In IR+ALGO: how long after Tail Start the FDN takes over (60 ms crossfade). Shorter = more ALGO." },
    { "rtF", "RT Freq", "この点の周波数。グラフの点を左右にドラッグ、←→キーでも動きます。", "This point's frequency. Drag the point sideways or use the left / right keys." },
    { "rtM", "RT % Decay", "この点のリバーブタイム（Decay に対する %）。100 %＝Decay と同じ、200 %＝2 倍長い。ダブルクリックで 100 %。", "This point's reverb time as % of Decay. 100 % = Decay, 200 % = twice as long. Double-click = 100 %." },
    { "rtQ", "RT Q", "点 1・5（シェルフ）は傾きの急さ、点 2〜4（ベル）は幅（大きいほど狭い）。グラフ上でホイールでも変えられます。", "Points 1 / 5 (shelves): how steep; points 2-4 (bells): width (higher = narrower). The mouse wheel over the point works too." },
    { "mbM", "Band x Decay", "この帯域のリバーブタイム（Decay に対する倍率）。", "This band's reverb time as a multiple of Decay." },
    { "mbX", "Crossover", "帯域の境目の周波数。", "Frequency where two bands meet." },
    { "eqQ", "EQ Q", "このバンドの幅（大きいほど狭い）。シェルフでは肩の形。グラフ上でホイールでも変えられます。", "This band's width (higher = narrower); for the shelves, the shape of the knee. Mouse wheel over the graph point works too." },
    { "tail", "Tail Preview", "ER を入力にした簡易テール（8本の FDN）。あとで作るテール用プラグインとの組み合わせを試すためのものです。", "A simple tail (8-line FDN) fed by the ER, to try the combination with the tail plug-in to come." },
    { "tailRT", "Tail Decay", "テールの残響時間（RT60）。", "Tail reverb time (RT60)." },
    { "tailLvl", "Tail Level", "テールの音量。", "Tail level." },
    // ===== the rooms (0.10.5) =====
    { "srcX", "Source X", "音源の左右の位置（0 % = 左の壁、100 % = 右の壁）。平面図でドラッグしても動きます。", "The source across the room (0 % = the left wall, 100 % = the right). Dragging it on the plan moves it too." },
    { "srcY", "Source Y", "音源の奥行きの位置（0 % = ステージ側の壁、100 % = 奥の壁）。", "The source along the depth (0 % = the stage-end wall, 100 % = the back wall)." },
    { "lisX", "Listener X", "聴く位置の左右（0 % = 左の壁、100 % = 右の壁）。音源との距離で直接音と反射の比が変わります。", "The listener across the room (0 % = the left wall, 100 % = the right). The distance to the source sets the direct-to-reflected ratio." },
    { "lisY", "Listener Y", "聴く位置の奥行き（0 % = ステージ側、100 % = 奥の壁）。奥ほど直接音が遠く、初回反射が早く来ます。", "The listener along the depth (0 % = the stage end, 100 % = the back wall). Further back, the direct sound is weaker and the first reflections arrive sooner." },
    { "shape", "Room Shape", "部屋の形。Shoebox＝直方体。Hall (fan)＝ステージ側が狭い扇形（平行な壁がなくフラッターが出にくい）。Skewed＝左の壁だけ傾く。Dome＝円筒＋半球（頭上の反射が集まる）。Connected＝戸口でつながる 2 部屋（減衰が二重勾配になる）。Corridor＝奥で曲がる L 字の廊下（角の先は直接音がさえぎられる）。",
      "The room's shape. Shoebox = a plain box. Hall (fan) = narrow at the stage and opening out (no parallel walls, so little flutter). Skewed = only the left wall leans. Dome = a drum with a hemisphere on top (reflections converge overhead). Connected = two rooms joined by a doorway (the decay gets two slopes). Corridor = an L-shaped hallway (past the bend the direct sound is blocked)." },
    { "splay", "Room Splay", "形ごとの「開き」。Hall＝壁が開く量、Skewed＝左の壁の傾き、Dome＝ドームの高さ（100 % = 半球）、Connected＝戸口の幅、Corridor＝廊下の幅。形を選ぶと目安の値が入ります。",
      "How far the shape opens. Hall = how much the walls splay, Skewed = the lean of the left wall, Dome = the dome's rise (100 % = a hemisphere), Connected = the width of the doorway, Corridor = the width of the hallway. Choosing a shape fills in a typical value." },
    { "backAbs", "Back Room Absorb", "連結（Connected）のときだけ。奥の部屋の壁を手前より吸音側（＋：客席のよう）／反射側（−：タイルのよう）にします。差が大きいほど減衰の折れ曲がり（二重勾配）がはっきりします。",
      "Connected rooms only. Makes the back room's walls more absorbent (+, like audience seating) or more reflective (-, like tile) than the front's. The bigger the difference, the clearer the bend in the decay (the double slope)." },
    { "clutter", "Room Clutter", "部屋の散らかり具合（家具・物・凹凸のある面）。材質とは別で、反射を散らして密で不規則にし（タップを増やし、時間とパンをずらす）、布や家具が高域を吸って RT を短くします。0 % = 何もない部屋。",
      "How cluttered the room is (furniture, objects, rough surfaces). Separate from the material: it scatters the reflections into a denser, less regular pattern (more taps, their time and pan moved) and the soft furnishings take the highs, shortening the RT. 0 % = an empty room." },
    { "erRoomSel", "ER Room", "ER（初期反射）をどの部屋から作るか（A / B）。カードの［ER］ボタンでも選べます。", "Which room the ER comes from (A / B). The cards' [ER] buttons do the same." },
    { "tRoomSel", "Tail Room", "テールをどの部屋の統計（容積・表面積・材質）から作るか（A / B）。", "Which room's statistics (volume, surface, materials) the tail is built from (A / B)." },
    { "erIrSel", "ER IR", "ER に使う IR（A / B）。選んだ方が解析（ターゲット）の対象になります。", "Which IR the ER uses (A / B). The one chosen is the one analysed (the target)." },
    { "tIrSel", "Tail IR", "テールに使う IR（A / B）。ER とは別に選べます。", "Which IR the tail uses (A / B). It can differ from the ER's." },
    { "tBody", "Tail Body", "T-HEAD のとき、頭の IR のあとを継ぐ後半（BODY）を何で作るか：ALGO / ROOM / もう一方の IR。", "With T-HEAD, what carries on after the head IR: ALGO, ROOM, or the other IR." },
};
const HelpText* helpFor (const juce::String& idIn)
{
    const juce::String id = (idIn.startsWith ("eqF") || idIn.startsWith ("eqG") || idIn.startsWith ("eqQ") || idIn.startsWith ("rtF") || idIn.startsWith ("rtM") || idIn.startsWith ("rtQ")
                             || idIn.startsWith ("mbM") || idIn.startsWith ("mbX")) && idIn.length() == 4 ? idIn.substring (0, 3) : idIn;
    static const std::pair<const char*, const char*> kAlias[] = { { "rbW", "roomW" }, { "rbD", "roomD" }, { "rbH", "roomH" }, { "rbWall", "wall" }, { "rbFloor", "floor" },
                                                                 { "rbShape", "shape" }, { "rbSplay", "splay" }, { "rbBackAbs", "backAbs" }, { "rbClutter", "clutter" } };
    juce::String key = id; for (auto& a : kAlias) if (key == a.first) { key = a.second; break; }      // room B shares room A's help
    for (auto& h : kHelp) if (key == h.id) return &h;
    return nullptr;
}

juce::String pct (float v) { return juce::String ((int) std::round (v)) + "%"; }
}

ParamCombo& MainView::mbox (const char* id)
{
    for (auto& b : modBoxes) if (b->paramID == id) return *b;
    modBoxes.push_back (std::make_unique<ParamCombo> (ph, id));
    auto& b = *modBoxes.back(); addAndMakeVisible (b);
    if (auto* h = helpFor (id)) setHelp (b, h->t, h->ja, h->en);
    return b;
}

DragValue& MainView::dv (const char* id, const char* caption, std::function<juce::String (float)> fmt)
{
    auto d = std::make_unique<DragValue> (ph, id, caption, std::move (fmt));
    if (auto* h = helpFor (id)) setHelp (*d, h->t, h->ja, h->en);
    addAndMakeVisible (*d);
    auto& ref = *d; vals[id] = std::move (d);
    return ref;
}

MainView::MainView (ERProcessor& p)
    : proc (p), ph (p),
      srcSeg (ph, "src", { "ROOM", "ALGO", "IR" }), erRoomSeg (ph, "erRoomSel", { "A", "B" }), tRoomSegAB (ph, "tRoomSel", { "A", "B" }), erIrSeg (ph, "erIrSel", { "A", "B" }), tIrSegAB (ph, "tIrSel", { "A", "B" }), irMode (ph, "irMode", { "Taps", "Conv", "Hybrid" }), irAuto (ph, "irAuto", { "Fixed", "Auto" }),
      irWet (ph, "irWet", { "Direct", "Wet only" }), tailSeg (ph, "tail", { "Off", "On" }), autoSeg (ph, "autoLvl", { "Off", "On" }),
      wallBox (ph, "wall"), floorBox (ph, "floor"), slopeBox (ph, "hcSlope"), lcSlopeBox (ph, "lcSlope"),
      echo (ph), roomView (ph), info (ph), lesson (ph), help (ph), ear (ph), earMask (ph),
      tabs (ph), detSwitch (ph, tabs), colErBtn (ph, "colEr", "ER"), colTailBtn (ph, "colTail", "TAIL"), rtGraph (ph), inGraph (ph, CurveGraph::input), eqGraph (ph, CurveGraph::masterEq), erfGraph (ph, CurveGraph::erFilter), tfGraph (ph, CurveGraph::tailFilter), combo (ph, rtGraph, eqGraph), tools (ph), measure (ph), layerBar (ph), irtView (ph),
      tSrcSeg (ph), irtRevSeg (ph, "irtRev", { "Off", "On" }), irtAutoSeg (ph, "irtAuto", { "Off", "On" }),
      tEngSeg (ph, "tEng", { "Hall", "Plate", "Spring", "Ambient" }), rtModeSeg (ph, "rtMode", { "EQ", "Multiband" }), freezeSeg (ph, "tFreeze", { "Off", "On" }), erOnSeg (ph, "erOn", { "Off", "On" }), tOnSeg (ph, "tOn", { "Off", "On" }),
      outModeSeg (ph, "outMode", { "Stereo", "Mono" }), iToSeg (ph, "iTo", { "ER + Tail", "Tail", "ER" }), eqOnSeg (ph, "eqOn", { "Off", "On" }),
      inModeBox (ph, "inMode"), iLCsBox (ph, "iLCs"), iHCsBox (ph, "iHCs"), tLCsBox (ph, "tLCs"), tHCsBox (ph, "tHCs"),
      wallA2 (ph, "wall"), floorA2 (ph, "floor"), wallB2 (ph, "rbWall"), floorB2 (ph, "rbFloor"), shapeA2 (ph, "shape"), shapeB2 (ph, "rbShape"),
      dkModeSeg (ph, "dkMode", { "Off", "Single", "Multi", "Spectral" }), dkToSeg (ph, "dkTo", { "ER + Tail", "Tail" }), gtOnSeg (ph, "gtOn", { "Off", "On" }), gtKeySeg (ph, "gtKey", { "Input", "Reverb" }),
      dynMeter (ph), modScope (ph),
      panel (ph), fgraph (ph), now (ph), deck (ph), colorDeck (ph), fader (ph), modern (ph)
{
    restoreLastView();       // (0.10.13) a new instance starts with the view the last one was left in
    theme::adjText = (float) (double) proc.uiState.getProperty ("adjText", ZoomPrefs::get ("rvlab.adjText", 0.0));
    theme::adjBright = (float) (double) proc.uiState.getProperty ("adjBright", ZoomPrefs::get ("rvlab.adjBright", 0.0));
    theme::adjContrast = (float) (double) proc.uiState.getProperty ("adjContrast", ZoomPrefs::get ("rvlab.adjContrast", 0.0));
    applySkin ((int) proc.uiState.getProperty ("skin", (int) ZoomPrefs::get ("rvlab.skinV2", 9)));       // (0.10.12) the default look: Soft Dusk, 20 % dimmer
    setLookAndFeel (&lnf);
    zoom.setSkin = [this] (int i) { applySkin (i); };
    // section presets (ER / TAIL / IO / EQ) and the tail's own IR
    addAndMakeVisible (secBtn); addChildComponent (tIrBtn); addChildComponent (tIrClearBtn);
    secBtn.onClick = [this] { showSectionMenu(); };
    tabs.onLongPress = [this] (int pg, juce::Rectangle<int> r) { showSectionMenu (pg, r); };
    tabs.onDoubleClick = [this] { tabs.toggle (1); if (onDesignChange) onDesignChange(); applyPage(); };        // a title double-clicked: open / close
    addChildComponent (detSwitch);          // (0.8.0: replaced by the EDIT / DYN + MOD buttons)
    // the detail panel: EDIT (INPUT | ER | TAIL | COLOR / OUTPUT) and DYN + MOD, each opening / closing it
    for (auto* b : { &detEditBtn, &detDynBtn, &detModBtn }) { addAndMakeVisible (*b); b->getProperties().set ("fontPx", 12.0); b->setClickingTogglesState (false); }
    detEditBtn.setButtonText ("EDIT"); detDynBtn.setButtonText ("DYN"); detModBtn.setButtonText ("MOD");
    detEditBtn.onClick = [this] { tabs.toggle (1); if (onDesignChange) onDesignChange(); applyPage(); };
    detDynBtn.onClick = [this] { setDeckView (deckView() == 2 ? 0 : 2); };
    detModBtn.onClick = [this] { setDeckView (deckView() == 3 ? 0 : 3); };
    // SOURCE (0.9.0): the rooms and the IRs the ER and the TAIL are designed from
    addAndMakeVisible (detSrcBtn); detSrcBtn.getProperties().set ("fontPx", 12.0); detSrcBtn.setButtonText ("SOURCE");
    detSrcBtn.onClick = [this] { setDeckView (deckView() == 1 ? 0 : 1); };
    setHelp (detSrcBtn, "SOURCE", "部屋 A・B と IR A・B のカード。各カードの［ER］［TAIL］（IR は［T-HEAD］も）で、ER と TAIL をどれから作るかを決めます。カードの［→ ALGO］で、その響きを ALGO のパラメーターに変換できます。",
             "Cards for rooms A and B and IRs A and B. Each card's [ER] [TAIL] (and [T-HEAD] on the IRs) chooses what the ER and the TAIL are made from; [-> ALGO] converts that sound into ALGO parameters.");
    {
        const auto mm = [] (float v) { return juce::String (v, 1) + " m"; };
        auto mk = [&] (const char* key, const char* id, const char* cap, std::function<juce::String (float)> f)
        {
            auto d = std::make_unique<DragValue> (ph, id, cap, std::move (f));
            if (auto* h = helpFor (id)) setHelp (*d, h->t, h->ja, h->en);
            addChildComponent (*d); srcComps.push_back (d.get()); srcVals[key] = std::move (d);
        };
        mk ("aW", "roomW", "Width", mm); mk ("aD", "roomD", "Depth", mm); mk ("aH", "roomH", "Height", mm);
        mk ("aO", "order", "Order", [] (float v) { return juce::String ((int) v); });
        mk ("bW", "rbW", "Width", mm); mk ("bD", "rbD", "Depth", mm); mk ("bH", "rbH", "Height", mm);
        mk ("bO", "order", "Order", [] (float v) { return juce::String ((int) v); });         // (0.10.9) the same on both cards (Order is shared)
        mk ("aC", "clutter", "Clutter", [] (float v) { return juce::String ((int) v) + " %"; }); mk ("bC", "rbClutter", "Clutter", [] (float v) { return juce::String ((int) v) + " %"; });
        mk ("aB", "backAbs", "Back", [] (float v) { return juce::String ((int) v) + " %"; }); mk ("bB", "rbBackAbs", "Back", [] (float v) { return juce::String ((int) v) + " %"; });
        mk ("aS", "splay", "Splay", [] (float v) { return juce::String ((int) v) + " %"; }); mk ("bS", "rbSplay", "Splay", [] (float v) { return juce::String ((int) v) + " %"; });
        for (auto* c : { &shapeA2, &shapeB2 }) { c->setItems ({ "Shoebox", "Hall (fan)", "Skewed", "Dome", "Connected", "Corridor" }); addChildComponent (*c); srcComps.push_back (c); }
        for (const auto& cs : { std::pair<ParamCombo*, const char*> { &shapeA2, "splay" }, std::pair<ParamCombo*, const char*> { &shapeB2, "rbSplay" } })
        {   // choosing a shape starts from a typical Splay: Dome = a hemisphere (100 %), Connected = a medium door, Corridor = a narrow arm
            ParamCombo* combo = cs.first; const char* splayId = cs.second; auto prev = combo->onChange;
            combo->onChange = [this, combo, splayId, prev] { if (prev) prev(); { const int id = combo->getSelectedId(); if (id == 4) proc.setParam (splayId, 100.0f); else if (id == 5) { proc.setParam (splayId, 40.0f); proc.setParam (std::string (splayId) == "splay" ? "backAbs" : "rbBackAbs", 60.0f); } else if (id == 6) proc.setParam (splayId, 30.0f); } };
        }
        juce::StringArray m; for (auto& x : er::materials()) m.add (juce::String::fromUTF8 (x.en));
        for (auto* c : { &wallA2, &floorA2, &wallB2, &floorB2 }) { c->setItems (m); addChildComponent (*c); srcComps.push_back (c); }
        for (auto* c : std::initializer_list<juce::Component*> { &copyABBtn, &copyBABtn, &ir2Load, &ir2Clear, &irAClear, &cmpB })
        { addChildComponent (*c); srcComps.push_back (c); c->getProperties().set ("fontPx", 11.0); }
        copyABBtn.setButtonText (ph.tr ("A をコピー", "Copy A")); copyABBtn.onClick = [this] { proc.undo.beginNewTransaction(); proc.copyRoom (true); repaint(); };
        copyBABtn.setButtonText (ph.tr ("B をコピー", "Copy B")); copyBABtn.onClick = [this] { proc.undo.beginNewTransaction(); proc.copyRoom (false); repaint(); };
        ir2Load.setButtonText (ph.tr ("IR読込", "Load IR")); ir2Load.onClick = [this] { showLoadMenu (1); };
        irAClear.setButtonText (ph.tr ("クリア", "Clear")); irAClear.onClick = [this] { proc.undo.beginNewTransaction(); proc.clearIR(); if ((int) ph.value ("src") == 2) ph.setGesture ("src", 1.0f); repaint(); };
        setHelp (irAClear, "Clear", "IR A を外します（ER が IR A を鳴らしていたら ALGO に戻ります）。", "Removes IR A (an ER playing IR A goes back to ALGO).");
        for (auto* c : std::initializer_list<juce::Component*> { &irBPrev, &irBNext }) { addChildComponent (*c); srcComps.push_back (c); }
        irBPrev.dir = -1; irBNext.dir = 1;
        irBPrev.onClick = [this] { const auto e = proc.stepIR (-1, 1); if (e.isNotEmpty()) flash ("IR B", e); repaint(); };
        irBNext.onClick = [this] { const auto e = proc.stepIR (1, 1); if (e.isNotEmpty()) flash ("IR B", e); repaint(); };
        cmpB.onClick = [this] { toggleCompare (1); };
        addChildComponent (cmpScope); srcComps.push_back (&cmpScope);
        cmpScope.items = { "ER", "TAIL", ph.tr ("全体", "ALL") };
        cmpScope.get = [this] { return cmpScopeV(); };
        cmpScope.set = [this] (int v) { proc.uiState.setProperty ("cmpScope", v, nullptr); dirty = true; repaint(); };
        setHelp (cmpScope, "Compare: ER / TAIL / ALL", "比べる部分。ER＝初期反射（エコーグラムの重ね表示と 7 項目のずれ）、TAIL＝帯域ごとの残響時間（RT グラフと測定に IR の T30 を重ねる）、全体＝両方と T30・EDT・C80 の差。",
                 "What to compare. ER = early reflections (echogram overlay and 7 distances); TAIL = reverb time per band (the IR's T30 on the RT graph and the measurement); ALL = both, plus the T30 / EDT / C80 differences.");
        ir2Clear.setButtonText (ph.tr ("クリア", "Clear")); ir2Clear.onClick = [this] { if (tIrClearBtn.onClick) tIrClearBtn.onClick(); repaint(); };
        setHelp (copyBABtn, "Copy B", "部屋 B の寸法・材質・形を A に写します。", "Copies room B's size, materials and shape into A.");
        setHelp (copyABBtn, "Copy A", "部屋 A の寸法・材質を B に写します（B を A から作り変える出発点）。", "Copies room A's size and materials into B (a starting point to reshape B).");
        setHelp (ir2Load, "IR B", "もう 1 つの IR（IR B）を読み込みます。IR A と同じく ER・TAIL・T-HEAD のどれにも送れます。", "Loads the second IR (IR B). Like IR A it can go to the ER, the TAIL or be the TAIL's head.");
        // the four source cards: [ER] [TAIL] ([T-HEAD] on the IRs) and "-> ALGO"
        for (int i = 0; i < 4; ++i)
        {
            for (auto* b : std::initializer_list<juce::Button*> { &sendEr[i], &sendTail[i], &toAlgo[i] }) { addChildComponent (*b); srcComps.push_back (b); b->getProperties().set ("fontPx", 11.0); }
            sendEr[i].setButtonText ("ER"); sendTail[i].setButtonText ("TAIL");
            toAlgo[i].setButtonText (juce::String::fromUTF8 ("\xe2\x86\x92 ALGO \xe2\x96\xbe"));
            sendEr[i].onClick = [this, i] { sendTo (i, 0); }; sendTail[i].onClick = [this, i] { sendTo (i, 1); };
            toAlgo[i].onClick = [this, i] { convertCard (i, 0); };
            toAlgo[i].onHold = [this, i]
            {
                juce::PopupMenu m; m.setLookAndFeel (&lnf);
                m.addItem (1, ph.tr ("ER だけ ALGO に", "ER only -> ALGO")); m.addItem (2, ph.tr ("TAIL だけ ALGO に", "TAIL only -> ALGO")); m.addItem (3, ph.tr ("ER と TAIL を ALGO に", "Both -> ALGO"));
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&toAlgo[i]), [this, i] (int r) { if (r > 0) convertCard (i, r); });
            };
            setHelp (sendEr[i], "ER", "ER（初期反射）をこのソースから作ります。点灯＝使用中、もう一度押すと消灯して ALGO に戻ります。ER は 1 つのソースだけ。", "Make the ER (early reflections) from this source. Lit = in use; press again to go back to ALGO. Only one source feeds the ER.");
            setHelp (toAlgo[i], "-> ALGO", "このソースの響きを ALGO のパラメーターに写します。押す＝点灯している送り先を変換（どれも点灯していなければ ER と TAIL の両方）。長押し／右クリックで ER だけ・TAIL だけを選べます。変換後はその送り先が ALGO になるので、ソースの ER／TAIL を押し直すと元の音と聴き比べられます。Undo で戻せます。",
                     "Writes this source's sound into ALGO parameters. Click = the lit destinations (both when none is lit); long-press / right-click picks ER only or TAIL only. The destination becomes ALGO afterwards: press the source's ER / TAIL again to compare with the original. Undo goes back.");
        }
        setHelp (sendTail[0], "TAIL", "TAIL（残響の本体）をこのソースから作ります。T-HEAD が点灯しているときは、頭のあとを受け持つソース（BODY）になります。", "Make the TAIL from this source. While a T-HEAD is lit this source is the BODY that takes over after it.");
        for (int i = 0; i < 2; ++i)
        {
            addChildComponent (sendHead[i]); srcComps.push_back (&sendHead[i]); sendHead[i].getProperties().set ("fontPx", 11.0); sendHead[i].setButtonText ("T-HEAD");
            sendHead[i].onClick = [this, i] { sendTo (2 + i, 2); };
            setHelp (sendHead[i], "T-HEAD", "TAIL の前半（頭）だけをこの IR で鳴らし、HEAD→BODY Time 以降は別のソース（ALGO・部屋・もう一方の IR）に引き継ぎます。IR のリアルな立ち上がり＋自由な長さ・明るさを両立できます。同じ IR で TAIL と T-HEAD は同時に使えません。",
                     "Plays only the first part (the head) of the TAIL from this IR; after HEAD -> BODY Time another source (ALGO, a room or the other IR) takes over: a real onset with a free length and tone. The same IR cannot be TAIL and T-HEAD at once.");
        }
    }
    focusL.o = this;
    for (auto* c : srcComps) c->addMouseListener (&focusL, true);
    for (auto* c : std::initializer_list<juce::Component*> { &irPrev, &irNext, &loadBtn, &cmpBtn }) c->addMouseListener (&focusL, true);
    setHelp (detEditBtn, "EDIT", "詳細パネル（INPUT｜ER｜TAIL｜COLOR・OUTPUT）を開く／閉じる。", "Opens / closes the detail panel (INPUT | ER | TAIL | COLOR / OUTPUT).");
    setHelp (detDynBtn, "DYN", "操作パネルの場所に、ダッキング・ゲートを出す／操作パネルに戻す。", "Shows the ducking and gate in place of the control deck / back to the deck.");
    setHelp (detModBtn, "MOD", "操作パネルの場所に、LFO・エンベロープ・コーラス・マトリクスを出す／操作パネルに戻す。", "Shows the LFO, envelope, chorus and matrix in place of the control deck / back to the deck.");
    // parameter lock: right-click a control to lock it; this button lists them
    addAndMakeVisible (lockBtn); lockBtn.getProperties().set ("fontPx", 11.0);
    lockBtn.onClick = [this] { showLockMenuAll(); };
    setHelp (lockBtn, "Lock", "ロックしたパラメーターは、プリセット・セクションプリセット・レッスンを読み込んでも今の値のまま。つまみや数値を右クリックでロック／解除。",
             "Locked parameters keep their values when presets, section presets or lessons load. Right-click a knob or a value to lock / unlock it.");
    setHelp (detSwitch, "Detail", "下の詳細パラメーターの画面：リバーブ（INPUT｜ER｜TAIL｜OUTPUT を同時に）／DYN・MOD。",
             "The detail panel's screens: REVERB (INPUT | ER | TAIL | OUTPUT at once) / DYN and MOD.");
    for (auto* t : { &colErBtn, &colTailBtn }) addAndMakeVisible (*t);
    setHelp (colErBtn, "Color on ER", "COLOR（Bandwidth・Bits・Wander）を ER にかける／かけない。", "Apply COLOR (Bandwidth, Bits, Wander) to the ER or not.");
    setHelp (colTailBtn, "Color on Tail", "COLOR の Bandwidth・Bits をテールにもかける／かけない（Wander は ER の反射を揺らすので ER だけ）。",
             "Apply COLOR's Bandwidth and Bits to the tail as well, or not (Wander moves the ER's reflections, so it is ER only).");
    secBtn.getProperties().set ("fontPx", 11.0);
    setHelp (secBtn, "Section preset", "いまのページの部分（ER／TAIL／I/O・インプットフィルター／MASTER EQ）だけを保存・読み込みます。ほかの部分はそのまま。例：好きなテールを保存しておき、別の ER と組み合わせる。",
             "Saves / loads only this page's part (ER / TAIL / I/O & input filter / MASTER EQ); the rest stays. E.g. keep a tail you like and pair it with another ER.");
    tIrBtn.onClick = [this] { loadTailIrFile(); };
    tIrClearBtn.onClick = [this] { proc.clearTailIR(); irtView.repaint(); flash ("IR", ph.tr ("テールは ER と同じ IR に戻りました。", "The tail uses the ER's IR again.")); };
    tIrBtn.getProperties().set ("fontPx", 11.0); tIrClearBtn.getProperties().set ("fontPx", 11.0);
    setHelp (tIrBtn, "Tail IR", "テールだけに別の IR を使います（ER はヘッダーの Load IR のまま）。例：ER は小さな部屋、テールは大きなホールの IR。読み込むとテールのソースを IR にします。",
             "Use a separate IR for the tail only (the ER keeps the header's Load IR). E.g. a small room's ER with a large hall's tail. Loading sets the tail source to IR.");
    setHelp (tIrClearBtn, "Same IR as ER", "テール用の IR を外して、ER と同じ IR に戻します。", "Removes the tail's own IR; the tail uses the ER's IR again.");
    // display modes
    addChildComponent (deck); addChildComponent (colorDeck); addChildComponent (fader); addChildComponent (modern); addChildComponent (viewBtn); addChildComponent (helpBtn);
    viewBtn.onClick = [this] { showViewMenu(); };
    // EDIT | Remote: EDIT is the full screen; the remotes (deck, '80s fader, modern) have a menu of their own
    for (auto* b : { &editBtn, &remoteBtn }) { addChildComponent (*b); b->getProperties().set ("fontPx", 11.0); }       // (no EDIT / remote screens any more: the buttons are never shown)
    basicBtn.setVisible (false);
    editBtn.setButtonText ("EDIT");
    editBtn.setConnectedEdges (juce::Button::ConnectedOnRight); remoteBtn.setConnectedEdges (juce::Button::ConnectedOnLeft);
    editBtn.onClick = [this] { setViewMode (0); syncViewButtons(); };
    remoteBtn.onClick = [this] { showRemoteMenu(); };
    setHelp (editBtn, "EDIT", "すべてのパラメーターとモニター（学習用の全画面）。", "Every parameter and the monitors (the full learning screen).");
    setHelp (remoteBtn, "Remote", "小さなリモコン画面：操作デッキ型／80 年代のフェーダー型／モダン型。戻るときは EDIT。", "Small remote screens: the deck type / the '80s fader type / the modern type. EDIT takes you back.");
    viewBtn.getProperties().set ("fontPx", 11.0);
    helpBtn.onClick = [this] { proc.uiState.setProperty ("simpleHelp", ! helpOn(), nullptr); if (onDesignChange) onDesignChange(); layoutMode(); repaint(); };
    helpBtn.getProperties().set ("fontPx", 11.0);
    setHelp (viewBtn, "View", "表示モード：FULL（学習用の全画面）／SIMPLE（つまみ一段＋グラフ）／REMOTE（小さなリモコン：80 年代フェーダー型、モダン型）。どのモードも同じパラメーターを動かします。",
             "Display mode: FULL (the whole lab) / SIMPLE (one deck of knobs + graphs) / REMOTE (small remotes: '80s fader type, modern type). Every mode moves the same parameters.");
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &srcPresetBox, &colorPresetBox, &irPrev, &irNext, &cmpBtn, &prevP, &nextP, &loadBtn, &exportBtn, &slopeBox, &lcSlopeBox, &panel, &now, &panelBtn, &langBtn, &undoBtn, &redoBtn, &zoom, &srcSeg,
                                                             &echo, &roomView, &info, &lesson, &help, &irMode, &irAuto, &irWet, &tailSeg, &autoSeg, &wallBox, &floorBox })
        addAndMakeVisible (c);
    for (auto* s : { &srcSeg, &irMode, &irAuto, &irWet, &tailSeg, &autoSeg, &erRoomSeg, &tRoomSegAB, &erIrSeg, &tIrSegAB }) if (auto* h = helpFor (s->paramID)) setHelp (*s, h->t, h->ja, h->en);
    for (auto* s : { &erRoomSeg, &tRoomSegAB, &erIrSeg, &tIrSegAB }) { addChildComponent (*s); s->onChange = [this] { layoutSource(); layoutTail(); repaint(); }; }
    addChildComponent (dmPreBtn); dmPreBtn.getProperties().set ("fontPx", 10.0);
    dmPreBtn.onClick = [this] { showSectionMenu (deckView() == 3 ? 6 : 5, dmPreBtn.getBounds()); };
    for (int k = 0; k < 2; ++k)
    {   // (0.10.8) SOURCE shows one ROOM and one IR card; this A | B chooses which (not which one is in use)
        auto& ab = k == 0 ? srcRoomAB : srcIrAB;
        ab.get = [this, k] { return srcShow (k); };
        ab.set = [this, k] (int v) { proc.uiState.setProperty (k == 0 ? "srcRoom" : "srcIr", v, nullptr); srcFocus = (k == 0 ? 0 : 2) + v; layoutSourceScreen(); layoutMonitors(); repaint(); };
        addChildComponent (ab); srcComps.push_back (&ab);
        setHelp (ab, k == 0 ? "ROOM A / B" : "IR A / B", "このカードに表示する方を選びます（どちらを ER・TAIL に使っているかは、カード下の［ER］［TAIL］と右の「いま鳴っているもの」で分かります）。",
                 "Chooses which one this card shows (which one feeds the ER / TAIL is shown by the [ER] [TAIL] buttons and by NOW PLAYING on the right).");
    }
    srcSeg.getProperties().set ("terms", "ROOM,ALGO,IR");
    for (auto* c : { &wallBox, &floorBox, &slopeBox, &lcSlopeBox }) if (auto* h = helpFor (c->paramID)) setHelp (*c, h->t, h->ja, h->en);

    // presets
    {
        fillPresetBox();
        presetBox.setTextWhenNothingSelected ("Preset");
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id >= 5000) { const auto U = proc.userPresets(); if (id - 5000 < U.size()) proc.loadUserPreset (U[id - 5000]); }
            else if (id >= 4000) showPresetMenuExtras (id);
            else if (id > 0 && id - 1 != proc.currentPreset) { proc.applyPreset (id - 1); proc.currentUserPreset = {}; }
        };
        auto stepP = [this] (int d)
        {   // the built-in presets in the menu's order, then the user presets (and round again)
            const auto& o = presetOrder(); const auto U = proc.userPresets(); const int n = (int) o.size(), m = U.size();
            int k = 0;
            if (proc.currentUserPreset.isNotEmpty())
            {
                k = n; for (int i = 0; i < m; ++i) { const auto nm = U[i].getFileNameWithoutExtension(); if (nm == proc.currentUserPreset || nm == juce::File::createLegalFileName (proc.currentUserPreset)) { k = n + i; break; } }
            }
            else if (proc.currentPreset >= 0) for (int i = 0; i < n; ++i) if (o[(size_t) i] == proc.currentPreset) k = i;
            const int tot = n + m, nk = ((k + d) % tot + tot) % tot;
            if (nk < n) proc.applyPreset (o[(size_t) nk]); else proc.loadUserPreset (U[nk - n]);
        };
        prevP.dir = -1; nextP.dir = 1;
        prevP.onClick = [stepP] { stepP (-1); }; nextP.onClick = [stepP] { stepP (1); };
        setHelp (presetBox, "Preset", "実際の空間（ROOM）と、ビンテージ・デジタル機の ER を参考にしたモデル（ALGO + COLOR）。モデル名は時代と用途で表しています。",
                 "Real spaces (ROOM) and models after the ER of vintage digital units (ALGO + COLOR). Models are named by era and use.");
    }
    loadBtn.onClick = [this] { showLoadMenu(); };
    irPrev.dir = -1; irNext.dir = 1;
    auto stepIR = [this] (int d) { const auto e = proc.stepIR (d); if (e.isNotEmpty()) flash ("IR", e); else { proc.undo.beginNewTransaction(); proc.setParam ("src", 2); } };
    irPrev.onClick = [stepIR] { stepIR (-1); }; irNext.onClick = [stepIR] { stepIR (1); };
    setHelp (irPrev, "Previous IR", "読み込んだリスト（複数のファイル、またはフォルダー）の前の IR。", "The previous IR of the loaded list (several files or a folder).");
    setHelp (irNext, "Next IR", "読み込んだリスト（複数のファイル、またはフォルダー）の次の IR。", "The next IR of the loaded list (several files or a folder).");
    cmpBtn.getProperties().set ("fontPx", 11.0);
    cmpBtn.onClick = [this] { toggleCompare (0); };
    for (auto* b : { &cmpBtn, &cmpB })
        setHelp (*b, "Compare", "このカードの IR と、いまのリバーブを比べる（ON／OFF）。比べる部分は右の ER／TAIL／全体で選びます。比べられるのは A か B のどちらか 1 つ。",
                 "Compares this card's IR with the reverb as it is now (on / off). Pick the part with ER / TAIL / ALL. One of A or B at a time.");
    srcPresetBox.setTextWhenNothingSelected ("Source preset");
    srcPresetBox.onChange = [this] { if (const int id = srcPresetBox.getSelectedId(); id > 0) proc.applySourcePreset (id - 1); };
    setHelp (srcPresetBox, "Source preset", "SOURCE の枠のパラメーターだけを読み込みます（VOICE・COLOR・OUTPUT はそのまま）。ROOM は実際の部屋、ALGO はクラシック機の ER の形。",
             "Loads only the SOURCE parameters (VOICE, COLOR and OUTPUT stay). ROOM: real rooms; ALGO: the ER shapes of the classic units.");
    for (int i = 0; i < (int) colorPresets().size(); ++i) colorPresetBox.addItem (colorPresets()[(size_t) i].name, i + 1);
    colorPresetBox.setTextWhenNothingSelected ("Era preset");
    colorPresetBox.onChange = [this] { if (const int id = colorPresetBox.getSelectedId(); id > 0) proc.applyColorPreset (id - 1); };
    setHelp (colorPresetBox, "Color preset", "デジタル機の時代ごとの帯域・ビット・揺らぎ。枠の色は今の値の「古さ」：青＝現代、緑＝80年代の高級機、黄〜橙＝80年代、赤茶＝70年代・ローファイ。",
             "Band, bits and wander of each digital era. The frame colour shows how old the current values are: blue = modern, teal = 80s flagship, amber = 80s, red-brown = 70s / lo-fi.");
    exportBtn.onClick = [this] { showExportMenu(); };
    addAndMakeVisible (lookBtn); lookBtn.getProperties().set ("fontPx", 11.0);
    lookBtn.onClick = [this]
    {
        auto lp = std::make_unique<LookPanel> (ph.ja()); lp->onChange = [this] { applyLookAdjust(); };
        juce::CallOutBox::launchAsynchronously (std::move (lp), lookBtn.getScreenBounds(), nullptr);
    };
    setHelp (lookBtn, "Look", "文字の濃さ・全体の明るさ・コントラストを調整するつまみを開きます（ダブルクリックで 0 に戻る）。画面の見やすさを、お使いのモニターに合わせます。",
             "Opens knobs for text weight, overall brightness and contrast (double-click a knob to reset it). Tune the screen to your monitor.");
    for (auto* b : { &loadBtn, &ir2Load, &dmPreBtn }) b->getProperties().set ("comboLook", true);
    slopeBox.setItems ({ "6", "12", "18", "24", "36", "48" });
    lcSlopeBox.setItems ({ "6", "12", "18", "24", "36", "48" });
    panelBtn.setClickingTogglesState (false); panelBtn.getProperties().set ("fontPx", 11.0);
    panelBtn.onClick = [this] { proc.uiState.setProperty ("panelOn", ! (bool) proc.uiState.getProperty ("panelOn", true), nullptr); resized(); };
    setHelp (panelBtn, "Faceplate", "実機パネルの表示を切り替えます。", "Show or hide the hardware faceplate.");
    setHelp (now, "What you hear now", "いま鳴っている ER の特徴と、直前に動かしたパラメーターで音がどう変わったかを言葉で説明します（グラフの値から自動で書いています）。",
             "Describes the ER you hear now and what the last change did to the sound, in words (written automatically from the graphs).");
    setHelp (exportBtn, "Export IR", "いまの ER を IR（WAV）として書き出します。直接音の有無、Tail Preview を含めるか、サンプルレート、ビット深度を選べます。コンボリューションリバーブや他の DAW でそのまま使えます。",
             "Exports the current ER as an IR (WAV): with or without the direct sound, with or without the Tail Preview, at the sample rate and bit depth you choose. Use it in any convolution reverb.");
    info.loadBtn.onClick = [this] { showLoadMenu(); };
    langBtn.onClick = [this] { proc.uiState.setProperty ("lang", ph.ja() ? "en" : "ja", nullptr); };
    undoBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb6")); redoBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb7"));
    undoBtn.onClick = [this] { proc.undo.undo(); }; redoBtn.onClick = [this] { proc.undo.redo(); };
    setHelp (undoBtn, "Undo", "元に戻す", "Undo"); setHelp (redoBtn, "Redo", "やり直す", "Redo");
    setHelp (loadBtn, "Load IR", "IR を読み込みます（WAV / AIFF / FLAC）。読み込むとソースが IR になり、他のソースでもターゲットとして重ねて表示されます。",
             "Load an IR (WAV / AIFF / FLAC). The source switches to IR; in the other sources it stays as the target overlay.");

    auto ms = [] (float v) { return fmtMs (v); };
    auto mm = [] (float v) { return juce::String (v, 1) + " m"; };
    // ROOM
    roomSet = { &dv ("roomW", "Width", mm), &dv ("roomD", "Depth", mm), &dv ("roomH", "Height", mm), &dv ("order", "Order", [] (float v) { return juce::String ((int) v); }), &wallBox, &floorBox };
    { juce::StringArray m; for (auto& x : er::materials()) m.add (juce::String::fromUTF8 (x.en)); wallBox.setItems (m); floorBox.setItems (m); }
    // ALGO
    patSet = { &dv ("first", "First", ms), &dv ("length", "Length", ms), &dv ("taps", "Reflections", [] (float v) { return juce::String ((int) v); }),
               &dv ("growth", "Growth", [] (float v) { return juce::String (v, 2); }), &dv ("vari", "Variation", [] (float v) { return "#" + juce::String ((int) v); }),
               &dv ("energy", "Energy", [] (float v) { return fmtDb (v); }), &dv ("decay", "Envelope", [] (float v) { return fmtDb (v); }),
               &dv ("damp", "Damping", [] (float v) { return fmtHz (v); }),
               &dv ("roomEarly", "ROOM Early", [] (float v) { return v < 0.5f ? juce::String ("Off") : juce::String ((int) std::round (v)) + " ms"; }) };
    // IR
    irSet = { &dv ("irWin", "IR Window", ms), &dv ("irThr", "Threshold", [] (float v) { return fmtDb (v, 0); }), &irMode, &irAuto, &irWet, &dv ("irSplit", "Split", ms), &dv ("irFill", "Fill", pct) };
    for (auto id : { "irSplit", "irFill" }) vals[id]->dimmed = [this] { return (int) ph.value ("irMode") != 2; };
    roomSet.push_back (&r2pBtn); addAndMakeVisible (r2pBtn); r2pBtn.getProperties().set ("fontPx", 11.5);
    r2pBtn.onClick = [this] { proc.roomToPattern (false); r2pBtn.setButtonText (ph.tr ("変換中…", "Converting...")); };
    roomSet.push_back (&r2pHybBtn); addAndMakeVisible (r2pHybBtn); r2pHybBtn.getProperties().set ("fontPx", 11.5);
    r2pHybBtn.onClick = [this] { proc.roomToPattern (true); r2pHybBtn.setButtonText (ph.tr ("変換中…", "Converting...")); };
    setHelp (r2pHybBtn, "ROOM -> ALGO (hybrid)", "ROOM → ALGO と同じ変換に加えて、最初の 20〜40 ms は部屋の反射をそのまま残します（ALGO の ROOM Early。20 / 30 / 40 ms のうち一致度が最も高い長さ）。ALGO はその後ろにつながるように合わせます。部屋らしい最初の数本と、ALGO の編集しやすい後半を組み合わせます。",
             "The same as ROOM -> ALGO, but the first 20-40 ms keep the room's own reflections (ALGO's ROOM Early: whichever of 20 / 30 / 40 ms matches best); the ALGO is fitted to follow them. The room's first few reflections with an easily edited ALGO after them.");
    lesson.onEar = [this] { startEar(); };
    ear.onEnd = [this] { endEar(); };
    addChildComponent (ear); addChildComponent (earMask);
    // ===== reverb pages =====
    {
        auto secs = [] (float v) { return v < 10 ? juce::String (v, 2) + " s" : juce::String (v, 1) + " s"; };
        auto mult = [] (float v) { return juce::String (v, 2) + "x"; };
        auto& d1 = dv ("tDecay", "Decay", secs); auto& d2 = dv ("tSize", "Size", ms); auto& d3 = dv ("tPre", "Tail Delay", ms); auto& d4 = dv ("tDiff", "Diffusion", pct);
        auto& d5 = dv ("tModD", "Mod Depth", pct); auto& d6 = dv ("tModR", "Mod Rate", [] (float v) { return juce::String (v, 2) + " Hz"; });
        auto& d7 = dv ("mbN", "MB Bands", [] (float v) { return juce::String ((int) v); });
        vals["mbN"]->dimmed = [this] { return (int) ph.value ("rtMode") != 1; };
        auto& g1 = dv ("aGrav", "Gravity", [] (float v) { return juce::String ((int) std::round (v)) + " %"; });
        auto& g2 = dv ("aFb", "Feedback", pct);
        pageComps[1] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &freezeSeg, &rtModeSeg, &tEngSeg, &g1, &g2 };
        auto& i1 = dv ("inW", "Input Width", pct);
        auto& i2 = dv ("iLC", "Low Cut", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHz (v); });
        auto& i3 = dv ("iHC", "High Cut", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHz (v); });
        auto& i4 = dv ("iTilt", "Tilt", [] (float v) { return fmtDb (v); });
        vals["inW"]->dimmed = [this] { return (int) ph.value ("inMode") != 0; };
        pageComps[3] = { &i1, &i2, &i3, &i4, &inModeBox, &outModeSeg, &iLCsBox, &iHCsBox, &iToSeg };
        pageComps[4] = { &eqOnSeg, &eqFlatBtn };
        {   // 0.6.0: DYN
            auto db0 = [] (float v) { return fmtDb (v, 0); };
            auto& k1 = dv ("dkDepth", "Depth", [] (float v) { return fmtDb (-v, 1); }); auto& k2 = dv ("dkThr", "Threshold", db0);
            auto& k3 = dv ("dkAtk", "Attack", ms); auto& k4 = dv ("dkRel", "Release", ms);
            auto& k5 = dv ("dkBands", "Bands", [] (float v) { return juce::String ((int) v); });
            vals["dkBands"]->dimmed = [this] { return (int) ph.value ("dkMode") != 2; };
            for (auto* id : { "dkDepth", "dkThr", "dkAtk", "dkRel" }) vals[id]->dimmed = [this] { return (int) ph.value ("dkMode") == 0; };
            auto& q1 = dv ("gtThr", "Threshold", db0); auto& q2 = dv ("gtHold", "Hold", ms); auto& q3 = dv ("gtRel", "Release", ms);
            for (auto* id : { "gtThr", "gtHold", "gtRel" }) vals[id]->dimmed = [this] { return ph.value ("gtOn") < 0.5f; };
            for (auto* c : { &dkModeSeg, &dkToSeg, &gtOnSeg, &gtKeySeg }) { addAndMakeVisible (*c); if (auto* h = helpFor (c->paramID)) setHelp (*c, h->t, h->ja, h->en); }
            dkModeSeg.getProperties().set ("terms", "DUCKING,SINGLE,MULTI,SPECTRAL");
            addAndMakeVisible (dynMeter);
            setHelp (dynMeter, "Ducking / gate meter", "いまのダッキング量（帯域ごと、dB）とゲートの開き具合。Single は 1 本、Multi は 2〜5 帯域、Spectral は半オクターブ 17 帯域。",
                     "How much the reverb is ducked now (per band, dB) and how open the gate is. Single: one bar, Multi: 2-5 bands, Spectral: 17 half-octave bands.");
            pageComps[5] = { &k1, &k2, &k3, &k4, &k5, &q1, &q2, &q3, &dkModeSeg, &dkToSeg, &gtOnSeg, &gtKeySeg, &dynMeter };
        }
        {   // 0.6.0: MOD
            auto hz = [] (float v) { return v < 1 ? juce::String (v, 2) + " Hz" : juce::String (v, 1) + " Hz"; };
            auto& a1 = dv ("l1Rate", "LFO 1 Rate", hz); auto& a2 = dv ("l2Rate", "LFO 2 Rate", hz);
            vals["l1Rate"]->dimmed = [this] { return (int) ph.value ("l1Sync") > 0; }; vals["l2Rate"]->dimmed = [this] { return (int) ph.value ("l2Sync") > 0; };
            auto& e1 = dv ("enAtk", "Env Attack", ms); auto& e2 = dv ("enRel", "Env Release", ms); auto& e3 = dv ("enSens", "Env Sens", [] (float v) { return fmtDb (v, 0); });
            auto& c1 = dv ("chMix", "Chorus", pct); auto& c2 = dv ("chRate", "Ch Rate", hz); auto& c3 = dv ("chDepth", "Ch Depth", ms);
            for (auto* id : { "chRate", "chDepth" }) vals[id]->dimmed = [this] { return ph.value ("chMix") <= 0; };
            pageComps[6] = { &a1, &a2, &e1, &e2, &e3, &c1, &c2, &c3, &modScope };
            const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H", "Random" };
            const juce::StringArray syncs { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T", "1/4 D", "1/8 D" };
            mbox ("l1Shape").setItems (shapes); mbox ("l1Sync").setItems (syncs); mbox ("l2Shape").setItems (shapes); mbox ("l2Sync").setItems (syncs);
            for (int i = 1; i <= 4; ++i)
            {
                const juce::String n (i);
                mbox (("m" + n + "Src").toRawUTF8()).setItems ({ "Off", "LFO 1", "LFO 2", "Env" });
                mbox (("m" + n + "Dst").toRawUTF8()).setItems ({ "Mix", "Width", "Out Level", "Tail Level", "ER Level", "Decay", "Treble Decay", "Chorus Depth", "Duck Depth", "Gate Thr" });
                auto& am = dv (("m" + n + "Amt").toRawUTF8(), "Amount", [] (float v) { return (v > 0.5f ? "+" : "") + juce::String ((int) std::round (v)) + " %"; });
                vals[("m" + n + "Amt").toRawUTF8()]->dimmed = [this, n] { return (int) ph.value (("m" + n + "Src").toRawUTF8()) == 0; };
                pageComps[6].push_back (&am);
            }
            for (auto& b : modBoxes) pageComps[6].push_back (b.get());
            addAndMakeVisible (modScope);
            setHelp (modScope, "Modulation sources", "LFO 1・LFO 2（−1〜+1）とエンベロープ（入力の大きさ、0〜1）のいまの動き。マトリクスの 4 つのスロットで、これらを行き先へ量（±%）を決めて送ります。",
                     "What LFO 1, LFO 2 (-1..+1) and the envelope (the input's level, 0..1) are doing now. The matrix's 4 slots send them to destinations by an amount (+/- %).");
        }
        {   // IR TAIL
            auto& a1 = dv ("irtX", "Tail Start", ms); auto& a2 = dv ("irtLen", "Length", [] (float v) { return juce::String (v, 2) + " s"; });
            auto& a3 = dv ("irtSt", "Stretch", pct); auto& a4 = dv ("irtLvl", "IR Level", [] (float v) { return fmtDb (v); });
            auto xs = [] (float v) { return juce::String (v, 2) + "x"; };
            auto& a5 = dv ("irtDL", "Decay Low", xs); auto& a6 = dv ("irtDM", "Decay Mid", xs); auto& a7 = dv ("irtDH", "Decay High", xs);
            auto& a8 = dv ("irtB", "IR -> ALGO", ms);
            vals["irtB"]->dimmed = [this] { return (int) ph.value ("tSrc") != 2; };
            // IR TAIL is part of the TAIL page now (SOURCE: IR / IR+ALGO), like the ER's sources
            for (auto* c : std::initializer_list<juce::Component*> { &a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8, &irtRevSeg, &irtAutoSeg, &fdnBtn, &irtView, &tIrBtn, &tIrClearBtn, &tSrcSeg, &hybBtn, &roomAlgoBtn })
                pageComps[1].push_back (c);
            addChildComponent (hybBtn); addChildComponent (roomAlgoBtn);
            hybBtn.getProperties().set ("fontPx", 11.0); roomAlgoBtn.getProperties().set ("fontPx", 12.0);
            hybBtn.onClick = [this] { proc.uiState.setProperty ("tEdit", (int) proc.uiState.getProperty ("tEdit", 0) == 0 ? 1 : 0, nullptr); layoutTail(); repaint(); };
            setHelp (hybBtn, "IR+ALGO: edit", "IR+ALGO のとき、SOURCE 欄に IR と ALGO のどちらの操作を出すかを切り替えます（どちらも鳴っています）。", "With IR+ALGO: which side's controls the SOURCE group shows (both are playing).");
            roomAlgoBtn.onClick = [this]
            {
                const auto rep = proc.roomToAlgo();
                flash ("ROOM -> ALGO", ph.tr ("部屋の残響時間（トリム込み）を ALGO の Decay と 5 つの点・Size に写し、ALGO に切り替えました：", "The room's reverb time (with the trim) went to ALGO's Decay, the 5 points and Size, now ALGO: ") + rep);
                layoutTail(); repaint();
            };
            setHelp (roomAlgoBtn, "ROOM -> ALGO", "いま部屋から計算している帯域ごとの残響時間を、ALGO の Decay と 5 つの点（EQ モード）と Size に写して ALGO に切り替えます。部屋を出発点に、自由に作り変えるための操作です。Undo で戻せます。",
                     "Copies the reverb time per band now computed from the room into ALGO's Decay, its 5 points (EQ mode) and Size, then switches to ALGO: the room as a starting point to reshape freely. Undo goes back.");
            tSrcSeg.onChange = [this] { layoutTail(); repaint(); };
            fdnBtn.setButtonText (juce::String::fromUTF8 ("IR \xe2\x86\x92 ALGO")); fdnBtn.getProperties().set ("fontPx", 12.0);
            fdnBtn.onClick = [this]
            {
                const auto rep = proc.irToFdn();
                flash ("IR -> ALGO", rep == "no IR" ? ph.tr ("IR を読み込んでください。", "Load an IR first.")
                                                   : ph.tr ("IR の帯域ごとの減衰（T30）を ALGO テールに写し、ALGO に切り替えました（IR / ALGO）：", "The IR's decay per band (T30) went to the ALGO tail, now ALGO (IR / ALGO): ") + rep);
            };
            setHelp (fdnBtn, "IR -> ALGO", "IR の帯域ごとの残響時間（T30）を測って、ALGO テール（FDN）の Decay と 5 つの点に写し、Size（混合時間から）と音量も合わせて ALGO に切り替えます。IR の響きを、編集・揺らせるアルゴリズムに翻訳する操作です。Undo で戻せます。",
                     "Measures the IR's reverb time per band (T30) and copies it to the ALGO tail (FDN): Decay and the 5 points, Size (from the mixing time) and level, then switches to ALGO. It translates the IR into an algorithm you can edit and modulate. Undo goes back.");
        }
        // the value boxes of the selected point (shown one point at a time, next to it)
        auto hz = [] (float v) { return fmtHz (v); }; auto q2 = [] (float v) { return juce::String (v, 2); };
        auto x2 = [] (float v) { return juce::String ((int) std::round (v * 100)) + " %"; };      // RT as % of Decay
        for (int i = 1; i <= 6; ++i)
        {
            const juce::String n (i);
            for (auto id : { "eqF" + n, "eqG" + n, "eqQ" + n }) popIds.push_back (id);
            dv (("eqF" + n).toRawUTF8(), "Freq", hz); dv (("eqG" + n).toRawUTF8(), "Gain", [] (float v) { return fmtDb (v); }); dv (("eqQ" + n).toRawUTF8(), "Q", q2);
        }
        dv ("eqHP", "High Pass", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHz (v); }); popIds.push_back ("eqHP");
        dv ("eqLP", "Low Pass", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHz (v); }); popIds.push_back ("eqLP");
        for (int i = 1; i <= 5; ++i)
        {
            const juce::String n (i);
            dv (("rtF" + n).toRawUTF8(), "Freq", hz); dv (("rtM" + n).toRawUTF8(), "% Decay", x2); dv (("rtQ" + n).toRawUTF8(), "Q", q2);
            dv (("mbM" + n).toRawUTF8(), "% Decay", x2);
            for (auto id : { "rtF" + n, "rtM" + n, "rtQ" + n, "mbM" + n }) popIds.push_back (id);
            if (i <= 4) { dv (("mbX" + n).toRawUTF8(), "Crossover", hz); popIds.push_back ("mbX" + n); }
        }
        for (auto& id : popIds) vals[id]->setVisible (false);
        addChildComponent (popup);
        rtGraph.onSelect = [this] { updatePopup(); }; eqGraph.onSelect = [this] { updatePopup(); }; combo.onSelect = [this] { updatePopup(); }; combo.onAddType = [this] { tools.repaint(); };
        // right-click on the small window (or its value boxes) closes it
        popCloser.fn = [this] { if (auto* gp = activeGraph()) { gp->sel = -1; gp->repaint(); } popup.setVisible (false); for (auto& id : popIds) vals[id]->setVisible (false); updatePopup(); };
        popup.addMouseListener (&popCloser, true);
        for (auto& id : popIds) vals[id]->addMouseListener (&popCloser, true);
        for (auto* id : { "eqF1", "eqG1", "eqQ1", "eqF2", "eqG2", "eqQ2", "eqF3", "eqG3", "eqQ3", "eqF4", "eqG4", "eqQ4", "eqF5", "eqG5", "eqQ5", "eqF6", "eqG6", "eqQ6", "eqHP", "eqLP" })
            if (vals.count (id)) vals[id]->addMouseListener (&popCloser, true);
        inGraph.onSelect = [this] { updatePopup(); }; erfGraph.onSelect = [this] { updatePopup(); }; tfGraph.onSelect = [this] { updatePopup(); };
        if ((int) proc.uiState.getProperty ("selRT", -1) >= 0) combo.sel = (int) proc.uiState.getProperty ("selRT", -1);     // (screen tests)
        if ((int) proc.uiState.getProperty ("selEQ", -1) >= 0) combo.sel = rtGraph.count() + (int) proc.uiState.getProperty ("selEQ", -1);
        dv ("outW", "Width", pct); dv ("tLvl", "Tail Level", [] (float v) { return fmtDb (v); });
        pageComps[1].push_back (vals["tLvl"].get());       // the tail's own level lives on the TAIL page
        {   // 0.6.1: build-up and the low width
            auto& b1 = dv ("tSpread", "Spread", ms); auto& b2 = dv ("tShape", "Shape", [] (float v) { return std::abs (v) < 0.5f ? juce::String ("Flat") : (v > 0 ? "Rise " : "Fall ") + juce::String ((int) std::round (std::abs (v))) + "%"; });
            auto& b3 = dv ("tBassW", "Bass Width", pct);
            auto& b4 = dv ("tFill", "Onset Fill", pct); pageComps[1].push_back (&b4);
            auto& b5 = dv ("tSpreadHi", "Spread High", pct); pageComps[1].push_back (&b5);
            auto& f1 = dv ("tLC", "Low Cut", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHz (v); });
            auto& f2 = dv ("tHC", "High Cut", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHz (v); });
            pageComps[1].push_back (&f1); pageComps[1].push_back (&f2);
            for (auto* c : { &tLCsBox, &tHCsBox }) { addAndMakeVisible (*c); pageComps[1].push_back (c); }
            const juce::StringArray sl2 { "6", "12", "18", "24", "36", "48" };
            tLCsBox.setItems (sl2); tHCsBox.setItems (sl2);
            setHelp (*vals["tLC"], "Tail Low Cut", "テールだけのローカット（ER はそのまま）。低域のにごりを残響からだけ取る。", "A low cut on the tail only (the ER stays). Clears mud from the reverb alone.");
            setHelp (*vals["tHC"], "Tail High Cut", "テールだけのハイカット。残響を奥へ・暗く。", "A high cut on the tail only: the reverb further back and darker.");
            setHelp (tLCsBox, "Tail LC slope", "テールのローカットの傾き（dB/oct）。", "The tail low cut's slope (dB/oct).");
            setHelp (tHCsBox, "Tail HC slope", "テールのハイカットの傾き（dB/oct）。", "The tail high cut's slope (dB/oct).");
            vals["tSpreadHi"]->dimmed = [this] { return ph.value ("tSpread") < 1.0f; };
            vals["tFill"]->dimmed = [this] { return (int) ph.value ("tEng") != 0 || (int) ph.value ("tSrc") == 1; };
            vals["tShape"]->dimmed = [this] { return ph.value ("tSpread") < 1.0f; };
            pageComps[1].push_back (&b1); pageComps[1].push_back (&b2); pageComps[1].push_back (&b3);
        }
        dv ("erBal", "ER / TAIL", [] (float v) { const int b = (int) std::round (v); return b == 0 ? juce::String ("Equal") : b < 0 ? "ER +" + juce::String (-b) : "TAIL +" + juce::String (b); });
        (void) mult;
        for (auto* c : std::initializer_list<juce::Component*> { &irtView, &tSrcSeg, &irtRevSeg, &irtAutoSeg, &fdnBtn }) addAndMakeVisible (c);
        if (auto* h = helpFor ("irtAuto")) setHelp (irtAutoSeg, h->t, h->ja, h->en);
        if (auto* h = helpFor ("irtRev")) setHelp (irtRevSeg, h->t, h->ja, h->en);
        if (auto* h = helpFor ("tSrc")) setHelp (tSrcSeg, h->t, h->ja, h->en);
        tSrcSeg.getProperties().set ("terms", "ROOM,ALGO,IR,IR+ALGO");
        tEngSeg.getProperties().set ("terms", "HALL,PLATE,SPRING,AMBIENT"); rtModeSeg.getProperties().set ("terms", "EQ,MULTIBAND");
        // the monitors: the spectrum with the curve being edited, and the echogram; they swap places with the work
        layerBar.onChange = [this] { layoutMonitors(); updatePopup(); repaint(); };
        swapBtn.onClick = [this] { proc.uiState.setProperty ("monSwap", ! (bool) proc.uiState.getProperty ("monSwap", false), nullptr); layoutMonitors(); updatePopup(); repaint(); };
        swapBtn.getProperties().set ("fontPx", 11.0);
        setHelp (swapBtn, "Monitors", "メインモニター（大）とサブモニター（小）の中身を入れ替えます。ページを変えると、そのページの作業に合う並びに戻ります（ER ではエコーグラムが大、それ以外はスペアナが大）。",
                 "Swaps the main (large) and sub (small) monitors. Changing the page goes back to the layout for that page's work (ER: the echogram large; the others: the spectrum large).");
        setHelp (layerBar, "Spectrum + edit", "入力（インプットフィルターの前、橙）とリバーブ出力（水色）のスペアナの上で、どの曲線を編集するかを選びます：インプットフィルター → ER フィルター → 周波数ごとの RT → MASTER EQ（信号の順）。",
                 "Over the spectra of the input (before the input filter, amber) and the reverb output (cyan), choose which curve to edit: input filter -> ER filter -> RT per frequency -> master EQ (signal order).");
        setHelp (erfGraph, "ER filter", "ER にかかるローカット・ハイカット（Butterworth）。L・H を左右にドラッグで周波数、ホイールで傾き（6〜48 dB/oct）。値は ER ページの VOICE にもあります。",
                 "The low / high cut on the ER (Butterworth). Drag L / H sideways for the frequency, wheel for the slope (6-48 dB/oct). The values are also in the ER page's VOICE.");
        setHelp (tfGraph, "Tail filter", "TAIL にかかるローカット・ハイカット（Butterworth）。L・H を左右にドラッグで周波数、ホイールで傾き（6〜48 dB/oct）。値は TAIL ページの VOICE にもあります。",
                 "The low / high cut on the tail (Butterworth). Drag L / H sideways for the frequency, wheel for the slope (6-48 dB/oct). The values are also in the TAIL page's VOICE.");
        setHelp (inGraph, "Input filter", "リバーブに入る前のフィルター（ドライには効きません）。L・H を左右＝周波数、ホイール＝傾き、T を上下＝Tilt（1 kHz 中心）。",
                 "The filter before the reverb (not on the dry). L / H sideways = frequency, wheel = slope, T up / down = tilt (around 1 kHz).");
        eqFlatBtn.onClick = [this] { flatEQ(); };
        tools.onRtFlat = [this] { flatRT(); }; tools.onEqFlat = [this] { flatEQ(); };
        addChildComponent (tools);
        setHelp (tools, "Monitor tools", "追加：何もない所をダブルクリックしたときに足す点（EQ のバンドか RT の点）。点は使うまで表示しません（ダブルクリックで点を消す）。RT フラット＝全帯域 100 %、EQ フラット＝全バンド 0 dB・ローカット／ハイカットなし。↶↷＝取り消し／やり直し。EQ の目盛りは右上の値をクリックで選べます。",
                 "Add: what a double-click on an empty spot adds (an EQ band or an RT point). Points stay hidden until used (double-click a point to remove it). RT flat = 100 % everywhere, EQ flat = every band 0 dB, no low / high cut. Undo / redo. Click the EQ scale's top value to choose the scale.");
        for (auto* c : std::initializer_list<juce::Component*> { &erfGraph, &tfGraph, &combo, &layerBar, &swapBtn, &eqFlatBtn }) addAndMakeVisible (c);
        setHelp (combo, "RT + EQ", "周波数ごとのリバーブタイム（青、左の目盛り＝Decay に対する %）とマスター EQ（黄、右の目盛り＝dB）を同じ画面で。RT の 100 % は上から 1/4、EQ の 0 dB は中央。青い四角＝測った T30、白い丸＝EDT。右クリックで小窓を閉じます。",
                 "Reverb time per frequency (blue, left scale = % of Decay) and the master EQ (yellow, right scale = dB) on one plot. RT 100 % sits a quarter down, EQ 0 dB in the middle. Blue squares = measured T30, white circles = EDT. Right-click closes the small window.");
        for (auto* c : std::initializer_list<juce::Component*> { &tabs, &rtGraph, &inGraph, &eqGraph, &measure, &tEngSeg, &rtModeSeg, &freezeSeg, &erOnSeg, &tOnSeg, &outModeSeg, &iToSeg, &eqOnSeg, &inModeBox, &iLCsBox, &iHCsBox })
            addAndMakeVisible (c);
        for (auto* sg : { &tEngSeg, &rtModeSeg, &freezeSeg, &erOnSeg, &tOnSeg, &outModeSeg, &iToSeg, &eqOnSeg }) if (auto* h = helpFor (sg->paramID)) setHelp (*sg, h->t, h->ja, h->en);
        for (auto* c : { &inModeBox, &iLCsBox, &iHCsBox }) if (auto* h = helpFor (c->paramID)) setHelp (*c, h->t, h->ja, h->en);
        setHelp (rtGraph, "Reverb time", "周波数ごとのリバーブタイム（RT60）。水色の線が設定、橙の点が実際に測った T30、白い丸が EDT（最初の 10 dB の減り方。聞こえる響きの長さに近い）。",
                 "Reverb time (RT60) per frequency. The cyan line is the setting, the orange dots the measured T30, the white circles the EDT (the first 10 dB of decay, close to the length you hear).");
        setHelp (measure, "Measured", "いまの設定のリバーブ全体（ER＋テール＋マスター EQ）にインパルスを入れて測った値。T30＝−5〜−35 dB の傾きから求めた残響時間、EDT＝最初の 10 dB、C80＝最初の 80 ms とそれ以降のエネルギー比（明瞭さ、+ほど明瞭）、D50＝最初の 50 ms の割合（言葉の明瞭さ）、IACC＝左右の似かた（小さいほど広い）。",
                 "Values measured from an impulse through the whole reverb as set (ER + tail + master EQ). T30 = reverb time from the -5 to -35 dB slope, EDT = the first 10 dB, C80 = energy before / after 80 ms (clarity, higher = clearer), D50 = share of the first 50 ms (speech clarity), IACC = left / right similarity (lower = wider).");
        if (! proc.uiState.hasProperty ("layer")) proc.uiState.setProperty ("layer", layerFor (page()), nullptr);
        tabs.onChange = [this]
        {
            proc.uiState.setProperty ("monSwap", false, nullptr);
            layerBar.setLayer (layerFor (page()));
            applyPage();
        };
        inModeBox.setItems ({ "Stereo", "Mono (L+R)", "Left only", "Right only" });
        const juce::StringArray sl { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
        iLCsBox.setItems ({ "6", "12", "18", "24", "36", "48" }); iHCsBox.setItems ({ "6", "12", "18", "24", "36", "48" }); (void) sl;
    }
    setHelp (r2pBtn, "ROOM -> ALGO", "いまの部屋の ER に合わせて ALGO の 8 つのパラメーターを求め、ALGO に切り替えます（VOICE・COLOR はそのまま）。部屋の響きを、ビンテージ機のような少ないパラメーターでどこまで描けるかを確かめられます。Undo で戻せます。",
             "Fits the eight ALGO parameters to the current room's ER and switches to ALGO (VOICE and COLOR stay). Shows how far a room can be drawn with the few parameters of a vintage unit. Undo goes back.");
    vals["irWin"]->dimmed = [this] { return ph.value ("irAuto") > 0.5f && false; };
    // VOICE / COLOR / OUT
    dv ("predelay", "Predelay", ms); dv ("size", "Size", pct); dv ("width", "Width", pct); dv ("diffusion", "Diffusion", pct);
    dv ("darken", "Darkening", [] (float v) { return v < 0.005f ? juce::String ("Off") : juce::String (v, 2) + " oct"; });
    vals["darken"]->dimmed = [this] { return (int) ph.value ("src") == 2 && (int) ph.value ("irMode") == 1; };
    dv ("lowcut", "Low Cut", [] (float v) { return v <= 20.5f ? juce::String ("Off") : fmtHz (v); });
    dv ("highcut", "High Cut", [] (float v) { return v >= 19900 ? juce::String ("Off") : fmtHz (v); });
    dv ("level", "ER Level", [] (float v) { return fmtDb (v); });
    dv ("bw", "Bandwidth", [] (float v) { return v >= 19.9f ? juce::String ("Off") : juce::String (v, 1) + " kHz"; });
    dv ("bits", "Bits", [] (float v) { return v >= 23.5f ? juce::String ("Off") : juce::String ((int) std::round (v)) + " bit"; });
    dv ("wander", "Wander", pct);
    dv ("mix", "Mix", pct); dv ("out", "Output", [] (float v) { return fmtDb (v); });
    dv ("tailRT", "Tail Decay", [] (float v) { return juce::String (v, 2) + " s"; }); dv ("tailLvl", "Tail Level", [] (float v) { return fmtDb (v); });
    vals["size"]->dimmed = [this] { return (int) ph.value ("src") == 2 && (int) ph.value ("irMode") == 1; };
    for (auto id : { "tailRT", "tailLvl" }) vals[id]->dimmed = [this] { return ph.value ("tail") < 0.5f; };

    // ---- EDIT / BASIC: monitor panes, the deck chooser, the notes column ----
    addAndMakeVisible (infoBtn); infoBtn.getProperties().set ("fontPx", 11.0);
    infoBtn.onClick = [this] { proc.uiState.setProperty ("info", ! infoOn(), nullptr); if (onDesignChange) onDesignChange(); resized(); if (viewMode() != 0) layoutMode(); repaint(); };
    setHelp (infoBtn, "Notes", "右の解説の列（レッスン・いまのリバーブ音・耳トレ・ヘルプ）を出す／隠す。隠すと窓がその幅だけ狭くなり、リバーブの部分の配置は変わりません。",
             "Shows / hides the notes column on the right (lesson, what you hear, ear training, help). Hidden, the window is that much narrower; the reverb part stays where it is.");
    for (int i = 0; i < 4; ++i)
    {
        addChildComponent (paneBtn[i]); paneBtn[i].getProperties().set ("fontPx", 10.0);
        paneBtn[i].onClick = [this, i] { showPaneMenu (i); };
        setHelp (paneBtn[i], "Monitor", "この面に何を出すか（自動＝ER／TAIL に合わせる）。", "What this pane shows (Auto = follows ER / TAIL).");
    }
    {
        const char* tn[3][2] = { { "レッスン", "Lesson" }, { "リバーブ音", "Reverb sound" }, { "耳トレ", "Ear training" } };
        for (int i = 0; i < 3; ++i)
        {
            addAndMakeVisible (infoTab[i]); infoTab[i].getProperties().set ("fontPx", 11.0); infoTab[i].setClickingTogglesState (false);
            infoTab[i].setButtonText (ph.tr (tn[i][0], tn[i][1]));
            infoTab[i].onClick = [this, i] { setInfoSel (i); };
            if (i == 0) { infoTab[i].onHold = [this] { showLessonList(); };
                          setHelp (infoTab[i], "Lesson", "クリックでレッスン、長押し（右クリック）で一覧から選ぶ。", "Click: the lessons; long-press (right-click): pick one from the list."); }
        }
    }
    {
        const char* dn[4] = { "SIMPLE", "BASIC 8", "MACRO", "UNIT" };
        for (int i = 0; i < 4; ++i)
        {
            addAndMakeVisible (deckSel[i]); deckSel[i].getProperties().set ("fontPx", 10.0); deckSel[i].setClickingTogglesState (false);
            deckSel[i].setButtonText (dn[i]);
            deckSel[i].onClick = [this, i] { proc.uiState.setProperty ("deck", i, nullptr); layoutDeck(); if (viewMode() != 0) layoutMode(); repaint(); };
        }
        setHelp (deckSel[0], "Deck: SIMPLE", "よく使うつまみを 2 段に並べた操作部（DAMPING・SHAPE・DIFF・MOD・EQ・OUT）。", "The classic deck: the everyday controls in two rows.");
        setHelp (deckSel[1], "Deck: BASIC 8", "毎日使う 8 つ：Mix・PreDelay・Decay・Size・Diffusion・低域と高域の残り方・ER/TAIL。", "The everyday eight: Mix, PreDelay, Decay, Size, Diffusion, bass and treble decay, ER / TAIL.");
        setHelp (deckSel[2], "Deck: MACRO", "Mix・Decay と、複数のパラメーターを一緒に動かすマクロ 4 つ（Size・Attack・Brightness・Distance）。動かしたパラメーターはヘルプに出ます。", "Mix, Decay and four macros that move several parameters together (Size, Attack, Brightness, Distance).");
        setHelp (deckSel[3], "Deck: UNIT", "往年の機材のプリセット（ALGO＋COLOR）では、その機材のパネル。", "With a classic-unit preset: that unit's faceplate.");
    }
    if ((int) proc.uiState.getProperty ("view", 0) == 1 && ! proc.uiState.hasProperty ("deckRemote")) proc.uiState.setProperty ("view", 0, nullptr);       // (the old BASIC screen is gone)
    colorDeck.setLayout (3); colorDeck.bottomPad = 30;
    colorDeck.frameFn = [this] { return colorAgeColour (colorAge (ph.value ("bw"), ph.value ("bits"))); };
    colorDeck.subFn = nullptr;          // (the title line holds the ER / TAIL switches; the frame's colour still shows the era)
    for (auto* id : { "bw", "bits", "wander" }) vals[id]->setVisible (false);       // (their knobs are in the COLOR group at the deck's right end)
    echo.prev = &prev; info.prev = &prev;
    // drag the echogram's "tail from" line: Tail Delay (ALGO / ROOM) or the IR tail's start (IR / IR+ALGO)
    echo.onTailDrag = [this] (double ms)
    {
        const double pre = ph.value ("predelay"); const int ts = (int) ph.value ("tSrc");
        const char* id = ts == 0 ? "tPre" : "irtX";
        ph.set (id, (float) std::max (0.0, ms - pre));
    };
    info.onFlash = [this] (const juce::String& t, const juce::String& b) { flash (t, b); layoutTail(); layoutMonitors(); repaint(); };
    setWantsKeyboardFocus (true);
    addMouseListener (this, true);
    layoutSource();
    startTimerHz (30);
}

MainView::~MainView() { setLookAndFeel (nullptr); }

void MainView::applySkin (int i)
{
    theme::setSkin (i); lnf.apply();
    proc.uiState.setProperty ("skin", theme::current, nullptr); ZoomPrefs::set ("rvlab.skinV2", theme::current);
    sendLookAndFeelChange(); repaint();
}
// (0.10.13) the view settings worth keeping from one use to the next (notes column, EDIT panel, language, monitor layout...)
static bool isViewKey (const juce::String& k)
{
    static const char* ex[] = { "info", "infoSel", "detShow", "lang", "page", "simpleHelp", "panelOn", "monLay" };
    for (auto* e : ex) if (k == e) return true;
    return k.startsWith ("monV") || k.startsWith ("monR");
}
void MainView::restoreLastView()
{
    auto& f = ZoomPrefs::file();
    for (auto& key : f.getAllProperties().getAllKeys())
    {
        if (! key.startsWith ("rvlab.ui.")) continue;
        const auto k = key.substring (9); if (! isViewKey (k) || proc.uiState.hasProperty (k)) continue;
        const auto v = f.getValue (key);
        if (v.containsOnly ("-0123456789")) proc.uiState.setProperty (k, v.getIntValue(), nullptr);
        else if (v.containsOnly ("-0123456789.")) proc.uiState.setProperty (k, v.getDoubleValue(), nullptr);
        else proc.uiState.setProperty (k, v, nullptr);
    }
}
void MainView::saveLastView()
{
    auto& f = ZoomPrefs::file(); bool any = false;
    for (int i = 0; i < proc.uiState.getNumProperties(); ++i)
    {
        const auto k = proc.uiState.getPropertyName (i).toString(); if (! isViewKey (k)) continue;
        const auto v = proc.uiState.getProperty (k).toString();
        if (f.getValue ("rvlab.ui." + k) != v) { f.setValue ("rvlab.ui." + k, v); any = true; }
    }
    if (any) f.saveIfNeeded();
}
void MainView::applyLookAdjust()
{
    proc.uiState.setProperty ("adjText", (double) theme::adjText, nullptr); proc.uiState.setProperty ("adjBright", (double) theme::adjBright, nullptr); proc.uiState.setProperty ("adjContrast", (double) theme::adjContrast, nullptr);
    ZoomPrefs::set ("rvlab.adjText", theme::adjText); ZoomPrefs::set ("rvlab.adjBright", theme::adjBright); ZoomPrefs::set ("rvlab.adjContrast", theme::adjContrast);
    theme::setSkin (theme::current); lnf.apply(); sendLookAndFeelChange(); repaint();
}

void MainView::showLoadMenu (int slot)
{
    // (0.10.10) the same menu for IR A and IR B (the IR -> algorithm batch works on IR A)
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    auto& L = slot == 1 ? proc.irListB : proc.irList; const int LI = slot == 1 ? proc.irListIndexB : proc.irListIndex;
    m.addItem (1, ph.tr ("ファイルを選ぶ（複数選択可）…", "Choose files (several allowed)..."));
    m.addItem (2, ph.tr ("フォルダーを選ぶ…", "Choose a folder..."));
    m.addSeparator();
    {   // (0.10.4) IR folders kept for next time (shared by every instance, saved on this computer)
        const auto saved = juce::StringArray::fromLines (ZoomPrefs::file().getValue ("rvlab.irFolders", ""));
        m.addSeparator(); m.addSectionHeader (ph.tr ("保存した IR フォルダー", "Saved IR folders"));
        for (int i = 0; i < saved.size() && i < 20; ++i)
        {
            const juce::File d (saved[i]); juce::PopupMenu sub;
            sub.addItem (1000 + i * 10, ph.tr ("このフォルダーの IR を読み込む", "Load the IRs of this folder"), d.isDirectory());
            sub.addItem (1001 + i * 10, ph.tr ("ここからファイルを選ぶ…", "Choose files from here..."), d.isDirectory());
            sub.addItem (1002 + i * 10, ph.tr ("保存から外す", "Remove from the saved folders"));
            m.addSubMenu ((d.isDirectory() ? juce::String() : juce::String ("(?) ")) + d.getFileName() + "  —  " + d.getParentDirectory().getFullPathName(), sub);
        }
        m.addItem (3, ph.tr ("フォルダーを選んで保存に追加…", "Add a folder to the saved folders..."));
        const juce::File cur (proc.uiState.getProperty ("irDir", "").toString());
        if (cur.isDirectory() && ! saved.contains (cur.getFullPathName()))
            m.addItem (4, ph.tr ("いまのフォルダーを保存: ", "Save the current folder: ") + cur.getFileName());
    }
    if (! L.isEmpty())
    {
        m.addSeparator(); m.addSectionHeader (ph.tr ("リスト", "List") + " (" + juce::String (L.size()) + ")");
        for (int i = 0; i < std::min (L.size(), 40); ++i) m.addItem (100 + i, L[i].getFileName(), true, i == LI);
    }
    // (0.10.12) the IR -> algorithm section is the last one of the menu
    if (slot == 0) m.addSeparator();
    if (slot == 0) m.addSectionHeader (ph.tr ("IR → アルゴリズム（鳴らして比べて合わせる、1 個 約 1 分）", "IR -> algorithm (render, compare, adjust: ~1 min each)"));
    if (slot == 1) {}
    else if (proc.batchRunning.load())
        m.addItem (7, ph.tr ("中止", "Stop") + " (" + juce::String (proc.batchDone.load()) + " / " + juce::String (proc.batchTotal.load()) + ")");
    else
    {
        if (proc.lastIRFile.existsAsFile())
            m.addItem (9, ph.tr ("いまの IR を高精度に再現（ER＝ALGO、TAIL＝ALGO）", "Rebuild this IR precisely (ER = ALGO, TAIL = ALGO)"));
        m.addItem (5, ph.tr ("ファイルを選んで作成（複数選択可）…", "Choose files (several)..."));
        m.addItem (6, ph.tr ("フォルダーを選んで作成…", "Choose a folder..."));
        if (proc.irList.size() > 0) m.addItem (8, ph.tr ("いまのリストから作成", "From the current list") + " (" + juce::String (proc.irList.size()) + ")");
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slot == 1 ? &ir2Load : &loadBtn), [this, slot] (int r)
    {
        if (r >= 1000)
        {
            auto saved = juce::StringArray::fromLines (ZoomPrefs::file().getValue ("rvlab.irFolders", ""));
            const int i = (r - 1000) / 10, act = (r - 1000) % 10; if (i >= saved.size()) return;
            const juce::File d (saved[i]);
            if (act == 0) loadIRFolder (d, slot);
            else if (act == 1) { proc.uiState.setProperty ("irDir", d.getFullPathName(), nullptr); chooseIR (false, slot); }
            else { saved.remove (i); ZoomPrefs::file().setValue ("rvlab.irFolders", saved.joinIntoString ("\n")); ZoomPrefs::file().saveIfNeeded(); }
        }
        else if (r == 3)
        {
            chooser = std::make_unique<juce::FileChooser> (ph.tr ("保存する IR フォルダーを選ぶ", "Choose an IR folder to save"), juce::File (proc.uiState.getProperty ("irDir", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()).toString()));
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc) { const auto d = fc.getResult(); if (d.isDirectory()) addIRFolder (d); });
        }
        else if (r == 4) addIRFolder (juce::File (proc.uiState.getProperty ("irDir", "").toString()));
        else if (r == 1) chooseIR (false, slot); else if (r == 2) chooseIR (true, slot);
        else if (r == 5 || r == 6) chooseBatch (r == 6);
        else if (r == 7) proc.cancelBatch();
        else if (r == 8) startBatchFor (proc.irList);
        else if (r == 9 && proc.lastIRFile.existsAsFile())
        {
            proc.startBatch ({ proc.lastIRFile }, "", true); batchWatch = true; batchApply = true;
            flash (ph.tr ("IR を高精度に再現", "Rebuilding the IR precisely"),
                   ph.tr ("プラグインが自分の IR を鳴らし、読み込んだ IR と T30・EDT・C80・音色・広がりを比べながら、ER（ALGO）と TAIL（ALGO）のパラメーターを約 350 回動かして合わせます（約 1 分）。終わると結果がそのまま入ります（IR がなくても同じ音）。",
                          "The plugin renders its own IR, compares it with the loaded IR (T30, EDT, C80, tone, width) and moves the ER (ALGO) and TAIL (ALGO) settings ~350 times (~1 min). The result stays on (it sounds the same without the IR)."));
        }
        else if (r >= 100)
        {
            (slot == 1 ? proc.irListIndexB : proc.irListIndex) = r - 100 - 1;
            const auto e = proc.stepIR (1, slot); if (e.isNotEmpty()) flash ("IR", e); else if (slot == 0) { proc.undo.beginNewTransaction(); proc.setParam ("src", 2); }
            repaint();
        }
    });
}

void MainView::chooseBatch (bool folder)
{
    const auto start = juce::File (proc.uiState.getProperty ("irDir", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()).toString());
    chooser = std::make_unique<juce::FileChooser> (ph.tr ("プリセットにする IR を選ぶ", "Choose the IRs to turn into presets"), start, "*.wav;*.aif;*.aiff;*.flac");
    const int flags = juce::FileBrowserComponent::openMode | (folder ? juce::FileBrowserComponent::canSelectDirectories : (juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems));
    chooser->launchAsync (flags, [this, folder] (const juce::FileChooser& fc)
    {
        juce::Array<juce::File> files;
        if (folder) { const auto d = fc.getResult(); if (d.isDirectory()) files = ERProcessor::irFilesIn (d); }
        else for (auto& f : fc.getResults()) if (f.existsAsFile()) files.add (f);
        if (! files.isEmpty()) startBatchFor (files);
    });
}

void MainView::startBatchFor (const juce::Array<juce::File>& files)
{
    if (files.isEmpty() || proc.batchRunning.load()) return;
    proc.startBatch (files, " (Fit)"); batchApply = false;
    flash (ph.tr ("プリセットの一括作成", "Batch presets"),
           juce::String (files.size()) + ph.tr (" 個の IR を順に、ER＝ALGO・TAIL＝ALGO で組み立ててから、鳴らして比べて（T30・EDT・C80・音色・広がり）合わせ、ユーザープリセット「（ファイル名） (Fit)」に保存します。1 個 約 1 分。True Stereo のペア（*_L／*_R）は 1 つにまとめます。終わると元の設定に戻ります。IR読込のメニューで中止できます。",
                                               " IRs: each is analysed, rebuilt (ER = ALGO, tail = ALGO) and then matched by rendering and comparing (T30, EDT, C80, tone, width; ~1 min each), and saved as the user preset \"(file name) (Fit)\". A true-stereo pair (*_L / *_R) counts as one. Your settings come back at the end. Stop from the Load IR menu."));
    batchWatch = true;
}

void MainView::chooseIR (bool folder, int slot)
{
    const auto start = juce::File (proc.uiState.getProperty ("irDir", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()).toString());
    chooser = std::make_unique<juce::FileChooser> (folder ? ph.tr ("IR のフォルダーを選ぶ", "Choose a folder of IRs") : ph.tr ("IR を選ぶ（複数可）", "Choose IRs"), start, "*.wav;*.aif;*.aiff;*.flac");
    const int flags = juce::FileBrowserComponent::openMode | (folder ? juce::FileBrowserComponent::canSelectDirectories : (juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems));
    chooser->launchAsync (flags, [this, folder, slot] (const juce::FileChooser& fc)
    {
        juce::Array<juce::File> files;
        if (folder) { const auto d = fc.getResult(); if (! d.isDirectory()) return; files = ERProcessor::irFilesIn (d); proc.uiState.setProperty ("irDir", d.getFullPathName(), nullptr); }
        else { for (auto& f : fc.getResults()) if (f.existsAsFile()) files.add (f); if (! files.isEmpty()) proc.uiState.setProperty ("irDir", files[0].getParentDirectory().getFullPathName(), nullptr); }
        if (files.isEmpty()) { if (folder) flash ("Load IR", ph.tr ("このフォルダーに WAV / AIFF / FLAC がありません。", "No WAV / AIFF / FLAC in this folder.")); return; }
        { juce::Array<juce::File> keep; for (auto& f : files) if (! fit::isRightOfPair (f)) keep.add (f); if (! keep.isEmpty()) files = keep; }      // (0.10.16) an L / R set counts once
        proc.setIRList (files, 0, slot);
        const auto err = proc.loadIRSlot (files[0], slot);
        if (err.isNotEmpty()) { flash ("Load IR", err); return; }
        if (slot == 1) { repaint(); if (files.size() > 1) flash ("IR B", juce::String (files.size()) + ph.tr (" 個の IR を読み込みました（矢印で切り替え）。", " IRs loaded (step with the arrows).")); return; }
        if (files.size() > 1) flash ("IR list", juce::String (files.size()) + ph.tr (" 個の IR を読み込みました。IR読込の左右の矢印で切り替えます。", " IRs loaded. Step through them with the arrows beside Load IR."));
        proc.undo.beginNewTransaction(); proc.setParam ("src", 2);
    });
}

void MainView::addIRFolder (const juce::File& d)
{
    if (! d.isDirectory()) return;
    auto saved = juce::StringArray::fromLines (ZoomPrefs::file().getValue ("rvlab.irFolders", ""));
    saved.removeEmptyStrings(); saved.removeString (d.getFullPathName()); saved.insert (0, d.getFullPathName());
    while (saved.size() > 20) saved.remove (saved.size() - 1);
    ZoomPrefs::file().setValue ("rvlab.irFolders", saved.joinIntoString ("\n")); ZoomPrefs::file().saveIfNeeded();
    flash ("IR folders", d.getFileName() + ph.tr (" を保存しました（IR読込のメニュー「保存した IR フォルダー」から開けます）。", " saved (open it from the Load IR menu, Saved IR folders)."));
}

void MainView::loadIRFolder (const juce::File& d, int slot)
{
    if (! d.isDirectory()) { flash ("Load IR", ph.tr ("フォルダーが見つかりません。", "The folder was not found.")); return; }
    const auto files = ERProcessor::irFilesIn (d);
    if (files.isEmpty()) { flash ("Load IR", ph.tr ("このフォルダーに WAV / AIFF / FLAC がありません。", "No WAV / AIFF / FLAC in this folder.")); return; }
    proc.uiState.setProperty ("irDir", d.getFullPathName(), nullptr);
    juce::Array<juce::File> files2; for (auto& f : files) if (! fit::isRightOfPair (f)) files2.add (f);
    proc.setIRList (files2.isEmpty() ? files : files2, 0, slot);
    const auto err = proc.loadIRSlot ((files2.isEmpty() ? files : files2)[0], slot);
    if (err.isNotEmpty()) { flash ("Load IR", err); return; }
    if (slot == 1) { repaint(); return; }
    if (files.size() > 1) flash ("IR list", juce::String (files.size()) + ph.tr (" 個の IR を読み込みました。IR読込の左右の矢印で切り替えます。", " IRs loaded. Step through them with the arrows beside Load IR."));
    proc.undo.beginNewTransaction(); proc.setParam ("src", 2);
}

void MainView::showExportMenu()
{
    auto& u = proc.uiState;
    const bool direct = (bool) u.getProperty ("exDirect", false), tail = (bool) u.getProperty ("exTail", true);
    const double sr = (double) u.getProperty ("exSR", 48000.0); const int bits = (int) u.getProperty ("exBits", 24);
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    m.addSectionHeader (ph.tr ("書き出す内容", "Content"));
    m.addItem (1, ph.tr ("ER だけ（Wet）", "ER only (wet)"), true, ! direct);
    m.addItem (2, ph.tr ("直接音 + ER（直接音 = 0 dBFS）", "Direct + ER (direct = 0 dBFS)"), true, direct);
    m.addItem (3, ph.tr ("Tail Preview を含める（オンのとき）", "Include the Tail Preview (when on)"), true, tail);
    m.addItem (4, ph.tr ("ピークを −1 dBFS にノーマライズ（直接音との比は保ったまま）", "Normalise the peak to -1 dBFS (keeps the ratio to the direct sound)"), true, (bool) u.getProperty ("exNorm", false));
    m.addSectionHeader (ph.tr ("サンプルレート", "Sample rate"));
    const double rates[] = { 44100, 48000, 88200, 96000 };
    for (int i = 0; i < 4; ++i) m.addItem (10 + i, juce::String (rates[i] / 1000.0, 1) + " kHz", true, std::abs (sr - rates[i]) < 1);
    m.addSectionHeader (ph.tr ("ビット深度", "Bit depth"));
    m.addItem (20, "24-bit", true, bits == 24);
    m.addItem (21, "32-bit float", true, bits == 32);
    m.addSeparator();
    m.addItem (30, ph.tr ("WAV に書き出す…", "Export WAV..."));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this, rates] (int r)
    {
        auto& uu = proc.uiState;
        if (r == 1 || r == 2) uu.setProperty ("exDirect", r == 2, nullptr);
        else if (r == 3) uu.setProperty ("exTail", ! (bool) uu.getProperty ("exTail", true), nullptr);
        else if (r == 4) uu.setProperty ("exNorm", ! (bool) uu.getProperty ("exNorm", false), nullptr);
        else if (r >= 10 && r < 14) uu.setProperty ("exSR", rates[r - 10], nullptr);
        else if (r == 20 || r == 21) uu.setProperty ("exBits", r == 21 ? 32 : 24, nullptr);
        if (r == 30) exportTo();
        else if (r > 0) { juce::Component::SafePointer<MainView> sp0 (this); juce::MessageManager::callAsync ([sp0] { if (sp0 != nullptr) sp0->showExportMenu(); }); }   // keep choosing
    });
}

void MainView::exportTo()
{
    auto& u = proc.uiState;
    ERProcessor::ExportOpts o;
    o.direct = (bool) u.getProperty ("exDirect", false); o.tail = (bool) u.getProperty ("exTail", true);
    o.sr = (double) u.getProperty ("exSR", 48000.0); o.bits = (int) u.getProperty ("exBits", 24);
    o.normalize = (bool) u.getProperty ("exNorm", false);
    const int src = (int) ph.value ("src");
    juce::String base = proc.currentPreset >= 0 ? juce::String::fromUTF8 (presets()[(size_t) proc.currentPreset].name).replace (juce::String::fromUTF8 ("\xe2\x80\x94"), "-")
                      : src == 2 && proc.irName.isNotEmpty() ? juce::File::createLegalFileName (proc.irName).upToLastOccurrenceOf (".", false, false)
                      : juce::String (src == 0 ? "Room" : src == 1 ? "Algo" : "IR");
    base << " ER" << (o.direct ? " +Direct" : "") << (o.tail && ph.value ("tail") > 0.5f ? " +Tail" : "") << " " << juce::String (o.sr / 1000.0, o.sr == 44100 || o.sr == 88200 ? 1 : 0) << "k";
    auto dir = juce::File (u.getProperty ("exDir", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName()).toString());
    chooser = std::make_unique<juce::FileChooser> (ph.tr ("IR を書き出す", "Export IR"), dir.getChildFile (juce::File::createLegalFileName (base) + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, o] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult(); if (f == juce::File()) return;
        if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
        bool ok = false;
        const auto report = proc.exportIR (f, o, ok);
        if (ok) proc.uiState.setProperty ("exDir", f.getParentDirectory().getFullPathName(), nullptr);
        flash (ph.tr ("IR を書き出しました", "IR exported"), report);
    });
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    const int vm = viewMode();
    if (vm == 2) { g.fillAll (juce::Colour (0xff1c1d20)); return; }        // the remote draws itself
    g.setColour (theme::header); g.fillRect (0, 0, getWidth(), 50);
    g.setColour (theme::line); g.drawHorizontalLine (50, 0, (float) getWidth());
    if (vm != 0)
    {
        g.setColour (theme::text); g.setFont (theme::ui (19.0f, true)); g.drawText ("Reverb Anatomy Lab", 16, 6, 230, 24, juce::Justification::centredLeft);
        g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawText (vm == 1 ? "REMOTE  |  deck" : "REMOTE  |  modern", 18, 30, 200, 14, juce::Justification::centredLeft);
        return;
    }
    g.setColour (theme::text); g.setFont (theme::ui (17.0f, true)); g.drawText ("Reverb Anatomy Lab", 14, 8, 182, 24, juce::Justification::centredLeft);
    g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawText ("Reverb  |  by Johan Plus Kei  v" JucePlugin_VersionString, 18, 30, 300, 14, juce::Justification::centredLeft);
    const int src = (int) ph.value ("src");
    auto group = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (theme::panel); g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (11.0f, true)); g.drawText (t, r.getX() + 12, r.getY() + 6, r.getWidth() - 20, 16, juce::Justification::centredLeft);
    };
    const int page = tabs.page();
    const bool p0 = shows (0);              // (the detail panel: only the open title's groups are drawn)
    // the REVERB screen: one panel per column (the titles above them); a thin line splits SOURCE from VOICE
    auto column = [&] (juce::Rectangle<int> r, bool split, juce::Colour c, const juce::String& title = {})
    {   // each part in its own colour: a tinted ground, a coloured frame and title strip
        g.setColour (theme::panel); g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (c.withAlpha (0.07f)); g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (c.withAlpha (0.16f)); g.fillRoundedRectangle (r.toFloat().withHeight ((float) kGT), 6.0f);
        g.setColour (c.withAlpha (0.75f)); g.drawRoundedRectangle (r.toFloat().reduced (0.75f), 6.0f, 1.5f);
        if (title.isNotEmpty())
        {   // (0.10.13) the title strip like the preset box: its ground, frame and 12 pt font
            const auto tb = r.toFloat().withHeight ((float) kGT).reduced (2.0f, 1.0f);
            g.setColour (theme::comboBg()); g.fillRoundedRectangle (tb, 3.0f);
            g.setColour (c.withAlpha (0.7f)); g.drawRoundedRectangle (tb.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (theme::text); g.setFont (theme::ui (12.0f)); g.drawText (title, r.getX() + 10, r.getY() + 1, r.getWidth() - 16, kGT - 2, juce::Justification::centredLeft);
        }
        if (split)
        {
            const float x = (float) (r.getX() + 4 * kP + 4);
            g.setColour (c.withAlpha (0.45f)); g.drawLine (x, (float) r.getY() + 4, x, (float) (r.getY() + kGT + 4 * kRP - 2), 1.0f);
            g.drawLine ((float) r.getX() + 6, (float) (r.getY() + kGT + 4 * kRP - 2), (float) r.getRight() - 6, (float) (r.getY() + kGT + 4 * kRP - 2), 1.0f);
            g.setColour (c.darker (0.2f)); g.setFont (theme::ui (10.5f, true));
            g.drawText ("VOICE", (int) x + 6, r.getY() + 3, 2 * kP, 15, juce::Justification::centredLeft);
        }
    };
    if (p0) column (gEr, true, sectionColour (0));
    if (shows (1)) column (gTl, true, sectionColour (1));
    if (shows (3)) column (gIn, false, sectionColour (3));
    if (shows (4))
    {
        const auto era = colorAgeColour (colorAge (ph.value ("bw"), ph.value ("bits")));
        column (gCol, false, era, "COLOR");
        column (gOut, false, sectionColour (4), "OUTPUT");
    }
    // SOURCE (0.9.1): four cards - ROOM A | ROOM B | IR A | IR B
    if (deckView() == 1)
    {
        auto roomCol = [&] (int i, int slot, int id)
        {
            const auto r = gCard[i]; column (r, false, sectionColour (id), slot == 0 ? "ROOM A" : "ROOM B");
            const auto R = proc.roomParams (slot);
            const auto GM = er::roomGeom (R); const double V = GM.V, S = GM.S;
            const auto a1 = er::roomAcoustics (R, 1000.0);
            const int x = r.getX() + 10;
            {   // (0.10.9) the numbers on the right of the controls
                const int rx = r.getX() + 8 + 4 * kP + 4, rw = r.getRight() - 8 - rx;
                const auto cd = er::coupledDecay (R, 1000.0, er::coupledRoomOf (R, R.sx, R.sy), er::coupledRoomOf (R, R.lx, R.ly));
                const juce::String ln[3] = { juce::String ((int) std::round (V)) + " m3  " + juce::String ((int) std::round (S)) + " m2",
                                             "RT60 " + juce::String (a1.rt, 2) + " s",
                                             R.shape == 4 ? ph.tr ("二重勾配 ", "2 slopes ") + juce::String (cd.Ta, 1) + " / " + juce::String (cd.Tb, 1) + " s"
                                                          : ph.tr ("TAIL Size ", "TAIL Size ") + juce::String ((int) std::round (er::roomTailSizeMs (R))) + " ms" };
                g.setColour (theme::text); g.setFont (theme::mono (10.0f));
                for (int k = 0; k < 3; ++k) g.drawFittedText (ln[k], rx, r.getY() + kGT + 4 + k * 14, rw, 14, juce::Justification::centredLeft, 1, 0.8f);
            }
            if (false) {
            const int y = 0, w = 0;
            const juce::String lines[] = {
                juce::String ((int) std::round (V)) + " m3  " + juce::String ((int) std::round (S)) + " m2  RT60 " + juce::String (a1.rt, 2) + " s",
                R.shape == 4 ? ph.tr ("二重勾配 ", "2 slopes ") + juce::String (er::coupledDecay (R, 1000.0, er::coupledRoomOf (R, R.sx, R.sy), er::coupledRoomOf (R, R.lx, R.ly)).Ta, 1) + " / " + juce::String (er::coupledDecay (R, 1000.0, er::coupledRoomOf (R, R.sx, R.sy), er::coupledRoomOf (R, R.lx, R.ly)).Tb, 1) + " s"
                                 : ph.tr ("自由行程 ", "MFP ") + juce::String (4 * V / S, 2) + " m  " + ph.tr ("→ TAIL Size ", "-> TAIL ") + juce::String ((int) std::round (er::roomTailSizeMs (R))) + " ms" };
            g.setColour (theme::text); g.setFont (theme::mono (10.5f));
            for (int k = 0; k < 2; ++k) g.drawFittedText (lines[k], x, y + k * 15, w, 15, juce::Justification::centredLeft, 1, 0.8f);
            }
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawText (ph.tr ("形", "Shape"), x - 2, cell (r, 0, 2).getY() - 4, 60, 13, juce::Justification::centredLeft);
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawText (ph.tr ("壁・天井", "Walls / ceiling"), cell (r, 0, 1).getX(), cell (r, 0, 1).getY() - 1, 100, 13, juce::Justification::centredLeft);
            g.drawText (ph.tr ("床", "Floor"), cell (r, 2, 1).getX(), cell (r, 2, 1).getY() - 1, 100, 13, juce::Justification::centredLeft);
        };
        auto irCol = [&] (int i, int slot, int id)
        {
            const auto r = gCard[i]; column (r, false, sectionColour (id), slot == 0 ? "IR A" : "IR B");
            const bool loaded = cardLoaded (i);
            const juce::String nm = slot == 1 ? proc.tIrName : proc.irName;
            const int x = r.getX() + 10, w = r.getWidth() - 20;
            g.setFont (theme::ui (10.5f, true)); g.setColour (loaded ? theme::text : theme::faint);
            const int lw = w - 160;      // (0.10.10) the left part: name and level; the right part: compare
            g.drawFittedText (loaded ? nm : ph.tr ("（未読込）", "(not loaded)"), x, r.getY() + kGT + 28, lw - 44, 14, juce::Justification::centredLeft, 1, 0.7f);
            {
                const auto ct = compareText (slot);
                g.setColour (cmpOn() && cmpSlot() == slot ? theme::text : theme::faint); g.setFont (theme::mono (9.5f));
                g.drawFittedText (ct, x + w - 150, r.getY() + kGT + 74, 150, 36, juce::Justification::topLeft, 3, 0.75f);
            }
            const auto& env = envOf (slot);
            if (loaded)
            {
                g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
                g.drawText (juce::String (irEnvLen[slot], 2) + " s", x, r.getY() + kGT + 28, lw, 14, juce::Justification::centredRight);
                const juce::Rectangle<float> pr ((float) x, (float) (r.getY() + kGT + 45), (float) lw, 52.0f);
                g.setColour (theme::bg.withAlpha (0.55f)); g.fillRoundedRectangle (pr, 3.0f);
                if (env.size() > 2)
                {
                    juce::Path pth; pth.startNewSubPath (pr.getX(), pr.getBottom());
                    for (size_t k = 0; k < env.size(); ++k) pth.lineTo (pr.getX() + pr.getWidth() * (float) k / (float) (env.size() - 1), pr.getBottom() - pr.getHeight() * env[k]);
                    pth.lineTo (pr.getRight(), pr.getBottom()); pth.closeSubPath();
                    g.setColour (sectionColour (id).withAlpha (0.45f)); g.fillPath (pth);
                    g.setColour (sectionColour (id)); g.strokePath (pth, juce::PathStrokeType (1.0f));
                }
                if (headLit (slot) && irEnvLen[slot] > 0.01)
                {   // where the head hands over to the body: the direct sound + IR Tail Start + hand-over
                    const double t = irEnvT0[slot] + (ph.value ("irtX") + ph.value ("irtB")) / 1000.0;
                    const float mx = pr.getX() + pr.getWidth() * (float) std::min (1.0, t / irEnvLen[slot]);
                    g.setColour (theme::direct); g.drawLine (mx, pr.getY(), mx, pr.getBottom(), 1.5f);
                    g.setFont (theme::ui (9.5f, true)); g.drawText ("HEAD | BODY", (int) mx + 3, (int) pr.getY() + 1, 80, 12, juce::Justification::centredLeft);
                }
            }
        };
        if (cardOn[0]) roomCol (0, 0, 8);
        if (cardOn[1]) roomCol (1, 1, 9);
        if (cardOn[2]) irCol (2, 0, 10);
        if (cardOn[3]) irCol (3, 1, 11);
        // each card's role now (in its title strip): ER / TAIL / HEAD / BODY
        for (int i = 0; i < 4; ++i)
        {
            if (! cardOn[i]) continue;
            juce::StringArray roles; const bool hd = ph.value ("tSrc") > 1.5f;
            if (erLit (i)) roles.add ("ER");
            if (i >= 2 && headLit (i - 2)) roles.add ("T-HEAD");
            if (tailLit (i)) roles.add (hd ? "BODY" : "TAIL");
            g.setFont (theme::ui (10.5f, true)); g.setColour (roles.isEmpty() ? theme::faint : sectionColour (8 + i).darker (0.4f));
            g.drawFittedText (roles.isEmpty() ? ph.tr ("使用なし", "not used") : ph.tr ("使用中：", "In use: ") + roles.joinIntoString (" + "), gCard[i].getX() + 70, gCard[i].getY() + 3, gCard[i].getWidth() - 126, 15, juce::Justification::centredLeft, 1, 0.8f);
        }
    }
    // the deck's COLOR box: the era preset + where it acts (its frame takes the era's colour)
    if (! colorBox.isEmpty())
    {
        const auto era = colorAgeColour (colorAge (ph.value ("bw"), ph.value ("bits")));
        g.setColour (era.withAlpha (0.08f)); g.fillRoundedRectangle (colorBox.toFloat(), 8.0f);
        g.setColour (era.withAlpha (0.8f)); g.drawRoundedRectangle (colorBox.toFloat().reduced (1), 8.0f, 2.0f);
        g.setFont (theme::ui (11.0f, true)); g.drawText ("COLOR", colorBox.getX() + 10, colorBox.getY() + 5, 60, 18, juce::Justification::centredLeft);
    }
    if (shows (3) || shows (4))
    {
        auto lab = [&] (juce::Component& c, const juce::String& t) { g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawFittedText (t, c.getX(), c.getY() - 14, 2 * kP - 4, 13, juce::Justification::centredLeft, 1, 0.75f); };
        if (shows (3)) { lab (inModeBox, ph.tr ("入力モード", "Input mode")); lab (iToSeg, ph.tr ("フィルターの先", "Filter to")); lab (iLCsBox, ph.tr ("LC／HC の傾き", "LC / HC slope")); }
        if (shows (4)) { lab (outModeSeg, ph.tr ("出力", "Output")); lab (eqOnSeg, "MASTER EQ"); }
    }
    if (p0) {
    if ((int) ph.value ("src") == 0)
    {   // (0.10.8) the room the ER is built from (its parameters are on the SOURCE screen)
        g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
        g.drawFittedText (roomSummary (proc.roomParams(), ph.value ("erRoomSel") > 0.5f ? "B" : "A", false), { gSrc.getX() + 8, gSrc.getY() + kGT + 4, 4 * kP - 4, 66 }, juce::Justification::topLeft, 4, 0.7f);
        g.setFont (theme::ui (9.5f));
        g.drawFittedText (ph.tr ("部屋の寸法・材質・形は SOURCE 画面で変えます。", "Change the room's size, materials and shape on the SOURCE screen."), { gSrc.getX() + 8, gSrc.getY() + kGT + 76, 4 * kP - 4, 24 }, juce::Justification::topLeft, 2, 0.8f);
    }
    g.setColour (theme::muted); g.setFont (theme::ui (10.5f));
    g.drawText ("LC slope", lcSlopeBox.getX(), lcSlopeBox.getY() - 15, kKW, 14, juce::Justification::centredLeft);
    g.drawText ("HC slope", slopeBox.getX(), slopeBox.getY() - 15, kKW, 14, juce::Justification::centredLeft);
    if (src == 1)
    {
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f));
        const int e = (int) std::round (ph.value ("roomEarly"));
        g.drawFittedText (e > 0 ? ph.tr ("最初の ", "The first ") + juce::String (e) + ph.tr (" ms は ROOM の反射（ROOM の設定のまま）", " ms are ROOM's reflections (as set in ROOM)")
                                : ph.tr ("ROOM Early：最初の数 ms を ROOM の反射にする（Off = ALGO だけ）", "ROOM Early: the first ms from ROOM (Off = ALGO only)"),
                          vals["roomEarly"]->getRight() + 8, vals["roomEarly"]->getY() + 4, gSrc.getRight() - vals["roomEarly"]->getRight() - 14, 40, juce::Justification::centredLeft, 3);
    }
    if (src == 2 && (int) ph.value ("irMode") == 2)
    {
        auto T = proc.target();
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f));
        juce::String t = ph.tr ("IR の反射＋補い（Fill）\n量は 4 ms ごとに IR に合わせる", "IR reflections + fill;\nlevel set to the IR every 4 ms");
        if (T && T->valid && T->mix > 0) t << ph.tr ("\n混合時間 ", "\nMixing time ") << fmtMs ((float) T->mix);
        g.drawFittedText (t, vals["irFill"]->getRight() + 8, vals["irFill"]->getY() + 2, gSrc.getRight() - vals["irFill"]->getRight() - 14, 44, juce::Justification::centredLeft, 3);
    }
    if (src == 2)
    {
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f));
        g.drawText ("Mode", irMode.getX(), irMode.getY() - 15, 100, 14, juce::Justification::centredLeft);
        g.drawText (ph.tr ("窓", "Window"), irAuto.getX(), irAuto.getY() - 15, 100, 14, juce::Justification::centredLeft);
        g.drawText (ph.tr ("IR の種類", "IR type"), irWet.getX(), irWet.getY() - 15, 100, 14, juce::Justification::centredLeft);
    }
    g.setColour (theme::muted); g.setFont (theme::ui (10.5f));
    g.drawText ("Auto Level", autoSeg.getX(), autoSeg.getY() - 15, 70, 14, juce::Justification::centredLeft);
    {   // the correction now, on the label's line
        const float c = proc.autoComp.load();
        g.setColour (std::abs (c) > 0.05f && ph.value ("autoLvl") > 0.5f ? theme::accent : theme::faint); g.setFont (theme::mono (10.0f, true));
        g.drawText (ph.value ("autoLvl") > 0.5f ? fmtDb (c) : juce::String ("-"), autoSeg.getX() + 60, autoSeg.getY() - 15, autoSeg.getWidth() - 60, 14, juce::Justification::centredRight);
    }
    }   // page 0
    bool drewMeas = false;
    for (int pg : { 1, 5, 6 }) if (shows (pg)) { paintReverbPages (g, pg); drewMeas = true; }
    if (! drewMeas) paintReverbPages (g, 0);         // (the measurement pane's frame)
    (void) page;
    // the spectrum monitor's frame (the echogram draws its own)
    if (! specBox.isEmpty())
    {
        g.setColour (theme::panel); g.fillRoundedRectangle (specBox.toFloat(), 6.0f);
        g.setColour (specMain() ? theme::line : theme::line.withAlpha (0.7f)); g.drawRoundedRectangle (specBox.toFloat().reduced (0.5f), 6.0f, 1.0f);
    }
    // ER meter in the header
    {
        const float ip = proc.inPeak.load(), op = proc.outPeak.load();
        auto bar = [&] (int y, float v, const juce::String& k)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f)); g.drawText (k, 648, y - 2, 24, 12, juce::Justification::centredLeft);
            juce::Rectangle<float> r (674, (float) y, 32, 7);
            g.setColour (theme::shade (0.35f)); g.fillRoundedRectangle (r, 2.0f);
            const float db = 20 * std::log10 (v + 1e-9f), x = juce::jlimit (0.0f, 1.0f, (db + 60) / 60);
            g.setColour (db > -1 ? theme::warn : theme::accent); g.fillRoundedRectangle (r.withWidth (r.getWidth() * x), 2.0f);
        };
        bar (14, ip, "IN"); bar (28, op, "OUT");
    }
}

void MainView::resized()
{
    // header: everything inside the reverb part (kRevW), so hiding the notes changes nothing here
    zoom.setBounds (198, 14, 44, 22); undoBtn.setBounds (246, 13, 24, 24); redoBtn.setBounds (272, 13, 24, 24);
    presetBox.setBounds (304, 12, 200, 26); prevP.setBounds (508, 12, 24, 26); nextP.setBounds (534, 12, 24, 26);
    viewBtn.setVisible (false); placeViewButtons (564, 12, 26);
    lockBtn.setBounds (564, 12, 78, 26); lockBtn.toFront (false);
    { const int n = proc.lockedIds().size(); lockBtn.setButtonText (ph.tr ("ロック", "Lock") + (n > 0 ? " " + juce::String (n) : juce::String())); lockBtn.setToggleState (n > 0, juce::dontSendNotification); }
    irPrev.setBounds (710, 12, 20, 26); loadBtn.setBounds (732, 12, 50, 26); irNext.setBounds (784, 12, 20, 26);
    for (auto* b : { &irPrev, &irNext }) b->getProperties().set ("fontPx", 9.0);
    cmpBtn.setBounds (806, 12, 42, 26); lookBtn.setBounds (852, 12, 40, 26); exportBtn.setBounds (896, 12, 52, 26);       // (0.10.12) 表示 (look knobs) left of the export
    langBtn.setBounds (952, 12, 36, 26);
    // monitors
    monArea = { 12, 58, kRevW - 24, 384 };
    // the deck under them (the deck chooser on its left): the main controls, large
    deckR = { 12, 450, kRevW - 24 - kColorW - 8, 152 };
    colorR = { deckR.getRight() + 8, 450, kColorW, 152 };
    // bottom: the detail parameters, opened by EDIT / DYN + MOD (the deck's right end). EDIT = INPUT | ER | TAIL | COLOR / OUTPUT side by side,
    // each part in its own colour; the titles over the columns carry the power switches (ER / TAIL) and the section presets (the mark).
    const int y0 = 610, top = y0 + 28, gh = 226;
    sideR = {}; detSwitch.setVisible (false);
    const int x0 = 12;
    infoBtn.setButtonText (infoOn() ? ph.tr ("解説を隠す", "Hide notes") : ph.tr ("解説を出す", "Show notes"));
    secBtn.setVisible (false);
    auto colW = [] (int n) { return n * kP + 12; };
    gIn = { x0, top, colW (2), gh };
    gEr = { gIn.getRight() + 6, top, colW (6), gh };
    gTl = { gEr.getRight() + 6, top, colW (6), gh };
    gCO = { gTl.getRight() + 6, top, kRevW - 12 - gTl.getRight() - 6, gh };
    gCol = gCO.withHeight (kGT + kRP + 2);                   // COLOR: one row on top
    gOut = gCO.withTrimmedTop (gCol.getHeight() + 4);        // OUTPUT under it
    gSrc = gEr.withWidth (8 + 4 * kP); gVoice = gEr.withTrimmedLeft (4 * kP); gColor = {};
    tabs.setBounds (x0, y0, kRevW - 12 - x0, 24);
    auto loc = [&] (juce::Rectangle<int> r) { return r.withX (r.getX() - x0); };
    tabs.setColumns (0, { { 3, loc (gIn) }, { 0, loc (gEr) }, { 1, loc (gTl) }, { 4, loc (gCO) } });
    auto place = [&] (const char* id, juce::Rectangle<int> g, int col, int row) { vals[id]->setBounds (cell (g, col, row)); };
    // ER VOICE: the two right-hand columns, then the bottom row under the source
    // (0.10.7) the same grid as the TAIL's VOICE:  Delay | Level / Width | Diffusion / Low Cut | High Cut / (own: Size | Darkening) / LC | HC slope
    place ("predelay", gEr, 4, 0); place ("level", gEr, 5, 0); place ("width", gEr, 4, 1); place ("diffusion", gEr, 5, 1);
    place ("size", gEr, 4, 2); place ("darken", gEr, 5, 2); place ("lowcut", gEr, 4, 3); place ("highcut", gEr, 5, 3);       // (0.10.15) Low / High Cut right above their slopes
    lcSlopeBox.setBounds (segAt (gEr, 4, 4, 1)); slopeBox.setBounds (segAt (gEr, 5, 4, 1));
    autoSeg.setBounds (segAt (gEr, 0, 4, 2));
    fgraph.setVisible (false); fgraph.setBounds ({});
    gFilter = {};
    // COLOR (Bandwidth, Bits, Wander) on top; OUTPUT under it: Mix, Output, Width, ER / TAIL balance, the output mode, MASTER EQ
    {
        int k = 0; for (auto* id : { "bw", "bits", "wander" }) vals[id]->setBounds (cell (gCol, k++, 0));
        k = 0;
        const char* cap[4] = { "Mix", "Out", "Width", "ER/TAIL" };
        for (auto* id : { "mix", "out", "outW", "erBal" }) { vals[id]->fader = false; vals[id]->setCaption (cap[k]); vals[id]->setBounds (cell (gOut, k % 3, k / 3)); ++k; }
        vals["tailRT"]->setVisible (false); vals["tailLvl"]->setVisible (false); tailSeg.setVisible (false);
        erOnSeg.setBounds ({}); tOnSeg.setBounds ({});             // (the power switches are on the ER / TAIL titles)
        outModeSeg.setBounds (segAt (gOut, 1, 1, 2));
        eqOnSeg.setBounds (segAt (gOut, 0, 2, 1)); eqFlatBtn.setBounds (segAt (gOut, 1, 2, 1));
        eqFlatBtn.setButtonText (ph.tr ("フラット", "Flat")); eqFlatBtn.getProperties().set ("fontPx", 10.0);
    }
    // INPUT: into the reverb only (the dry stays)
    {
        const char* cap[4] = { "Width", "Tilt", "Low Cut", "High Cut" };
        int k = 0; for (auto* id : { "inW", "iTilt", "iLC", "iHC" }) { vals[id]->fader = false; vals[id]->setCaption (cap[k]); vals[id]->setBounds (cell (gIn, k % 2, k / 2)); ++k; }
        inModeBox.setBounds (segAt (gIn, 0, 2, 2));
        iLCsBox.setBounds (segAt (gIn, 0, 3, 1)); iHCsBox.setBounds (segAt (gIn, 1, 3, 1));
        iToSeg.setBounds (segAt (gIn, 0, 4, 2));
    }
    layoutInfo();
    earMask.setBounds (0, 52, kRevW, getHeight() - 52);
    layoutReverb();
    layoutDynMod();
    layoutSourceScreen();
    // the ER's source (ROOM / ALGO / IR) lives in its own group now, not in the header
    {   // ROOM / ALGO / IR, and for ROOM which of the two rooms this column edits
        const bool room = (int) ph.value ("src") == 0;
        (void) room;
        srcSeg.setBounds (gEr.getX() + 8, gEr.getY() + 2, 3 * kP - 6, 17);
        erRoomSeg.setBounds (gEr.getX() + 8 + 3 * kP, gEr.getY() + 2, kP - 4, 17); erIrSeg.setBounds (erRoomSeg.getBounds());
    }
    srcPresetBox.setBounds (gSrc.getX() + 8, gSrc.getY() + kGT + 2 * kRP + 57, 4 * kP - 4, 20);
    // the ER page's components: whatever sits in the ER groups (the reverb pages have their own lists)
    erComps.clear();
    layoutSource();                        // gives the source controls their bounds first
    {
        std::set<juce::Component*> other; for (int i = 1; i < 7; ++i) for (auto* c : pageComps[i]) other.insert (c);
        other.insert (&measure); other.insert (&popup);
        for (auto* c : std::initializer_list<juce::Component*> { &echo, &rtGraph, &inGraph, &eqGraph, &erfGraph, &tfGraph, &combo, &tools, &layerBar, &swapBtn, &panel, &panelBtn, &roomView, &info, &now, &help,
                                                                 &deck, &colorDeck, &colorPresetBox, &infoBtn, &detSwitch, &colErBtn, &colTailBtn, &detEditBtn, &detDynBtn, &detModBtn, &dmPreBtn, &lockBtn, &fader, &modern, &viewBtn, &editBtn, &basicBtn, &remoteBtn, &helpBtn, &secBtn, &tIrBtn, &tIrClearBtn, &lesson, &ear, &earMask }) other.insert (c);
        for (auto& id : popIds) other.insert (vals[id].get());
        for (auto* c : srcComps) other.insert (c);
        for (auto* c : std::initializer_list<juce::Component*> { &irPrev, &irNext, &loadBtn, &cmpBtn, &srcRoomAB, &srcIrAB }) other.insert (c);       // (the SOURCE screen's cards stand over the ER column's place)
        for (auto* c : std::initializer_list<juce::Component*> { vals["mix"].get(), vals["out"].get(), vals["outW"].get(), vals["erBal"].get(), &erOnSeg, &tOnSeg, &outModeSeg, &eqOnSeg, &eqFlatBtn,
                                                                 vals["bw"].get(), vals["bits"].get(), vals["wander"].get() }) other.insert (c);       // (the OUTPUT panel; COLOR is in the deck)
        for (auto* c : getChildren())
            if (! other.count (c) && c != &tabs && c->getBounds().intersects (gEr))
                erComps.push_back (c);
    }
    applyPage();
    if (viewMode() != 0 && modeInit) layoutMode();
}

// ---------------------------------------------------------------------------
//  EDIT / BASIC: monitors, deck, notes column
// ---------------------------------------------------------------------------
int MainView::monLayout() const { return 3; }          // fixed: two wide monitors on the left (stacked), one tall on the right
int MainView::deckMode() const { return 0; }          // the deck is SIMPLE, always
bool MainView::infoOn() const { return (bool) proc.uiState.getProperty ("info", true); }
int MainView::infoSel() const { return juce::jlimit (0, 2, (int) proc.uiState.getProperty ("infoSel", 1)); }     // default: what you hear
const char* MainView::viewName (int v, bool ja)
{
    static const char* n[5][2] = { { "スペアナ＋編集", "Spectrum + edit" }, { "エコーグラム", "Echogram" }, { "測定", "Measured" }, { "部屋（音源・リスナー）", "Room (source / listener)" }, { "ER 情報・IR 解析", "ER info / IR analysis" } };
    return n[juce::jlimit (0, 4, v)][ja ? 0 : 1];
}
void MainView::setInfoSel (int i)
{
    const int old = infoSel();
    proc.uiState.setProperty ("infoSel", juce::jlimit (0, 2, i), nullptr);
    if (i == 2 && old != 2 && ! proc.earActive()) startEar();
    else if (i != 2 && proc.earActive()) endEar();
    layoutInfo(); repaint();
}
void MainView::showLockMenuAll()
{
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    const auto ids = proc.lockedIds();
    const bool mixL = proc.isLocked ("mix");
    m.addItem (1, ph.tr ("Mix をロック", "Lock Mix"), true, mixL);
    m.addItem (2, ph.tr ("Mix と Output をロック", "Lock Mix and Output"), true, mixL && proc.isLocked ("out"));
    m.addSeparator();
    m.addSectionHeader (ph.tr ("ロック中（クリックで解除）", "Locked (click to unlock)"));
    for (int i = 0; i < ids.size(); ++i)
    {
        auto* p = ph.get (ids[i]);
        m.addItem (100 + i, p != nullptr ? p->getName (40) : ids[i], true, true);
    }
    if (ids.isEmpty()) m.addItem (3, ph.tr ("（なし：つまみや数値を右クリックでロック）", "(none: right-click a knob or a value)"), false);
    m.addSeparator();
    m.addItem (4, ph.tr ("すべて解除", "Unlock all"), ! ids.isEmpty());
    juce::Component::SafePointer<MainView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lockBtn), [safe, ids] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& P = safe->proc;
        if (r == 1) P.setLocked ("mix", ! P.isLocked ("mix"));
        if (r == 2) { const bool on = ! (P.isLocked ("mix") && P.isLocked ("out")); P.setLocked ("mix", on); P.setLocked ("out", on); }
        if (r == 4) P.uiState.setProperty ("locks", "", nullptr);
        if (r >= 100 && r - 100 < ids.size()) P.setLocked (ids[r - 100], false);
        safe->resized(); safe->repaint();
    });
}
void MainView::showLessonList()
{
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    const auto& L = lessons();
    const int cur = lesson.index();
    for (int i = 0; i < (int) L.size(); ++i)
        m.addItem (1 + i, juce::String (i + 1) + ". " + juce::String::fromUTF8 (ph.ja() ? L[(size_t) i].titleJ : L[(size_t) i].titleE), true, i == cur);
    juce::Component::SafePointer<MainView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&infoTab[0]).withMaximumNumColumns (1), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->proc.uiState.setProperty ("lessonView", r - 1, nullptr);
        safe->setInfoSel (0); safe->lesson.repaint();
    });
}
void MainView::layoutInfo()
{
    // the notes column: tabs (reverb sound / lesson / ear training) over their card (the upper half); help from about the middle,
    // as tall as its text needs, with room left under it
    const bool on = infoOn() && viewMode() == 0;
    const int x = kRevW, w = kInfoW - 12, y0 = 58, H = getHeight();
    const int tabsH = 24;
    const int sel = proc.earActive() ? 2 : infoSel();
    const int pos[3] = { 1, 0, 2 };          // the order on screen: reverb sound, lesson, ear training
    for (int i = 0; i < 3; ++i)
    {
        infoTab[i].setBounds (x + pos[i] * (w / 3), y0, w / 3 - 4, tabsH);
        infoTab[i].setToggleState (i == sel, juce::dontSendNotification);
        infoTab[i].setVisible (on);
    }
    const int mid = juce::jmax (y0 + 300, H / 2 - 20);
    const juce::Rectangle<int> card (x, y0 + tabsH + 6, w, mid - 8 - (y0 + tabsH + 6));
    lesson.setBounds (card); now.setBounds (card); ear.setBounds (card);
    const int maxH = H - 24 - mid;
    help.setBounds (x, mid, w, juce::jlimit (90, maxH, help.neededHeight (w)));
    lesson.setVisible (on && sel == 0); now.setVisible (on && sel == 1); ear.setVisible (on && sel == 2);
    help.setVisible (on);
}
void MainView::layoutDeck()
{
    // BASIC deck (and on EDIT, the strip under the monitors): SIMPLE / BASIC 8 / MACRO, or a classic unit's faceplate
    const int dm = deckMode();
    const bool unitOk = (int) ph.value ("src") == 1 && proc.unitIndex() >= 0;
    const bool pnl = dm == 3 && unitOk;
    if (dm < 3) deck.setLayout (dm);
    const bool show = viewMode() <= 1;
    const bool dk = deckView() == 0;      // (0.10.9) SOURCE / DYN / MOD stand in the deck's place
    deck.setBounds (deckR); deck.setVisible (show && ! pnl && dk);
    panel.setBounds (deckR); panel.setVisible (show && pnl && dk);
    // COLOR at the deck's right end (whatever the deck shows): Bandwidth, Bits, Wander + the era preset under them
    colorR = { deckR.getRight() + 8, deckR.getY(), kColorW, deckR.getHeight() };
    // COLOR (0.8.0): the era preset and where it acts (ER / TAIL); its three values are in the detail panel (EDIT)
    colorDeck.setVisible (false);
    colorBox = colorR.withTrimmedTop (colorR.getHeight() - 64);          // (0.10.12) the COLOR type under the SOURCE / EDIT / DYN | MOD buttons
    colorPresetBox.setBounds (colorBox.getX() + 8, colorBox.getBottom() - 30, colorBox.getWidth() - 16, 22); colorPresetBox.setVisible (show);
    colTailBtn.setBounds (colorBox.getRight() - 8 - 40, colorBox.getY() + 6, 40, 17); colErBtn.setBounds (colTailBtn.getX() - 4 - 30, colorBox.getY() + 6, 30, 17);
    {   // EDIT | DYN + MOD: the detail panel
        auto r = colorR.withTrimmedBottom (colorBox.getHeight() + 8);
        const int bh = (r.getHeight() - 8) / 3;           // SOURCE | EDIT | DYN, MOD (0.10.8: the last row split in two)
        detSrcBtn.setBounds (r.removeFromTop (bh)); r.removeFromTop (4);
        detEditBtn.setBounds (r.removeFromTop (bh)); r.removeFromTop (4);
        { auto last = r.removeFromTop (bh); detDynBtn.setBounds (last.removeFromLeft (last.getWidth() / 2 - 2)); last.removeFromLeft (4); detModBtn.setBounds (last); }
        detSrcBtn.setToggleState (deckView() == 1, juce::dontSendNotification); detSrcBtn.setVisible (show);
        detEditBtn.setToggleState (tabs.shown() == 1, juce::dontSendNotification); detDynBtn.setToggleState (deckView() == 2, juce::dontSendNotification); detModBtn.setToggleState (deckView() == 3, juce::dontSendNotification);
        detEditBtn.setVisible (show); detDynBtn.setVisible (show); detModBtn.setVisible (show);
    }
    colErBtn.setVisible (show); colTailBtn.setVisible (show); colErBtn.toFront (false); colTailBtn.toFront (false); colorPresetBox.toFront (false);       // (above the COLOR group, which would otherwise take its clicks)
    panelBtn.setVisible (false);
    for (int i = 0; i < 4; ++i)
    {
        deckSel[i].setBounds (12, deckR.getY() + i * (deckR.getHeight() / 4), 76, deckR.getHeight() / 4 - 3);
        deckSel[i].setToggleState (i == dm, juce::dontSendNotification);
        deckSel[i].setVisible (false);
    }
    deckSel[3].setEnabled (true);
    deckSel[3].setAlpha (unitOk ? 1.0f : 0.45f);
}
void MainView::showPaneMenu (int pane)
{
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    const int cur = (int) proc.uiState.getProperty ("monV" + juce::String (pane), -1);
    m.addItem (100, ph.tr ("自動（ER／TAIL に合わせる）", "Auto (follows ER / TAIL)"), true, cur < 0);
    for (int v = 2; v < 5; ++v) m.addItem (v + 1, juce::String::fromUTF8 (viewName (v, ph.ja())), true, cur == v);     // (spectrum and echogram are always on)
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&paneBtn[pane]), [this, pane] (int r)
    {
        if (r <= 0) return;
        const int v = r == 100 ? -1 : r - 1;
        // the view can be on one pane only: the pane that had it takes this pane's old one
        if (v >= 0) for (int j = 0; j < 4; ++j) if (j != pane && (int) proc.uiState.getProperty ("monV" + juce::String (j), -1) == v) proc.uiState.setProperty ("monV" + juce::String (j), paneView[pane], nullptr);
        proc.uiState.setProperty ("monV" + juce::String (pane), v, nullptr);
        layoutMonitors(); if (viewMode() != 0) layoutMode(); repaint();
    });
}

void MainView::paintReverbPages (juce::Graphics& g, int page)
{
    auto group = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (theme::panel); g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true)); g.drawFittedText (t, r.getX() + 10, r.getY() + 3, r.getWidth() - 16, 14, juce::Justification::centredLeft, 1, 0.8f);
    };
    auto label = [&] (juce::Component& c, const juce::String& t) { g.setColour (theme::muted); g.setFont (theme::ui (10.5f)); g.drawText (t, c.getX(), c.getY() - 15, 160, 14, juce::Justification::centredLeft); };
    auto note = [&] (juce::Rectangle<int> r, const juce::String& t, int lines)
    { g.setColour (theme::faint); g.setFont (theme::ui (10.5f)); g.drawFittedText (t, r, juce::Justification::topLeft, lines, 0.9f); };
    const juce::String mon = ph.tr ("モニターの「スペアナ＋編集」", "the monitor's \"Spectrum + edit\"");
    // OUTPUT (every page)
    if (erOnSeg.isVisible()) { label (erOnSeg, "ER"); label (tOnSeg, "TAIL"); }
    if (! gMeas.isEmpty())
    {
        group (gMeas, {});
        const bool nar = gMeas.getWidth() < 300;
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f, true));
        g.drawFittedText (nar ? ph.tr ("測定（全体）", "MEASURED") : ph.tr ("測定（いまの設定のリバーブ全体）", "MEASURED (the whole reverb as set)"),
                          gMeas.getX() + 10, gMeas.getY() + 3, gMeas.getWidth() - 110, 14, juce::Justification::centredLeft, 1, 0.8f);
    }
    if (page == 0) return;
    if (page == 5)
    {
        group (gDuck, ph.tr ("ダッキング（ドライが鳴る間リバーブを下げる）", "DUCKING (the reverb steps back under the dry)"));
        group (gGate, ph.tr ("ゲート", "GATE"));
        label (dkToSeg, ph.tr ("かける先", "Applies to")); label (gtOnSeg, "Gate"); label (gtKeySeg, ph.tr ("キー", "Key"));
        group (gDynInfo, ph.tr ("メーター", "METER"));
        note ({ gDynInfo.getX() + 10, gDynV.getBottom() + 6, gDynInfo.getWidth() - 20, gDynInfo.getBottom() - gDynV.getBottom() - 10 },
              ph.tr ("ボーカルや主旋律が鳴っている間だけ残響を下げ、隙間で戻す。Multi は帯域ごと、Spectral は半オクターブごとに、ドライと重なる帯域だけ下げます。",
                     "Lowers the reverb while the vocal or lead plays and lets it back in the gaps. Multi works per band, Spectral per half-octave: only where the dry overlaps."), 3);
        return;
    }
    if (page == 6)
    {
        group (gLfo, ph.tr ("LFO・エンベロープ・コーラス", "LFO / ENVELOPE / CHORUS")); group (gMat, ph.tr ("マトリクス（ソース → 行き先、量）", "MATRIX (source -> destination, amount)"));
        label (mbox ("l1Shape"), ph.tr ("波形", "Shape")); label (mbox ("l1Sync"), ph.tr ("同期", "Sync"));
        label (mbox ("l2Shape"), ph.tr ("波形", "Shape")); label (mbox ("l2Sync"), ph.tr ("同期", "Sync"));
        for (int i = 1; i <= 4; ++i)
        {
            const juce::String n (i);
            label (mbox (("m" + n + "Src").toRawUTF8()), juce::String (i) + ph.tr ("  ソース", "  source"));
            label (mbox (("m" + n + "Dst").toRawUTF8()), ph.tr ("行き先", "destination"));
            // the slot's live value: a bar under it
            const float v = proc.mSum[(size_t) juce::jlimit (0, dm::kDst - 1, (int) ph.value (("m" + n + "Dst").toRawUTF8()))].load();
            auto bar = segAt (gMat, ((i - 1) % 2) * 4 + 1, (i - 1) / 2, 2).translated (0, 21).withHeight (3).toFloat();
            g.setColour (theme::shade (0.3f)); g.fillRect (bar);
            g.setColour (theme::accent); const float cx = bar.getCentreX(); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, cx + v * bar.getWidth() * 0.5f), bar.getY(), std::max (cx, cx + v * bar.getWidth() * 0.5f), bar.getBottom()));
        }
        return;
    }
    if (page == 1)
    {
        const int m = TailSourceSeg::modeOf (ph);
        const bool hybIR = m == 3 && (int) proc.uiState.getProperty ("tEdit", 0) == 0, showIR = m == 2 || hybIR;
        const int engI = (int) ph.value ("tEng"); const bool plate = engI == 1, spring = engI == 2, amb = engI == 3;
        if (false) group (gTView, m == 0 ? ph.tr ("部屋から計算", "FROM THE ROOM") : m == 1 ? (plate ? ph.tr ("プレート", "PLATE") : spring ? ph.tr ("スプリング", "SPRING") : amb ? ph.tr ("アンビエント", "AMBIENT") : ph.tr ("HALL（FDN 16 本）", "HALL (16-LINE FDN)"))
                                                : ph.tr ("IR と鳴る部分（水色）", "IR + PART PLAYED (cyan)"));
        label (rtModeSeg, ph.tr ("RT モード", "RT mode")); label (tLCsBox, "LC slope"); label (tHCsBox, "HC slope");
        if (! showIR) { label (freezeSeg, "Freeze"); label (tEngSeg, ph.tr ("エンジン", "Engine")); }
        else { label (irtRevSeg, "Reverse"); label (irtAutoSeg, ph.tr ("音量合わせ", "Match level")); }
        const auto R = proc.roomParams();
        const auto& M = er::materials();
        auto matName = [&] (int i) { const auto& mm = M[(size_t) std::clamp (i, 0, 6)]; return juce::String::fromUTF8 (ph.ja() ? mm.ja : mm.en); };
        if (m == 0)
        {   // (0.10.5) the room the tail is built from, and what follows from it
            const auto T = proc.tailRoomP();
            const char* shn[6] = { "Shoebox", "Hall (fan)", "Skewed", "Dome", "Connected", "Corridor" };
            const auto A = er::roomAcoustics (T, 1000.0);
            juce::String t = juce::String ((int) ph.value ("tRoomSel") == 1 ? "B" : "A") + ":  "
                           + juce::String (T.W, 1) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (T.D, 1) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (T.H, 1) + " m   "
                           + juce::String (shn[std::clamp (T.shape, 0, 5)]) + (T.shape != 0 ? " " + juce::String ((int) std::lround (T.splay * 100)) + " %" : juce::String()) + "\n"
                           + matName (T.wall) + " / " + matName (T.floor)
                           + (T.clutter > 0.005 ? ph.tr ("   散らかり ", "   clutter ") + juce::String ((int) std::lround (T.clutter * 100)) + " %" : juce::String()) + "\n"
                           + juce::String ((int) A.V) + juce::String::fromUTF8 (" m\xc2\xb3   RT60 ") + juce::String (A.rt, 2) + " s   Size "
                           + juce::String ((int) std::lround (er::roomTailSizeMs (T))) + " ms";
            if (proc.tailDual())
            {
                const auto c = er::coupledDecay (T, 1000.0, er::coupledRoomOf (T, T.sx, T.sy), er::coupledRoomOf (T, T.lx, T.ly));
                t += ph.tr ("   二重勾配 ", "   two slopes ") + juce::String (c.Ta, 2) + " / " + juce::String (c.Tb, 2) + " s";
            }
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.setFont (theme::ui (9.5f));
            g.drawFittedText (t, { gTail.getX() + 8, gTail.getY() + kGT + 2 * kRP + 1, gTail.getWidth() - 16, 38 }, juce::Justification::topLeft, 4, 0.7f);
            if (false) note ({ gTail.getX() + 8, gTail.getY() + kGT + 3 * kRP, gTail.getWidth() - 16, 36 },
                  ph.tr ("部屋は ER ページの ROOM で変えます（ER が ALGO／IR でも有効）。", "Change the room on the ER page (ROOM); it counts even with a ALGO / IR ER."), 3);
        }
        (void) mon; (void) plate; (void) spring;
    }
    else if (page == 3)
    {
        group (gIO, ph.tr ("入出力", "INPUT / OUTPUT"));
        group (gInF, ph.tr ("インプットフィルター（リバーブに入る前）", "INPUT FILTER (before the reverb)"));
        label (inModeBox, ph.tr ("入力", "Input")); label (outModeSeg, ph.tr ("出力", "Output")); label (iToSeg, ph.tr ("かける先", "Applies to"));
        label (iLCsBox, "LC Slope"); label (iHCsBox, "HC Slope");
        note ({ gIO.getX() + 12, gIO.getBottom() - 40, gIO.getWidth() - 24, 34 },
              ph.tr ("入力モードと幅はリバーブへの入力だけに効きます（ドライはそのまま）。出力 Mono はドライも含めた全体をモノにします。",
                     "Input mode and width only change what goes into the reverb (the dry stays). Output Mono folds everything, dry included."), 3);
        note ({ gInF.getX() + 290, gInF.getY() + 30, gInF.getWidth() - 302, gInF.getHeight() - 36 },
              ph.tr ("カーブは", "The curve is on ") + mon + ph.tr ("（スペアナ＋編集の「インプットフィルター」）で、入力（橙）のスペアナに重ねて編集できます。\n\nリバーブに入る低域を切ると濁りが減り、高域を切ると響きが奥に下がります。ドライには効きません。",
                     " (SPECTRUM + EDIT: \"Input filter\"), over the input spectrum (amber).\n\nCutting lows into the reverb clears mud; cutting highs sets it further back. The dry is untouched."), 8);
    }
    else
    {
        group (gEQ, ph.tr ("MASTER EQ（リバーブ出力：ER＋テール）", "MASTER EQ (reverb output: ER + tail)"));
        g.setColour (theme::muted); g.setFont (theme::ui (10.5f)); g.drawText ("EQ", eqOnSeg.getX() - 26, eqOnSeg.getY() + 3, 22, 14, juce::Justification::centredRight);
        // the bands at a glance (edited on the spectrum monitor)
        const int x0 = gEQ.getX() + 16, y0 = gEQ.getY() + 26, cw = 104, rh = 17;
        const char* heads[] = { "", "Freq", "Gain", "Q", "" };
        g.setFont (theme::ui (10.0f, true)); g.setColour (theme::muted);
        for (int c = 1; c < 4; ++c) g.drawText (heads[c], x0 + 70 + (c - 1) * cw, y0, cw - 8, rh, juce::Justification::centredRight);
        for (int i = 0; i < 8; ++i)
        {
            const int y = y0 + rh * (i + 1); const juce::String n (i + 1);
            const bool isBand = i < 6, on = isBand ? ph.value ("eqB" + n) > 0.5f : true;
            const juce::String name = i == 6 ? juce::String ("High Pass") : i == 7 ? juce::String ("Low Pass") : i == 0 ? juce::String ("1 Low Shelf") : i == 5 ? juce::String ("6 High Shelf") : n + " Bell";
            g.setColour (on ? theme::text : theme::faint); g.setFont (theme::ui (10.0f)); g.drawText (name, x0, y, 80, rh, juce::Justification::centredLeft);
            g.setFont (theme::mono (10.0f));
            if (isBand)
            {
                g.drawText (fmtHz (ph.value ("eqF" + n)), x0 + 70, y, cw - 8, rh, juce::Justification::centredRight);
                g.drawText (fmtDb (ph.value ("eqG" + n)), x0 + 70 + cw, y, cw - 8, rh, juce::Justification::centredRight);
                g.drawText (juce::String (ph.value ("eqQ" + n), 2), x0 + 70 + 2 * cw, y, cw - 8, rh, juce::Justification::centredRight);
                if (! on) { g.setColour (theme::faint); g.drawText ("off", x0 + 70 + 3 * cw, y, 40, rh, juce::Justification::centredLeft); }
            }
            else
            {
                const float v = ph.value (i == 6 ? "eqHP" : "eqLP"); const bool off = i == 6 ? v <= 20.5f : v >= 19900;
                g.drawText (off ? juce::String ("Off") : fmtHz (v), x0 + 70, y, cw - 8, rh, juce::Justification::centredRight);
            }
        }
        note ({ gEQ.getX() + 470, gEQ.getY() + 34, gEQ.getWidth() - 482, gEQ.getHeight() - 74 },
              ph.tr ("EQ は", "Operate the EQ on ") + mon + ph.tr ("（スペアナ＋編集の「RT＋MASTER EQ」、黄色の線と右の dB 目盛り）で、リバーブ出力（水色）のスペアナに重ねて操作します。\nドラッグ＝周波数・ゲイン（Shift 微調整）、ホイール＝Q、←→↑↓キー、Option+クリック＝0 dB、ダブルクリック＝オン・オフ、右クリック＝小窓を閉じる。触った点の値だけが点の横に出ます。",
                     " (SPECTRUM + EDIT: \"RT + MASTER EQ\", the yellow line and right dB scale), over the reverb output spectrum (cyan).\nDrag = freq / gain (Shift fine), wheel = Q, arrow keys, Option+click = 0 dB, double-click = on / off, right-click = close the small window. Only the touched point's values appear next to it."), 7);
    }
}

int MainView::page() const { return tabs.page(); }
int MainView::layerFor (int) { return 5; }       // (0.10.16) the six panels on every page
void MainView::flatRT()
{
    proc.undo.beginNewTransaction();
    for (int i = 1; i <= 5; ++i) { ph.setGesture ("rtM" + juce::String (i), 1.0f); ph.setGesture ("mbM" + juce::String (i), 1.0f); }
    proc.uiState.setProperty ("rtAdd", 0, nullptr);
    combo.sel = -1; updatePopup(); combo.repaint();
}
void MainView::flatEQ()
{
    proc.undo.beginNewTransaction();
    for (int i = 1; i <= 6; ++i) ph.setGesture ("eqG" + juce::String (i), 0.0f);
    ph.setGesture ("eqHP", 20.0f); ph.setGesture ("eqLP", 20000.0f);
    proc.uiState.setProperty ("eqAdd", 0, nullptr);
    combo.sel = -1; updatePopup(); combo.repaint();
}
// the right column: the faceplate (when the preset uses one) on top on every page, then the room / info (ER) or the measurement
void MainView::layoutRight() { layoutDeck(); }
bool MainView::specMain() const
{
    for (int i = 0; i < nPanes; ++i) if (paneView[i] == 0) return i == 0;
    return false;
}
NodeGraph* MainView::activeGraph()
{
    switch (layerBar.layer())
    {
        case 5: return &inGraph;          // (six panels: the first one is the one the keys and the pop-up follow)
        default: return &combo;
    }
}
// the tall pane's width that shows the room plan at full height (it is drawn to fit: 16 px margins, 22 above, 78 below), its read-outs need ~300
double MainView::roomPaneWidth (int paneH) const
{
    const auto R = proc.roomParams();
    const double areaH = paneH - 32 - 22 - 78;
    return std::max (300.0, areaH * R.W / std::max (0.1, (double) R.D) + 32);
}
void MainView::layoutMonitors()
{
    // the panes for this layout; the split ratios can be dragged (uiState monR1_L / monR2_L). BASIC: two side by side.
    const int L = monLayout();
    const auto A = monArea; const int g = 8;
    for (auto& r : paneR) r = {};
    splits.clear();
    auto ratio = [&] (int which, double def) { return juce::jlimit (0.15, 0.85, (double) proc.uiState.getProperty ("monR" + juce::String (which) + "_" + juce::String (L), def)); };
    // split rect r at ratio t: a | b (vertical line) or a over b (horizontal line); records the draggable gap
    auto splitV = [&] (juce::Rectangle<int> r, int which, double t, juce::Rectangle<int>& a, juce::Rectangle<int>& b)
    { const int w = (int) std::round ((r.getWidth() - g) * t); a = r.withWidth (w); b = r.withTrimmedLeft (w + g); splits.push_back ({ { r.getX() + w, r.getY(), g, r.getHeight() }, which, true, r }); };
    auto splitH = [&] (juce::Rectangle<int> r, int which, double t, juce::Rectangle<int>& a, juce::Rectangle<int>& b)
    { const int h = (int) std::round ((r.getHeight() - g) * t); a = r.withHeight (h); b = r.withTrimmedTop (h + g); splits.push_back ({ { r.getX(), r.getY() + h, r.getWidth(), g }, which, false, r }); };
    juce::Rectangle<int> x1, x2, x3, x4;
    switch (L)
    {
        case 0: nPanes = 2; splitV (A, 1, ratio (1, 0.5), paneR[0], paneR[1]); break;                                         // side by side
        case 1: nPanes = 2; splitH (A, 1, ratio (1, 0.6), paneR[0], paneR[1]); break;                                         // over / under
        case 2: nPanes = 3; splitH (A, 1, ratio (1, 0.62), paneR[0], x1); splitV (x1, 2, ratio (2, 0.5), paneR[1], paneR[2]); break;   // 1 over 2
        case 3:
        {   // (0.8.0) left: the echogram on top (about a quarter of the height, as small as it goes without its compact form), the spectrum
            // under it; right: the tall pane, as narrow as the room plan (and its read-outs) allow
            nPanes = 3;
            const double rightW = std::max (std::floor (A.getWidth() * 0.2) - g, 256.0);        // (0.10.12) a fifth of the monitors, at least 256 px (the measured table's notes)
            splitV (A, 5, ratio (5, (A.getWidth() - g - rightW) / (double) (A.getWidth() - g)), x1, paneR[2]);
            const double topH = std::max (Echogram::kMinFullH, (int) std::round (A.getHeight() * 0.25));
            splitH (x1, 3, ratio (3, topH / (double) (x1.getHeight() - g)), paneR[1], paneR[0]);
            break;
        }
        case 4: nPanes = 3; splitV (A, 1, ratio (1, 0.38), paneR[2], x1); splitH (x1, 2, ratio (2, 0.5), paneR[0], paneR[1]); break;   // tall left + 2 stacked
        case 5: nPanes = 3; splitH (A, 1, ratio (1, 0.5), x1, paneR[2]); splitV (x1, 2, ratio (2, 0.5), paneR[0], paneR[1]); break;    // 2 over 1
        case 6: nPanes = 3; splitV (A, 1, ratio (1, 0.4), paneR[0], x1); splitV (x1, 2, ratio (2, 0.5), paneR[1], paneR[2]); break;    // 3 columns
        default:
            nPanes = 4; splitV (A, 1, ratio (1, 0.55), x1, x2);
            { const double t = ratio (2, 0.55); splitH (x1, 2, t, paneR[0], paneR[2]); splitH (x2, 2, t, paneR[1], paneR[3]); }
            (void) x3; (void) x4;
    }
    // what each pane shows: its own choice, else automatic (pane 1 spectrum, 2 echogram, 3 measured on TAIL / room on ER, 4 the other)
    {
        const int pg = page();
        auto T = proc.target(); const bool irIn = T && T->valid;
        // automatic: on TAIL the measurement (or the IR analysis while the tail plays an IR), on ER the room / info
        const int third = pg == 1 ? ((int) ph.value ("tSrc") != 0 && irIn ? 4 : 2) : 3;
        const int autoOrder[5] = { 0, 1, third, third == 3 ? 2 : 3, third == 4 ? 3 : 4 };
        bool used[5] { false, false, false, false, false };
        int want[4];
        for (int i = 0; i < nPanes; ++i) want[i] = juce::jlimit (-1, 4, (int) proc.uiState.getProperty ("monV" + juce::String (i), -1));
        // always on screen: the spectrum + EQ / RT (pane 1) and the echogram / spectrogram (pane 2)
        want[0] = 0; want[1] = 1;
        for (int i = 2; i < nPanes; ++i) if (want[i] == 0 || want[i] == 1) want[i] = -1;
        // (0.9.2) SOURCE: the card being worked on decides what the right-hand monitor shows (rooms: the plan, IRs: the analysis)
        if (deckView() != 1) srcFocus = -1;
        else if (srcFocus < 0) srcFocus = (int) ph.proc.uiState.getProperty ("srcFocusT", -1);   // (test hook)
        roomView.slot = srcFocus >= 0 && srcFocus < 2 ? srcFocus : -1; info.focusIr = srcFocus >= 2 ? srcFocus - 2 : -1;
        if (srcFocus >= 0 && nPanes >= 3)
        {
            const int v = srcFocus < 2 ? 3 : 4;
            for (int i = 2; i < nPanes; ++i) if (want[i] == v) want[i] = -1;
            want[2] = v;
        }
        for (int i = 0; i < nPanes; ++i) if (want[i] >= 0 && ! used[want[i]]) { paneView[i] = want[i]; used[want[i]] = true; } else paneView[i] = -1;
        for (int i = 0; i < nPanes; ++i)
            if (paneView[i] < 0)
            {
                int v = autoOrder[juce::jlimit (0, 4, i)];
                if (used[v]) for (int k = 0; k < 5; ++k) if (! used[autoOrder[k]]) { v = autoOrder[k]; break; }
                paneView[i] = v; used[v] = true;
            }
        for (int i = nPanes; i < 4; ++i) paneView[i] = -1;
    }
    auto rectOf = [&] (int v) { for (int i = 0; i < nPanes; ++i) if (paneView[i] == v) return paneR[i]; return juce::Rectangle<int>(); };
    const bool show = viewMode() == 0;
    // the notes switch: in the space freed above the right-hand tall monitor (which is one button row shorter)
    if (L == 3 && nPanes == 3)
    {
        const int bh = 26, gap = 4;
        infoBtn.setBounds (paneR[2].getX(), paneR[2].getY(), paneR[2].getWidth(), bh);
        paneR[2] = paneR[2].withTrimmedTop (bh + gap);
        infoBtn.setVisible (show);
    }
    // spectrum + edit
    specBox = show ? rectOf (0) : juce::Rectangle<int>();
    const bool specOn = ! specBox.isEmpty();
    if (specOn)
    {
        auto inner = specBox.reduced (8, 6);
        auto row = inner.removeFromTop (22);
        const bool showTools = true, toolsLive = activeGraph() == &combo;       // (0.8.0) the same row on every layer; off the RT + EQ layer it is greyed
        tools.live = toolsLive;
        if (showTools)
        {   // the add / flat / undo tools next to the layers, or on a second line when the pane is narrow
            if (row.getWidth() - 470 >= MonitorTools::naturalWidth()) tools.setBounds (row.removeFromRight (MonitorTools::naturalWidth()));
            else { inner.removeFromTop (3); tools.setBounds (inner.removeFromTop (22).withWidth (std::min (inner.getWidth(), MonitorTools::naturalWidth()))); }
        }
        tools.setVisible (showTools);
        layerBar.setBounds (row.withTrimmedRight (8)); layerBar.setVisible (true);
        inner.removeFromTop (4);
        for (auto* gr : { static_cast<NodeGraph*> (&rtGraph), static_cast<NodeGraph*> (&eqGraph), static_cast<NodeGraph*> (&inGraph), static_cast<NodeGraph*> (&erfGraph), static_cast<NodeGraph*> (&tfGraph), static_cast<NodeGraph*> (&combo) })
        {
            rtGraph.tiled = eqGraph.tiled = (layerBar.layer() == 5);
            if (layerBar.layer() == 5)
            {   // (0.10.16) six panels: input, ER, tail filters above; reverb time, master EQ below (the last cell is empty)
                const int w3 = (inner.getWidth() - 8) / 3, h2 = (inner.getHeight() - 4) / 2;
                NodeGraph* cells[6] = { &inGraph, &erfGraph, &tfGraph, &rtGraph, &eqGraph, nullptr };
                int k = -1; for (int q = 0; q < 6; ++q) if (gr == cells[q]) k = q;
                if (k >= 0) { gr->setBounds (inner.getX() + (k % 3) * (w3 + 4), inner.getY() + (k / 3) * (h2 + 4), w3, h2); gr->setVisible (true); continue; }
                gr->setBounds (inner); gr->setVisible (false); continue;
            }
            gr->setBounds (inner); gr->setVisible (gr == activeGraph());
        }
        activeGraph()->repaint();
        if (layerBar.layer() == 5) for (auto* gr : { static_cast<NodeGraph*> (&erfGraph), static_cast<NodeGraph*> (&tfGraph), static_cast<NodeGraph*> (&rtGraph), static_cast<NodeGraph*> (&eqGraph) }) gr->repaint();
    }
    else
    {
        tools.setVisible (false); layerBar.setVisible (false);
        for (auto* gr : { static_cast<NodeGraph*> (&rtGraph), static_cast<NodeGraph*> (&eqGraph), static_cast<NodeGraph*> (&inGraph), static_cast<NodeGraph*> (&erfGraph), static_cast<NodeGraph*> (&tfGraph), static_cast<NodeGraph*> (&combo) }) gr->setVisible (false);
    }
    // echogram
    {
        const auto r = show ? rectOf (1) : juce::Rectangle<int>();
        echo.compact = r.getHeight() < Echogram::kMinFullH;           // (the three views and the time span on every pane that has room)
        echo.setBounds (r); echo.setVisible (! r.isEmpty()); echo.repaint();
    }
    // measured
    {
        gMeas = show ? rectOf (2) : juce::Rectangle<int>();
        measure.setBounds (gMeas.reduced (12, 0).withTrimmedTop (28).withTrimmedBottom (8)); measure.setVisible (! gMeas.isEmpty());
    }
    // room / info (by the ER's source) and the IR analysis (the info panel, always on the IR); with both shown, room / info = the room
    {
        const auto r = show ? rectOf (3) : juce::Rectangle<int>(), ri = show ? rectOf (4) : juce::Rectangle<int>();
        roomView.setBounds (r); roomView.setVisible (! r.isEmpty());          // the room plan (drag the source and the listener), always the room
        auto T = proc.target();
        info.forceIR = T && T->valid && (int) ph.value ("src") != 1;          // IR analysis when an IR is in, else the ALGO model
        info.setBounds (ri); info.setVisible (! ri.isEmpty());
    }
    swapBtn.setVisible (false);
    // the small "what's shown" menu at each pane's top-right
    for (int i = 0; i < 4; ++i)
    {
        const bool on = show && i < nPanes && i >= 2;
        paneBtn[i].setVisible (on);
        if (! on) continue;
        paneBtn[i].setButtonText (juce::String::fromUTF8 (viewName (paneView[i], ph.ja())) + juce::String::fromUTF8 (" \xe2\x96\xbe"));
        paneBtn[i].setBounds (paneR[i].getRight() - 96, paneR[i].getY() + 5, 88, 18);
        paneBtn[i].toFront (false);
    }
    repaint (monArea.expanded (2));
}

void MainView::applyPage()
{
    const int pg = page();
    // a faceplate (ALGO units) stands in the right column on every page
    layoutDeck();
    for (auto* c : erComps) c->setVisible (false);
    for (int i = 1; i < 7; ++i) for (auto* c : pageComps[i]) c->setVisible (shows (i));
    if (shows (0)) { for (auto* c : erComps) c->setVisible (true); }
    layoutTail();
    layoutSource();
    layoutRight();
    layoutMonitors();
    applyDetailVisibility();
    updatePopup();
    if (viewMode() != 0 && modeInit) layoutMode();
    repaint();
}
// the detail panel: which title is open (ER / TAIL / DYN / MOD / INPUT / OUTPUT, or none)
bool MainView::shows (int id) const
{
    if (id == 5) return deckView() == 2;
    if (id == 6) return deckView() == 3;
    if (id >= 8) return deckView() == 1;
    return tabs.shows (id);
}
int MainView::deckView() const { return viewMode() == 0 ? juce::jlimit (0, 3, (int) proc.uiState.getProperty ("deckView", 0)) : 0; }
void MainView::setDeckView (int v)
{
    proc.uiState.setProperty ("deckView", juce::jlimit (0, 3, v), nullptr);
    if (v != 1) srcFocus = -1;
    layoutSourceScreen(); layoutDynMod(); applyPage();
}
int MainView::cardAt (juce::Point<int> p) const { for (int i = 0; i < 4; ++i) if (cardOn[i] && gCard[i].contains (p)) return i; return -1; }
int MainView::srcShow (int kind) const
{
    const char* k = kind == 0 ? "srcRoom" : "srcIr";
    if (proc.uiState.hasProperty (k)) return (int) proc.uiState.getProperty (k) > 0 ? 1 : 0;
    return ph.value (kind == 0 ? "erRoomSel" : "erIrSel") > 0.5f ? 1 : 0;
}
juce::String MainView::roomSummary (const er::RoomP& T, const juce::String& tag, bool dual) const
{
    const auto& M = er::materials();
    auto matName = [&] (int i) { const auto& mm = M[(size_t) std::clamp (i, 0, 6)]; return juce::String::fromUTF8 (ph.ja() ? mm.ja : mm.en); };
    const char* shn[6] = { "Shoebox", "Hall (fan)", "Skewed", "Dome", "Connected", "Corridor" };
    const auto A = er::roomAcoustics (T, 1000.0);
    juce::String t = tag + ":  " + juce::String (T.W, 1) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (T.D, 1) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (T.H, 1) + " m   "
                   + juce::String (shn[std::clamp (T.shape, 0, 5)]) + (T.shape != 0 ? " " + juce::String ((int) std::lround (T.splay * 100)) + " %" : juce::String()) + "\n"
                   + matName (T.wall) + " / " + matName (T.floor)
                   + (T.clutter > 0.005 ? ph.tr ("   散らかり ", "   clutter ") + juce::String ((int) std::lround (T.clutter * 100)) + " %" : juce::String()) + "\n"
                   + juce::String ((int) A.V) + juce::String::fromUTF8 (" m\xc2\xb3   RT60 ") + juce::String (A.rt, 2) + " s   Size "
                   + juce::String ((int) std::lround (er::roomTailSizeMs (T))) + " ms";
    if (dual)
    {
        const auto c = er::coupledDecay (T, 1000.0, er::coupledRoomOf (T, T.sx, T.sy), er::coupledRoomOf (T, T.lx, T.ly));
        t += ph.tr ("   二重勾配 ", "   two slopes ") + juce::String (c.Ta, 2) + " / " + juce::String (c.Tb, 2) + " s";
    }
    return t;
}
// what feeds the ER (part 0) / the TAIL (part 1) now, and how it is made (0.10.10: shown in EDIT's ER and TAIL columns)
void MainView::nowInfo (int part, juce::String& what, juce::String& how) const
{
    const juce::String AB[2] = { "A", "B" };
    const char* engN[4] = { "Hall (FDN)", "Plate", "Spring", "Ambient" };
    if (part == 0)
    {
        const int src = (int) ph.value ("src");
        if (ph.value ("erOn") < 0.5f) { what = "OFF"; how = ph.tr ("ER の電源が切れています。", "The ER is switched off."); return; }
        if (src == 0) { what = "ROOM " + AB[ph.value ("erRoomSel") > 0.5f ? 1 : 0]; how = ph.tr ("部屋の壁・床・天井の鏡像から、反射を 1 本ずつ計算（鏡像法）。", "Each reflection from the mirror images of the room's surfaces (image sources)."); }
        else if (src == 1) { what = "ALGO"; how = ph.tr ("8 つのパラメーターで作る反射の並び（ヴィンテージ機のパターン）。", "A reflection pattern from eight parameters (like a vintage unit)."); }
        else
        {
            const int im = (int) ph.value ("irMode"); what = "IR " + AB[ph.value ("erIrSel") > 0.5f ? 1 : 0];
            how = im == 1 ? ph.tr ("IR の初期部分を畳み込みで再生。", "The IR's early part, by convolution.")
                : im == 2 ? ph.tr ("IR の初期部分を、タップ＋畳み込みで再生。", "The IR's early part, by taps plus convolution.")
                          : ph.tr ("IR の初期部分を反射のタップ列に置き換えて再生。", "The IR's early part as a row of reflection taps.");
        }
        return;
    }
    if (ph.value ("tOn") < 0.5f) { what = "OFF"; how = ph.tr ("TAIL の電源が切れています。", "The tail is switched off."); return; }
    const int m = TailSourceSeg::modeOf (const_cast<ParamHelp&> (ph)); const juce::String fdn (engN[juce::jlimit (0, 3, (int) ph.value ("tEng"))]);
    const juce::String ir = "IR " + AB[ph.value ("tIrSel") > 0.5f ? 1 : 0];
    if (m == 0) { what = "ROOM " + AB[ph.value ("tRoomSel") > 0.5f ? 1 : 0] + " / " + fdn; how = ph.tr ("部屋の容積と吸音から Decay と Size を計算して鳴らす。", "Decay and Size computed from the room's volume and absorption."); }
    else if (m == 1) { what = "ALGO / " + fdn; how = ph.tr ("Decay・Size などを手で設定。", "Decay, Size and the rest set by hand."); }
    else if (m == 2) { what = ir; how = ph.tr ("IR の後半を畳み込みで再生（FDN は止まる）。", "The IR's tail by convolution (the FDN is off)."); }
    else { what = ir + " + ALGO"; how = ph.tr ("頭は IR、後半は ALGO（", "The head is the IR, the rest ALGO (") + fdn + ph.tr ("）へ引き継ぐ。", ")."); }
}
void MainView::toggleCompare (int slot)
{
    if (cmpOn() && cmpSlot() == slot) proc.uiState.setProperty ("cmpOn", false, nullptr);
    else
    {
        proc.uiState.setProperty ("cmpOn", true, nullptr); proc.uiState.setProperty ("cmpSlot", slot, nullptr);
        // the ER's analysis follows erIrSel; while the ER plays something else than an IR it can follow the card compared
        if ((int) ph.value ("src") != 2 && (ph.value ("erIrSel") > 0.5f ? 1 : 0) != slot) { proc.undo.beginNewTransaction(); ph.setGesture ("erIrSel", (float) slot); }
    }
    dirty = true; repaint();
}
const rv::Measure& MainView::irMeasOf (int slot)
{
    const auto& L = proc.rawOf (slot, false); const auto& Rr = proc.rawOf (slot, true);
    const juce::String key = (slot == 1 ? proc.tIrName : proc.irName) + "|" + juce::String ((int) L.size()) + "|" + juce::String (slot == 1 ? proc.tIrVersion.load() : 0);
    if (key == irMKey[slot]) return irM[slot];
    irMKey[slot] = key; irM[slot] = {};
    if (L.size() < 1024 || Rr.size() != L.size()) return irM[slot];
    // from the direct sound (the first sample at 25 % of the peak, less 1 ms)
    float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (Rr[i])));
    size_t t0 = 0; for (size_t i = 0; i < L.size(); ++i) if (std::max (std::abs (L[i]), std::abs (Rr[i])) >= 0.25f * pk) { t0 = i; break; }
    t0 = t0 > 48 ? t0 - 48 : 0;
    irM[slot] = rv::measureIR (std::vector<float> (L.begin() + (long) t0, L.end()), std::vector<float> (Rr.begin() + (long) t0, Rr.end()), er::kSR);
    return irM[slot];
}
// the card's compare line: how far the reverb now is from this IR, for the part chosen
juce::String MainView::compareText (int slot)
{
    if (! cmpOn() || cmpSlot() != slot) return ph.tr ("比較 OFF", "Compare off");
    if (! cardLoaded (2 + slot)) return ph.tr ("IR が読み込まれていません", "No IR loaded");
    const int sc = cmpScopeV(); juce::StringArray out;
    if (sc != 1)
    {
        const auto T = proc.target();
        if ((int) ph.value ("src") == 2 && (ph.value ("erIrSel") > 0.5f ? 1 : 0) != slot) out.add (ph.tr ("ER は別の IR を鳴らし中", "The ER plays the other IR"));
        else if (! T || ! T->valid || proc.tgtSlot != slot) out.add (ph.tr ("ER：解析中…", "ER: analysing..."));
        else if (prev.hasFeat) out.add (ph.tr ("ER 一致度 ", "ER match ") + juce::String (prev.d.score) + "  (" + juce::String (prev.d.total, 2) + ")");
    }
    if (sc != 0)
    {
        const auto& I = irMeasOf (slot); const auto m = proc.measurement();
        if (! I.valid || ! m.valid) out.add (ph.tr ("TAIL：測定待ち…", "TAIL: measuring..."));
        else
        {
            double sum = 0; int n = 0;
            for (int k = 1; k < rv::kMBands - 1; ++k) if (I.t30[(size_t) k] > 0 && m.t30[(size_t) k] > 0) { sum += m.t30[(size_t) k] / I.t30[(size_t) k] - 1; ++n; }
            const int k1 = 4;
            out.add ("T30 1k  IR " + juce::String (I.t30[k1], 2) + ph.tr (" / いま ", " / now ") + juce::String (m.t30[k1], 2) + " s");
            if (n > 0) out.add (ph.tr ("帯域平均の差 ", "Mean diff ") + (sum >= 0 ? "+" : "") + juce::String ((int) std::lround (100.0 * sum / n)) + " %");
            if (sc == 2) out.add ("EDT " + juce::String (m.edtWide - I.edtWide, 2) + " s   C80 " + juce::String (m.c80 - I.c80, 1) + " dB");
        }
    }
    return out.joinIntoString ("\n");
}
void MainView::FocusL::mouseDown (const juce::MouseEvent& e)
{
    if (o == nullptr || o->deckView() != 1) return;
    const int c = o->cardAt (e.getEventRelativeTo (o).getPosition());
    if (c >= 0 && c != o->srcFocus) { o->srcFocus = c; o->layoutMonitors(); o->repaint(); }
}
void MainView::layoutSourceScreen()
{
    // (0.10.9) in the deck's place (782 x 152): ROOM (A | B) | IR (A | B)   (0.10.10: what is playing now moved to EDIT's ER / TAIL)
    const int top = deckR.getY(), gh = deckR.getHeight(), x0 = deckR.getX();
    const int roomW = 8 + 4 * kP + 4 + 150;
    const auto slotR = juce::Rectangle<int> (x0, top, roomW, gh), irR = juce::Rectangle<int> (slotR.getRight() + 6, top, deckR.getRight() - slotR.getRight() - 6, gh);
    gNow = {};
    const int sr = srcShow (0), si = srcShow (1);
    gCard[0] = gCard[1] = slotR; gCard[2] = gCard[3] = irR;
    cardOn[0] = sr == 0; cardOn[1] = sr == 1; cardOn[2] = si == 0; cardOn[3] = si == 1;
    srcRoomAB.setBounds (slotR.getRight() - 50, top + 2, 44, 16); srcIrAB.setBounds (irR.getRight() - 50, top + 2, 44, 16);
    srcRoomAB.toFront (false); srcIrAB.toFront (false);
    // ROOM: the same on A and B - Width | Depth | Height | Order, walls | floor, shape | Splay | Back | Clutter; on the right the numbers, [ER] [TAIL], -> ALGO, copy
    int k = 0; for (auto* key : { "aW", "aD", "aH", "aO" }) srcVals[key]->setBounds (cell (gCard[0], k++, 0));
    k = 0; for (auto* key : { "bW", "bD", "bH", "bO" }) srcVals[key]->setBounds (cell (gCard[1], k++, 0));
    wallA2.setBounds (segAt (gCard[0], 0, 1, 2)); floorA2.setBounds (segAt (gCard[0], 2, 1, 2));
    wallB2.setBounds (segAt (gCard[1], 0, 1, 2)); floorB2.setBounds (segAt (gCard[1], 2, 1, 2));
    for (int i = 0; i < 2; ++i)
    {
        const int sx = gCard[i].getX() + 8, sy = top + kGT + 2 * kRP - 2;
        const int cw = 38, gap = 2, right = sx + 4 * kP - 4, c0 = right - 3 * cw - 2 * gap;
        (i == 0 ? shapeA2 : shapeB2).setBounds (sx, sy + 12, c0 - sx - 4, 20);
        srcVals[i == 0 ? "aS" : "bS"]->setBounds (c0, sy, cw, kKH);
        srcVals[i == 0 ? "aB" : "bB"]->setBounds (c0 + cw + gap, sy, cw, kKH);
        srcVals[i == 0 ? "aC" : "bC"]->setBounds (c0 + 2 * (cw + gap), sy, cw, kKH);
        const int rx = gCard[i].getX() + 8 + 4 * kP + 4, rw = gCard[i].getRight() - 8 - rx, bw = (rw - 4) / 2;
        sendEr[i].setBounds (rx, top + 70, bw, 22); sendTail[i].setBounds (rx + bw + 4, top + 70, rw - bw - 4, 22);
        toAlgo[i].setBounds (rx, top + 96, rw, 22);
        (i == 0 ? copyBABtn : copyABBtn).setBounds (rx, top + 124, rw, 20);
    }
    for (int i = 2; i < 4; ++i)
    {   // IR (A and B the same): arrows | load | arrows | clear;  name, level | compare, ER / TAIL / ALL;  [ER] [TAIL] [T-HEAD] [-> ALGO]
        const int x = gCard[i].getX() + 8, w = gCard[i].getWidth() - 16, bw = (w - 12) / 4, y = top + gh - 28;
        sendEr[i].setBounds (x, y, bw, 22); sendTail[i].setBounds (x + (bw + 4), y, bw, 22); sendHead[i - 2].setBounds (x + 2 * (bw + 4), y, bw, 22);
        toAlgo[i].setBounds (x + 3 * (bw + 4), y, w - 3 * (bw + 4), 22);
        auto r = juce::Rectangle<int> (x, top + kGT + 2, w, 20);
        auto& pv = i == 2 ? irPrev : irBPrev; auto& nx = i == 2 ? irNext : irBNext; auto& ld = i == 2 ? loadBtn : ir2Load; auto& cl = i == 2 ? irAClear : ir2Clear;
        pv.setBounds (r.removeFromLeft (20)); r.removeFromLeft (3);
        cl.setBounds (r.removeFromRight (56)); r.removeFromRight (3);
        nx.setBounds (r.removeFromRight (20)); r.removeFromRight (3);
        ld.setBounds (r);
        const int rx = x + w - 150;
        (i == 2 ? cmpBtn : cmpB).setBounds (rx, top + kGT + 28, 150, 20);
    }
    cmpScope.setBounds (gCard[2].getX() + 8 + gCard[2].getWidth() - 16 - 150, top + kGT + 52, 150, 18);
    const bool on = deckView() == 1;
    for (auto* c : std::initializer_list<juce::Component*> { &irPrev, &irNext, &loadBtn, &cmpBtn }) c->setVisible (on);
    for (auto* c : srcComps) c->setVisible (on);
    {   // the card that is not shown gives its parts no room
        auto V = [&] (const char* k) { return static_cast<juce::Component*> (srcVals[k].get()); };
        std::vector<juce::Component*> off[4] = {
            { V ("aW"), V ("aD"), V ("aH"), V ("aO"), V ("aS"), V ("aB"), V ("aC"), &wallA2, &floorA2, &shapeA2, &copyBABtn },
            { V ("bW"), V ("bD"), V ("bH"), V ("bO"), V ("bS"), V ("bB"), V ("bC"), &wallB2, &floorB2, &shapeB2, &copyABBtn },
            { &irPrev, &irNext, &loadBtn, &cmpBtn, &irAClear, &sendHead[0] },
            { &irBPrev, &irBNext, &ir2Load, &ir2Clear, &cmpB, &sendHead[1] } };
        for (int i = 0; i < 4; ++i)
        {
            if (cardOn[i]) continue;
            for (auto* c : off[i]) c->setBounds ({});
            sendEr[i].setBounds ({}); sendTail[i].setBounds ({}); toAlgo[i].setBounds ({});
        }
    }
    syncSourceButtons();
}
bool MainView::cardLoaded (int card) const { return proc.cardLoaded (card); }
bool MainView::erLit (int card) const { return proc.erLit (card); }
bool MainView::headLit (int slot) const { return proc.headLit (slot); }
bool MainView::tailLit (int card) const { return proc.tailLit (card); }
void MainView::syncSourceButtons()
{
    for (int i = 0; i < 4; ++i)
    {
        const bool ok = cardLoaded (i);
        sendEr[i].setToggleState (erLit (i), juce::dontSendNotification); sendTail[i].setToggleState (tailLit (i), juce::dontSendNotification);
        sendEr[i].setEnabled (ok); sendTail[i].setEnabled (ok); toAlgo[i].setEnabled (ok);
        sendTail[i].setButtonText ((int) ph.value ("tSrc") == 2 ? "BODY" : "TAIL");
        if (i >= 2) { sendHead[i - 2].setToggleState (headLit (i - 2), juce::dontSendNotification); sendHead[i - 2].setEnabled (ok); }
    }
}
void MainView::sendTo (int card, int what) { proc.undo.beginNewTransaction(); proc.routeSend (card, what); syncSourceButtons(); repaint(); }
// -> ALGO on a card: the lit destinations (both when none is lit), or the one chosen in the menu
void MainView::convertCard (int card, int which)
{
    const bool room = card < 2; const int s = room ? card : card - 2;
    bool toEr = which == 1 || which == 3, toTail = which == 2 || which == 3;
    if (which == 0) { toEr = erLit (card); toTail = tailLit (card) || (! room && headLit (s)); }
    if (! toEr && ! toTail) toEr = toTail = true;
    proc.undo.beginNewTransaction();
    if (room) proc.convertRoom (s, toEr, toTail); else proc.convertIr (s, toEr, toTail);
    flash (ph.tr ("→ ALGO", "-> ALGO"), (room ? ph.tr ("部屋 ", "Room ") + juce::String (s == 0 ? "A" : "B") : ph.tr ("IR ", "IR ") + juce::String (s == 0 ? "A" : "B"))
                                          + juce::String (toEr && toTail ? "  ER + TAIL" : toEr ? "  ER" : "  TAIL") + ph.tr ("を ALGO に変換します（Undo で戻せます）", " is converted to ALGO (Undo goes back)"));
    syncSourceButtons(); repaint();
}
// the IR's level envelope (200 bins, 0 to -70 dB) for its card; kept until the file changes
const std::vector<float>& MainView::envOf (int slot)
{
    const auto& L = proc.rawOf (slot, false); const auto& R = proc.rawOf (slot, true);
    const juce::String key = (slot == 1 ? proc.tIrName : proc.irName) + "|" + juce::String ((int) L.size()) + "|" + juce::String (slot == 1 ? proc.tIrVersion.load() : 0);
    if (key == irEnvKey[slot]) return irEnv[slot];
    irEnvKey[slot] = key; irEnv[slot].clear(); irEnvT0[slot] = 0; irEnvLen[slot] = 0;
    if (L.size() < 256 || R.size() != L.size()) return irEnv[slot];
    float pk = 0; for (size_t i = 0; i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
    if (pk <= 1e-9f) return irEnv[slot];
    for (size_t i = 0; i < L.size(); ++i) if (std::max (std::abs (L[i]), std::abs (R[i])) >= 0.25f * pk) { irEnvT0[slot] = (double) i / er::kSR; break; }
    irEnvLen[slot] = (double) L.size() / er::kSR;
    const int bins = 200;
    for (int b = 0; b < bins; ++b)
    {
        const size_t a0 = L.size() * (size_t) b / bins, a1 = std::max (a0 + 1, L.size() * (size_t) (b + 1) / bins);
        float m = 0; for (size_t i = a0; i < a1 && i < L.size(); ++i) m = std::max (m, std::max (std::abs (L[i]), std::abs (R[i])));
        const float db = 20.0f * std::log10 (std::max (1e-7f, m / pk));
        irEnv[slot].push_back (juce::jlimit (0.0f, 1.0f, (db + 70.0f) / 70.0f));
    }
    return irEnv[slot];
}
void MainView::applyDetailVisibility()
{
    for (auto* c : srcComps) c->setVisible (deckView() == 1);
    // IR A's loading row lives on the SOURCE card; another mode's layout used to leave it standing over its panels (once something else had shown it again)
    for (auto* c : std::initializer_list<juce::Component*> { &irPrev, &irNext, &loadBtn, &cmpBtn }) c->setVisible (deckView() == 1);
    layoutEditRoom();
    if (deckView() >= 2) layoutDynMod();
    if (deckView() == 1) layoutSourceScreen();
    dmPreBtn.setVisible (deckView() >= 2); dmPreBtn.toFront (false);      // (0.10.8) the cards' parts may have been moved by the EDIT page
    if (viewMode() != 0) return;
    const bool inOn = shows (3), outOn = shows (4);
    for (auto* c : std::initializer_list<juce::Component*> { vals["inW"].get(), vals["iTilt"].get(), vals["iLC"].get(), vals["iHC"].get(), &inModeBox, &iLCsBox, &iHCsBox, &iToSeg }) c->setVisible (inOn);
    for (auto* c : std::initializer_list<juce::Component*> { vals["mix"].get(), vals["out"].get(), vals["outW"].get(), vals["erBal"].get(), &outModeSeg, &eqOnSeg, &eqFlatBtn }) c->setVisible (outOn);
    erOnSeg.setVisible (false); tOnSeg.setVisible (false);
    for (auto* id : { "bw", "bits", "wander" }) vals[id]->setVisible (outOn);
    if (! shows (0)) for (auto* c : erComps) c->setVisible (false);
    for (int i : { 1, 5, 6 }) if (! shows (i)) for (auto* c : pageComps[i]) c->setVisible (false);
}

void MainView::updatePopup()
{
    NodeGraph* gph = activeGraph();
    const auto ids = gph->isVisible() && gph->sel >= 0 && gph->sel < gph->count() ? gph->ids (gph->sel) : juce::StringArray();
    for (auto& id : popIds) vals[id]->setVisible (false);
    if (ids.isEmpty() || proc.earActive()) { popup.setVisible (false); return; }
    const int n = ids.size(), w = 8 + n * 48, h = 52;                      // (0.7.1) about half the old size
    const auto np = gph->getPosition().toFloat() + gph->nodePos (gph->sel);
    const auto gb = gph->getBounds();
    int x = (int) np.x + 16, y = (int) np.y - h - 10;
    if (x + w > gb.getRight()) x = (int) np.x - w - 16;
    if (y < gb.getY()) y = (int) np.y + 14;
    x = juce::jlimit (gb.getX(), gb.getRight() - w, x); y = juce::jlimit (gb.getY(), gb.getBottom() - h, y);
    popup.setBounds (x, y, w, h); popup.title = gph->title (gph->sel);
    popup.setVisible (true); popup.toFront (false); popup.repaint();
    for (int k = 0; k < n; ++k)
    {
        auto* v = vals[ids[k]].get();
        v->setBounds (x + 4 + k * 48, y + 16, 46, 34); v->setVisible (true); v->toFront (false);
    }
}

void MainView::layoutDynMod()
{
    // (0.10.9) in the deck's place (782 x 152): DYN = DUCKING | GATE | meter + note;  MOD = LFO / ENV / CHORUS (+ the sources' scope) | MATRIX (2 x 2)
    const int top = deckR.getY(), gh = deckR.getHeight(), x0 = deckR.getX();
    auto colW = [] (int n) { return n * kP + 12; };
    gDuck = { x0, top, colW (4), gh }; gGate = { gDuck.getRight() + 6, top, colW (3), gh };
    gDynInfo = { gGate.getRight() + 6, top, deckR.getRight() - gGate.getRight() - 6, gh };
    gDynV = gDynInfo.withTrimmedTop (kGT + 4).reduced (8, 0).withHeight (64);
    gLfo = { x0, top, colW (6), gh }; gMat = { gLfo.getRight() + 6, top, deckR.getRight() - gLfo.getRight() - 6, gh };
    gScope = {};
    dmPreBtn.setBounds (deckR.getRight() - 82, top + 2, 78, 16); dmPreBtn.setButtonText (ph.tr ("プリセット", "Presets"));
    auto at = [&] (const char* id, juce::Rectangle<int> g, int col, int row) { vals[id]->setBounds (cell (g, col, row)); };
    dkModeSeg.setBounds (segAt (gDuck, 0, 0, 4).withY (gDuck.getY() + kGT + 2));
    at ("dkDepth", gDuck, 0, 1); at ("dkThr", gDuck, 1, 1); at ("dkAtk", gDuck, 2, 1); at ("dkRel", gDuck, 3, 1);
    at ("dkBands", gDuck, 0, 2); dkToSeg.setBounds (segAt (gDuck, 1, 2, 3));
    gtOnSeg.setBounds (segAt (gGate, 0, 0, 1)); gtKeySeg.setBounds (segAt (gGate, 1, 0, 2));
    at ("gtThr", gGate, 0, 1); at ("gtHold", gGate, 1, 1); at ("gtRel", gGate, 2, 1);
    dynMeter.setBounds (gDynV);
    // MOD: LFO 1 | LFO 2 on row 0, ENV | CHORUS on row 1, the sources' scope on row 2
    at ("l1Rate", gLfo, 0, 0); mbox ("l1Shape").setBounds (segAt (gLfo, 1, 0, 1)); mbox ("l1Sync").setBounds (segAt (gLfo, 2, 0, 1));
    at ("l2Rate", gLfo, 3, 0); mbox ("l2Shape").setBounds (segAt (gLfo, 4, 0, 1)); mbox ("l2Sync").setBounds (segAt (gLfo, 5, 0, 1));
    at ("enAtk", gLfo, 0, 1); at ("enRel", gLfo, 1, 1); at ("enSens", gLfo, 2, 1);
    at ("chMix", gLfo, 3, 1); at ("chRate", gLfo, 4, 1); at ("chDepth", gLfo, 5, 1);
    for (int i = 1; i <= 4; ++i)
    {   // the matrix 2 x 2: slots 1 | 2 on row 0, 3 | 4 on row 1
        const juce::String n (i); const int c0 = ((i - 1) % 2) * 4, row = (i - 1) / 2;
        mbox (("m" + n + "Src").toRawUTF8()).setBounds (segAt (gMat, c0, row, 1));
        mbox (("m" + n + "Dst").toRawUTF8()).setBounds (segAt (gMat, c0 + 1, row, 2));
        at (("m" + n + "Amt").toRawUTF8(), gMat, c0 + 3, row);
    }
    modScope.setBounds (gLfo.getX() + 6, top + kGT + 2 * kRP + 2, gLfo.getWidth() - 12, gh - kGT - 2 * kRP - 6);
}

void MainView::layoutReverb()
{
    const int top = gSrc.getY();
    gTail = gTl.withWidth (8 + 4 * kP); gRT = gTl.withTrimmedLeft (4 * kP);
    gTView = cell (gTl, 0, 3).withWidth (2 * kP - 4);        // the IR tail's picture (IR / IR+ALGO)
    (void) top;
    gIO = {}; gInF = {}; gEQ = {};
    // (the measurement's place in the right column: layoutRight)
    auto at = [&] (juce::Component* c, juce::Rectangle<int> g, int col, int row) { c->setBounds (cell (g, col, row)); };
    // TAIL, laid out like the ER: SOURCE (ROOM / ALGO / IR / IR+ALGO) | VOICE (any source) | what the source is
    tSrcSeg.setBounds (gTl.getX() + 8, gTl.getY() + 2, 3 * kP - 6, 17);                       // (0.10.8) as wide as the ER's, A | B in the same place
    tRoomSegAB.setBounds (gTl.getX() + 8 + 3 * kP, gTl.getY() + 2, kP - 4, 17); tIrSegAB.setBounds (tRoomSegAB.getBounds());
    hybBtn.setBounds (gTl.getX() + 8 + 4 * kP + 46, gTl.getY() + 2, 2 * kP - 50, 17);     // (0.10.7) in the VOICE title strip
    // VOICE: the two right-hand columns, then the bottom row under the source
    // (0.10.7) the same grid as the ER's VOICE:  Delay | Level / Width | Diffusion / Low Cut | High Cut / (own: Mod Depth | Mod Rate) / LC | HC slope
    at (vals["tPre"].get(), gTl, 4, 0); at (vals["tLvl"].get(), gTl, 5, 0); at (vals["tSpread"].get(), gTl, 4, 1); at (vals["tDiff"].get(), gTl, 5, 1);
    at (vals["tModD"].get(), gTl, 4, 2); at (vals["tModR"].get(), gTl, 5, 2); at (vals["tLC"].get(), gTl, 4, 3); at (vals["tHC"].get(), gTl, 5, 3);
    tLCsBox.setBounds (segAt (gTl, 4, 4, 1)); tHCsBox.setBounds (segAt (gTl, 5, 4, 1));
    // the source side: RT mode / bands / hand-over on row 3, the tail's other stereo and texture controls on row 4
    rtModeSeg.setBounds (segAt (gTl, 0, 3, 2));
    at (vals["mbN"].get(), gTl, 2, 3); at (vals["irtB"].get(), gTl, 3, 3);
    at (vals["tSpreadHi"].get(), gTl, 0, 4); at (vals["tBassW"].get(), gTl, 1, 4); at (vals["tShape"].get(), gTl, 2, 4); at (vals["tFill"].get(), gTl, 3, 4);
    irtView.setBounds ({});            // (the IR's picture: the monitors' IR analysis)
    layoutTail();
    layoutRight();
    layoutMonitors();
    updatePopup();
}

// the TAIL page's SOURCE group: what it holds depends on the source, as on the ER page
void MainView::layoutTail()
{
    const int m = TailSourceSeg::modeOf (ph);
    lastTailMode = (m * 2 + (int) proc.uiState.getProperty ("tEdit", 0)) * 8 + (int) ph.value ("tEng");
    const bool hybIR = m == 3 && (int) proc.uiState.getProperty ("tEdit", 0) == 0;
    const bool showIR = m == 2 || hybIR, showAlgo = m <= 1 || (m == 3 && ! hybIR);
    auto at = [&] (juce::Component* c, int col, int row) { c->setBounds (cell (gTail, col, row)); };
    if (m == 0)
    {   // ROOM: the length comes from the room; Diffusion, Freeze and the engine stay yours
        freezeSeg.setBounds (segAt (gTail, 2, 0, 1));
        roomAlgoBtn.setBounds (segAt (gTail, 0, 0, 2));
        tEngSeg.setBounds (segAt (gTail, 0, 1, 3));
    }
    else
    {
        at (vals["tDecay"].get(), 0, 0); at (vals["tSize"].get(), 1, 0); 
        freezeSeg.setBounds (segAt (gTail, 2, 0, 1));
        tEngSeg.setBounds (segAt (gTail, 0, 1, 4));
    }
    // AMBIENT's own two (row 3, on the right)
    at (vals["aGrav"].get(), 2, 2); at (vals["aFb"].get(), 3, 2);
    at (vals["irtX"].get(), 0, 0); at (vals["irtLen"].get(), 1, 0); at (vals["irtSt"].get(), 2, 0);
    irtRevSeg.setBounds (segAt (gTail, 3, 0, 1));
    at (vals["irtDL"].get(), 0, 1); at (vals["irtDM"].get(), 1, 1); at (vals["irtDH"].get(), 2, 1); at (vals["irtLvl"].get(), 3, 1);
    {
        // (0.10.7) four one-column items: they stay inside the source side (the VOICE grid starts at column 4)
        tIrBtn.setBounds (segAt (gTail, 0, 2, 1)); tIrBtn.setButtonText (ph.tr ("テール IR", "Tail IR"));
        tIrClearBtn.setBounds (segAt (gTail, 1, 2, 1)); tIrClearBtn.setButtonText (ph.tr ("ER と同じ", "As ER"));
        fdnBtn.setBounds (segAt (gTail, 2, 2, 1));
        irtAutoSeg.setBounds (segAt (gTail, 3, 2, 1));
        for (auto* b : { &tIrBtn, &tIrClearBtn, &fdnBtn }) b->getProperties().set ("fontPx", 9.0);
    }
    roomAlgoBtn.setButtonText (juce::String::fromUTF8 ("ROOM \xe2\x86\x92 ALGO"));
    hybBtn.setButtonText (ph.tr (hybIR ? "編集：IR" : "編集：ALGO", hybIR ? "Edit: IR" : "Edit: ALGO"));
    if (viewMode() != 0) return;
    const bool pg = shows (1);
    for (auto* c : std::initializer_list<juce::Component*> { &freezeSeg, &tEngSeg }) c->setVisible (pg && showAlgo);
    vals["tDiff"]->setVisible (pg);
    for (auto* c : std::initializer_list<juce::Component*> { vals["tDecay"].get(), vals["tSize"].get() }) c->setVisible (pg && showAlgo && m != 0);
    roomAlgoBtn.setVisible (pg && m == 0); tRoomSegAB.setVisible (pg && m == 0); tIrSegAB.setVisible (pg && (m == 2 || m == 3));
    const bool amb = (int) ph.value ("tEng") == 3;
    for (auto* c : std::initializer_list<juce::Component*> { vals["aGrav"].get(), vals["aFb"].get() }) c->setVisible (pg && showAlgo && amb);
    for (auto* c : std::initializer_list<juce::Component*> { vals["irtX"].get(), vals["irtLen"].get(), vals["irtSt"].get(), vals["irtLvl"].get(), vals["irtDL"].get(), vals["irtDM"].get(), vals["irtDH"].get(),
                                                             &irtRevSeg, &irtAutoSeg, &tIrBtn, &tIrClearBtn, &fdnBtn }) c->setVisible (pg && showIR);
    hybBtn.setVisible (pg && m == 3);
    irtView.setVisible (false);
    // VOICE: what does not apply to this source is dimmed
    vals["tModD"]->dimmed = vals["tModR"]->dimmed = vals["tDiff"]->dimmed = [this] { return TailSourceSeg::modeOf (ph) == 2; };
}

// ---------------------------------------------------------------------------
//  Display modes
// ---------------------------------------------------------------------------
juce::Point<int> MainView::designSize() const
{
    switch (viewMode())
    {
        case 1: return { kRevW, kDeckH };
        case 2: return { 440, 900 };
        case 3: return { 1000, 640 };
        default: return { kRevW + (infoOn() ? kInfoW : 0), tabs.open() ? kEditH : kEditClosedH };     // (closed: the titles stay)
    }
}
void MainView::initMode()
{
    modeInit = true;
    if (viewMode() == 0 && onDesignChange) onDesignChange();       // (EDIT: with or without the notes column)
    if (viewMode() != 0)
    {   // remember the full screen's visibility first, then switch
        fullVisible.clear(); for (auto* c : getChildren()) if (c->isVisible()) fullVisible.push_back (c);
        if (onDesignChange) onDesignChange();
        layoutMode();
    }
}
void MainView::setViewMode (int m)
{
    m = juce::jlimit (0, 3, m); const int old = viewMode(); if (m == old) return;
    if (old == 0) { fullVisible.clear(); for (auto* c : getChildren()) if (c->isVisible()) fullVisible.push_back (c); }
    proc.uiState.setProperty ("view", m, nullptr); if (m == 1) proc.uiState.setProperty ("deckRemote", true, nullptr);
    proc.uiState.setProperty ("monSwap", false, nullptr);
    if (m == 0)
    {
        for (auto* c : { static_cast<juce::Component*> (&deck), static_cast<juce::Component*> (&fader), static_cast<juce::Component*> (&modern), static_cast<juce::Component*> (&helpBtn) }) c->setVisible (false);
        for (auto* c : fullVisible) c->setVisible (true);
        layerBar.setLayer (layerFor (page()));
        monArea = {};
        if (onDesignChange) onDesignChange();
        resized(); applyPage(); repaint();
        return;
    }
    if (m == 1 || m == 3) layerBar.setLayer (0);
    if (onDesignChange) onDesignChange();
    layoutMode(); repaint();
}
void MainView::placeViewButtons (int x, int y, int h)
{
    editBtn.setBounds (x, y, 50, h); remoteBtn.setBounds (x + 50, y, 98, h); basicBtn.setBounds ({});
    syncViewButtons();
}
void MainView::syncViewButtons()
{
    const int vm = viewMode();
    editBtn.setToggleState (vm == 0, juce::dontSendNotification);
    remoteBtn.setToggleState (vm >= 1, juce::dontSendNotification);
    remoteBtn.setButtonText (ph.tr ("リモコン", "Remote")); remoteBtn.getProperties().set ("fontPx", ph.ja() ? 9.5 : 10.0);
}
void MainView::showRemoteMenu()
{
    juce::PopupMenu menu; menu.setLookAndFeel (&lnf); const int vm = viewMode();
    menu.addItem (2, ph.tr ("デッキリモコン（操作デッキ＋COLOR）", "Deck remote (the deck + COLOR)"), true, vm == 1);
    menu.addItem (3, ph.tr ("フェーダーリモコン（'80s）", "Fader remote ('80s)"), true, vm == 2);
    menu.addItem (4, ph.tr ("モダン（つまみ列＋RT / EQ グラフ）", "Modern (knob row + RT / EQ graph)"), true, vm == 3);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&remoteBtn), [this] (int r) { if (r > 0) { setViewMode (r - 1); syncViewButtons(); } });
}
void MainView::showViewMenu()
{
    juce::PopupMenu menu; menu.setLookAndFeel (&lnf); const int vm = viewMode();
    menu.addItem (1, ph.tr ("EDIT（すべてのパラメーター＋モニター）", "EDIT (every parameter + monitors)"), true, vm == 0);
    menu.addSectionHeader ("REMOTE");
    menu.addItem (2, ph.tr ("デッキリモコン（操作デッキ＋COLOR）", "Deck remote (the deck + COLOR)"), true, vm == 1);
    menu.addItem (3, ph.tr ("フェーダーリモコン（'80s）", "Fader remote ('80s)"), true, vm == 2);
    menu.addItem (4, ph.tr ("モダン（つまみ列＋RT / EQ グラフ）", "Modern (knob row + RT / EQ graph)"), true, vm == 3);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&viewBtn), [this] (int r) { if (r > 0) setViewMode (r - 1); });
}
void MainView::layoutMode()
{
    const int vm = viewMode(); if (vm == 0) return;
    modeAllowed.clear();
    auto allow = [&] (juce::Component& c, juce::Rectangle<int> r, bool show = true) { modeAllowed.insert (&c); if (! r.isEmpty()) c.setBounds (r); if (show) c.setVisible (true); };
    const juce::String names[] = { "EDIT", "DECK", "REMOTE", "MODERN" };
    viewBtn.setButtonText (juce::String::fromUTF8 ("VIEW: ") + names[vm]);
    auto allowPopup = [&]
    {
        modeAllowed.insert (&popup);
        for (auto& id : popIds) modeAllowed.insert (vals[id].get());
    };
    if (vm == 2)
    {   // fader remote: the remote and a small strip on top
        placeViewButtons (10, 8, 24); allow (editBtn, {}); allow (remoteBtn, {});
        allow (presetBox, { 164, 8, 200, 24 }); allow (prevP, { 370, 8, 28, 24 }); allow (nextP, { 402, 8, 28, 24 });
        allow (fader, { 10, 40, 420, 852 });
    }
    else
    {
        allow (zoom, { 198, 14, 44, 22 }); allow (undoBtn, { 246, 13, 24, 24 }); allow (redoBtn, { 272, 13, 24, 24 });
        allow (presetBox, { 304, 12, 200, 26 }); allow (prevP, { 508, 12, 24, 26 }); allow (nextP, { 534, 12, 24, 26 });
        placeViewButtons (564, 12, 26); allow (editBtn, {}); allow (remoteBtn, {}); allow (langBtn, { 952, 12, 36, 26 });
        allowPopup();
        if (vm == 1)
        {
            // deck remote: the deck (SIMPLE / BASIC 8 / MACRO / UNIT) with COLOR at its right end, nothing else
            deckR = { 92, 58, kRevW - 104 - kColorW - 8, 156 };
            layoutDeck();
            if (deck.isVisible()) modeAllowed.insert (&deck);
            if (panel.isVisible()) modeAllowed.insert (&panel);
            modeAllowed.insert (&colorDeck); modeAllowed.insert (&colorPresetBox);
            for (auto& b : deckSel) modeAllowed.insert (&b);
        }
        else
        {   // modern remote: the knob row and the RT (%) + EQ (dB) graph
            allow (modern, { 12, 58, 976, 176 });
            allow (tools, { 988 - MonitorTools::naturalWidth(), 240, MonitorTools::naturalWidth(), 22 });
            allow (combo, { 12, 266, 976, 364 });
            for (auto* gr : { static_cast<juce::Component*> (&rtGraph), static_cast<juce::Component*> (&eqGraph), static_cast<juce::Component*> (&inGraph), static_cast<juce::Component*> (&erfGraph), static_cast<juce::Component*> (&tfGraph) }) gr->setVisible (false);
            specBox = {};
        }
    }
    enforceMode();
    updatePopup();
    repaint();
}
void MainView::enforceMode()
{
    if (viewMode() == 0 || ! modeInit) return;
    for (auto* c : getChildren()) if (c->isVisible() && ! modeAllowed.count (c)) c->setVisible (false);
}

juce::String MainView::pageSection() const
{
    switch (page()) { case 0: return "ER"; case 3: return "IO"; case 4: return "EQ"; case 5: return "DYN"; case 6: return "MOD"; default: return "TAIL"; }
}
void MainView::showSectionMenu (int forPage, juce::Rectangle<int> at)
{
    const auto sec = forPage < 0 ? pageSection() : forPage == 0 ? juce::String ("ER") : forPage == 3 ? juce::String ("IO") : forPage == 4 ? juce::String ("EQ")
                   : forPage == 5 ? juce::String ("DYN") : forPage == 6 ? juce::String ("MOD") : juce::String ("TAIL");
    const juce::String nm = sec == "IO" ? juce::String ("I/O") : sec;
    juce::PopupMenu m; m.setLookAndFeel (&lnf);
    // the IR-fit presets (group 3, "(IR fit)"), filed by type; ids base + preset index
    auto irFitMenu = [this] (int base)
    {
        juce::PopupMenu all; const auto& P = presets();
        std::vector<std::string> types;
        for (int i = 0; i < (int) P.size(); ++i)
            if (P[(size_t) i].group == 3 && std::string (P[(size_t) i].name).find ("(IR fit)") != std::string::npos)
                if (auto* c = presetCat (P[(size_t) i].name)) if (std::find (types.begin(), types.end(), c->type) == types.end()) types.push_back (c->type);
        for (auto& ty : types)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) P.size(); ++i)
                if (P[(size_t) i].group == 3 && std::string (P[(size_t) i].name).find ("(IR fit)") != std::string::npos)
                    if (auto* c = presetCat (P[(size_t) i].name); c != nullptr && ty == c->type) sub.addItem (base + i, juce::String::fromUTF8 (P[(size_t) i].name));
            all.addSubMenu (juce::String (ty), sub);
        }
        return all;
    };
    m.addSectionHeader (nm + ph.tr (" のプリセット（この部分だけ）", " presets (this part only)"));
    if (sec == "ER")
    {   // the built-in ER presets (real spaces, classic units): only the ER changes, the tail and the rest stay
        const auto& P = presets();
        juce::PopupMenu rooms, units;
        for (int i = 0; i < (int) P.size(); ++i)
            if (P[(size_t) i].group <= 1) (P[(size_t) i].group == 0 ? rooms : units).addItem (1000 + i, juce::String::fromUTF8 (P[(size_t) i].name));
        m.addSubMenu (ph.tr ("内蔵：実在の空間（ROOM）", "Built-in: real spaces (ROOM)"), rooms);
        m.addSubMenu (ph.tr ("内蔵：往年の機材（ALGO＋COLOR）", "Built-in: classic units (ALGO + COLOR)"), units);
        m.addSubMenu (ph.tr ("内蔵：IR フィット（実測 IR に合わせた ER）", "Built-in: IR fits (ER matched to a measured IR)"), irFitMenu (1000));
        m.addSeparator();
        m.addSectionHeader (ph.tr ("ユーザー", "User"));
    }
    else if (sec == "TAIL")
    {   // the built-in IR fits: only the tail changes, the ER and the rest stay
        m.addSubMenu (ph.tr ("内蔵：IR フィット（実測 IR に合わせたテール）", "Built-in: IR fits (tail matched to a measured IR)"), irFitMenu (3000));
        m.addSeparator();
        m.addSectionHeader (ph.tr ("ユーザー", "User"));
    }
    const auto files = proc.sectionPresets (sec);
    for (int i = 0; i < files.size(); ++i) m.addItem (100 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) m.addItem (-1, ph.tr ("（まだありません）", "(none yet)"), false);
    m.addSeparator();
    m.addItem (1, ph.tr ("いまの ", "Save the current ") + nm + ph.tr (" を保存…", "..."));
    m.addItem (2, ph.tr ("フォルダーを開く", "Open the folder"));
    juce::Component::SafePointer<MainView> safe (this);
    auto opt = at.isEmpty() ? juce::PopupMenu::Options().withTargetComponent (&secBtn)
                            : juce::PopupMenu::Options().withTargetScreenArea (tabs.localAreaToGlobal (at));
    m.showMenuAsync (opt, [safe, sec, nm, files] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 3000) { safe->proc.applyPresetPart (r - 3000, "TAIL"); safe->flash ("TAIL", safe->ph.tr ("TAIL だけ読み込みました：", "TAIL only: ") + juce::String::fromUTF8 (presets()[(size_t) (r - 3000)].name)); return; }
        if (r >= 1000) { safe->proc.applyPresetPart (r - 1000, "ER"); safe->flash ("ER", safe->ph.tr ("ER だけ読み込みました：", "ER only: ") + juce::String::fromUTF8 (presets()[(size_t) (r - 1000)].name)); return; }
        if (r >= 100) { if (safe->proc.loadSectionPreset (files[r - 100])) safe->flash (nm, safe->ph.tr ("読み込みました：", "Loaded: ") + files[r - 100].getFileNameWithoutExtension()); return; }
        if (r == 2) { auto d = ERProcessor::sectionDir (sec); d.createDirectory(); d.startAsProcess(); return; }
        auto* w = new juce::AlertWindow (nm + safe->ph.tr (" のプリセットを保存", " preset"),
                                         safe->ph.tr ("この部分のパラメーターだけを名前を付けて保存します（同じ名前は上書き）。", "Saves only this part's parameters under a name (the same name is overwritten)."),
                                         juce::MessageBoxIconType::NoIcon, safe.getComponent());
        w->addTextEditor ("name", safe->ph.tr ("マイ ", "My ") + nm);
        w->addButton (safe->ph.tr ("保存", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (safe->ph.tr ("キャンセル", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, sec, nm] (int rr)
        {
            if (rr != 1 || safe == nullptr) return;
            const auto f = safe->proc.saveSectionPreset (sec, w->getTextEditorContents ("name"));
            safe->flash (nm, f.existsAsFile() ? safe->ph.tr ("保存しました：", "Saved: ") + f.getFileNameWithoutExtension() : safe->ph.tr ("保存できませんでした。", "Could not save."));
        }), true);
    });
}
void MainView::loadTailIrFile()
{
    chooser = std::make_unique<juce::FileChooser> (ph.tr ("テール用の IR を選ぶ", "Choose an IR for the tail"), juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<MainView> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
        const auto e = safe->proc.loadTailIR (fc.getResult());
        if (e.isNotEmpty()) { safe->flash ("IR", e); return; }
        safe->proc.undo.beginNewTransaction();
        if ((int) safe->ph.value ("tSrc") == 0) safe->ph.setGesture ("tSrc", 1.0f);
        safe->flash ("IR", safe->ph.tr ("テール用 IR：", "Tail IR: ") + fc.getResult().getFileName());
        safe->irtView.repaint();
    });
}

void MainView::fillSourcePresets()
{
    const int src = (int) ph.value ("src");
    if (src == srcPresetFor) return;
    srcPresetFor = src;
    srcPresetBox.clear (juce::dontSendNotification);
    srcPresetBox.setVisible (src != 2);
    const auto& P = presets();
    for (int i = 0; i < (int) P.size(); ++i)
        if ((src == 0 && P[(size_t) i].group == 0) || (src == 1 && P[(size_t) i].group == 1))
            srcPresetBox.addItem (juce::String::fromUTF8 (P[(size_t) i].name), i + 1);
    srcPresetBox.setTextWhenNothingSelected (src == 0 ? ph.tr ("部屋を選ぶ", "Room preset") : ph.tr ("ER の形を選ぶ", "ER shape preset"));
}

// (0.10.5) EDIT's ER column, ROOM tab: the same items as the SOURCE card, for room A or B
void MainView::layoutEditRoom()
{
    // (0.10.8) the room's own parameters now live only on the SOURCE screen; here the ER column keeps which source / room / IR it uses
    const bool ed = tabs.shown() == 1 && viewMode() == 0; const int src = (int) ph.value ("src");
    erRoomSeg.setVisible (ed && src == 0); erIrSeg.setVisible (ed && src == 2);
}

void MainView::layoutSource()
{
    const int src = (int) ph.value ("src");
    for (auto* c : roomSet) c->setVisible (src == 0);
    for (auto* c : patSet) c->setVisible (src == 1);
    for (auto* c : irSet) c->setVisible (src == 2);
    if (viewMode() == 0) layoutMonitors();

    fillSourcePresets();
    auto at = [&] (juce::Component* c, int col, int row) { c->setBounds (cell (gSrc, col, row)); };
    if (src == 0)
    {   // (0.10.5) the SOURCE card's own controls stand here, for whichever room feeds the ER; only Order is shared
        for (auto* c : std::initializer_list<juce::Component*> { roomSet[0], roomSet[1], roomSet[2], roomSet[3], &wallBox, &floorBox }) c->setBounds ({});
        const int yb = gSrc.getY() + kGT + 2 * kRP + 18;
        r2pBtn.setBounds (gSrc.getX() + 8, yb, 2 * kP - 4, 19); r2pHybBtn.setBounds (gSrc.getX() + 8 + 2 * kP, yb, 2 * kP - 4, 19);
        srcPresetBox.setBounds (gSrc.getX() + 8, yb + 26, 4 * kP - 4, 20);
    }
    else if (src == 1)
    {
        const int order[8] = { 0, 1, 2, 3, 5, 6, 7, 4 };      // First Length Refl Growth / Energy Envelope Damping Variation
        for (int i = 0; i < 8; ++i) at (patSet[(size_t) order[i]], i % 4, i / 4);
        at (patSet[8], 0, 2);
    }
    else
    {
        at (irSet[0], 0, 0); at (irSet[1], 1, 0);
        irMode.setBounds (segAt (gSrc, 2, 0, 2));
        irAuto.setBounds (segAt (gSrc, 0, 1, 2)); irWet.setBounds (segAt (gSrc, 2, 1, 2));
        at (irSet[5], 0, 2); at (irSet[6], 1, 2);
    }
    applyDetailVisibility();
    repaint();
}

void MainView::mouseMove (const juce::MouseEvent& e)
{
    if (e.eventComponent != this) return;
    for (auto& sp : splits) if (sp.hit.expanded (2).contains (e.getPosition())) { setMouseCursor (sp.vertical ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor); return; }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}
void MainView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSplit < 0 || dragSplit >= (int) splits.size()) return;
    const auto sp = splits[(size_t) dragSplit];
    const auto p = e.getEventRelativeTo (this).getPosition();
    const double t = sp.vertical ? (double) (p.x - sp.span.getX() - 4) / (sp.span.getWidth() - 8) : (double) (p.y - sp.span.getY() - 4) / (sp.span.getHeight() - 8);
    const int L = monLayout();
    proc.uiState.setProperty ("monR" + juce::String (sp.which) + "_" + juce::String (L), juce::jlimit (0.15, 0.85, t), nullptr);
    layoutMonitors(); updatePopup();
    // the split list was rebuilt: find the same one again
    for (int i = 0; i < (int) splits.size(); ++i) if (splits[(size_t) i].which == sp.which && splits[(size_t) i].vertical == sp.vertical) { dragSplit = i; break; }
}
void MainView::mouseUp (const juce::MouseEvent&) { dragSplit = -1; }
void MainView::mouseDown (const juce::MouseEvent& e)
{
    dragSplit = -1;
    if (e.eventComponent == this)
        for (int i = 0; i < (int) splits.size(); ++i) if (splits[(size_t) i].hit.expanded (2).contains (e.getPosition())) { dragSplit = i; return; }
    if (deckView() == 1)                                       // (0.10.5) the card's title / background too, not only its controls
        if (const int c = cardAt (e.getEventRelativeTo (this).getPosition()); c >= 0 && c != srcFocus) { srcFocus = c; layoutMonitors(); repaint(); }
    // clicks anywhere in the editor give it the keyboard (arrow keys), except while typing a value
    if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) == nullptr) grabKeyboardFocus();
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
        if (c->getProperties().contains ("pid")) { keyPid = c->getProperties()["pid"].toString(); break; }
}

bool MainView::keyPressed (const juce::KeyPress& k)
{
    if (proc.earActive()) return false;           // ear training: no hidden changes, no undo into the hidden settings
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    const int kc = k.getKeyCode();
    if ((kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey) && k.getModifiers().isCommandDown())
    { proc.applyPreset (presetStep (proc.currentPreset, kc == juce::KeyPress::rightKey ? 1 : -1)); return true; }
    if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey || ((kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey) && ! k.getModifiers().isCommandDown()))
    {
        auto* p = ph.get (keyPid); if (p == nullptr) return false;
        const float dir = (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey) ? 1.0f : -1.0f; const bool fine = k.getModifiers().isShiftDown();
        const auto lab = p->getLabel(); const float v = ph.value (keyPid); const auto rg = p->getNormalisableRange();
        float nv;
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) nv = dir > 0 ? 1.0f : 0.0f;
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) nv = (float) juce::jlimit (0, c->choices.size() - 1, c->getIndex() + (int) dir);
        else if (auto* ip = dynamic_cast<juce::AudioParameterInt*> (p)) nv = (float) juce::jlimit (ip->getRange().getStart(), ip->getRange().getEnd(), ip->get() + (int) dir);
        else if ((lab == "Hz") && rg.start > 0) nv = v * std::pow (2.0f, dir * (fine ? 1.0f / 48.0f : 1.0f / 12.0f));
        else if (lab == "dB") nv = v + dir * (fine ? 0.1f : 0.5f);
        else if (lab == "ms" && rg.start > 0) nv = v * std::pow (fine ? 1.02f : 1.1f, dir);
        else if (lab == "ms") nv = v + dir * (fine ? 0.1f : 1.0f);
        else nv = v + dir * (fine ? 0.2f : 1.0f) * std::max (0.01f, (rg.end - rg.start) / 100.0f);
        proc.undo.beginNewTransaction();
        ph.setGesture (keyPid, rg.snapToLegalValue (juce::jlimit (rg.start, rg.end, nv)));
        return true;
    }
    return false;
}

void MainView::updateSpectra()
{
    // direct (input) and ER (wet) spectra for the FILTER graph: 4096-point FFT, 1/6-octave, smoothed fall
    // CPU: 15 Hz (every other tick), the window and the band edges worked out once, buffers kept, the ER-filter spectra only
    // while that graph shows, and nothing at all once the input has been silent long enough for every curve to fall away
    if ((++specTick & 1) != 0) return;
    const int order = 12, N = 1 << order, P = 120;
    static juce::dsp::FFT fft (order);
    static std::vector<float> win;
    if ((int) win.size() != N) { win.resize ((size_t) N); for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2 * juce::MathConstants<float>::pi * (i + 0.5f) / N); }
    const double sr = std::max (8000.0, proc.getSampleRate());
    if (sr != specSr) { specSr = sr; specB0.resize ((size_t) P + 1); specB1.resize ((size_t) P + 1);
        for (int q = 0; q <= P; ++q) { const double f = 20 * std::pow (1000.0, (double) q / P), lo = f / 1.06, hi = f * 1.06;
            specB0[(size_t) q] = std::max (1, (int) (lo * N / sr)); specB1[(size_t) q] = std::max (specB0[(size_t) q], (int) (hi * N / sr)); } }
    specBuf.resize ((size_t) (2 * N));
    const float norm = 20 * std::log10 (N / 4.0f);
    auto spec = [&] (const std::vector<float>& x, std::vector<float>& out)
    {
        std::fill (specBuf.begin() + N, specBuf.end(), 0.0f);
        for (int i = 0; i < N; ++i) specBuf[(size_t) i] = x[(size_t) i] * win[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (specBuf.data());
        if ((int) out.size() != P + 1) out.assign ((size_t) P + 1, -120.0f);
        for (int q = 0; q <= P; ++q)
        {
            const int b0 = specB0[(size_t) q], b1 = specB1[(size_t) q];
            double e = 0; for (int b = b0; b <= b1 && b < N / 2; ++b) e += (double) specBuf[(size_t) b] * specBuf[(size_t) b];
            const float db = (float) (10 * std::log10 (e / (b1 - b0 + 1) + 1e-14)) - norm;      // a full-scale sine ~ 0 dB
            out[(size_t) q] = db > out[(size_t) q] ? db : out[(size_t) q] - 3.0f;                // fast rise, slow fall
        }
    };
    proc.readFifo2 (fI, fR, N);
    float pk = 0; for (int i = N - 2048; i < N; ++i) pk = std::max ({ pk, std::abs (fI[(size_t) i]), std::abs (fR[(size_t) i]) });
    if (pk < 1e-6f) { if (++specQuiet > 40) return; } else specQuiet = 0;       // ~2.7 s of silence: everything has fallen to the floor
    if (fgraph.isShowing())
    {
        proc.readFifo (fD, fE, N);
        spec (fD, specD); spec (fE, specE);
        fgraph.specD = &specD; fgraph.specE = &specE;
    }
    // the spectrum monitor (always on screen): input before the input filter, and the wet output
    spec (fI, specI); spec (fR, specR);
    for (auto* gr : { static_cast<NodeGraph*> (&rtGraph), static_cast<NodeGraph*> (&eqGraph), static_cast<NodeGraph*> (&inGraph), static_cast<NodeGraph*> (&erfGraph), static_cast<NodeGraph*> (&tfGraph), static_cast<NodeGraph*> (&combo) })
    { gr->specIn = &specI; gr->specWet = &specR; if (gr->isShowing()) gr->repaint(); }
}

void MainView::fillPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& P = presets();
    // grouped (the list keeps its numbers; the menu shows them by kind): starting points / init first
    // starting points flat; the reverbs (ER + TAIL) filed three ways: by type, by era / model (IR reproductions), by use
    // (the ER-only presets: the ER tab's long-press menu)
    presetBox.addSectionHeading (ph.tr ("出発点（初期設定）", "Starting points (init)"));
    for (int i = 0; i < (int) P.size(); ++i) if (P[(size_t) i].group == 2) presetBox.addItem (juce::String::fromUTF8 (P[(size_t) i].name), i + 1);
    presetBox.addSectionHeading (ph.tr ("リバーブ（ER＋TAIL）", "Reverb (ER + TAIL)"));
    auto* root = presetBox.getRootMenu();
    auto byKey = [&] (const char* PresetCat::* field, const std::vector<std::pair<const char*, const char*>>& keys, bool othersToo)
    {
        juce::PopupMenu m; m.setLookAndFeel (&lnf);
        for (auto& [k, label] : keys)
        {
            juce::PopupMenu sub; int n = 0;
            for (int i = 0; i < (int) P.size(); ++i)
                if (P[(size_t) i].group == 3)
                    if (auto* c = presetCat (P[(size_t) i].name); c != nullptr && c->*field != nullptr && std::string (c->*field) == k)
                    { sub.addItem (i + 1, juce::String::fromUTF8 (P[(size_t) i].name), true, proc.currentPreset == i); ++n; }
            if (n > 0) m.addSubMenu (juce::String::fromUTF8 (label), sub);
        }
        if (othersToo)
        {
            juce::PopupMenu sub; int n = 0;
            for (int i = 0; i < (int) P.size(); ++i) if (P[(size_t) i].group == 3 && presetCat (P[(size_t) i].name) == nullptr) { sub.addItem (i + 1, juce::String::fromUTF8 (P[(size_t) i].name)); ++n; }
            if (n > 0) m.addSubMenu (ph.tr ("その他", "Others"), sub);
        }
        return m;
    };
    const bool J = ph.ja();
    root->addSubMenu (ph.tr ("タイプ別", "By type"), byKey (&PresetCat::type, {
        { "Room", J ? "ルーム" : "Room" }, { "Hall", J ? "ホール" : "Hall" }, { "Chamber", J ? "チェンバー" : "Chamber" }, { "Plate", J ? "プレート" : "Plate" },
        { "Spring", J ? "スプリング" : "Spring" }, { "Ambient", J ? "アンビエント" : "Ambient" }, { "Non-linear / FX", J ? "ノンリニア・効果" : "Non-linear / FX" } }, true));
    root->addSubMenu (ph.tr ("時代・機種の再現（IR から合わせたものなど）", "By era / model (fitted to IRs, and more)"), byKey (&PresetCat::era, {
        { "1970s", J ? "1970 年代" : "1970s" }, { "1980s", J ? "1980 年代" : "1980s" }, { "2000s", J ? "2000 年代" : "2000s" }, { "2010s", J ? "2010 年代" : "2010s" } }, false));
    root->addSubMenu (ph.tr ("用途別", "By use"), byKey (&PresetCat::use, {
        { "Vocal", J ? "ボーカル" : "Vocal" }, { "Drums", J ? "ドラム" : "Drums" }, { "Guitar", J ? "ギター" : "Guitar" }, { "Orchestra / keys", J ? "ピアノ・オーケストラ" : "Piano / orchestra" },
        { "Synth / pad", J ? "シンセ・パッド" : "Synth / pad" }, { "Mix bus", J ? "ミックス全体" : "Mix bus" }, { "FX", J ? "効果" : "FX" } }, false));
    presetBox.addSectionHeading (ph.tr ("ユーザープリセット", "User presets"));
    const auto U = proc.userPresets();
    for (int i = 0; i < U.size(); ++i) presetBox.addItem (U[i].getFileNameWithoutExtension(), 5000 + i);
    presetBox.addSeparator();
    presetBox.addItem (ph.tr ("いまの設定をユーザープリセットに保存…", "Save the current settings as a user preset..."), 4000);
    presetBox.addItem (ph.tr ("ユーザープリセットのフォルダーを開く（削除・名前変更）", "Open the user preset folder (delete / rename)"), 4001);
    presetBox.addItem (ph.tr ("内蔵プリセットをファイルで開く（フォルダーに書き出す）", "Open the built-in presets as files (written to a folder)"), 4002);
    syncPresetBox();
}
void MainView::syncPresetBox()
{
    if (proc.currentPreset >= 0) presetBox.setSelectedId (proc.currentPreset + 1, juce::dontSendNotification);
    else if (proc.currentUserPreset.isNotEmpty()) presetBox.setText (proc.currentUserPreset, juce::dontSendNotification);
    else presetBox.setSelectedId (0, juce::dontSendNotification);
}
void MainView::showPresetMenuExtras (int id)
{
    syncPresetBox();
    if (id == 4001) { auto d = ERProcessor::userPresetDir(); d.createDirectory(); d.startAsProcess(); return; }
    if (id == 4002)
    {
        const auto d = ERProcessor::writeFactoryPresets(); d.startAsProcess();
        flash (ph.tr ("内蔵プリセットのフォルダー", "Built-in presets folder"),
               ph.tr ("内蔵プリセットを種類ごとのフォルダーに .rvpreset で書き出しました（中身を見たり、コピーして自分用に作り変えたりできます）。内蔵プリセット自体はプラグインに入っているので、ここを消しても消えません。ユーザープリセットのフォルダーへコピーすると、メニューのユーザー欄に出ます。\n",
                      "The built-in presets were written as .rvpreset files, one folder per kind (to read, or copy and change). The built-in ones live in the plug-in, so deleting these does not remove them. Copy a file into the user preset folder to see it in the menu's user part.\n") + d.getFullPathName());
        return;
    }
    auto* w = new juce::AlertWindow (ph.tr ("ユーザープリセットを保存", "Save user preset"),
                                     ph.tr ("名前を付けて保存します（同じ名前は上書き）。IR がソースのときは IR ファイルの場所も記録します。",
                                            "Saved under this name (the same name is overwritten). With an IR source the IR file's location is kept too."),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentUserPreset.isNotEmpty() ? proc.currentUserPreset : ph.tr ("マイ ER", "My ER"));
    w->addButton (ph.tr ("保存", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ph.tr ("キャンセル", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        const auto f = safe->proc.saveUserPreset (w->getTextEditorContents ("name"));
        safe->fillPresetBox();
        safe->flash (safe->ph.tr ("ユーザープリセット", "User preset"),
                     f.existsAsFile() ? safe->ph.tr ("保存しました：", "Saved: ") + f.getFileNameWithoutExtension() : safe->ph.tr ("保存できませんでした。", "Could not save."));
    }), true);
}

void MainView::startEar()
{
    if (! infoOn()) { proc.uiState.setProperty ("info", true, nullptr); if (onDesignChange) onDesignChange(); }
    proc.uiState.setProperty ("infoSel", 2, nullptr);
    ear.start();
    layoutInfo(); earMask.setVisible (true); earMask.toFront (false); ear.toFront (false);
}
void MainView::endEar()
{
    earMask.setVisible (false);
    if (proc.earActive()) proc.earEnd();
    if (infoSel() == 2) proc.uiState.setProperty ("infoSel", 1, nullptr);
    layoutInfo();
}

void MainView::flash (const juce::String& title, const juce::String& body)
{
    msgT = title; msgB = body; msgUntil = juce::Time::getMillisecondCounterHiRes() + 9000;
    help.title = title; help.body = body; help.repaint(); layoutInfo();
}

void MainView::updateHelp()
{
    if (juce::Time::getMillisecondCounterHiRes() < msgUntil) return;
    auto pos = getMouseXYRelative();
    juce::String t, b;
    // terms: a group's heading, a tab, or one item of a segmented switch
    auto term = [&] (const juce::String& key) -> bool
    {
        if (auto* tm = findTerm (key)) { t = juce::String::fromUTF8 (tm->title); b = juce::String::fromUTF8 (ph.ja() ? tm->ja : tm->en); return true; }
        return false;
    };
    bool gotTerm = false;
    if (viewMode() == 0)
    {
        if (tabs.isShowing() && tabs.getBounds().contains (pos))
        {   const int id = tabs.idOf (tabs.tabAt ((float) (pos.x - tabs.getX()))); gotTerm = term (id == 0 ? "ER" : id == 1 ? "TAIL" : id == 3 ? "INPUT" : id == 4 ? "OUTPUT" : id == 5 ? "DYN" : "MOD"); }
        if (! gotTerm && deck.isShowing() && deck.getBounds().contains (pos)) { const auto gname = deck.groupAt (pos - deck.getPosition()); if (gname.isNotEmpty()) gotTerm = term (gname); }
        const int pg = page();
        auto head = [&] (juce::Rectangle<int> g, const char* key) { if (! gotTerm && ! g.isEmpty() && g.withHeight (22).contains (pos)) gotTerm = term (key); };
        if (shows (3)) head (gIn, "INPUT");
        if (shows (4)) head (gOut, "OUTPUT");
        head (colorBox, "COLOR");
        if (shows (0)) { head (gSrc, "SOURCE"); head (gVoice, "VOICE"); }
        if (shows (1))
        {
            head (gTail, "SOURCE"); head (gRT, "VOICE");
            const int m = TailSourceSeg::modeOf (ph), e = (int) ph.value ("tEng");
            const char* ek[] = { "HALL", "PLATE", "SPRING", "AMBIENT" };
            (void) m; (void) e; (void) ek;
        }
        if (shows (5)) { head (gDuck, "DUCKING"); head (gGate, "GATE"); head (gDynV, "METER"); }
        if (shows (6)) { head (gLfo, "LFO"); head (gMat, "MATRIX"); head (gScope, "SOURCES"); }
    }
    if (! gotTerm && viewMode() == 1 && deck.isShowing() && deck.getBounds().contains (pos)) { const auto gname = deck.groupAt (pos - deck.getPosition()); if (gname.isNotEmpty()) gotTerm = term (gname); }
    if (! gotTerm && colorBox.withHeight (24).contains (pos)) gotTerm = term ("COLOR");
    if (! gotTerm && getLocalBounds().contains (pos))
        for (auto* c = getComponentAt (pos); c != nullptr && c != this; c = c->getParentComponent())
            if (c->getProperties().contains ("helpT"))
            {
                t = c->getProperties()["helpT"].toString();
                b = c->getProperties()[ph.ja() ? "helpJ" : "helpE"].toString();
                if (c->getProperties().contains ("pid"))
                {
                    keyPid = c->getProperties()["pid"].toString();
                    b << ph.tr ("\n↑↓：値を 1 段ずつ（Shift で細かく）／Option＋クリック：初期値", "\nUp / Down: step the value (Shift: finer) / Option+click: default");
                }
                // a segmented switch: the item under the mouse, explained
                if (c->getProperties().contains ("terms"))
                {
                    juce::StringArray keys; keys.addTokens (c->getProperties()["terms"].toString(), ",", "");
                    const int i = juce::jlimit (0, keys.size() - 1, (int) ((pos.x - c->getScreenX() + getScreenX()) * keys.size() / std::max (1, c->getWidth())));
                    if (auto* tm = findTerm (keys[i])) b << "\n\n" << juce::String::fromUTF8 ("\xe2\x96\xb8 ") << juce::String::fromUTF8 (tm->title) << juce::String::fromUTF8 ("\xef\xbc\x9a") << juce::String::fromUTF8 (ph.ja() ? tm->ja : tm->en);
                }
                // a deck / remote control: the EDIT parameters it actually moves, when the name differs or it moves several
                if (c->getProperties().contains ("hids"))
                {
                    juce::StringArray ids; ids.addTokens (c->getProperties()["hids"].toString(), ",", "");
                    const auto lab = c->getProperties()["hlabel"].toString();
                    juce::String lines; int n = 0;
                    for (auto& id : ids)
                        if (auto* p = ph.get (id))
                        {
                            const auto nm = p->getName (40);
                            if (ids.size() == 1 && nm.equalsIgnoreCase (lab)) continue;
                            lines << "\n・" << nm << " = " << p->getCurrentValueAsText() << (p->getLabel().isNotEmpty() && ! p->getCurrentValueAsText().contains (p->getLabel()) ? " " + p->getLabel() : juce::String()); ++n;
                        }
                    if (n > 0) b << ph.tr ("\n\nEDIT で動くパラメーター：", "\n\nEDIT parameters it moves:") << lines;
                }
                break;
            }
    if (t.isEmpty() && proc.currentPreset >= 0)
    {
        const auto& P = presets()[(size_t) proc.currentPreset];
        t = juce::String::fromUTF8 (P.name); b = juce::String::fromUTF8 (ph.ja() ? P.ja : P.en);
    }
    if (t != help.title || b != help.body) { help.title = t; help.body = b; help.repaint(); layoutInfo(); }
}

bool MainView::showPanel() const
{
    return (int) proc.value ("src") == 1 && proc.unitIndex() >= 0 && (bool) proc.uiState.getProperty ("panelOn", true);
}

// ---------------------------------------------------------------------------
//  Words for changes. Each parameter: what going up / down does to the sound.
// ---------------------------------------------------------------------------
namespace
{
struct Phrase { const char* id; const char* name; const char* upJ; const char* dnJ; const char* upE; const char* dnE; };
const Phrase kPhr[] = {
    { "dkDepth", "Duck Depth", "ドライが鳴る間、残響がより大きく引っ込み、声や主旋律が前に出る", "残響がドライに重なったまま鳴り、ひとまとまりの響きに", "the reverb steps further back under the dry: the vocal comes forward", "the reverb stays under the dry: one blended sound" },
    { "dkRel", "Duck Release", "残響がゆっくり戻り、隙間がドライだけで空く", "残響がすぐ戻り、フレーズの隙間がすぐ埋まる", "the reverb returns slowly: the gaps stay dry", "the reverb returns at once: the gaps fill quickly" },
    { "gtHold", "Gate Hold", "ゲートが開いている時間が長くなり、切れる前の残響が長い", "残響が短く切れ、パンチのあるゲートリバーブに", "the gate stays open longer: more reverb before the cut", "a shorter, punchier gated reverb" },
    { "chMix", "Chorus", "残響が揺れて広がり、80 年代のデジタルリバーブの質感に", "残響の揺れが減り、静かで素直な響きに", "the reverb shimmers and widens: the '80s digital sound", "less motion: a calmer, plainer reverb" },
    { "chDepth", "Chorus Depth", "残響のピッチの揺れが大きくなる", "揺れが小さくなり、自然に", "more pitch movement in the reverb", "less movement: more natural" },
    { "first", "First", "壁が遠くなり、部屋が広く奥行きが深く感じる", "壁が近くなり、狭く密着した響きに（1 ms 前後では音色の変化＝コムになる）", "the walls move away: a bigger, deeper room", "the walls come closer: a small, tight room (around 1 ms it turns into comb colour)" },
    { "length", "Length", "反射が長い時間に散らばり、空間が大きく", "ER が短くまとまり、音が太くなるだけで広さは減る", "reflections spread over more time: a bigger space", "a short, compact ER: thicker, less space" },
    { "taps", "Reflections", "反射が増えて粒が目立たなくなる", "一つずつの反射が聞こえ、金属的・バウンス感に", "more reflections: smoother", "fewer reflections: audible, metallic bounces" },
    { "growth", "Growth", "反射が後半に集まり、自然な部屋のふくらみに", "反射が均等に並び、ディレイを並べたような人工的な響きに", "reflections crowd towards the end: a natural room build", "evenly spaced reflections: an artificial, delay-like field" },
    { "energy", "Energy", "響きが増え、音源が遠く感じる", "響きが減り、近くドライに", "more ER: the source sounds farther", "less ER: closer and drier" },
    { "decay", "Envelope", "反射が後ろまで強く残る（平ら・ビルドアップ寄り）", "反射がすっと消え、柔らかい部屋に", "reflections stay strong to the end (flat / build-up)", "reflections fade quickly: a softer room" },
    { "damp", "Damping", "後半の反射が明るく、硬い部屋に", "後半の反射が暗く、柔らかい材質・距離感に", "brighter late reflections: a hard room", "darker late reflections: soft materials, distance" },
    { "predelay", "Predelay", "直接音と ER の間が空き、輪郭が前に出る", "ER が直接音に密着する", "a gap after the direct sound: the attack stays clear", "the ER sits right on the direct sound" },
    { "size", "Size", "すべての反射が遅れ、部屋が大きく", "すべての反射が早まり、部屋が小さく", "every reflection later: a bigger room", "every reflection sooner: a smaller room" },
    { "width", "Width", "左右に広がり包まれる（モノでの打ち消しに注意）", "中央にまとまる", "wider and more enveloping (watch mono)", "narrower, towards the centre" },
    { "diffusion", "Diffusion", "反射がにじみ、密な響き（プレート的）に", "反射の粒がはっきりする", "reflections smear into a dense field (plate-like)", "reflections become distinct" },
    { "lowcut", "Low Cut", "ER の低域が減り、濁りが取れる", "ER の低域が残り、太く重い", "less low end in the ER: less mud", "more low end in the ER: thicker, heavier" },
    { "highcut", "High Cut", "ER が明るく近い", "ER が暗く遠い", "brighter, closer ER", "darker, farther ER" },
    { "lcSlope", "LC Slope", "ローカットの切れが急に", "ローカットがゆるやかに", "steeper low cut", "gentler low cut" },
    { "hcSlope", "HC Slope", "ハイカットの切れが急に（帯域制限らしい切れ方）", "ハイカットがゆるやかに（空気吸収のような落ち方）", "steeper high cut (like a band limit)", "gentler high cut (like air absorption)" },
    { "level", "ER Level", "ER 全体が大きく", "ER 全体が小さく", "the whole ER louder", "the whole ER quieter" },
    { "bw", "Bandwidth", "帯域が広がり、現代的に", "上の帯域が切れ、昔のデジタル機の暗さに", "wider band: more modern", "the top is cut: the darkness of old digital units" },
    { "bits", "Bits", "量子化のざらつきが減る", "量子化のざらつきが増える", "less quantisation grain", "more quantisation grain" },
    { "wander", "Wander", "反射が揺れて、にじみ・コーラス感が出る", "反射が静止してクリアに", "moving reflections: smear and chorus", "still reflections: clearer" },
    { "roomW", "Width (room)", "側壁が遠く、左右の反射が遅れる", "側壁が近く、左右の反射が早まる", "side walls farther: later side reflections", "side walls closer: earlier side reflections" },
    { "roomD", "Depth (room)", "前後の壁が遠く、部屋が大きく", "前後の壁が近く", "front / back walls farther: a bigger room", "front / back walls closer" },
    { "roomH", "Height", "天井が高く、床と天井の反射が遅れる", "天井が低く、反射が早く箱鳴りしやすい", "higher ceiling: later floor / ceiling bounces", "lower ceiling: early bounces, boxier" },
    { "order", "Order", "反射の回数が増え、ER が密に", "反射が減り、まばらに", "more bounces: a denser ER", "fewer bounces: a sparser ER" },
    { "darken", "Darkening", "後ろの反射ほど暗くなり、実際の部屋らしい奥行きと柔らかさに", "後ろの反射まで明るさが変わらず、硬く人工的に", "later reflections grow darker: the depth and softness of a real room", "the late reflections stay as bright: hard and artificial" },
    { "irFill", "Fill", "後半の隙間が埋まり、なめらか・密に", "後半が拾った反射だけになり、Taps に近い粒立ちに", "the gaps later on fill in: smoother, denser", "only the picked reflections later on: grainier, close to Taps" },
    { "irSplit", "Split", "補いを足し始める時刻が遅くなり、前半の粒立ちが長く続く", "早くから補いが入り、なめらかに", "the fill starts later: the early grain lasts longer", "the fill starts sooner: smoother" },
    { "tDecay", "Decay", "響きが長くなり、空間が大きく・遠く", "響きが短くなり、空間が小さく・近く", "a longer reverb: a bigger, farther space", "a shorter reverb: a smaller, closer space" },
    { "tSize", "Size", "空間が大きくなり、響きの粒が粗く", "空間が小さくなり、響きが密に", "a bigger space with a coarser grain", "a smaller space, a denser reverb" },
    { "tPre", "Tail Delay", "ER と残響の間が空き、空間が大きく感じる", "残響がすぐに続き、空間が小さく感じる", "a longer gap between ER and reverb: a bigger space", "the reverb follows at once: a smaller space" },
    { "tDiff", "Diffusion", "立ち上がりがなめらかに", "粒が聞こえる硬い響きに", "a smoother onset", "a grainy, harder sound" },
    { "tModD", "Mod Depth", "金属的な鳴りがぼけ、広がる（上げすぎると音程が揺れる）", "揺れが減り、素直だが金属的な鳴りが出やすい", "metallic ringing blurs, the sound widens (too much and pitch wavers)", "less movement: plain, but metallic ringing shows more" },
    { "erBal", "ER / TAIL", "ER が下がりテールが前に：遠く", "テールが下がり ER が前に：近く", "less ER, tail forward: farther", "less tail, ER forward: closer" },
    { "tLvl", "Tail Level", "残響が増え、遠くに", "残響が減り、近くに", "more reverb: farther away", "less reverb: closer" },
    { "outW", "Width", "リバーブが広がる", "リバーブが中央に寄る", "the reverb spreads wider", "the reverb narrows to the centre" },
    { "inW", "Input Width", "入力が広がり、テールの左右差が増える", "入力が中央に寄り、テールの左右差が減る", "a wider input: more left / right difference in the tail", "a narrower input: less left / right difference" },
    { "iLC", "Input Low Cut", "リバーブの低域が減り、濁りが取れる", "リバーブの低域が増え、厚く（濁りやすく）", "less low end in the reverb: clearer", "more low end in the reverb: fuller (muddier)" },
    { "iHC", "Input High Cut", "リバーブの高域が増え、明るく", "リバーブの高域が減り、暗く落ち着く", "more highs in the reverb: brighter", "fewer highs in the reverb: darker, calmer" },
    { "mix", "Mix", "ER が大きく（100% で ER だけ）", "直接音が主に", "more ER (100% = ER only)", "more direct sound" },
    { "tailRT", "Tail Decay", "テールが長く", "テールが短く", "longer tail", "shorter tail" },
};
const Phrase* phraseFor (const juce::String& id) { for (auto& p : kPhr) if (id == p.id) return &p; return nullptr; }
}

void MainView::describeChange()
{
    const int n = (int) (sizeof (pid::ids) / sizeof (pid::ids[0]));
    if ((int) settledSnap.size() < n || (int) snap.size() < n) return;
    juce::StringArray parts;
    // the faceplate control that moved, if any
    const auto pnl = proc.uiState.getProperty ("pnl", "").toString();
    juce::String head;
    if (pnl != settledPnl && proc.unitIndex() >= 0)
    {
        juce::StringArray a, b; a.addTokens (settledPnl, ",", ""); b.addTokens (pnl, ",", "");
        const auto& U = units()[(size_t) proc.unitIndex()];
        for (int i = 0; i < std::min ({ a.size(), b.size(), (int) U.ctl.size() }); ++i)
            if (std::abs (a[i].getFloatValue() - b[i].getFloatValue()) > 1e-4f)
            { head = ph.tr ("パネル ", "Panel ") + U.ctl[(size_t) i].label + " " + juce::String (a[i].getFloatValue(), 1) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + juce::String (b[i].getFloatValue(), 1) + ph.tr ("：", ": "); break; }
    }
    struct C { juce::String id; float a, b; float rel; };
    std::vector<C> cs;
    for (int i = 0; i < n; ++i)
    {
        const float a = settledSnap[(size_t) i], b = snap[(size_t) i];
        if (std::abs (a - b) < 1e-5f) continue;
        auto* p = proc.apvts.getParameter (pid::ids[i]); if (p == nullptr) continue;
        cs.push_back ({ pid::ids[i], a, b, std::abs (p->convertTo0to1 (b) - p->convertTo0to1 (a)) });
    }
    if (cs.empty()) return;
    std::sort (cs.begin(), cs.end(), [] (const C& x, const C& y) { return x.rel > y.rel; });
    if (cs.size() > 6 && head.isEmpty()) { now.change = ph.tr ("設定をまとめて切り替えました（プリセット・レッスン）。", "Settings switched together (preset / lesson)."); return; }
    for (size_t k = 0; k < std::min<size_t> (2, cs.size()); ++k)
    {
        const auto& c = cs[k]; auto* p = proc.apvts.getParameter (c.id);
        auto txt = [&] (float v) -> juce::String
        {
            if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) return p->getText (p->convertTo0to1 (v), 24);
            const float a = std::abs (v);
            return minus (a >= 100 ? juce::String ((int) std::round (v)) : a >= 10 ? juce::String (v, 1) : juce::String (v, 2)) + " " + p->getLabel();
        };
        juce::String line = (phraseFor (c.id) ? juce::String (phraseFor (c.id)->name) : p->getName (24)) + " " + txt (c.a).trim() + juce::String::fromUTF8 (" \xe2\x86\x92 ") + txt (c.b).trim();
        if (auto* ph2 = phraseFor (c.id)) line << juce::String::fromUTF8 (" \xe2\x80\x94 ") << juce::String::fromUTF8 (ph.ja() ? (c.b > c.a ? ph2->upJ : ph2->dnJ) : (c.b > c.a ? ph2->upE : ph2->dnE));
        else if (c.id == "wall" || c.id == "floor") line << ph.tr ("（材質で反射の大きさと高域が変わる）", " (the material changes reflection level and treble)");
        else if (c.id.startsWith ("src") || c.id.startsWith ("lis")) line << ph.tr ("（位置で初回反射の時間と方向が変わる）", " (position changes the first reflection's time and direction)");
        parts.add (line);
    }
    now.change = head + parts.joinIntoString (ph.ja() ? "。" : "; ");
}

void MainView::describeNow()
{
    juce::StringArray n;
    const auto& F = prev.self;
    double t1 = 1e9; for (int i = 0; i < prev.taps.n; ++i) t1 = std::min (t1, 0.5 * (double) (prev.taps.t[(size_t) i].tL + prev.taps.t[(size_t) i].tR));
    if (prev.conv) t1 = F.T1;
    if (t1 < 1e8)
    {
        juce::String k = t1 < 1.5 ? ph.tr ("反射が直接音に重なり、響きより音色（コム）として聞こえる", "reflections overlap the direct sound: heard as colour (comb), not space")
                       : t1 < 4 ? ph.tr ("狭い空間・壁のすぐそば", "a small space, right by a wall")
                       : t1 < 10 ? ph.tr ("小〜中くらいの部屋", "a small to medium room")
                       : t1 < 25 ? ph.tr ("広い部屋・ホールの前方", "a large room, the front of a hall")
                       : t1 < 50 ? ph.tr ("大きなホール・遠い壁", "a big hall, distant walls")
                                 : ph.tr ("エコーとして分離して聞こえ始める", "starts to separate as an echo");
        n.add (ph.tr ("初回反射 ", "First reflection ") + fmtMs (t1) + ph.tr ("：", ": ") + k);
    }
    const double er = F.erDb;
    n.add ("ER " + fmtDb (er) + ph.tr ("（直接音比）：", " re direct: ")
           + (er < -18 ? ph.tr ("ほぼドライ。近い", "almost dry, close")
            : er < -8 ? ph.tr ("直接音が主役で、部屋の気配が足される", "the direct sound leads, the room adds air")
            : er < -2 ? ph.tr ("部屋の中で鳴っている感じがはっきり", "clearly 'in the room'")
                      : ph.tr ("響きが直接音と同等以上。遠く、ぼやけやすい", "as loud as the direct sound or more: far and easily blurred")));
    const double dens = (double) F.taps.size() / std::max (5.0, prev.selfWc) * 10.0, dif = ph.value ("diffusion");
    n.add (ph.tr ("密度 ", "Density ") + juce::String (dens, 1) + ph.tr (" 本/10 ms・拡散 ", " per 10 ms, diffusion ") + juce::String ((int) dif) + "%" + ph.tr ("：", ": ")
           + (dens < 1.5 && dif < 30 ? ph.tr ("反射の粒が一つずつ聞こえる（バウンス・金属感）", "you can hear single reflections (bounce, metallic)")
            : dens < 4 && dif < 50 ? ph.tr ("粒が少し残る", "some grain left") : ph.tr ("なめらかな響き", "a smooth field")));
    const double sl = F.slope;   // dB per 10 ms
    const bool crowd = sl > 0.4 && ph.value ("decay") < -3 && (int) ph.value ("src") == 1;
    n.add (ph.tr ("形：", "Shape: ") + (crowd ? ph.tr ("反射が後ろほど増えるので、全体のエネルギーは後ろへふくらむ（1本ずつは減衰）", "reflections crowd towards the end, so the energy swells (each one decays)")
                                     : sl > 0.4 ? ph.tr ("後ろへふくらむ（リバース・ビルド型）", "swells towards the end (reverse / build)")
                                     : sl > -0.4 ? ph.tr ("平ら（ノンリニア・ゲート型）", "flat (non-linear / gated)")
                                                 : ph.tr ("自然に減衰（", "decays naturally (") + juce::String (std::abs (sl), 1) + ph.tr (" dB / 10 ms）", " dB per 10 ms)")));
    {
        const double late = F.band[7] - F.band[3];
        juce::String t = late < -6 ? ph.tr ("後ろほど高域が落ちる（柔らかい材質・距離）", "treble falls away towards the end (soft materials, distance)")
                       : late < -2 ? ph.tr ("高域はやや落ちる", "treble drops a little") : ph.tr ("後ろまで明るい（硬い面）", "bright to the end (hard surfaces)");
        if (ph.value ("bw") < 19.9f) t << ph.tr ("。帯域 ", ". Band limit ") << juce::String (ph.value ("bw"), 1) << ph.tr (" kHz で上が切れる", " kHz");
        if (ph.value ("bits") < 23.5f) t << ph.tr ("。", ". ") << juce::String ((int) ph.value ("bits")) << ph.tr (" bit のざらつき", "-bit grain");
        n.add (ph.tr ("音色：", "Tone: ") + t);
    }
    n.add (ph.tr ("広がり：", "Width: ") + (F.iacc < 0.3 ? ph.tr ("左右に大きく広がる（モノで確認を）", "very wide (check in mono)")
                                          : F.iacc < 0.6 ? ph.tr ("自然な広がり", "natural width") : ph.tr ("中央寄り", "centred"))
           + " (IACC " + juce::String (F.iacc, 2) + ")");
    if (ph.value ("tail") > 0.5f) n.add (ph.tr ("テール（RT ", "Tail (RT ") + juce::String (ph.value ("tailRT"), 1) + ph.tr (" s）が ER に続く", " s) follows the ER"));
    if (prev.hasFeat && echo.tgt)
    {
        auto hs = diagnose (echo.tgt->F, prev.feat, ph.ja());
        n.add (ph.tr ("ターゲットとの一致度 ", "Match to target ") + juce::String (prev.d.score) + (hs.empty() ? juce::String() : ph.tr ("。いちばんのずれ：", ". Biggest gap: ") + hs.front().text));
    }
    now.notes = n; now.repaint();
}

void MainView::updateRings()
{
    juce::StringArray ids; juce::Colour c = juce::Colours::transparentBlack;
    if (showPanel())
    {
        for (auto& kv : proc.panelOutputs()) ids.add (kv.first);
        c = juce::Colour (units()[(size_t) proc.unitIndex()].accent);
        if (c.getPerceivedBrightness() > 0.75f) c = theme::accent;
    }
    for (auto& [k, v] : vals) { const auto want = ids.contains (k) ? c : juce::Colours::transparentBlack; if (want != v->ring) { v->ring = want; v->repaint(); } }
}

void MainView::timerCallback()
{
    repaint (646, 8, 62, 32);                     // (0.10.5) the IN / OUT meter follows the audio, not just parameter changes
    if (++saveTick >= 30) { saveTick = 0; saveLastView(); }
    // (the tabs' chips: see the block below)
    if ((TailSourceSeg::modeOf (ph) * 2 + (int) proc.uiState.getProperty ("tEdit", 0)) * 8 + (int) ph.value ("tEng") != lastTailMode) { layoutTail(); if (viewMode() != 0) enforceMode(); repaint(); }
    if (viewMode() == 0) { if ((popTick & 1) == 0) { deck.repaintKnobs(); repaint (colorBox); colErBtn.repaint(); colTailBtn.repaint(); if (shows (4)) repaint (gCol); tabs.repaint(); } }       // (the deck's knobs follow presets, undo and the host)
    if (viewMode() != 0)
    {
        enforceMode();
        if ((popTick & 1) == 0) { deck.repaintKnobs(); modern.repaintKnobs(); fader.repaint(); }
    }
    // which parameters / state changed?
    std::vector<float> s;
    for (auto* id : pid::ids) s.push_back (proc.value (id));
    s.push_back ((float) proc.targetVersion.load()); s.push_back ((float) proc.currentPreset);
    s.push_back ((float) (int) proc.uiState.getProperty ("span", 400)); s.push_back (ph.ja() ? 1.0f : 0.0f);
    s.push_back ((float) proc.analysing.load()); s.push_back ((float) proc.unitIndex()); s.push_back ((float) (bool) proc.uiState.getProperty ("panelOn", true));
    s.push_back (cmpOn() ? 1.0f : 0.0f);
    const double tnow = juce::Time::getMillisecondCounterHiRes();
    const int nIds = (int) (sizeof (pid::ids) / sizeof (pid::ids[0]));
    if (s != snap)
    {
        const bool layoutChanged = snap.empty() || snap[0] != s[0] || snap[(size_t) nIds + 5] != s[(size_t) nIds + 5] || snap[(size_t) nIds + 6] != s[(size_t) nIds + 6];
        bool paramsChanged = false;
        if (! snap.empty()) for (int i = 0; i < nIds; ++i) if (snap[(size_t) i] != s[(size_t) i])
        {
            paramsChanged = true;
            if (auto it = vals.find (pid::ids[i]); it != vals.end() && ! it->second->isMouseButtonDown()) it->second->flash = 1.0f;
        }
        if (paramsChanged && tnow - lastChange > 700)          // a new change starts: keep "before"
        {
            settledSnap = snap; settledPnl = lastPnl; ghost = prev;
            echo.ghost = &ghost;
        }
        snap = s; dirty = true; settle = 0;
        if (paramsChanged) { lastChange = tnow; describeChange(); }
        for (auto& [k, v] : vals) v->repaint();
        for (auto* c : std::initializer_list<juce::Component*> { &srcSeg, &irMode, &irAuto, &irWet, &tailSeg, &roomView, &lesson, &panel,
                                                                 &rtGraph, &inGraph, &eqGraph, &erfGraph, &tfGraph, &combo, &measure, &irtView, &tSrcSeg, &irtRevSeg, &irtAutoSeg, &tEngSeg, &rtModeSeg, &freezeSeg, &erOnSeg, &tOnSeg, &outModeSeg, &iToSeg, &eqOnSeg }) c->repaint();
        inModeBox.sync(); iLCsBox.sync(); iHCsBox.sync();
        updatePopup();
        wallBox.sync(); floorBox.sync(); slopeBox.sync(); lcSlopeBox.sync();
        if (deckView() == 1) { wallA2.sync(); floorA2.sync(); wallB2.sync(); floorB2.sync(); shapeA2.sync(); shapeB2.sync(); srcVals["aS"]->setEnabled ((int) ph.value ("shape") != 0); srcVals["bS"]->setEnabled ((int) ph.value ("rbShape") != 0); srcVals["aB"]->setEnabled ((int) ph.value ("shape") == 4); srcVals["bB"]->setEnabled ((int) ph.value ("rbShape") == 4); for (auto* c : srcComps) c->repaint(); repaint (gCard[0].getUnion (gCard[3])); syncSourceButtons(); }
        syncPresetBox();
        langBtn.setButtonText (ph.ja() ? "EN" : juce::String::fromUTF8 ("\xe6\x97\xa5"));
        loadBtn.setButtonText (ph.tr ("IR読込", "Load IR")); exportBtn.setButtonText (ph.tr ("IR書出", "Export")); lookBtn.setButtonText (ph.tr ("表示", "Look"));
        panelBtn.setButtonText (showPanel() ? ph.tr ("パネルを隠す", "Hide panel") : ph.tr ("実機パネル", "Faceplate"));
        panelBtn.setVisible ((int) proc.value ("src") == 1 && proc.unitIndex() >= 0);
        for (int k = 0; k < 2; ++k)
        {   // (0.10.10) one compare button per IR card
            auto& b = k == 0 ? cmpBtn : cmpB; const bool on = cmpOn() && cmpSlot() == k;
            b.setButtonText (on ? ph.tr ("この IR と比較中", "Comparing with this IR") : ph.tr ("この IR と比較", "Compare with this IR"));
            b.setToggleState (on, juce::dontSendNotification); b.setEnabled (cardLoaded (2 + k));
        }
        cmpScope.setEnabled (cmpOn());
        irAClear.setEnabled (cardLoaded (2)); ir2Clear.setEnabled (cardLoaded (3));
        { const int nb = proc.irListB.size() > 0 ? proc.irListB.size() : juce::StringArray::fromLines (proc.uiState.getProperty ("irListB", "").toString()).size(); irBPrev.setEnabled (nb > 1); irBNext.setEnabled (nb > 1); }
        const int nList = proc.irList.size() > 0 ? proc.irList.size() : juce::StringArray::fromLines (proc.uiState.getProperty ("irList", "").toString()).size();
        irPrev.setEnabled (nList > 1); irNext.setEnabled (nList > 1);
        if (lastLang != (ph.ja() ? "ja" : "en"))
        {   // (0.10.5) the texts written once when the editor was built have to follow the language too
            lastLang = ph.ja() ? "ja" : "en"; srcPresetFor = -2; fillSourcePresets();
            const char* tn2[3][2] = { { "レッスン", "Lesson" }, { "リバーブ音", "Reverb sound" }, { "耳トレ", "Ear training" } };
            for (int i = 0; i < 3; ++i) infoTab[i].setButtonText (ph.tr (tn2[i][0], tn2[i][1]));
            copyABBtn.setButtonText (ph.tr ("A をコピー", "Copy A")); copyBABtn.setButtonText (ph.tr ("B をコピー", "Copy B"));
            ir2Load.setButtonText (ph.tr ("IR読込", "Load IR")); irAClear.setButtonText (ph.tr ("クリア", "Clear")); cmpScope.items = { "ER", "TAIL", ph.tr ("全体", "ALL") }; ir2Clear.setButtonText (ph.tr ("クリア", "Clear"));
            resized(); layoutSource();                            // (the notes button and the tail IR buttons are written there)
        }
        colorPresetBox.setSelectedId (0, juce::dontSendNotification);
        for (int i = 0; i < (int) colorPresets().size(); ++i)
        { const auto& c = colorPresets()[(size_t) i];
          if (std::abs (c.bw - ph.value ("bw")) < 0.05f && std::abs (c.bits - ph.value ("bits")) < 0.05f && std::abs (c.wander - ph.value ("wander")) < 0.5f) colorPresetBox.setSelectedId (i + 1, juce::dontSendNotification); }
        repaint (gColor);
        info.refreshButtons();
        if (layoutChanged) { resized(); layoutSource(); } else repaint (gSrc);
        updateRings();
        repaint (0, 0, getWidth(), 50);
    }
    if (tnow - lastChange > 700) lastPnl = proc.uiState.getProperty ("pnl", "").toString();
    for (auto& [k, v] : vals) if (v->flash > 0.01f) { v->flash *= 0.9f; v->repaint(); }
    if (dirty && ++settle >= 2)          // ~15 Hz while dragging
    {
        dirty = false; settle = 0;
        auto T = proc.target();
        const bool erCmp = cmpOn() && cmpScopeV() != 1 && proc.tgtSlot == cmpSlot();
        echo.tgt = erCmp ? T : nullptr; info.tgt = T; info.compare = erCmp;
        // the live preview: as long as the echogram shows (at most ~1 s while it is the whole-reverb view; the rest comes from the background render)
        const double len = std::max ({ (echo.compact ? 1060.0 : std::min (echo.span() + 60, echo.span() > 3000 ? 1060.0 : 3060.0)), T && T->valid ? T->Wc + 30 : 0.0, 120.0 });
        proc.renderPreview (prev, len);
        if (proc.previewTailStale) dirty = true;               // the tail is still the one from a moment ago: once more shortly
        fader.taps.clear();
        for (int i = 0; i < prev.taps.n; ++i)
        {
            const auto& t = prev.taps.t[(size_t) i];
            fader.taps.push_back ({ std::max (t.tL, t.tR), (float) (20 * std::log10 (std::max (std::abs (t.gL), std::abs (t.gR)) + 1e-9f)) });
        }
        {   // where the ER ends and the tail comes in (echogram marks)
            double tEnd = 0; for (int i = 0; i < prev.taps.n; ++i) tEnd = std::max (tEnd, (double) std::max (prev.taps.t[(size_t) i].tL, prev.taps.t[(size_t) i].tR));
            echo.erEndMs = ph.value ("erOn") > 0.5f ? tEnd : 0.0;
            const int ts = (int) ph.value ("tSrc");
            echo.tailStartMs = ph.value ("tOn") > 0.5f ? ph.value ("predelay") + (ts == 0 ? ph.value ("tPre") : ph.value ("irtX")) : -1.0;
        }
        if (! erCmp) prev.hasFeat = false;
        describeNow();
        echo.repaint(); info.repaint(); info.refreshButtons();
    }
    if (const int iv = proc.irtVersion.load() + proc.targetVersion.load() * 1000; iv != lastIrtV) { lastIrtV = iv; irtView.repaint(); }
    if (++popTick % 10 == 0) updatePopup();          // keeps the value window next to its point
    if (const int mv = proc.measVersion.load(); mv != lastMeasV)
    {
        lastMeasV = mv; const auto m = proc.measurement(); rtGraph.meas = m; measure.m = m;
        {   // (0.10.10) TAIL / ALL compare: the IR's T30 per band on the RT graph and in the measurement
            const bool tc = cmpOn() && cmpScopeV() != 0 && cardLoaded (2 + cmpSlot());
            rtGraph.irM = tc ? irMeasOf (cmpSlot()) : rv::Measure(); measure.irM = rtGraph.irM;
        }
        proc.measuredIR (echo.longL, echo.longR); measure.setIR (echo.longL, echo.longR, er::kSR);
        {   // the fader remote's screen: the whole reverb's peak envelope (dB re direct)
            const auto& L = echo.longL; const auto& R = echo.longR; const int P = 200;
            fader.envDb.assign ((size_t) P, -90.0f); fader.envMs = 1000.0 * (double) L.size() / er::kSR; fader.meas = m;
            for (int k = 0; k < P && ! L.empty(); ++k)
            {
                const size_t a = (size_t) ((double) k / P * L.size()), bnd = (size_t) ((double) (k + 1) / P * L.size()); float pk = 0;
                for (size_t i = a; i < bnd && i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
                fader.envDb[(size_t) k] = (float) (20 * std::log10 (pk + 1e-9));
            }
        }
        combo.repaint(); measure.repaint(); echo.repaint();
    }
    else if (proc.measuring.load()) combo.repaint();
    {   // the tabs' mode chips follow the parameters (also when they change from a preset, the deck or automation)
        juce::String st; for (int pg : { 0, 1, 5, 6 }) st << tabs.status (pg) << "|";
        if (st != lastTabStatus) { lastTabStatus = st; tabs.repaint(); }
    }
    if (viewMode() == 0 && ! proc.earActive())
    {   // DYN / MOD: the live meter and scope
        if (shows (5)) dynMeter.repaint();
        if (shows (6)) { modScope.push(); repaint (gMat); }
    }
    if (batchWatch)
    {   // the batch: progress on the Load IR button, the report when it ends
        const int pc = (int) std::round (proc.batchFitProgress.load() * 100);
        if (proc.batchRunning.load()) loadBtn.setButtonText (batchApply ? juce::String (pc) + "%" : juce::String (proc.batchDone.load() + 1) + "/" + juce::String (proc.batchTotal.load()) + " " + juce::String (pc) + "%");
        else if (batchApply)
        {
            batchWatch = false; loadBtn.setButtonText (ph.tr ("IR読込", "Load IR"));
            flash (ph.tr ("IR を高精度に再現：完了", "Precise rebuild: done"),
                   ph.tr ("いまの設定が IR の再現です（ER＝ALGO、TAIL＝ALGO）。数値は誤差（はじめ → 合わせた後）：\n", "The settings now rebuild the IR (ER = ALGO, TAIL = ALGO). Errors (before -> after):\n") + proc.lastFitReport);
        }
        else
        {
            batchWatch = false; fillPresetBox(); loadBtn.setButtonText (ph.tr ("IR読込", "Load IR"));
            flash (ph.tr ("プリセットの一括作成：完了", "Batch presets: done"),
                   juce::String (proc.batchOk.load()) + " / " + juce::String (proc.batchTotal.load()) + ph.tr (" 個を保存（プリセットメニューのユーザー欄）\n", " saved (the user part of the preset menu)\n") + proc.batchLog);
        }
    }
    updateHelp();
    for (auto& [k, v] : vals) { const bool sel = k == keyPid; if (v->selected != sel) { v->selected = sel; v->repaint(); } }
    updateSpectra();
    repaint (824, 8, 74, 34);             // meters
    if (const int d = proc.r2pDone.load(); d != lastR2p)
    {
        lastR2p = d;
        const auto sc = juce::String (proc.r2pScore.load());
        const int k = proc.r2pKind.load();
        if (k == 2)
            flash ("IR -> ROOM", ph.tr ("IR に近い部屋：", "Room closest to the IR: ") + juce::String (ph.value ("roomW"), 1) + " x " + juce::String (ph.value ("roomD"), 1) + " x " + juce::String (ph.value ("roomH"), 1)
                                     + ph.tr (" m、一致度 ", " m, match ") + sc
                                     + ph.tr ("。鏡に映した部屋は同じ反射になるので、答えはそのうちの一つです。部屋の形がない IR（プレート・実機）は低く出ます。Undo で戻ります。",
                                              ". Mirrored rooms give the same reflections, so this is one of them. IRs without a room shape (plates, hardware) score low. Undo goes back."));
        else
            flash (k == 1 ? "ROOM -> ALGO (hybrid)" : "ROOM -> ALGO",
                   (k == 1 ? ph.tr ("最初の ", "The first ") + juce::String ((int) std::round (ph.value ("roomEarly"))) + ph.tr (" ms は部屋の反射のまま、そのあとを ALGO にしました。一致度 ", " ms stay the room's reflections, ALGO after them. Match ")
                           : ph.tr ("部屋の ER に合わせた ALGO に切り替えました。一致度 ", "Switched to a ALGO fitted to the room's ER. Match ")) + sc
                   + (k == 1 ? ph.tr ("。Undo で ROOM に戻ります。", ". Undo returns to ROOM.")
                             : ph.tr ("。最初の数本の時刻は合わず、エンベロープ・密度・音色・広がりが合います。Undo で ROOM に戻ります。",
                                      ". The first few reflection times differ; the envelope, density, tone and width match. Undo returns to ROOM.")));
        info.repaint();
    }
    if (! proc.converting.load())
    {
        r2pBtn.setButtonText (juce::String::fromUTF8 ("ROOM \xe2\x86\x92 ALGO"));
        r2pHybBtn.setButtonText (juce::String::fromUTF8 ("ROOM \xe2\x86\x92 ALGO") + ph.tr ("（前半 ROOM）", " (early ROOM)"));
    }
    else if (proc.converting.load() == 3) info.repaint();
    if (proc.earActive() != earMask.isVisible()) { if (proc.earActive()) { earMask.setVisible (true); layoutInfo(); earMask.toFront (false); ear.toFront (false); } else endEar(); }
    if (const float c = proc.autoComp.load(); c != lastComp) { lastComp = c; repaint (autoSeg.getBounds().withHeight (40)); dirty = true; }
}

// The editor
