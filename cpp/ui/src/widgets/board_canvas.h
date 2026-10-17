#pragma once
// Main board rendering loop and the exploration hover tooltip.
// Dependencies (included by main.cpp before this header): common/common.h
// (ImGui, engine headers, premiumAt, sq, fromBoardCode, toChar, fromIndex,
// EMPTY_SQUARE, Board, Letter, MULT_*), common/theme.h (DrawTileBody,
// DrawTileGlyph, AddHeavyText, AddBoldText, DrawStar, PremiumFillNyt,
// PremiumTextNyt, PremiumLabelNyt, HeatFill, COL_BOARD_EMPTY, COL_CENTER,
// COL_TILE, kCell, kGap, kCellRound, g_fontFranklin), state/state.h (ModeState,
// g_boardHover*, g_ui), state/board_rules.h (IsAnchorAgainst,
// EvaluatePlacement, Placement, HasUncommitted), widgets/drag_drop.h
// (BoardKind, IsTouchable, DropOnBoard).
#include <cmath>
#include <cstdio>

#include "common/theme.h"
#include "state/board_rules.h"
#include "state/state.h"
#include "widgets/drag_drop.h"

// Renders the 15x15 board inside the current ImGui window/child.
static void DrawBoardCanvas(BoardKind kind, ModeState& ms, bool interactive) {
  const bool explore = (kind == BoardKind::Explore);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 mouse = ImGui::GetMousePos();
  // Hovering only counts when this window is the one under the mouse, so popups
  // that sit over the board don't leak clicks through.
  const bool winHovered =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  int hoverR = -1, hoverC = -1;

  const float grid = BOARD_LENGTH * (kCell + kGap) - kGap;

  // Premium label metrics are constant for the frame: measure the four labels
  // once.
  struct PremLab {
    const char* txt;
    float size;
    ImVec2 ts;
  };
  PremLab premLab[4];
  ImFont* premFont = nullptr;
  {
    static const Mult kPrem[4] = {MULT_DL, MULT_TL, MULT_DW, MULT_TW};
    ImGui::PushFont(g_fontFranklin);
    premFont = ImGui::GetFont();
    const float lblSize = ImGui::GetFontSize() * 1.05f;
    for (int i = 0; i < 4; ++i) {
      premLab[i].txt = PremiumLabelNyt(kPrem[i]);
      premLab[i].size = lblSize;
      premLab[i].ts =
          premFont->CalcTextSizeA(lblSize, 10000.0f, 0.0f, premLab[i].txt);
    }
    ImGui::PopFont();
  }
  auto premIdx = [](Mult m) {
    switch (m) {
      case MULT_DL:
        return 0;
      case MULT_TL:
        return 1;
      case MULT_DW:
        return 2;
      default:
        return 3;  // MULT_TW
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
      if (hovered) {
        hoverR = r;
        hoverC = c;
      }

      const bool isAnchor = (ms.anchors.by_row[r] & (1u << c)) != 0;
      const Mult prem = premiumAt(r, c);
      const bool isCenter = (r == BOARD_LENGTH / 2 && c == BOARD_LENGTH / 2);
      const bool centerMark = isCenter && !explore;

      const bool hot = hovered && interactive;

      if (code != EMPTY_SQUARE) {
        // Placed tile: same silhouette as the rack.
        DrawTileBody(dl, p0, kCell,
                     hot ? IM_COL32(86, 118, 188, 255) : COL_TILE);
        const Letter L = fromBoardCode(code);
        const bool blank = ms.board.is_blank.test(idx);
        char ch = toChar(L);
        if (blank)
          ch = (char)std::tolower((unsigned char)ch);
        DrawTileGlyph(dl, p0, kCell, ch, blank ? 0 : LetterPoints(L), true);
      } else {
        ImU32 bg;
        if (centerMark)
          bg = COL_CENTER;
        else if (explore) {
          // Match the tooltip: cc[0] at actual coords, cc[1] at transposed
          // coords.
          const uint32_t mH = (uint32_t)ms.board.cross_check[0][idx];
          const uint32_t mV = (uint32_t)ms.board.cross_check[1][sq(c, r)];
          const uint32_t mi = mH & mV;
          bg = HeatFill(std::popcount(mi));
        } else if (prem != MULT_NONE)
          bg = PremiumFillNyt(prem);
        else
          bg = COL_BOARD_EMPTY;
        if (hot)
          bg = IM_COL32(176, 190, 226, 255);
        dl->AddRectFilled(p0, p1, bg, kCellRound);

        if (centerMark) {
          DrawStar(dl, ImVec2(p0.x + kCell * 0.5f, p0.y + kCell * 0.5f),
                   kCell * 0.32f, COL_TILE_TEXT);
        } else if (prem != MULT_NONE) {
          const PremLab& pl = premLab[premIdx(prem)];
          AddHeavyText(dl, premFont, pl.size,
                       ImVec2(p0.x + (kCell - pl.ts.x) * 0.5f,
                              p0.y + (kCell - pl.ts.y) * 0.5f),
                       PremiumTextNyt(prem), pl.txt);
        }
        // Cyan anchor outline, only when adjacent to a committed tile.
        if (isAnchor) {
          if (kind == BoardKind::Sim ||
              IsAnchorAgainst(ms.turnStart().board, r, c))
            dl->AddRect(p0, p1, IM_COL32(0, 200, 230, 255), kCellRound, 0,
                        2.5f);
        }
      }
    }
  }
  ImGui::Dummy(ImVec2(grid, grid));

  // Highlight of the move selected in the Move Generation tab.
  if (explore) {
    for (const auto& h : g_mg.hlCells) {
      if (ms.board.letters_row[sq(h.r, h.c)] != EMPTY_SQUARE)
        continue;
      ImVec2 p0(origin.x + h.c * (kCell + kGap),
                origin.y + h.r * (kCell + kGap));
      ImVec2 p1(p0.x + kCell, p0.y + kCell);
      dl->AddRectFilled(p0, p1, IM_COL32(107, 165, 219, 190), kCellRound);
      dl->AddRect(p0, p1, IM_COL32(40, 90, 160, 255), kCellRound, 0, 2.0f);
      const char buf[2] = {h.ch, 0};
      const float sz = kCell * 0.55f;
      const ImVec2 ts =
          ImGui::GetFont()->CalcTextSizeA(sz, 10000.0f, 0.0f, buf);
      AddBoldText(
          dl, ImGui::GetFont(), sz,
          ImVec2(p0.x + (kCell - ts.x) * 0.5f, p0.y + (kCell - ts.y) * 0.5f),
          IM_COL32(255, 255, 255, 255), buf);
    }
  }

  // Pending-move green outline: only when the board is valid AND the staged
  // tiles form a geometrically valid placement. The outline covers the entire
  // formed word (committed tiles included), not just the new tiles.
  if (interactive && ms.boardValid && HasUncommitted(ms)) {
    const Placement pend = EvaluatePlacement(ms.board, ms.turnStart().board);
    if (pend.ok) {
      // Walk from pend.(row,col) in the move direction to find the full run
      // end.
      const int dr = pend.desc ? 1 : 0;
      const int dc = pend.desc ? 0 : 1;
      int len = 0;
      while (
          pend.row + len * dr < BOARD_LENGTH &&
          pend.col + len * dc < BOARD_LENGTH &&
          ms.board.letters_row[sq(pend.row + len * dr, pend.col + len * dc)] !=
              EMPTY_SQUARE) {
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

      char pb[16];
      std::snprintf(pb, sizeof pb, "%d", pend.points);
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

  // Record hover for the caller's tooltip. Tooltips drawn inside a
  // BeginChild/EndChild pair can render blank on the SDL2/SDLRenderer2 backend,
  // so the tooltip is drawn by DrawBoardHoverTooltip after the board child
  // window has closed.
  g_boardHoverR = hoverR;
  g_boardHoverC = hoverC;
  g_boardHoverOk = explore && (hoverR >= 0) && !g_ui.dragging;

  if (!interactive)
    return;
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
        g_ui.drag.fromR = hoverR;
        g_ui.drag.fromC = hoverC;
        g_ui.drag.letter = fromBoardCode(ms.board.letters_row[idx]);
        g_ui.drag.isBlank = ms.board.is_blank.test(idx);
      }
    }
  }
  // Releasing over the board drops; releasing anywhere else cancels (see
  // end-of-frame cleanup).
  if (g_ui.dragging && overBoard &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    DropOnBoard(ms, hoverR, hoverC);
}

// Draws the cross-check tooltip for the hovered Exploration square. Called at
// the top level (outside any window/child) so it renders correctly on
// SDL2/SDLRenderer2.
static void DrawBoardHoverTooltip(const ModeState& ms) {
  if (!g_boardHoverOk)
    return;
  const int r = g_boardHoverR, c = g_boardHoverC;
  const int idx = sq(r, c);
  const Mult prem = premiumAt(r, c);

  ImGui::BeginTooltip();
  ImGui::Text("(%d,%d)", r, c);
  if (prem != MULT_NONE)
    ImGui::Text("Premium: %s", PremiumLabelNyt(prem));
  ImGui::Separator();
  const uint8_t code = ms.board.letters_row[idx];
  if (code != EMPTY_SQUARE) {
    char ch = toChar(fromBoardCode(code));
    if (ms.board.is_blank.test(idx))
      ch = (char)std::tolower((unsigned char)ch);
    ImGui::Text("Tile: %c", ch);
  } else {
    auto maskStr = [](uint32_t m, char* out) {
      size_t k = 0;
      for (int i = 0; i < NUM_LETTERS; ++i)
        if (m & (1u << i))
          out[k++] = (char)('A' + i);
      out[k] = 0;
    };
    // cc[0] uses actual coords; cc[1] uses transposed coords (matching
    // MoveGen's reads).
    const uint32_t mH = (uint32_t)ms.board.cross_check[0][idx];
    const uint32_t mV = (uint32_t)ms.board.cross_check[1][sq(c, r)];
    char sH[40], sV[40], sA[40];
    maskStr(mH, sH);
    maskStr(mV, sV);
    maskStr(mH & mV, sA);
    ImGui::Text("H: %s", sH);
    ImGui::Text("V: %s", sV);
    ImGui::Text("&: %s", sA);
  }
  ImGui::EndTooltip();
}