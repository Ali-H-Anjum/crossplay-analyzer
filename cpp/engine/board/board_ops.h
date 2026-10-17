//BoardOps.h
#pragma once

#include <cassert>
#include "Board.h"
#include "Move.h"      // pulls in Letter.h (toBoardCode)

inline bool inBounds(int row, int col) {
    return row >= 0 && row <= 14 && col >= 0 && col <= 14;
}

// UI-side tile placement. Writes tiles only: anchors and cross-checks live in AnchorLines /
// Board::cross_check and are refreshed by rebuild() (RefreshDerived) or onPlay().
inline void placeTile(Board& b, int row, int col, Letter letter, bool isBlank) {
    assert(inBounds(row, col));
    const int idxR = sq(row, col);
    const int idxC = sq(col, row);
    const uint8_t code = toBoardCode(letter);

    b.letters_row[idxR] = code;
    b.letters_col[idxC] = code;
    b.row_occ[row] |= static_cast<uint16_t>(1u << col);
    b.col_occ[col] |= static_cast<uint16_t>(1u << row);

    if (isBlank) b.is_blank.set(idxR);
}

// Inverse of placeTile.
inline void removeTile(Board& b, int row, int col) {
    assert(inBounds(row, col));
    const int idxR = sq(row, col);
    const int idxC = sq(col, row);

    b.letters_row[idxR] = EMPTY_SQUARE;
    b.letters_col[idxC] = EMPTY_SQUARE;
    b.row_occ[row] &= static_cast<uint16_t>(~(1u << col));
    b.col_occ[col] &= static_cast<uint16_t>(~(1u << row));
    b.is_blank.reset(idxR);
}

