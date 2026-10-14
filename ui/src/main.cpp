// Vision -- Gaddag move explorer / Player-vs-Greedy / Greedy-vs-Greedy
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Letter.h"
#include "Board.h"
#include "Move.h"
#include "Tray.h"
#include "BoardOps.h"
#include "CrossChecks.h"
#include "Gaddag.h"
#include "TileBag.h"
#include "MoveGen.h"
#include "MoveEval.h"
#include "Simulation.h"
#include "BotSearchPanel.h"

extern const unsigned char kWordListData[];
extern const std::size_t kWordListSize;
extern const unsigned char kFontMonoData[];
extern const std::size_t kFontMonoSize;
extern const unsigned char kFontFranklinData[];
extern const std::size_t kFontFranklinSize;

static ImFont* g_fontMono = nullptr;
static ImFont* g_fontFranklin = nullptr;

namespace fs = std::filesystem;

// ================================================================ constants
static constexpr int      kRackSize         = 7;
// Must match the bingo bonus used by MoveEval (NYT Crossplay: 40).
static constexpr int      kBingoBonus       = 40;
static constexpr float    kCell             = 30.0f;
static constexpr float    kGap              = 3.0f;
static constexpr float    kCellRound        = 6.0f;    // corner radius of board squares
static constexpr uint64_t kExplorationSeed  = 12345;
static constexpr int      kBotWaitFrames    = 3;       // lets "BOT'S TURN" render before the bot moves

// ================================================================ palette
static const ImU32 COL_ACCENT          = IM_COL32(107, 165, 219, 255);
static const ImU32 COL_TEXT            = IM_COL32(235, 235, 240, 255);

static const ImU32 COL_BOARD_EMPTY     = IM_COL32(242, 238, 237, 255);
static const ImU32 COL_CENTER          = IM_COL32( 35,  35,  35, 255);
static const ImU32 COL_TILE            = IM_COL32( 43,  84, 146, 255);
static const ImU32 COL_RACK_SHELF      = IM_COL32(206, 204, 208, 255);
static const ImU32 COL_TILE_TEXT       = IM_COL32(255, 255, 255, 255);

// Premium squares (sampled from the reference screenshot).
static const ImU32 COL_2L              = IM_COL32(240, 235, 205, 255);
static const ImU32 COL_2L_TEXT         = IM_COL32(122,  92,  41, 255);
static const ImU32 COL_3L              = IM_COL32(223, 231, 208, 255);
static const ImU32 COL_3L_TEXT         = IM_COL32( 88, 102,  55, 255);
static const ImU32 COL_2W              = IM_COL32(217, 225, 238, 255);
static const ImU32 COL_2W_TEXT         = IM_COL32( 72,  94, 139, 255);
static const ImU32 COL_3W              = IM_COL32(231, 221, 232, 255);
static const ImU32 COL_3W_TEXT         = IM_COL32(156,  35, 171, 255);

static const ImU32 COL_HEAT_0          = IM_COL32(232,  92,  92, 200);
static const ImU32 COL_HEAT_LOW        = IM_COL32(240, 179,  92, 200);
static const ImU32 COL_HEAT_MID        = IM_COL32(244, 227, 138, 200);
static const ImU32 COL_HEAT_HI         = IM_COL32(168, 216, 168, 200);

static const ImVec4 PANEL_BG           = ImVec4(0.98f, 0.976f, 0.965f, 1.0f);
static const ImVec4 PANEL_TEXT         = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
static const ImVec4 PANEL_TEXT_DIM     = ImVec4(0.45f, 0.45f, 0.50f, 1.0f);
static const ImVec4 PANEL_SEP          = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

static constexpr ImGuiWindowFlags PANEL_FLAGS =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

// ================================================================ small drawing helpers
static int LetterPoints(Letter l) { return POINTS_PER_TILE[toIndex(l)]; }

// The default ImGui font has no bold weight, so fake one with two extra offset passes.
static void AddBoldText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s) {
    const float o = std::max(1.0f, size * 0.035f);
    dl->AddText(font, size, p, col, s);
    dl->AddText(font, size, ImVec2(p.x + o, p.y), col, s);
    dl->AddText(font, size, ImVec2(p.x, p.y + o), col, s);
}

// Heavier fake-bold used for tile letters and premium square labels so they match
// the reference tile style. 5 offset passes vs AddBoldText's 3.
static void AddHeavyText(ImDrawList* dl, ImFont* font, float size, ImVec2 p, ImU32 col, const char* s) {
    const float o = std::max(1.0f, size * 0.06f);
    dl->AddText(font, size, p, col, s);
    dl->AddText(font, size, ImVec2(p.x + o, p.y), col, s);
    dl->AddText(font, size, ImVec2(p.x, p.y + o), col, s);
    dl->AddText(font, size, ImVec2(p.x + o * 0.5f, p.y + o * 0.5f), col, s);
    dl->AddText(font, size, ImVec2(p.x + o, p.y + o), col, s);
}

// Tile silhouette used everywhere: top-left and bottom-right are the most rounded corners.
static void DrawTileBody(ImDrawList* dl, ImVec2 p0, float size, ImU32 col) {
    const ImVec2 p1(p0.x + size, p0.y + size);
    const float rTL = size * 0.15f, rTR = size * 0.06f, rBR = size * 0.25f, rBL = size * 0.10f;
    dl->PathArcTo(ImVec2(p0.x + rTL, p0.y + rTL), rTL, IM_PI,        IM_PI * 1.5f);
    dl->PathArcTo(ImVec2(p1.x - rTR, p0.y + rTR), rTR, IM_PI * 1.5f, IM_PI * 2.0f);
    dl->PathArcTo(ImVec2(p1.x - rBR, p1.y - rBR), rBR, 0.0f,         IM_PI * 0.5f);
    dl->PathArcTo(ImVec2(p0.x + rBL, p1.y - rBL), rBL, IM_PI * 0.5f, IM_PI);
    dl->PathFillConvex(col);
}

// Letter bottom-left, point value as a superscript top-right. Everything scales with `cell`.
// The letter itself is drawn with a heavier stroke than AddBoldText's default so it
// matches the reference tile style; the point value keeps the standard weight.
static void DrawTileGlyph(ImDrawList* dl, ImVec2 p0, float cell,
                          char ch, int pts, bool showPts) {
    ImGui::PushFont(g_fontFranklin);
    ImFont* font = ImGui::GetFont();
    const float letterSize = cell * 0.60f;
    const char buf[2] = { ch, 0 };
    const ImVec2 ts = font->CalcTextSizeA(letterSize, 10000.0f, 0.0f, buf);
    const ImVec2 lp(p0.x + cell * 0.17f, p0.y + cell - ts.y - cell * 0.08f);
    const ImU32 ink = showPts ? COL_TILE_TEXT : IM_COL32(168, 190, 240, 255);
    if (ch != ' ') AddHeavyText(dl, font, letterSize, lp, ink, buf);

    if (showPts && pts > 0) {
        char pb[8]; std::snprintf(pb, sizeof pb, "%d", pts);
        const float ptSize = cell * 0.30f;
        const ImVec2 ps = font->CalcTextSizeA(ptSize, 10000.0f, 0.0f, pb);
        AddBoldText(dl, font, ptSize,
                    ImVec2(p0.x + cell - ps.x - cell * 0.10f, p0.y + cell * 0.06f),
                    COL_TILE_TEXT, pb);
    } else {
        const char zb[4] = "0";
        const float ptSize = cell * 0.30f;
        const ImVec2 zs = font->CalcTextSizeA(ptSize, 10000.0f, 0.0f, zb);
        AddBoldText(dl, font, ptSize,
                    ImVec2(p0.x + cell - zs.x - cell * 0.10f, p0.y + cell * 0.06f),
                    ink, zb);
    }
    ImGui::PopFont();
}

// Five-point star (concave, so drawn as a triangle fan) for the center square.
static void DrawStar(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
    ImVec2 pt[10];
    for (int i = 0; i < 10; ++i) {
        const float a  = -IM_PI * 0.5f + i * (IM_PI / 5.0f);
        const float rr = (i % 2 == 0) ? r : r * 0.42f;
        pt[i] = ImVec2(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr);
    }
    for (int i = 0; i < 10; ++i) dl->AddTriangleFilled(c, pt[i], pt[(i + 1) % 10], col);
}

static ImU32 PremiumFillNyt(Mult m) {
    switch (m) {
        case MULT_DL: return COL_2L;
        case MULT_TL: return COL_3L;
        case MULT_DW: return COL_2W;
        case MULT_TW: return COL_3W;
        default:      return 0;
    }
}
static ImU32 PremiumTextNyt(Mult m) {
    switch (m) {
        case MULT_DL: return COL_2L_TEXT;
        case MULT_TL: return COL_3L_TEXT;
        case MULT_DW: return COL_2W_TEXT;
        case MULT_TW: return COL_3W_TEXT;
        default:      return COL_TEXT;
    }
}
static const char* PremiumLabelNyt(Mult m) {
    switch (m) {
        case MULT_DL: return "2L";
        case MULT_TL: return "3L";
        case MULT_DW: return "2W";
        case MULT_TW: return "3W";
        default:      return "";
    }
}
static ImU32 HeatFill(int n) {
    if (n == 0)       return COL_HEAT_0;
    if (n <= 5)       return COL_HEAT_LOW;
    if (n <= 15)      return COL_HEAT_MID;
    if (n <= 25)      return COL_HEAT_HI;
    return COL_BOARD_EMPTY;   // 26 = no constraint
}

static void DrawSwatch(ImU32 color, const char* label) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + h, p.y + h), color);
    ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + h, p.y + h), IM_COL32(120,120,130,255));
    ImGui::Dummy(ImVec2(h, h)); ImGui::SameLine(); ImGui::TextUnformatted(label);
}

// Popups are centered when they appear (so the user can still drag them aside);
// only fixed windows such as the title card pass ImGuiCond_Always.
static void CenterNextWindow(ImGuiCond cond = ImGuiCond_Appearing) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, cond, ImVec2(0.5f, 0.5f));
}

// Floating windows must never dock into the locked layout.
static constexpr ImGuiWindowFlags POPUP_FLAGS =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;

// ================================================================ dock helpers
static void LockDockTree(ImGuiDockNode* n) {
    if (!n) return;
    // Public and private flag enums are different types; combine them as plain ints.
    n->LocalFlags |= (ImGuiDockNodeFlags)((int)ImGuiDockNodeFlags_NoDockingSplit |
                                          (int)ImGuiDockNodeFlags_NoDockingOverMe |
                                          (int)ImGuiDockNodeFlags_NoResize |
                                          (int)ImGuiDockNodeFlags_NoTabBar);
    LockDockTree(n->ChildNodes[0]);
    LockDockTree(n->ChildNodes[1]);
}
static void FinishAndLock(ImGuiID root) {
    ImGui::DockBuilderFinish(root);
    LockDockTree(ImGui::DockBuilderGetNode(root));
}

enum class AppMode { Title, Exploration, PlayerVsBot, GreedyVsGreedy };
static AppMode g_mode = AppMode::Title;

// Dock IDs must not depend on the current window's ID stack: the dockspace is
// submitted inside "##DockHost" but the builders run outside any window, so
// ImGui::GetID() would hash differently in the two places.
static ImGuiID DockIdFor(AppMode m) {
    switch (m) {
        case AppMode::Exploration:    return ImHashStr("Dock_Exploration");
        case AppMode::PlayerVsBot:    return ImHashStr("Dock_PvG");
        case AppMode::GreedyVsGreedy: return ImHashStr("Dock_GvG");
        default:                      return ImHashStr("Dock_Title");
    }
}

static const char* kTopEx  = "Vision -- Exploration##top";
static const char* kTopPvb = "Vision -- PvG##top";
static const char* kTopSim = "Vision -- GvG##top";

// Fixed dock layout: top strip across the width, legend on the left (fixed px),
// cream body in the middle (flex), right panel on the right (fixed px).
static void BuildDock(ImGuiID dockId, const char* topName,
                      const char* creamName, const char* rightName,
                      float legendW, float rightW) {
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);

    const float totalW = ImGui::GetMainViewport()->WorkSize.x;

    ImGuiID body;
    ImGuiID topStrip = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Up, 0.07f, nullptr, &body);

    ImGuiID rest;
    const float legendFrac = std::clamp(legendW / std::max(1.0f, totalW), 0.05f, 0.5f);
    ImGuiID legend = ImGui::DockBuilderSplitNode(body, ImGuiDir_Left, legendFrac, nullptr, &rest);

    ImGuiID rightCol;
    const float rightFrac = std::clamp(rightW / std::max(1.0f, totalW), 0.05f, 0.5f);
    ImGuiID middle = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Left, 1.0f - rightFrac, nullptr, &rightCol);

    ImGui::DockBuilderDockWindow(topName, topStrip);
    ImGui::DockBuilderDockWindow("Legend##legend", legend);
    ImGui::DockBuilderDockWindow(creamName, middle);
    ImGui::DockBuilderDockWindow(rightName, rightCol);

    FinishAndLock(dockId);
}

// ================================================================ dictionary helper
// One GADDAG path per split in [0, n-1]. The probe at split == n is not used: it would
// consume all n letters and then test the last letter's bit, which can never be a valid
// encoding (and could false-positive).
static bool TrySplit(const Gaddag& g, const std::string& w, size_t split) {
    const size_t n = w.size();
    const Arc* a = &g.get_init();
    for (size_t i = split; i-- > 0; ) {
        a = g.next_arc(*a, w[i]);
        if (!a) return false;
    }
    a = g.step(*a, SEPARATOR_INDEX);
    if (!a) return false;
    for (size_t i = split; i + 1 < n; ++i) {
        a = g.next_arc(*a, w[i]);
        if (!a) return false;
    }
    const uint32_t bit = 1u << (w[n - 1] - 'A');
    return (a->letter_set_mask & bit) != 0;
}
static bool GaddagContains(const Gaddag& g, const std::string& word) {
    if (word.empty()) return false;
    for (char ch : word)
        if (ch < 'A' || ch > 'Z') return false;
    for (size_t s = 0; s < word.size(); ++s)
        if (TrySplit(g, word, s)) return true;
    return false;
}

static Gaddag* g_gaddag = nullptr;   // owned by main()

// ================================================================ state types
using BagCounts = std::array<uint8_t, NUM_LETTER_SLOTS>;

struct Snapshot {
    Board      board{};
    Tray       trayA{};
    Tray       trayB{};
    std::array<Letter, TOTAL_TILES> bagTiles{};
    uint16_t   bagTotal = 0;
    BagCounts  bagCounts{};
    std::mt19937_64 bagRng{};
    int        score[2] = {0, 0};
};
struct Entry : Snapshot {
    std::string label;
    int turnNumber = 0;
    int player = 0;      // 0/1 for PvB and Sim, -1 for Exploration
};
struct InvalidRun { int r0, c0, r1, c1; bool descending; std::string word; };

struct ModeState {
    Board       board{};
    AnchorLines anchors{};
    Tray        trayA{};
    Tray        trayB{};
    std::unique_ptr<TileBag> bag;
    int         score[2] = {0, 0};

    Snapshot            initial;
    std::vector<Entry>  history;
    int                 cursor = 0;   // number of entries currently applied

    std::vector<InvalidRun> invalidRuns;
    bool                    boardValid = true;

    bool atEnd() const { return cursor == (int)history.size(); }
    const Snapshot& turnStart() const {
        if (cursor == 0 || history.empty()) return initial;
        return history[cursor - 1];
    }
};

static ModeState g_ex;
static ModeState g_pvb;
static ModeState g_simState;
static BotPanel  g_bot;

static ModeState& StateForMode(AppMode m) {
    return m == AppMode::Exploration ? g_ex :
           m == AppMode::PlayerVsBot ? g_pvb : g_simState;
}

// ================================================================ shared UI state
enum class ConfirmAction { Title, Rematch };
enum class DragSource { None, Tray, Board };

struct DragState {
    DragSource src = DragSource::None;
    Letter     letter = Letter::A;
    bool       isBlank = false;
    int        fromR = 0, fromC = 0;
};

struct UiState {
    std::string lastAction = "(no action yet)";
    bool dockBuiltEx = false, dockBuiltPvb = false, dockBuiltSim = false;
    bool howToPlayOpen = false;

    bool          confirmBackOpen = false;
    ConfirmAction confirmAction = ConfirmAction::Title;

    DragState drag;
    bool      dragging = false;

    bool       blankPopupOpen = false;
    ModeState* blankMs = nullptr;
    int        blankDropR = 0, blankDropC = 0;

    bool addTilePopupOpen = false;

    bool        savePopupOpen = false;
    bool        loadPopupOpen = false;
    char        saveName[64] = "session";
    std::string saveMessage;
    std::string loadMessage;
    std::vector<std::string> loadFiles;
    bool        loadListDirty = true;

    float lastViewportW = 0.0f;
    float lastViewportH = 0.0f;
};

static UiState g_ui;

static void EndDrag() { g_ui.dragging = false; g_ui.drag.src = DragSource::None; }

// Board hover state, captured inside the board child window and consumed by the tooltip
// that is drawn outside it (see DrawBoardHoverTooltip).
static int  g_boardHoverR  = -1;
static int  g_boardHoverC  = -1;
static bool g_boardHoverOk = false;

// PvB turn state
struct PvbUiState {
    bool firstPlayerDialog = false;
    int  turn = 0;                    // whose turn: 0 = human, 1 = bot
    int  consecutivePasses = 0;
    bool gameOver = false;
    int  winner = -1;
    int  botWait = 0;
    bool gameOverDismissed = false;   // "Game Over" card closed so the final board can be inspected
};

static PvbUiState g_pvbUi;

// Sim run state
struct SimUiState {
    float  speedMs = 60.0f;
    double lastStep = 0.0;
    bool   autoRunning = false;
    bool   replay = false;        // loaded from a file: view only
    bool   started = false;
    GreedySim sim;
};

static SimUiState g_simUi;

// ================================================================ move-generation panel state
struct ScoredMove { RawMove rm; MoveScore sc; std::string word; std::string upper; };
struct HlCell { int r, c; char ch; };

// Three views for the move list:
//   All    -- every legal placement, no dedupe, no cap.
//   Unique -- best placement per distinct word, capped at topN.
//   TopN   -- topN highest-scoring placements, duplicates allowed.
enum class MoveView { All, Unique, TopN };

struct MoveGenState {
    std::vector<ScoredMove> allMoves;
    std::vector<ScoredMove> uniqueMoves;   // deduped by word, capped at topN, sorted by moveSortMode
    std::vector<ScoredMove> topNMoves;     // topN highest-scoring placements, dupes allowed
    std::vector<int>        filtered;
    std::vector<HlCell>     hlCells;
    bool movesDirty = true;      // filtered list must be rebuilt
    bool movesStale = false;     // board changed since generation
    MoveView view = MoveView::Unique;
    int  hlMoveIdx = -1;
    int  topN = 40;
    int  moveSortMode = 0;
    char moveFilter[64] = "";

    void Invalidate() {
        movesStale = true;
        hlMoveIdx = -1;
        hlCells.clear();
    }
};

static MoveGenState g_mg;

// ================================================================ board scanning / validation
template <class F>
static void ForEachRun(const Board& b, F&& fn) {
    for (int dir = 0; dir < 2; ++dir) {
        const bool desc = (dir == 1);
        for (int line = 0; line < BOARD_LENGTH; ++line) {
            auto at = [&](int k) { return b.letters_row[desc ? sq(k, line) : sq(line, k)]; };
            int i = 0;
            while (i < BOARD_LENGTH) {
                if (at(i) == EMPTY_SQUARE) { ++i; continue; }
                const int start = i;
                std::string w;
                while (i < BOARD_LENGTH && at(i) != EMPTY_SQUARE) {
                    w.push_back(toChar(fromBoardCode(at(i))));
                    ++i;
                }
                if (w.size() >= 2) fn(desc, line, start, i - 1, w);
            }
        }
    }
}

static void ValidateBoardFull(ModeState& ms) {
    ms.invalidRuns.clear();
    ms.boardValid = true;
    if (!g_gaddag) return;
    ForEachRun(ms.board, [&](bool desc, int line, int s, int e, const std::string& w) {
        if (GaddagContains(*g_gaddag, w)) return;
        ms.boardValid = false;
        if (desc) ms.invalidRuns.push_back({ s, line, e, line, true, w });
        else      ms.invalidRuns.push_back({ line, s, line, e, false, w });
    });
}

static bool IsSquareInInvalidRun(const ModeState& ms, int r, int c) {
    for (const auto& run : ms.invalidRuns) {
        if (run.descending) { if (c == run.c0 && r >= run.r0 && r <= run.r1) return true; }
        else                { if (r == run.r0 && c >= run.c0 && c <= run.c1) return true; }
    }
    return false;
}

// True if (r, c) is empty and orthogonally adjacent to a tile in `ref`.
// Used to draw anchor outlines that reflect only the committed position, so staging
// tiles don't shift the cyan outlines while the player is mid-turn.
static bool IsAnchorAgainst(const Board& ref, int r, int c) {
    if (ref.letters_row[sq(r, c)] != EMPTY_SQUARE) return false;
    auto occ = [&](int rr, int cc) {
        return rr >= 0 && cc >= 0 && rr < BOARD_LENGTH && cc < BOARD_LENGTH &&
               ref.letters_row[sq(rr, cc)] != EMPTY_SQUARE;
    };
    // First move: the center square is the only anchor.
    bool boardEmpty = true;
    for (int i = 0; i < NUM_SQUARES && boardEmpty; ++i)
        if (ref.letters_row[i] != EMPTY_SQUARE) boardEmpty = false;
    if (boardEmpty) return r == BOARD_LENGTH / 2 && c == BOARD_LENGTH / 2;
    return occ(r - 1, c) || occ(r + 1, c) || occ(r, c - 1) || occ(r, c + 1);
}

static void FullCrossRecompute(ModeState& ms) {
    if (!g_gaddag) return;
    rebuild(ms.board, ms.anchors, *g_gaddag, g_gaddag->get_init());
}

// Every board mutation made through the UI funnels through here.
static void RefreshDerived(ModeState& ms) {
    FullCrossRecompute(ms);
    ValidateBoardFull(ms);
    if (&ms == &g_ex) g_mg.Invalidate();
}

// ================================================================ placement rules + scoring
static bool BoardsEqual(const Board& a, const Board& b) {
    for (int i = 0; i < NUM_SQUARES; ++i) {
        if (a.letters_row[i] != b.letters_row[i]) return false;
        if (a.is_blank.test(i) != b.is_blank.test(i)) return false;
    }
    return true;
}
static bool TraysEqual(const Tray& a, const Tray& b) {
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        if (a.counts[i] != b.counts[i]) return false;
    return true;
}
// A turn is "uncommitted" when tiles have been placed on the board since the last
// committed position. Tray changes alone (adding tiles, right-click returning a
// tile) don't count as a staged move.
static bool HasUncommitted(const ModeState& ms) {
    return !BoardsEqual(ms.board, ms.turnStart().board);
}

static int TileValueAt(const Board& b, int idx) {
    return b.is_blank.test(idx) ? 0 : LetterPoints(fromBoardCode(b.letters_row[idx]));
}

static void FindRun(const Board& b, int r, int c, int dr, int dc, int& sr, int& sc, int& len) {
    sr = r; sc = c;
    while (sr - dr >= 0 && sc - dc >= 0 &&
           b.letters_row[sq(sr - dr, sc - dc)] != EMPTY_SQUARE) { sr -= dr; sc -= dc; }
    len = 0;
    int rr = sr, cc = sc;
    while (rr < BOARD_LENGTH && cc < BOARD_LENGTH &&
           b.letters_row[sq(rr, cc)] != EMPTY_SQUARE) { ++len; rr += dr; cc += dc; }
}

struct Placement {
    bool ok = false;
    std::string error;
    int points = 0;
    std::string word;
    int row = 0, col = 0;
    bool desc = false;
    int tiles = 0;
};

// Compares `b` against `ref` (the board at the start of the turn). The squares that
// are newly occupied are the tiles being played. Checks geometry/connectivity and
// scores the move; premium squares are applied only to the new tiles.
// Dictionary validity is checked separately (ModeState::boardValid).
static Placement EvaluatePlacement(const Board& b, const Board& ref) {
    Placement p;
    std::bitset<NUM_SQUARES> isNew;
    std::vector<std::pair<int,int>> placed;
    int occupied = 0;
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const int idx = sq(r, c);
            const bool now = b.letters_row[idx] != EMPTY_SQUARE;
            const bool was = ref.letters_row[idx] != EMPTY_SQUARE;
            if (now) ++occupied;
            if (was && !now) { p.error = "A previously committed tile was removed."; return p; }
            if (now && !was) { isNew.set(idx); placed.push_back({r, c}); }
        }
    }
    if (placed.empty()) { p.error = "Place at least one tile."; return p; }

    bool sameRow = true, sameCol = true;
    for (auto& pl : placed) {
        if (pl.first  != placed[0].first)  sameRow = false;
        if (pl.second != placed[0].second) sameCol = false;
    }
    if (!sameRow && !sameCol) { p.error = "New tiles must lie in one row or one column."; return p; }

    bool desc;
    if (placed.size() == 1) {
        int sr, sc, h, v;
        FindRun(b, placed[0].first, placed[0].second, 0, 1, sr, sc, h);
        FindRun(b, placed[0].first, placed[0].second, 1, 0, sr, sc, v);
        if (h < 2 && v < 2) { p.error = "A move must form a word of 2 or more letters."; return p; }
        desc = (h < 2);
    } else {
        desc = sameCol;
    }
    const int dr = desc ? 1 : 0, dc = desc ? 0 : 1;

    int sr, sc, len;
    FindRun(b, placed[0].first, placed[0].second, dr, dc, sr, sc, len);
    for (auto& pl : placed) {
        const bool inRun = desc ? (pl.second == sc && pl.first  >= sr && pl.first  < sr + len)
                                : (pl.first  == sr && pl.second >= sc && pl.second < sc + len);
        if (!inRun) { p.error = "Gap in the new tiles."; return p; }
    }

    bool touchesOld = false;
    auto scoreRun = [&](int r0, int c0, int n, int rdr, int rdc) {
        int sum = 0, wmul = 1;
        for (int k = 0; k < n; ++k) {
            const int r = r0 + k * rdr, c = c0 + k * rdc, idx = sq(r, c);
            int v = TileValueAt(b, idx);
            if (isNew.test(idx)) {
                switch (premiumAt(r, c)) {
                    case MULT_DL: v *= 2; break;
                    case MULT_TL: v *= 3; break;
                    case MULT_DW: wmul *= 2; break;
                    case MULT_TW: wmul *= 3; break;
                    default: break;
                }
            } else touchesOld = true;
            sum += v;
        }
        return sum * wmul;
    };

    int total = scoreRun(sr, sc, len, dr, dc);
    std::string word;
    for (int k = 0; k < len; ++k)
        word.push_back(toChar(fromBoardCode(b.letters_row[sq(sr + k * dr, sc + k * dc)])));

    for (auto& pl : placed) {
        int cr, cc, cl;
        FindRun(b, pl.first, pl.second, dc, dr, cr, cc, cl);   // perpendicular run
        if (cl >= 2) total += scoreRun(cr, cc, cl, dc, dr);
    }

    if (occupied == (int)placed.size()) {   // first move of the game
        const int C = BOARD_LENGTH / 2;
        if (!isNew.test(sq(C, C))) { p.error = "First move must cover the center square."; return p; }
    } else if (!touchesOld) {
        p.error = "Move must connect to existing tiles."; return p;
    }
    if ((int)placed.size() == kRackSize) total += kBingoBonus;

    p.ok = true; p.points = total; p.word = word;
    p.row = sr; p.col = sc; p.desc = desc; p.tiles = (int)placed.size();
    return p;
}

static std::string FormatMoveLabel(const std::string& word, int row, int col, bool desc, int points) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s (%d,%d) %s +%d",
                  word.c_str(), row, col, desc ? "V" : "H", points);
    return buf;
}
static std::string FormatMove(const Placement& p) {
    return FormatMoveLabel(p.word, p.row, p.col, p.desc, p.points);
}

// Places a generated move on the board and removes its tiles from the tray.
// Used by the Exploration "Play Move" button when a move-generation row is
// highlighted. Staged tiles must be empty (caller checks). Assumes g_gaddag
// is loaded and the move is valid for the current board.
static void ApplyHighlightedMove(ModeState& ms, const RawMove& rm) {
    const int dr = rm.is_descending ? 1 : 0;
    const int dc = rm.is_descending ? 0 : 1;
    int r = rm.row, c = rm.col;
    for (int i = 0; i < rm.len; ++i, r += dr, c += dc) {
        const int idx = sq(r, c);
        if (ms.board.letters_row[idx] != EMPTY_SQUARE) continue;   // pre-existing tile
        const bool isBlank = (rm.blank_mask >> i) & 1u;
        const Letter L = fromBoardCode(rm.letters[i]);
        placeTile(ms.board, r, c, L, isBlank);
        ms.trayA.removeTile(isBlank ? Letter::Blank : L);
    }
    RefreshDerived(ms);
    g_ui.lastAction = "Played highlighted move.";
}

// A tile can be dropped on any empty, in-bounds square. Connectivity / center-square
// rules are enforced at commit time by EvaluatePlacement (which gates Play Move).
static bool CanDropTile(const Board& b, int r, int c, std::string& reason) {
    if (r < 0 || c < 0 || r >= BOARD_LENGTH || c >= BOARD_LENGTH) { reason = "Out of bounds."; return false; }
    if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) { reason = "Square already occupied."; return false; }
    return true;
}

// ================================================================ snapshots / history
static std::vector<Letter> TrayLetters(const Tray& t) {
    std::vector<Letter> v;
    v.reserve(kRackSize);
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        for (uint8_t k = 0; k < t.counts[i]; ++k)
            v.push_back(fromIndex((uint8_t)i));
    return v;
}

static Snapshot MakeSnapshot(const Board& board, const Tray& a, const Tray& b,
                             TileBag* bag, int s0, int s1) {
    Snapshot s;
    s.board = board; s.trayA = a; s.trayB = b;
    s.score[0] = s0; s.score[1] = s1;
    if (bag) {
        s.bagTiles  = bag->tilesCopy();
        s.bagTotal  = bag->totalCount();
        s.bagRng    = bag->rngCopy();
        s.bagCounts = bag->counts();
    }
    return s;
}
static Snapshot SnapshotOf(ModeState& ms) {
    return MakeSnapshot(ms.board, ms.trayA, ms.trayB, ms.bag.get(), ms.score[0], ms.score[1]);
}

// Board / trays / score only (no bag, no derived data). Used by the sim viewer.
static void ApplyVisible(ModeState& ms, const Snapshot& s) {
    ms.board = s.board;
    ms.trayA = s.trayA;
    ms.trayB = s.trayB;
    ms.score[0] = s.score[0];
    ms.score[1] = s.score[1];
}
static void ApplySnapshot(ModeState& ms, const Snapshot& s) {
    ApplyVisible(ms, s);
    if (ms.bag) {
        ms.bag->restoreFrom(s.bagTiles, s.bagTotal);
        ms.bag->restoreRng(s.bagRng);
    }
    RefreshDerived(ms);
}

static void PushEntry(ModeState& ms, const std::string& label, int player) {
    if (ms.cursor < (int)ms.history.size())
        ms.history.erase(ms.history.begin() + ms.cursor, ms.history.end());
    Entry e;
    static_cast<Snapshot&>(e) = SnapshotOf(ms);
    e.label = label;
    e.turnNumber = (int)ms.history.size() + 1;
    e.player = player;
    ms.history.push_back(std::move(e));
    ms.cursor = (int)ms.history.size();
}

static void GoToCursor(ModeState& ms, int target) {
    target = std::clamp(target, 0, (int)ms.history.size());
    ms.cursor = target;
    EndDrag();
    g_ui.blankPopupOpen = false;     // a pending blank assignment refers to the position we just left
    if (target == 0) ApplySnapshot(ms, ms.initial);
    else             ApplySnapshot(ms, ms.history[target - 1]);
}

static bool BagReturn(TileBag* bag, Letter L) {
    if (!bag) return false;
    auto tiles = bag->tilesCopy();
    const uint16_t total = bag->totalCount();
    if (total >= TOTAL_TILES) return false;
    tiles[total] = L;
    bag->restoreFrom(tiles, (uint16_t)(total + 1));
    return true;
}

// ================================================================ cream style
static void PushCreamStyle() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, PANEL_BG);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, PANEL_BG);
    ImGui::PushStyleColor(ImGuiCol_Text, PANEL_TEXT);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, PANEL_TEXT_DIM);
    ImGui::PushStyleColor(ImGuiCol_Separator, PANEL_SEP);
}
static void PopCreamStyle() { ImGui::PopStyleColor(5); }

// ================================================================ tile bag / tray widgets
// ---- "Unseen Tiles" panel: 6 columns, last row centered, counts under each tile ----
static const ImU32 COL_BAG_TILE     = IM_COL32(104, 128, 206, 255);
static const ImU32 COL_BAG_TILE_DIM = IM_COL32(184, 195, 226, 255);   // count == 0
static const ImU32 COL_BAG_HEADER   = IM_COL32( 74,  98, 140, 255);
static const ImU32 COL_BAG_INK      = IM_COL32( 20,  20,  24, 255);

static void DrawTileBagGridCream(const BagCounts& counts) {
    constexpr int   kCols = 6;
    constexpr float kGapX = 12.0f, kGapY = 10.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float baseSize  = ImGui::GetFontSize();
    const float avail     = ImGui::GetContentRegionAvail().x;
    const float cell      = std::clamp((avail - kGapX * (kCols - 1)) / kCols, 24.0f, 46.0f);
    const float countSize = baseSize * 1.15f;
    const float rowH      = cell + countSize + 4.0f + kGapY;
    const float gridW     = kCols * cell + (kCols - 1) * kGapX;
    const ImVec2 origin   = ImGui::GetCursorScreenPos();
    const float left      = origin.x + std::max(0.0f, (avail - gridW) * 0.5f);
    const float rounding  = cell * 0.28f;
    // One "leaf" corner pair is rounded, the other pair stays nearly square.
    const ImDrawFlags corners = ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomRight;

    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        const int row = i / kCols, col = i % kCols;
        const int inRow = std::min(kCols, NUM_LETTER_SLOTS - row * kCols);   // last row is short
        const float rowW = inRow * cell + (inRow - 1) * kGapX;
        const float x = left + (gridW - rowW) * 0.5f + col * (cell + kGapX);
        const float y = origin.y + row * rowH;
        const ImVec2 p0(x, y), p1(x + cell, y + cell);
        const unsigned n = counts[i];
        const bool isBlank = (i == NUM_LETTER_SLOTS - 1);

        dl->AddRectFilled(p0, p1, n > 0 ? COL_BAG_TILE : COL_BAG_TILE_DIM, rounding, corners);

        if (!isBlank) {
            const char buf[2] = { toChar(fromIndex((uint8_t)i)), 0 };
            const float letterSize = cell * 0.62f;
            ImGui::PushFont(g_fontFranklin);
            ImFont* lf = ImGui::GetFont();
            const ImVec2 ls = lf->CalcTextSizeA(letterSize, 10000.0f, 0.0f, buf);
            AddHeavyText(dl, lf, letterSize,
                         ImVec2(x + (cell - ls.x) * 0.5f, y + (cell - ls.y) * 0.5f),
                         IM_COL32(255, 255, 255, 255), buf);
            ImGui::PopFont();
        }

        char cb[8]; std::snprintf(cb, sizeof cb, "%u", n);
        const ImVec2 cs = font->CalcTextSizeA(countSize, 10000.0f, 0.0f, cb);
        AddBoldText(dl, font, countSize, ImVec2(x + (cell - cs.x) * 0.5f, y + cell + 3.0f), COL_BAG_INK, cb);
    }
    const int rows = (NUM_LETTER_SLOTS + kCols - 1) / kCols;
    ImGui::Dummy(ImVec2(avail, rows * rowH));
}

static void DrawTileBagPanelCream(const BagCounts& counts, unsigned remaining) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float baseSize = ImGui::GetFontSize();
    const float avail = ImGui::GetContentRegionAvail().x;

    // Header: "N Unseen Tiles", centered.
    char head[48]; std::snprintf(head, sizeof head, "%u Unseen Tiles", remaining);
    const float headSize = baseSize * 1.2f;
    const ImVec2 hs = font->CalcTextSizeA(headSize, 10000.0f, 0.0f, head);
    const ImVec2 hp = ImGui::GetCursorScreenPos();
    AddBoldText(dl, font, headSize, ImVec2(hp.x + (avail - hs.x) * 0.5f, hp.y), COL_BAG_HEADER, head);
    ImGui::Dummy(ImVec2(avail, hs.y + 8.0f));

    DrawTileBagGridCream(counts);

    // Footer: vowel / consonant totals. The blank counts as neither.
    static const int kVowels[] = { 0, 4, 8, 14, 20 };    // A E I O U
    unsigned total = 0, vowels = 0;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) total += counts[i];
    for (int v : kVowels) vowels += counts[v];
    const unsigned blanks = counts[NUM_LETTER_SLOTS - 1];
    const unsigned consonants = total - vowels - blanks;

    char lv[32], rc[32];
    std::snprintf(lv, sizeof lv, "Vowels: %u", vowels);
    std::snprintf(rc, sizeof rc, "Consonants: %u", consonants);
    const float footSize = baseSize * 1.15f;
    const ImVec2 rs = font->CalcTextSizeA(footSize, 10000.0f, 0.0f, rc);
    const ImVec2 fp = ImGui::GetCursorScreenPos();
    AddBoldText(dl, font, footSize, fp, COL_BAG_INK, lv);
    AddBoldText(dl, font, footSize, ImVec2(fp.x + avail - rs.x - 1.0f, fp.y), COL_BAG_INK, rc);
    ImGui::Dummy(ImVec2(avail, rs.y + 6.0f));
    ImGui::Separator();
}

// What the bag panel should show. Exploration and PvB read the live bag; the sim viewer
// shows the bag as it was at the position being viewed (a snapshot).
struct BagView {
    BagCounts counts{};
    unsigned  remaining = 0;
    bool      valid = false;      // false = nothing to draw (no bag yet)
};
static BagView LiveBagView(const ModeState& ms) {
    BagView v;
    if (!ms.bag) return v;
    v.counts = ms.bag->counts();
    v.remaining = (unsigned)ms.bag->size();
    v.valid = true;
    return v;
}
static BagView SnapshotBagView(const Snapshot& s) {
    BagView v;
    v.counts = s.bagCounts;
    v.remaining = (unsigned)s.bagTotal;
    v.valid = true;
    return v;
}

static void DrawTrayStrip(const std::vector<Letter>& order, bool interactive, int* hoverIdxOut) {
    const float cell = 46.0f, gap = 8.0f;
    const int slots = std::max(kRackSize, (int)order.size());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool winHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const float rackW = cell * slots + gap * (slots - 1);

    // Shelf the tiles sit on: starts below the tiles' midline so it shows underneath them.
    dl->AddRectFilled(ImVec2(origin.x - 10.0f, origin.y + cell * 0.72f),
                      ImVec2(origin.x + rackW + 10.0f, origin.y + cell + 10.0f),
                      COL_RACK_SHELF, 6.0f);

    for (int i = 0; i < (int)order.size(); ++i) {
        const ImVec2 p0(origin.x + i * (cell + gap), origin.y);
        DrawTileBody(dl, p0, cell, COL_TILE);
        if (order[i] == Letter::Blank) DrawTileGlyph(dl, p0, cell, ' ', 0, true);
        else                            DrawTileGlyph(dl, p0, cell, toChar(order[i]), LetterPoints(order[i]), true);
    }
    ImGui::Dummy(ImVec2(rackW, cell + 12.0f));

    if (hoverIdxOut) {
        *hoverIdxOut = -1;
        if (interactive && winHovered) {
            ImVec2 mouse = ImGui::GetMousePos();
            ImVec2 tMax(origin.x + slots * (cell + gap) - gap, origin.y + cell);
            if (mouse.x >= origin.x && mouse.x < tMax.x && mouse.y >= origin.y && mouse.y < tMax.y) {
                int i = (int)((mouse.x - origin.x) / (cell + gap));
                if (i >= 0 && i < (int)order.size()) *hoverIdxOut = i;
            }
        }
    }
}

// Draws player A's rack and handles starting a drag (and, optionally, returning a
// tile to the bag with right-click).
static void DrawTrayInteractive(ModeState& ms, bool interactive, bool rightClickToBag) {
    const std::vector<Letter> order = TrayLetters(ms.trayA);
    int hover = -1;
    DrawTrayStrip(order, interactive, &hover);
    if (!interactive || hover < 0 || g_ui.dragging) return;
    const Letter L = order[hover];
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_ui.dragging = true;
        g_ui.drag.src = DragSource::Tray;
        g_ui.drag.letter = L;
        g_ui.drag.isBlank = (L == Letter::Blank);
    } else if (rightClickToBag && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (BagReturn(ms.bag.get(), L)) {
            ms.trayA.removeTile(L);
            g_ui.lastAction = "Returned tile to bag.";
        }
    }
}

static void DrawScoresLine(const char* leftLabel, int leftScore,
                           const char* rightLabel, int rightScore,
                           const char* statusText, ImU32 statusCol) {
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s: %d    %s: %d", leftLabel, leftScore, rightLabel, rightScore);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
    ImGui::Text("|  %s", statusText);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
}

// ================================================================ drag & drop onto the board
enum class BoardKind { Pvb, Sim, Explore };

static bool IsTouchable(BoardKind kind, const ModeState& ms, int idx) {
    if (ms.board.letters_row[idx] == EMPTY_SQUARE) return false;
    if (kind == BoardKind::Explore) return true;
    // In PvB only tiles placed during the current turn may be moved or lifted.
    return ms.turnStart().board.letters_row[idx] == EMPTY_SQUARE;
}

static void CompleteTrayDrop(ModeState& ms, int r, int c, Letter assigned, bool blank) {
    ms.trayA.removeTile(blank ? Letter::Blank : assigned);
    placeTile(ms.board, r, c, assigned, blank);
    RefreshDerived(ms);
    g_ui.lastAction = "Placed tile.";
}

static void DropOnBoard(ModeState& ms, int r, int c) {
    std::string reason;
    if (g_ui.drag.src == DragSource::Tray) {
        const Letter tl = g_ui.drag.isBlank ? Letter::Blank : g_ui.drag.letter;
        if (ms.trayA.count(tl) == 0) {
            g_ui.lastAction = "Rejected: tile is no longer in the rack.";
        } else if (!CanDropTile(ms.board, r, c, reason)) {
            g_ui.lastAction = "Rejected: " + reason;
        } else if (g_ui.drag.isBlank) {
            g_ui.blankMs = &ms; g_ui.blankDropR = r; g_ui.blankDropC = c; g_ui.blankPopupOpen = true;
        } else {
            CompleteTrayDrop(ms, r, c, g_ui.drag.letter, false);
        }
    } else if (g_ui.drag.src == DragSource::Board) {
        const int fr = g_ui.drag.fromR, fc = g_ui.drag.fromC;
        if (fr != r || fc != c) {
            const Letter L = g_ui.drag.letter;
            const bool wasBlank = g_ui.drag.isBlank;
            removeTile(ms.board, fr, fc);
            if (CanDropTile(ms.board, r, c, reason)) {
                placeTile(ms.board, r, c, L, wasBlank);
                RefreshDerived(ms);
                g_ui.lastAction = "Moved tile.";
            } else {
                placeTile(ms.board, fr, fc, L, wasBlank);
                g_ui.lastAction = "Rejected move: " + reason;
            }
        }
    }
    EndDrag();
}

static void DrawDragGhost() {
    if (!g_ui.dragging) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 mp = ImGui::GetMousePos();
    const ImVec2 p0(mp.x - kCell * 0.5f, mp.y - kCell * 0.5f);
    DrawTileBody(dl, ImVec2(p0.x + 2.0f, p0.y + 3.0f), kCell, IM_COL32(0, 0, 0, 60));   // soft drop shadow
    DrawTileBody(dl, p0, kCell, COL_TILE);
    if (g_ui.drag.src == DragSource::Tray && g_ui.drag.isBlank) return;   // unassigned blank = empty tile
    char ch = toChar(g_ui.drag.letter);
    if (g_ui.drag.isBlank) ch = (char)std::tolower((unsigned char)ch);
    DrawTileGlyph(dl, p0, kCell, ch, g_ui.drag.isBlank ? 0 : LetterPoints(g_ui.drag.letter), !g_ui.drag.isBlank);
}

// ================================================================ board canvas
static void DrawBoardCanvas(BoardKind kind, ModeState& ms, bool interactive) {
    const bool explore = (kind == BoardKind::Explore);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 mouse = ImGui::GetMousePos();
    // Hovering only counts when this window is the one under the mouse, so popups
    // that sit over the board don't leak clicks through.
    const bool winHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    int hoverR = -1, hoverC = -1;

    const float grid = BOARD_LENGTH * (kCell + kGap) - kGap;

    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            ImVec2 p0(origin.x + c * (kCell + kGap), origin.y + r * (kCell + kGap));
            ImVec2 p1(p0.x + kCell, p0.y + kCell);
            const int idx = sq(r, c);
            const uint8_t code = ms.board.letters_row[idx];

            const bool hovered = winHovered && mouse.x >= p0.x && mouse.x < p1.x &&
                                 mouse.y >= p0.y && mouse.y < p1.y;
            if (hovered) { hoverR = r; hoverC = c; }

            const bool isAnchor = (ms.anchors.by_row[r] & (1u << c)) != 0;
            const Mult prem = premiumAt(r, c);
            const bool isCenter = (r == BOARD_LENGTH / 2 && c == BOARD_LENGTH / 2);
            const bool centerMark = isCenter && !explore;

            const bool hot = hovered && interactive;

            if (code != EMPTY_SQUARE) {
                // Placed tile: same silhouette as the rack.
                DrawTileBody(dl, p0, kCell, hot ? IM_COL32(86, 118, 188, 255) : COL_TILE);
                const Letter L = fromBoardCode(code);
                const bool blank = ms.board.is_blank.test(idx);
                char ch = toChar(L);
                if (blank) ch = (char)std::tolower((unsigned char)ch);
                DrawTileGlyph(dl, p0, kCell, ch, blank ? 0 : LetterPoints(L), true);
            } else {
                ImU32 bg;
                if (centerMark)                 bg = COL_CENTER;
                else if (explore) {
                    // Match the tooltip: cc[0] at actual coords, cc[1] at transposed coords.
                    const uint32_t mH = (uint32_t)ms.board.cross_check[0][idx];
                    const uint32_t mV = (uint32_t)ms.board.cross_check[1][sq(c, r)];
                    const uint32_t mi = mH & mV;
                    bg = HeatFill(std::popcount(mi));
                }
                else if (prem != MULT_NONE)     bg = PremiumFillNyt(prem);
                else                            bg = COL_BOARD_EMPTY;
                if (hot) bg = IM_COL32(176, 190, 226, 255);
                dl->AddRectFilled(p0, p1, bg, kCellRound);

                if (centerMark) {
                    DrawStar(dl, ImVec2(p0.x + kCell * 0.5f, p0.y + kCell * 0.5f), kCell * 0.32f, COL_TILE_TEXT);
                } else if (prem != MULT_NONE) {
                    const char* lab = PremiumLabelNyt(prem);
                    ImGui::PushFont(g_fontFranklin);
                    ImFont* font = ImGui::GetFont();
                    const float lblSize = ImGui::GetFontSize() * 1.05f;
                    const ImVec2 ts = font->CalcTextSizeA(lblSize, 10000.0f, 0.0f, lab);
                    AddHeavyText(dl, font, lblSize,
                                 ImVec2(p0.x + (kCell - ts.x) * 0.5f, p0.y + (kCell - ts.y) * 0.5f),
                                 PremiumTextNyt(prem), lab);
                    ImGui::PopFont();
                }
                // Cyan anchor outline, only when adjacent to a committed tile.
                if (isAnchor) {
                    if (kind == BoardKind::Sim || IsAnchorAgainst(ms.turnStart().board, r, c))
                        dl->AddRect(p0, p1, IM_COL32(0, 200, 230, 255), kCellRound, 0, 2.5f);
                }
            }
        }
    }
    ImGui::Dummy(ImVec2(grid, grid));

    // Highlight of the move selected in the Move Generation tab.
    if (explore) {
        for (const auto& h : g_mg.hlCells) {
            if (ms.board.letters_row[sq(h.r, h.c)] != EMPTY_SQUARE) continue;
            ImVec2 p0(origin.x + h.c * (kCell + kGap), origin.y + h.r * (kCell + kGap));
            ImVec2 p1(p0.x + kCell, p0.y + kCell);
            dl->AddRectFilled(p0, p1, IM_COL32(107, 165, 219, 190), kCellRound);
            dl->AddRect(p0, p1, IM_COL32(40, 90, 160, 255), kCellRound, 0, 2.0f);
            const char buf[2] = { h.ch, 0 };
            const float sz = kCell * 0.55f;
            const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(sz, 10000.0f, 0.0f, buf);
            AddBoldText(dl, ImGui::GetFont(), sz,
                        ImVec2(p0.x + (kCell - ts.x) * 0.5f, p0.y + (kCell - ts.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), buf);
        }
    }

        // Pending-move green outline: only when the board is valid AND the staged tiles
    // form a geometrically valid placement. The outline covers the entire formed word
    // (committed tiles included), not just the new tiles.
    if (interactive && ms.boardValid) {
        const Placement pend = EvaluatePlacement(ms.board, ms.turnStart().board);
        if (pend.ok) {
            // Walk from pend.(row,col) in the move direction to find the full run end.
            const int dr = pend.desc ? 1 : 0;
            const int dc = pend.desc ? 0 : 1;
            int len = 0;
            while (pend.row + len * dr < BOARD_LENGTH &&
                   pend.col + len * dc < BOARD_LENGTH &&
                   ms.board.letters_row[sq(pend.row + len * dr, pend.col + len * dc)] != EMPTY_SQUARE) {
                ++len;
            }

            const float pad = 3.0f;
            ImVec2 a(origin.x + pend.col * (kCell + kGap) - pad,
                     origin.y + pend.row * (kCell + kGap) - pad);
            ImVec2 b;
            if (pend.desc) {
                b = ImVec2(origin.x + (pend.col + 1) * (kCell + kGap) - kGap + pad,
                           origin.y + (pend.row + len) * (kCell + kGap) - kGap + pad);
            } else {
                b = ImVec2(origin.x + (pend.col + len) * (kCell + kGap) - kGap + pad,
                           origin.y + (pend.row + 1) * (kCell + kGap) - kGap + pad);
            }
            dl->AddRect(a, b, IM_COL32(60, 200, 80, 255), kCellRound + 2.0f, 0, 3.0f);

            char pb[16]; std::snprintf(pb, sizeof pb, "%d", pend.points);
            ImFont* font = ImGui::GetFont();
            const float badgeFont = kCell * 0.55f;
            const ImVec2 ts = font->CalcTextSizeA(badgeFont, 10000.0f, 0.0f, pb);
            const float r = kCell * 0.42f;
            const ImVec2 cc(b.x - r * 0.1f, b.y + r * 0.7f);
            dl->AddCircleFilled(cc, r, IM_COL32(60, 200, 80, 255));
            AddBoldText(dl, font, badgeFont,
                        ImVec2(cc.x - ts.x * 0.5f, cc.y - ts.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), pb);
        }
    }

    // Record hover for the caller's tooltip. Tooltips drawn inside a BeginChild/EndChild
    // pair can render blank on the SDL2/SDLRenderer2 backend, so the tooltip is drawn by
    // DrawBoardHoverTooltip after the board child window has closed.
    g_boardHoverR  = hoverR;
    g_boardHoverC  = hoverC;
    g_boardHoverOk = explore && (hoverR >= 0) && !g_ui.dragging;

    if (!interactive) return;
    const bool overBoard = hoverR >= 0 && hoverC >= 0;

    if (!g_ui.dragging && overBoard) {
        const int idx = sq(hoverR, hoverC);
        if (kind != BoardKind::Sim && IsTouchable(kind, ms, idx)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                // Lift: return the tile to the rack.
                const Letter letter = fromBoardCode(ms.board.letters_row[idx]);
                const bool isBlank = ms.board.is_blank.test(idx);
                removeTile(ms.board, hoverR, hoverC);
                ms.trayA.addTile(isBlank ? Letter::Blank : letter);
                RefreshDerived(ms);
                g_ui.lastAction = "Lifted tile to tray.";
                return;
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                g_ui.dragging = true;
                g_ui.drag.src = DragSource::Board;
                g_ui.drag.fromR = hoverR; g_ui.drag.fromC = hoverC;
                g_ui.drag.letter = fromBoardCode(ms.board.letters_row[idx]);
                g_ui.drag.isBlank = ms.board.is_blank.test(idx);
            }
        }
    }
    // Releasing over the board drops; releasing anywhere else cancels (see end-of-frame cleanup).
    if (g_ui.dragging && overBoard && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        DropOnBoard(ms, hoverR, hoverC);
}

// Draws the cross-check tooltip for the hovered Exploration square. Called at the top
// level (outside any window/child) so it renders correctly on SDL2/SDLRenderer2.
static void DrawBoardHoverTooltip(const ModeState& ms) {
    if (!g_boardHoverOk) return;
    const int r = g_boardHoverR, c = g_boardHoverC;
    const int idx = sq(r, c);
    const Mult prem = premiumAt(r, c);

    ImGui::BeginTooltip();
    ImGui::Text("(%d,%d)", r, c);
    if (prem != MULT_NONE) ImGui::Text("Premium: %s", PremiumLabelNyt(prem));
    ImGui::Separator();
    const uint8_t code = ms.board.letters_row[idx];
    if (code != EMPTY_SQUARE) {
        char ch = toChar(fromBoardCode(code));
        if (ms.board.is_blank.test(idx)) ch = (char)std::tolower((unsigned char)ch);
        ImGui::Text("Tile: %c", ch);
    } else {
        auto maskStr = [](uint32_t m, char* out) {
            size_t k = 0;
            for (int i = 0; i < NUM_LETTERS; ++i) if (m & (1u << i)) out[k++] = (char)('A' + i);
            out[k] = 0;
        };
        // cc[0] uses actual coords; cc[1] uses transposed coords (matching MoveGen's reads).
        const uint32_t mH = (uint32_t)ms.board.cross_check[0][idx];
        const uint32_t mV = (uint32_t)ms.board.cross_check[1][sq(c, r)];
        char sH[40], sV[40], sA[40];
        maskStr(mH, sH); maskStr(mV, sV); maskStr(mH & mV, sA);
        ImGui::Text("H: %s", sH);
        ImGui::Text("V: %s", sV);
        ImGui::Text("&: %s", sA);
    }
    ImGui::EndTooltip();
}

// ================================================================ shared screen pieces
// The upper half of every mode screen: the board on the left, the tile bag on the right.
// Child-window names only need to be unique inside their parent window, and each mode
// draws this inside its own window, so one set of names serves all three modes.
static void DrawBoardAndBag(BoardKind kind, ModeState& ms, bool interactive, const BagView& bag) {
    ImGui::BeginChild("##boardbag", ImVec2(0, 500), false);
    {
        ImGui::BeginChild("##board", ImVec2(520, 500), false);
        DrawBoardCanvas(kind, ms, interactive);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##bag", ImVec2(0, 500), false);
        if (bag.valid) DrawTileBagPanelCream(bag.counts, bag.remaining);
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

// ================================================================ top-strip helpers
static void PushTopAccent() {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 90, 120, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 115, 150, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(55, 75, 105, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
}

// Button colours for cream-background panels (Explore / PvB / Sim body).
static void PushPanelButton() {
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 60, 100, 160, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32( 80, 125, 190, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32( 45,  80, 135, 255));
    ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(255, 255, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,  IM_COL32(160, 165, 175, 255));
}
static void PopPanelButton() { ImGui::PopStyleColor(5); }

static void PopTopAccent() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}
static void TopGroupSep() {
    ImGui::SameLine(); ImGui::Dummy(ImVec2(12, 0)); ImGui::SameLine();
}

static void OpenSavePopup() { g_ui.saveMessage.clear(); g_ui.savePopupOpen = true; }
static void OpenLoadPopup() { g_ui.loadMessage.clear(); g_ui.loadListDirty = true; g_ui.loadPopupOpen = true; }

static void DrawSaveLoadButtons() {
    const float bw = 100.0f;
    ImGui::SameLine();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float need = bw * 2 + ImGui::GetStyle().ItemSpacing.x;
    if (avail > need) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - need);
    if (ImGui::Button("Load State", ImVec2(bw, 0))) OpenLoadPopup();
    ImGui::SameLine();
    if (ImGui::Button("Save State", ImVec2(bw, 0))) OpenSavePopup();
}

// Compact history nav: four buttons in a single row, sized to fit a narrow panel.
// Returns the cursor the user asked to jump to, or -1.
static int DrawHistoryNavBar(const ModeState& ms) {
    int target = -1;
    const bool canPrev = ms.cursor > 0;
    const bool canNext = ms.cursor < (int)ms.history.size();

    const float bw = 40.0f;
    ImGui::BeginDisabled(!canPrev);
    if (ImGui::Button("|<", ImVec2(bw, 0))) target = 0;
    ImGui::SameLine();
    if (ImGui::Button("<", ImVec2(bw, 0))) target = ms.cursor - 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!canNext);
    if (ImGui::Button(">", ImVec2(bw, 0))) target = ms.cursor + 1;
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(bw, 0))) target = (int)ms.history.size();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("%d / %d", ms.cursor, (int)ms.history.size());
    return target;
}

// Common top strip: Back to Title + Rematch + optional extra buttons + centered mode name
// + right-aligned save/load. `onBack`/`onRematch` are called when those buttons are pressed.
// `extra` (may be null) is drawn between Rematch and the mode name.
struct TopStripCallbacks {
    void (*onBack)() = nullptr;
    void (*onRematch)() = nullptr;
    void (*extra)() = nullptr;
    const char* modeName = nullptr;   // centered in the strip; may be null
};

static void DrawModeTopStrip(const ModeState& ms, const TopStripCallbacks& cb) {
    (void)ms;   // nav moved to the Move History panel; kept for signature compatibility
    PushTopAccent();
    if (ImGui::Button("Back to Title")) { cb.onBack(); PopTopAccent(); return; }
    TopGroupSep();
    if (ImGui::Button("Rematch")) cb.onRematch();
    TopGroupSep();
    if (cb.extra) { cb.extra(); TopGroupSep(); }

    // Centered mode name. Compute where the Save/Load block will end so we can center
    // the name in the space between the cursor's current X and that block's start.
    if (cb.modeName) {
        const float bw = 100.0f;
        const float saveLoadW = bw * 2 + ImGui::GetStyle().ItemSpacing.x;
        const float stripW = ImGui::GetWindowContentRegionMax().x;
        const float saveLoadX = stripW - saveLoadW;

        const float nameW = ImGui::CalcTextSize(cb.modeName).x;
        const float nameX = ImGui::GetCursorPosX() + (saveLoadX - ImGui::GetCursorPosX() - nameW) * 0.5f;
        if (nameX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(nameX);
        ImGui::TextDisabled("%s", cb.modeName);
        ImGui::SameLine();
    }

    DrawSaveLoadButtons();
    PopTopAccent();
}

static void LeaveToTitle() {
    EndDrag();
    g_ui.blankPopupOpen = false;
    g_ui.addTilePopupOpen = false;
    g_ui.savePopupOpen = false;      // Save/Load are mode-specific; leaving them open would act on the wrong state
    g_ui.loadPopupOpen = false;
    g_ui.confirmBackOpen = false;
    g_mode = AppMode::Title;
}

// Opens the mode's dock (once) and top strip, and returns false if the user left the mode
// via the top strip during this call. Callers must return immediately if it returns false.
static bool BeginModeScreen(AppMode mode, const char* topName, bool& dockBuiltFlag,
                            const char* creamName, const char* rightName,
                            float legendW, float rightW,
                            void (*drawTop)()) {
    if (!dockBuiltFlag) {
        BuildDock(DockIdFor(mode), topName, creamName, rightName, legendW, rightW);
        dockBuiltFlag = true;
    }
    ImGui::Begin(topName, nullptr, PANEL_FLAGS);
    drawTop();
    ImGui::End();
    return g_mode == mode;
}

// ================================================================ Exploration
static void ExplorationReset() {
    g_ex.bag = std::make_unique<TileBag>(kExplorationSeed);
    g_ex.board = Board{};
    g_ex.trayA = Tray{};
    g_ex.trayB = Tray{};
    g_ex.history.clear();
    g_ex.cursor = 0;
    g_ex.score[0] = g_ex.score[1] = 0;
    RefreshDerived(g_ex);
    g_ex.initial = SnapshotOf(g_ex);
    g_ui.lastAction = "(fresh session)";
}

static void LoadTestingCaseIntoExploration() {
    ExplorationReset();   // initial snapshot = empty board, full bag

    struct MoveSpec { const char* word; int row, col; bool desc; int player; };
    const MoveSpec specs[] = {
        {"LANDAU", 2, 7, true,  0},
        {"NUDELY", 7, 6, false, 1},
        {"BESES",  8, 5, false, 0},
    };
    for (const auto& s : specs) {
        const Board before = g_ex.board;
        Move mv{ s.word, s.row, s.col, s.desc };
        for (const auto& pl : mv.letterPositions()) {
            if (g_ex.board.letters_row[sq(pl.row, pl.col)] != EMPTY_SQUARE) continue;
            placeTile(g_ex.board, pl.row, pl.col, pl.letter, pl.is_blank);
            const Letter tl = pl.is_blank ? Letter::Blank : pl.letter;
            if (g_ex.bag->count(tl) > 0) g_ex.bag->removeLetter(tl);   // keep the bag consistent
        }
        RefreshDerived(g_ex);
        const Placement ps = EvaluatePlacement(g_ex.board, before);
        if (ps.ok) g_ex.score[s.player] += ps.points;
        PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string(s.word), s.player);
    }

    static const char kRack[] = "ERSBEES";
    g_ex.trayA = Tray{};
    g_ex.trayA.addTiles(kRack);
    for (const char* p = kRack; *p; ++p) {
        const Letter L = fromIndex((uint8_t)(*p - 'A'));
        if (g_ex.bag->count(L) > 0) g_ex.bag->removeLetter(L);
    }
    // Make the last committed entry include the rack so the position isn't "uncommitted".
    static_cast<Snapshot&>(g_ex.history.back()) = SnapshotOf(g_ex);
    g_ex.cursor = (int)g_ex.history.size();
    g_ui.lastAction = "(testing case loaded)";
}

static void ExUndoTurn() {
    // Revert only the board; the tray may have been modified outside the turn
    // (added tiles from the bag, random tile) and shouldn't be rolled back.
    if (g_ex.cursor == 0) g_ex.board = g_ex.initial.board;
    else                  g_ex.board = g_ex.history[g_ex.cursor - 1].board;
    RefreshDerived(g_ex);
    EndDrag();
    g_ui.blankPopupOpen = false;
    g_ui.lastAction = "Reverted to last committed.";
}

static void ExTopOnBack()    { LeaveToTitle(); }
static void ExTopOnRematch() { ExplorationReset(); }
static void ExTopExtra()     { if (ImGui::Button("Load Testing Case")) LoadTestingCaseIntoExploration(); }

static void DrawExplorationTop() {
    TopStripCallbacks cb;
    cb.onBack    = ExTopOnBack;
    cb.onRematch = ExTopOnRematch;
    cb.extra     = ExTopExtra;
    cb.modeName  = "Exploration";
    DrawModeTopStrip(g_ex, cb);
}

static const RawMove* BotPlayable();

static void DrawExplorationCream() {
    PushCreamStyle();
    ImGui::Begin("Cream##ex", nullptr, PANEL_FLAGS);

    const bool atEnd = g_ex.atEnd();

    DrawBoardAndBag(BoardKind::Explore, g_ex, atEnd, LiveBagView(g_ex));

    ImGui::Separator();

    const Placement exPS = atEnd ? EvaluatePlacement(g_ex.board, g_ex.turnStart().board)
                                 : Placement{};
    const bool hasUncommitted = HasUncommitted(g_ex);
    const bool hasHighlight = (g_mg.hlMoveIdx >= 0);
    const RawMove* botMove = BotPlayable();

    // Play Move: staged tiles form a valid placement, or a Move Generation row is highlighted,
    // or a Bot Search row is selected. The last two need nothing staged.
    const bool canPlayStaged    = atEnd && hasUncommitted && g_ex.boardValid && exPS.ok;
    const bool canPlayHighlight = atEnd && hasHighlight && !hasUncommitted && g_ex.boardValid;
    const bool canPlayBot       = atEnd && !hasHighlight && botMove && g_ex.boardValid;
    const bool canPlay = canPlayStaged || canPlayHighlight || canPlayBot;
    const bool canUndo = atEnd && hasUncommitted;

    PushPanelButton();
    if (!canPlay) ImGui::BeginDisabled();
    if (ImGui::Button("Play Move", ImVec2(120, 32))) {
        if (canPlayStaged) {
            PushEntry(g_ex, FormatMove(exPS), -1);
            g_ui.lastAction = "Move committed.";
        } else if (canPlayHighlight) {
            const std::vector<ScoredMove>* listPtr = &g_mg.allMoves;
            if (g_mg.view == MoveView::Unique) listPtr = &g_mg.uniqueMoves;
            else if (g_mg.view == MoveView::TopN) listPtr = &g_mg.topNMoves;
            const auto& list = *listPtr;
            if (g_mg.hlMoveIdx < (int)list.size()) {
                const RawMove rm = list[g_mg.hlMoveIdx].rm;
                ApplyHighlightedMove(g_ex, rm);
                // Evaluate AFTER applying: before, the staged diff is empty and the label was always "(play)".
                const Placement ps = EvaluatePlacement(g_ex.board, g_ex.turnStart().board);
                PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string("(play)"), -1);
                g_mg.hlMoveIdx = -1;
                g_mg.hlCells.clear();
                g_mg.movesStale = true;
            }
        } else if (canPlayBot) {
            const RawMove rm = *botMove;               // copy: selRaw is cleared below
            ApplyHighlightedMove(g_ex, rm);
            const Placement ps = EvaluatePlacement(g_ex.board, g_ex.turnStart().board);
            PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string("(play)"), -1);
            g_bot.clearSel();
            g_mg.hlCells.clear();
            g_mg.movesStale = true;
        }
    }
    if (!canPlay) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo Turn", ImVec2(120, 32))) ExUndoTurn();
    if (!canUndo) ImGui::EndDisabled();

    ImGui::SameLine();
    const bool boardEdited = !BoardsEqual(g_ex.board, g_ex.turnStart().board);
    if (!atEnd || boardEdited) ImGui::BeginDisabled();
    if (ImGui::Button("Pass", ImVec2(80, 32))) {
        PushEntry(g_ex, "PASS", -1);
        g_ui.lastAction = "Passed.";
    }
    if (!atEnd || boardEdited) ImGui::EndDisabled();
    PopPanelButton();

    ImGui::Separator();
    DrawTrayInteractive(g_ex, atEnd, true);

    const bool rackFull = (int)g_ex.trayA.size() >= kRackSize;
    const bool bagEmpty = (!g_ex.bag || g_ex.bag->size() == 0);
    PushPanelButton();
    if (!atEnd || rackFull || bagEmpty) ImGui::BeginDisabled();
    if (ImGui::Button("Add Tile from Bag")) g_ui.addTilePopupOpen = true;
    ImGui::SameLine();
    if (ImGui::Button("Add Random Tile")) {
        g_ex.bag->drawInto(g_ex.trayA, 1);
        g_ui.lastAction = "Added random tile.";
    }
    if (!atEnd || rackFull || bagEmpty) ImGui::EndDisabled();
    PopPanelButton();

    ImGui::Separator();
    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextUnformatted(g_ex.boardValid ? "Board valid" : "Board INVALID");
    ImGui::SetWindowFontScale(1.0f);
    if (canPlayHighlight || canPlayBot) {
        ImGui::TextDisabled("Highlighted move ready.");
    } else if (atEnd && !BoardsEqual(g_ex.board, g_ex.turnStart().board)) {
        if (exPS.ok) ImGui::Text("Pending move: %s  (+%d)", exPS.word.c_str(), exPS.points);
        else         ImGui::TextDisabled("Pending move: %s", exPS.error.c_str());
    }
    ImGui::TextDisabled("Last: %s", g_ui.lastAction.c_str());

    ImGui::End();
    PopCreamStyle();
}

// ================================================================ move generation panel
static bool MoveLess(const ScoredMove& a, const ScoredMove& b, int mode) {
    auto pos = [](const ScoredMove& m) {
        return std::make_tuple((int)m.rm.is_descending, (int)m.rm.row, (int)m.rm.col);
    };
    switch (mode) {
        case 1: if (a.rm.len != b.rm.len) return a.rm.len > b.rm.len;
                if (a.upper != b.upper) return a.upper < b.upper;
                return pos(a) < pos(b);
        case 2: if (a.upper != b.upper) return a.upper < b.upper;
                return pos(a) < pos(b);
        case 3: if (pos(a) != pos(b)) return pos(a) < pos(b);
                return a.upper < b.upper;
        default:
                if (a.sc.points != b.sc.points) return a.sc.points > b.sc.points;
                if (a.upper != b.upper) return a.upper < b.upper;
                // Same word and same score: vertical before horizontal.
                if (a.rm.is_descending != b.rm.is_descending) return a.rm.is_descending;
                if (a.rm.row != b.rm.row) return a.rm.row < b.rm.row;
                return a.rm.col < b.rm.col;
    }
}

// Rebuilds the three derived views from g_mg.allMoves:
//   uniqueMoves -- best placement per distinct word, capped at topN.
//   topNMoves   -- topN highest-scoring placements, duplicates allowed.
// Both are then ordered by the current sort mode.
static void RebuildMoveViews() {
    // Unique: iterate the score-sorted list, keep the first (highest-scoring) placement
    // for each distinct word, cap at topN.
    std::vector<int> idx(g_mg.allMoves.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [](int a, int b) { return MoveLess(g_mg.allMoves[a], g_mg.allMoves[b], 0); });

    g_mg.uniqueMoves.clear();
    std::unordered_set<std::string> seen;
    for (int i : idx) {
        if (!seen.insert(g_mg.allMoves[i].upper).second) continue;
        g_mg.uniqueMoves.push_back(g_mg.allMoves[i]);
        if (g_mg.topN > 0 && (int)g_mg.uniqueMoves.size() >= g_mg.topN) break;
    }

    // TopN: highest-scoring placements, duplicates allowed, cap at topN.
    g_mg.topNMoves.clear();
    for (int i = 0; i < (int)g_mg.allMoves.size() && (g_mg.topN <= 0 || (int)g_mg.topNMoves.size() < g_mg.topN); ++i)
        g_mg.topNMoves.push_back(g_mg.allMoves[i]);

    auto sortByMode = [](std::vector<ScoredMove>& v) {
        std::sort(v.begin(), v.end(),
                  [](const ScoredMove& a, const ScoredMove& b) { return MoveLess(a, b, g_mg.moveSortMode); });
    };
    sortByMode(g_mg.uniqueMoves);
    sortByMode(g_mg.topNMoves);

    g_mg.movesDirty = true;
}

static void RunMoveGeneration() {
    MoveList sink;
    generateMovesFromScratch(*g_gaddag, g_ex.board, g_ex.trayA, sink);   // rebuilds cross-checks itself
    g_mg.allMoves.clear();
    g_mg.allMoves.reserve(sink.moves.size());
    for (auto& rm : sink.moves) {
        g_mg.allMoves.push_back({ rm, scoreRawFull(g_ex.board, rm), std::string(), std::string() });
        ScoredMove& sm = g_mg.allMoves.back();
        sm.word = toMove(rm).word;
        sm.upper = sm.word;
        for (char& ch : sm.upper) ch = (char)std::toupper((unsigned char)ch);
    }
    g_mg.moveSortMode = 0;
    std::sort(g_mg.allMoves.begin(), g_mg.allMoves.end(),
              [](const ScoredMove& a, const ScoredMove& b) { return MoveLess(a, b, 0); });
    RebuildMoveViews();
    g_mg.hlMoveIdx = -1;
    g_mg.hlCells.clear();
    g_mg.movesStale = false;
    g_ui.lastAction = "Generated moves.";
}

static void HighlightRaw(const RawMove& rm) {
    g_mg.hlCells.clear();
    Move mv = toMove(rm);
    for (const auto& pl : mv.letterPositions()) {
        char ch = toChar(pl.letter);
        if (pl.is_blank) ch = (char)std::tolower((unsigned char)ch);
        g_mg.hlCells.push_back({ pl.row, pl.col, ch });
    }
}

static void SelectMove(const ScoredMove& sm) {
    // Reject if the user has already staged tiles this turn.
    if (HasUncommitted(g_ex)) {
        g_mg.hlMoveIdx = -1;
        g_mg.hlCells.clear();
        g_ui.lastAction = "Cannot highlight: clear staged tiles first.";
        return;
    }
    g_bot.clearSel();                     // un-mark any Bot Search row
    HighlightRaw(sm.rm);
}

// Bot Search row callback: nullptr clears the highlight.
static void BotSelect(const RawMove* rm) {
    g_mg.hlMoveIdx = -1; g_mg.hlCells.clear();
    if (!rm) return;
    if (HasUncommitted(g_ex)) {
        g_bot.clearSel();
        g_ui.lastAction = "Cannot highlight: clear staged tiles first.";
        return;
    }
    HighlightRaw(*rm);
}

// The Bot Search row to play, or nullptr. Only valid while the board tiles and rack still match
// the search and nothing is staged (ApplyHighlightedMove would otherwise remove tiles the rack lacks).
static const RawMove* BotPlayable() {
    const RawMove* m = g_bot.selected();
    if (!m || g_bot.running() || HasUncommitted(g_ex)) return nullptr;
    if (g_ex.board.letters_row != g_bot.rootBoard.letters_row ||
        g_ex.board.is_blank != g_bot.rootBoard.is_blank ||
        !TraysEqual(g_ex.trayA, g_bot.rootRack)) return nullptr;
    return m;
}

static void DrawMoveGenerationTab() {
    const bool canGenerate = g_ex.boardValid && g_gaddag;
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Run Move Generation")) RunMoveGeneration();
    if (!canGenerate) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Board must be valid.");
    if (g_mg.movesStale && !g_mg.allMoves.empty())
        ImGui::TextColored(ImVec4(0.85f, 0.6f, 0.2f, 1.0f), "Board changed since these moves were generated.");

    ImGui::Separator();

    // View selector: Unique (deduped by word) or All (every placement).
    const bool isUnique = (g_mg.view == MoveView::Unique);
    if (ImGui::RadioButton("Unique", isUnique)) {
        g_mg.view = MoveView::Unique;
        g_mg.hlMoveIdx = -1; g_mg.hlCells.clear(); g_mg.movesDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("All", !isUnique)) {
        g_mg.view = MoveView::TopN;   // reuse TopN = "every placement, dupes allowed"
        g_mg.hlMoveIdx = -1; g_mg.hlCells.clear(); g_mg.movesDirty = true;
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(16, 0));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputInt("##topn", &g_mg.topN, 5, 40)) {
        g_mg.topN = std::clamp(g_mg.topN, 1, 100000);
        RebuildMoveViews();
    }
    ImGui::SameLine();
    if (ImGui::Button("Max")) {
        g_mg.topN = std::max(1, (int)g_mg.allMoves.size());
        RebuildMoveViews();
    }

    ImGui::Spacing();

    // Filter and sort on their own row so nothing is cramped.
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##filter", "filter word...", g_mg.moveFilter, sizeof g_mg.moveFilter);
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(16, 0));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    const char* sortItems[] = { "Sort: Score", "Sort: Length", "Sort: A-Z", "Sort: Position" };
    if (ImGui::Combo("##sort", &g_mg.moveSortMode, sortItems, 4)) {
        std::sort(g_mg.allMoves.begin(), g_mg.allMoves.end(),
                  [](const ScoredMove& a, const ScoredMove& b) { return MoveLess(a, b, g_mg.moveSortMode); });
        RebuildMoveViews();
        g_mg.hlMoveIdx = -1;
        g_mg.hlCells.clear();
    }

    // Pick which list the user is looking at.
    const std::vector<ScoredMove>* listPtr = &g_mg.allMoves;
    if (g_mg.view == MoveView::Unique) listPtr = &g_mg.uniqueMoves;
    else if (g_mg.view == MoveView::TopN) listPtr = &g_mg.topNMoves;
    const auto& list = *listPtr;

    // Rebuild the filtered index list only when something that affects it changed.
    static std::string lastFilter;
    if (g_mg.movesDirty || lastFilter != g_mg.moveFilter) {
        lastFilter = g_mg.moveFilter;
        std::string pat = g_mg.moveFilter;
        for (char& ch : pat) ch = (char)std::toupper((unsigned char)ch);
        g_mg.filtered.clear();
        for (int i = 0; i < (int)list.size(); ++i)
            if (pat.empty() || list[i].upper.find(pat) != std::string::npos) g_mg.filtered.push_back(i);
        g_mg.movesDirty = false;
    }

    ImGui::Spacing();
    ImGui::Text("%zu / %zu", g_mg.filtered.size(), list.size());
    if (g_mg.hlMoveIdx >= 0) {
        ImGui::SameLine();
        if (ImGui::Button("Clear highlight")) { g_mg.hlMoveIdx = -1; g_mg.hlCells.clear(); }
    }
    ImGui::Separator();

    ImGui::BeginChild("##moves", ImVec2(0, 0), true);
    {
        char header[80];
        std::snprintf(header, sizeof header,
                      "%-3s %4s  %-15.15s  %-9s  %s",
                      "#", "Pts", "Word", "Dir & Pos", "Bingo");
        ImGui::TextUnformatted(header);
        ImGui::Separator();

        ImGuiListClipper clipper;
        clipper.Begin((int)g_mg.filtered.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const int i = g_mg.filtered[row];
                const ScoredMove& sm = list[i];
                char label[160];
                std::snprintf(label, sizeof label,
                              "%-3d %+4d  %-15.15s  %c (%2d,%2d)  %s",
                              row + 1,
                              sm.sc.points,
                              sm.word.c_str(),
                              sm.rm.is_descending ? 'V' : 'H',
                              (int)sm.rm.row, (int)sm.rm.col,
                              sm.sc.bingo ? "**" : "");
                ImGui::PushID(i);

                if (ImGui::Selectable(label, g_mg.hlMoveIdx == i)) {
                    if (g_mg.hlMoveIdx == i) {
                        g_mg.hlMoveIdx = -1;
                        g_mg.hlCells.clear();
                    } else {
                        g_mg.hlMoveIdx = i;
                        SelectMove(sm);
                    }
                }

                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

// ================================================================ Legend
static void DrawLegendFull() {
    ImGui::Begin("Legend##legend", nullptr, PANEL_FLAGS);
    ImGui::TextUnformatted("Board colors");
    ImGui::Separator();
    DrawSwatch(COL_2L, "2L -- double letter");
    DrawSwatch(COL_3L, "3L -- triple letter");
    DrawSwatch(COL_2W, "2W -- double word");
    DrawSwatch(COL_3W, "3W -- triple word");
    DrawSwatch(COL_TILE, "placed tile");
    DrawSwatch(IM_COL32(0, 200, 230, 255), "anchor");
    DrawSwatch(IM_COL32(220, 60, 60, 255), "invalid word");
    ImGui::Separator();
    ImGui::TextUnformatted("Cross-check heat");
    ImGui::Separator();
    DrawSwatch(COL_HEAT_0,   "0 -- dead square");
    DrawSwatch(COL_HEAT_LOW, "1-5 -- very tight");
    DrawSwatch(COL_HEAT_MID, "6-15 -- moderate");
    DrawSwatch(COL_HEAT_HI,  "16-25 -- wide open");
    DrawSwatch(COL_BOARD_EMPTY, "26 -- unconstrained");
    ImGui::End();
}

static void DrawLegendCompact() {
    ImGui::Begin("Legend##legend", nullptr, PANEL_FLAGS);
    DrawSwatch(COL_2L, "2L");
    DrawSwatch(COL_3L, "3L");
    DrawSwatch(COL_2W, "2W");
    DrawSwatch(COL_3W, "3W");
    ImGui::End();
}

// ================================================================ Move History tab
// Renders the move history as columns: Turn | Who | Pts | Word | Dir | Start.
// The label stored on each Entry is human-readable text like "NUDELY (7,6) H +22";
// we parse it back into fields for display (no data-model change required).
static bool ParseEntryLabel(const std::string& label,
                            char* word, size_t wordSz,
                            int& row, int& col, char& dir, int& pts) {
    word[0] = '\0'; row = col = pts = 0; dir = '?';
    if (label == "PASS") { std::snprintf(word, wordSz, "PASS"); return false; }
    // "%15s (%d,%d) %c +%d"
    char w[32] = {0};
    if (std::sscanf(label.c_str(), "%31s (%d,%d) %c +%d", w, &row, &col, &dir, &pts) != 5) {
        // Fall back: dump the whole thing into the word column.
        std::snprintf(word, wordSz, "%s", label.c_str());
        return false;
    }
    std::snprintf(word, wordSz, "%s", w);
    return true;
}

static void DrawMoveHistoryTab(const ModeState& ms, size_t& lastSeen) {
    ImGui::BeginChild("##historyScroll", ImVec2(0, 0), true);
    {
        char header[80];
        std::snprintf(header, sizeof header,
                      "%-3s %-4s %+4s  %-15.15s  %-9s",
                      "#", "Who", "Pts", "Word", "Dir & Pos");
        ImGui::TextUnformatted(header);
        ImGui::Separator();

        if (ms.history.empty()) ImGui::TextDisabled("(no committed moves yet)");

        ImGuiListClipper clipper;
        clipper.Begin((int)ms.history.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Entry& e = ms.history[i];
                const char* who = (e.player == 0) ? "P1" : (e.player == 1 ? "P2" : "-");
                const bool isCursorRow = (i == ms.cursor - 1);

                char word[64];
                int row = 0, col = 0, pts = 0;
                char dir = '?';
                ParseEntryLabel(e.label, word, sizeof word, row, col, dir, pts);

                char label[160];
                if (dir == '?') {
                    std::snprintf(label, sizeof label,
                                  "%-3d %-4s   --   %-15.15s  %s",
                                  e.turnNumber, who, word, "-");
                } else {
                    char posBuf[16];
                    std::snprintf(posBuf, sizeof posBuf, "%c (%2d,%2d)", dir, row, col);
                    std::snprintf(label, sizeof label,
                                  "%-3d %-4s %+4d  %-15.15s  %-9s",
                                  e.turnNumber, who, pts, word, posBuf);
                }

                if (isCursorRow) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 200, 255, 255));
                ImGui::TextUnformatted(label);
                if (isCursorRow) ImGui::PopStyleColor();
            }
        }
    }
    ImGui::EndChild();

    if (ms.history.size() > lastSeen && ms.atEnd()) ImGui::SetScrollHereY(1.0f);
    lastSeen = ms.history.size();
}

// ================================================================ Exploration mode
static void DrawExplorationMode() {
    if (!BeginModeScreen(AppMode::Exploration, kTopEx, g_ui.dockBuiltEx,
                         "Cream##ex", "Right Panel##ex", 180.0f, 440.0f,
                         DrawExplorationTop))
        return;

    DrawExplorationCream();

    ImGui::Begin("Right Panel##ex", nullptr, PANEL_FLAGS);
    static int exTab = 0;
    if (ImGui::Button(exTab == 0 ? "[ Move History ]" : "Move History")) exTab = 0;
    ImGui::SameLine();
    if (ImGui::Button(exTab == 1 ? "[ Move Generation ]" : "Move Generation")) exTab = 1;
    ImGui::SameLine();
    if (ImGui::Button(exTab == 2 ? "[ Bot Search ]" : "Bot Search")) exTab = 2;
    ImGui::Separator();
    if (exTab == 0) {
        const int t = DrawHistoryNavBar(g_ex);
        if (t >= 0) GoToCursor(g_ex, t);
        ImGui::Separator();
        static size_t seen = 0;
        DrawMoveHistoryTab(g_ex, seen);
    } else if (exTab == 1) {
        DrawMoveGenerationTab();
    } else {
        DrawBotSearchTab(g_bot, *g_gaddag, g_ex.board, g_ex.trayA, g_ex.boardValid, BotSelect);
    }
    ImGui::End();

    DrawLegendFull();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) &&
        g_ex.atEnd() && HasUncommitted(g_ex))
        ExUndoTurn();
}

// ================================================================ Player vs Greedy
static void PvbStartGame(int firstPlayer);

static int RackValue(const Tray& t) {
    int s = 0;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) s += POINTS_PER_TILE[i] * (int)t.counts[i];
    return s;
}

static Placement PvbEvaluateTurn() {
    Placement ps = EvaluatePlacement(g_pvb.board, g_pvb.turnStart().board);
    if (ps.ok && !g_pvb.boardValid) { ps.ok = false; ps.error = "Board has invalid word(s)."; }
    return ps;
}

static void PvbRefill(Tray& tray) {
    const int need = std::min(kRackSize - (int)tray.size(), (int)g_pvb.bag->size());
    if (need > 0) g_pvb.bag->drawInto(tray, (uint8_t)need);
}

// Called after a move or pass by `player` has been recorded in history.
static void PvbFinishTurn(int player) {
    ModeState& ms = g_pvb;
    const Tray& mine = (player == 0) ? ms.trayA : ms.trayB;
    const bool bagEmpty = !ms.bag || ms.bag->size() == 0;
    bool over = false;
    if (bagEmpty && mine.size() == 0) {                 // went out
        const int a = RackValue(ms.trayA), b = RackValue(ms.trayB);
        ms.score[0] -= a; ms.score[1] -= b;
        ms.score[player] += a + b;
        over = true;
    } else if (g_pvbUi.consecutivePasses >= 2) {           // both sides passed
        ms.score[0] -= RackValue(ms.trayA);
        ms.score[1] -= RackValue(ms.trayB);
        over = true;
    }
    if (over) {
        g_pvbUi.gameOver = true;
        g_pvbUi.winner = (ms.score[0] > ms.score[1]) ? 0 : (ms.score[1] > ms.score[0]) ? 1 : -1;
        if (!ms.history.empty()) {                      // final history entry shows final scores
            ms.history.back().score[0] = ms.score[0];
            ms.history.back().score[1] = ms.score[1];
        }
        return;
    }
    g_pvbUi.turn = 1 - player;
    g_pvbUi.botWait = kBotWaitFrames;
}

static void PvbPlayMove() {
    const Placement ps = PvbEvaluateTurn();
    if (!ps.ok) { g_ui.lastAction = "Cannot play: " + ps.error; return; }
    g_pvb.score[0] += ps.points;
    g_pvbUi.consecutivePasses = 0;
    PvbRefill(g_pvb.trayA);                      // refill first so the history entry matches the result
    PushEntry(g_pvb, FormatMove(ps), 0);
    g_ui.lastAction = "Played " + ps.word + ".";
    PvbFinishTurn(0);
}

static void PvbHumanPass() {
    // Return any staged tiles before passing.
    ApplySnapshot(g_pvb, g_pvb.turnStart());
    ++g_pvbUi.consecutivePasses;
    PushEntry(g_pvb, "PASS", 0);
    PvbFinishTurn(0);
}

static void PvbBotTurn() {
    ModeState& ms = g_pvb;
    FullCrossRecompute(ms);
    MoveEvaluator ev; ev.setBoard(&ms.board);
    MoveList sink;
    generateMovesFromScratch(*g_gaddag, ms.board, ms.trayB, sink);
    const RawMove* best = nullptr;
    int bestScore = -1;
    for (auto& rm : sink.moves) {
        const int sc = ev.score(rm).points;
        if (sc > bestScore) { bestScore = sc; best = &rm; }
    }
    if (!best) {
        ++g_pvbUi.consecutivePasses;
        PushEntry(ms, "PASS", 1);
    } else {
        Move mv = toMove(*best);
        g_pvbUi.consecutivePasses = 0;
        for (const auto& pl : mv.letterPositions()) {
            const int idx = sq(pl.row, pl.col);
            if (ms.board.letters_row[idx] != EMPTY_SQUARE) continue;
            placeTile(ms.board, pl.row, pl.col, pl.letter, pl.is_blank);
            ms.trayB.removeTile(pl.is_blank ? Letter::Blank : pl.letter);
        }
        ms.score[1] += bestScore;
        PvbRefill(ms.trayB);
        PushEntry(ms, FormatMoveLabel(mv.word, mv.row, mv.col, mv.is_descending, bestScore), 1);
    }
    RefreshDerived(ms);
    PvbFinishTurn(1);
}

static void PvbStartGame(int firstPlayer) {
    std::random_device rd;
    const uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
    g_pvb.bag = std::make_unique<TileBag>(seed);
    g_pvb.board = Board{};
    g_pvb.trayA = Tray{};
    g_pvb.trayB = Tray{};
    g_pvb.bag->drawInto(g_pvb.trayA, kRackSize);
    g_pvb.bag->drawInto(g_pvb.trayB, kRackSize);
    g_pvb.history.clear();
    g_pvb.cursor = 0;
    g_pvb.score[0] = g_pvb.score[1] = 0;
    g_pvbUi.gameOver = false;
    g_pvbUi.gameOverDismissed = false;
    g_pvbUi.winner = -1;
    g_pvbUi.consecutivePasses = 0;
    g_pvbUi.turn = firstPlayer;
    g_pvbUi.botWait = kBotWaitFrames;
    g_ui.lastAction = "(game started)";
    RefreshDerived(g_pvb);
    g_pvb.initial = SnapshotOf(g_pvb);
    EndDrag();
    g_mode = AppMode::PlayerVsBot;
}

static void RequestRematchOrTitle(ConfirmAction a) {
    const bool inProgress = !g_pvbUi.gameOver && !g_pvb.history.empty();
    if (inProgress) { g_ui.confirmAction = a; g_ui.confirmBackOpen = true; return; }
    if (a == ConfirmAction::Title) LeaveToTitle();
    else PvbStartGame((int)(std::random_device{}() & 1));
}

static void PvbTopOnBack()    { RequestRematchOrTitle(ConfirmAction::Title); }
static void PvbTopOnRematch() { RequestRematchOrTitle(ConfirmAction::Rematch); }

static void DrawPvbTop() {
    TopStripCallbacks cb;
    cb.onBack    = PvbTopOnBack;
    cb.onRematch = PvbTopOnRematch;
    cb.extra     = nullptr;
    cb.modeName  = "Player vs Greedy";
    DrawModeTopStrip(g_pvb, cb);
}

static void DrawPvbCream() {
    PushCreamStyle();
    ImGui::Begin("Cream##pvb", nullptr, PANEL_FLAGS);

    const bool interactive = g_pvb.atEnd() && g_pvbUi.turn == 0 && !g_pvbUi.gameOver;

    DrawBoardAndBag(BoardKind::Pvb, g_pvb, interactive, LiveBagView(g_pvb));

    ImGui::Separator();
    DrawTrayInteractive(g_pvb, interactive, false);

    Placement ps;
    if (interactive) ps = PvbEvaluateTurn();
    const bool canPlay = interactive && ps.ok;

    PushPanelButton();
    if (!canPlay) ImGui::BeginDisabled();
    if (ImGui::Button("Play Move", ImVec2(120, 32))) PvbPlayMove();
    if (!canPlay) ImGui::EndDisabled();

    ImGui::SameLine();
    const bool canUndo = interactive && HasUncommitted(g_pvb);
    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo Turn", ImVec2(120, 32))) {
        ApplySnapshot(g_pvb, g_pvb.turnStart());
        g_ui.lastAction = "Undid turn.";
    }
    if (!canUndo) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!interactive) ImGui::BeginDisabled();
    if (ImGui::Button("Pass", ImVec2(80, 32))) PvbHumanPass();
    if (!interactive) ImGui::EndDisabled();
    PopPanelButton();

    if (interactive && !ps.ok && !ps.error.empty()) ImGui::TextDisabled("%s", ps.error.c_str());
    else if (interactive && ps.ok) ImGui::TextDisabled("%s: +%d", ps.word.c_str(), ps.points);
    else ImGui::TextDisabled("Last: %s", g_ui.lastAction.c_str());

    ImGui::Separator();
    const bool viewing = !g_pvb.atEnd();
    const char* statusTxt = g_pvbUi.gameOver ? "GAME OVER" :
                            (viewing ? "VIEWING HISTORY" :
                             (g_pvbUi.turn == 0 ? "YOUR TURN" : "BOT'S TURN"));
    const ImU32 statusCol = g_pvbUi.gameOver ? IM_COL32(220, 80, 80, 255) :
                            (viewing ? IM_COL32(200, 160, 60, 255) :
                             (g_pvbUi.turn == 0 ? IM_COL32(80, 180, 80, 255) : IM_COL32(220, 180, 60, 255)));
    DrawScoresLine("You", g_pvb.score[0], "Bot", g_pvb.score[1], statusTxt, statusCol);

    ImGui::End();
    PopCreamStyle();
}

static void DrawPvbMode() {
    if (!BeginModeScreen(AppMode::PlayerVsBot, kTopPvb, g_ui.dockBuiltPvb,
                         "Cream##pvb", "Right Panel##pvb", 180.0f, 440.0f,
                         DrawPvbTop))
        return;

    DrawPvbCream();

    ImGui::Begin("Right Panel##pvb", nullptr, PANEL_FLAGS);
    {
        const int t = DrawHistoryNavBar(g_pvb);
        if (t >= 0) GoToCursor(g_pvb, t);
    }
    ImGui::Separator();
    { static size_t seen = 0; DrawMoveHistoryTab(g_pvb, seen); }
    ImGui::End();

    DrawLegendCompact();

    if (!g_pvbUi.gameOver && g_pvbUi.turn == 1 && g_pvb.atEnd()) {
        if (g_pvbUi.botWait > 0) --g_pvbUi.botWait;
        else PvbBotTurn();
    }

    const bool dialogOpen = g_ui.blankPopupOpen || g_ui.confirmBackOpen || g_ui.savePopupOpen || g_ui.loadPopupOpen;
    if (g_pvbUi.turn == 0 && !g_pvbUi.gameOver && g_pvb.atEnd() && !dialogOpen &&
        !ImGui::GetIO().WantTextInput &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
        if (PvbEvaluateTurn().ok) PvbPlayMove();
    }

    if (g_pvbUi.gameOver && !g_pvbUi.gameOverDismissed) {
        CenterNextWindow();
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
        ImGui::Begin("Game Over##pvb", nullptr, POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Final: You = %d   Bot = %d", g_pvb.score[0], g_pvb.score[1]);
        if (g_pvbUi.winner < 0) ImGui::TextUnformatted("Tie!");
        else if (g_pvbUi.winner == 0) ImGui::TextUnformatted("You win!");
        else ImGui::TextUnformatted("Bot wins.");
        ImGui::Separator();
        const bool back    = ImGui::Button("Back to Title", ImVec2(-1, 32));
        const bool rematch = ImGui::Button("Rematch", ImVec2(-1, 32));
        const bool view    = ImGui::Button("View Board", ImVec2(-1, 0));
        ImGui::End();
        if (back)         LeaveToTitle();
        else if (rematch) PvbStartGame((int)(std::random_device{}() & 1));
        else if (view)    g_pvbUi.gameOverDismissed = true;
    }
}

// ================================================================ Greedy vs Greedy
static const Snapshot& SimViewSnapshot() {
    return g_simState.turnStart();
}
// The sim view is always drawn from snapshots (initial / history entries), never from the
// live GreedySim, so stepping the sim never changes what a history view shows.
static void SimSyncView() { ApplyVisible(g_simState, SimViewSnapshot()); }

static void SimGoTo(int target) {
    g_simState.cursor = std::clamp(target, 0, (int)g_simState.history.size());
    SimSyncView();
}

static void SimPushHistory(const GreedySim::Turn& t) {
    Entry e;
    static_cast<Snapshot&>(e) = MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                                             g_simUi.sim.bag, g_simUi.sim.score[0], g_simUi.sim.score[1]);
    e.turnNumber = t.turnNumber;
    e.player = t.player;
    e.label = t.passed ? std::string("PASS")
                       : FormatMoveLabel(t.move.word, t.move.row, t.move.col, t.move.is_descending, t.score);
    g_simState.history.push_back(std::move(e));
    g_simState.cursor = (int)g_simState.history.size();
    SimSyncView();
}

static bool SimStepAndRecord() {
    if (g_simUi.sim.gameOver) return false;
    const int before = (int)g_simUi.sim.turns.size();
    if (!g_simUi.sim.step()) return false;
    if ((int)g_simUi.sim.turns.size() > before) SimPushHistory(g_simUi.sim.turns.back());
    return true;
}

static void SimReset() {
    std::random_device rd;
    const uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
    g_simUi.sim.reset(g_gaddag, seed);
    g_simState.history.clear();
    g_simState.cursor = 0;
    g_simState.initial = MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                                      g_simUi.sim.bag, g_simUi.sim.score[0], g_simUi.sim.score[1]);
    SimSyncView();
    g_simUi.started = true;
    g_simUi.replay = false;
    g_simUi.autoRunning = false;
    g_simUi.lastStep = ImGui::GetTime();
}

static void SimTopOnBack()    { LeaveToTitle(); }
static void SimTopOnRematch() { SimReset(); }

static void SimTopExtra() {
    const bool live = !g_simUi.replay && !g_simUi.sim.gameOver;
    if (!live) ImGui::BeginDisabled();
    if (ImGui::Button("Step")) {
        g_simUi.autoRunning = false;
        SimGoTo((int)g_simState.history.size());   // stepping always acts on the live end
        SimStepAndRecord();
    }
    ImGui::SameLine();
    if (ImGui::Button(g_simUi.autoRunning ? "Pause" : "Play")) {
        g_simUi.autoRunning = !g_simUi.autoRunning;
        if (g_simUi.autoRunning) {
            SimGoTo((int)g_simState.history.size());
            g_simUi.lastStep = ImGui::GetTime();
        }
    }
    if (!live) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##simspeed", &g_simUi.speedMs, 10.0f, 1000.0f, "%.0f ms");
}

static void DrawSimTop() {
    TopStripCallbacks cb;
    cb.onBack    = SimTopOnBack;
    cb.onRematch = SimTopOnRematch;
    cb.extra     = SimTopExtra;
    cb.modeName  = "Greedy vs Greedy";
    DrawModeTopStrip(g_simState, cb);
}

static void DrawSimCream() {
    PushCreamStyle();
    ImGui::Begin("Cream##sim", nullptr, PANEL_FLAGS);

    DrawBoardAndBag(BoardKind::Sim, g_simState, false, SnapshotBagView(SimViewSnapshot()));

    ImGui::Separator();
    DrawTrayStrip(TrayLetters(g_simState.trayA), false, nullptr);
    ImGui::Spacing();
    DrawTrayStrip(TrayLetters(g_simState.trayB), false, nullptr);

    ImGui::Separator();
    const bool atEnd = g_simState.atEnd();
    const char* statusTxt = !atEnd ? "VIEWING HISTORY" :
                            g_simUi.replay ? "REPLAY" :
                            g_simUi.sim.gameOver ? "GAME OVER" :
                            (g_simState.history.empty() ? "READY" : "RUNNING");
    const ImU32 statusCol = !atEnd ? IM_COL32(200, 160, 60, 255) :
                            (g_simUi.replay || g_simUi.sim.gameOver) ? IM_COL32(220, 80, 80, 255) :
                            IM_COL32(80, 180, 80, 255);
    DrawScoresLine("P1", g_simState.score[0], "P2", g_simState.score[1], statusTxt, statusCol);

    ImGui::End();
    PopCreamStyle();
}

static void DrawSimMode() {
    // Sim needs SimReset() before the dock is built the first time.
    if (!g_simUi.started) SimReset();

    if (!BeginModeScreen(AppMode::GreedyVsGreedy, kTopSim, g_ui.dockBuiltSim,
                         "Cream##sim", "Right Panel##sim", 180.0f, 440.0f,
                         DrawSimTop))
        return;

    DrawSimCream();

    ImGui::Begin("Right Panel##sim", nullptr, PANEL_FLAGS);
    {
        const int t = DrawHistoryNavBar(g_simState);
        if (t >= 0) SimGoTo(t);
    }
    ImGui::Separator();
    { static size_t seen = 0; DrawMoveHistoryTab(g_simState, seen); }
    ImGui::End();

    DrawLegendCompact();

    if (g_simUi.autoRunning && !g_simUi.replay) {
        if (g_simUi.sim.gameOver) g_simUi.autoRunning = false;
        else if (g_simState.atEnd()) {
            const double now = ImGui::GetTime();
            if (now - g_simUi.lastStep >= g_simUi.speedMs / 1000.0) {
                if (!SimStepAndRecord()) g_simUi.autoRunning = false;
                g_simUi.lastStep = now;
            }
        }
    }
}

// ================================================================ save / load
static fs::path SaveDir() {
    const char* home = std::getenv("USERPROFILE");
    if (!home) home = std::getenv("HOME");
    std::error_code ec;
    if (home) {
        fs::path d = fs::path(home) / "Downloads";
        if (fs::is_directory(d, ec)) return d;
        return fs::path(home);
    }
    return fs::current_path(ec);
}

static std::string SanitizeName(const char* s) {
    std::string out;
    for (const char* p = s; *p && out.size() < 40; ++p)
        if (std::isalnum((unsigned char)*p)) out.push_back(*p);
    return out.empty() ? std::string("session") : out;
}
static int AlnumFilter(ImGuiInputTextCallbackData* d) {
    return (d->EventChar < 128 && std::isalnum((int)d->EventChar)) ? 0 : 1;
}

static const char* ModeName(AppMode m) {
    switch (m) {
        case AppMode::Exploration:    return "exploration";
        case AppMode::PlayerVsBot:    return "pvb";
        case AppMode::GreedyVsGreedy: return "sim";
        default:                      return "unknown";
    }
}

static void WriteSnapshot(std::ostream& o, const Snapshot& s) {
    std::string brd(NUM_SQUARES, '.');
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const int idx = sq(r, c);
            const uint8_t code = s.board.letters_row[idx];
            if (code == EMPTY_SQUARE) continue;
            char ch = toChar(fromBoardCode(code));
            if (s.board.is_blank.test(idx)) ch = (char)std::tolower((unsigned char)ch);
            brd[r * BOARD_LENGTH + c] = ch;
        }
    }
    o << "board " << brd << "\n";
    o << "trayA"; for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.trayA.counts[i]; o << "\n";
    o << "trayB"; for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.trayB.counts[i]; o << "\n";
    o << "bagcounts"; for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.bagCounts[i]; o << "\n";
    o << "bag " << s.bagTotal;
    for (int i = 0; i < (int)s.bagTotal && i < TOTAL_TILES; ++i) o << ' ' << (int)toIndex(s.bagTiles[i]);
    o << "\n";
    o << "rng " << s.bagRng << "\n";
    o << "score " << s.score[0] << ' ' << s.score[1] << "\n";
}

static bool ReadSnapshot(std::istream& in, Snapshot& s) {
    std::string tag, brd;
    if (!(in >> tag >> brd) || tag != "board" || (int)brd.size() != NUM_SQUARES) return false;
    s = Snapshot{};
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const char ch = brd[r * BOARD_LENGTH + c];
            if (ch == '.') continue;
            if (!std::isalpha((unsigned char)ch)) return false;
            const bool blank = std::islower((unsigned char)ch) != 0;
            const int li = std::toupper((unsigned char)ch) - 'A';
            if (li < 0 || li >= NUM_LETTERS) return false;
            placeTile(s.board, r, c, fromIndex((uint8_t)li), blank);
        }
    }
    auto readTray = [&](const char* want, Tray& t) {
        if (!(in >> tag) || tag != want) return false;
        t = Tray{};
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
            int n;
            if (!(in >> n) || n < 0 || n > TOTAL_TILES) return false;
            for (int k = 0; k < n; ++k) t.addTile(fromIndex((uint8_t)i));
        }
        return true;
    };
    if (!readTray("trayA", s.trayA) || !readTray("trayB", s.trayB)) return false;
    if (!(in >> tag) || tag != "bagcounts") return false;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        int n; if (!(in >> n) || n < 0 || n > 255) return false;
        s.bagCounts[i] = (uint8_t)n;
    }
    int total;
    if (!(in >> tag >> total) || tag != "bag" || total < 0 || total > TOTAL_TILES) return false;
    s.bagTotal = (uint16_t)total;
    for (int i = 0; i < total; ++i) {
        int v; if (!(in >> v) || v < 0 || v >= NUM_LETTER_SLOTS) return false;
        s.bagTiles[i] = fromIndex((uint8_t)v);
    }
    if (!(in >> tag) || tag != "rng" || !(in >> s.bagRng)) return false;
    if (!(in >> tag >> s.score[0] >> s.score[1]) || tag != "score") return false;
    return true;
}

static bool SaveState(const fs::path& file, std::string& err) {
    std::ofstream out(file, std::ios::trunc);
    if (!out) { err = "Could not open file for writing."; return false; }
    ModeState& ms = StateForMode(g_mode);
    out << "VISION_STATE 3\n";
    out << "mode " << ModeName(g_mode) << "\n";
    out << "cursor " << ms.cursor << "\n";
    if (g_mode == AppMode::PlayerVsBot)
        out << "pvb " << g_pvbUi.turn << ' ' << g_pvbUi.consecutivePasses << ' '
            << (int)g_pvbUi.gameOver << ' ' << g_pvbUi.winner << "\n";
    else
        out << "pvb 0 0 0 -1\n";
    out << "current\n";  WriteSnapshot(out, SnapshotOf(ms));
    out << "initial\n";  WriteSnapshot(out, ms.initial);
    out << "entries " << ms.history.size() << "\n";
    for (const Entry& e : ms.history) {
        out << "entry " << e.turnNumber << ' ' << e.player << "\n";
        out << "label " << e.label << "\n";
        WriteSnapshot(out, e);
    }
    out.flush();
    if (!out) { err = "Write failed."; return false; }
    return true;
}

struct LoadedState {
    std::string mode;
    int cursor = 0;
    int pvbTurn = 0, pvbPasses = 0, pvbOver = 0, pvbWinner = -1;
    Snapshot current, initial;
    std::vector<Entry> entries;
};

static bool ReadStateFile(const fs::path& file, LoadedState& L, std::string& err) {
    std::ifstream in(file);
    if (!in) { err = "Could not open file."; return false; }
    std::string tag; int version = 0;
    if (!(in >> tag >> version) || tag != "VISION_STATE") { err = "Not a Vision save file."; return false; }
    if (version != 3) { err = "Unsupported save version (" + std::to_string(version) + ")."; return false; }
    if (!(in >> tag >> L.mode) || tag != "mode") { err = "Corrupt file (mode)."; return false; }
    if (!(in >> tag >> L.cursor) || tag != "cursor") { err = "Corrupt file (cursor)."; return false; }
    if (!(in >> tag >> L.pvbTurn >> L.pvbPasses >> L.pvbOver >> L.pvbWinner) || tag != "pvb") { err = "Corrupt file (pvb)."; return false; }
    if (!(in >> tag) || tag != "current" || !ReadSnapshot(in, L.current)) { err = "Corrupt file (current)."; return false; }
    if (!(in >> tag) || tag != "initial" || !ReadSnapshot(in, L.initial)) { err = "Corrupt file (initial)."; return false; }
    int n;
    if (!(in >> tag >> n) || tag != "entries" || n < 0 || n > 100000) { err = "Corrupt file (entries)."; return false; }
    for (int i = 0; i < n; ++i) {
        Entry e;
        if (!(in >> tag >> e.turnNumber >> e.player) || tag != "entry") { err = "Corrupt file (entry)."; return false; }
        std::string line;
        std::getline(in, line);                  // rest of the "entry" line
        std::getline(in, line);                  // "label ..."
        e.label = line.size() > 6 ? line.substr(6) : std::string();
        Snapshot s;
        if (!ReadSnapshot(in, s)) { err = "Corrupt file (entry snapshot)."; return false; }
        static_cast<Snapshot&>(e) = s;
        L.entries.push_back(std::move(e));
    }
    L.cursor = std::clamp(L.cursor, 0, (int)L.entries.size());
    return true;
}

static bool ApplyLoadedState(LoadedState& L, std::string& err) {
    if (L.mode != ModeName(g_mode)) {
        err = "That save is for mode '" + L.mode + "'; open that mode to load it.";
        return false;
    }
    ModeState& ms = StateForMode(g_mode);
    if (g_mode != AppMode::GreedyVsGreedy && !ms.bag) ms.bag = std::make_unique<TileBag>(uint64_t{1});
    ms.history = std::move(L.entries);
    ms.initial = L.initial;
    ms.cursor  = L.cursor;
    EndDrag();
    if (g_mode == AppMode::GreedyVsGreedy) {
        g_simUi.started = true;
        g_simUi.replay = true;
        g_simUi.autoRunning = false;
        SimSyncView();
    } else {
        ApplySnapshot(ms, L.current);
        if (g_mode == AppMode::PlayerVsBot) {
            g_pvbUi.turn = L.pvbTurn;
            g_pvbUi.consecutivePasses = L.pvbPasses;
            g_pvbUi.gameOver = L.pvbOver != 0;
            g_pvbUi.gameOverDismissed = false;
            g_pvbUi.winner = L.pvbWinner;
            g_pvbUi.botWait = kBotWaitFrames;
        }
    }
    return true;
}

static void RefreshLoadList() {
    g_ui.loadFiles.clear();
    std::error_code ec;
    for (fs::directory_iterator it(SaveDir(), ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (!it->is_regular_file(ec2)) continue;
        const std::string fn = it->path().filename().string();
        if (fn.size() > 14 && fn.rfind("vision_", 0) == 0 &&
            fn.compare(fn.size() - 7, 7, ".vstate") == 0)
            g_ui.loadFiles.push_back(fn);
    }
    std::sort(g_ui.loadFiles.begin(), g_ui.loadFiles.end());
    g_ui.loadListDirty = false;
}

// ================================================================ popups
static void DrawBlankPopup() {
    if (!g_ui.blankPopupOpen) return;
    CenterNextWindow();
    ImGui::Begin("Assign blank letter", &g_ui.blankPopupOpen,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Placing blank at (%d,%d). Choose its letter:", g_ui.blankDropR, g_ui.blankDropC);
    ImGui::Separator();
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 13; ++col) {
            const int i = row * 13 + col;
            if (i >= NUM_LETTERS) break;
            const char buf[2] = { (char)('A' + i), 0 };
            if (col > 0) ImGui::SameLine();
            if (ImGui::Button(buf, ImVec2(26, 26))) {
                std::string reason;
                ModeState* ms = g_ui.blankMs;
                // The popup is modeless, so re-check that the position is still editable.
                const bool editable = ms && ms->atEnd() &&
                                      (ms != &g_pvb || (g_pvbUi.turn == 0 && !g_pvbUi.gameOver));
                if (editable && ms->trayA.count(Letter::Blank) > 0 &&
                    CanDropTile(ms->board, g_ui.blankDropR, g_ui.blankDropC, reason)) {
                    CompleteTrayDrop(*ms, g_ui.blankDropR, g_ui.blankDropC, fromIndex((uint8_t)i), true);
                } else {
                    if (!editable)      reason = "position is no longer editable";
                    else if (reason.empty()) reason = "no blank in rack";
                    g_ui.lastAction = "Rejected: " + reason;
                }
                g_ui.blankPopupOpen = false;
            }
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Cancel", ImVec2(-1, 0))) g_ui.blankPopupOpen = false;
    ImGui::End();
}

static void DrawAddTilePopup() {
    if (!g_ui.addTilePopupOpen) return;
    CenterNextWindow();
    ImGui::Begin("Add tile to rack", &g_ui.addTilePopupOpen,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted("Pick a letter from the bag.");
    ImGui::Text("Bag: %u tiles   Rack: %u/%d", g_ex.bag ? (unsigned)g_ex.bag->size() : 0u,
                (unsigned)g_ex.trayA.size(), kRackSize);
    ImGui::Separator();
    const BagCounts bagCounts = g_ex.bag ? g_ex.bag->counts() : BagCounts{};
    const bool rackFull = (int)g_ex.trayA.size() >= kRackSize;
    const bool canEdit  = g_ex.atEnd();     // never edit a historical position
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 9; ++col) {
            const int i = row * 9 + col;
            if (i >= NUM_LETTER_SLOTS) break;
            const char ch = (i == NUM_LETTER_SLOTS - 1) ? '?' : toChar(fromIndex((uint8_t)i));
            if (col > 0) ImGui::SameLine();
            const bool enabled = bagCounts[i] > 0 && !rackFull && canEdit;
            if (!enabled) ImGui::BeginDisabled();
            char id[40]; std::snprintf(id, sizeof id, "%c\n%d##add%d", ch, (int)bagCounts[i], i);
            if (ImGui::Button(id, ImVec2(40, 42)) && g_ex.bag) {
                const Letter L = (i == NUM_LETTER_SLOTS - 1) ? Letter::Blank : fromIndex((uint8_t)i);
                if (g_ex.bag->count(L) > 0) {
                    g_ex.bag->removeLetter(L);
                    g_ex.trayA.addTile(L);
                    g_ui.lastAction = "Added tile from bag.";
                }
            }
            if (!enabled) ImGui::EndDisabled();
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(-1, 0))) g_ui.addTilePopupOpen = false;
    ImGui::End();
}

static void DrawSavePopup() {
    if (!g_ui.savePopupOpen) return;
    CenterNextWindow();
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    ImGui::Begin("Save State", &g_ui.savePopupOpen,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextUnformatted("Save name (letters/numbers only):");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##saveName", g_ui.saveName, sizeof g_ui.saveName,
                     ImGuiInputTextFlags_CallbackCharFilter, AlnumFilter);

    const fs::path file = SaveDir() / (std::string("vision_") + SanitizeName(g_ui.saveName) + ".vstate");
    ImGui::TextDisabled("%s", file.string().c_str());

    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(-1, 32))) {
        std::string err;
        g_ui.saveMessage = SaveState(file, err) ? "Saved to " + file.string() : "Failed to save: " + err;
        g_ui.loadListDirty = true;
    }
    if (ImGui::Button("Cancel", ImVec2(-1, 0))) g_ui.savePopupOpen = false;
    if (!g_ui.saveMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", g_ui.saveMessage.c_str());
    }
    ImGui::End();
}

static void DrawLoadPopup() {
    if (!g_ui.loadPopupOpen) return;
    CenterNextWindow();
    ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_Appearing);
    ImGui::Begin("Load State", &g_ui.loadPopupOpen, POPUP_FLAGS);

    const fs::path dir = SaveDir();
    ImGui::Text("Directory: %s", dir.string().c_str());
    ImGui::Separator();
    if (g_ui.loadListDirty) RefreshLoadList();

    ImGui::BeginChild("##list", ImVec2(0, 240), true);
    if (g_ui.loadFiles.empty()) ImGui::TextDisabled("(no saved states found)");
    for (const std::string& fn : g_ui.loadFiles) {
        if (ImGui::Selectable(fn.c_str())) {
            LoadedState L; std::string err;
            if (ReadStateFile(dir / fn, L, err) && ApplyLoadedState(L, err)) {
                g_ui.loadMessage = "Loaded: " + fn;
                g_ui.lastAction = "Loaded " + fn;
            } else {
                g_ui.loadMessage = "Load failed: " + err;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Refresh", ImVec2(120, 0))) g_ui.loadListDirty = true;
    ImGui::SameLine();
    if (ImGui::Button("Open folder", ImVec2(-1, 0))) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
        // file:///C:/Users/x (Windows) vs file:///home/x (POSIX); percent-encode characters that break URLs.
        const std::string path = dir.generic_string();
        std::string url = "file://";
        if (path.empty() || path[0] != '/') url += '/';
        for (unsigned char ch : path) {
            if (ch == ' ' || ch == '#' || ch == '%' || ch == '?') {
                char enc[4]; std::snprintf(enc, sizeof enc, "%%%02X", (unsigned)ch);
                url += enc;
            } else url += (char)ch;
        }
        if (SDL_OpenURL(url.c_str()) != 0) g_ui.loadMessage = std::string("Could not open folder: ") + SDL_GetError();
#else
        g_ui.loadMessage = "Opening folders needs SDL 2.0.14+.";
#endif
    }
    if (!g_ui.loadMessage.empty()) ImGui::TextWrapped("%s", g_ui.loadMessage.c_str());
    ImGui::End();
}

static void DrawHowToPlay() {
    if (!g_ui.howToPlayOpen) return;
    CenterNextWindow();
    ImGui::SetNextWindowSize(ImVec2(650, 750), ImGuiCond_Always);
    ImGui::Begin("How to Play", &g_ui.howToPlayOpen,
                 POPUP_FLAGS | ImGuiWindowFlags_NoResize);

    ImGui::TextUnformatted("Modes"); ImGui::Separator();
    ImGui::BulletText("Player vs Greedy  -- play against the bot.");
    ImGui::BulletText("Greedy vs Greedy  -- watch two bots play (Play/Pause, Step, speed).");
    ImGui::BulletText("Exploration  -- sandbox for move analysis and search.");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Controls"); ImGui::Separator();
    ImGui::BulletText("Drag a tile from your rack onto the board to place it.");
    ImGui::BulletText("Dragging a blank asks which letter it stands for.");
    ImGui::BulletText("Right-click a tile you placed this turn to return it to your rack.");
    ImGui::BulletText("Left-drag a tile you placed this turn to move it.");
    ImGui::BulletText("In Exploration, right-click a rack tile to return it to the bag.");
    ImGui::BulletText("Play Move commits your placement if it forms valid words.");
    ImGui::BulletText("Undo Turn reverts every tile placed this turn.");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Exploration right panel"); ImGui::Separator();
    ImGui::BulletText("Move History   -- log of committed turns.");
    ImGui::BulletText("Move Generation -- every legal move from the current board; filter, sort, select to preview.");
    ImGui::BulletText("Bot Search      -- run alpha-beta or MCTS; select a result to preview it.");
    ImGui::BulletText("Click a row in Move Generation or Bot Search to highlight that move on the board.");
    ImGui::BulletText("With a move highlighted and no tiles staged, Play Move plays it for you.");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Bot Search"); ImGui::Separator();
    ImGui::BulletText("Determinized AB -- alpha-beta search on a sampled full-information state.");
    ImGui::BulletText("MCTS            -- Monte Carlo Tree Search with random playouts.");
    ImGui::BulletText("Hover any option for a description of what it does.");
    ImGui::BulletText("Start Search begins; Stop ends it early and returns the best move found.");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Shortcuts"); ImGui::Separator();
    ImGui::BulletText("Esc        cancel drag / close popup / return to Title");
    ImGui::BulletText("Ctrl+Z     undo turn (Exploration)");
    ImGui::BulletText("Enter      Play Move (Player vs Greedy)");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Board"); ImGui::Separator();
    ImGui::BulletText("2L / 3L: double / triple the letter's point value.");
    ImGui::BulletText("2W / 3W: double / triple the whole word's score.");
    ImGui::BulletText("Cyan-outlined empty squares are anchors.");
    ImGui::BulletText("Invalid words are outlined in red.");
    ImGui::BulletText("In Exploration, square colour shows cross-check heat (how many letters can play there).");
    ImGui::Dummy(ImVec2(0, 8));

    ImGui::TextUnformatted("Game end (Player vs Greedy)"); ImGui::Separator();
    ImGui::BulletText("A player goes out with the bag empty, or both players pass in a row.");
    ImGui::BulletText("Leftover tile values are deducted; the player who went out gains the opponent's.");
    ImGui::End();
}

static void DrawConfirmBackPopup() {
    if (!g_ui.confirmBackOpen) return;
    CenterNextWindow();
    ImGui::Begin("Confirm", &g_ui.confirmBackOpen,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted(g_ui.confirmAction == ConfirmAction::Title
                           ? "Leave the current game? Progress will be lost."
                           : "Start a new game? Progress will be lost.");
    ImGui::Separator();
    if (ImGui::Button(g_ui.confirmAction == ConfirmAction::Title ? "Yes, back to Title" : "Yes, rematch",
                      ImVec2(-1, 32))) {
        g_ui.confirmBackOpen = false;
        if (g_ui.confirmAction == ConfirmAction::Title) LeaveToTitle();
        else PvbStartGame((int)(std::random_device{}() & 1));
    }
    if (ImGui::Button("Cancel", ImVec2(-1, 32))) g_ui.confirmBackOpen = false;
    ImGui::End();
}

// ================================================================ Title screen
static void DrawTitleScreen() {
    // Fill the viewport background so the empty area around the title card reads as
    // intentional rather than as unrendered space.
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
            IM_COL32(24, 24, 30, 255));
    }

    CenterNextWindow(ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(38, 38, 46, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::Begin("##title", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_AlwaysAutoResize);

    {
        const float w = ImGui::GetContentRegionAvail().x;
        const char* t = "Vision";
        ImGui::SetWindowFontScale(3.2f);
        const float tw = ImGui::CalcTextSize(t).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }
    {
        const float w = ImGui::GetContentRegionAvail().x;
        const char* s = "Gaddag engine + greedy AI";
        const float sw = ImGui::CalcTextSize(s).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - sw) * 0.5f);
        ImGui::TextDisabled("%s", s);
    }
    ImGui::Dummy(ImVec2(0, 16));

    const float bw = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(130, 185, 230, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(90, 145, 200, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    if (ImGui::Button("Player vs Greedy", ImVec2(bw, 48))) g_pvbUi.firstPlayerDialog = true;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6);
    ImGui::TextDisabled("you vs the bot");
    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::Button("Greedy vs Greedy", ImVec2(bw, 48))) {
        g_simUi.started = false;
        g_ui.dockBuiltSim = false;
        g_mode = AppMode::GreedyVsGreedy;
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6);
    ImGui::TextDisabled("watch two bots play");
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 92, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(105, 105, 118, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 65, 76, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(235,235,240,255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    if (ImGui::Button("Exploration", ImVec2(bw, 48))) {
        g_ui.dockBuiltEx = false;
        g_mode = AppMode::Exploration;
        if (!g_ex.bag) ExplorationReset();
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6);
    ImGui::TextDisabled("sandbox & debug");
    ImGui::Dummy(ImVec2(0, 16));

    if (ImGui::Button("How to Play", ImVec2(bw, 32))) g_ui.howToPlayOpen = true;
    ImGui::Dummy(ImVec2(0, 6));
    if (ImGui::Button("Quit", ImVec2(bw, 32))) {
        SDL_Event q{}; q.type = SDL_QUIT; SDL_PushEvent(&q);
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 14));
    {
        const float w = ImGui::GetContentRegionAvail().x;
        const char* v = "v0.4";
        const float vw = ImGui::CalcTextSize(v).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - vw) * 0.5f);
        ImGui::TextDisabled("%s", v);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (g_pvbUi.firstPlayerDialog) {
        CenterNextWindow();
        ImGui::Begin("Who goes first?", &g_pvbUi.firstPlayerDialog,
                     POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Choose the starting player.");
        ImGui::Separator();
        int choice = -1;
        if (ImGui::Button("You first", ImVec2(-1, 32))) choice = 0;
        if (ImGui::Button("Bot first", ImVec2(-1, 32))) choice = 1;
        if (ImGui::Button("Random",    ImVec2(-1, 32))) choice = (int)(std::random_device{}() & 1);
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) g_pvbUi.firstPlayerDialog = false;
        ImGui::End();
        if (choice >= 0) {
            g_pvbUi.firstPlayerDialog = false;
            g_ui.dockBuiltPvb = false;
            PvbStartGame(choice);
        }
    }
}

// ================================================================ main
static std::string MaterializeEmbeddedWordList() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec) dir = fs::current_path(ec);
    fs::path out = dir / "vision_wordlist.txt";
    if (fs::exists(out, ec) && (std::size_t)fs::file_size(out, ec) == kWordListSize)
        return out.string();
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    if (!f) return {};
    f.write(reinterpret_cast<const char*>(kWordListData), (std::streamsize)kWordListSize);
    return f ? out.string() : std::string{};
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::printf("SDL_Init: %s\n", SDL_GetError()); return 1;
    }

    // Load the dictionary first so a missing file produces a clear message instead of a crash.
    std::unique_ptr<Gaddag> gaddag;
    {
        const std::string path = MaterializeEmbeddedWordList();
        std::string failure;
        if (path.empty()) {
            failure = "Could not find wordList.txt.\nPass the path as the first argument, set VISION_WORDLIST, "
                      "or place wordList.txt next to the executable.";
        } else {
            try { gaddag = std::make_unique<Gaddag>(path.c_str()); }
            catch (const std::exception& ex) { failure = std::string("Failed to load word list:\n") + path + "\n" + ex.what(); }
        }
        if (!gaddag) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vision", failure.c_str(), nullptr);
            SDL_Quit();
            return 1;
        }
        g_gaddag = gaddag.get();
    }

    // Query the display's usable bounds so the window fits on smaller screens
    // (laptops) as well as large desktops.
    SDL_DisplayMode dm{};
    int displayW = 1440, displayH = 900;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        displayW = dm.w;
        displayH = dm.h;
    }

    // Target size we'd like if the screen is big enough.
    constexpr int kTargetW = 1920, kTargetH = 1080;
    // Absolute minimum below which the layout gets cramped; will be relaxed if
    // the display is smaller than this.
    constexpr int kMinAbsoluteW = 1500, kMinAbsoluteH = 840;
    // Leave headroom for the OS taskbar / window chrome.
    const int maxFitW = std::max(800, displayW - 20);
    const int maxFitH = std::max(600, displayH - 60);

    const int kDefaultW = std::min(kTargetW, maxFitW);
    const int kDefaultH = std::min(kTargetH, maxFitH);
    const int kMinW     = std::min(kMinAbsoluteW, kDefaultW);
    const int kMinH     = std::min(kMinAbsoluteH, kDefaultH);

    SDL_Window* window = SDL_CreateWindow("Vision",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kDefaultW, kDefaultH,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { std::printf("SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(window, kMinW, kMinH);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer) { std::printf("SDL_CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;     // dock layouts are rebuilt (and locked) in code; don't persist them
    ImGui::StyleColorsDark();

    {
        ImGuiIO& io2 = ImGui::GetIO();
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = true;
        cfg.FontDataOwnedByAtlas = false;
        g_fontMono = io2.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(kFontMonoData), (int)kFontMonoSize, 15.0f, &cfg);
        g_fontFranklin = io2.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(kFontFranklinData), (int)kFontFranklinSize, 15.0f, &cfg);
        if (!g_fontMono)     g_fontMono = io2.Fonts->AddFontDefault();
        if (!g_fontFranklin) g_fontFranklin = g_fontMono;
    }

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 6.0f;
    st.FrameRounding  = 6.0f;
    st.GrabRounding   = 6.0f;
    st.WindowPadding  = ImVec2(10, 10);
    st.FramePadding   = ImVec2(10, 6);
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg]       = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_ChildBg]        = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.15f, 0.15f, 0.18f, 0.98f);
    c[ImGuiCol_Border]         = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBg]        = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_TitleBg]        = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_TitleBgActive]  = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_MenuBarBg]      = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_Header]         = ImVec4(0.30f, 0.45f, 0.60f, 1.00f);
    c[ImGuiCol_HeaderHovered]  = ImVec4(0.35f, 0.52f, 0.68f, 1.00f);
    c[ImGuiCol_HeaderActive]   = ImVec4(0.28f, 0.42f, 0.58f, 1.00f);
    c[ImGuiCol_Button]         = ImVec4(0.30f, 0.42f, 0.56f, 1.00f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(0.36f, 0.50f, 0.65f, 1.00f);
    c[ImGuiCol_ButtonActive]   = ImVec4(0.26f, 0.38f, 0.50f, 1.00f);
    c[ImGuiCol_Text]           = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled]   = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        {
            const ImVec2 sz = ImGui::GetMainViewport()->WorkSize;
            if (g_ui.lastViewportW != 0.0f &&
                (std::abs(sz.x - g_ui.lastViewportW) > 40.0f ||
                 std::abs(sz.y - g_ui.lastViewportH) > 40.0f)) {
                g_ui.dockBuiltEx  = false;
                g_ui.dockBuiltPvb = false;
                g_ui.dockBuiltSim = false;
            }
            g_ui.lastViewportW = sz.x;
            g_ui.lastViewportH = sz.y;
        }

        // Esc: cancel drag, then close popups, then leave the mode. Ignored while typing.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantTextInput) {
            if (g_ui.dragging) {
                EndDrag();
            } else if (g_ui.blankPopupOpen || g_ui.addTilePopupOpen || g_ui.savePopupOpen || g_ui.loadPopupOpen ||
                       g_ui.howToPlayOpen || g_ui.confirmBackOpen || g_pvbUi.firstPlayerDialog) {
                g_ui.blankPopupOpen = g_ui.addTilePopupOpen = g_ui.savePopupOpen = g_ui.loadPopupOpen = false;
                g_ui.howToPlayOpen = g_ui.confirmBackOpen = g_pvbUi.firstPlayerDialog = false;
            } else if (g_mode != AppMode::Title) {
                if (g_mode == AppMode::PlayerVsBot) RequestRematchOrTitle(ConfirmAction::Title);
                else LeaveToTitle();
            }
        }

        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::SetNextWindowViewport(vp->ID);
            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("##DockHost", nullptr, flags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(DockIdFor(g_mode), ImVec2(0, 0), ImGuiDockNodeFlags_None);
            ImGui::End();
        }

        switch (g_mode) {
            case AppMode::Title:          DrawTitleScreen(); break;
            case AppMode::Exploration:    DrawExplorationMode(); DrawAddTilePopup(); break;
            case AppMode::PlayerVsBot:    DrawPvbMode(); break;
            case AppMode::GreedyVsGreedy: DrawSimMode(); break;
        }

        DrawBlankPopup();
        DrawSavePopup();
        DrawLoadPopup();
        DrawHowToPlay();
        DrawConfirmBackPopup();

        // Board hover tooltip, drawn at the top level (outside any window or child) so it
        // renders correctly on the SDL2/SDLRenderer2 backend. See DrawBoardHoverTooltip.
        if (g_mode == AppMode::Exploration) DrawBoardHoverTooltip(g_ex);

        // End-of-frame drag housekeeping: a drag that wasn't consumed by a drop target
        // (released off the board) is cancelled; otherwise draw the ghost on top of everything.
        if (g_ui.dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) EndDrag();
        DrawDragGhost();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 30, 30, 36, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    g_bot.shutdown();
    g_gaddag = nullptr;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}