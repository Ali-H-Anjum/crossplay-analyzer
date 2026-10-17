#pragma once
// Board + bag screen shell, tray interaction, and bag views.
#include "widgets/bag_panel.h"
#include "widgets/screen_shell.h"
#include "widgets/tray_widget.h"

// ================================================================ Exploration
static void ExplorationReset() {
  g_ex.bag = std::make_unique<TileBag>(kExplorationSeed);
  g_ex.board = Board{};
  g_ex.trayA = Tray{};
  g_ex.trayB = Tray{};
  g_ex.history.clear();
  g_ex.cursor = 0;
  g_ex.score[0] = g_ex.score[1] = 0;
  RefreshDerived(g_ex);
  g_ex.initial = SnapshotOf(g_ex);
  g_ui.lastAction = "(fresh session)";
}

static void LoadTestingCaseIntoExploration() {
  ExplorationReset();  // initial snapshot = empty board, full bag

  struct MoveSpec {
    const char* word;
    int row, col;
    bool desc;
    int player;
  };
  const MoveSpec specs[] = {
      {"LANDAU", 2, 7, true, 0},
      {"NUDELY", 7, 6, false, 1},
      {"BESES", 8, 5, false, 0},
  };
  for (const auto& s : specs) {
    const Board before = g_ex.board;
    Move mv{s.word, s.row, s.col, s.desc};
    for (const auto& pl : mv.letterPositions()) {
      if (g_ex.board.letters_row[sq(pl.row, pl.col)] != EMPTY_SQUARE)
        continue;
      placeTile(g_ex.board, pl.row, pl.col, pl.letter, pl.is_blank);
      const Letter tl = pl.is_blank ? Letter::Blank : pl.letter;
      if (g_ex.bag->count(tl) > 0)
        g_ex.bag->removeLetter(tl);  // keep the bag consistent
    }
    RefreshDerived(g_ex);
    const Placement ps = EvaluatePlacement(g_ex.board, before);
    if (ps.ok)
      g_ex.score[s.player] += ps.points;
    PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string(s.word), s.player);
  }

  static const char kRack[] = "ERSBEES";
  g_ex.trayA = Tray{};
  g_ex.trayA.addTiles(kRack);
  for (const char* p = kRack; *p; ++p) {
    const Letter L = fromIndex((uint8_t)(*p - 'A'));
    if (g_ex.bag->count(L) > 0)
      g_ex.bag->removeLetter(L);
  }
  // Make the last committed entry include the rack so the position isn't
  // "uncommitted".
  static_cast<Snapshot&>(g_ex.history.back()) = SnapshotOf(g_ex);
  g_ex.cursor = (int)g_ex.history.size();
  g_ui.lastAction = "(testing case loaded)";
}

static void ExUndoTurn() {
  // Revert only the board; the tray may have been modified outside the turn
  // (added tiles from the bag, random tile) and shouldn't be rolled back.
  if (g_ex.cursor == 0)
    g_ex.board = g_ex.initial.board;
  else
    g_ex.board = g_ex.history[g_ex.cursor - 1].board;
  RefreshDerived(g_ex);
  EndDrag();
  g_ui.blankPopupOpen = false;
  g_ui.lastAction = "Reverted to last committed.";
}

static void ExTopOnBack() {
  LeaveToTitle();
}
static void ExTopOnRematch() {
  ExplorationReset();
}
static void ExTopExtra() {
  if (ImGui::Button("Load Testing Case"))
    LoadTestingCaseIntoExploration();
}

static void DrawExplorationTop() {
  TopStripCallbacks cb;
  cb.onBack = ExTopOnBack;
  cb.onRematch = ExTopOnRematch;
  cb.extra = ExTopExtra;
  cb.modeName = "Exploration";
  DrawModeTopStrip(g_ex, cb);
}

static const RawMove* BotPlayable();

static void DrawExplorationCream() {
  PushCreamStyle();
  ImGui::Begin("Cream##ex", nullptr, PANEL_FLAGS);

  const bool atEnd = g_ex.atEnd();

  DrawBoardAndBag(BoardKind::Explore, g_ex, atEnd, LiveBagView(g_ex));

  ImGui::Separator();

  const bool hasUncommitted = HasUncommitted(g_ex);
  const Placement exPS =
      (atEnd && hasUncommitted)
          ? EvaluatePlacement(g_ex.board, g_ex.turnStart().board)
          : Placement{};
  const bool hasHighlight = (g_mg.hlMoveIdx >= 0);
  const RawMove* botMove = BotPlayable();

  // Play Move: staged tiles form a valid placement, or a Move Generation row is
  // highlighted, or a Bot Search row is selected. The last two need nothing
  // staged.
  const bool canPlayStaged =
      atEnd && hasUncommitted && g_ex.boardValid && exPS.ok;
  const bool canPlayHighlight =
      atEnd && hasHighlight && !hasUncommitted && g_ex.boardValid;
  const bool canPlayBot = atEnd && !hasHighlight && botMove && g_ex.boardValid;
  const bool canPlay = canPlayStaged || canPlayHighlight || canPlayBot;
  const bool canUndo = atEnd && hasUncommitted;

  PushPanelButton();
  if (!canPlay)
    ImGui::BeginDisabled();
  if (ImGui::Button("Play Move", ImVec2(120, 32))) {
    if (canPlayStaged) {
      PushEntry(g_ex, FormatMove(exPS), -1);
      g_ui.lastAction = "Move committed.";
    } else if (canPlayHighlight) {
      const std::vector<ScoredMove>* listPtr = &g_mg.allMoves;
      if (g_mg.view == MoveView::Unique)
        listPtr = &g_mg.uniqueMoves;
      else if (g_mg.view == MoveView::TopN)
        listPtr = &g_mg.topNMoves;
      const auto& list = *listPtr;
      if (g_mg.hlMoveIdx < (int)list.size()) {
        const RawMove rm = list[g_mg.hlMoveIdx].rm;
        ApplyHighlightedMove(g_ex, rm);
        // Evaluate AFTER applying: before, the staged diff is empty and the
        // label was always "(play)".
        const Placement ps =
            EvaluatePlacement(g_ex.board, g_ex.turnStart().board);
        PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string("(play)"), -1);
        g_mg.hlMoveIdx = -1;
        g_mg.hlCells.clear();
        g_mg.movesStale = true;
      }
    } else if (canPlayBot) {
      const RawMove rm = *botMove;  // copy: selRaw is cleared below
      ApplyHighlightedMove(g_ex, rm);
      const Placement ps =
          EvaluatePlacement(g_ex.board, g_ex.turnStart().board);
      PushEntry(g_ex, ps.ok ? FormatMove(ps) : std::string("(play)"), -1);
      g_bot.clearSel();
      g_mg.hlCells.clear();
      g_mg.movesStale = true;
    }
  }
  if (!canPlay)
    ImGui::EndDisabled();

  ImGui::SameLine();
  if (!canUndo)
    ImGui::BeginDisabled();
  if (ImGui::Button("Undo Turn", ImVec2(120, 32)))
    ExUndoTurn();
  if (!canUndo)
    ImGui::EndDisabled();

  ImGui::SameLine();
  const bool boardEdited = hasUncommitted;
  if (!atEnd || boardEdited)
    ImGui::BeginDisabled();
  if (ImGui::Button("Pass", ImVec2(80, 32))) {
    PushEntry(g_ex, "PASS", -1);
    g_ui.lastAction = "Passed.";
  }
  if (!atEnd || boardEdited)
    ImGui::EndDisabled();
  PopPanelButton();

  ImGui::Separator();
  DrawTrayInteractive(g_ex, atEnd, true);

  const bool rackFull = (int)g_ex.trayA.size() >= kRackSize;
  const bool bagEmpty = (!g_ex.bag || g_ex.bag->size() == 0);
  PushPanelButton();
  if (!atEnd || rackFull || bagEmpty)
    ImGui::BeginDisabled();
  if (ImGui::Button("Add Tile from Bag"))
    g_ui.addTilePopupOpen = true;
  ImGui::SameLine();
  if (ImGui::Button("Add Random Tile")) {
    g_ex.bag->drawInto(g_ex.trayA, 1);
    g_ui.lastAction = "Added random tile.";
  }
  if (!atEnd || rackFull || bagEmpty)
    ImGui::EndDisabled();
  PopPanelButton();

  ImGui::Separator();
  ImGui::SetWindowFontScale(1.3f);
  ImGui::TextUnformatted(g_ex.boardValid ? "Board valid" : "Board INVALID");
  ImGui::SetWindowFontScale(1.0f);
  if (canPlayHighlight || canPlayBot) {
    ImGui::TextDisabled("Highlighted move ready.");
  } else if (atEnd && hasUncommitted) {
    if (exPS.ok)
      ImGui::Text("Pending move: %s  (+%d)", exPS.word.c_str(), exPS.points);
    else
      ImGui::TextDisabled("Pending move: %s", exPS.error.c_str());
  }
  ImGui::TextDisabled("Last: %s", g_ui.lastAction.c_str());

  ImGui::End();
  PopCreamStyle();
}

// ================================================================ Exploration
// mode
static void DrawExplorationMode() {
  if (!BeginModeScreen(AppMode::Exploration, kTopEx, g_ui.dockBuiltEx,
                       "Legend##legend", "Cream##ex", "Right Panel##ex", 180.0f,
                       440.0f, DrawExplorationTop))
    return;

  DrawExplorationCream();

  ImGui::Begin("Right Panel##ex", nullptr, PANEL_FLAGS);
  static int exTab = 0;
  if (ImGui::Button(exTab == 0 ? "[ Move History ]" : "Move History"))
    exTab = 0;
  ImGui::SameLine();
  if (ImGui::Button(exTab == 1 ? "[ Move Generation ]" : "Move Generation"))
    exTab = 1;
  ImGui::SameLine();
  if (ImGui::Button(exTab == 2 ? "[ Bot Search ]" : "Bot Search"))
    exTab = 2;
  ImGui::Separator();
  if (exTab == 0) {
    const int t = DrawHistoryNavBar(g_ex);
    if (t >= 0)
      GoToCursor(g_ex, t);
    ImGui::Separator();
    static size_t seen = 0;
    DrawMoveHistoryTab(g_ex, seen);
  } else if (exTab == 1) {
    DrawMoveGenerationTab();
  } else {
    DrawBotSearchTab(g_bot, *g_gaddag, g_ex.board, g_ex.trayA, g_ex.boardValid,
                     BotSelect);
  }
  ImGui::End();

  DrawLegendFull();

  ImGuiIO& io = ImGui::GetIO();
  if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) &&
      g_ex.atEnd() && HasUncommitted(g_ex))
    ExUndoTurn();
}
