#pragma once
#include "state/state.h"
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

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
    DrawSwatch(COL_HEAT_0, "0 -- dead square");
    DrawSwatch(COL_HEAT_LOW, "1-5 -- very tight");
    DrawSwatch(COL_HEAT_MID, "6-15 -- moderate");
    DrawSwatch(COL_HEAT_HI, "16-25 -- wide open");
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
static bool ParseEntryLabel(const std::string& label, char* word, size_t wordSz, int& row, int& col,
                            char& dir, int& pts) {
    word[0] = '\0';
    row = col = pts = 0;
    dir = '?';
    if (label == "PASS") {
        std::snprintf(word, wordSz, "PASS");
        return false;
    }
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
        std::snprintf(header, sizeof header, "%-3s %-4s %+4d  %-15.15s  %-9s", "#", "Who", 0,
                      "Word", "Dir & Pos");
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
                    std::snprintf(label, sizeof label, "%-3d %-4s   --   %-15.15s  %s",
                                  e.turnNumber, who, word, "-");
                } else {
                    char posBuf[16];
                    std::snprintf(posBuf, sizeof posBuf, "%c (%2d,%2d)", dir, row, col);
                    std::snprintf(label, sizeof label, "%-3d %-4s %+4d  %-15.15s  %-9s",
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
