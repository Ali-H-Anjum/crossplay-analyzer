#pragma once
// Drag-and-drop onto the board, plus the drag ghost.
// Dependencies (included by main.cpp before this header): common/common.h (ImGui,
// engine headers, toChar, fromBoardCode, LetterPoints, toIndex, fromIndex, Letter,
// EMPTY_SQUARE, kRackSize), common/theme.h (DrawTileBody, DrawTileGlyph, kCell,
// COL_TILE), state/state.h (ModeState, DragState, DragSource, g_ui, EndDrag),
// state/board_rules.h (placeTile, removeTile, CanDropTile, RefreshDerived).
#include "common/theme.h"
#include "state/state.h"
#include "state/board_rules.h"

#include <string>

// How the current board responds to direct tile manipulation.
enum class BoardKind { Pvb, Sim, Explore };

// True when the tile at flat index `idx` can be lifted or dragged.
static bool IsTouchable(BoardKind kind, const ModeState& ms, int idx) {
    if (ms.board.letters_row[idx] == EMPTY_SQUARE) return false;
    if (kind == BoardKind::Explore) return true;
    // In PvB only tiles placed during the current turn may be moved or lifted.
    return ms.turnStart().board.letters_row[idx] == EMPTY_SQUARE;
}

// Commit a tray-drop or board-drop onto square (r, c). `assigned` is the letter
// the tile stands for (a blank may stand for any letter).
static void CompleteTrayDrop(ModeState& ms, int r, int c, Letter assigned, bool blank) {
    ms.trayA.removeTile(blank ? Letter::Blank : assigned);
    placeTile(ms.board, r, c, assigned, blank);
    RefreshDerived(ms);
    g_ui.lastAction = "Played tile.";
}

// Handle releasing a drag over the board: drop on an empty square, or cancel.
static void DropOnBoard(ModeState& ms, int r, int c) {
    std::string reason;
    if (g_ui.drag.src == DragSource::Tray) {
        if (!CanDropTile(ms.board, r, c, reason)) {
            g_ui.lastAction = "Rejected: " + reason;
        } else {
            const bool isBlank = g_ui.drag.isBlank;
            const Letter L = isBlank ? Letter::Blank : g_ui.drag.letter;
            if (isBlank) {
                // Defer blank assignment to the popup; remember where the drop landed.
                g_ui.blankMs = &ms; g_ui.blankDropR = r; g_ui.blankDropC = c;
                g_ui.blankPopupOpen = true;
            } else {
                if (ms.trayA.count(L) > 0) {
                    CompleteTrayDrop(ms, r, c, L, false);
                } else {
                    g_ui.lastAction = "Rejected: no tile in rack.";
                }
            }
        }
    } else if (g_ui.drag.src == DragSource::Board) {
        // Lift-and-drop: move a tile already on the board.
        const int sr = g_ui.drag.fromR, sc = g_ui.drag.fromC;
        const int sidx = sq(sr, sc);
        if (ms.board.letters_row[sidx] == EMPTY_SQUARE) {
            g_ui.lastAction = "Rejected: source square empty.";
        } else if (!CanDropTile(ms.board, r, c, reason)) {
            g_ui.lastAction = "Rejected: " + reason;
        } else {
            const Letter L = g_ui.drag.letter;
            const bool isBlank = g_ui.drag.isBlank;
            removeTile(ms.board, sr, sc);
            ms.trayA.addTile(isBlank ? Letter::Blank : L);
            placeTile(ms.board, r, c, L, isBlank);
            RefreshDerived(ms);
            g_ui.lastAction = "Moved tile.";
        }
    }
    EndDrag();
}

// Draw the dragged tile under the cursor (ghost).
static void DrawDragGhost() {
    if (!g_ui.dragging) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 mp = ImGui::GetMousePos();
    const float cell = kCell;
    const ImVec2 p0(mp.x - cell * 0.5f, mp.y - cell * 0.5f);
    DrawTileBody(dl, p0, cell, COL_TILE);
    const char buf[2] = { toChar(g_ui.drag.letter), 0 };
    DrawTileGlyph(dl, p0, cell, buf[0],
                  g_ui.drag.isBlank ? 0 : LetterPoints(g_ui.drag.letter), true);
}