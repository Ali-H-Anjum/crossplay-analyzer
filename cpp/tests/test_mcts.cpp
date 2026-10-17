#include "check.h"
#include "test_common.h"
#include <chrono>
#include <thread>
#include "mcts.h"

namespace {
struct MFix {
    Board b{}; TileBag bag; Position p{};
    MFix() : bag(3) {
        const Gaddag& g = testGaddag();
        p.b = &b; p.g = &g; p.init = &g.get_init(); p.bag = &bag;
        rebuild(b, p.al, g, g.get_init());
        p.tray[0].addTiles("AEINRST");
    }
};
bool trayEq(const Tray& a, const Tray& b) {
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        if (a.count(fromIndex(uint8_t(i))) != b.count(fromIndex(uint8_t(i)))) return false;
    return true;
}
long long msSince(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}
}

TEST(mcts_worker_restores_state) {
    MFix f;
    const Board b0 = f.b; const AnchorLines al0 = f.p.al; const Tray t0 = f.p.tray[0];
    MctsConfig c; c.simulations = 120; c.simsPerTree = 40;
    const auto roots = MctsWorker(f.p, c, 5).run();
    CHECK(!roots.empty());
    uint64_t v = 0; for (const MRoot& r : roots) v += r.visits;
    CHECK(v == 120);                                         // every simulation visits exactly one root move
    CHECK(f.b.letters_row == b0.letters_row && f.b.letters_col == b0.letters_col);
    CHECK(f.b.row_occ == b0.row_occ && f.b.col_occ == b0.col_occ);
    CHECK(f.b.is_blank == b0.is_blank && f.b.cross_check == b0.cross_check);
    CHECK(f.p.al.by_row == al0.by_row && f.p.al.by_col == al0.by_col);
    CHECK(trayEq(f.p.tray[0], t0));
    CHECK(f.p.score[0] == 0 && f.p.score[1] == 0 && f.p.side == 0 && f.p.finalTurns == 0);
}

TEST(mcts_parallel_reproducible) {
    MFix f;
    MctsConfig c; c.simulations = 150; c.simsPerTree = 50; c.threads = 2; c.seed = 9; c.maxChildren = 12;
    const auto t = std::chrono::steady_clock::now();
    MctsResult a, b;
    { MctsSearch s(f.p, c); CHECK(s.threads() == 2); a = s.take(); }
    const long long ms = msSince(t);
    { MctsSearch s(f.p, c); b = s.take(); }
    std::printf("  %llu sims in %lld ms (%.0f sims/s), %zu root moves, best win rate %.3f\n",
                (unsigned long long)a.sims, ms, a.sims * 1000.0 / std::max(1LL, ms), a.ranked.size(), a.winRate);
    CHECK(a.hasMove && a.sims == 300 && a.best.kind == MK_PLACE);
    CHECK(a.winRate >= 0.0 && a.winRate <= 1.0);
    CHECK(a.ranked.size() <= 14);                            // maxChildren 12 + at most 2 swaps
    uint64_t v = 0; for (const MctsMove& m : a.ranked) v += m.visits;
    CHECK(v == 300);
    for (size_t i = 1; i < a.ranked.size(); ++i) CHECK(a.ranked[i - 1].visits >= a.ranked[i].visits);
    CHECK(b.hasMove && sameMove(a.best, b.best) && a.ranked.size() == b.ranked.size());
    for (size_t i = 0; i < a.ranked.size() && i < b.ranked.size(); ++i) CHECK(a.ranked[i].visits == b.ranked[i].visits);
    CHECK(f.p.tray[0].size() == 7 && f.p.tray[1].size() == 0);   // caller's position untouched
}

TEST(mcts_rollout_to_end_runs) {                              // rolloutMaxPlies = 0: play to game end
    MFix f;
    MctsConfig c; c.simulations = 6; c.simsPerTree = 6; c.threads = 1; c.rolloutMaxPlies = 0; c.epsilon = 0.0;
    const auto t = std::chrono::steady_clock::now();
    const MctsResult r = MctsSearch(f.p, c).take();
    std::printf("  6 full-game rollouts: %lld ms\n", msSince(t));
    CHECK(r.hasMove && r.sims == 6);
}

TEST(mcts_cancel_is_fast) {
    MFix f;
    MctsConfig c; c.simulations = 100000000; c.threads = 2;
    const auto t = std::chrono::steady_clock::now();
    MctsResult r;
    {
        MctsSearch s(f.p, c);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(!s.finished());
        s.cancel();
        r = s.take();
    }
    std::printf("  cancelled after %lld ms, %llu sims\n", msSince(t), (unsigned long long)r.sims);
    CHECK(r.hasMove && msSince(t) < 2000);
}