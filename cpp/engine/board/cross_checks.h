// crosschecks.h
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

#include "board.h"
#include "gaddag.h"  // needed: step/next_arc/arena access
#include "node.h"

constexpr uint32_t LINE_BITS = (1u << BOARD_LENGTH) - 1;

struct AnchorLines {  // by_col is the transpose of by_row
    std::array<uint16_t, BOARD_LENGTH> by_row{};
    std::array<uint16_t, BOARD_LENGTH> by_col{};
};

struct Placed {
    int r, c;
};

inline bool boardIsEmpty(const Board& b) {
    uint16_t any = 0;
    for (uint16_t o : b.row_occ) any |= o;
    return any == 0;
}

// Legal-letter mask for empty square g of a line (letters ln, occupancy occ). Needs >=1 adjacent
// tile.
inline uint32_t crossMask(const uint8_t* ln, uint32_t occ, int g, const Gaddag& gd,
                          const Arc& init) {
    const int nb = std::countl_one(static_cast<uint16_t>(occ << (16 - g)));  // tiles before g
    const int na = std::countr_one(occ >> (g + 1));                          // tiles after g
    const int lo = g - nb, hi = g + na;

    auto walk = [&](const Arc* a, int from, int to, int d) {  // tiles from..to (exclusive of to)
        for (int k = from; a && k != to; k += d) a = gd.step(*a, boardCodeToIndex(ln[k]));
        return a;
    };
    auto setOf = [](const Arc* a) { return a ? a->letter_set_mask & ALL_LETTERS : 0u; };

    if (!nb) return setOf(walk(&init, hi, g, -1));  // suffix only

    const Arc* a = walk(&init, g - 1, lo - 1, -1);  // rev(prefix)
    a = a ? gd.step(*a, SEPARATOR_INDEX) : nullptr;
    if (!a || !na) return setOf(a);  // prefix only

    const Node& n = gd.node(a->destination_state);  // both sides
    const int last = boardCodeToIndex(ln[hi]);
    const Arc* base = gd.arc_span().data() + n.arc_offset;  // packed, ascending; counter == rank
    uint32_t res = 0;
    for (uint32_t m = n.arc_mask & ALL_LETTERS; m; m &= m - 1, ++base) {
        const int x = std::countr_zero(m);
        const Arc* b = walk(base, g + 1, hi, 1);
        if (b && ((b->letter_set_mask >> last) & 1u)) res |= 1u << x;
    }
    return res;
}

// Tile now sits at (line, pos). Recompute the two empty squares that end its run along the
// moving axis. perpOcc/perpLetters are indexed by `pos`; out is written at sq(t, pos).
//   column refresh: line = row,    pos = column, out = cross_check[0]
//   row refresh:    line = column, pos = row,    out = cross_check[1]
inline void refreshRunEnds(const uint16_t* perpOcc, const uint8_t* perpLetters, uint32_t* out,
                           const Gaddag& gd, const Arc& init, int line, int pos) {
    const uint32_t occ = perpOcc[pos];
    const uint8_t* ln = &perpLetters[sq(pos, 0)];
    const uint32_t below = ~occ & ((1u << line) - 1);
    const uint32_t above = ~occ & ~((2u << line) - 1) & LINE_BITS;
    if (below) {
        const int t = 31 - std::countl_zero(below);
        out[sq(t, pos)] = crossMask(ln, occ, t, gd, init);
    }
    if (above) {
        const int t = std::countr_zero(above);
        out[sq(t, pos)] = crossMask(ln, occ, t, gd, init);
    }
}

inline void markAnchor(const Board& b, AnchorLines& al, int r, int c) {
    if (r < 0 || c < 0 || r >= BOARD_LENGTH || c >= BOARD_LENGTH) return;
    if ((b.row_occ[r] >> c) & 1u) return;
    al.by_row[r] |= static_cast<uint16_t>(1u << c);
    al.by_col[c] |= static_cast<uint16_t>(1u << r);
}

// colWord: refresh squares whose column word changed; rowWord: row word.
// Slot convention (matches MoveGen.h):
//   cross_check[0] = column constraint on this square (used by HORIZONTAL moves, which
//                    require each new tile's perpendicular column word to be valid).
//   cross_check[1] = row constraint on this square (used by VERTICAL moves).
inline void touchTile(Board& b, AnchorLines& al, const Gaddag& gd, const Arc& init, int r, int c,
                      bool colWord, bool rowWord) {
    al.by_row[r] &= static_cast<uint16_t>(~(1u << c));
    al.by_col[c] &= static_cast<uint16_t>(~(1u << r));
    markAnchor(b, al, r, c - 1);
    markAnchor(b, al, r, c + 1);
    markAnchor(b, al, r - 1, c);
    markAnchor(b, al, r + 1, c);
    if (colWord)
        refreshRunEnds(b.col_occ.data(), b.letters_col.data(), b.cross_check[0].data(), gd, init, r,
                       c);
    if (rowWord)
        refreshRunEnds(b.row_occ.data(), b.letters_row.data(), b.cross_check[1].data(), gd, init, c,
                       r);
}

// Call after occ/letters already contain the new tiles. Cost: ~2 walks per tile, not ~30 lines.
inline void onPlay(Board& b, AnchorLines& al, const Gaddag& gd, const Arc& init, const Placed* t,
                   int n, bool horizontal) {
    for (int i = 0; i < n; ++i)
        touchTile(b, al, gd, init, t[i].r, t[i].c, horizontal || i == 0, !horizontal || i == 0);
}

// Reset / load an arbitrary position.
inline void rebuild(Board& b, AnchorLines& al, const Gaddag& gd, const Arc& init) {
    std::fill(b.cross_check[0].begin(), b.cross_check[0].end(), ALL_LETTERS);
    std::fill(b.cross_check[1].begin(), b.cross_check[1].end(), ALL_LETTERS);
    al = AnchorLines{};
    if (boardIsEmpty(b)) {
        constexpr int C = BOARD_LENGTH / 2;
        al.by_row[C] = al.by_col[C] = static_cast<uint16_t>(1u << C);
        return;
    }
    for (int r = 0; r < BOARD_LENGTH; ++r)
        for (uint32_t m = b.row_occ[r]; m; m &= m - 1)
            touchTile(b, al, gd, init, r, std::countr_zero(m), true, true);
}
