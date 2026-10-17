#include "check.h"
#include "position.h"
#include "test_common.h"

static bool trayEq(const Tray& a, const Tray& b) {
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
    if (a.count(fromIndex(uint8_t(i))) != b.count(fromIndex(uint8_t(i))))
      return false;
  return true;
}
static int allOf(const Position& p, int i) {  // letter i in bag + both trays
  const Letter l = fromIndex(uint8_t(i));
  return p.bag->count(l) + p.tray[0].count(l) + p.tray[1].count(l);
}

TEST(swap_roundtrip_and_conservation) {
  TileBag bag(5);
  bag.shuffle();
  Board b{};
  Position p{};
  p.b = &b;
  p.bag = &bag;
  bag.drawNextInto(p.tray[0], 7);
  bag.drawNextInto(p.tray[1], 7);

  Tray thrown{};
  Letter ret[3];
  int k = 0;
  for (int i = 0; i < NUM_LETTER_SLOTS && k < 3; ++i) {
    const Letter l = fromIndex(uint8_t(i));
    for (uint8_t n = p.tray[0].count(l); n && k < 3; --n) {
      thrown.addTile(l);
      ret[k++] = l;
    }
  }
  CHECK(thrown.size() == 3 && canSwap(p, thrown));

  int tot[NUM_LETTER_SLOTS];
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
    tot[i] = allOf(p, i);
  const Tray t0 = p.tray[0], t1 = p.tray[1];
  const auto bag0 = bag.tilesCopy();
  const uint16_t n0 = bag.totalCount();

  SwapRec r;
  makeSwap(p, thrown, r);
  CHECK(p.side == 1 && p.tray[0].size() == 7 && bag.size() == n0 &&
        trayEq(p.tray[1], t1));
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
    CHECK(allOf(p, i) == tot[i]);
  const auto after = bag.tilesCopy();
  for (int j = 0; j < 3; ++j)
    CHECK(after[j] == ret[j]);  // thrown tiles went to the bottom

  unmakeSwap(p, r);
  CHECK(p.side == 0 && trayEq(p.tray[0], t0) && bag.tilesCopy() == bag0 &&
        bag.totalCount() == n0);
}

TEST(swap_tiny_bag_and_limit) {
  TileBag bag(5);
  Board b{};
  Position p{};
  p.b = &b;
  p.bag = &bag;
  std::array<uint8_t, NUM_LETTER_SLOTS> pool{};
  pool[0] = 2;
  pool[1] = 1;  // A A B
  bag.setPool(pool);
  bag.shuffle();
  p.tray[0].addTiles("QZX");
  Tray thrown{};
  thrown.addTiles("QZX");
  Tray tooMany{};
  tooMany.addTiles("QZXE");
  CHECK(canSwap(p, thrown) && !canSwap(p, tooMany));

  const auto bag0 = bag.tilesCopy();
  SwapRec r;
  makeSwap(p, thrown, r);
  CHECK(bag.size() == 3 && p.tray[0].count(fromIndex(0)) == 2 &&
        p.tray[0].count(fromIndex(1)) == 1);
  CHECK(bag.count(fromIndex(16)) == 1 && bag.count(fromIndex(25)) == 1 &&
        bag.count(fromIndex(23)) == 1);
  unmakeSwap(p, r);
  CHECK(bag.tilesCopy() == bag0 && p.tray[0].size() == 3 &&
        p.tray[0].count(fromIndex(16)) == 1);
}