#include "check.h"
#include "test_common.h"
#include <chrono>
#include "Search.h"

namespace {
struct PrFix {
    Board b{}; TileBag bag; Position p;
    explicit PrFix(uint64_t seed) : bag(seed) {
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
Snap snap(const PrFix& f) {
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
long long msSince(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}
}

TEST(prune_neutral_settings_same_value) {            // "never prune" settings must equal plain alpha-beta
    PrFix f(11);
    SearchConfig c; c.depth = 3;
    const SearchResult base = Searcher(f.p, c).run();

    SearchConfig b = c; b.useBeam = true; b.beam.fill(100000);
    const SearchResult rb = Searcher(f.p, b).run();

    SearchConfig l = c; l.lmr = true; l.lmrAfter = 100000;
    const SearchResult rl = Searcher(f.p, l).run();

    SearchConfig pc = c; pc.probCut = true; pc.pcMinDepth = 2; pc.pcMargin = 100000;
    const SearchResult rp = Searcher(f.p, pc).run();

    std::printf("  nodes: base=%llu beam(huge)=%llu lmr(never)=%llu probcut(huge margin)=%llu\n",
                (unsigned long long)base.nodes, (unsigned long long)rb.nodes,
                (unsigned long long)rl.nodes, (unsigned long long)rp.nodes);
    CHECK(base.hasMove && rb.hasMove && rl.hasMove && rp.hasMove);
    CHECK(base.value == rb.value && base.value == rl.value && base.value == rp.value);
}

TEST(prune_beam_and_lmr_reduce_nodes_depth3) {
    PrFix f(11);
    SearchConfig c; c.depth = 3;
    const SearchResult base = Searcher(f.p, c).run();
    SearchConfig b = c; b.useBeam = true;
    const SearchResult rb = Searcher(f.p, b).run();
    SearchConfig l = c; l.lmr = true; l.lmrAfter = 3;
    const SearchResult rl = Searcher(f.p, l).run();
    std::printf("  nodes: base=%llu (value %d)  beam=%llu (value %d)  lmr=%llu (value %d)\n",
                (unsigned long long)base.nodes, base.value, (unsigned long long)rb.nodes, rb.value,
                (unsigned long long)rl.nodes, rl.value);
    CHECK(rb.hasMove && rl.hasMove);
    CHECK(rb.nodes < base.nodes);
    CHECK(rl.nodes < base.nodes);
}

TEST(prune_all_on_depth4_restores_state) {
    PrFix f(23);
    const Snap s0 = snap(f);
    SearchConfig c; c.depth = 4; c.killers = true; c.history = true;
    c.useBeam = true; c.lmr = true; c.probCut = true; c.pcMinDepth = 3; c.useTT = true;
    const auto t = std::chrono::steady_clock::now();
    const SearchResult r = Searcher(f.p, c).run();
    std::printf("  depth 4 all pruning: nodes=%llu  %lld ms  value=%d\n",
                (unsigned long long)r.nodes, msSince(t), r.value);
    CHECK(r.hasMove);
    CHECK(same(snap(f), s0));
}

TEST(prune_iterative_restores_after_abort) {
    PrFix f(5);
    const Snap s0 = snap(f);
    SearchConfig c; c.iterative = true; c.timeLimitMs = 400;
    c.useBeam = true; c.lmr = true; c.probCut = true; c.killers = true;
    SearchProgress pr;
    const SearchResult r = Searcher(f.p, c, &pr).run();
    std::printf("  reached depth %d, nodes=%llu\n", pr.depth.load(), (unsigned long long)r.nodes);
    CHECK(r.hasMove && pr.done.load() && pr.depth.load() >= 2);
    CHECK(same(snap(f), s0));
}