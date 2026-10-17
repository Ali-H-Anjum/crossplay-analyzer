#include "check.h"
#include "search.h"
#include "test_common.h"

namespace {
struct TTFix {
  Board b{};
  TileBag bag;
  Position p;
  explicit TTFix(uint64_t seed) : bag(seed) {
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
void sameValue(TTFix& f, int depth, bool os, bool kil, bool his) {
  SearchConfig c;
  c.depth = depth;
  c.orderStatic = os;
  c.killers = kil;
  c.history = his;
  const SearchResult a = Searcher(f.p, c).run();
  c.useTT = true;
  const SearchResult b = Searcher(f.p, c).run();
  std::printf(
      "  depth %d: nodes plain=%llu  tt=%llu  (tt hits %llu)  value %d / %d\n",
      depth, (unsigned long long)a.nodes, (unsigned long long)b.nodes,
      (unsigned long long)b.ttHits, a.value, b.value);
  CHECK(a.hasMove && b.hasMove && a.value == b.value);
}
}  // namespace

TEST(zobrist_make_unmake_and_sensitivity) {
  TTFix f(11);
  const uint64_t h0 = hashPosition(f.p, 0, 0);
  MoveList ml;
  generateMoves(*f.p.g, f.b, f.p.al, f.p.tray[0], ml);
  CHECK(!ml.moves.empty());
  PlyRec r;
  makePly(f.p, ml.moves[0], r);
  CHECK(hashPosition(f.p, 0, 0) != h0);
  unmakePly(f.p, r);
  CHECK(hashPosition(f.p, 0, 0) == h0);

  CHECK(hashPosition(f.p, 0, 12345) != h0);  // swap-history term
  f.p.side = 1;
  CHECK(hashPosition(f.p, 0, 0) != h0);
  f.p.side = 0;
  f.p.score[0] = 7;
  CHECK(hashPosition(f.p, 0, 0) != h0);
  f.p.score[0] = 0;
  f.p.finalTurns = 1;
  CHECK(hashPosition(f.p, 0, 0) != h0);
  f.p.finalTurns = 0;
  const Tray saved = f.p.tray[1];
  f.p.tray[1] = Tray{};
  CHECK(hashPosition(f.p, 0, 0) != h0);
  f.p.tray[1] = saved;
  CHECK(hashPosition(f.p, 0, 0) == h0);
}

TEST(tt_same_value_depth3) {
  TTFix f(11);
  sameValue(f, 3, true, false, false);
  TTFix g(23);
  sameValue(g, 3, true, true, false);
}

TEST(tt_same_value_no_static_order_depth2) {
  TTFix f(5);
  sameValue(f, 2, false, true, true);
}

TEST(tt_same_value_tiny_bag) {  // bag drains and swaps are possible inside the
                                // tree
  TTFix f(5);
  std::array<uint8_t, NUM_LETTER_SLOTS> pool{};
  pool[18] = 1;
  pool[19] = 1;  // S, T
  f.bag.setPool(pool);
  f.bag.shuffle();
  f.p.tray[0] = Tray{};
  f.p.tray[0].addTiles("AEINRST");
  f.p.tray[1] = Tray{};
  f.p.tray[1].addTiles("DEGLOUY");
  sameValue(f, 3, true, false, false);
}

TEST(tt_iterative_matches_fixed) {
  TTFix f(11);
  SearchConfig c;
  c.depth = 3;
  const SearchResult fx = Searcher(f.p, c).run();
  SearchConfig ci;
  ci.iterative = true;
  ci.maxDepth = 3;
  ci.useTT = true;
  ci.killers = true;
  SearchProgress pr;
  const SearchResult it = Searcher(f.p, ci, &pr).run();
  std::printf("  fixed nodes=%llu  iterative+tt nodes=%llu\n",
              (unsigned long long)fx.nodes, (unsigned long long)it.nodes);
  CHECK(it.hasMove && it.value == fx.value && pr.depth.load() == 3);
}

TEST(tt_solved_endgame_stops_early) {  // bag empty, small racks: game ends at
                                       // ply 2
  TTFix f(1);
  std::array<uint8_t, NUM_LETTER_SLOTS> none{};
  f.bag.setPool(none);
  f.p.tray[0] = Tray{};
  f.p.tray[0].addTiles("CAT");
  f.p.tray[1] = Tray{};
  f.p.tray[1].addTiles("DOG");
  SearchConfig c;
  c.iterative = true;
  SearchProgress p0;
  const SearchResult a = Searcher(f.p, c, &p0).run();
  c.useTT = true;
  SearchProgress p1;
  const SearchResult b = Searcher(f.p, c, &p1).run();
  CHECK(a.hasMove && b.hasMove && a.value == b.value);
  CHECK(p0.depth.load() == 2 && p1.depth.load() == 2);
}