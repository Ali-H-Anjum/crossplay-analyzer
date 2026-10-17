#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

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
            AddHeavyText(dl, lf, letterSize,
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

static void DrawTrayStrip(const TrayOrder& order, bool interactive, int* hoverIdxOut) {
    const float cell = 46.0f, gap = 8.0f;
    const int slots = std::max(kRackSize, order.size());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool winHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const float rackW = cell * slots + gap * (slots - 1);

    // Shelf the tiles sit on: starts below the tiles' midline so it shows underneath them.
    dl->AddRectFilled(ImVec2(origin.x - 10.0f, origin.y + cell * 0.72f),
                      ImVec2(origin.x + rackW + 10.0f, origin.y + cell + 10.0f),
                      COL_RACK_SHELF, 6.0f);

    for (int i = 0; i < order.size(); ++i) {
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
                if (i >= 0 && i < order.size()) *hoverIdxOut = i;
            }
        }
    }
}

// Draws player A's rack and handles starting a drag (and, optionally, returning a
// tile to the bag with right-click).
static void DrawTrayInteractive(ModeState& ms, bool interactive, bool rightClickToBag) {
    const TrayOrder order = TrayLetters(ms.trayA);
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

    // Premium label metrics are constant for the frame: measure the four labels once.
    struct PremLab { const char* txt; float size; ImVec2 ts; };
    PremLab premLab[4];
    ImFont* premFont = nullptr;
    {
        static const Mult kPrem[4] = { MULT_DL, MULT_TL, MULT_DW, MULT_TW };
        ImGui::PushFont(g_fontFranklin);
        premFont = ImGui::GetFont();
        const float lblSize = ImGui::GetFontSize() * 1.05f;
        for (int i = 0; i < 4; ++i) {
            premLab[i].txt  = PremiumLabelNyt(kPrem[i]);
            premLab[i].size = lblSize;
            premLab[i].ts   = premFont->CalcTextSizeA(lblSize, 10000.0f, 0.0f, premLab[i].txt);
        }
        ImGui::PopFont();
    }
    auto premIdx = [](Mult m) {
        switch (m) {
            case MULT_DL: return 0;
            case MULT_TL: return 1;
            case MULT_DW: return 2;
            default:      return 3;   // MULT_TW
        }
    };

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
                    const PremLab& pl = premLab[premIdx(prem)];
                    AddHeavyText(dl, premFont, pl.size,
                                 ImVec2(p0.x + (kCell - pl.ts.x) * 0.5f, p0.y + (kCell - pl.ts.y) * 0.5f),
                                 PremiumTextNyt(prem), pl.txt);
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
    if (interactive && ms.boardValid && HasUncommitted(ms)) {
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

