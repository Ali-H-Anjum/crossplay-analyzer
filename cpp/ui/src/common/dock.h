#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

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

enum class AppMode { Title, Exploration, PlayerVsBot, BotVsBot };
static AppMode g_mode = AppMode::Title;

// Dock IDs must not depend on the current window's ID stack: the dockspace is
// submitted inside "##DockHost" but the builders run outside any window, so
// ImGui::GetID() would hash differently in the two places.
static ImGuiID DockIdFor(AppMode m) {
    switch (m) {
        case AppMode::Exploration:    return ImHashStr("Dock_Exploration");
        case AppMode::PlayerVsBot:    return ImHashStr("Dock_PvG");
        case AppMode::BotVsBot: return ImHashStr("Dock_GvG");
        default:                      return ImHashStr("Dock_Title");
    }
}

static const char* kTopEx  = "Vision -- Exploration##top";
static const char* kTopPvb = "Vision -- PvG##top";
static const char* kTopSim = "Vision -- GvG##top";

// Fixed dock layout: top strip across the width, legend on the left (fixed px),
// cream body in the middle (flex), right panel on the right (fixed px).
static void BuildDock(ImGuiID dockId, const char* topName,
                      const char* leftName, const char* creamName, const char* rightName,
                      float leftW, float rightW) {
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_None);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);

    const float totalW = ImGui::GetMainViewport()->WorkSize.x;

    ImGuiID body;
    ImGuiID topStrip = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Up, 0.07f, nullptr, &body);

    ImGuiID rest;
    ImGuiID left = ImGui::DockBuilderSplitNode(body, ImGuiDir_Left, leftW / totalW, nullptr, &rest);

    ImGuiID rightCol;
    ImGuiID middle = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Left, 1.0f - rightW / totalW, nullptr, &rightCol);

    // Pin the horizontal nodes to exact pixel widths.
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(left)) {
        n->SizeRef = ImVec2(leftW, 0.0f);
        n->WantLockSizeOnce = true;
    }
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(rightCol)) {
        n->SizeRef = ImVec2(rightW, 0.0f);
        n->WantLockSizeOnce = true;
    }

    ImGui::DockBuilderDockWindow(topName, topStrip);
    ImGui::DockBuilderDockWindow(leftName, left);
    ImGui::DockBuilderDockWindow(creamName, middle);
    ImGui::DockBuilderDockWindow(rightName, rightCol);

    FinishAndLock(dockId);
}

