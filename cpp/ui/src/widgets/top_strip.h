#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

// ================================================================ top-strip
// helpers
static void PushTopAccent() {
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 90, 120, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 115, 150, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(55, 75, 105, 255));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
}

// Button colours for cream-background panels (Explore / PvB / Sim body).
static void PushPanelButton() {
  ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 100, 160, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 125, 190, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(45, 80, 135, 255));
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(160, 165, 175, 255));
}
static void PopPanelButton() {
  ImGui::PopStyleColor(5);
}

static void PopTopAccent() {
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
}
static void TopGroupSep() {
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(12, 0));
  ImGui::SameLine();
}

static void OpenSavePopup() {
  g_ui.saveMessage.clear();
  g_ui.savePopupOpen = true;
}
static void OpenLoadPopup() {
  g_ui.loadMessage.clear();
  g_ui.loadListDirty = true;
  g_ui.loadPopupOpen = true;
}

static void DrawSaveLoadButtons() {
  const float bw = 100.0f;
  ImGui::SameLine();
  const float avail = ImGui::GetContentRegionAvail().x;
  const float need = bw * 2 + ImGui::GetStyle().ItemSpacing.x;
  if (avail > need)
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - need);
  if (ImGui::Button("Load State", ImVec2(bw, 0)))
    OpenLoadPopup();
  ImGui::SameLine();
  if (ImGui::Button("Save State", ImVec2(bw, 0)))
    OpenSavePopup();
}

// Compact history nav: four buttons in a single row, sized to fit a narrow
// panel. Returns the cursor the user asked to jump to, or -1.
static int DrawHistoryNavBar(const ModeState& ms) {
  int target = -1;
  const bool canPrev = ms.cursor > 0;
  const bool canNext = ms.cursor < (int)ms.history.size();

  const float bw = 40.0f;
  ImGui::BeginDisabled(!canPrev);
  if (ImGui::Button("|<", ImVec2(bw, 0)))
    target = 0;
  ImGui::SameLine();
  if (ImGui::Button("<", ImVec2(bw, 0)))
    target = ms.cursor - 1;
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!canNext);
  if (ImGui::Button(">", ImVec2(bw, 0)))
    target = ms.cursor + 1;
  ImGui::SameLine();
  if (ImGui::Button(">|", ImVec2(bw, 0)))
    target = (int)ms.history.size();
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::TextDisabled("%d / %d", ms.cursor, (int)ms.history.size());
  return target;
}

// Common top strip: Back to Title + Rematch + optional extra buttons + centered
// mode name
// + right-aligned save/load. `onBack`/`onRematch` are called when those buttons
// are pressed. `extra` (may be null) is drawn between Rematch and the mode
// name.
struct TopStripCallbacks {
  void (*onBack)() = nullptr;
  void (*onRematch)() = nullptr;
  void (*extra)() = nullptr;
  const char* modeName = nullptr;  // centered in the strip; may be null
};

static void DrawModeTopStrip(const ModeState& ms, const TopStripCallbacks& cb) {
  (void)ms;  // nav moved to the Move History panel; kept for signature
             // compatibility
  PushTopAccent();
  if (ImGui::Button("Back to Title")) {
    cb.onBack();
    PopTopAccent();
    return;
  }
  TopGroupSep();
  if (ImGui::Button("Rematch"))
    cb.onRematch();
  TopGroupSep();
  if (cb.extra) {
    cb.extra();
    TopGroupSep();
  }

  // Centered mode name. Compute where the Save/Load block will end so we can
  // center the name in the space between the cursor's current X and that
  // block's start.
  if (cb.modeName) {
    const float bw = 100.0f;
    const float saveLoadW = bw * 2 + ImGui::GetStyle().ItemSpacing.x;
    const float stripW = ImGui::GetWindowContentRegionMax().x;
    const float saveLoadX = stripW - saveLoadW;

    const float nameW = ImGui::CalcTextSize(cb.modeName).x;
    const float nameX = ImGui::GetCursorPosX() +
                        (saveLoadX - ImGui::GetCursorPosX() - nameW) * 0.5f;
    if (nameX > ImGui::GetCursorPosX())
      ImGui::SetCursorPosX(nameX);
    ImGui::TextDisabled("%s", cb.modeName);
    ImGui::SameLine();
  }

  DrawSaveLoadButtons();
  PopTopAccent();
}

static void LeaveToTitle() {
  EndDrag();
  g_ui.blankPopupOpen = false;
  g_ui.addTilePopupOpen = false;
  g_ui.savePopupOpen = false;  // Save/Load are mode-specific; leaving them open
                               // would act on the wrong state
  g_ui.loadPopupOpen = false;
  g_ui.confirmBackOpen = false;
  g_mode = AppMode::Title;
}

// Opens the mode's dock (once) and top strip, and returns false if the user
// left the mode via the top strip during this call. Callers must return
// immediately if it returns false.
static bool BeginModeScreen(AppMode mode, const char* topName,
                            bool& dockBuiltFlag, const char* leftName,
                            const char* creamName, const char* rightName,
                            float leftW, float rightW, void (*drawTop)()) {
  if (!dockBuiltFlag) {
    BuildDock(DockIdFor(mode), topName, leftName, creamName, rightName, leftW,
              rightW);
    dockBuiltFlag = true;
  }
  ImGui::Begin(topName, nullptr, PANEL_FLAGS);
  drawTop();
  ImGui::End();
  return g_mode == mode;
}
