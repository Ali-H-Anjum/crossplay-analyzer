#pragma once
// Board + bag screen shell, tray interaction, and bag views.
#include "widgets/bag_panel.h"
#include "widgets/screen_shell.h"
#include "widgets/tray_widget.h"

// ================================================================ Player vs
// Greedy
static void PvbStartGame(int firstPlayer);

static int RackValue(const Tray& t) {
  int s = 0;
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
    s += POINTS_PER_TILE[i] * (int)t.counts[i];
  return s;
}

static Placement PvbEvaluateTurn() {
  Placement ps = EvaluatePlacement(g_pvb.board, g_pvb.turnStart().board);
  if (ps.ok && !g_pvb.boardValid) {
    ps.ok = false;
    ps.error = "Board has invalid word(s).";
  }
  return ps;
}

static void PvbRefill(Tray& tray) {
  const int need =
      std::min(kRackSize - (int)tray.size(), (int)g_pvb.bag->size());
  if (need > 0)
    g_pvb.bag->drawInto(tray, (uint8_t)need);
}

// Called after a move or pass by `player` has been recorded in history.
static void PvbFinishTurn(int player) {
  ModeState& ms = g_pvb;
  const Tray& mine = (player == 0) ? ms.trayA : ms.trayB;
  const bool bagEmpty = !ms.bag || ms.bag->size() == 0;
  bool over = false;
  if (bagEmpty && mine.size() == 0) {  // went out
    const int a = RackValue(ms.trayA), b = RackValue(ms.trayB);
    ms.score[0] -= a;
    ms.score[1] -= b;
    ms.score[player] += a + b;
    over = true;
  } else if (g_pvbUi.consecutivePasses >= 2) {  // both sides passed
    ms.score[0] -= RackValue(ms.trayA);
    ms.score[1] -= RackValue(ms.trayB);
    over = true;
  }
  if (over) {
    g_pvbUi.gameOver = true;
    g_pvbUi.winner = (ms.score[0] > ms.score[1])   ? 0
                     : (ms.score[1] > ms.score[0]) ? 1
                                                   : -1;
    if (!ms.history.empty()) {  // final history entry shows final scores
      ms.history.back().score[0] = ms.score[0];
      ms.history.back().score[1] = ms.score[1];
    }
    return;
  }
  g_pvbUi.turn = 1 - player;
  g_pvbUi.botWait = kBotWaitFrames;
}

static void PvbPlayMove() {
  const Placement ps = PvbEvaluateTurn();
  if (!ps.ok) {
    g_ui.lastAction = "Cannot play: " + ps.error;
    return;
  }
  g_pvb.score[0] += ps.points;
  g_pvbUi.consecutivePasses = 0;
  PvbRefill(
      g_pvb.trayA);  // refill first so the history entry matches the result
  PushEntry(g_pvb, FormatMove(ps), 0);
  g_ui.lastAction = "Played " + ps.word + ".";
  PvbFinishTurn(0);
}

static void PvbHumanPass() {
  // Return any staged tiles before passing.
  ApplySnapshot(g_pvb, g_pvb.turnStart());
  ++g_pvbUi.consecutivePasses;
  PushEntry(g_pvb, "PASS", 0);
  PvbFinishTurn(0);
}

static void PvbBotTurn() {
  ModeState& ms = g_pvb;
  MoveEvaluator ev;
  ev.setBoard(&ms.board);
  MoveList sink;
  generateMovesFromScratch(*g_gaddag, ms.board, ms.trayB, sink);
  const RawMove* best = nullptr;
  int bestScore = -1;
  for (auto& rm : sink.moves) {
    const int sc = ev.score(rm).points;
    if (sc > bestScore) {
      bestScore = sc;
      best = &rm;
    }
  }
  if (!best) {
    ++g_pvbUi.consecutivePasses;
    PushEntry(ms, "PASS", 1);
  } else {
    Move mv = toMove(*best);
    g_pvbUi.consecutivePasses = 0;
    for (const auto& pl : mv.letterPositions()) {
      const int idx = sq(pl.row, pl.col);
      if (ms.board.letters_row[idx] != EMPTY_SQUARE)
        continue;
      placeTile(ms.board, pl.row, pl.col, pl.letter, pl.is_blank);
      ms.trayB.removeTile(pl.is_blank ? Letter::Blank : pl.letter);
    }
    ms.score[1] += bestScore;
    PvbRefill(ms.trayB);
    PushEntry(
        ms,
        FormatMoveLabel(mv.word, mv.row, mv.col, mv.is_descending, bestScore),
        1);
  }
  RefreshDerived(ms);
  PvbFinishTurn(1);
}

static void PvbStartGame(int firstPlayer) {
  std::random_device rd;
  const uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
  g_pvb.bag = std::make_unique<TileBag>(seed);
  g_pvb.board = Board{};
  g_pvb.trayA = Tray{};
  g_pvb.trayB = Tray{};
  g_pvb.bag->drawInto(g_pvb.trayA, kRackSize);
  g_pvb.bag->drawInto(g_pvb.trayB, kRackSize);
  g_pvb.history.clear();
  g_pvb.cursor = 0;
  g_pvb.score[0] = g_pvb.score[1] = 0;
  g_pvbUi.gameOver = false;
  g_pvbUi.gameOverDismissed = false;
  g_pvbUi.winner = -1;
  g_pvbUi.consecutivePasses = 0;
  g_pvbUi.turn = firstPlayer;
  g_pvbUi.botWait = kBotWaitFrames;
  g_ui.lastAction = "(game started)";
  RefreshDerived(g_pvb);
  g_pvb.initial = SnapshotOf(g_pvb);
  EndDrag();
  g_mode = AppMode::PlayerVsBot;
}

static void RequestRematchOrTitle(ConfirmAction a) {
  const bool inProgress = !g_pvbUi.gameOver && !g_pvb.history.empty();
  if (inProgress) {
    g_ui.confirmAction = a;
    g_ui.confirmBackOpen = true;
    return;
  }
  if (a == ConfirmAction::Title)
    LeaveToTitle();
  else
    PvbStartGame((int)(std::random_device{}() & 1));
}

static void PvbTopOnBack() {
  RequestRematchOrTitle(ConfirmAction::Title);
}
static void PvbTopOnRematch() {
  RequestRematchOrTitle(ConfirmAction::Rematch);
}

static void DrawPvbTop() {
  TopStripCallbacks cb;
  cb.onBack = PvbTopOnBack;
  cb.onRematch = PvbTopOnRematch;
  cb.extra = nullptr;
  cb.modeName = "Player vs Greedy";
  DrawModeTopStrip(g_pvb, cb);
}

static void DrawPvbCream() {
  PushCreamStyle();
  ImGui::Begin("Cream##pvb", nullptr, PANEL_FLAGS);

  const bool interactive =
      g_pvb.atEnd() && g_pvbUi.turn == 0 && !g_pvbUi.gameOver;

  DrawBoardAndBag(BoardKind::Pvb, g_pvb, interactive, LiveBagView(g_pvb));

  ImGui::Separator();
  DrawTrayInteractive(g_pvb, interactive, false);

  const bool hasUncommitted = interactive && HasUncommitted(g_pvb);
  Placement ps;
  if (hasUncommitted)
    ps = PvbEvaluateTurn();
  const bool canPlay = hasUncommitted && ps.ok;

  PushPanelButton();
  if (!canPlay)
    ImGui::BeginDisabled();
  if (ImGui::Button("Play Move", ImVec2(120, 32)))
    PvbPlayMove();
  if (!canPlay)
    ImGui::EndDisabled();

  ImGui::SameLine();
  const bool canUndo = hasUncommitted;
  if (!canUndo)
    ImGui::BeginDisabled();
  if (ImGui::Button("Undo Turn", ImVec2(120, 32))) {
    ApplySnapshot(g_pvb, g_pvb.turnStart());
    g_ui.lastAction = "Undid turn.";
  }
  if (!canUndo)
    ImGui::EndDisabled();

  ImGui::SameLine();
  if (!interactive)
    ImGui::BeginDisabled();
  if (ImGui::Button("Pass", ImVec2(80, 32)))
    PvbHumanPass();
  if (!interactive)
    ImGui::EndDisabled();
  PopPanelButton();

  if (hasUncommitted && !ps.ok && !ps.error.empty())
    ImGui::TextDisabled("%s", ps.error.c_str());
  else if (hasUncommitted && ps.ok)
    ImGui::TextDisabled("%s: +%d", ps.word.c_str(), ps.points);
  else
    ImGui::TextDisabled("Last: %s", g_ui.lastAction.c_str());

  ImGui::Separator();
  const bool viewing = !g_pvb.atEnd();
  const char* statusTxt =
      g_pvbUi.gameOver
          ? "GAME OVER"
          : (viewing ? "VIEWING HISTORY"
                     : (g_pvbUi.turn == 0 ? "YOUR TURN" : "BOT'S TURN"));
  const ImU32 statusCol =
      g_pvbUi.gameOver
          ? IM_COL32(220, 80, 80, 255)
          : (viewing ? IM_COL32(200, 160, 60, 255)
                     : (g_pvbUi.turn == 0 ? IM_COL32(80, 180, 80, 255)
                                          : IM_COL32(220, 180, 60, 255)));
  DrawScoresLine("You", g_pvb.score[0], "Bot", g_pvb.score[1], statusTxt,
                 statusCol);

  ImGui::End();
  PopCreamStyle();
}

static void DrawPvbMode() {
  if (!BeginModeScreen(AppMode::PlayerVsBot, kTopPvb, g_ui.dockBuiltPvb,
                       "Legend##legend", "Cream##pvb", "Right Panel##pvb",
                       180.0f, 440.0f, DrawPvbTop))
    return;

  DrawPvbCream();

  ImGui::Begin("Right Panel##pvb", nullptr, PANEL_FLAGS);
  {
    const int t = DrawHistoryNavBar(g_pvb);
    if (t >= 0)
      GoToCursor(g_pvb, t);
  }
  ImGui::Separator();
  {
    static size_t seen = 0;
    DrawMoveHistoryTab(g_pvb, seen);
  }
  ImGui::End();

  DrawLegendCompact();

  if (!g_pvbUi.gameOver && g_pvbUi.turn == 1 && g_pvb.atEnd()) {
    if (g_pvbUi.botWait > 0)
      --g_pvbUi.botWait;
    else
      PvbBotTurn();
  }

  const bool dialogOpen = g_ui.blankPopupOpen || g_ui.confirmBackOpen ||
                          g_ui.savePopupOpen || g_ui.loadPopupOpen;
  if (g_pvbUi.turn == 0 && !g_pvbUi.gameOver && g_pvb.atEnd() && !dialogOpen &&
      !ImGui::GetIO().WantTextInput &&
      (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
    if (PvbEvaluateTurn().ok)
      PvbPlayMove();
  }

  if (g_pvbUi.gameOver && !g_pvbUi.gameOverDismissed) {
    CenterNextWindow();
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    ImGui::Begin("Game Over##pvb", nullptr,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Final: You = %d   Bot = %d", g_pvb.score[0], g_pvb.score[1]);
    if (g_pvbUi.winner < 0)
      ImGui::TextUnformatted("Tie!");
    else if (g_pvbUi.winner == 0)
      ImGui::TextUnformatted("You win!");
    else
      ImGui::TextUnformatted("Bot wins.");
    ImGui::Separator();
    const bool back = ImGui::Button("Back to Title", ImVec2(-1, 32));
    const bool rematch = ImGui::Button("Rematch", ImVec2(-1, 32));
    const bool view = ImGui::Button("View Board", ImVec2(-1, 0));
    ImGui::End();
    if (back)
      LeaveToTitle();
    else if (rematch)
      PvbStartGame((int)(std::random_device{}() & 1));
    else if (view)
      g_pvbUi.gameOverDismissed = true;
  }
}
