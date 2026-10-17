#pragma once
// Sim UI: ImGui rendering and UI state for Bot vs Bot mode.
// Depends on sim_game.h, sim_stats.h, sim_csv.h, sim_mover.h and widget/screen headers.

#include <cmath>
#include <cstdio>
#include <random>
#include <thread>

#include "common/theme.h"
#include "panels/bot_search_panel.h"
#include "panels/history_panel.h"
#include "sim_csv.h"
#include "sim_game.h"
#include "sim_mover.h"
#include "sim_stats.h"
#include "state/board_rules.h"
#include "state/state.h"
#include "widgets/bag_panel.h"
#include "widgets/board_canvas.h"
#include "widgets/screen_shell.h"
#include "widgets/top_strip.h"
#include "widgets/tray_widget.h"

// ================================================================ Sim UI state
struct SimUiState {
    float speedMs = 60.0f;
    double lastStep = 0.0;
    bool autoRunning = false;
    bool stepPending = false;
    bool replay = false;
    bool started = false;
    int nGames = 20;
    BatchRun batch;
    SimGame sim;
    int nParallel = 1;
    bool perfInfo = false;
};

static SimUiState g_simUi;

// ================================================================ Sim view sync (snapshot-based)
static const Snapshot& SimViewSnapshot() { return g_simState.turnStart(); }

static void SimSyncView() { ApplyVisible(g_simState, SimViewSnapshot()); }

static void SimGoTo(int target) {
    g_simState.cursor = std::clamp(target, 0, (int)g_simState.history.size());
    SimSyncView();
}

static void SimPushHistory(const SimGame::Turn& t) {
    Entry e;
    static_cast<Snapshot&>(e) =
        MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                     g_simUi.sim.bag.get(), g_simUi.sim.score[0], g_simUi.sim.score[1]);
    e.turnNumber = t.turnNumber;
    e.player = t.player;
    if (t.swapped)
        e.label = "SWAP " + t.swapLetters;
    else if (t.passed)
        e.label = "PASS";
    else
        e.label =
            FormatMoveLabel(t.move.word, t.move.row, t.move.col, t.move.is_descending, t.score);
    g_simState.history.push_back(std::move(e));
    g_simState.cursor = (int)g_simState.history.size();
    SimSyncView();
}

// ================================================================ Sim stepping
static bool SimStepAndRecord() {
    return ::SimStepAndRecord(g_simUi.sim, g_simUi,
                              [](const SimGame::Turn& t) { SimPushHistory(t); });
}

static bool SimTryAdvanceAsync() {
    return ::SimTryAdvanceAsync(g_simUi.sim, g_simUi,
                                [](const SimGame::Turn& t) { SimPushHistory(t); });
}

static void SimReset() {
    std::random_device rd;
    const uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
    g_simUi.sim.reset(g_gaddag, seed);
    g_simState.history.clear();
    g_simState.cursor = 0;
    g_simState.initial =
        MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                     g_simUi.sim.bag.get(), g_simUi.sim.score[0], g_simUi.sim.score[1]);
    SimSyncView();
    g_simUi.started = true;
    g_simUi.replay = false;
    g_simUi.autoRunning = false;
    g_simUi.stepPending = false;
    g_simUi.lastStep = ImGui::GetTime();
}

static void BatchStart(int n, int parallel) {
    BatchRun& b = g_simUi.batch;
    if (b.running || !g_gaddag) return;
    b.stop();
    b.cancel = false;
    b.finished = false;
    b.done = 0;
    b.next = 0;
    b.total = std::clamp(n, 1, 1000);
    if (b.total > 1 && (b.total & 1)) ++b.total;
    const int cores = std::max(1, (int)std::thread::hardware_concurrency());
    b.parallel = std::clamp(parallel, 1, std::min(cores, b.total));
    b.perfInfo = g_simUi.perfInfo;
    b.summary.clear();
    b.showSummary = false;
    for (int i = 0; i < 2; ++i) {
        CopyMoverSettings(b.cfg[i], g_simUi.sim.movers[i]);
        if (b.parallel > 1) b.cfg[i].search.threads = 1;
    }
    std::random_device rd;
    b.baseSeed = ((uint64_t)rd() << 32) ^ rd();
    b.runId = TimeStamp();
    b.res.assign(b.total, BatchGameRes{});
    b.t0 = std::chrono::steady_clock::now();
    g_simUi.autoRunning = false;
    g_simUi.stepPending = false;
    b.running = true;
    b.alive = b.parallel;
    for (int i = 0; i < b.parallel; ++i) b.pool.emplace_back([&b] { b.worker(g_gaddag); });
}

static void BatchPoll() {
    BatchRun& b = g_simUi.batch;
    if (b.running && b.finished) {
        b.stop();
        b.running = false;
        b.showSummary = true;
    }
}

static void DrawBatchPopup() {
    BatchRun& b = g_simUi.batch;
    if (!b.running && !b.showSummary) return;
    CenterNextWindow();
    ImGui::Begin(b.running ? "Running batch##sim" : "Batch results##sim", nullptr,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    if (b.running) {
        const int done = b.done;
        const double el =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - b.t0).count();
        ImGui::Text("Game %d / %d    %.0f s elapsed", std::min(done + 1, b.total), b.total, el);
        ImGui::ProgressBar(b.total ? (float)done / (float)b.total : 0.0f, ImVec2(420, 0));
        if (ImGui::Button("Cancel", ImVec2(100, 28))) b.cancel = true;
    } else {
        ImGui::InputTextMultiline("##sum", b.summary.data(), b.summary.size() + 1, ImVec2(900, 520),
                                  ImGuiInputTextFlags_ReadOnly);
        static double copiedAt = -10.0;
        if (ImGui::Button("Copy", ImVec2(100, 28))) {
            ImGui::SetClipboardText(b.summary.c_str());
            copiedAt = ImGui::GetTime();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 28))) b.showSummary = false;
        if (ImGui::GetTime() - copiedAt < 2.0) {
            ImGui::SameLine();
            ImGui::TextDisabled("Copied.");
        }
    }
    ImGui::End();
}
static void SimTopOnBack() {
    BatchRun& b = g_simUi.batch;
    b.stop();
    b.running = false;
    b.showSummary = false;
    LeaveToTitle();
}
static void SimTopOnRematch() { SimReset(); }

static void SimTopExtra() {
    const bool batching = g_simUi.batch.running;
    const bool live = !g_simUi.replay && !g_simUi.sim.gameOver && !batching;
    const bool thinking = g_simUi.sim.movers[(int)(g_simUi.sim.turns.size() & 1)].search.running();

    if (!live || thinking) ImGui::BeginDisabled();
    if (ImGui::Button(g_simUi.autoRunning ? "Pause" : "Run")) {
        g_simUi.autoRunning = !g_simUi.autoRunning;
        if (g_simUi.autoRunning) {
            SimGoTo((int)g_simState.history.size());
            g_simUi.lastStep = ImGui::GetTime();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        g_simUi.autoRunning = false;
        SimGoTo((int)g_simState.history.size());
        g_simUi.stepPending = true;
    }
    if (!live || thinking) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12, 0));
    ImGui::SameLine();
    if (batching || thinking || !g_gaddag) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("Games", &g_simUi.nGames, 0, 0);
    g_simUi.nGames = std::clamp(g_simUi.nGames, 1, 1000);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    ImGui::InputInt("Parallel", &g_simUi.nParallel, 0, 0);
    g_simUi.nParallel =
        std::clamp(g_simUi.nParallel, 1, std::max(1, (int)std::thread::hardware_concurrency()));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Games played at once.\n"
            "1 = one game at a time, each search uses its configured threads.\n"
            ">1 = that many games at once, each search forced to 1 thread.\n"
            "Keep at or below your core count: timed searches lose depth when oversubscribed.");
    ImGui::SameLine();
    ImGui::Checkbox("Perfect-info", &g_simUi.perfInfo);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Batch only. After each search turn, re-run alpha-beta with the opponent's TRUE rack\n"
            "known and record how much the chosen move loses against it.\n"
            "Roughly doubles the time of every search turn.");
    ImGui::SameLine();
    if (ImGui::Button("Batch")) BatchStart(g_simUi.nGames, g_simUi.nParallel);
    if (batching || thinking || !g_gaddag) ImGui::EndDisabled();
}

static void DrawSimTop() {
    TopStripCallbacks cb;
    cb.onBack = SimTopOnBack;
    cb.onRematch = SimTopOnRematch;
    cb.extra = SimTopExtra;
    cb.modeName = "Bot vs Bot";
    DrawModeTopStrip(g_simState, cb);
}

static void DrawBotThinkingBar(int p) {
    BotPanel& bp = g_simUi.sim.movers[p].search;
    if (!bp.running()) return;
    const double sec = bp.det    ? bp.det->elapsedMs() / 1000.0
                       : bp.mcts ? bp.mcts->elapsedMs() / 1000.0
                                 : 0.0;
    if (ImGui::Button("Stop", ImVec2(80, 28))) {
        g_simUi.autoRunning = false;
        g_simUi.stepPending = true;
        bp.cancel();
    }
    ImGui::SameLine();
    ImGui::Text("Thinking... %.1f s", sec);
    ImGui::Separator();
}

static void DrawSimCream() {
    PushCreamStyle();
    ImGui::Begin("Cream##sim", nullptr, PANEL_FLAGS);

    DrawBoardAndBag(BoardKind::Sim, g_simState, false, SnapshotBagView(SimViewSnapshot()));

    ImGui::Separator();

    const float trayCell = 46.0f, trayGap = 8.0f;
    const float trayW = trayCell * kRackSize + trayGap * (kRackSize - 1);
    const float availW = ImGui::GetContentRegionAvail().x;
    const float baseX = ImGui::GetCursorPosX();
    const float scoreRowTop = ImGui::GetCursorPosY();
    const float trayRowTop = scoreRowTop + ImGui::GetTextLineHeight() * 2.0f;

    auto drawPlayer = [&](const char* name, int score, const Tray& tray, float x) {
        ImGui::SetCursorPosY(scoreRowTop);
        ImGui::SetCursorPosX(x);
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("%s: %d", name, score);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPosY(trayRowTop);
        ImGui::SetCursorPosX(x);
        DrawTrayStrip(TrayLetters(tray), false, nullptr);
    };
    drawPlayer("P1", g_simState.score[0], g_simState.trayA, baseX);
    drawPlayer("P2", g_simState.score[1], g_simState.trayB, baseX + availW - trayW);

    ImGui::Separator();

    const auto& sim = g_simUi.sim;
    const int p = (int)(sim.turns.size() & 1);
    const bool atEnd = g_simState.atEnd();
    const bool thinking = !g_simUi.replay && !sim.gameOver && sim.movers[p].search.running();
    const char* statusTxt = !atEnd           ? "VIEWING HISTORY"
                            : g_simUi.replay ? "REPLAY"
                            : sim.gameOver   ? "GAME OVER"
                            : thinking       ? (p == 0 ? "P1 THINKING" : "P2 THINKING")
                                             : (g_simState.history.empty() ? "READY" : "RUNNING");
    const ImU32 statusCol = !atEnd                             ? IM_COL32(200, 160, 60, 255)
                            : (g_simUi.replay || sim.gameOver) ? IM_COL32(220, 80, 80, 255)
                            : thinking                         ? IM_COL32(80, 150, 220, 255)
                                                               : IM_COL32(80, 180, 80, 255);
    {
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::SetWindowFontScale(1.8f);
        const float tw = ImGui::CalcTextSize(statusTxt).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
        ImGui::TextUnformatted(statusTxt);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::End();
    PopCreamStyle();
}

static void DrawSimMode() {
    if (!g_simUi.started) SimReset();
    BatchPoll();

    if (!BeginModeScreen(AppMode::BotVsBot, kTopSim, g_ui.dockBuiltSim, "Left Panel##sim",
                         "Cream##sim", "Right Panel##sim", 300.0f, 440.0f, DrawSimTop))
        return;

    DrawSimCream();

    MoverConfig* mv = g_simUi.sim.movers;

    ImGui::Begin("Left Panel##sim", nullptr, PANEL_FLAGS);
    ImGui::TextUnformatted("Bot 1");
    ImGui::Separator();
    DrawBotThinkingBar(0);
    DrawBotConfigPanel(mv[0].search, mv[0].search.running(), &mv[0].kind);
    ImGui::End();

    ImGui::Begin("Right Panel##sim", nullptr, PANEL_FLAGS);
    DrawBotThinkingBar(1);
    static int simTab = 0;
    if (ImGui::Button(simTab == 0 ? "[ Bot 2 ]" : "Bot 2")) simTab = 0;
    ImGui::SameLine();
    if (ImGui::Button(simTab == 1 ? "[ Move History ]" : "Move History")) simTab = 1;
    ImGui::Separator();
    if (simTab == 0) {
        DrawBotConfigPanel(mv[1].search, mv[1].search.running(), &mv[1].kind);
    } else {
        const int t = DrawHistoryNavBar(g_simState);
        if (t >= 0) SimGoTo(t);
        ImGui::Separator();
        static size_t seen = 0;
        DrawMoveHistoryTab(g_simState, seen);
    }
    ImGui::End();

    if (g_simUi.replay || g_simUi.sim.gameOver) {
        g_simUi.autoRunning = false;
        g_simUi.stepPending = false;
    } else if ((g_simUi.autoRunning || g_simUi.stepPending) && g_simState.atEnd()) {
        if (SimTryAdvanceAsync()) g_simUi.stepPending = false;
    }

    DrawBatchPopup();
}
