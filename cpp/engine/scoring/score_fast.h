#pragma once
#include "move_eval.h"

// Scores a RawMove against the PRE-move board. No heap use.
inline MoveScore scoreRawFull(const Board& b, const RawMove& m) {
    const int dr = m.is_descending ? 1 : 0, dc = 1 - dr;
    const int pr = dc, pc = dr;  // perpendicular step
    int mainPts = 0, mainMul = 1, cross = 0, used = 0;
    int r = m.row, c = m.col;
    for (int i = 0; i < m.len; ++i, r += dr, c += dc) {
        const int idx = sq(r, c);
        const int val = POINTS_PER_TILE[m.letters[i] - 1];
        const bool blank = ((m.blank_mask >> i) & 1) || b.is_blank.test(idx);
        if (b.letters_row[idx] != EMPTY_SQUARE) {
            if (!blank) mainPts += val;
            continue;
        }
        ++used;
        const Mult mult = kPremium[idx];
        const int lv = blank ? 0 : val * (mult == MULT_DL ? 2 : mult == MULT_TL ? 3 : 1);
        const int wm = mult == MULT_DW ? 2 : mult == MULT_TW ? 3 : 1;
        mainPts += lv;
        mainMul *= wm;

        int side = 0;
        bool any = false;
        for (int s = -1; s <= 1; s += 2)
            for (int rr = r + s * pr, cc = c + s * pc;
                 rr >= 0 && cc >= 0 && rr < BOARD_LENGTH && cc < BOARD_LENGTH;
                 rr += s * pr, cc += s * pc) {
                const int j = sq(rr, cc);
                if (b.letters_row[j] == EMPTY_SQUARE) break;
                any = true;
                if (!b.is_blank.test(j)) side += POINTS_PER_TILE[b.letters_row[j] - 1];
            }
        if (any) cross += (side + lv) * wm;
    }
    MoveScore s;
    s.tilesUsed = used;
    s.bingo = (used == 7);
    s.points = mainPts * mainMul + cross + (s.bingo ? 40 : 0);
    return s;
}
inline int scoreRaw(const Board& b, const RawMove& m) { return scoreRawFull(b, m).points; }