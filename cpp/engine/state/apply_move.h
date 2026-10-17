#pragma once
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>

#include "board.h"
#include "board_ops.h"
#include "cross_checks.h"
#include "move_gen.h"

constexpr int MAX_NEW_TILES = 7;

struct SavedCC {
  uint32_t old;
  uint16_t idx;
  uint8_t orient;
};

struct UndoRec {  // ~200 bytes, stack only
  AnchorLines al;
  std::array<SavedCC, 2 * (MAX_NEW_TILES + 1)> cc;
  std::array<uint8_t, MAX_NEW_TILES> pr, pc;
  uint8_t nTiles = 0, nCC = 0;
};

// Mirrors the squares refreshRunEnds will write (same occ, same run-end math).
inline void saveRunEnds(UndoRec& u, uint8_t orient, const uint16_t* perpOcc,
                        const uint32_t* cc, int line, int pos) {
  const uint32_t occ = perpOcc[pos];
  const uint32_t below = ~occ & ((1u << line) - 1);
  const uint32_t above = ~occ & ~((2u << line) - 1) & LINE_BITS;
  if (below) {
    const int i = sq(31 - std::countl_zero(below), pos);
    SavedCC& s = u.cc[u.nCC++];
    s.old = cc[i];
    s.idx = static_cast<uint16_t>(i);
    s.orient = orient;
  }
  if (above) {
    const int i = sq(std::countr_zero(above), pos);
    SavedCC& s = u.cc[u.nCC++];
    s.old = cc[i];
    s.idx = static_cast<uint16_t>(i);
    s.orient = orient;
  }
}

// Board + anchors only. Tray, bag and score belong to the search layer.
// Call scoreRaw BEFORE this (it needs the pre-move board).
inline void applyMove(Board& b, AnchorLines& al, const Gaddag& g,
                      const Arc& init, const RawMove& m, UndoRec& u) {
  u.al = al;
  u.nTiles = 0;
  u.nCC = 0;
  const int dr = m.is_descending ? 1 : 0, dc = 1 - dr;
  Placed placed[MAX_NEW_TILES];
  int r = m.row, c = m.col;
  for (int i = 0; i < m.len; ++i, r += dr, c += dc) {
    const int idx = sq(r, c);
    if (b.letters_row[idx] != EMPTY_SQUARE)
      continue;
    assert(u.nTiles < MAX_NEW_TILES);
    b.letters_row[idx] = m.letters[i];
    b.letters_col[sq(c, r)] = m.letters[i];
    b.row_occ[r] |= static_cast<uint16_t>(1u << c);
    b.col_occ[c] |= static_cast<uint16_t>(1u << r);
    if ((m.blank_mask >> i) & 1)
      b.is_blank.set(idx);
    placed[u.nTiles].r = r;
    placed[u.nTiles].c = c;
    u.pr[u.nTiles] = static_cast<uint8_t>(r);
    u.pc[u.nTiles] = static_cast<uint8_t>(c);
    ++u.nTiles;
  }
  const bool horizontal = !m.is_descending;
  for (int i = 0; i < u.nTiles; ++i) {  // same flags as onPlay
    if (horizontal || i == 0)
      saveRunEnds(u, 0, b.col_occ.data(), b.cross_check[0].data(), placed[i].r,
                  placed[i].c);
    if (!horizontal || i == 0)
      saveRunEnds(u, 1, b.row_occ.data(), b.cross_check[1].data(), placed[i].c,
                  placed[i].r);
  }
  onPlay(b, al, g, init, placed, u.nTiles, horizontal);
}

inline void undoMove(Board& b, AnchorLines& al, const UndoRec& u) {
  for (int k = u.nCC; k-- > 0;)
    b.cross_check[u.cc[k].orient][u.cc[k].idx] = u.cc[k].old;
  for (int i = 0; i < u.nTiles; ++i) {
    const int r = u.pr[i], c = u.pc[i];
    b.letters_row[sq(r, c)] = EMPTY_SQUARE;
    b.letters_col[sq(c, r)] = EMPTY_SQUARE;
    b.row_occ[r] &= static_cast<uint16_t>(~(1u << c));
    b.col_occ[c] &= static_cast<uint16_t>(~(1u << r));
    b.is_blank.reset(sq(r, c));
  }
  al = u.al;
}