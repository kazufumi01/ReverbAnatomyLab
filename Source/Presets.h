#pragma once
// Factory presets and lessons. Model names describe an era and a use, not a product.
#include <vector>
#include <utility>
#include <string>
#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

struct PresetDef
{
    const char* name; int group;          // 0 = real spaces (ROOM), 1 = classic digital (ALGO), 2 = starting points, 3 = reverb (ER + TAIL)
    const char* ja; const char* en;
    std::vector<std::pair<const char*, float>> v;   // everything else goes back to its default
    int unit = -1, prog = 0;                        // classic units: faceplate and program
    std::vector<float> panel;                       // panel values (mapped into the parameters on load)
};

// ===========================================================================
//  Hardware panels. Each classic unit gets a faceplate with its own controls; moving one writes
//  the internal parameters through map(), so the panel and the BUILD view always agree.
//  Faceplates take the unit's colours but no names or logos.
// ===========================================================================
struct PanelCtl { const char* label; const char* unit; float lo, hi; int kind; int dec; bool log; const char* ja; const char* en; };   // kind 0 knob, 1 fader
using ParamList = std::vector<std::pair<const char*, float>>;
struct UnitDef
{
    const char* face; const char* era;                   // faceplate title, sub-line
    juce::uint32 bg, ink, accent, dispBg, dispInk; int disp;  // disp: 0 red LED, 1 green LCD, 2 blue OLED, 3 amber VFD
    std::vector<PanelCtl> ctl;
    std::vector<const char*> programs;
    void (*map) (int prog, const std::vector<float>& v, ParamList& out);
};
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

inline const std::vector<UnitDef>& units()
{
    static const std::vector<UnitDef> u {
        // 0 mechanical plate (remote damper box)
        { "STEEL PLATE", "mechanical plate, 1950s", 0xffb9b4a7, 0xff26231e, 0xff8a2a1e, 0xff101010, 0xffff4a3a, 0,
          { { "DECAY", "s", 0.6f, 5.0f, 0, 1, false, "ダンパーの位置＝残響時間（Tail Preview の RT）。", "Damper position = reverb time (Tail Preview RT)." },
            { "TONE", "", 0.0f, 10.0f, 0, 1, false, "板の明るさ。上げるほど Damping が上がり、後半まで明るい。", "Plate brightness. Higher = higher Damping, brighter to the end." } },
          { "PLATE" },
          [] (int, const std::vector<float>& v, ParamList& o) { o = { { "tailRT", v[0] }, { "damp", 8000 + 600 * v[1] } }; } },
        // 1 80s flagship processor (fader remote)
        { "DIGITAL REVERB PROCESSOR", "flagship, 1980s", 0xff2a2c30, 0xffe2e2e2, 0xffd8d8d8, 0xff0a0a0a, 0xffff3b30, 0,
          { { "RT MID", "s", 0.2f, 10.0f, 1, 1, true, "中域の残響時間（Tail Preview の RT）。", "Mid-band reverb time (Tail Preview RT)." },
            { "SIZE", "m", 9.0f, 71.0f, 1, 0, false, "空間の大きさ。すべての反射の時間を伸縮（VOICE の Size）。", "Space size: stretches every reflection (VOICE Size)." },
            { "PREDELAY", "ms", 0.0f, 200.0f, 1, 0, false, "ER 全体を後ろへ（Predelay）。", "Moves the ER later (Predelay)." },
            { "SHAPE", "", 0.0f, 255.0f, 1, 0, false, "響きの立ち上がりの形。小さい＝すぐ減衰、大きい＝後ろへふくらむ（Envelope と Growth）。", "Shape of the build-up. Low = decays at once, high = swells towards the back (Envelope and Growth)." },
            { "SPREAD", "", 0.0f, 255.0f, 1, 0, false, "立ち上がりにかける時間＝ER の長さ（Length）。", "How long the build-up takes = ER length (Length)." },
            { "DIFFUSION", "", 0.0f, 99.0f, 1, 0, false, "反射のにじみ（Diffusion）。", "Smearing of the reflections (Diffusion)." } },
          { "HALL", "PLATE", "ROOM", "CHAMBER" },
          [] (int, const std::vector<float>& v, ParamList& o)
          { o = { { "tailRT", v[0] }, { "size", clampf (100.0f * std::pow (v[1] / 35.0f, 0.9f), 50, 200) }, { "predelay", v[2] },
                  { "decay", -24 + 36 * v[3] / 255 }, { "growth", 0.2f + 1.6f * v[3] / 255 }, { "length", 15 + 225 * v[4] / 255 }, { "diffusion", v[5] / 99 * 100 } }; } },
        // 2 early-80s British non-linear unit
        { "DIGITAL REVERB  NON-LINEAR", "British, early 1980s", 0xff23262b, 0xffd9dde2, 0xffe04a3a, 0xff0a0a0a, 0xffff3b30, 0,
          { { "PRE-DELAY", "ms", 0.0f, 200.0f, 0, 0, false, "ER 全体を後ろへ（Predelay）。", "Moves the ER later (Predelay)." },
            { "LENGTH", "ms", 20.0f, 400.0f, 0, 0, true, "ノンリニアの長さ＝ゲートが閉じる時刻（Length）。", "Non-linear length = where the gate closes (Length)." },
            { "HF DAMP", "Hz", 2000.0f, 16000.0f, 0, 0, true, "後半の高域（Damping）。", "Treble of the later reflections (Damping)." },
            { "WIDTH", "%", 0.0f, 150.0f, 0, 0, false, "左右の広がり（Width）。", "Stereo width (Width)." } },
          { "AMBIENCE", "NONLIN", "REVERSE" },
          [] (int, const std::vector<float>& v, ParamList& o) { o = { { "predelay", v[0] }, { "length", v[1] }, { "damp", v[2] }, { "width", v[3] } }; } },
        // 3 German room simulator
        { "ROOM SIMULATOR", "German, 1980s", 0xffd8d3c4, 0xff2a2620, 0xff9c3b2a, 0xff151210, 0xffff5a3a, 0,
          { { "ROOM SIZE", "m3", 2.0f, 4.5f, 0, 0, false, "部屋の容積（10^x m³）。体積の 1/3 乗で反射の時間が伸びる（Size）。", "Room volume (10^x m3). Reflection times grow with its cube root (Size)." },
            { "1ST REFL", "ms", 1.0f, 80.0f, 0, 1, true, "最初の反射までの時間（First）。", "Time to the first reflection (First)." },
            { "ENHANCE", "dB", -12.0f, 6.0f, 0, 1, false, "初期反射の量（Energy）。", "Amount of early reflections (Energy)." },
            { "RT", "s", 0.1f, 20.0f, 0, 1, true, "残響時間（Tail Preview の RT）。", "Reverb time (Tail Preview RT)." },
            { "HF DECAY", "", 0.1f, 1.0f, 0, 2, false, "高域の減衰の速さ。小さいほど暗い（Damping）。", "How fast the treble decays. Lower = darker (Damping)." } },
          { "ROOM" },
          [] (int, const std::vector<float>& v, ParamList& o)
          { o = { { "size", clampf (100.0f * std::cbrt (std::pow (10.0f, v[0]) / 2000.0f), 50, 200) }, { "first", v[1] }, { "energy", -6 + v[2] },
                  { "tailRT", v[3] }, { "damp", 2000 + 14000 * v[4] } }; } },
        // 4 first-generation digital (lever panel)
        { "DIGITAL REVERBERATOR", "first generation, 1970s", 0xff2b231e, 0xffe8dccb, 0xffd8402a, 0xff0d0a08, 0xffff4a30, 0,
          { { "DECAY", "s", 0.4f, 4.5f, 1, 1, false, "残響時間（Tail Preview の RT）。", "Reverb time (Tail Preview RT)." },
            { "PREDELAY", "ms", 0.0f, 160.0f, 1, 0, false, "ER 全体を後ろへ（Predelay）。", "Moves the ER later (Predelay)." },
            { "HIGH", "x", 0.25f, 1.0f, 1, 2, false, "高域の減衰（Damping）。", "Treble decay (Damping)." } },
          { "REVERB" },
          [] (int, const std::vector<float>& v, ParamList& o) { o = { { "tailRT", v[0] }, { "predelay", v[1] }, { "damp", 2000 + 10000 * v[2] } }; } },
        // 5 late-70s digital hall (slider head)
        { "DIGITAL REVERB", "late 1970s", 0xffb89e72, 0xff2a2116, 0xff7a3b12, 0xff120e08, 0xffff5a30, 0,
          { { "DECAY", "s", 0.5f, 8.0f, 1, 1, true, "残響時間（Tail Preview の RT）。", "Reverb time (Tail Preview RT)." },
            { "TREBLE", "x", 0.2f, 1.0f, 1, 2, false, "高域の減衰（Damping）。", "Treble decay (Damping)." },
            { "DEPTH", "", 0.0f, 100.0f, 1, 0, false, "音源の奥行き。深いほど初回反射が遅く、ER が長い（First と Length）。", "Source depth. Deeper = later first reflection and a longer ER (First and Length)." },
            { "DIFFUSION", "", 0.0f, 7.0f, 1, 1, false, "反射のにじみ（Diffusion）。", "Smearing (Diffusion)." },
            { "CHORUS", "", 0.0f, 6.0f, 1, 1, false, "反射の揺らぎ（Wander）。", "Movement of the reflections (Wander)." } },
          { "HALL" },
          [] (int, const std::vector<float>& v, ParamList& o)
          { o = { { "tailRT", v[0] }, { "damp", 2500 + 9000 * v[1] }, { "first", 4 + 30 * v[2] / 100 }, { "length", 60 + 150 * v[2] / 100 },
                  { "diffusion", v[3] / 7 * 100 }, { "wander", v[4] / 6 * 100 } }; } },
        // 6 late-80s budget multi-effects (LCD menu)
        { "DIGITAL MULTI-EFFECT", "budget, late 1980s", 0xff1d1f22, 0xffd9dde2, 0xff4f9a6a, 0xffa9c98a, 0xff1d2a12, 1,
          { { "ROOM SIZE", "", 0.1f, 20.0f, 0, 1, false, "ER の長さと初回反射（Length と First）。", "ER length and first reflection (Length and First)." },
            { "LIVENESS", "", 0.0f, 10.0f, 0, 1, false, "反射の残り方。上げるほど後ろまで強い（Envelope）。", "How lively the reflections stay. Higher = stronger to the end (Envelope)." },
            { "INIT DLY", "ms", 0.0f, 200.0f, 0, 0, false, "最初の遅れ（Predelay）。", "Initial delay (Predelay)." },
            { "DIFFUSION", "", 0.0f, 10.0f, 0, 0, false, "反射のにじみ（Diffusion）。", "Smearing (Diffusion)." },
            { "ER NUMBER", "", 1.0f, 19.0f, 0, 0, false, "反射の数（Reflections）。", "Number of reflections (Reflections)." } },
          { "HALL", "RANDOM", "REVERSE", "PLATE" },
          [] (int prog, const std::vector<float>& v, ParamList& o)
          { o = { { "length", 10 + 9 * v[0] }, { "first", std::max (1.0f, v[0] * (prog == 1 ? 0.4f : prog == 3 ? 0.15f : 1.0f)) },
                  { "decay", prog == 2 ? 2 * v[1] : -20 + 2 * v[1] }, { "predelay", v[2] }, { "diffusion", 10 * v[3] }, { "taps", std::round (1.26f * std::round (v[4])) } }; } },
        // 7 early-80s stereo room unit
        { "SIGNAL PROCESSOR  STEREO ROOM", "early 1980s", 0xff1b1c20, 0xffdfe2e6, 0xff3d7bd9, 0xff0a0a0a, 0xffff3b30, 0,
          { { "PREDELAY", "ms", 0.0f, 200.0f, 0, 0, false, "ER 全体を後ろへ（Predelay）。", "Moves the ER later (Predelay)." },
            { "DECAY", "s", 0.1f, 20.0f, 0, 1, true, "残響時間（Tail Preview の RT）。", "Reverb time (Tail Preview RT)." },
            { "POSITION", "", 0.0f, 100.0f, 0, 0, false, "聴く位置（前→後ろ）。後ろほど直接音が遠く、初回反射が早く、ER が多い（First と Energy）。", "Listening position (front -> back). Further back = the direct sound is farther, the first reflection comes sooner and the ER is louder (First and Energy)." },
            { "HF DECAY", "x", 0.1f, 1.0f, 0, 2, false, "高域の減衰（Damping）。", "Treble decay (Damping)." },
            { "DIFFUSION", "%", 0.0f, 100.0f, 0, 0, false, "反射のにじみ（Diffusion）。", "Smearing (Diffusion)." } },
          { "STEREO ROOM", "ROOM", "HI-DENSITY PLATE" },
          [] (int prog, const std::vector<float>& v, ParamList& o)
          { const float p = v[2] / 100;
            o = { { "predelay", v[0] }, { "tailRT", v[1] }, { "first", prog == 2 ? 1.0f : 2 + 16 * (1 - p) }, { "energy", prog == 2 ? -5.0f : -12 + 10 * p },
                  { "damp", 2000 + 12000 * v[3] }, { "diffusion", v[4] } }; } },
        // 8 modern high-end unit
        { "STEREO REVERB PROCESSOR", "modern high-end, 2000s", 0xffc4c7cb, 0xff1e2126, 0xff2f6fb0, 0xff05070a, 0xff7fd0ff, 2,
          { { "REVERB TIME", "s", 0.2f, 12.0f, 0, 1, true, "残響時間（Tail Preview の RT）。", "Reverb time (Tail Preview RT)." },
            { "PREDELAY", "ms", 0.0f, 200.0f, 0, 0, false, "ER 全体を後ろへ（Predelay）。", "Moves the ER later (Predelay)." },
            { "SIZE", "", 0.0f, 100.0f, 0, 0, false, "空間の大きさ（Size）。", "Space size (Size)." },
            { "DENSITY", "", 0.0f, 100.0f, 0, 0, false, "反射の密度。本数と拡散が一緒に増える（Reflections と Diffusion）。", "Reflection density: number and diffusion together (Reflections and Diffusion)." },
            { "EARLY", "dB", -20.0f, 6.0f, 0, 1, false, "初期反射の量（Energy）。", "Early reflection level (Energy)." },
            { "ER SELECT", "", 1.0f, 16.0f, 0, 0, false, "初期反射のパターン（Variation）。", "Early reflection pattern (Variation)." },
            { "HF ROLLOFF", "Hz", 1000.0f, 20000.0f, 0, 0, true, "高域のロールオフ（High Cut）。", "High-frequency roll-off (High Cut)." },
            { "VLF CUT", "Hz", 20.0f, 200.0f, 0, 0, true, "超低域のカット（Low Cut）。", "Very-low-frequency cut (Low Cut)." } },
          { "HALL", "ROOM", "PLATE", "CHAMBER" },
          [] (int prog, const std::vector<float>& v, ParamList& o)
          { static const float e0[4] = { -8, -7, -5, -7 };
            o = { { "tailRT", v[0] }, { "predelay", v[1] }, { "size", 60 + 0.8f * v[2] }, { "taps", std::round (30 + 0.9f * v[3]) }, { "diffusion", 40 + 0.5f * v[3] },
                  { "energy", e0[std::clamp (prog, 0, 3)] + v[4] }, { "vari", std::round (v[5]) }, { "highcut", v[6] }, { "lowcut", v[7] } }; } },
    };
    return u;
}

inline const std::vector<PresetDef>& presets()
{
    static const std::vector<PresetDef> p {
        // ---------------- starting points ----------------
        { "Init — Room", 2, "初期値の部屋（6×8×3.2 m、しっくいの壁・木の床）。", "Default room (6 x 8 x 3.2 m, plaster walls, wooden floor).", { { "src", 0 } } },
        { "Init — Algo", 2, "初期値の ALGO（アルゴリズムで描く ER）。", "Default ALGO (the algorithmic ER).", { { "src", 1 } } },
        // ---------------- reverb (ER + TAIL) ----------------
        { "Ambient Wash", 3, "長く揺れる残響（9 s、大きな Size、揺れ多め）。パッド・ギターのアンビエンスに。", "A long, moving wash (9 s, a big size, more modulation) for pads and ambient guitar.",
          { { "erOn", 0 }, { "tDecay", 9 }, { "tSize", 260 }, { "tPre", 30 }, { "tDiff", 100 }, { "tModD", 60 }, { "tModR", 0.3f }, { "rtM5", 0.5f }, { "iLC", 120 }, { "mix", 50 } } },
        // ---------------- real spaces: image-source rooms ----------------
        { "Vocal Booth", 0, "2.2×2.6 m の吸音ブース。初回反射 2〜3 ms、すぐ消える。ドライなボーカルに「部屋の中にいる」最小限の空気を足す。",
          "A 2.2 x 2.6 m treated booth. First reflection at 2-3 ms, gone almost at once. The smallest bit of air for a dry vocal.",
          { { "src", 0 }, { "roomW", 2.2f }, { "roomD", 2.6f }, { "roomH", 2.3f }, { "wall", 4 }, { "floor", 5 }, { "order", 4 }, { "srcX", 0.5f }, { "srcY", 0.31f }, { "lisX", 0.5f }, { "lisY", 0.62f }, { "diffusion", 15 } } },
        { "Bedroom", 0, "3.6×4.2 m の生活空間。家具と布で中高域が柔らかい。宅録のアコギやボーカルの「家っぽさ」。",
          "A 3.6 x 4.2 m furnished room. Soft upper mids from fabric. The home-recording feel on guitar or vocal.",
          { { "src", 0 }, { "roomW", 3.6f }, { "roomD", 4.2f }, { "roomH", 2.4f }, { "wall", 3 }, { "floor", 5 }, { "order", 4 }, { "srcX", 0.39f }, { "srcY", 0.31f }, { "lisX", 0.61f }, { "lisY", 0.67f }, { "diffusion", 25 } } },
        { "Tiled Bathroom", 0, "1.8×2.6 m のタイル張り。硬い面だけで、反射が多く明るい。短いのに強い響き。",
          "A 1.8 x 2.6 m tiled room. Hard surfaces only: many bright reflections. Short but strong.",
          { { "src", 0 }, { "roomW", 1.8f }, { "roomD", 2.6f }, { "roomH", 2.4f }, { "wall", 0 }, { "floor", 0 }, { "order", 5 }, { "srcX", 0.33f }, { "srcY", 0.31f }, { "lisX", 0.61f }, { "lisY", 0.73f } } },
        { "Live Room (Wood)", 0, "7×9×3.6 m の木のライブルーム。初回反射 5〜10 ms。ドラムのアンビエンスや、生楽器の自然な距離感。",
          "A 7 x 9 x 3.6 m wooden live room. First reflections at 5-10 ms. Drum ambience and a natural distance for acoustic sources.",
          { { "src", 0 }, { "roomW", 7.0f }, { "roomD", 9.0f }, { "roomH", 3.6f }, { "wall", 3 }, { "floor", 3 }, { "order", 4 }, { "srcX", 0.46f }, { "srcY", 0.29f }, { "lisX", 0.54f }, { "lisY", 0.69f }, { "diffusion", 20 } } },
        { "Stone Drum Room", 0, "8×10×5 m の石の部屋。硬く明るく、スネアに当たると反射の粒がはっきり出る。",
          "An 8 x 10 x 5 m stone room. Hard and bright: each reflection is audible on a snare.",
          { { "src", 0 }, { "roomW", 8.0f }, { "roomD", 10.0f }, { "roomH", 5.0f }, { "wall", 1 }, { "floor", 3 }, { "order", 4 }, { "srcX", 0.5f }, { "srcY", 0.32f }, { "lisX", 0.42f }, { "lisY", 0.7f } } },
        { "Concrete Echo Chamber", 0, "4×6×3 m のコンクリートのエコーチェンバー。スピーカーとマイクを部屋に置く、スタジオの古典的な残響室。反射が密で明るい。",
          "A 4 x 6 x 3 m concrete echo chamber: the classic studio room with a speaker and a mic. Dense, bright reflections.",
          { { "src", 0 }, { "roomW", 4.0f }, { "roomD", 6.0f }, { "roomH", 3.0f }, { "wall", 1 }, { "floor", 1 }, { "order", 5 }, { "srcX", 0.2f }, { "srcY", 0.15f }, { "lisX", 0.8f }, { "lisY", 0.85f }, { "diffusion", 35 } } },
        { "Small Club", 0, "10×14×4 m のクラブ。しっくいの壁と木の床。ライブ録音の距離感。",
          "A 10 x 14 x 4 m club with plaster walls and a wooden floor. The distance of a live recording.",
          { { "src", 0 }, { "roomW", 10.0f }, { "roomD", 14.0f }, { "roomH", 4.0f }, { "wall", 2 }, { "floor", 3 }, { "order", 4 }, { "srcX", 0.5f }, { "srcY", 0.15f }, { "lisX", 0.55f }, { "lisY", 0.6f }, { "diffusion", 25 } } },
        { "Concert Hall", 0, "22×34×14 m のホール。客席が床の反射を吸う。初回反射 15〜30 ms、ERだけでも奥行きが出る。",
          "A 22 x 34 x 14 m hall. The seats absorb the floor bounce. First reflections at 15-30 ms; depth from the ER alone.",
          { { "src", 0 }, { "roomW", 22.0f }, { "roomD", 34.0f }, { "roomH", 14.0f }, { "wall", 3 }, { "floor", 6 }, { "order", 3 }, { "srcX", 0.5f }, { "srcY", 0.15f }, { "lisX", 0.59f }, { "lisY", 0.59f }, { "diffusion", 40 } } },
        { "Stone Chapel", 0, "12×26×12 m の石造り。反射が強く長く、減衰しにくい。",
          "A 12 x 26 x 12 m stone chapel. Strong, long reflections that hardly decay.",
          { { "src", 0 }, { "roomW", 12.0f }, { "roomD", 26.0f }, { "roomH", 12.0f }, { "wall", 1 }, { "floor", 1 }, { "order", 3 }, { "srcX", 0.5f }, { "srcY", 0.15f }, { "lisX", 0.62f }, { "lisY", 0.58f }, { "diffusion", 45 } } },

        // ---------------- by shape (0.10.4): fan, skewed, dome, connected, corridor, clutter ----------------
        { "Fan Hall (Concert)", 3, "26×32×13 m の扇形ホール（ステージ側が狭く奥へ開く）。平行な壁がないので側壁の反射が客席へ散り、フラッターが出にくい。客席の人と椅子で高域はやや柔らかい。ER と TAIL ともこの部屋から。", "A 26 x 32 x 13 m fan-shaped hall (narrow at the stage, opening toward the back). With no parallel walls the side reflections fan out and flutter is rare; the audience softens the highs. ER and TAIL both come from this room.",
          { { "src", 0 }, { "roomW", 26.0f }, { "roomD", 32.0f }, { "roomH", 13.0f }, { "wall", 2 }, { "floor", 6 }, { "order", 3 }, { "shape", 1 }, { "splay", 45.0f }, { "clutter", 25.0f }, { "srcX", 0.5f }, { "srcY", 0.1f }, { "lisX", 0.5f }, { "lisY", 0.55f }, { "tRoom", 1 }, { "predelay", 12.0f }, { "mix", 30 } } },
        { "Fan Hall (Small Theater)", 3, "14×20×8 m の小さな扇形の劇場。客席（Clutter 40 %）が高域と後半の勢いを吸う。ボーカルや語りに自然な広がり。", "A small 14 x 20 x 8 m fan-shaped theater. The seats (Clutter 40 %) take the highs and the energy of the late part. Natural width for vocals or speech.",
          { { "src", 0 }, { "roomW", 14.0f }, { "roomD", 20.0f }, { "roomH", 8.0f }, { "wall", 3 }, { "floor", 6 }, { "order", 3 }, { "shape", 1 }, { "splay", 35.0f }, { "clutter", 40.0f }, { "srcX", 0.5f }, { "srcY", 0.1f }, { "lisX", 0.5f }, { "lisY", 0.55f }, { "tRoom", 1 }, { "mix", 28 } } },
        { "Skewed Studio Room", 3, "6×8×3.2 m で左の壁だけ 18 % 傾けた部屋。向かい合う平行面がないので、手拍子やスネアのフラッターエコーが消え、素直な部屋鳴りになる。家具（Clutter 30 %）で中高域が少し丸い。", "A 6 x 8 x 3.2 m room with the left wall leaned by 18 %. No facing parallel walls, so the flutter echo of a clap or a snare disappears. Furnishings (Clutter 30 %) round off the upper mids.",
          { { "src", 0 }, { "roomW", 6.0f }, { "roomD", 8.0f }, { "roomH", 3.2f }, { "wall", 3 }, { "floor", 3 }, { "order", 3 }, { "shape", 2 }, { "splay", 18.0f }, { "clutter", 30.0f }, { "srcX", 0.4f }, { "srcY", 0.25f }, { "lisX", 0.62f }, { "lisY", 0.65f }, { "tRoom", 1 }, { "mix", 25 } } },
        { "Skewed Live Room", 3, "9×12×4 m のライブルーム、左壁を 25 % 傾けた形。木の壁と床で明るいが、反射の向きが散るのでドラムのアンビエンスが濁りにくい。", "A 9 x 12 x 4 m live room with the left wall leaned by 25 %. Bright wood walls and floor, but the reflections point in different directions, so drum ambience stays clear.",
          { { "src", 0 }, { "roomW", 9.0f }, { "roomD", 12.0f }, { "roomH", 4.0f }, { "wall", 3 }, { "floor", 3 }, { "order", 3 }, { "shape", 2 }, { "splay", 25.0f }, { "clutter", 15.0f }, { "srcX", 0.45f }, { "srcY", 0.25f }, { "lisX", 0.6f }, { "lisY", 0.62f }, { "tRoom", 1 }, { "mix", 28 } } },
        { "Dome (Planetarium)", 3, "直径 20 m・壁 6 m のコンクリートのドーム（Splay 100 % ＝半球）。天頂からの反射が集まり、中心付近でフォーカスして聞こえる。何もない空間（Clutter 0 %）。", "A 20 m wide concrete dome with 6 m walls (Splay 100 % = a hemisphere). Reflections from the zenith converge, so the middle focuses. An empty space (Clutter 0 %).",
          { { "src", 0 }, { "roomW", 20.0f }, { "roomD", 20.0f }, { "roomH", 6.0f }, { "wall", 1 }, { "floor", 1 }, { "order", 3 }, { "shape", 3 }, { "splay", 100.0f }, { "clutter", 0.0f }, { "srcX", 0.5f }, { "srcY", 0.5f }, { "lisX", 0.62f }, { "lisY", 0.55f }, { "tRoom", 1 }, { "mix", 30 } } },
        { "Dome (Cupola)", 3, "直径 16 m・壁 10 m の石のドーム（Splay 60 %）。壁が高く、天井が浅く丸い大聖堂のクーポラ。長く明るい。", "A 16 m wide stone dome with 10 m walls (Splay 60 %): the cupola of a cathedral, with a tall drum and a shallow round ceiling. Long and bright.",
          { { "src", 0 }, { "roomW", 16.0f }, { "roomD", 16.0f }, { "roomH", 10.0f }, { "wall", 1 }, { "floor", 1 }, { "order", 3 }, { "shape", 3 }, { "splay", 60.0f }, { "clutter", 5.0f }, { "srcX", 0.5f }, { "srcY", 0.35f }, { "lisX", 0.58f }, { "lisY", 0.62f }, { "tRoom", 1 }, { "mix", 30 } } },
        { "Dome (Small Cupola)", 3, "直径 8 m・壁 4 m のしっくいのドーム（Splay 70 %）。小さな礼拝堂の丸天井。短く、頭上から降る反射。", "An 8 m wide plaster dome with 4 m walls (Splay 70 %): a small chapel's round ceiling. Short, with reflections falling from overhead.",
          { { "src", 0 }, { "roomW", 8.0f }, { "roomD", 8.0f }, { "roomH", 4.0f }, { "wall", 2 }, { "floor", 3 }, { "order", 3 }, { "shape", 3 }, { "splay", 70.0f }, { "clutter", 10.0f }, { "srcX", 0.5f }, { "srcY", 0.4f }, { "lisX", 0.55f }, { "lisY", 0.6f }, { "tRoom", 1 }, { "mix", 28 } } },
        { "Connected (Live Room + Booth)", 3, "手前のライブルーム（木・8×10 m）と奥のブース側（吸音 +60 %）をつないだ形。音源もリスナーも手前の部屋なので、最初は手前の部屋の反射が速く減り、戸口から戻る奥の部屋の遅い尾が後から顔を出す（二重勾配）。", "A live room (wood, 8 x 10 m) joined to a more absorbent room behind it (+60 %). Source and listener are in the front room: its reflections die fast first, then the slow tail returning through the door shows up later (double slope).",
          { { "src", 0 }, { "roomW", 8.0f }, { "roomD", 10.0f }, { "roomH", 3.2f }, { "wall", 3 }, { "floor", 3 }, { "order", 3 }, { "shape", 4 }, { "splay", 40.0f }, { "clutter", 20.0f }, { "srcX", 0.4f }, { "srcY", 0.2f }, { "lisX", 0.6f }, { "lisY", 0.4f }, { "tRoom", 1 }, { "backAbs", 60.0f }, { "mix", 28 } } },
        { "Connected (Hall + Stone Foyer)", 3, "ホール（20×28 m）の奥に硬い石のロビーが戸口でつながる。リスナーは奥のロビーで、ホールからの音は戸口越しに遅れて小さく届き、遅い勾配だけが聞こえる。", "A hall (20 x 28 m) with a hard stone foyer behind it, joined by a doorway. The listener stands in the foyer: the hall's sound arrives late and small through the door - only the slow slope is heard.",
          { { "src", 0 }, { "roomW", 20.0f }, { "roomD", 28.0f }, { "roomH", 10.0f }, { "wall", 2 }, { "floor", 6 }, { "order", 3 }, { "shape", 4 }, { "splay", 40.0f }, { "clutter", 15.0f }, { "srcX", 0.4f }, { "srcY", 0.2f }, { "lisX", 0.65f }, { "lisY", 0.8f }, { "tRoom", 1 }, { "backAbs", -60.0f }, { "mix", 32 } } },
        { "Connected (Same Room)", 3, "連結（戸口の幅 40 %）で、音源もリスナーも手前の部屋。減衰が途中で折れる二重勾配を、いちばん分かりやすく聞ける設定。ドラムやスネアで。", "Connected rooms (door 40 %) with source and listener in the same, front room. The clearest way to hear the double slope, where the decay bends partway. Try it on drums or a snare.",
          { { "src", 0 }, { "roomW", 10.0f }, { "roomD", 14.0f }, { "roomH", 4.0f }, { "wall", 1 }, { "floor", 3 }, { "order", 3 }, { "shape", 4 }, { "splay", 40.0f }, { "clutter", 10.0f }, { "srcX", 0.45f }, { "srcY", 0.2f }, { "lisX", 0.55f }, { "lisY", 0.38f }, { "tRoom", 1 }, { "backAbs", 30.0f }, { "mix", 30 } } },
        { "Corridor (Hallway)", 3, "6×24×3 m の廊下で奥で右に曲がる（幅 Splay 15 %）。細長く平行な壁が続くのでフラッター気味の連続した反射、曲がり角の先は直接音がさえぎられて反射から届く。", "A 6 x 24 x 3 m hallway that turns right at the back (width Splay 15 %). Long parallel walls give fluttery repeating reflections; beyond the bend the direct sound is blocked and the first arrival is a reflection.",
          { { "src", 0 }, { "roomW", 6.0f }, { "roomD", 24.0f }, { "roomH", 3.0f }, { "wall", 2 }, { "floor", 3 }, { "order", 3 }, { "shape", 5 }, { "splay", 15.0f }, { "clutter", 10.0f }, { "srcX", 0.5f }, { "srcY", 0.1f }, { "lisX", 0.4f }, { "lisY", 0.9f }, { "tRoom", 1 }, { "mix", 28 } } },
        { "Corridor (Tile Stairwell)", 3, "4×18×4 m のタイルの廊下（L 字、幅 Splay 30 %）。硬い面だけで高域が残り、ピシッと明るく長い。スネアやクラップの FX に。", "A 4 x 18 x 4 m tiled L-shaped corridor (width Splay 30 %). Hard surfaces only, so the highs survive: bright, snappy and long. For snare or clap FX.",
          { { "src", 0 }, { "roomW", 4.0f }, { "roomD", 18.0f }, { "roomH", 4.0f }, { "wall", 0 }, { "floor", 0 }, { "order", 3 }, { "shape", 5 }, { "splay", 30.0f }, { "clutter", 0.0f }, { "srcX", 0.5f }, { "srcY", 0.12f }, { "lisX", 0.45f }, { "lisY", 0.85f }, { "tRoom", 1 }, { "mix", 30 } } },
        { "Empty Room (Clutter 0)", 3, "6×8×3.2 m の何もない部屋（Clutter 0 %）。次の Cluttered Room と聴き比べると、散らかり具合（散乱と家具の吸音）の効果が分かる。", "An empty 6 x 8 x 3.2 m room (Clutter 0 %). Compare it with the next preset, Cluttered Room, to hear what clutter does (scattering and the furnishings' absorption).",
          { { "src", 0 }, { "roomW", 6.0f }, { "roomD", 8.0f }, { "roomH", 3.2f }, { "wall", 2 }, { "floor", 3 }, { "order", 3 }, { "shape", 0 }, { "splay", 0.0f }, { "clutter", 0.0f }, { "srcX", 0.4f }, { "srcY", 0.25f }, { "lisX", 0.62f }, { "lisY", 0.65f }, { "tRoom", 1 }, { "mix", 30 } } },
        { "Cluttered Room (Clutter 80)", 3, "同じ 6×8×3.2 m の部屋に家具・本・布が多い（Clutter 80 %）。反射は密で不規則になり、高域が吸われてRTが短く丸くなる。", "The same 6 x 8 x 3.2 m room, full of furniture, books and fabric (Clutter 80 %). Reflections get denser and less regular, and the absorbed highs make it shorter and rounder.",
          { { "src", 0 }, { "roomW", 6.0f }, { "roomD", 8.0f }, { "roomH", 3.2f }, { "wall", 2 }, { "floor", 3 }, { "order", 3 }, { "shape", 0 }, { "splay", 0.0f }, { "clutter", 80.0f }, { "srcX", 0.4f }, { "srcY", 0.25f }, { "lisX", 0.62f }, { "lisY", 0.65f }, { "tRoom", 1 }, { "mix", 30 } } },

        // ---------------- classic units (ALGO + COLOR, played from a faceplate) ----------------
        // panel values are mapped into the parameters by units()[unit].map; v holds the program's fixed parameters
        { "Steel Plate '50s", 1, "機械式の鉄板プレート。離散的なERがなく、0 ms から密な響きが立ち上がる。明るく、拡散100%に近い。ボーカル・スネアの定番。",
          "A mechanical steel plate. No discrete early reflections: a dense, bright build from 0 ms. The vocal and snare classic.",
          { { "src", 1 }, { "first", 1.0f }, { "length", 40 }, { "taps", 100 }, { "growth", 0.2f }, { "energy", -4 }, { "decay", -6 }, { "diffusion", 95 }, { "width", 120 } }, 0, 0, { 2.5f, 10 } },
        { "Digital Hall '80s", 1, "80年代の高級デジタル機のホール。まばらな初期の反射のあと、後ろに向かって密度と音量が増える（Shape／Spread 型の立ち上がり）。",
          "The hall of an 80s flagship unit. Sparse first reflections, then density and level grow towards the back (a Shape / Spread style build).",
          { { "src", 1 }, { "first", 18 }, { "taps", 40 }, { "energy", -8 }, { "damp", 6500 }, { "width", 130 }, { "bw", 15 }, { "bits", 20 }, { "wander", 30 } }, 1, 0, { 2.0f, 35, 8, 191, 142, 60 } },
        { "Digital Plate '80s", 1, "80年代の高級デジタル機のプレート。短いプリディレイ、高い拡散、なめらかな立ち上がり。帯域15 kHz前後・ゆっくりした揺らぎ。",
          "The plate of an 80s flagship digital unit. Short predelay, high diffusion, smooth build. About 15 kHz bandwidth and slow wander.",
          { { "src", 1 }, { "first", 2.0f }, { "taps", 70 }, { "energy", -6 }, { "damp", 9000 }, { "width", 110 }, { "bw", 15 }, { "bits", 20 }, { "wander", 25 } }, 1, 1, { 1.6f, 35, 0, 99, 51, 85 } },
        { "Digital Room '80s", 1, "同じ時代のルーム。初回反射 6 ms、50 ms で終わるまとまった響き。ドラムやギターを「部屋に置く」。",
          "The same era's room. First reflection at 6 ms, a compact 50 ms field. Puts drums or guitars in a room.",
          { { "src", 1 }, { "first", 6 }, { "taps", 36 }, { "energy", -7 }, { "damp", 8000 }, { "bw", 15 }, { "bits", 20 }, { "wander", 20 } }, 1, 2, { 0.8f, 35, 0, 85, 40, 50 } },
        { "Digital Chamber '80s", 1, "エコーチェンバーをデジタルで作ったもの。中くらいの密度と、ゆるい減衰。ボーカルに馴染む。",
          "An echo chamber made in digital. Medium density and a gentle decay. Sits well behind vocals.",
          { { "src", 1 }, { "first", 9 }, { "taps", 50 }, { "energy", -7 }, { "damp", 7000 }, { "width", 110 }, { "bw", 15 }, { "bits", 20 }, { "wander", 20 } }, 1, 3, { 1.4f, 35, 0, 113, 74, 70 } },
        { "Ambience '80s (Non-linear)", 1, "80年代初頭の英国製デジタル機の「アンビエンス」。20〜50 ms に密な反射が平らに並び、テールなしで音を太く・近く広げる。",
          "The 'ambience' of an early-80s British digital unit. A flat, dense cluster at 20-50 ms with no tail: thicker and wider, still close.",
          { { "src", 1 }, { "first", 2 }, { "taps", 80 }, { "growth", 0.0f }, { "energy", -2 }, { "decay", 0 }, { "diffusion", 75 }, { "bw", 18 }, { "bits", 16 } }, 2, 0, { 0, 45, 10000, 130 } },
        { "Gated '80s", 1, "ゲートリバーブ的なノンリニア。平らなまま 110 ms 続き、急に切れる。80年代スネアの音。",
          "Gated, non-linear. Flat for 110 ms, then cut. The 80s snare.",
          { { "src", 1 }, { "first", 3 }, { "taps", 70 }, { "growth", 0.0f }, { "energy", -1 }, { "decay", -2 }, { "diffusion", 70 }, { "bw", 18 }, { "bits", 16 } }, 2, 1, { 0, 110, 9000, 120 } },
        { "Reverse '80s", 1, "後ろほど大きくなるリバース型。Envelope をプラスにすると反射が「吸い込まれる」ように立ち上がる。",
          "Reverse type: louder towards the end. A positive Envelope makes the reflections swell in.",
          { { "src", 1 }, { "first", 10 }, { "taps", 60 }, { "growth", 0.4f }, { "energy", -5 }, { "decay", 18 }, { "diffusion", 60 }, { "bw", 16 }, { "bits", 16 } }, 2, 2, { 0, 180, 9000, 120 } },
        { "Room Simulator '80s", 1, "80年代のドイツ製ルームシミュレーター風。非常に密で自然な初期反射（Growth 2 = 実際の部屋の増え方）、帯域はやや狭い。",
          "In the manner of an 80s German room simulator. Very dense, natural early field (Growth 2 = how a real room fills up), slightly narrow band.",
          { { "src", 1 }, { "length", 70 }, { "taps", 90 }, { "growth", 2.0f }, { "decay", -16 }, { "diffusion", 55 }, { "bw", 16 }, { "bits", 18 } }, 3, 0, { 3.3f, 4, 0, 1.2f, 0.39f } },
        { "First-gen Digital '70s", 1, "70年代後半の第一世代デジタル。反射が少なく粒が見え、帯域 8 kHz・12 bit のざらつき。",
          "Late-70s first-generation digital. Few reflections you can count, 8 kHz bandwidth and 12-bit grain.",
          { { "src", 1 }, { "first", 12 }, { "length", 90 }, { "taps", 18 }, { "growth", 0.8f }, { "energy", -7 }, { "decay", -10 }, { "diffusion", 30 }, { "width", 90 }, { "bw", 8 }, { "bits", 12 }, { "wander", 10 } }, 4, 0, { 2.0f, 0, 0.4f } },
        { "Early Digital Hall '70s (Chorused)", 1, "70年代末のデジタルホール。帯域 10 kHz、ゆっくりした揺らぎ（コーラス）で反射がにじむ。暗く甘い。",
          "A late-70s digital hall. 10 kHz bandwidth and slow chorusing that smears the reflections. Dark and sweet.",
          { { "src", 1 }, { "predelay", 5 }, { "taps", 30 }, { "growth", 1.0f }, { "energy", -8 }, { "decay", -8 }, { "width", 130 }, { "bw", 10 }, { "bits", 16 } }, 5, 0, { 2.5f, 0.5f, 33, 3.5f, 3.6f } },
        { "Multi-FX ER '80s — Hall", 1, "80年代後半の普及型マルチエフェクターのERパターン（Hall）。反射は少なく規則的、拡散なし。ERを「パターン」で選ぶ時代の音。",
          "An ER pattern (Hall) of a late-80s budget multi-effects unit. Few, regular reflections, no diffusion. Picking the ER as a 'pattern'.",
          { { "src", 1 }, { "growth", 0.5f }, { "energy", -9 }, { "damp", 9000 }, { "width", 120 }, { "bw", 12 }, { "bits", 16 } }, 6, 0, { 10, 5.5f, 0, 1.5f, 19 } },
        { "Multi-FX ER '80s — Random", 1, "同じ機種の Random パターン。間隔も音量もばらばらで、ほぼ平ら。ディレイを並べたような人工的な響き。",
          "The same unit's Random pattern. Irregular spacing and level, nearly flat. An obviously artificial field.",
          { { "src", 1 }, { "growth", 0.0f }, { "energy", -8 }, { "damp", 10000 }, { "width", 140 }, { "bw", 12 }, { "bits", 16 }, { "vari", 7 } }, 6, 1, { 12.2f, 8, 0, 1, 16 } },
        { "Multi-FX ER '80s — Reverse", 1, "同じ機種の Reverse パターン。反射が後ろほど大きくなる。",
          "The same unit's Reverse pattern: the reflections grow towards the end.",
          { { "src", 1 }, { "growth", 0.3f }, { "energy", -7 }, { "damp", 9000 }, { "width", 120 }, { "bw", 12 }, { "bits", 16 }, { "vari", 3 } }, 6, 2, { 10, 7, 0, 2, 19 } },
        { "Multi-FX ER '80s — Plate", 1, "同じ機種の Plate パターン。0 ms 近くから反射が詰まり、短く明るい。",
          "The same unit's Plate pattern: packed reflections from almost 0 ms, short and bright.",
          { { "src", 1 }, { "growth", 0.2f }, { "energy", -6 }, { "damp", 12000 }, { "width", 120 }, { "bw", 12 }, { "bits", 16 } }, 6, 3, { 3, 4, 0, 6, 19 } },
        { "Stereo Room '82", 1, "80年代初頭の米国製シグナルプロセッサーの代表的なルーム。聴く位置（Position）で直接音との距離が変わり、ER の早さと量が動く。密で滑らか、高域はやや丸い。",
          "The signature room of an early-80s American signal processor. Position moves you away from the source: the ER comes sooner and louder. Dense and smooth with slightly rounded highs.",
          { { "src", 1 }, { "length", 70 }, { "taps", 50 }, { "growth", 1.3f }, { "decay", -10 }, { "width", 140 }, { "bw", 16 }, { "bits", 18 } }, 7, 0, { 0, 1.5f, 50, 0.5f, 70 } },
        { "Room '82", 1, "同じ機種のルーム。短くまとまった ER で、ドラムやギターに。", "The same unit's room: a short, compact ER for drums or guitars.",
          { { "src", 1 }, { "length", 45 }, { "taps", 40 }, { "growth", 1.5f }, { "decay", -14 }, { "width", 110 }, { "bw", 16 }, { "bits", 18 } }, 7, 1, { 0, 0.8f, 40, 0.45f, 60 } },
        { "High-Density Plate '82", 1, "同じ機種の高密度プレート。0 ms から非常に密に立ち上がる。", "The same unit's high-density plate: very dense from 0 ms.",
          { { "src", 1 }, { "length", 50 }, { "taps", 110 }, { "growth", 0.3f }, { "decay", -6 }, { "width", 120 }, { "bw", 16 }, { "bits", 18 } }, 7, 2, { 0, 1.8f, 50, 0.75f, 95 } },
        { "Modern Hall '08", 1, "2000年代の高級リバーブのホール。実際のホールのように自然で非常に密な ER、帯域の制限なし、ごくわずかな揺らぎ。",
          "The hall of a 2000s high-end reverb. A natural, very dense ER like a real hall, full bandwidth, the faintest movement.",
          { { "src", 1 }, { "first", 12 }, { "length", 120 }, { "growth", 1.8f }, { "decay", -12 }, { "damp", 9000 }, { "width", 125 }, { "wander", 15 } }, 8, 0, { 2.8f, 10, 50, 60, 0, 1, 12000, 40 } },
        { "Modern Room '08", 1, "同じ機種のルーム。短く自然で、ドライな素材を自然に部屋へ置く。", "The same unit's room: short and natural, puts dry sources in a room without colour.",
          { { "src", 1 }, { "first", 5 }, { "length", 55 }, { "growth", 1.6f }, { "decay", -16 }, { "damp", 10000 }, { "width", 110 }, { "wander", 10 } }, 8, 1, { 0.9f, 0, 45, 55, 0, 2, 14000, 40 } },
        { "Modern Plate '08", 1, "同じ機種のプレート。密で明るく、ボーカルに。", "The same unit's plate: dense and bright, for vocals.",
          { { "src", 1 }, { "first", 1 }, { "length", 45 }, { "growth", 0.4f }, { "decay", -7 }, { "damp", 12000 }, { "width", 120 }, { "wander", 10 } }, 8, 2, { 2.0f, 0, 40, 80, 0, 3, 15000, 40 } },
        { "Modern Chamber '08", 1, "同じ機種のチェンバー。中くらいの大きさで、ゆるやかに減衰。", "The same unit's chamber: medium size, gentle decay.",
          { { "src", 1 }, { "first", 7 }, { "length", 85 }, { "growth", 1.4f }, { "decay", -9 }, { "damp", 8500 }, { "width", 115 }, { "wander", 12 } }, 8, 3, { 1.6f, 5, 50, 60, 0, 4, 11000, 40 } },
        // ---------------- 0.7.3: IR fits of a wider set of hardware (plate / hall / chamber / room / ambient) ----------------
        { "Steel Plate 2 s '50s (IR fit)", 3, "機械式の鉄板プレートの IR から合わせたもの。テール 2.2 s。誤差：T30 7.4 %・EDT 8.4 %・C80 0.2 dB。",
          "Fitted to an IR of a mechanical steel plate. Tail 2.2 s. Error: T30 7.4 %, EDT 8.4 %, C80 0.2 dB.",
          { { "src", 1 }, { "first", 2.109f }, { "length", 22.45f }, { "taps", 12 }, { "growth", 0.5552f }, { "energy", 10.4f }, { "decay", 0.6186f }, { "damp", 10905.7f }, { "predelay", 0.5089f }, { "width", 142.4f }, { "level", -11.63f }, { "tDecay", 2.201f }, { "tSize", 23.69f }, { "tPre", 0 }, { "tDiff", 86.23f }, { "tModD", 34.34f }, { "tLvl", -6.468f }, { "rtF1", 100.0f }, { "rtM1", 0.2281f }, { "rtM2", 0.6341f }, { "rtM3", 1.266f }, { "rtM4", 0.976f }, { "rtF5", 10000.0f }, { "rtM5", 0.3334f }, { "outW", 87.25f }, { "eqLP", 9415.0f }, { "eqG1", -10.52f }, { "eqG2", -3.257f }, { "eqG3", 6.581f }, { "eqG4", 1.481f }, { "eqG5", 7.093f }, { "eqG6", 5.283f }, { "tSpread", 3.921f }, { "tShape", -21.36f }, { "tFill", 21.65f }, { "tSpreadHi", 61.76f }, { "mix", 35 } } },
        { "First-gen Hall '77 (IR fit)", 3, "第一世代デジタル機のホールの IR から合わせたもの。テール 2.5 s。誤差：T30 4.5 %・EDT 5.0 %・C80 0.3 dB。",
          "Fitted to an IR of a first-generation digital unit's hall. Tail 2.5 s. Error: T30 4.5 %, EDT 5.0 %, C80 0.3 dB.",
          { { "src", 1 }, { "first", 5.297f }, { "length", 38.6f }, { "taps", 120 }, { "growth", 0.2f }, { "energy", 6.287f }, { "decay", -4.663f }, { "damp", 8787.0f }, { "width", 150 }, { "level", -2.371f }, { "tDecay", 2.502f }, { "tSize", 92.47f }, { "tPre", 0 }, { "tDiff", 87.29f }, { "tModD", 36.49f }, { "tLvl", 2.044f }, { "rtF1", 100.0f }, { "rtM1", 1.578f }, { "rtM2", 1.452f }, { "rtM3", 1.038f }, { "rtM4", 0.8301f }, { "rtF5", 10000.0f }, { "rtM5", 0.1615f }, { "outW", 100.3f }, { "eqLP", 10224.6f }, { "eqG1", 0.8252f }, { "eqG2", 0.7179f }, { "eqG3", 1.404f }, { "eqG4", 4.594f }, { "eqG5", 2.227f }, { "eqG6", -7.492f }, { "tSpread", 54.22f }, { "tShape", -29.77f }, { "tFill", 40.29f }, { "tSpreadHi", 44.58f }, { "mix", 35 } } },
        { "Vocal Plate '10s (IR fit)", 3, "2010年代の高級機のプレートの IR から合わせたもの。テール 1.5 s。誤差：T30 6.6 %・EDT 10.8 %・C80 0.5 dB。",
          "Fitted to an IR of a 2010s high-end unit's plate. Tail 1.5 s. Error: T30 6.6 %, EDT 10.8 %, C80 0.5 dB.",
          { { "src", 1 }, { "first", 2.753f }, { "length", 49.9f }, { "taps", 48 }, { "growth", 0.5528f }, { "energy", 12 }, { "decay", -8.212f }, { "damp", 12878.3f }, { "predelay", 4.495f }, { "width", 147 }, { "level", -6.607f }, { "tDecay", 1.534f }, { "tSize", 30.77f }, { "tPre", 0.4121f }, { "tDiff", 77.27f }, { "tLvl", -0.07634f }, { "rtF1", 100.0f }, { "rtM1", 0.8524f }, { "rtM2", 0.8999f }, { "rtM3", 1.01f }, { "rtM4", 1.2f }, { "rtF5", 10000.0f }, { "rtM5", 0.15f }, { "eqLP", 17919.1f }, { "eqG1", 3.014f }, { "eqG2", 0.4951f }, { "eqG3", 2.45f }, { "eqG4", 3.443f }, { "eqG5", -4.83f }, { "eqG6", -3.914f }, { "tSpread", 38.81f }, { "tShape", -5.395f }, { "tBassW", 98 }, { "tFill", 15.7f }, { "tSpreadHi", 47.58f }, { "mix", 35 } } },
        { "Snare Plate '10s (IR fit)", 3, "2010年代の高級機のスネア用プレートの IR から合わせたもの。テール 1.3 s。誤差：T30 4.8 %・EDT 16.7 %・C80 1.3 dB。",
          "Fitted to an IR of a 2010s high-end unit's snare plate. Tail 1.3 s. Error: T30 4.8 %, EDT 16.7 %, C80 1.3 dB.",
          { { "src", 1 }, { "first", 1.144f }, { "length", 34.82f }, { "taps", 29 }, { "growth", 0.5137f }, { "energy", 11.16f }, { "decay", -15.18f }, { "damp", 4788.0f }, { "width", 147 }, { "level", -5.639f }, { "tDecay", 1.317f }, { "tSize", 29.95f }, { "tPre", 16.78f }, { "tDiff", 86.54f }, { "tLvl", -1.703f }, { "rtF1", 100.0f }, { "rtM1", 1.24f }, { "rtM2", 0.9597f }, { "rtM3", 0.9173f }, { "rtM4", 0.9657f }, { "rtF5", 10000.0f }, { "rtM5", 0.4209f }, { "eqLP", 10488.5f }, { "eqG1", 4.392f }, { "eqG2", 2.749f }, { "eqG3", 4.145f }, { "eqG4", 7.434f }, { "eqG5", -4.764f }, { "eqG6", -7.583f }, { "tSpread", 38.24f }, { "tShape", -3.358f }, { "tBassW", 98 }, { "tFill", 33.98f }, { "tSpreadHi", 40.56f }, { "mix", 35 } } },
        { "Master Plate '00s (IR fit)", 3, "2000年代のデジタル機のプレートの IR から合わせたもの。テール 1.0 s。誤差：T30 2.9 %・EDT 4.8 %・C80 0.4 dB。",
          "Fitted to an IR of a 2000s digital unit's plate. Tail 1.0 s. Error: T30 2.9 %, EDT 4.8 %, C80 0.4 dB.",
          { { "src", 1 }, { "first", 1.749f }, { "length", 79.49f }, { "taps", 60 }, { "growth", 0.05f }, { "energy", 9.953f }, { "decay", -9.999f }, { "damp", 16000 }, { "predelay", 5.17f }, { "width", 150 }, { "level", -10.73f }, { "tDecay", 0.9775f }, { "tSize", 159.1f }, { "tPre", 0.8803f }, { "tDiff", 88.67f }, { "tModD", 53.82f }, { "tLvl", -1.766f }, { "rtF1", 100.0f }, { "rtM1", 0.5001f }, { "rtM2", 0.7994f }, { "rtM3", 0.9464f }, { "rtM4", 1.251f }, { "rtF5", 10000.0f }, { "rtM5", 0.851f }, { "outW", 105.3f }, { "eqLP", 15288.5f }, { "eqG1", -12 }, { "eqG2", -0.4637f }, { "eqG3", 1.809f }, { "eqG4", 5.782f }, { "eqG5", 11.06f }, { "eqG6", 4.632f }, { "tSpread", 53.26f }, { "tShape", -18.01f }, { "tBassW", 73.09f }, { "tFill", 34.43f }, { "tSpreadHi", 48.89f }, { "mix", 35 } } },
        { "Concert Hall '10s (IR fit)", 3, "2010年代の高級機のコンサートホールの IR から合わせたもの。テール 2.4 s。誤差：T30 2.7 %・EDT 12.4 %・C80 0.2 dB。",
          "Fitted to an IR of a 2010s high-end unit's concert hall. Tail 2.4 s. Error: T30 2.7 %, EDT 12.4 %, C80 0.2 dB.",
          { { "src", 1 }, { "first", 1.092f }, { "length", 76.88f }, { "taps", 71 }, { "growth", 0.176f }, { "energy", 11.16f }, { "decay", -10.09f }, { "damp", 16000 }, { "predelay", 8.746f }, { "width", 143.6f }, { "level", -1.712f }, { "tDecay", 2.372f }, { "tSize", 108.9f }, { "tPre", 27.29f }, { "tDiff", 86.21f }, { "tModD", 22.25f }, { "tLvl", 3.042f }, { "rtF1", 100.0f }, { "rtM1", 0.9701f }, { "rtM2", 1.039f }, { "rtM3", 0.9907f }, { "rtM4", 0.9005f }, { "rtF5", 10000.0f }, { "rtM5", 0.2925f }, { "outW", 94 }, { "eqLP", 16964.0f }, { "eqG1", 6.866f }, { "eqG2", -0.5252f }, { "eqG3", 1.6f }, { "eqG4", 2.695f }, { "eqG5", -6.495f }, { "eqG6", -4.871f }, { "tSpread", 70.03f }, { "tShape", -2.365f }, { "tBassW", 95.75f }, { "tFill", 22.6f }, { "tSpreadHi", 35.03f }, { "mix", 35 } } },
        { "Classic Hall '10s (IR fit)", 3, "2010年代の高級機の古典的なホールの IR から合わせたもの。テール 2.3 s。誤差：T30 2.3 %・EDT 15.5 %・C80 3.4 dB。",
          "Fitted to an IR of a 2010s high-end unit's classic hall. Tail 2.3 s. Error: T30 2.3 %, EDT 15.5 %, C80 3.4 dB.",
          { { "src", 1 }, { "first", 3.302f }, { "length", 57.78f }, { "taps", 51 }, { "growth", 0.05f }, { "energy", 11.16f }, { "decay", -14.19f }, { "damp", 16000 }, { "predelay", 6.258f }, { "width", 150 }, { "level", -3.412f }, { "tDecay", 2.257f }, { "tSize", 51.08f }, { "tPre", 24.52f }, { "tDiff", 88.4f }, { "tModD", 20.64f }, { "tLvl", 0.07842f }, { "rtF1", 100.0f }, { "rtM1", 0.946f }, { "rtM2", 1.097f }, { "rtM3", 0.9658f }, { "rtM4", 0.7575f }, { "rtF5", 10000.0f }, { "rtM5", 0.1615f }, { "outW", 98.4f }, { "eqG1", 0.6787f }, { "eqG2", 2.189f }, { "eqG3", 0.2312f }, { "eqG4", 2.669f }, { "eqG5", -3.904f }, { "eqG6", -5.036f }, { "tSpread", 6 }, { "tShape", 31.12f }, { "tFill", 5.215f }, { "tSpreadHi", 65.63f }, { "mix", 35 } } },
        { "Small Hall '10s (IR fit)", 3, "2010年代の高級機の小さめのホールの IR から合わせたもの。テール 1.6 s。誤差：T30 4.1 %・EDT 8.8 %・C80 1.5 dB。",
          "Fitted to an IR of a 2010s high-end unit's small hall. Tail 1.6 s. Error: T30 4.1 %, EDT 8.8 %, C80 1.5 dB.",
          { { "src", 1 }, { "first", 1.529f }, { "length", 30.95f }, { "taps", 35 }, { "growth", 1.205f }, { "energy", 12 }, { "decay", -6.424f }, { "damp", 9833.0f }, { "predelay", 1.857f }, { "width", 147 }, { "level", -10.2f }, { "tDecay", 1.575f }, { "tSize", 36.2f }, { "tPre", 12.63f }, { "tDiff", 92.97f }, { "tLvl", -0.513f }, { "rtF1", 100.0f }, { "rtM1", 0.7928f }, { "rtM2", 1.007f }, { "rtM3", 0.9207f }, { "rtM4", 0.8949f }, { "rtF5", 10000.0f }, { "rtM5", 0.15f }, { "eqLP", 10546.1f }, { "eqG1", 5.87f }, { "eqG2", 3.583f }, { "eqG3", 6.333f }, { "eqG4", 4.637f }, { "eqG5", -2.111f }, { "eqG6", -9.397f }, { "tSpread", 78.25f }, { "tShape", -20.34f }, { "tBassW", 98 }, { "tFill", 13.67f }, { "tSpreadHi", 61.95f }, { "mix", 35 } } },
        { "Concert Hall '00s (IR fit)", 3, "2000年代のルームシミュレーター系の機器のコンサートホールの IR から合わせたもの。テール 2.1 s。誤差：T30 3.4 %・EDT 5.8 %・C80 0.4 dB。",
          "Fitted to an IR of a 2000s room-simulator-type unit's concert hall. Tail 2.1 s. Error: T30 3.4 %, EDT 5.8 %, C80 0.4 dB.",
          { { "src", 1 }, { "first", 11.89f }, { "length", 33.41f }, { "taps", 58 }, { "growth", 0.1934f }, { "energy", 4.163f }, { "decay", -29.3f }, { "damp", 6360.0f }, { "predelay", 3.509f }, { "width", 118.0f }, { "level", -3.935f }, { "tDecay", 2.052f }, { "tSize", 59.0f }, { "tPre", 2.894f }, { "tDiff", 92.08f }, { "tLvl", 3.809f }, { "rtF1", 100.0f }, { "rtM1", 1.219f }, { "rtM2", 0.9139f }, { "rtM3", 0.9479f }, { "rtM4", 0.6707f }, { "rtF5", 10000.0f }, { "rtM5", 0.1615f }, { "eqLP", 10387.1f }, { "eqG1", -0.659f }, { "eqG2", 1.165f }, { "eqG3", 1.131f }, { "eqG4", 2.606f }, { "eqG5", 1.029f }, { "eqG6", -3.788f }, { "tSpread", 119.9f }, { "tShape", -15.03f }, { "tBassW", 98 }, { "tFill", 17.56f }, { "tSpreadHi", 47.05f }, { "mix", 35 } } },
        { "Large Hall '80s (IR fit)", 3, "80年代後半のデジタル機の大ホールの IR から合わせたもの。テール 2.7 s。誤差：T30 5.9 %・EDT 14.1 %・C80 1.8 dB。",
          "Fitted to an IR of a late-80s digital unit's large hall. Tail 2.7 s. Error: T30 5.9 %, EDT 14.1 %, C80 1.8 dB.",
          { { "src", 1 }, { "first", 3.408f }, { "length", 13.83f }, { "taps", 4 }, { "growth", 0.9437f }, { "energy", -17.7f }, { "decay", -19.76f }, { "damp", 1500 }, { "predelay", 2.4f }, { "width", 3 }, { "level", 5.405f }, { "tDecay", 2.735f }, { "tSize", 28.2f }, { "tPre", 4 }, { "tDiff", 65.43f }, { "tLvl", -1.606f }, { "rtF1", 100.0f }, { "rtM1", 0.746f }, { "rtM2", 1.202f }, { "rtM3", 0.9442f }, { "rtM4", 0.7052f }, { "rtF5", 10000.0f }, { "rtM5", 0.3126f }, { "eqLP", 12928.0f }, { "eqG1", 3.038f }, { "eqG2", 0.01435f }, { "eqG3", -0.1668f }, { "eqG4", 2.044f }, { "eqG5", 1.351f }, { "eqG6", -6.237f }, { "tSpread", 6 }, { "tShape", 43.02f }, { "tBassW", 98 }, { "tFill", 9.661f }, { "tSpreadHi", 122.3f }, { "mix", 35 } } },
        { "Studio Hall '00s (IR fit)", 3, "2000年代のデジタル機のホールの IR から合わせたもの。テール 1.4 s。誤差：T30 2.1 %・EDT 4.3 %・C80 0.1 dB。",
          "Fitted to an IR of a 2000s digital unit's hall. Tail 1.4 s. Error: T30 2.1 %, EDT 4.3 %, C80 0.1 dB.",
          { { "src", 1 }, { "first", 1.471f }, { "length", 58.18f }, { "taps", 57 }, { "growth", 0.8877f }, { "energy", 12 }, { "decay", -8.571f }, { "damp", 12946.6f }, { "predelay", 0.8911f }, { "width", 150 }, { "level", -8.026f }, { "tDecay", 1.358f }, { "tSize", 48.24f }, { "tPre", 24.16f }, { "tDiff", 86.73f }, { "tModD", 18.71f }, { "tLvl", -0.9466f }, { "rtF1", 100.0f }, { "rtM1", 0.1923f }, { "rtM2", 0.9068f }, { "rtM3", 1.158f }, { "rtM4", 0.8899f }, { "rtF5", 10000.0f }, { "rtM5", 1.174f }, { "outW", 64.42f }, { "eqG1", -5.565f }, { "eqG2", -3.261f }, { "eqG3", -1.422f }, { "eqG4", 5.649f }, { "eqG5", 3.252f }, { "eqG6", 0.4798f }, { "tSpread", 0.5467f }, { "tShape", -0.9997f }, { "tBassW", 88.1f }, { "tFill", 38.35f }, { "tSpreadHi", 40.7f }, { "mix", 35 } } },
        { "Jazz Hall '00s (IR fit)", 3, "2000年代のデジタル機のジャズ向けホールの IR から合わせたもの。テール 1.0 s。誤差：T30 3.7 %・EDT 9.3 %・C80 0.5 dB。",
          "Fitted to an IR of a 2000s digital unit's jazz hall. Tail 1.0 s. Error: T30 3.7 %, EDT 9.3 %, C80 0.5 dB.",
          { { "src", 1 }, { "first", 3.369f }, { "length", 80.4f }, { "taps", 106 }, { "growth", 1.217f }, { "energy", 11.16f }, { "decay", -14.45f }, { "damp", 16000 }, { "predelay", 3.363f }, { "width", 150 }, { "level", -3.403f }, { "tDecay", 1.008f }, { "tSize", 41.78f }, { "tPre", 24.46f }, { "tDiff", 88.69f }, { "tModD", 33.17f }, { "tLvl", 1.851f }, { "rtF1", 100.0f }, { "rtM1", 1.016f }, { "rtM2", 0.88f }, { "rtM3", 0.9345f }, { "rtM4", 0.9578f }, { "rtF5", 10000.0f }, { "rtM5", 0.6367f }, { "outW", 93.74f }, { "eqLP", 15254.7f }, { "eqG1", 1.07f }, { "eqG2", 0.4047f }, { "eqG3", 0.6144f }, { "eqG4", 3.218f }, { "eqG5", -1.837f }, { "eqG6", -4.368f }, { "tSpread", 35.31f }, { "tShape", 2.919f }, { "tBassW", 88.58f }, { "tFill", 30.72f }, { "tSpreadHi", 38.9f }, { "mix", 35 } } },
        { "Empty Hall '00s (IR fit)", 3, "2000年代のデジタル機の人のいないホールの IR から合わせたもの。テール 1.4 s。誤差：T30 5.1 %・EDT 12.5 %・C80 0.7 dB。",
          "Fitted to an IR of a 2000s digital unit's empty hall. Tail 1.4 s. Error: T30 5.1 %, EDT 12.5 %, C80 0.7 dB.",
          { { "src", 1 }, { "first", 2.032f }, { "length", 82.12f }, { "taps", 66 }, { "growth", 0.3133f }, { "energy", 12 }, { "decay", 0.7602f }, { "damp", 9793.0f }, { "width", 147 }, { "level", -6.227f }, { "tDecay", 1.358f }, { "tSize", 51.74f }, { "tPre", 0 }, { "tDiff", 92.37f }, { "tLvl", 3.823f }, { "rtF1", 100.0f }, { "rtM1", 0.86f }, { "rtM2", 0.9344f }, { "rtM3", 1.082f }, { "rtM4", 0.866f }, { "rtF5", 10000.0f }, { "rtM5", 0.2707f }, { "eqLP", 18566.4f }, { "eqG1", 2.588f }, { "eqG2", 4.861f }, { "eqG3", 4.681f }, { "eqG4", 1.419f }, { "eqG5", -6.936f }, { "eqG6", -7.88f }, { "tSpread", 122.1f }, { "tShape", -17.71f }, { "tBassW", 98 }, { "tFill", 15.87f }, { "tSpreadHi", 57.39f }, { "mix", 35 } } },
        { "Church '10s (IR fit)", 3, "2010年代の高級機の教会の IR から合わせたもの。テール 4.9 s。誤差：T30 6.9 %・EDT 12.5 %・C80 0.8 dB。",
          "Fitted to an IR of a 2010s high-end unit's church. Tail 4.9 s. Error: T30 6.9 %, EDT 12.5 %, C80 0.8 dB.",
          { { "src", 1 }, { "first", 8.099f }, { "length", 57.5f }, { "taps", 63 }, { "growth", 0.7494f }, { "energy", 12 }, { "decay", -8.726f }, { "damp", 10885.6f }, { "width", 147 }, { "level", -7.955f }, { "tDecay", 4.897f }, { "tSize", 64.87f }, { "tPre", 19.86f }, { "tDiff", 83.83f }, { "tLvl", -4.091f }, { "rtF1", 100.0f }, { "rtM1", 1.098f }, { "rtM2", 1.111f }, { "rtM3", 1.118f }, { "rtM4", 0.5405f }, { "rtF5", 10000.0f }, { "rtM5", 0.1615f }, { "eqLP", 16099.4f }, { "eqG1", 6.101f }, { "eqG2", 4.835f }, { "eqG3", 2.277f }, { "eqG4", 7.901f }, { "eqG5", -3.998f }, { "eqG6", -10.95f }, { "tShape", 19.95f }, { "tBassW", 98 }, { "tFill", 11.44f }, { "tSpreadHi", 57.93f }, { "mix", 35 } } },
        { "Vocal Chamber '10s (IR fit)", 3, "2010年代の高級機のボーカル用チェンバーの IR から合わせたもの。テール 1.4 s。誤差：T30 2.0 %・EDT 7.4 %・C80 0.6 dB。",
          "Fitted to an IR of a 2010s high-end unit's vocal chamber. Tail 1.4 s. Error: T30 2.0 %, EDT 7.4 %, C80 0.6 dB.",
          { { "src", 1 }, { "first", 3.274f }, { "length", 61.48f }, { "taps", 44 }, { "growth", 0.4441f }, { "energy", 12 }, { "decay", -16.22f }, { "damp", 12917.0f }, { "predelay", 2.4f }, { "width", 150 }, { "level", -5.646f }, { "tDecay", 1.409f }, { "tSize", 45.17f }, { "tPre", 28.97f }, { "tDiff", 80.13f }, { "tModD", 34.83f }, { "tLvl", -0.3909f }, { "rtF1", 100.0f }, { "rtM1", 1.012f }, { "rtM2", 0.9141f }, { "rtM3", 1.119f }, { "rtM4", 1.071f }, { "rtF5", 10000.0f }, { "rtM5", 0.6465f }, { "outW", 95.78f }, { "eqLP", 11217.2f }, { "eqG1", 4.802f }, { "eqG2", 1.039f }, { "eqG3", 1.043f }, { "eqG4", 7.049f }, { "eqG5", -2.694f }, { "eqG6", -6.877f }, { "tSpread", 34.72f }, { "tShape", 9.795f }, { "tBassW", 76.84f }, { "tFill", 33.28f }, { "tSpreadHi", 41.27f }, { "mix", 35 } } },
        { "Large Chamber '10s (IR fit)", 3, "2010年代の高級機の大きなチェンバーの IR から合わせたもの。テール 1.5 s。誤差：T30 5.2 %・EDT 12.4 %・C80 1.4 dB。",
          "Fitted to an IR of a 2010s high-end unit's large chamber. Tail 1.5 s. Error: T30 5.2 %, EDT 12.4 %, C80 1.4 dB.",
          { { "src", 1 }, { "first", 1.422f }, { "length", 71.49f }, { "taps", 27 }, { "growth", 0.5771f }, { "energy", 12 }, { "decay", -20.96f }, { "damp", 4718.0f }, { "predelay", 2.153f }, { "width", 147 }, { "level", -6.467f }, { "tDecay", 1.522f }, { "tSize", 36.72f }, { "tPre", 40.67f }, { "tDiff", 92.15f }, { "tLvl", -1.504f }, { "rtF1", 100.0f }, { "rtM1", 1.126f }, { "rtM2", 0.8761f }, { "rtM3", 0.9898f }, { "rtM4", 0.906f }, { "rtF5", 10000.0f }, { "rtM5", 0.15f }, { "eqLP", 11491.3f }, { "eqG1", 6.791f }, { "eqG2", 4.108f }, { "eqG3", 2.881f }, { "eqG4", 6.027f }, { "eqG5", -0.7148f }, { "eqG6", -9.531f }, { "tSpread", 35.07f }, { "tShape", 9.827f }, { "tBassW", 98 }, { "tFill", 33.27f }, { "tSpreadHi", 44.2f }, { "mix", 35 } } },
        { "Echo Chamber '00s (IR fit)", 3, "エコーチェンバーの IR から合わせたもの。テール 0.9 s。誤差：T30 3.2 %・EDT 8.5 %・C80 0.2 dB。",
          "Fitted to an IR of an echo chamber. Tail 0.9 s. Error: T30 3.2 %, EDT 8.5 %, C80 0.2 dB.",
          { { "src", 1 }, { "first", 4.53f }, { "length", 63.07f }, { "taps", 47 }, { "growth", 0.1743f }, { "energy", 12 }, { "decay", -3.565f }, { "damp", 15652.9f }, { "predelay", 2.4f }, { "width", 57.01f }, { "level", -3.458f }, { "tDecay", 0.8726f }, { "tSize", 62.02f }, { "tPre", 7.665f }, { "tDiff", 89.63f }, { "tModD", 12.61f }, { "tLvl", 3.068f }, { "rtF1", 100.0f }, { "rtM1", 0.2123f }, { "rtM2", 0.8911f }, { "rtM3", 1.235f }, { "rtM4", 0.9959f }, { "rtF5", 10000.0f }, { "rtM5", 0.7641f }, { "outW", 55.9f }, { "eqLP", 19788.9f }, { "eqG1", -4.388f }, { "eqG2", -3.056f }, { "eqG3", 1.738f }, { "eqG4", 3.101f }, { "eqG5", 2.659f }, { "eqG6", -1.625f }, { "tSpread", 6 }, { "tShape", 33.58f }, { "tBassW", 87.9f }, { "tFill", 3.987f }, { "tSpreadHi", 67.51f }, { "mix", 35 } } },
        { "Theater '00s (IR fit)", 3, "2000年代のルームシミュレーター系の機器のシアターの IR から合わせたもの。テール 1.0 s。誤差：T30 1.8 %・EDT 3.9 %・C80 0.3 dB。",
          "Fitted to an IR of a 2000s room-simulator-type unit's theater. Tail 1.0 s. Error: T30 1.8 %, EDT 3.9 %, C80 0.3 dB.",
          { { "src", 1 }, { "first", 13.7f }, { "length", 66.57f }, { "taps", 49 }, { "growth", 0.2644f }, { "energy", 4.105f }, { "decay", -29.48f }, { "damp", 5592.0f }, { "predelay", 1.212f }, { "width", 147 }, { "level", -4.036f }, { "tDecay", 1.027f }, { "tSize", 32.71f }, { "tPre", 10.68f }, { "tDiff", 89.43f }, { "tLvl", 2.587f }, { "rtF1", 100.0f }, { "rtM1", 2.315f }, { "rtM2", 1.127f }, { "rtM3", 0.9339f }, { "rtM4", 0.8978f }, { "rtF5", 10000.0f }, { "rtM5", 0.1729f }, { "eqLP", 13111.7f }, { "eqG1", 2.843f }, { "eqG2", -1.196f }, { "eqG3", 1.061f }, { "eqG4", 0.5367f }, { "eqG5", 3.561f }, { "eqG6", -4.854f }, { "tSpread", 124.0f }, { "tShape", -47.93f }, { "tBassW", 98 }, { "tFill", 20.13f }, { "tSpreadHi", 53.21f }, { "mix", 35 } } },
        { "Ambience '80s (IR fit)", 3, "80年代の英国製デジタル機のアンビエンスの IR から合わせたもの。テール 2.1 s。誤差：T30 2.0 %・EDT 5.2 %・C80 0.7 dB。",
          "Fitted to an IR of an 80s British digital unit's ambience. Tail 2.1 s. Error: T30 2.0 %, EDT 5.2 %, C80 0.7 dB.",
          { { "src", 1 }, { "first", 1.473f }, { "length", 77.42f }, { "taps", 60 }, { "growth", 0.776f }, { "energy", 12 }, { "decay", -5.139f }, { "damp", 14338.0f }, { "predelay", 2.4f }, { "width", 148.1f }, { "level", -11.1f }, { "tDecay", 2.08f }, { "tSize", 42.38f }, { "tPre", 4 }, { "tDiff", 91.53f }, { "tModD", 9.095f }, { "tLvl", 2.476f }, { "rtF1", 100.0f }, { "rtM1", 1.079f }, { "rtM2", 1.056f }, { "rtM3", 1.014f }, { "rtM4", 0.7689f }, { "rtF5", 10000.0f }, { "rtM5", 0.3315f }, { "outW", 63.95f }, { "eqG1", -1.105f }, { "eqG2", 0.1211f }, { "eqG3", -0.8759f }, { "eqG4", 1.751f }, { "eqG5", -1.544f }, { "eqG6", 0.2514f }, { "tSpread", 144.1f }, { "tShape", -0.06121f }, { "tBassW", 93.68f }, { "tFill", 12.83f }, { "tSpreadHi", 42.91f }, { "mix", 35 } } },
        { "Warehouse '00s (IR fit)", 3, "2000年代の高級デジタル機の倉庫の IR から合わせたもの。テール 2.0 s。誤差：T30 4.3 %・EDT 14.9 %・C80 0.6 dB。",
          "Fitted to an IR of a 2000s flagship digital unit's warehouse. Tail 2.0 s. Error: T30 4.3 %, EDT 14.9 %, C80 0.6 dB.",
          { { "src", 1 }, { "first", 2.129f }, { "length", 106.9f }, { "taps", 44 }, { "growth", 0.1373f }, { "energy", 11.16f }, { "decay", -11.45f }, { "damp", 16000 }, { "predelay", 3.953f }, { "width", 150 }, { "level", -0.5316f }, { "tDecay", 1.992f }, { "tSize", 39.67f }, { "tPre", 50.19f }, { "tDiff", 93.82f }, { "tModD", 43.16f }, { "tLvl", -7.111f }, { "rtF1", 100.0f }, { "rtM1", 1.091f }, { "rtM2", 0.7875f }, { "rtM3", 0.9952f }, { "rtM4", 0.9211f }, { "rtF5", 10000.0f }, { "rtM5", 1.172f }, { "outW", 90.55f }, { "eqLP", 11272.3f }, { "eqG1", 8.786f }, { "eqG2", 1.098f }, { "eqG3", 4.28f }, { "eqG4", 1.319f }, { "eqG5", -5.185f }, { "eqG6", -5.902f }, { "tShape", 27.69f }, { "tBassW", 46.71f }, { "tFill", 6.435f }, { "tSpreadHi", 53.83f }, { "mix", 35 } } },
        { "Tracking Room '00s (IR fit)", 3, "2000年代のデジタル機のレコーディングルームの IR から合わせたもの。テール 2.5 s。誤差：T30 3.0 %・EDT 8.4 %・C80 0.7 dB。",
          "Fitted to an IR of a 2000s digital unit's tracking room. Tail 2.5 s. Error: T30 3.0 %, EDT 8.4 %, C80 0.7 dB.",
          { { "src", 1 }, { "first", 2.082f }, { "length", 57.5f }, { "taps", 20 }, { "growth", 0.6006f }, { "energy", 11.16f }, { "decay", -8.528f }, { "damp", 16000 }, { "predelay", 4.467f }, { "width", 146.9f }, { "level", -24 }, { "tDecay", 2.522f }, { "tSize", 15.98f }, { "tPre", 60.84f }, { "tDiff", 99.0f }, { "tModD", 21.44f }, { "tLvl", -3.131f }, { "rtF1", 100.0f }, { "rtM1", 0.2006f }, { "rtM2", 0.8836f }, { "rtM3", 0.9418f }, { "rtM4", 0.9067f }, { "rtF5", 10000.0f }, { "rtM5", 2.251f }, { "outW", 96.72f }, { "eqG1", -1.623f }, { "eqG2", 0.1964f }, { "eqG3", -2.129f }, { "eqG4", -0.5542f }, { "eqG5", -4.154f }, { "eqG6", 4.003f }, { "tSpread", 274.8f }, { "tShape", -22.3f }, { "tBassW", 90.56f }, { "tFill", 75.91f }, { "tSpreadHi", 41.7f }, { "mix", 35 } } },
        { "Big Studio '00s (IR fit)", 3, "2000年代のデジタル機の大きなスタジオの IR から合わせたもの。テール 2.4 s。誤差：T30 2.5 %・EDT 6.6 %・C80 0.4 dB。",
          "Fitted to an IR of a 2000s digital unit's big studio. Tail 2.4 s. Error: T30 2.5 %, EDT 6.6 %, C80 0.4 dB.",
          { { "src", 1 }, { "first", 1.104f }, { "length", 45.49f }, { "taps", 62 }, { "growth", 0.5722f }, { "energy", 12 }, { "decay", 12.31f }, { "damp", 16000 }, { "predelay", 8.435f }, { "width", 147 }, { "level", -14.06f }, { "tDecay", 2.428f }, { "tSize", 75.71f }, { "tPre", 5.013f }, { "tDiff", 91.28f }, { "tLvl", 4.02f }, { "rtF1", 100.0f }, { "rtM1", 0.7309f }, { "rtM2", 1.052f }, { "rtM3", 1.045f }, { "rtM4", 0.8179f }, { "rtF5", 10000.0f }, { "rtM5", 0.4185f }, { "eqLP", 10543.1f }, { "eqG1", 3.411f }, { "eqG2", 0.4864f }, { "eqG3", 0.2597f }, { "eqG4", 3.169f }, { "eqG5", -1.019f }, { "eqG6", -4.882f }, { "tSpread", 65.48f }, { "tShape", -40.2f }, { "tBassW", 98 }, { "tFill", 15.17f }, { "tSpreadHi", 60.23f }, { "mix", 35 } } },
        { "Studio A '10s (IR fit)", 3, "2010年代の高級機のスタジオの IR から合わせたもの。テール 0.7 s。誤差：T30 8.2 %・EDT 9.6 %・C80 0.9 dB。",
          "Fitted to an IR of a 2010s high-end unit's studio. Tail 0.7 s. Error: T30 8.2 %, EDT 9.6 %, C80 0.9 dB.",
          { { "src", 1 }, { "first", 1.162f }, { "length", 36.03f }, { "taps", 16 }, { "growth", 1.158f }, { "energy", 12 }, { "decay", -17.85f }, { "damp", 13838.4f }, { "predelay", 2.4f }, { "width", 150 }, { "level", -3.812f }, { "tDecay", 0.6917f }, { "tSize", 11.74f }, { "tPre", 4 }, { "tDiff", 87.89f }, { "tModD", 24.69f }, { "tLvl", -1.097f }, { "rtF1", 100.0f }, { "rtM1", 0.73f }, { "rtM2", 0.7673f }, { "rtM3", 0.9894f }, { "rtM4", 1.001f }, { "rtF5", 10000.0f }, { "rtM5", 0.5135f }, { "outW", 93.39f }, { "eqG1", 3.43f }, { "eqG2", -3.862f }, { "eqG3", -1.95f }, { "eqG4", -0.6103f }, { "eqG5", -5.717f }, { "eqG6", 0.214f }, { "tSpread", 50.91f }, { "tShape", -8.778f }, { "tBassW", 89.09f }, { "tFill", 31.95f }, { "tSpreadHi", 50.88f }, { "mix", 35 } } },
        { "Small Room '10s (IR fit)", 3, "2010年代の高級機の小部屋の IR から合わせたもの。テール 0.7 s。誤差：T30 5.6 %・EDT 18.8 %・C80 0.0 dB。",
          "Fitted to an IR of a 2010s high-end unit's small room. Tail 0.7 s. Error: T30 5.6 %, EDT 18.8 %, C80 0.0 dB.",
          { { "src", 1 }, { "first", 2.175f }, { "length", 63.21f }, { "taps", 48 }, { "growth", 0.03852f }, { "energy", 12 }, { "decay", -18.57f }, { "damp", 16000 }, { "predelay", 2.4f }, { "width", 150 }, { "level", -2.747f }, { "tDecay", 0.724f }, { "tSize", 26.58f }, { "tPre", 52.86f }, { "tDiff", 97.13f }, { "tModD", 33.82f }, { "tLvl", -11.45f }, { "rtF1", 100.0f }, { "rtM1", 0.9883f }, { "rtM2", 0.8324f }, { "rtM3", 1.006f }, { "rtM4", 0.886f }, { "rtF5", 10000.0f }, { "rtM5", 0.4945f }, { "outW", 104.9f }, { "eqLP", 10903.1f }, { "eqG1", 6.836f }, { "eqG2", 0.6128f }, { "eqG3", 2.419f }, { "eqG4", 4.752f }, { "eqG5", -3.245f }, { "eqG6", -6.355f }, { "tSpread", 6 }, { "tShape", 20.48f }, { "tFill", 22.95f }, { "tSpreadHi", 72.01f }, { "mix", 35 } } },
        { "Large Room '10s (IR fit)", 3, "2010年代の高級機の大きな部屋の IR から合わせたもの。テール 1.1 s。誤差：T30 6.5 %・EDT 17.9 %・C80 0.1 dB。",
          "Fitted to an IR of a 2010s high-end unit's large room. Tail 1.1 s. Error: T30 6.5 %, EDT 17.9 %, C80 0.1 dB.",
          { { "src", 1 }, { "first", 2.652f }, { "length", 57.21f }, { "taps", 40 }, { "growth", 0.08639f }, { "energy", 11.55f }, { "decay", -26.37f }, { "damp", 7244.0f }, { "predelay", 6.728f }, { "width", 147 }, { "level", -3.854f }, { "tDecay", 1.069f }, { "tSize", 42.06f }, { "tPre", 30.69f }, { "tDiff", 81.01f }, { "tLvl", -3.36f }, { "rtF1", 100.0f }, { "rtM1", 1.117f }, { "rtM2", 0.9865f }, { "rtM3", 0.9719f }, { "rtM4", 0.9286f }, { "rtF5", 10000.0f }, { "rtM5", 0.2088f }, { "eqLP", 10016.8f }, { "eqG1", 3.418f }, { "eqG2", 3.981f }, { "eqG3", 2.845f }, { "eqG4", 5.913f }, { "eqG5", -1.661f }, { "eqG6", -6.723f }, { "tSpread", 6 }, { "tShape", 46.04f }, { "tBassW", 98 }, { "tFill", 19.25f }, { "tSpreadHi", 51.27f }, { "mix", 35 } } },
        { "Marble Foyer '10s (IR fit)", 3, "2010年代の高級機の大理石のロビーの IR から合わせたもの。テール 1.2 s。誤差：T30 3.8 %・EDT 12.2 %・C80 0.3 dB。",
          "Fitted to an IR of a 2010s high-end unit's marble foyer. Tail 1.2 s. Error: T30 3.8 %, EDT 12.2 %, C80 0.3 dB.",
          { { "src", 1 }, { "first", 1.791f }, { "length", 83.54f }, { "taps", 55 }, { "growth", 0.4824f }, { "energy", 12 }, { "decay", -19.85f }, { "damp", 16000 }, { "predelay", 2.4f }, { "width", 150 }, { "level", -3.48f }, { "tDecay", 1.169f }, { "tSize", 56.85f }, { "tPre", 4 }, { "tDiff", 79.29f }, { "tModD", 70.29f }, { "tLvl", 1.519f }, { "rtF1", 100.0f }, { "rtM1", 0.8474f }, { "rtM2", 0.8602f }, { "rtM3", 1.11f }, { "rtM4", 0.9197f }, { "rtF5", 10000.0f }, { "rtM5", 0.5208f }, { "outW", 99.04f }, { "eqG1", -1.317f }, { "eqG2", 0.9579f }, { "eqG3", -2.45f }, { "eqG4", 1.987f }, { "eqG5", -4.348f }, { "eqG6", -1.477f }, { "tSpread", 6 }, { "tShape", -8.743f }, { "tBassW", 80.34f }, { "tFill", 6.416f }, { "tSpreadHi", 79.39f }, { "mix", 35 } } },
        { "Cavern '10s (IR fit)", 3, "2010年代の高級機の洞窟の IR から合わせたもの。テール 3.4 s。誤差：T30 3.7 %・EDT 13.8 %・C80 0.7 dB。",
          "Fitted to an IR of a 2010s high-end unit's cavern. Tail 3.4 s. Error: T30 3.7 %, EDT 13.8 %, C80 0.7 dB.",
          { { "src", 1 }, { "first", 1.597f }, { "length", 40.19f }, { "taps", 66 }, { "growth", 0.9178f }, { "energy", 12 }, { "decay", 16.36f }, { "damp", 11960.3f }, { "width", 147 }, { "level", -9.166f }, { "tDecay", 3.421f }, { "tSize", 66.43f }, { "tPre", 0 }, { "tDiff", 89.03f }, { "tLvl", -0.4275f }, { "rtF1", 100.0f }, { "rtM1", 2.405f }, { "rtM2", 1.474f }, { "rtM3", 1.058f }, { "rtM4", 0.6363f }, { "rtF5", 10000.0f }, { "rtM5", 0.1689f }, { "eqG1", 2.989f }, { "eqG2", 1.608f }, { "eqG3", 4.031f }, { "eqG4", 7.99f }, { "eqG5", -8.032f }, { "eqG6", -5.474f }, { "tSpread", 124.6f }, { "tShape", -16.74f }, { "tBassW", 98 }, { "tFill", 33.76f }, { "tSpreadHi", 35.59f }, { "mix", 35 } } },
        // (kept last so the other presets keep their numbers)
        { "Flat (Init)", 2, "すべてフラット：周波数ごとの RT は全帯域 100 %、MASTER EQ・インプットフィルター・ER のフィルターはなし、色付け（帯域制限・ビット・暗くなる量・ゆらぎ）もなし。ここから自分で作るための出発点。",
          "Everything flat: RT 100 % at every frequency, no master EQ, input filter or ER filter, no colouring (band limit, bits, darkening, wander). A starting point to build your own.",
          { { "src", 0 }, { "tDecay", 2.0f }, { "rtM1", 1 }, { "rtM2", 1 }, { "rtM3", 1 }, { "rtM4", 1 }, { "rtM5", 1 },
            { "mbM1", 1 }, { "mbM2", 1 }, { "mbM3", 1 }, { "mbM4", 1 }, { "mbM5", 1 }, { "rtMode", 0 },
            { "eqG1", 0 }, { "eqG2", 0 }, { "eqG3", 0 }, { "eqG4", 0 }, { "eqG5", 0 }, { "eqG6", 0 }, { "eqHP", 20 }, { "eqLP", 20000 },
            { "iLC", 20 }, { "iHC", 20000 }, { "iTilt", 0 }, { "lowcut", 20 }, { "highcut", 20000 }, { "bw", 20 }, { "bits", 24 }, { "darken", 0 }, { "wander", 0 } } },
        { "Vocal Plate", 3, "プレートエンジン。2.0 s、明るくなめらか、プリディレイ 30 ms でボーカルの前に置かない。低域は短め（濁らない）。",
          "The plate engine: 2.0 s, bright and smooth, 30 ms predelay keeps it behind the vocal. Shorter lows (no mud).",
          { { "src", 1 }, { "first", 1 }, { "length", 40 }, { "taps", 48 }, { "growth", 0.6f }, { "level", -10 }, { "tEng", 1 }, { "tDecay", 2.0f }, { "tSize", 80 }, { "tPre", 2 }, { "tDiff", 85 },
            { "tModD", 25 }, { "predelay", 30 }, { "rtM1", 0.7f }, { "rtM2", 0.9f }, { "rtM4", 0.95f }, { "rtM5", 0.75f }, { "eqHP", 150 }, { "mix", 25 } } },
        { "Drum Plate", 3, "短く密なプレート（1.2 s）。スネアに。高域を残し、低域はさらに短く。", "A short dense plate (1.2 s) for snares: keeps the highs, even shorter lows.",
          { { "src", 1 }, { "first", 1 }, { "length", 30 }, { "taps", 48 }, { "growth", 0.5f }, { "level", -12 }, { "tEng", 1 }, { "tDecay", 1.2f }, { "tSize", 60 }, { "tPre", 1 }, { "tDiff", 90 },
            { "tModD", 10 }, { "predelay", 10 }, { "rtM1", 0.6f }, { "rtM2", 0.85f }, { "rtM5", 0.85f }, { "eqHP", 200 }, { "mix", 20 } } },
        { "Guitar Amp Spring", 3, "ギターアンプのばねリバーブ（2.3 s）。ER なし、0 ms から「ビヨーン」と返り、5 kHz までの帯域。", "A guitar amp's spring reverb (2.3 s): no ER, the boing from 0 ms, a band up to 5 kHz.",
          { { "erOn", 0 }, { "tEng", 2 }, { "tDecay", 2.3f }, { "tSize", 80 }, { "tPre", 0 }, { "tDiff", 70 }, { "tModD", 15 }, { "tModR", 0.8f }, { "predelay", 0 },
            { "rtM1", 0.8f }, { "rtM2", 1.0f }, { "rtM4", 0.8f }, { "rtM5", 0.6f }, { "iLC", 120 }, { "mix", 25 } } },
        { "Dub Spring", 3, "長く強く揺れるばね（4.5 s、Diffusion 100 %＝ビヨーン最大）。ダブのスネアやギターの飛ばしに。", "A long, strongly ringing spring (4.5 s, Diffusion 100 % = the most boing): dub snares and guitar throws.",
          { { "erOn", 0 }, { "tEng", 2 }, { "tDecay", 4.5f }, { "tSize", 110 }, { "tPre", 0 }, { "tDiff", 100 }, { "tModD", 25 }, { "tModR", 0.5f }, { "predelay", 0 },
            { "rtM1", 0.9f }, { "rtM5", 0.6f }, { "eqHP", 150 }, { "mix", 40 } } },
        { "Ambient Space", 3, "部屋を超える空間（12 s、Size 最大寄り、Feedback 30 %）。パッドやギターの背景に。", "A space beyond rooms (12 s, a large Size, Feedback 30 %): behind pads and guitars.",
          { { "erOn", 0 }, { "tEng", 3 }, { "tDecay", 12.0f }, { "tSize", 200 }, { "tPre", 0 }, { "tDiff", 100 }, { "tModD", 50 }, { "tModR", 0.3f }, { "predelay", 20 },
            { "aGrav", 0 }, { "aFb", 30 }, { "rtM1", 0.8f }, { "rtM5", 0.5f }, { "eqHP", 120 }, { "mix", 40 } } },
        { "Reverse Swell", 3, "後ろほど大きくなる響き（Gravity −80 %、6 s）。ボーカルの語尾やスネアを逆再生のように膨らませる。", "A sound that grows later (Gravity -80 %, 6 s): swells vocal tails and snares like a reversed reverb.",
          { { "erOn", 0 }, { "tEng", 3 }, { "tDecay", 6.0f }, { "tSize", 140 }, { "tPre", 0 }, { "tDiff", 90 }, { "tModD", 30 }, { "tModR", 0.4f }, { "predelay", 0 },
            { "aGrav", -80 }, { "aFb", 0 }, { "rtM5", 0.6f }, { "eqHP", 150 }, { "mix", 35 } } },
        // ================= 0.7.0 =================
        // ---- reproductions of classic units (closed-loop fits to IRs; era names, no product names) ----
        { "Digital Hall '78 (IR fit)", 3, "70年代末の第一世代デジタルホールの IR から合わせたもの。約 190 ms かけて膨らむ立ち上がり（Spread）と立ち上がりの充填（Onset Fill 26 %）、2.2 s、高域は早く暗くなる。誤差：T30 3.1 %・EDT 11 %・C80 0.4 dB。",
          "Fitted to an IR of a late-70s first-generation digital hall. A swelling onset over ~190 ms (Spread) with Onset Fill 26 %, 2.2 s, the highs darken early. Error: T30 3.1 %, EDT 11 %, C80 0.4 dB.",
          { { "src", 1 }, { "first", 9.398f }, { "length", 85 }, { "taps", 66 }, { "growth", 0.7757f }, { "energy", 12 }, { "decay", 5.253f }, { "damp", 16000 }, { "width", 150 }, { "level", -17.5f }, { "tDecay", 2.226f }, { "tSize", 98.86f }, { "tPre", 2.407f }, { "tDiff", 73.9f }, { "tModD", 8.504f }, { "tLvl", 5.525f }, { "rtF1", 100 }, { "rtM1", 1.081f }, { "rtM2", 1.02f }, { "rtM3", 1.062f }, { "rtM4", 0.8793f }, { "rtF5", 10000 }, { "rtM5", 0.4449f }, { "outW", 101.4f }, { "eqLP", 12590 }, { "eqG1", 4.082f }, { "eqG2", 2.622f }, { "eqG3", 0.669f }, { "eqG4", 2.831f }, { "eqG5", -1.485f }, { "eqG6", -5.862f }, { "tSpread", 190.7f }, { "tShape", -7.387f }, { "tBassW", 83.06f }, { "tFill", 26.05f }, { "mix", 35 } } },
        { "Random Hall '86 (IR fit)", 3, "80年代半ばの高級デジタル機のランダムホールの IR から。まばらな ER のあと 180 ms かけて密になる（Spread）、2.9 s、上は急に落ちる、揺らぎやや多め。誤差：T30 4.9 %・EDT 9.0 %・C80 0.5 dB。",
          "Fitted to an IR of a mid-80s flagship unit's random hall. Sparse ER, then density builds over 180 ms (Spread), 2.9 s, the top falls fast, a little more movement. Error: T30 4.9 %, EDT 9.0 %, C80 0.5 dB.",
          { { "src", 1 }, { "first", 2.079f }, { "length", 94.98f }, { "taps", 44 }, { "growth", 0.6564f }, { "energy", 10.82f }, { "decay", -0.9503f }, { "damp", 13570 }, { "predelay", 23.93f }, { "width", 150 }, { "level", -20.19f }, { "tDecay", 2.884f }, { "tSize", 80.06f }, { "tPre", 7.619f }, { "tDiff", 49.91f }, { "tModD", 30.56f }, { "tLvl", 2.918f }, { "rtF1", 100 }, { "rtM1", 1.072f }, { "rtM2", 1.122f }, { "rtM3", 1.124f }, { "rtM4", 0.6181f }, { "rtF5", 10000 }, { "rtM5", 0.2361f }, { "outW", 98.59f }, { "eqLP", 13600 }, { "eqG1", 7.634f }, { "eqG2", 3.738f }, { "eqG3", 0.189f }, { "eqG4", 5.516f }, { "eqG5", -3.135f }, { "eqG6", -9.364f }, { "tSpread", 185.5f }, { "tShape", -1.29f }, { "tFill", 14.41f }, { "mix", 35 } } },
        { "Wide Hall '00s (IR fit)", 3, "広い大ホールの IR から。低域 7 s・高域 1.5 s と帯域で長さが大きく違い、0 ms から密。左右いっぱいに広い（Output Width 200 %）。誤差：T30 3.9 %・EDT 7.7 %・C80 0.3 dB。",
          "Fitted to an IR of a large, wide hall. Lows 7 s, highs 1.5 s: very different lengths per band, dense from 0 ms, as wide as it goes (Output Width 200 %). Error: T30 3.9 %, EDT 7.7 %, C80 0.3 dB.",
          { { "src", 1 }, { "first", 1.355f }, { "length", 71.27f }, { "taps", 90 }, { "growth", 0.7991f }, { "energy", 11.95f }, { "decay", -20.71f }, { "damp", 14180 }, { "predelay", 2.725f }, { "width", 144.9f }, { "level", -11.1f }, { "mix", 35 },
            { "tDecay", 3.669f }, { "tSize", 52.56f }, { "tPre", 41.67f }, { "tDiff", 76.24f }, { "tModD", 3.675f }, { "tLvl", -6.12f }, { "rtF1", 100 }, { "rtM1", 2.998f }, { "rtM2", 1.745f }, { "rtM3", 1.061f }, { "rtM4", 0.5978f }, { "rtF5", 10000 }, { "rtM5", 0.1778f },
            { "outW", 200 }, { "eqLP", 19910 }, { "eqG2", 2.089f }, { "eqG3", 0.8429f }, { "eqG4", 2.735f }, { "eqG5", -0.4277f }, { "eqG6", -7.184f } } },
        { "Modern Chamber '10s (IR fit)", 3, "2010年代の高級機のチェンバーの IR から。すぐ密になり、2.4 s でほぼ平らな周波数ごとの長さ、上は 8 kHz から落ちる。誤差：T30 3.3 %・EDT 5.5 %・C80 0.1 dB。",
          "Fitted to an IR of a 2010s high-end unit's chamber. Dense at once, 2.4 s, nearly the same length at every frequency, the top rolls off above 8 kHz. Error: T30 3.3 %, EDT 5.5 %, C80 0.1 dB.",
          { { "src", 1 }, { "first", 1.092f }, { "length", 57.82f }, { "taps", 32 }, { "growth", 0.6411f }, { "energy", 11.16f }, { "decay", -18.76f }, { "damp", 10770 }, { "width", 147.1f }, { "level", -8.815f }, { "tDecay", 2.385f }, { "tSize", 55.6f }, { "tPre", 3.484f }, { "tDiff", 51.22f }, { "tModD", 23.06f }, { "tLvl", -0.625f }, { "rtF1", 100 }, { "rtM1", 1.196f }, { "rtM2", 0.9912f }, { "rtM3", 1.02f }, { "rtM4", 0.9089f }, { "rtF5", 10000 }, { "rtM5", 0.7832f }, { "outW", 98.76f }, { "eqLP", 7765 }, { "eqG1", 5.596f }, { "eqG2", 5.022f }, { "eqG3", 3.14f }, { "eqG4", 4.935f }, { "eqG5", 0.2565f }, { "eqG6", -9.442f }, { "tSpread", 107.5f }, { "tShape", 7.949f }, { "tFill", 21.83f }, { "mix", 35 } } },
        { "Reel Room '00s (IR fit)", 3, "2000年代のデジタル機のルームの IR（True Stereo）から。強い初期反射と 1.6 s の締まったテール、高域は短い。誤差：T30 4.1 %・EDT 11 %・C80 1.2 dB。",
          "Fitted to a true-stereo IR of a 2000s digital unit's room. Strong early reflections, a tight 1.6 s tail, short highs. Error: T30 4.1 %, EDT 11 %, C80 1.2 dB.",
          { { "src", 1 }, { "first", 6.312f }, { "length", 50.82f }, { "taps", 30 }, { "growth", 1.132f }, { "energy", -2.371f }, { "decay", -28.22f }, { "damp", 2980 }, { "predelay", 2.4f }, { "width", 145.5f }, { "level", 9.466f }, { "tDecay", 1.639f }, { "tSize", 66.99f }, { "tPre", 33.2f }, { "tDiff", 82.08f }, { "tModD", 27.44f }, { "tLvl", -2.884f }, { "rtF1", 100 }, { "rtM1", 0.8064f }, { "rtM2", 1.068f }, { "rtM3", 1.447f }, { "rtM4", 0.6601f }, { "rtF5", 10000 }, { "rtM5", 0.1781f }, { "outW", 105.4f }, { "eqG1", -2.552f }, { "eqG2", -0.1223f }, { "eqG3", -2.552f }, { "eqG4", -2.552f }, { "eqG5", -2.552f }, { "eqG6", 3.712f }, { "tSpread", 9.065f }, { "tShape", 24.6f }, { "tFill", 15.15f }, { "mix", 35 } } },
        // ---- by type, using 0.6 / 0.7 features ----
        { "Small Studio Room", 3, "4×5×3 m の吸音したスタジオ。0.5 s、すぐ消える。ドライな素材を自然に部屋へ置くだけ。", "A treated 4 x 5 x 3 m studio: 0.5 s, gone at once. Just puts a dry source in a room.",
          { { "src", 0 }, { "roomW", 4 }, { "roomD", 5 }, { "roomH", 3 }, { "wall", 3 }, { "floor", 3 }, { "tDecay", 0.5f }, { "tSize", 30 }, { "tPre", 4 }, { "tDiff", 80 }, { "rtM1", 1.0f }, { "rtM5", 0.6f }, { "tBassW", 50 }, { "mix", 25 } } },
        { "Large Hall (Build-up)", 3, "大ホール 2.8 s。テールが 120 ms かけて膨らむ（Spread 120 ms・Shape +40）ので、アタックの後ろに残響が回り込む。", "A large hall, 2.8 s. The tail swells over 120 ms (Spread 120 ms, Shape +40), so the reverb wraps in behind the attack.",
          { { "src", 0 }, { "roomW", 24 }, { "roomD", 34 }, { "roomH", 13 }, { "wall", 2 }, { "floor", 6 }, { "order", 3 }, { "predelay", 18 }, { "tDecay", 2.8f }, { "tSize", 150 }, { "tPre", 10 }, { "tDiff", 85 }, { "tModD", 15 },
            { "rtM1", 1.3f }, { "rtM2", 1.1f }, { "rtM4", 0.8f }, { "rtM5", 0.5f }, { "tSpread", 120 }, { "tShape", 40 }, { "tBassW", 60 }, { "mix", 30 } } },
        { "Chorused Hall '80s", 3, "80年代のデジタルホールの質感：出力コーラス（Mix 45 %）で揺れて広がり、上は 8 kHz で丸く、立ち上がりは 80 ms。", "The '80s digital hall texture: an output chorus (45 %) that moves and widens, the top rounded at 8 kHz, an 80 ms onset.",
          { { "src", 1 }, { "first", 14 }, { "taps", 40 }, { "energy", -8 }, { "damp", 7000 }, { "width", 130 }, { "predelay", 15 }, { "tDecay", 2.6f }, { "tSize", 110 }, { "tPre", 20 }, { "tDiff", 80 }, { "tModD", 25 },
            { "rtM1", 1.2f }, { "rtM5", 0.5f }, { "eqLP", 8000 }, { "tSpread", 80 }, { "tShape", 30 }, { "chMix", 45 }, { "chRate", 0.45f }, { "chDepth", 3.5f }, { "mix", 30 } } },
        { "Gated Snare '80s", 3, "80年代のゲートリバーブ：長い部屋（3 s）をゲートで 280 ms 後にバッサリ切る（Hold 280 ms・Release 40 ms、キー＝入力）。スネアやタムに。", "The '80s gated reverb: a long room (3 s) cut 280 ms later by the gate (hold 280 ms, release 40 ms, key = input). Snares and toms.",
          { { "src", 0 }, { "roomW", 9 }, { "roomD", 12 }, { "roomH", 5 }, { "wall", 1 }, { "floor", 3 }, { "tDecay", 3.0f }, { "tSize", 90 }, { "tPre", 2 }, { "tDiff", 90 }, { "rtM5", 0.8f },
            { "gtOn", 1 }, { "gtThr", -36 }, { "gtHold", 280 }, { "gtRel", 40 }, { "gtKey", 0 }, { "eqHP", 180 }, { "mix", 40 } } },
        { "Tempo Pulse Space", 3, "アンビエントの長い空間に、テンポに同期した LFO（1/8、矩形）でテールの音量を刻む（LFO 1 → Tail Level −70 %）。シンセやギターの効果に。", "A long ambient space chopped in time: a tempo-synced LFO (1/8, square) on the tail level (LFO 1 -> Tail Level -70 %). For synths and guitars.",
          { { "erOn", 0 }, { "tEng", 3 }, { "tDecay", 8 }, { "tSize", 180 }, { "tDiff", 100 }, { "tModD", 40 }, { "aFb", 15 }, { "rtM5", 0.5f }, { "eqHP", 150 },
            { "l1Sync", 6 }, { "l1Shape", 3 }, { "m1Src", 1 }, { "m1Dst", 3 }, { "m1Amt", -70 }, { "mix", 40 } } },
        // ---- by use ----
        { "Vocal Hall (Spectral Duck)", 3, "歌ものホール 2.4 s に、スペクトル型ダッキング（8 dB）。声と重なる帯域だけ残響が引き、隙間で戻るので、量感を保ったまま言葉が明瞭。", "A 2.4 s vocal hall with spectral ducking (8 dB): only the bands that overlap the voice step back, returning in the gaps. Size stays, words stay clear.",
          { { "src", 0 }, { "roomW", 16 }, { "roomD", 24 }, { "roomH", 9 }, { "predelay", 25 }, { "tDecay", 2.4f }, { "tSize", 110 }, { "tPre", 18 }, { "tDiff", 85 }, { "tModD", 18 }, { "rtM1", 1.05f }, { "rtM5", 0.55f }, { "iLC", 180 }, { "iHC", 10000 },
            { "dkMode", 3 }, { "dkDepth", 8 }, { "dkThr", -34 }, { "dkAtk", 15 }, { "dkRel", 300 }, { "mix", 30 } } },
        { "Vocal Plate (Ducked)", 3, "ボーカル用プレート 2.2 s を、声が鳴る間だけ 6 dB 下げる（Single、Release 250 ms）。テンポの速い曲でも残響が言葉にかぶらない。", "A 2.2 s vocal plate that dips 6 dB while the voice sings (single, release 250 ms). The reverb stays off the words even in fast songs.",
          { { "src", 1 }, { "first", 1 }, { "length", 40 }, { "taps", 48 }, { "growth", 0.6f }, { "level", -10 }, { "tEng", 1 }, { "tDecay", 2.2f }, { "tSize", 80 }, { "tPre", 2 }, { "tDiff", 85 }, { "tModD", 25 }, { "predelay", 25 },
            { "rtM1", 0.7f }, { "rtM2", 0.9f }, { "rtM5", 0.75f }, { "eqHP", 150 }, { "dkMode", 1 }, { "dkDepth", 6 }, { "dkThr", -32 }, { "dkAtk", 10 }, { "dkRel", 250 }, { "mix", 25 } } },
        { "Drum Room (MB Duck)", 3, "ドラムの部屋 1.0 s に、3 帯域のダッキング。キックとスネアが鳴る瞬間だけ低域・中域の残響が引くので、打点が埋もれない。", "A 1.0 s drum room with 3-band ducking: the low and mid reverb steps back on each kick and snare, so the hits stay clear.",
          { { "src", 0 }, { "roomW", 7 }, { "roomD", 9 }, { "roomH", 3.6f }, { "wall", 1 }, { "floor", 3 }, { "tDecay", 1.0f }, { "tSize", 55 }, { "tPre", 6 }, { "tDiff", 60 }, { "rtM1", 1.1f }, { "rtM5", 0.6f }, { "tBassW", 40 },
            { "dkMode", 2 }, { "dkBands", 3 }, { "dkDepth", 9 }, { "dkThr", -30 }, { "dkAtk", 3 }, { "dkRel", 150 }, { "mix", 30 } } },
        { "Guitar Chamber", 3, "ギター用のチェンバー 1.5 s。中域が前に出て、低域は短く（Bass Width 50 %）。ストロークやアルペジオに。", "A 1.5 s chamber for guitars: forward mids, short and narrower lows (Bass Width 50 %). Strums and arpeggios.",
          { { "src", 1 }, { "first", 5 }, { "length", 55 }, { "taps", 60 }, { "growth", 1.3f }, { "tDecay", 1.5f }, { "tSize", 65 }, { "tPre", 8 }, { "tDiff", 85 }, { "tModD", 15 },
            { "rtM1", 0.8f }, { "rtM2", 0.95f }, { "rtM5", 0.65f }, { "eqHP", 120 }, { "eqG3", 1.5f }, { "tBassW", 50 }, { "mix", 25 } } },
        { "Piano Hall", 3, "ピアノ用のホール 2.2 s。低域を長くしすぎず（濁らない）、高域はゆっくり減衰。プリディレイ 15 ms でアタックを残す。", "A 2.2 s hall for piano: lows not too long (no mud), highs decay slowly, 15 ms predelay keeps the attack.",
          { { "src", 0 }, { "roomW", 18 }, { "roomD", 26 }, { "roomH", 10 }, { "wall", 2 }, { "floor", 6 }, { "predelay", 15 }, { "tDecay", 2.2f }, { "tSize", 120 }, { "tPre", 15 }, { "tDiff", 85 }, { "tModD", 10 },
            { "rtM1", 1.05f }, { "rtM4", 0.9f }, { "rtM5", 0.7f }, { "tSpread", 40 }, { "tShape", 20 }, { "mix", 25 } } },
        { "Strings Hall (Long Build)", 3, "弦・オーケストラ用の大ホール 3.2 s。150 ms かけて膨らむ立ち上がり（Spread 150 ms・Shape +50）で、ホールの奥から響きが来る。", "A 3.2 s hall for strings and orchestra: a swelling onset over 150 ms (Spread 150 ms, Shape +50), as if from deep in the hall.",
          { { "src", 0 }, { "roomW", 26 }, { "roomD", 38 }, { "roomH", 15 }, { "wall", 2 }, { "floor", 6 }, { "order", 3 }, { "predelay", 20 }, { "tDecay", 3.2f }, { "tSize", 170 }, { "tPre", 12 }, { "tDiff", 90 }, { "tModD", 12 },
            { "rtM1", 1.3f }, { "rtM2", 1.1f }, { "rtM4", 0.8f }, { "rtM5", 0.5f }, { "tSpread", 150 }, { "tShape", 50 }, { "tBassW", 60 }, { "mix", 35 } } },
        { "Breathing Pad Space", 3, "パッド用の空間 7 s。エンベロープで弾くほど短く（Env → Decay −40 %）、ゆっくりした LFO で広がりが呼吸する（LFO 2 → Width）。", "A 7 s space for pads: the harder you play the shorter it gets (Env -> Decay -40 %), and a slow LFO makes the width breathe (LFO 2 -> Width).",
          { { "erOn", 0 }, { "tEng", 3 }, { "tDecay", 7 }, { "tSize", 160 }, { "tDiff", 100 }, { "tModD", 45 }, { "rtM5", 0.55f }, { "eqHP", 120 }, { "chMix", 25 },
            { "l2Rate", 0.08f }, { "l2Shape", 0 }, { "m1Src", 3 }, { "m1Dst", 5 }, { "m1Amt", -40 }, { "m2Src", 2 }, { "m2Dst", 1 }, { "m2Amt", 35 }, { "mix", 40 } } },
        { "Mix Bus Glue Room", 3, "ミックス全体にうすく足す部屋（0.6 s、Mix 12 %）。低域はモノ（Bass Width 0 %）で、ステレオの低音を崩さずにまとまりだけ足す。", "A room added lightly to a whole mix (0.6 s, Mix 12 %). Mono lows (Bass Width 0 %): glue without blurring the stereo bass.",
          { { "src", 0 }, { "roomW", 6 }, { "roomD", 8 }, { "roomH", 3.5f }, { "wall", 2 }, { "floor", 3 }, { "tDecay", 0.6f }, { "tSize", 40 }, { "tPre", 4 }, { "tDiff", 85 }, { "rtM1", 0.8f }, { "rtM5", 0.6f }, { "iLC", 200 }, { "tBassW", 0 }, { "mix", 12 } } },
        { "Dub Throw Spring (Env)", 3, "ダブの飛ばし用スプリング 4 s。強く入れたときほど長く鳴る（Env → Decay +60 %）。スネアやギターの一発に。", "A 4 s spring for dub throws: the harder it is hit the longer it rings (Env -> Decay +60 %). Single snare or guitar hits.",
          { { "erOn", 0 }, { "tEng", 2 }, { "tDecay", 4.0f }, { "tSize", 110 }, { "tDiff", 95 }, { "tModD", 20 }, { "rtM1", 0.9f }, { "rtM5", 0.6f }, { "eqHP", 150 },
            { "enAtk", 5 }, { "enRel", 600 }, { "m1Src", 3 }, { "m1Dst", 5 }, { "m1Amt", 60 }, { "mix", 35 } } },
    };
    return p;
}

// 0.7.0: how the main menu files the reverbs: type, era / model (reproductions), use (each preset can appear in several)
struct PresetCat { const char* name; const char* type; const char* era; const char* use; };
inline const std::vector<PresetCat>& presetCats()
{
    static const std::vector<PresetCat> c {
        { "Fan Hall (Concert)", "Hall", nullptr, "Orchestra / keys" }, { "Fan Hall (Small Theater)", "Hall", nullptr, "Vocal" },
        { "Skewed Studio Room", "Room", nullptr, "Vocal" }, { "Skewed Live Room", "Room", nullptr, "Drums" },
        { "Dome (Planetarium)", "Hall", nullptr, "Synth / pad" }, { "Dome (Cupola)", "Hall", nullptr, "Orchestra / keys" }, { "Dome (Small Cupola)", "Chamber", nullptr, "Guitar" },
        { "Connected (Live Room + Booth)", "Room", nullptr, "Drums" }, { "Connected (Hall + Stone Foyer)", "Hall", nullptr, "FX" }, { "Connected (Same Room)", "Room", nullptr, "Drums" },
        { "Corridor (Hallway)", "Room", nullptr, "FX" }, { "Corridor (Tile Stairwell)", "Chamber", nullptr, "FX" },
        { "Empty Room (Clutter 0)", "Room", nullptr, "Vocal" }, { "Cluttered Room (Clutter 80)", "Room", nullptr, "Vocal" },
        { "Small Studio Room", "Room", nullptr, "Vocal" },
        { "Mix Bus Glue Room", "Room", nullptr, "Mix bus" }, { "Drum Room (MB Duck)", "Room", nullptr, "Drums" },
        { "Large Hall (Build-up)", "Hall", nullptr, "Orchestra / keys" },
        { "Piano Hall", "Hall", nullptr, "Orchestra / keys" }, { "Strings Hall (Long Build)", "Hall", nullptr, "Orchestra / keys" }, { "Vocal Hall (Spectral Duck)", "Hall", nullptr, "Vocal" },
        { "Chorused Hall '80s", "Hall", "1980s", "Synth / pad" },
        { "Guitar Chamber", "Chamber", nullptr, "Guitar" },
        { "Vocal Plate", "Plate", nullptr, "Vocal" }, { "Drum Plate", "Plate", nullptr, "Drums" }, { "Vocal Plate (Ducked)", "Plate", nullptr, "Vocal" },
        { "Guitar Amp Spring", "Spring", nullptr, "Guitar" }, { "Dub Spring", "Spring", nullptr, "FX" }, { "Dub Throw Spring (Env)", "Spring", nullptr, "FX" },
        { "Ambient Wash", "Ambient", nullptr, "Synth / pad" }, { "Ambient Space", "Ambient", nullptr, "Synth / pad" }, { "Breathing Pad Space", "Ambient", nullptr, "Synth / pad" },
        { "Reverse Swell", "Non-linear / FX", nullptr, "FX" }, { "Gated Snare '80s", "Non-linear / FX", "1980s", "Drums" }, { "Tempo Pulse Space", "Non-linear / FX", nullptr, "FX" },
        { "Digital Hall '78 (IR fit)", "Hall", "1970s", "Vocal" }, { "Random Hall '86 (IR fit)", "Hall", "1980s", "Vocal" }, { "Wide Hall '00s (IR fit)", "Hall", "2000s", "Orchestra / keys" },
        { "Modern Chamber '10s (IR fit)", "Chamber", "2010s", "Vocal" }, { "Reel Room '00s (IR fit)", "Room", "2000s", "Drums" },
        { "Steel Plate 2 s '50s (IR fit)", "Plate", "1950s", "Vocal" }, { "First-gen Hall '77 (IR fit)", "Hall", "1970s", "Vocal" }, { "Vocal Plate '10s (IR fit)", "Plate", "2010s", "Vocal" }, { "Snare Plate '10s (IR fit)", "Plate", "2010s", "Drums" }, { "Master Plate '00s (IR fit)", "Plate", "2000s", "Vocal" }, { "Concert Hall '10s (IR fit)", "Hall", "2010s", "Orchestra / keys" }, { "Classic Hall '10s (IR fit)", "Hall", "2010s", "Orchestra / keys" }, { "Small Hall '10s (IR fit)", "Hall", "2010s", "Vocal" }, { "Concert Hall '00s (IR fit)", "Hall", "2000s", "Orchestra / keys" }, { "Large Hall '80s (IR fit)", "Hall", "1980s", "Vocal" }, { "Studio Hall '00s (IR fit)", "Hall", "2000s", "Vocal" }, { "Jazz Hall '00s (IR fit)", "Hall", "2000s", "Guitar" }, { "Empty Hall '00s (IR fit)", "Hall", "2000s", "Orchestra / keys" }, { "Church '10s (IR fit)", "Hall", "2010s", "Orchestra / keys" }, { "Vocal Chamber '10s (IR fit)", "Chamber", "2010s", "Vocal" }, { "Large Chamber '10s (IR fit)", "Chamber", "2010s", "Orchestra / keys" }, { "Echo Chamber '00s (IR fit)", "Chamber", "2000s", "Vocal" }, { "Theater '00s (IR fit)", "Room", "2000s", "Orchestra / keys" }, { "Ambience '80s (IR fit)", "Room", "1980s", "Drums" }, { "Warehouse '00s (IR fit)", "Room", "2000s", "Drums" }, { "Tracking Room '00s (IR fit)", "Room", "2000s", "Vocal" }, { "Big Studio '00s (IR fit)", "Room", "2000s", "Drums" }, { "Studio A '10s (IR fit)", "Room", "2010s", "Drums" }, { "Small Room '10s (IR fit)", "Room", "2010s", "Vocal" }, { "Large Room '10s (IR fit)", "Room", "2010s", "Drums" }, { "Marble Foyer '10s (IR fit)", "Room", "2010s", "FX" }, { "Cavern '10s (IR fit)", "Ambient", "2010s", "Synth / pad" },
    };
    return c;
}
inline const PresetCat* presetCat (const char* name) { for (auto& c : presetCats()) if (std::string (c.name) == name) return &c; return nullptr; }

struct LessonDef
{
    const char* titleJ; const char* titleE; const char* ja; const char* en;
    const char* preset;                                // preset to load first (nullptr = none)
    std::vector<std::pair<const char*, float>> v;      // then these values
};

// COLOR presets: digital eras. age() maps the current Bandwidth / Bits to 0 (modern, cool) .. 1 (oldest, warm).
struct ColorPreset { const char* name; const char* ja; const char* en; float bw, bits, wander; };
inline const std::vector<ColorPreset>& colorPresets()
{
    static const std::vector<ColorPreset> c {
        { "Clean / Modern", "帯域・ビットの制限なし。今のプラグインと同じ。", "No band or bit limit: like today's plug-ins.", 20, 24, 0 },
        { "High-end '08", "制限なし、わずかな揺らぎ（2000年代の高級機）。", "No limits, a faint movement (2000s high-end).", 20, 24, 15 },
        { "Flagship '80s", "帯域 15 kHz・20 bit 相当、ゆっくり揺れる（80年代の高級機）。", "15 kHz band, about 20 bits, slow wander (80s flagship).", 15, 20, 25 },
        { "Early '80s 16-bit", "帯域 16 kHz・16 bit（80年代初頭の米英の機種）。", "16 kHz band, 16 bits (early-80s units).", 16, 16, 0 },
        { "Budget late '80s", "帯域 12 kHz・16 bit（80年代後半の普及型）。", "12 kHz band, 16 bits (late-80s budget units).", 12, 16, 0 },
        { "Late '70s Chorused", "帯域 10 kHz・16 bit・大きく揺れる（70年代末のホール）。", "10 kHz band, 16 bits, strong chorusing (late-70s hall).", 10, 16, 60 },
        { "First-gen '70s", "帯域 8 kHz・12 bit のざらつき（第一世代）。", "8 kHz band, 12-bit grain (first generation).", 8, 12, 10 },
        { "Lo-fi Sampler", "帯域 6 kHz・10 bit。明らかに荒い、効果用。", "6 kHz band, 10 bits: clearly rough, as an effect.", 6, 10, 0 },
    };
    return c;
}
inline float colorAge (float bwKHz, float bits)
{
    const float a = (20.0f - std::min (20.0f, bwKHz)) / 14.0f, b = (24.0f - std::min (24.0f, bits)) / 14.0f;
    return std::min (1.0f, std::max (0.0f, 0.6f * a + 0.4f * b));
}
inline juce::Colour colorAgeColour (float age)          // cool blue -> teal -> amber -> warm red-brown
{
    const juce::Colour stops[] = { juce::Colour (0xff4a86d8), juce::Colour (0xff3fb2a6), juce::Colour (0xffd9a03a), juce::Colour (0xffd0602e), juce::Colour (0xffa8452a) };
    const float x = std::min (0.999f, std::max (0.0f, age)) * 4; const int i = (int) x;
    return stops[i].interpolatedWith (stops[i + 1], x - (float) i);
}
inline const char* colorEra (float age, bool ja)
{
    if (age < 0.05f) return ja ? "現代" : "modern";
    if (age < 0.30f) return ja ? "80年代の高級機" : "80s flagship";
    if (age < 0.55f) return ja ? "80年代" : "1980s";
    if (age < 0.80f) return ja ? "70年代末" : "late 1970s";
    return ja ? "第一世代・ローファイ" : "first generation / lo-fi";
}

// the order the main menu shows them in (starting points / init first, then the reverbs; the ER-only ones: the ER tab's menu)
inline const std::vector<int>& presetOrder()
{
    static const std::vector<int> o = [] { std::vector<int> v; const auto& P = presets(); for (int gr : { 2, 3 }) for (int i = 0; i < (int) P.size(); ++i) if (P[(size_t) i].group == gr) v.push_back (i); return v; }();
    return o;
}
inline int presetStep (int cur, int d)
{
    const auto& o = presetOrder(); const int n = (int) o.size();
    int k = 0; for (int i = 0; i < n; ++i) if (o[(size_t) i] == cur) k = i;
    return cur < 0 ? o[0] : o[(size_t) (((k + d) % n + n) % n)];
}
inline int presetIndex (const char* name)
{
    if (name == nullptr) return -1;
    const auto& P = presets();
    for (size_t i = 0; i < P.size(); ++i) if (std::string (P[i].name) == name) return (int) i;
    return -1;
}

inline const std::vector<LessonDef>& lessons()
{
    static const std::vector<LessonDef> l {
        { "1. 直接音とERのバランス", "1. Direct sound and ER",
          "Mix 50% は「直接音 1：ER は部屋のとおり」です。0% で直接音だけ、100% で ER だけ。まず 50% と 100% を切り替えて、ERだけの音と、直接音に混ざったときの「部屋の中にいる感じ」を比べます。",
          "Mix 50% plays the direct sound at unity with the ER at the room's own level. 0% is dry, 100% is ER only. Switch between 50% and 100% to hear the ER alone and the 'in the room' feel when it is mixed in.",
          "Live Room (Wood)", { { "mix", 50 } } },
        { "2. First（ITDG）で距離と大きさ", "2. First (ITDG): distance and size",
          "ALGO の First を 2 ms → 20 ms に動かします。直接音から最初の反射までの空白（ITDG）で、脳は壁までの距離と部屋の大きさを推定します。短いと狭く近く、長いと広く奥に聞こえます。",
          "Move First from 2 ms to 20 ms. The gap between the direct sound and the first reflection (ITDG) tells the brain how far the walls are. Short = small and close, long = big and deep.",
          nullptr, { { "src", 1 }, { "first", 2 }, { "length", 40 }, { "taps", 24 }, { "energy", -8 }, { "decay", -12 }, { "mix", 50 } } },
        { "3. 1 ms 帯はコムフィルター", "3. Around 1 ms: comb filtering",
          "反射 4 本、First 1 ms。反射は「音」として聞こえず、直接音と干渉して櫛形の音色変化になります。First を 1 → 5 → 15 ms と上げ、音色の変化が「響き」に変わる境目を探します。",
          "Four reflections, First 1 ms. They are not heard as reflections but as a comb-filter colour. Raise First 1 -> 5 -> 15 ms and find where colour turns into space.",
          nullptr, { { "src", 1 }, { "first", 1 }, { "length", 9 }, { "taps", 4 }, { "growth", 0 }, { "energy", -3 }, { "decay", -3 }, { "damp", 16000 }, { "width", 0 }, { "mix", 50 } } },
        { "4. 先行音効果", "4. The precedence effect",
          "Width 150% で反射を左右に強く振っても、音像は直接音の位置（中央）に残ります。10〜30 ms の反射は方向ではなく「広がり」として聞こえます。First を 50 ms 以上にすると、反射が別の音（エコー）として分離し始めます。",
          "Even with Width at 150%, the image stays where the direct sound is. Reflections at 10-30 ms add width, not direction. Push First beyond 50 ms and they split off as echoes.",
          nullptr, { { "src", 1 }, { "first", 15 }, { "length", 40 }, { "taps", 6 }, { "growth", 0 }, { "energy", -4 }, { "decay", -6 }, { "width", 150 }, { "mix", 50 } } },
        { "5. Density と Diffusion", "5. Density and Diffusion",
          "反射 8 本・拡散 0% では、一つずつの反射が聞こえて金属的です。Reflections を 80 本に、次に Diffusion を 70% に上げます。本数を増やすのと、拡散で反射をにじませるのは似て非なる操作です。",
          "Eight reflections and no diffusion: each one is audible and metallic. Raise Reflections to 80, then Diffusion to 70%. More reflections and smearing them are not the same thing.",
          nullptr, { { "src", 1 }, { "first", 8 }, { "length", 70 }, { "taps", 8 }, { "growth", 0.5f }, { "energy", -6 }, { "decay", -10 }, { "diffusion", 0 }, { "mix", 50 } } },
        { "6. Envelope：減衰・平ら・ビルド", "6. Envelope: decay, flat, build",
          "Envelope は ER の終わりまでの音量変化です。−30 dB で自然な部屋、0 dB でノンリニア（平ら）、+18 dB でリバース型。Growth（後半に反射が集まる度合い）と組み合わせると、ビンテージ機のさまざまな立ち上がりが作れます。",
          "Envelope is the level change across the ER. -30 dB = a natural room, 0 dB = non-linear (flat), +18 dB = reverse. Combined with Growth it gives the many build-ups of vintage units.",
          nullptr, { { "src", 1 }, { "first", 5 }, { "length", 120 }, { "taps", 60 }, { "growth", 0.5f }, { "energy", -5 }, { "decay", -30 }, { "diffusion", 60 }, { "mix", 50 } } },
        { "7. 材質と Damping", "7. Materials and damping",
          "同じ部屋の壁を「タイル」→「カーテン」に替えます。反射の数は同じでも、吸音で音量と高域が下がります。エコーグラムの後半の色（高域）と、Sabine の RT60 の読み取り値を見比べます。",
          "Change the walls of the same room from Tile to Curtain. The same reflections, but absorption lowers their level and treble. Compare the late part of the echogram and the Sabine RT60.",
          nullptr, { { "src", 0 }, { "roomW", 5 }, { "roomD", 6 }, { "roomH", 3 }, { "wall", 0 }, { "floor", 0 }, { "order", 4 }, { "mix", 50 } } },
        { "8. 音源とリスナーの位置", "8. Source and listener position",
          "部屋の図で ▲（音源）と ●（リスナー）をドラッグします。壁に近づけると初回反射が早く強くなり、部屋の中央では反射が遅れて均等になります。マイク位置を決めるときと同じ考え方です。",
          "Drag the triangle (source) and the dot (listener) in the room view. Near a wall the first reflection is early and strong; in the middle they arrive later and evenly. The same thinking as placing a mic.",
          "Live Room (Wood)", { { "mix", 50 } } },
        { "9. Width とモノ互換", "9. Width and mono",
          "Width を 0 → 150% に。広げるほど包まれますが、モノにすると左右の反射が打ち消し合ったり、櫛形の音色が出ます。100% 前後が実用の上限の目安です。",
          "Width from 0 to 150%. Wider is more enveloping, but in mono the reflections can cancel or comb. Around 100% is a practical ceiling.",
          nullptr, { { "src", 1 }, { "first", 6 }, { "length", 50 }, { "taps", 30 }, { "energy", -6 }, { "width", 0 }, { "mix", 50 } } },
        { "10. ビンテージ・デジタルの色", "10. The colour of vintage digital",
          "Bandwidth（帯域）、Bits（量子化）、Wander（タップの揺らぎ）は昔のデジタル機の個性です。8 kHz・12 bit で第一世代、15 kHz・20 bit・少しの揺らぎで80年代の高級機の手触りになります。",
          "Bandwidth, Bits and Wander are what made old digital units sound the way they do. 8 kHz / 12 bit for the first generation; 15 kHz / 20 bit with a little wander for an 80s flagship.",
          "First-gen Digital '70s", { { "mix", 50 } } },
        { "11. IR から ER を作る", "11. Making an ER from an IR",
          "LOAD IR で手持ちの IR を読み込むと、反射を拾ってタップにします（IR Taps）。Convolution はそのままの ER を鳴らす答え合わせ用です。Fit → Pattern で、9 つのパラメーターに翻訳して編集できる形にします。紫がターゲット、自作が上に重なります。",
          "LOAD IR picks the reflections of your IR and turns them into taps (IR Taps). Convolution plays the ER as it is, for reference. Fit -> Pattern translates it into the nine parameters so you can edit it. Violet is the target.",
          nullptr, { { "src", 2 }, { "mix", 50 } } },
        { "12. ER とテールの関係", "12. ER and tail",
          "Tail Preview をオンにすると、ER を入力にした簡易テールが鳴ります（あとで作るテール用プラグインの試作）。ERの形を変えると、テールの立ち上がり方も変わることを確認します。",
          "Turn Tail Preview on: a simple tail fed by the ER (a sketch of the tail plug-in to come). Change the ER and hear how the tail's onset changes with it.",
          "Live Room (Wood)", { { "tail", 1 }, { "tailRT", 1.6f }, { "tailLvl", -8 }, { "mix", 50 } } },
        { "13. 実機パネルと中身", "13. The faceplate and what is inside",
          "80年代の高級機のホールをパネルで操作します。SHAPE を動かすと Envelope と Growth が、SPREAD を動かすと Length が変わります。下の ALGO の値と、エコーグラムの破線（変更前）を見比べ、実機のつまみが ER のどこを動かしているかを確かめます。",
          "Play the 80s flagship hall from its faceplate. SHAPE moves Envelope and Growth, SPREAD moves Length. Compare the ALGO values below and the dashed line (before the change) in the echogram to see what each hardware control really moves.",
          "Digital Hall '80s", { { "mix", 50 } } },
        { "14. スペクトログラムとウォーターフォール", "14. Spectrogram and waterfall",
          "表示を SPECTRO に切り替えると、時間（横）と周波数（縦）で ER のエネルギーが見えます。Damping を下げると後ろほど上の帯域が暗くなり、Bandwidth を下げると上が一律に切れます。WATERFALL は同じ情報を、時間をずらしたスペクトルの重なりで見せます（後ろの山ほど遅い時刻）。",
          "Switch the view to SPECTRO to see the ER's energy over time (across) and frequency (up). Lower Damping darkens the top more towards the end; lower Bandwidth cuts the top evenly. WATERFALL shows the same as spectra stacked in time (further back = later).",
          "Concrete Echo Chamber", { { "mix", 50 } } },
        // ---------------- 0.7.0: the whole reverb ----------------
        { "15. ER と TAIL：2 つの部品", "15. ER and TAIL: the two parts",
          "リバーブは「初期反射（ER）」と「後部残響（TAIL）」の 2 つでできています。OUTPUT の ER／TAIL の On を片方ずつ切って聴き比べます。ER だけ＝距離と部屋の形、TAIL だけ＝長さと空気。エコーグラムで、ER の粒がどこで終わり、テールの霧がどこから始まるか（テール開始の線）を確かめます。",
          "A reverb is two parts: the early reflections (ER) and the late reverb (TAIL). Switch ER / TAIL On in OUTPUT one at a time. ER alone = distance and the room's shape; TAIL alone = length and air. In the echogram, see where the ER's grains end and the tail's haze begins (the tail-start line).",
          "Concert Hall '10s (IR fit)", { { "mix", 50 } } },
        { "16. Decay と周波数ごとの長さ", "16. Decay and length per frequency",
          "Decay は 1 kHz の残響時間。モニターの青い線（RT）の点で、低域・高域の長さを倍率で変えます。高域（点 5）を 0.4 にすると、時間とともに暗くなる「本物の部屋」らしさが出ます。1.0（平ら）と聴き比べ、測定の T30 が線どおりになっているかも見ます。",
          "Decay is the reverb time at 1 kHz. The points on the blue line (RT) scale the lows and highs. Set the top point to 0.4: the sound darkens as it decays, like a real room. Compare with 1.0 (flat), and check that the measured T30 follows the line.",
          "Flat (Init)", { { "tDecay", 2.4f }, { "mix", 50 } } },
        { "17. ビルドアップ：Spread と Shape", "17. Build-up: Spread and Shape",
          "TAIL の Spread を 0 → 120 ms にすると、テールが一気に立ち上がらず、少し遅れて膨らみます。Shape を＋にすると後ろほど大きく（ふわっと）、−にすると先が大きく（すぐ鳴る）。70〜80 年代のデジタルホールの「後ろから回り込む」感じはここ。測定の EDT（初期の減衰）が長くなり、C80（明瞭度）が下がるのを確かめます。",
          "Raise the tail's Spread from 0 to 120 ms: the tail no longer starts at once but swells in. Shape + = louder towards the end (a soft swell), - = loud first. The 'wrapping in from behind' of the '70s-'80s digital halls lives here. Watch the measured EDT get longer and C80 (clarity) drop.",
          "Large Hall (Build-up)", { { "mix", 50 } } },
        { "18. Bass Width とモノ互換", "18. Bass Width and mono",
          "テールの 250 Hz 以下の広がり。100 % と 0 % を切り替え、OUTPUT を Mono にしても聴きます。低域が左右でばらばらだと、モノで低音が痩せたり、スピーカーで位置がぼやけます。実際の部屋も低域ほど左右がそろっています。",
          "The tail's width below 250 Hz. Switch 100 % and 0 %, and listen with OUTPUT on Mono too. Lows that differ left and right thin out in mono and blur on speakers. Real rooms are more alike left and right in the lows.",
          "Mix Bus Glue Room", { { "mix", 40 } } },
        { "19. テールのエンジン 4 種", "19. The four tail engines",
          "TAIL の Engine を Room/Hall（FDN 16 本）→ Plate → Spring → Ambient と切り替えます。同じ Decay でも、立ち上がりの速さ、密度、色（プレートの明るさ、スプリングの「ビヨーン」、アンビエントの巨大さ）が違います。エコーグラムとウォーターフォールで形の違いも見ます。",
          "Switch the tail engine: Room/Hall (16-line FDN) -> Plate -> Spring -> Ambient. With the same Decay the onset, density and colour differ (the plate's brightness, the spring's boing, the ambient's size). Look at the echogram and waterfall too.",
          "Vocal Plate", { { "mix", 45 } } },
        { "20. ダッキング（Single）", "20. Ducking (single)",
          "DYN タブ。ボーカルが鳴っている間だけ残響を 6 dB 下げ、フレーズの隙間で戻します。Depth を 0 と 6 dB で比べると、言葉の明瞭さが変わります。Release は戻る速さ：テンポの 8 分（120 BPM で 250 ms）あたりが自然。メーターで下がる量が見えます。",
          "DYN tab. The reverb dips 6 dB while the vocal sings and comes back in the gaps. Compare Depth 0 and 6 dB: the words get clearer. Release is how fast it returns: about an eighth note (250 ms at 120 BPM) sounds natural. The meter shows how much it dips.",
          "Vocal Plate (Ducked)", { { "mix", 35 } } },
        { "21. Multi と Spectral のダッキング", "21. Multiband and spectral ducking",
          "Single は全体を一緒に下げますが、Multi は帯域ごと、Spectral は半オクターブごとに、ドライと重なる帯域だけ下げます。声の帯域（中域）だけが引くので、残響の量感（低域の厚み、高域の空気）は残ります。Single → Multi → Spectral と切り替え、メーターの帯域の動きを見ます。",
          "Single turns everything down together; Multi works per band and Spectral per half-octave, lowering only where the dry overlaps. Only the voice's range steps back, so the reverb keeps its body and air. Switch Single -> Multi -> Spectral and watch the meter's bands.",
          "Vocal Hall (Spectral Duck)", { { "mix", 35 } } },
        { "22. ゲートリバーブ", "22. Gated reverb",
          "80 年代のスネアの音：長い部屋の残響を、ゲートで 300 ms 前後でバッサリ切ります。Hold が「鳴っている長さ」、Release が「切れ方」。Gate を Off にして元の長い残響と比べ、Hold を 150〜500 ms で動かして曲のテンポに合う長さを探します。",
          "The '80s snare: a long room cut abruptly by a gate after about 300 ms. Hold is how long it rings, Release how it ends. Turn the gate off to hear the long reverb underneath, then move Hold from 150 to 500 ms to fit the tempo.",
          "Gated Snare '80s", { { "mix", 45 } } },
        { "23. コーラスと 80 年代の質感", "23. Chorus and the '80s texture",
          "MOD タブの Chorus を 0 と 45 % で比べます。残響が揺れて左右に広がり、80 年代のデジタルリバーブらしいきらびやかさが出ます。Depth を上げるとピッチの揺れがはっきり聞こえます。上の帯域を 8 kHz で丸めているのも当時の質感です。",
          "Compare the MOD tab's Chorus at 0 and 45 %. The reverb moves and widens: the shimmer of '80s digital reverbs. Raise Depth to hear the pitch move. The top rounded at 8 kHz is part of that era's sound too.",
          "Chorused Hall '80s", { { "mix", 45 } } },
        { "24. LFO・エンベロープ・マトリクス", "24. LFOs, envelope and the matrix",
          "MOD タブのマトリクスで、ソース（LFO 1・LFO 2・エンベロープ）を行き先へ送ります。このプリセットは「強く弾くと残響が短くなる」（Env → Decay −40 %）と「広がりがゆっくり呼吸する」（LFO 2 → Width）。右のスコープでソースの動き、スロットの下のバーで今の量が見えます。",
          "The MOD tab's matrix sends sources (LFO 1, LFO 2, envelope) to destinations. This preset makes 'play harder = shorter reverb' (Env -> Decay -40 %) and 'the width breathes slowly' (LFO 2 -> Width). The scope shows the sources, the bars under the slots the amounts now.",
          "Breathing Pad Space", { { "mix", 45 } } },
        { "25. テンポに同期した変調", "25. Tempo-synced modulation",
          "LFO 1 の Sync を 1/8 にすると、曲のテンポに合わせて動きます（再生中は小節の位置にも合わせる）。このプリセットは矩形の LFO でテールの音量を刻むので、長い残響がリズムになります。1/4・1/16・3 連符に変えて、曲に合う刻みを探します。",
          "Set LFO 1's Sync to 1/8 and it follows the song's tempo (and the bar position while playing). Here a square LFO chops the tail's level, turning a long reverb into rhythm. Try 1/4, 1/16 and triplets.",
          "Tempo Pulse Space", { { "mix", 50 } } },
        { "26. IR からアルゴリズムへ（T30・EDT・C80）", "26. From an IR to an algorithm (T30, EDT, C80)",
          "このプリセットは、IR とプラグインの IR を鳴らして比べ、約 350 回パラメーターを動かして合わせたもの。合わせた数値：T30（−5〜−35 dB の減衰から出す残響時間）、EDT（最初の 10 dB の減衰、聞こえの長さに近い）、C80（最初の 80 ms と後の比、明瞭さ）、0〜400 ms の大きさの変化、音色、左右の広がり。IR読込のメニュー「いまの IR を高精度に再現」で自分の IR でも試せます。",
          "This preset was made by rendering the plugin's IR, comparing it with an IR and moving the settings about 350 times. What was matched: T30 (reverb time from the -5 to -35 dB decay), EDT (the first 10 dB, close to the length you hear), C80 (the first 80 ms against the rest: clarity), the level over 0-400 ms, tone and width. Try it on your own IR: Load IR menu, 'Rebuild this IR precisely'.",
          "Digital Hall '78 (IR fit)", { { "mix", 45 } } },
        { "27. 時代ごとの響きを聴き比べる", "27. Comparing the eras",
          "IR から合わせた 4 つを順に読み込みます：Digital Hall '78（ゆっくり膨らむ・暗い）→ Random Hall '86（まばらな ER から密に・上が急に落ちる）→ Wide Hall '00s（低域が長く、とても広い）→ Modern Chamber '10s（すぐ密・周波数ごとの長さがほぼ平ら）。測定の T30 と EDT の線、Spread と高域の RT の点の違いが、時代の個性です。",
          "Load the four IR fits in turn: Digital Hall '78 (slow swell, dark) -> Random Hall '86 (sparse ER then dense, the top falls fast) -> Wide Hall '00s (long lows, very wide) -> Modern Chamber '10s (dense at once, nearly flat per frequency). The differences in the measured T30 / EDT lines, Spread and the high RT point are each era's character.",
          "Random Hall '86 (IR fit)", { { "mix", 45 } } },
        { "28. 帯域ごとのビルドアップ：Spread High", "28. Build-up per band: Spread High",
          "教会やホールでは、高域は早く立ち上がり、低域は遅れて膨らみます。TAIL の Spread High は、約 1.5 kHz 以上のビルドアップの長さを Spread に対する % で決めます。100 % ＝低域と同じ。50 % にすると高域が先に、200 % にすると高域が遅れて膨らみます。エコーグラムとモニターの帯域ごとの EDT で、低域と高域の立ち上がりの差を見ます。IR から合わせたプリセット（Church '10s など）は、この値で帯域の差を再現しています。",
          "In churches and halls the highs arrive early and the lows swell later. Spread High sets the build-up length above about 1.5 kHz as a % of Spread: 100 % = the same as the lows, 50 % = the highs come first, 200 % = the highs swell later. See the difference in the echogram and in the per-band EDT of the monitor. The IR-fit presets (Church '10s and others) use it to reproduce the difference between the bands.",
          "Church '10s (IR fit)", { { "mix", 40 } } },
    };
    return l;
}
