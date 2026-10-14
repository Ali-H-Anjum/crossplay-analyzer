#pragma once
#include <algorithm>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include "imgui.h"
#include "DetSearch.h"
#include "Mcts.h"

struct BotPanel {
    int  algo = 0;                       // 0 = Alpha-Beta (determinized), 1 = MCTS
    int  threads = 0;                    // 0 = cores - 1
    bool iterative = true;
    int  abSeconds = 600, mctsSeconds = 600;   // default 10 minutes; Stop ends it early
    SearchConfig sc; DetConfig dc; MctsConfig mc;

    std::unique_ptr<DetSearch>  det;
    std::unique_ptr<MctsSearch> mcts;
    DetResult detRes; MctsResult mctsRes;
    bool haveResult = false; int resultAlgo = 0;
    int  sel = -1;                       // selected row in the ranked list
    bool selPlace = false; RawMove selRaw{};   // the selected row's move, valid when selPlace
    Board rootBoard{};                   // board the search ran on (used for move points)
    Tray  rootRack{};                    // rack the search ran on

    BotPanel() { sc.killers = true; sc.lmr = true; }
    bool running() const { return det != nullptr || mcts != nullptr; }
    void clearSel() { sel = -1; selPlace = false; }
    // The selected placed move, or nullptr.
    const RawMove* selected() const { return (haveResult && selPlace) ? &selRaw : nullptr; }

    void start(const Gaddag& g, const Board& b, const Tray& rack) {
        rootBoard = b; rootRack = rack;
        Position root{};
        rebuild(rootBoard, root.al, g, g.get_init());
        root.b = &rootBoard; root.g = &g; root.init = &g.get_init(); root.tray[0] = rack;
        haveResult = false; clearSel();
        dc.threads = mc.threads = std::max(0, threads);
        dc.rootMaxMoves = std::max(0, dc.rootMaxMoves);
        if (algo == 0) {
            SearchConfig c = sc;
            c.iterative = iterative; c.depth = std::max(1, sc.depth);
            c.timeLimitMs = std::clamp(abSeconds, 1, 86400) * 1000;
            det = std::make_unique<DetSearch>(root, c, dc);
        } else {
            MctsConfig c = mc;
            c.timeLimitMs = std::clamp(mctsSeconds, 1, 86400) * 1000;
            c.simulations = std::max(1, c.simulations); c.simsPerTree = std::max(1, c.simsPerTree);
            mcts = std::make_unique<MctsSearch>(root, c);
        }
    }
    void poll() {
        if (det && det->finished())   { detRes = det->take();   det.reset();  haveResult = true; resultAlgo = 0; clearSel(); }
        if (mcts && mcts->finished()) { mctsRes = mcts->take(); mcts.reset(); haveResult = true; resultAlgo = 1; clearSel(); }
    }
    void cancel()   { if (det) det->cancel(); if (mcts) mcts->cancel(); }
    void shutdown() { cancel(); det.reset(); mcts.reset(); }   // joins all threads

    // ---- shared cell formats (same as Move Generation: 0-based, H/V) ----
    int pts(const SMove& s) const { return s.kind == MK_PLACE ? scoreRaw(rootBoard, s.m) : 0; }
    std::string fmtWord(const SMove& s, const Tray& thrown) const {
        if (s.kind == MK_PASS) return "PASS";
        if (s.kind == MK_SWAP) {
            std::string t = "SWAP ";
            for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
                for (int n = thrown.count(fromIndex(static_cast<uint8_t>(i))); n > 0; --n)
                    t += (i == BLANK_SLOT) ? '?' : static_cast<char>('A' + i);
            return t;
        }
        std::string w;
        for (int i = 0; i < s.m.len; ++i) {
            char ch = static_cast<char>('A' + s.m.letters[i] - 1);
            if ((s.m.blank_mask >> i) & 1) ch = static_cast<char>(ch + 32);
            w += ch;
        }
        return w;
    }
    std::string fmtDirPos(const SMove& s) const {
        if (s.kind != MK_PLACE) return "";
        char b[32];
        std::snprintf(b, sizeof b, "%c (%2d,%2d)", s.m.is_descending ? 'V' : 'H', (int)s.m.row, (int)s.m.col);
        return b;
    }
    std::string describe(const SMove& s, const Tray& thrown) const {
        std::string t = fmtWord(s, thrown);
        if (s.kind != MK_PLACE) return t;
        char b[64];
        std::snprintf(b, sizeof b, "  %s  +%d", fmtDirPos(s).c_str(), pts(s));
        return t + b;
    }
};

// Scrollable, clickable rows. label(i, buf, n) formats row i; moveAt(i) returns its SMove.
template <class Label, class MoveAt>
inline void DrawRankRows(BotPanel& bp, int count, Label label, MoveAt moveAt, void (*onSelect)(const RawMove*)) {
    const float h = std::max(220.0f, ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##botmoves", ImVec2(0, h), true);
    ImGuiListClipper clipper;
    clipper.Begin(count);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            char buf[200];
            label(i, buf, sizeof buf);
            ImGui::PushID(i);
            if (ImGui::Selectable(buf, bp.sel == i)) {
                const SMove& m = moveAt(i);
                if (bp.sel == i) { bp.clearSel(); onSelect(nullptr); }
                else {
                    bp.sel = i; bp.selPlace = (m.kind == MK_PLACE);
                    if (bp.selPlace) bp.selRaw = m.m;
                    onSelect(bp.selPlace ? &m.m : nullptr);
                }
            }
            ImGui::PopID();
        }
    ImGui::EndChild();
}

inline void DrawBotSearchTab(BotPanel& bp, const Gaddag& g, const Board& board, const Tray& rack, bool boardValid,
                             void (*onSelect)(const RawMove*)) {
    bp.poll();
    const bool running = bp.running();

    ImGui::BeginDisabled(running);

    ImGui::SetNextItemWidth(150);
    ImGui::Combo("Algorithm", &bp.algo, "Determinized AB\0MCTS\0");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Which search to run.\n"
            "Determinized AB: alpha-beta on a sampled full-information state.\n"
            "Pros: fast, strong with good heuristics.\n"
            "Cons: assumes opponent's tiles are known (strategy fusion).\n\n"
            "MCTS: Monte Carlo Tree Search with random playouts.\n"
            "Pros: handles uncertainty more naturally.\n"
            "Cons: slower, weaker without strong rollouts.");

    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Threads (0 = cores-1)", &bp.threads);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Number of worker threads. 0 = all cores minus one.\n"
            "More threads = more parallel search, but diminishing returns.");

    if (bp.algo == 0) {
        ImGui::SeparatorText("Limits");

        ImGui::Checkbox("Iterative deepening", &bp.iterative);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Search depth 1, then 2, then 3, ... until time runs out.\n"
                "Pros: always has a usable answer; can stop anytime.\n"
                "Cons: repeats work each iteration (usually minor).");

        ImGui::SetNextItemWidth(120);
        if (bp.iterative) ImGui::InputInt("Time limit (s)", &bp.abSeconds);
        else              ImGui::InputInt("Depth limit (plies)", &bp.sc.depth);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How long to search (or how deep if iterative is off).\n"
                "Longer/deeper = stronger, but slower.");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Root moves kept", &bp.dc.rootMaxMoves);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How many candidate moves to keep and show.\n"
                "Doesn't change which move is chosen, only how many are listed.");

        ImGui::SeparatorText("Move ordering");

        ImGui::Checkbox("Static move ordering", &bp.sc.orderStatic);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Sort moves by a cheap estimate before searching.\n"
                "Pros: best moves searched first → earlier cutoffs → much faster.\n"
                "Cons: none meaningful.");

        ImGui::Checkbox("Killer move heuristic", &bp.sc.killers);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Remember moves that caused cutoffs at each ply.\n"
                "Try them first at sibling nodes.\n"
                "Pros: speeds up search. Cons: minimal memory overhead.");

        ImGui::Checkbox("History heuristic", &bp.sc.history);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Track which moves have been good across the whole search.\n"
                "Pros: improves ordering. Cons: extra memory, tiny per-node cost.");

        ImGui::SeparatorText("Forward pruning");

        ImGui::Checkbox("Late move reduction", &bp.sc.lmr);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Search late-ordered moves to reduced depth first.\n"
                "If they look promising, re-search deeper.\n"
                "Pros: large speedup. Cons: can miss rare tactics.");

        ImGui::Checkbox("ProbCut", &bp.sc.probCut);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Skip a deep search if a shallow one says the move is bad.\n"
                "Pros: speedup. Cons: riskier than LMR.");

        ImGui::Checkbox("Beam Search (30,15,8,5...)", &bp.sc.useBeam);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "At each ply, only search the top N moves (30, then 15, ...).\n"
                "Pros: fast. Cons: can miss the best move entirely.");

        ImGui::SeparatorText("Cache");

        ImGui::Checkbox("Transposition table", &bp.sc.useTT);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Cache results of searched positions.\n"
                "Pros: big speedup when the same position recurs.\n"
                "Cons: memory; hash collisions can rarely confuse it.");
    } else {
        ImGui::SeparatorText("Limits");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Time limit (s)", &bp.mctsSeconds);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("How long to run before picking the best move found so far.");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Simulations / thread", &bp.mc.simulations);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How many random playouts each thread runs.\n"
                "More sims = stronger, slower.");

        ImGui::SeparatorText("Tree");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Sims per tree", &bp.mc.simsPerTree);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How many sims to run before the tree is reused/expanded.\n"
                "Tradeoff between tree refinement and restart cost.");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Max children", &bp.mc.maxChildren);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How many moves per node are considered.\n"
                "Higher = broader search, but slower.");

        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble("UCB1 C", &bp.mc.C, 0.05, 0.25, "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Exploration constant in the UCB1 formula.\n"
                "Higher = try less-visited moves more.\n"
                "Lower = exploit known-good moves.\n"
                "Theory suggests sqrt(2) ~ 1.41; tune per game.");

        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble("Epsilon", &bp.mc.epsilon, 0.05, 0.1, "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Probability of picking a random move instead of the tree's choice.\n"
                "Higher = more exploration, less consistency.");

        ImGui::SeparatorText("Rollout");

        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Rollout plies (0 = to end)", &bp.mc.rolloutMaxPlies);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "How many plies to simulate from a leaf.\n"
                "0 = play to end of game (accurate but slow).\n"
                "Higher values are more accurate but slower.");
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    const bool canStart = !running && boardValid && rack.size() > 0;
    if (!canStart) ImGui::BeginDisabled();
    if (ImGui::Button("Start Search", ImVec2(120, 28))) { bp.start(g, board, rack); onSelect(nullptr); }
    if (!canStart) ImGui::EndDisabled();
    ImGui::SameLine();
    if (!running) ImGui::BeginDisabled();
    if (ImGui::Button("Stop", ImVec2(80, 28))) bp.cancel();
    if (!running) ImGui::EndDisabled();
    if (!running && !canStart) ImGui::TextDisabled("Needs a valid board and at least one tile in the rack.");

    if (bp.det) {
        ImGui::Text("%d threads | depth %d | %llu nodes | %.2f s", bp.det->threads(), bp.det->minDepth(),
                    (unsigned long long)bp.det->nodes(), bp.det->elapsedMs() / 1000.0);
        const SearchProgress& lp = bp.det->live(0);
        std::lock_guard<std::mutex> lk(lp.mu);
        if (lp.hasBest) ImGui::TextWrapped("Thread 0 best so far: %s", bp.describe(lp.best, lp.thrown).c_str());
    }
    if (bp.mcts)
        ImGui::Text("%d threads | %llu sims | %.2f s", bp.mcts->threads(),
                    (unsigned long long)bp.mcts->sims(), bp.mcts->elapsedMs() / 1000.0);

    if (!bp.haveResult) return;
    ImGui::Separator();
    char hdr[160];
    if (bp.resultAlgo == 0) {
        const DetResult& r = bp.detRes;
        if (!r.hasMove) { ImGui::TextDisabled("No result."); return; }
        ImGui::TextWrapped("Best: %s", bp.describe(r.best, r.swapThrown).c_str());
        ImGui::Text("Depth %d | Nodes %llu | Det %d | Time %.2fs", r.depth, (unsigned long long)r.nodes,
                    r.samples, r.timeMs / 1000.0);
        std::snprintf(hdr, sizeof hdr, "%-3s %7s %-5s %-3s %-15s %s", "#", "Eval", "Depth", "Pts", "Word", "Dir & Pos");
        ImGui::Separator(); ImGui::TextUnformatted(hdr); ImGui::Separator();
        DrawRankRows(bp, static_cast<int>(r.ranked.size()),
            [&](int i, char* b, size_t n) {
                const DetMove& d = r.ranked[static_cast<size_t>(i)];
                std::snprintf(b, n, "%-3d %+7.1f %-5d %-3d %-15.15s %s", i + 1, d.avg / 10.0, r.depth,
                              bp.pts(d.m), bp.fmtWord(d.m, d.thrown).c_str(), bp.fmtDirPos(d.m).c_str());
            },
            [&](int i) -> const SMove& { return r.ranked[static_cast<size_t>(i)].m; }, onSelect);
    } else {
        const MctsResult& r = bp.mctsRes;
        if (!r.hasMove) { ImGui::TextDisabled("No result."); return; }
        ImGui::TextWrapped("Best: %s", bp.describe(r.best, r.swapThrown).c_str());
        ImGui::Text("Sims %llu | Threads %d | Time %.2fs", (unsigned long long)r.sims, r.threads, r.timeMs / 1000.0);
        std::snprintf(hdr, sizeof hdr, "%-3s %6s %-7s %-7s %-3s %-15s %s", "#", "Win%", "Sims", "Visits", "Pts",
                      "Word", "Dir & Pos");
        ImGui::Separator(); ImGui::TextUnformatted(hdr); ImGui::Separator();
        DrawRankRows(bp, static_cast<int>(r.ranked.size()),
            [&](int i, char* b, size_t n) {
                const MctsMove& d = r.ranked[static_cast<size_t>(i)];
                std::snprintf(b, n, "%-3d %6.1f %-7llu %-7llu %-3d %-15.15s %s", i + 1, d.winRate * 100.0,
                              (unsigned long long)r.sims, (unsigned long long)d.visits, bp.pts(d.m),
                              bp.fmtWord(d.m, d.thrown).c_str(), bp.fmtDirPos(d.m).c_str());
            },
            [&](int i) -> const SMove& { return r.ranked[static_cast<size_t>(i)].m; }, onSelect);
    }
}