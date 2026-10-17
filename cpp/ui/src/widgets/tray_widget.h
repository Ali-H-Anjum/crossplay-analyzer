#pragma once
// Tray strip rendering and interaction.
// Dependencies (included by main.cpp before this header): common/common.h (ImGui,
// engine headers, LetterPoints, toChar, fromIndex, Letter), common/theme.h
// (DrawTileBody, DrawTileGlyph, COL_RACK_SHELF, COL_TILE, kRackSize),
// state/state.h (ModeState, Tray, g_ui, DragState, DragSource),
// state/board_rules.h (TrayOrder, TrayLetters, BagReturn).
#include "common/theme.h"
#include "state/state.h"
#include "state/board_rules.h"

#include <algorithm>

// Draws a static rack strip (no interaction). Returns nothing; hoverIdxOut is set
// when the mouse is over a tile slot (interactive mode only).
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