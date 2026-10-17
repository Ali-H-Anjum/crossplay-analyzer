#include "check.h"
#include "swap_gen.h"

TEST(swapgen_candidates) {
  TileBag bag(3);
  Board b{};
  Position p{};
  p.b = &b;
  p.bag = &bag;
  p.tray[0].addTiles("QSEAIRT");
  Tray out[2];
  int n = swapCandidates(p, out);
  CHECK(n == 2 && out[0].size() == 7 && out[1].size() == 3);
  CHECK(out[1].count(fromIndex(16)) == 1 && out[1].count(fromIndex(8)) == 1 &&
        out[1].count(fromIndex(19)) == 1);  // Q I T

  p.tray[0] = Tray{};
  p.tray[0].addTiles("SERANHX");  // every tile has positive leave
  CHECK(swapCandidates(p, out) == 1 && out[0].size() == 7);

  std::array<uint8_t, NUM_LETTER_SLOTS> pool{};
  pool[0] = 2;  // bag holds 2 tiles
  bag.setPool(pool);
  p.tray[0] = Tray{};
  p.tray[0].addTiles("QSEAIRT");
  CHECK(swapCandidates(p, out) == 0);  // both sets are bigger than the bag
}