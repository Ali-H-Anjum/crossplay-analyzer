#include "check.h"
#include "test_common.h"
#include <chrono>
#include "search.h"

namespace {
struct ItFix {
    Board b{}; TileBag bag; Position p;
    explicit ItFix(uint64_t seed) : bag(seed) {
        const Gaddag& g = testGaddag();
        p.b = &b; p.g = &g; p.init = &g.get_init(); p.bag = &bag;
        rebuild(b, p.al, g, g.get_init());
        bag.shuffle();
        bag.drawNextInto(p.tray[0], 7); bag.drawNextInto(p.tray[1], 7);
    }
};
struct Snap {
    Board b; AnchorLines al; std::array<uint8_t, 2 * NUM_LETTER_SLOTS> tr{};
    std::array<Letter, TOTAL_TILES> bag; uint16_t n; int s0, s1, side, ft;
};
Snap snap(const ItFix& f) {
    Snap s{f.b, f.p.al, {}, f.bag.tilesCopy(), f.bag.totalCount(), f.p.score[0], f.p.score[1], f.p.side, f.p.finalTurns};
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        s.tr[i] = f.p.tray[0].count(fromIndex(uint8_t(i)));
        s.tr[NUM_LETTER_SLOTS + i] = f.p.tray[1].count(fromIndex(uint8_t(i)));
    }
    return s;
}
bool same(const Snap& a, const Snap& b) {
    return a.b.letters_row == b.b.letters_row && a.b.letters_col == b.b.letters_col &&
           a.b.row_occ == b.b.row_occ && a.b.col_occ == b.b.col_occ &&
           a.b.is_blank == b.b.is_blank && a.b.cross_check == b.b.cross_check &&
           a.al.by_row == b.al.by_row && a.al.by_col == b.al.by_col && a.tr == b.tr &&
           a.bag == b.bag && a.n == b.n && a.s0 == b.s0 && a.s1 == b.s1 && a.side == b.side && a.ft == b.ft;
}
}

TEST(iter_matches_fixed_value) {
    ItFix f(11);
    SearchConfig c; c.depth = 2;
    const SearchResult fx = Searcher(f.p, c).run();
    SearchConfig ci; ci.iterative = true; ci.maxDepth = 2;
    SearchProgress pr;
    const SearchResult it = Searcher(f.p, ci, &pr).run();
    CHECK(it.hasMove && fx.value == it.value);
    CHECK(pr.depth.load() == 2 && pr.done.load() && pr.hasBest);
}

TEST(iter_time_limit_stops_and_restores) {
    ItFix f(23);
    const Snap s0 = snap(f);
    SearchConfig c; c.iterative = true; c.timeLimitMs = 300;
    SearchProgress pr;
    const auto t = std::chrono::steady_clock::now();
    const SearchResult r = Searcher(f.p, c, &pr).run();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
    std::printf("  reached depth %d, nodes=%llu, %lld ms\n", pr.depth.load(), (unsigned long long)r.nodes, (long long)ms);
    CHECK(r.hasMove && pr.done.load());
    CHECK(pr.depth.load() >= 2 && pr.depth.load() < 64);
    CHECK(ms < 2000);
    CHECK(same(snap(f), s0));                        // aborted search unwound cleanly
}

TEST(iter_stop_flag) {
    ItFix f(23);
    const Snap s0 = snap(f);
    SearchConfig c; c.iterative = true;
    SearchProgress pr; pr.stop = true;               // already cancelled
    const SearchResult r = Searcher(f.p, c, &pr).run();
    CHECK(r.hasMove);                                // depth 1 always completes
    CHECK(pr.depth.load() >= 1 && pr.depth.load() <= 3);
    CHECK(same(snap(f), s0));
}

TEST(iter_stops_when_solved) {                       // unplayable racks, bag empty, one final turn left
    ItFix f(1);
    std::array<uint8_t, NUM_LETTER_SLOTS> none{}; f.bag.setPool(none);
    f.p.tray[0] = Tray{}; f.p.tray[0].addTiles("VVVVVVV");
    f.p.tray[1] = Tray{}; f.p.tray[1].addTiles("VVVVVVV");
    f.p.finalTurns = 1;
    SearchConfig c; c.iterative = true;
    SearchProgress pr;
    const SearchResult r = Searcher(f.p, c, &pr).run();
    CHECK(r.hasMove && r.best.kind == MK_PASS);
    CHECK(pr.depth.load() == 1);                     // stopped at once, not at depth 64
}