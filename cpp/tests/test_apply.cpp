#include "check.h"
#include "test_common.h"
#include "apply_move.h"

static bool boardEq(const Board& a, const Board& b) {
    return a.letters_row == b.letters_row && a.letters_col == b.letters_col &&
           a.row_occ == b.row_occ && a.col_occ == b.col_occ &&
           a.is_blank == b.is_blank && a.cross_check == b.cross_check;
}
static bool anchorsEq(const AnchorLines& a, const AnchorLines& b) {
    return a.by_row == b.by_row && a.by_col == b.by_col;
}
static const RawMove* pickMove(const Board& b, const MoveList& s, int salt, bool wantBlank) {
    for (size_t k = 0; k < s.moves.size(); ++k) {
        const RawMove& m = s.moves[(salt * 37 + k) % s.moves.size()];
        if (wantBlank && !m.blank_mask) continue;
        for (int i = 0; i < m.len; ++i) {
            const int r = m.row + (m.is_descending ? i : 0), c = m.col + (m.is_descending ? 0 : i);
            if (b.letters_row[sq(r, c)] == EMPTY_SQUARE) return &m;
        }
    }
    return nullptr;
}

TEST(apply_undo_roundtrip) {
    const Gaddag& g = testGaddag();
    const Arc& init = g.get_init();
    Board b{}; AnchorLines al; rebuild(b, al, g, init);
    const char* trays[] = {"AEINRST", "DEGLOUY", "AEINRS", "AEIOULN", "RSTLNEA",
                           "AEINRS", "MINDFUL", "CAKESIT", "AEINRS", "PLAYERS"};
    for (int ply = 0; ply < 10; ++ply) {
        const bool blankPly = (ply % 3 == 2);
        Tray tray{}; tray.addTiles(trays[ply]);
        if (blankPly) tray.addTile(Letter::Blank);
        MoveList sink; generateMoves(g, b, al, tray, sink);
        const RawMove* pick = blankPly ? pickMove(b, sink, ply, true) : nullptr;
        if (!pick) pick = pickMove(b, sink, ply, false);
        CHECK(pick != nullptr);
        if (!pick) break;
        const RawMove m = *pick;

        const Board before = b; const AnchorLines alBefore = al;
        UndoRec u; applyMove(b, al, g, init, m, u);

        // 1. apply matches a full rebuild (empty squares only for cross-checks)
        Board ref = b; AnchorLines refAl; rebuild(ref, refAl, g, init);
        CHECK(anchorsEq(al, refAl));
        for (int r = 0; r < BOARD_LENGTH; ++r)
            for (int c = 0; c < BOARD_LENGTH; ++c) {
                if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) continue;
                CHECK(b.cross_check[0][sq(r, c)] == ref.cross_check[0][sq(r, c)]);
                CHECK(b.cross_check[1][sq(c, r)] == ref.cross_check[1][sq(c, r)]);
            }

        // 2. two-deep apply, then undo both, must be bit-exact
        {
            const Board mid = b; const AnchorLines midAl = al;
            Tray t2{}; t2.addTiles("AEINRST");
            MoveList s2; generateMoves(g, b, al, t2, s2);
            if (const RawMove* p2 = pickMove(b, s2, ply + 1, false)) {
                const RawMove m2 = *p2;
                UndoRec u2; applyMove(b, al, g, init, m2, u2);
                undoMove(b, al, u2);
                CHECK(boardEq(b, mid)); CHECK(anchorsEq(al, midAl));
            }
        }
        undoMove(b, al, u);
        CHECK(boardEq(b, before)); CHECK(anchorsEq(al, alBefore));

        applyMove(b, al, g, init, m, u);           // advance the game
    }
    CHECK(b.is_blank.any());                        // blank path was exercised
}