#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

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

    if (ImGui::Button("Bot vs Bot", ImVec2(bw, 48))) {
        g_simUi.started = false;
        g_ui.dockBuiltSim = false;
        g_mode = AppMode::BotVsBot;
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

