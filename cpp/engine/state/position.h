#pragma once
#include "apply_move.h"
#include "score_fast.h"
#include "tile_bag.h"

struct Position {
  Board* b;
  AnchorLines al;
  const Gaddag* g;
  const Arc* init;
  TileBag* bag;
  Tray tray[2];
  int score[2] = {0, 0};
  int side = 0;
  int finalTurns = 0;
};

struct PlyRec {
  UndoRec u;
  Tray savedTray;
  int pts = 0;
  uint8_t drawn = 0;
  bool bagWasEmpty = false;
};

inline bool isTerminal(const Position& p) {
  return p.finalTurns >= 2;
}

inline void makePly(Position& p, const RawMove& m, PlyRec& r) {
  r.bagWasEmpty = p.bag->empty();
  r.savedTray = p.tray[p.side];
  r.pts = scoreRaw(*p.b, m);  // pre-move board
  applyMove(*p.b, p.al, *p.g, *p.init, m, r.u);
  Tray& t = p.tray[p.side];
  for (int i = 0; i < r.u.nTiles; ++i) {
    const int idx = sq(r.u.pr[i], r.u.pc[i]);
    t.removeTile(p.b->is_blank.test(idx)
                     ? Letter::Blank
                     : fromBoardCode(p.b->letters_row[idx]));
  }
  r.drawn = p.bag->drawNextInto(t, r.u.nTiles);
  p.score[p.side] += r.pts;
  if (r.bagWasEmpty)
    ++p.finalTurns;
  p.side ^= 1;
}
inline void unmakePly(Position& p, const PlyRec& r) {
  p.side ^= 1;
  if (r.bagWasEmpty)
    --p.finalTurns;
  Tray& t = p.tray[p.side];
  p.bag->undoDraw(t, r.drawn);
  t = r.savedTray;
  p.score[p.side] -= r.pts;
  undoMove(*p.b, p.al, r.u);
}

inline void makePass(Position& p, PlyRec& r) {
  r.bagWasEmpty = p.bag->empty();
  if (r.bagWasEmpty)
    ++p.finalTurns;
  p.side ^= 1;
}
inline void unmakePass(Position& p, const PlyRec& r) {
  p.side ^= 1;
  if (r.bagWasEmpty)
    --p.finalTurns;
}

struct SwapRec {
  Tray savedTray;
  SwapUndo bu;
};

// Swap needs 1..7 tiles and at least that many left in the bag.
inline bool canSwap(const Position& p, const Tray& thrown) {
  return thrown.size() >= 1 && thrown.size() <= 7 &&
         p.bag->size() >= thrown.size();
}
inline void makeSwap(Position& p, const Tray& thrown, SwapRec& r) {
  Tray& t = p.tray[p.side];
  r.savedTray = t;
  Letter ret[7];
  uint8_t k = 0;
  for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
    const Letter l = fromIndex(static_cast<uint8_t>(i));
    for (uint8_t n = thrown.count(l); n; --n) {
      assert(k < 7);
      ret[k++] = l;
      t.removeTile(l);
    }
  }
  p.bag->swapIn(t, ret, k, r.bu);
  p.side ^= 1;  // swap uses the whole turn
}
inline void unmakeSwap(Position& p, const SwapRec& r) {
  p.side ^= 1;
  Tray& t = p.tray[p.side];
  p.bag->undoSwap(t, r.bu);
  t = r.savedTray;
}