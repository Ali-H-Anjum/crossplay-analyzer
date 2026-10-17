#include "check.h"
#include "position.h"
#include "test_common.h"

static bool trayEq(const Tray& a, const Tray& b) {
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
    if (a.count(fromIndex(uint8_t(i))) != b.count(fromIndex(uint8_t(i))))
      return false;
  return true;
}

TEST(position_final_turns_and_unmake) {
  const Gaddag& g = testGaddag();
  const Arc& init = g.get_init();
  Board b{};
  Position p;
  p.b = &b;
  p.g = &g;
  p.init = &init;
  rebuild(b, p.al, g, init);
  TileBag bag(9);
  p.bag = &bag;
  std::array<uint8_t, NUM_LETTER_SLOTS> pool{};
  pool[18] = 1;
  pool[19] = 1;  // S, T
  bag.setPool(pool);
  bag.shuffle();
  p.tray[0].addTiles("AEINRST");
  p.tray[1].addTiles("DEGLOUY");

  MoveList ml;
  generateMoves(g, b, p.al, p.tray[0], ml);
  const RawMove* pick = nullptr;
  for (const RawMove& m : ml.moves)
    if (m.len >= 2) {
      pick = &m;
      break;
    }
  CHECK(pick != nullptr);
  if (!pick)
    return;
  const RawMove m = *pick;

  const Board b0 = b;
  const Tray t0 = p.tray[0], t1 = p.tray[1];
  const auto bag0 = bag.tilesCopy();
  const uint16_t tot0 = bag.totalCount();

  PlyRec r1, r2, r3;
  makePly(p, m, r1);
  CHECK(bag.empty() && p.finalTurns == 0 && !isTerminal(p) && p.side == 1);
  CHECK(p.score[0] == r1.pts && p.score[0] > 0);
  makePass(p, r2);
  CHECK(p.finalTurns == 1 && !isTerminal(p));
  makePass(p, r3);
  CHECK(p.finalTurns == 2 && isTerminal(p));

  unmakePass(p, r3);
  unmakePass(p, r2);
  unmakePly(p, r1);
  CHECK(p.finalTurns == 0 && p.side == 0 && p.score[0] == 0);
  CHECK(trayEq(p.tray[0], t0) && trayEq(p.tray[1], t1));
  CHECK(bag.tilesCopy() == bag0 && bag.totalCount() == tot0);
  CHECK(b.letters_row == b0.letters_row && b.cross_check == b0.cross_check &&
        b.is_blank == b0.is_blank);
}