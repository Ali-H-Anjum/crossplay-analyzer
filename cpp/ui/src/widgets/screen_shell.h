#pragma once
// Cream panel style, scores line, and the shared board+bag screen shell.
// Dependencies (included by main.cpp before this header): common/common.h (ImGui),
// common/theme.h (PushCreamStyle/PopCreamStyle use PANEL_* colors; PANEL_FLAGS),
// widgets/bag_panel.h (DrawTileBagPanelCream, BagView, LiveBagView, SnapshotBagView),
// widgets/board_canvas.h (DrawBoardCanvas).
#include "common/theme.h"
#include "widgets/bag_panel.h"
#include "widgets/board_canvas.h"

// ================================================================ cream style
static void PushCreamStyle() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, PANEL_BG);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, PANEL_BG);
    ImGui::PushStyleColor(ImGuiCol_Text, PANEL_TEXT);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, PANEL_TEXT_DIM);
    ImGui::PushStyleColor(ImGuiCol_Separator, PANEL_SEP);
}
static void PopCreamStyle() { ImGui::PopStyleColor(5); }

// ================================================================ shared screen pieces
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