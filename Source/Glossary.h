#pragma once
// 0.7.1: terms (the names of parts, sources, engines and categories) explained in the help column.
// Shown when the mouse is over a group's heading, a tab, or one item of a segmented switch.
#include <juce_core/juce_core.h>

struct Term { const char* key; const char* title; const char* ja; const char* en; };
inline const Term* findTerm (const juce::String& key)
{
    static const Term t[] = {
        // the two parts and the additions
        { "ER", "ER (early reflections)", "初期反射。直接音のあと 0〜100 ms ほどに届く、壁・床・天井からの最初の反射の集まり。距離感、部屋の大きさと形、近さを決めます。",
          "Early reflections: the first bounces off walls, floor and ceiling, about 0-100 ms after the direct sound. They give distance, the room's size and shape, closeness." },
        { "TAIL", "TAIL (late reverb)", "後部残響。反射が数えきれないほど密になった後の、なめらかに減衰する響き。長さ（Decay）と空気感、周波数ごとの減り方を決めます。",
          "The late reverb: once the reflections are too dense to count, a smooth decay. It sets the length (Decay), the air, and how each frequency fades." },
        { "DYN", "DYN (dynamics)", "リバーブの音量を入力に合わせて動かす部品：ダッキング（ドライが鳴る間は下げる）とゲート（キーが下がったら切る）。",
          "Moves the reverb's level with the input: ducking (lower while the dry plays) and the gate (cut once the key falls)." },
        { "MOD", "MOD (modulation)", "時間とともに音を動かす部品：出力コーラス、LFO 2 つ、エンベロープ、そしてそれらをパラメーターへ送るマトリクス。",
          "Moves the sound over time: the output chorus, two LFOs, an envelope, and the matrix that sends them to parameters." },
        // where the sound comes from (the same three words for ER and TAIL)
        { "ROOM", "ROOM (from a room)", "部屋の寸法・材質・位置から計算する。ER は鏡像法で反射を 1 本ずつ、TAIL は部屋の吸音から周波数ごとの残響時間を出します（Eyring の式＋空気の吸収）。",
          "Computed from a room's size, materials and positions. The ER traces each reflection (image sources); the TAIL takes the reverb time per frequency from the room's absorption (Eyring + air)." },
        { "ALGO", "ALGO (algorithmic)", "アルゴリズムで作る。ER は 9 つのパラメーター（First・Length・Taps・Growth…）で反射の並びを描き、TAIL はエンジン（Hall／Plate／Spring／Ambient）で響きを作ります。往年のデジタル機はこの方式。",
          "Made by an algorithm. The ER draws its reflections with nine parameters (First, Length, Taps, Growth...); the TAIL uses an engine (Hall / Plate / Spring / Ambient). The classic digital units work this way." },
        { "IR", "IR (impulse response)", "実際の空間や機材を録った「インパルス応答」をそのまま畳み込む。いちばんリアルだが、長さや明るさは録ったときのまま（Decay などは限られた調整）。",
          "Plays a recorded impulse response of a real space or unit (convolution). The most real, but the length and tone are as recorded (only limited changes)." },
        { "IR+ALGO", "IR + ALGO (hybrid)", "最初は IR、途中から ALGO に引き継ぐ。前半のリアルさと、後半の長さ・明るさの自由さを両立します（IR -> ALGO Time が受け渡しの時刻）。",
          "The IR first, then ALGO takes over: a real onset with a freely set length and tone later (IR -> ALGO Time is the hand-over)." },
        // tail engines
        { "HALL", "Hall engine (FDN)", "部屋・ホール用のエンジン。FDN（フィードバック・ディレイ・ネットワーク）＝長さの違う 16 本の遅延線を行列で混ぜ合わせて戻す回路で、密でなめらかな残響を作ります。線ごとの吸音フィルターで周波数ごとの長さを決めます。",
          "The engine for rooms and halls. An FDN (feedback delay network): 16 delay lines of different lengths mixed by a matrix and fed back, for a dense, smooth reverb. A filter in each line sets the length per frequency." },
        { "FDN", "FDN", "フィードバック・ディレイ・ネットワーク。複数の遅延線の出力を混ぜて入力へ戻す回路。混ぜ方（直交行列）でエネルギーを保ったまま反射の密度を増やし、各線の減衰で残響時間を決めます。このプラグインの Hall エンジン。",
          "Feedback delay network: delay lines whose outputs are mixed and fed back. The mixing (an orthogonal matrix) multiplies the echo density without changing the energy; the loss in each line sets the reverb time. This plug-in's Hall engine." },
        { "PLATE", "Plate engine", "鉄板の振動を模したエンジン。0 ms から密に立ち上がり、明るく、低域は短め。ボーカルやスネアの定番。",
          "Models a vibrating steel plate: dense from 0 ms, bright, shorter lows. The vocal and snare classic." },
        { "SPRING", "Spring engine", "ギターアンプのばねリバーブ。ばねの中で高い音ほど速く伝わる（分散）ので「ビヨーン」と鳴く。帯域は 5 kHz 程度まで。",
          "The guitar amp's spring. In a spring high frequencies travel faster (dispersion), hence the boing. Band up to about 5 kHz." },
        { "AMBIENT", "Ambient engine", "部屋を超える巨大な空間のエンジン。とても長い遅延線と揺らぎ、Gravity（前寄り・後ろ寄り）と Feedback（入力へ戻す）。",
          "An engine for spaces beyond rooms: very long lines and movement, Gravity (front- or back-heavy) and Feedback (back into the input)." },
        // groups
        { "SOURCE", "SOURCE", "響きの「素材」の作り方（ROOM／ALGO／IR）と、その方式だけのパラメーター。",
          "How the raw reverb is made (ROOM / ALGO / IR) and the parameters that only that method has." },
        { "VOICE", "VOICE", "どの方式でも共通の「声」：プリディレイ、大きさ、広がり、拡散、フィルター、レベルなど。SOURCE を切り替えても残ります。",
          "The voice common to every method: predelay, size, width, diffusion, filters, level. It stays when you switch SOURCE." },
        { "COLOR", "COLOR", "往年のデジタル機の「色」：帯域の上限（Bandwidth）、ビット数（Bits）、時間とともに暗くなる量（Darkening）、ゆらぎ（Wander）。",
          "The colour of classic digital units: the band limit (Bandwidth), bit depth (Bits), darkening over time (Darkening), movement (Wander)." },
        { "INPUT", "INPUT", "リバーブに入る前の音の整え方：入力モード、広がり、傾き（Tilt）、ローカット・ハイカット（スロープ付き）、それをどこにかけるか。",
          "Shapes the sound before the reverb: input mode, width, Tilt, low / high cut (with slopes), and where it applies." },
        { "OUTPUT", "OUTPUT", "出口：ドライとウェットの割合（Mix）、出力レベル、広がり、ER と TAIL のバランス、ステレオ／モノ、MASTER EQ。",
          "The exit: dry / wet (Mix), output level, width, ER vs TAIL balance, stereo / mono, MASTER EQ." },
        { "MASTER EQ", "MASTER EQ", "リバーブ音（ER＋TAIL）全体の EQ。周波数ごとの「長さ」ではなく「量」を変えます（長さは RT の点）。",
          "An EQ on the whole reverb (ER + TAIL). It changes the amount per frequency, not the length (that is the RT points)." },
        { "DUCKING", "Ducking", "ドライ（入力）が鳴っている間だけリバーブを下げ、隙間で戻す。Single＝全体、Multi＝2〜5 帯域ごと、Spectral＝半オクターブ 17 帯域ごと。",
          "Lowers the reverb while the dry plays and lets it back in the gaps. Single: all; Multi: per band (2-5); Spectral: per half-octave (17)." },
        { "GATE", "Gate", "キーが閾値を下回り Hold が過ぎたら、Release でリバーブを切る。80 年代のゲートリバーブ。",
          "Cuts the reverb over the release once the key is below the threshold and the hold has passed: the '80s gated reverb." },
        { "MATRIX", "Modulation matrix", "ソース（LFO 1・LFO 2・エンベロープ）を、行き先のパラメーターへ量（±%）を決めて送る配線表。4 スロット。",
          "A patch table sending sources (LFO 1, LFO 2, envelope) to destination parameters by an amount (+/- %). Four slots." },
        { "LFO", "LFO / envelope / chorus", "LFO＝周期的にゆっくり動く信号（波形と速さ、テンポ同期）。エンベロープ＝入力の大きさを追う信号。コーラス＝リバーブ音を揺らして広げる。",
          "LFO: a slowly repeating signal (shape, rate, tempo sync). Envelope: follows the input's level. Chorus: moves and widens the reverb." },
        { "METER", "Meter", "いまのダッキング量（帯域ごと）と、ゲートの開き具合。", "How much is ducked now (per band) and how open the gate is." },
        { "SOURCES", "Sources", "LFO 1・LFO 2・エンベロープのいまの動き。", "What LFO 1, LFO 2 and the envelope are doing now." },
        // the deck's groups
        { "DAMPING", "DAMPING", "周波数ごとの「減り方」の調整。高域（HF）や低域（Bass）の残響時間を中域に対して短く／長くする。実際の部屋は空気と壁が高域をよく吸うので、高域が先に消えます（高域を短く＝自然、長く＝明るく人工的）。",
          "How each frequency decays. Makes the highs (HF) or lows (Bass) shorter or longer than the mids. Real rooms lose the highs first (air and walls absorb them): shorter highs = natural, longer = bright, artificial." },
        { "SHAPE", "SHAPE", "響きの「形」：空間の大きさ（Size）と、立ち上がり方（Attack・Spread）、ER とテールの量。音が鳴ってから響きがどう膨らんでいくか。",
          "The shape of the reverb: the space's size (Size), how it builds (Attack, Spread), the amounts of ER and tail - how the reverb grows after a note." },
        { "SPACE", "SPACE", "空間の基本：残響時間（Decay）とプリディレイ（直接音から響きまでの間）。", "The basics of the space: reverb time (Decay) and predelay (the gap before the reverb)." },
        { "DIFF", "DIFFUSION", "拡散：反射が一つずつ聞こえる（低い）か、なめらかに溶け合う（高い）か。", "Diffusion: reflections you can hear one by one (low) or a smooth blend (high)." },
        { "MACRO", "MACRO", "1 つのつまみで複数のパラメーターを一緒に動かす。ヘルプに、EDIT で実際に動くパラメーターと値が出ます。", "One knob moves several parameters. The help shows which EDIT parameters move and their values." },
        { "OUT", "OUT", "ドライとウェットの割合（Mix）と出力。", "Dry / wet (Mix) and the output." },
        { "QUICK", "QUICK", "よく使う基本の操作だけをまとめたもの。", "Just the most used basic controls." },
        // single words used around the UI
        { "EQ", "RT mode: EQ", "周波数ごとの残響時間を、5 つの点（シェルフ 2・ベル 3）で描くモード。なめらかに変わります。",
          "Draws the reverb time per frequency with five points (two shelves, three bells): smooth changes." },
        { "MULTIBAND", "RT mode: Multiband", "帯域を 2〜5 に分け、帯域ごとに残響時間の倍率を決めるモード。往年の機材の「Bass Mult」「Crossover」と同じ考え方。",
          "Splits into 2-5 bands and sets a reverb-time multiplier per band, as the classic units' Bass Mult and Crossover do." },
        { "SINGLE", "Ducking: Single", "全体を一緒に下げる（いちばんシンプル）。", "Turns everything down together (the simplest)." },
        { "MULTI", "Ducking: Multi", "2〜5 帯域に分け、ドライの各帯域の大きさで、その帯域だけ下げる。", "Splits into 2-5 bands; each band ducks by the dry's level in that band." },
        { "SPECTRAL", "Ducking: Spectral", "半オクターブ 17 帯域ごとに、ドライと重なる帯域だけ下げる（動く EQ）。量感を保ったまま明瞭に。",
          "Per half-octave (17 bands), lowers only where the dry overlaps (a moving EQ): clear without losing size." },
    };
    for (auto& x : t) if (key.equalsIgnoreCase (x.key)) return &x;
    return nullptr;
}
