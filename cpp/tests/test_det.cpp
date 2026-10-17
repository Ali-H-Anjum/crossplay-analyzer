#include "check.h"
#include "test_common.h"
#include <chrono>
#include <thread>
#include "det_search.h"

namespace {
struct DetFix {
    Board b{}; Position p{};
    DetFix() {
        const Gaddag& g = testGaddag();
        p.b = &b; p.g = &g; p.init = &g.get_init(); p.bag = nullptr;
        rebuild(b, p.al, g, g.get_init());
        p.tray[0].addTiles("AEINRST");
    }
};
long long msSince(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}
}

TEST(det_unseen_pool_counts) {
    Board b{};
    placeTile(b, 7, 7, letterFromChar('H'), false);
    placeTile(b, 7, 8, letterFromChar('I'), true);          // a blank standing in for I
    Tray own{}; own.addTiles("AEINRST");
    const auto pool = unseenPool(b, own);
    int total = 0; for (uint8_t v : pool) total += v;
    CHECK(total == 100 - 2 - 7);
    CHECK(pool[0] == 8);                                    // A: 9 - 1 in rack
    CHECK(pool[7] == 2);                                    // H: 3 - 1 on board
    CHECK(pool[8] == 7);                                    // I: 8 - 1 in rack (the board I is a blank)
    CHECK(pool[BLANK_SLOT] == 2);                           // blank: 3 - 1 on board
}

TEST(det_rootvals_exact_and_same_value) {
    DetFix f; TileBag bag(11); bag.shuffle(); f.p.bag = &bag;
    bag.drawNextInto(f.p.tray[1], 7);
    SearchConfig c; c.depth = 2; c.algo = Algo::Minimax; c.rootValues = true;
    const SearchResult mm = Searcher(f.p, c).run();
    c.algo = Algo::AlphaBeta;
    const SearchResult ab = Searcher(f.p, c).run();
    c.rootValues = false;
    const SearchResult plain = Searcher(f.p, c).run();
    std::printf("  root moves=%zu  nodes: minimax=%llu ab+rootvals=%llu ab=%llu\n", ab.rootVals.size(),
                (unsigned long long)mm.nodes, (unsigned long long)ab.nodes, (unsigned long long)plain.nodes);
    CHECK(mm.rootVals.size() > 1 && mm.rootVals.size() == ab.rootVals.size());
    CHECK(plain.rootVals.empty() && plain.value == ab.value && mm.value == ab.value);
    int mx = -(1 << 29);
    for (const RootVal& a : ab.rootVals) {
        mx = std::max(mx, a.value);
        bool found = false;
        for (const RootVal& m : mm.rootVals)
            if (sameMove(a.m, m.m)) { found = true; CHECK(a.value == m.value); break; }
        CHECK(found);
    }
    CHECK(mx == ab.value);
}

TEST(det_parallel_reproducible) {
    DetFix f;
    SearchConfig c; c.depth = 2;
    DetConfig d; d.threads = 3; d.seed = 7;
    DetResult a, b;
    { DetSearch s(f.p, c, d); CHECK(s.threads() == 3); a = s.take(); }
    { DetSearch s(f.p, c, d); b = s.take(); }
    std::printf("  3 samples: best value %d, %zu root moves, nodes=%llu\n", a.value, a.ranked.size(),
                (unsigned long long)a.nodes);
    CHECK(a.hasMove && a.samples == 3 && !a.ranked.empty());
    CHECK(a.value == b.value && sameMove(a.best, b.best) && a.ranked.size() == b.ranked.size());
    CHECK(sameMove(a.best, a.ranked[0].m));
    for (size_t i = 1; i < a.ranked.size(); ++i) {
        const DetMove& x = a.ranked[i - 1]; const DetMove& y = a.ranked[i];
        CHECK(x.samples > y.samples || (x.samples == y.samples && x.avg >= y.avg));
    }
    CHECK(f.p.tray[0].size() == 7 && f.p.tray[1].size() == 0);   // root position untouched
}

TEST(det_cancel_is_fast) {
    DetFix f;
    SearchConfig c; c.iterative = true;
    DetConfig d; d.threads = 2;
    const auto t = std::chrono::steady_clock::now();
    DetResult r;
    {
        DetSearch s(f.p, c, d);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(!s.finished());
        s.cancel();
        r = s.take();
    }
    std::printf("  cancelled after %lld ms, default threads here = %d\n", msSince(t), DetSearch::defaultThreads());
    CHECK(r.hasMove && r.samples == 2 && msSince(t) < 2000);
    CHECK(DetSearch::defaultThreads() >= 1);
}

TEST(det_root_cap) {
    DetFix f; TileBag bag(11); bag.shuffle(); f.p.bag = &bag;
    bag.drawNextInto(f.p.tray[1], 7);
    SearchConfig c; c.depth = 2; c.rootValues = true;
    const SearchResult full = Searcher(f.p, c).run();
    c.rootMaxMoves = 50;
    const SearchResult cap = Searcher(f.p, c).run();
    std::printf("  root moves %zu -> %zu, nodes %llu -> %llu\n", full.rootVals.size(), cap.rootVals.size(),
                (unsigned long long)full.nodes, (unsigned long long)cap.nodes);
    CHECK(cap.rootVals.size() > 1 && cap.rootVals.size() <= 52);   // 50 placed + at most 2 swaps
    CHECK(cap.nodes < full.nodes && cap.value <= full.value);
    for (const RootVal& a : cap.rootVals) {
        bool found = false;
        for (const RootVal& b : full.rootVals)
            if (sameMove(a.m, b.m)) { found = true; CHECK(a.value == b.value); break; }
        CHECK(found);
    }
}