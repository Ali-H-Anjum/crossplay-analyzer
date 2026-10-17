#include "check.h"
#include "test_common.h"
#include "Board.h"
#include "BoardOps.h"
#include "CrossChecks.h"
#include "MoveGen.h"
#include "Tray.h"
#include "Letter.h"
#include <vector>

TEST(onplay_matches_rebuild) {
    const Gaddag& g = testGaddag();
    const Arc& init = g.get_init();
    Board b{};
    AnchorLines al;
    rebuild(b, al, g, init);

    const char* trays[] = {"AEINRST", "DEGLOUY", "BCMPHKO", "AEIOULN", "RSTLNEA",
                           "TOWERSD", "MINDFUL", "CAKESIT", "HOUSING", "PLAYERS"};
    int horiz = 0, vert = 0;
    for (int ply = 0; ply < 10; ++ply) {
        Tray tray{};
        tray.addTiles(trays[ply]);
        MoveList sink;
        generateMoves(g, b, al, tray, sink);
        CHECK(!sink.moves.empty());
        if (sink.moves.empty()) break;

        // pick a move that places at least one new tile, starting at a varying offset
        const RawMove* pick = nullptr;
        for (size_t k = 0; k < sink.moves.size() && !pick; ++k) {
            const RawMove& m = sink.moves[(ply * 37 + k) % sink.moves.size()];
            for (int i = 0; i < m.len; ++i) {
                int r = m.row + (m.is_descending ? i : 0), c = m.col + (m.is_descending ? 0 : i);
                if (b.letters_row[sq(r, c)] == EMPTY_SQUARE) { pick = &m; break; }
            }
        }
        CHECK(pick != nullptr);
        if (!pick) break;
        const RawMove& m = *pick;
        (m.is_descending ? vert : horiz)++;

        std::vector<Placed> placed;
        for (int i = 0; i < m.len; ++i) {
            int r = m.row + (m.is_descending ? i : 0), c = m.col + (m.is_descending ? 0 : i);
            if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) continue;
            placeTile(b, r, c, letterFromChar(char('A' + m.letters[i] - 1)), (m.blank_mask >> i) & 1);
            Placed p; p.r = r; p.c = c;
            placed.push_back(p);
        }
        onPlay(b, al, g, init, placed.data(), (int)placed.size(), !m.is_descending);

        Board ref = b;
        AnchorLines refAl;
        rebuild(ref, refAl, g, init);

        for (int i = 0; i < BOARD_LENGTH; ++i) {
            CHECK(al.by_row[i] == refAl.by_row[i]);
            CHECK(al.by_col[i] == refAl.by_col[i]);
        }
        // cross_check is only meaningful at empty squares
        for (int r = 0; r < BOARD_LENGTH; ++r)
            for (int c = 0; c < BOARD_LENGTH; ++c) {
                if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) continue;
                if (b.cross_check[0][sq(r, c)] != ref.cross_check[0][sq(r, c)] ||
                    b.cross_check[1][sq(c, r)] != ref.cross_check[1][sq(c, r)]) {
                    std::printf("  mismatch ply %d at (%d,%d)\n", ply, r, c);
                    CHECK(false);
                }
            }
    }
    CHECK(horiz > 0 && vert > 0);
}