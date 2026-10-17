#include "check.h"
#include "test_common.h"
#include <chrono>
#include "search.h"

struct Fixture {
    Board b{}; TileBag bag; Position p;
    explicit Fixture(uint64_t seed) : bag(seed) {
        const Gaddag& g = testGaddag();
        p.b = &b; p.g = &g; p.init = &g.get_init(); p.bag = &bag;
        rebuild(b, p.al, g, g.get_init());
        bag.shuffle();
        bag.drawNextInto(p.tray[0], 7); bag.drawNextInto(p.tray[1], 7);
    }
};
static bool trayEq(const Tray& a, const Tray& b) {
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        if (a.count(fromIndex(uint8_t(i))) != b.count(fromIndex(uint8_t(i)))) return false;
    return true;
}

static void checkRestore(Fixture& f, int depth) {
    const Board b0 = f.b; const AnchorLines al0 = f.p.al;
    const Tray t0 = f.p.tray[0], t1 = f.p.tray[1];
    const auto bag0 = f.bag.tilesCopy(); const uint16_t n0 = f.bag.totalCount();
    const int s0 = f.p.score[0], s1 = f.p.score[1], side0 = f.p.side, ft0 = f.p.finalTurns;

    SearchConfig c; c.depth = depth;
    const auto t = std::chrono::steady_clock::now();
    const SearchResult r = Searcher(f.p, c).run();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
    std::printf("  depth %d: nodes=%llu  %lld ms  value=%d\n", depth, (unsigned long long)r.nodes, (long long)ms, r.value);

    CHECK(r.hasMove);
    CHECK(f.b.letters_row == b0.letters_row && f.b.letters_col == b0.letters_col);
    CHECK(f.b.row_occ == b0.row_occ && f.b.col_occ == b0.col_occ);
    CHECK(f.b.is_blank == b0.is_blank && f.b.cross_check == b0.cross_check);
    CHECK(f.p.al.by_row == al0.by_row && f.p.al.by_col == al0.by_col);
    CHECK(trayEq(f.p.tray[0], t0) && trayEq(f.p.tray[1], t1));
    CHECK(f.bag.tilesCopy() == bag0 && f.bag.totalCount() == n0);
    CHECK(f.p.score[0] == s0 && f.p.score[1] == s1 && f.p.side == side0 && f.p.finalTurns == ft0);
}

TEST(search_ab_equals_minimax_depth2) {
    Fixture f(11);
    SearchConfig c; c.depth = 2; c.algo = Algo::Minimax; c.orderStatic = false;
    const SearchResult mm = Searcher(f.p, c).run();
    c.algo = Algo::AlphaBeta;
    const SearchResult ab = Searcher(f.p, c).run();
    c.orderStatic = true;
    const SearchResult abo = Searcher(f.p, c).run();
    std::printf("  nodes: minimax=%llu  ab=%llu  ab+order=%llu\n",
                (unsigned long long)mm.nodes, (unsigned long long)ab.nodes, (unsigned long long)abo.nodes);
    CHECK(mm.hasMove && ab.hasMove && abo.hasMove);
    CHECK(mm.value == ab.value && ab.value == abo.value);
    CHECK(ab.nodes <= mm.nodes);
}

TEST(search_restores_state_depth3) { Fixture f(23); checkRestore(f, 3); }

TEST(search_restores_state_tiny_bag) {            // bag drains inside the tree: final-turn path
    Fixture f(5);
    std::array<uint8_t, NUM_LETTER_SLOTS> pool{}; pool[18] = 1; pool[19] = 1;   // S, T
    f.bag.setPool(pool); f.bag.shuffle();
    f.p.tray[0] = Tray{}; f.p.tray[0].addTiles("AEINRST");
    f.p.tray[1] = Tray{}; f.p.tray[1].addTiles("DEGLOUY");
    checkRestore(f, 3);
}