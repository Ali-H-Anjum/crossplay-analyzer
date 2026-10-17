#pragma once
// Drag-and-drop onto the board: CompleteTrayDrop.
#include "widgets/drag_drop.h"

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

