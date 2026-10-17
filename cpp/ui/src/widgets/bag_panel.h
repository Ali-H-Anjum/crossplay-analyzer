#pragma once
// Tile bag / unseen-tiles panel rendering.
// Dependencies (included by main.cpp before this header): common/common.h (ImGui,
// SDL, engine headers, g_font*), common/theme.h (AddBoldText, DrawTileBody,
// DrawTileGlyph, kCell, kGap, PANEL_*), state/state.h (BagCounts, ModeState,
// Snapshot, NUM_LETTER_SLOTS via letter.h).
#include "common/theme.h"
#include "state/state.h"

#include <cstdint>
#include <cstdio>

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
    ImFont* lf = g_fontFranklin ? g_fontFranklin : font;
    const float baseSize  = ImGui::GetFontSize();
    const float avail     = ImGui::GetContentRegionAvail().x;
    const float cell      = std::clamp((avail - kGapX * (kCols - 1)) / kCols, 24.0f, 46.0f);
    const float countSize = baseSize * 1.15f;
    const float letterSize = cell * 0.62f;
    const float rowH      = cell + countSize + 4.0f + kGapY;
    const float gridW     = kCols * cell + (kCols - 1) * kGapX;
    const ImVec2 origin   = ImGui::GetCursorScreenPos();
    const float left      = origin.x + std::max(0.0f, (avail - gridW) * 0.5f);
    const float rounding  = cell * 0.28f;
    // One "leaf" corner pair is rounded, the other pair stays nearly square.
    const ImDrawFlags corners = ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomRight;

    // Text metrics cached across frames; reset when the fonts or sizes change.
    struct Metrics {
        ImFont* font = nullptr; ImFont* lf = nullptr;
        float countSize = -1.0f, letterSize = -1.0f;
        ImVec2 letter[NUM_LETTER_SLOTS];
        ImVec2 count[32];
    };
    static Metrics m;
    if (m.font != font || m.lf != lf || m.countSize != countSize || m.letterSize != letterSize) {
        m.font = font; m.lf = lf; m.countSize = countSize; m.letterSize = letterSize;
        for (auto& v : m.letter) v = ImVec2(-1.0f, -1.0f);
        for (auto& v : m.count)  v = ImVec2(-1.0f, -1.0f);
    }

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
            ImVec2& ls = m.letter[i];
            if (ls.x < 0.0f) ls = lf->CalcTextSizeA(letterSize, 10000.0f, 0.0f, buf);
            dl->AddText(lf, letterSize,
                        ImVec2(x + (cell - ls.x) * 0.5f, y + (cell - ls.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), buf);
        }

        char cb[8]; std::snprintf(cb, sizeof cb, "%u", n);
        ImVec2 cs;
        if (n < 32) {
            ImVec2& v = m.count[n];
            if (v.x < 0.0f) v = font->CalcTextSizeA(countSize, 10000.0f, 0.0f, cb);
            cs = v;
        } else {
            cs = font->CalcTextSizeA(countSize, 10000.0f, 0.0f, cb);
        }
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