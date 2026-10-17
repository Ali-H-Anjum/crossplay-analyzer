#include "check.h"
#include "search.h"
#include "test_common.h"

namespace {
struct OrdFix {
    Board b{};
    TileBag bag;
    Position p;
    explicit OrdFix(uint64_t seed) : bag(seed) {
        const Gaddag& g = testGaddag();
        p.b = &b;
        p.g = &g;
        p.init = &g.get_init();
        p.bag = &bag;
        rebuild(b, p.al, g, g.get_init());
        bag.shuffle();
        bag.drawNextInto(p.tray[0], 7);
        bag.drawNextInto(p.tray[1], 7);
    }
};
}  // namespace

TEST(order_variants_same_value_depth3) {
    OrdFix f(11);
    SearchConfig c;
    c.depth = 3;
    const SearchResult base = Searcher(f.p, c).run();
    c.killers = true;
    const SearchResult k = Searcher(f.p, c).run();
    c.killers = false;
    c.history = true;
    const SearchResult h = Searcher(f.p, c).run();
    c.killers = true;
    const SearchResult kh = Searcher(f.p, c).run();
    std::printf("  nodes: base=%llu killers=%llu history=%llu both=%llu\n",
                (unsigned long long)base.nodes, (unsigned long long)k.nodes,
                (unsigned long long)h.nodes, (unsigned long long)kh.nodes);
    CHECK(base.hasMove && k.hasMove && h.hasMove && kh.hasMove);
    CHECK(base.value == k.value && base.value == h.value && base.value == kh.value);
}

TEST(order_without_static_same_value_depth2) {
    OrdFix f(23);
    SearchConfig c;
    c.depth = 2;
    c.orderStatic = false;
    const SearchResult plain = Searcher(f.p, c).run();
    c.killers = true;
    c.history = true;
    const SearchResult both = Searcher(f.p, c).run();
    std::printf("  nodes: plain=%llu killers+history=%llu\n", (unsigned long long)plain.nodes,
                (unsigned long long)both.nodes);
    CHECK(plain.value == both.value);
}