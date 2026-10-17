#include "check.h"
#include "test_common.h"
#include "board_ops.h"
#include "cross_checks.h"
#include "score_fast.h"
#include <vector>

TEST(scoreraw_matches_evaluator) {
    const Gaddag& g = testGaddag();
    const Arc& init = g.get_init();
    Board b{}; AnchorLines al; rebuild(b, al, g, init);
    MoveEvaluator ev; ev.setBoard(&b);
    const char* trays[] = {"AEINRST", "DEGLOUY", "BCMPHKO", "AEIOULN", "RSTLNEA", "TOWERSD"};
    size_t compared = 0; int bad = 0;
    for (const char* t : trays) {
        Tray tray{}; tray.addTiles(t);
        MoveList sink; generateMoves(g, b, al, tray, sink);
        if (sink.moves.empty()) break;
        const RawMove* best = &sink.moves[0]; int bestPts = -1;
        for (const RawMove& m : sink.moves) {
            const int fast = scoreRaw(b, m), slow = ev.score(m).points;
            if (fast != slow && bad++ < 5)
                std::printf("  mismatch fast=%d slow=%d at (%d,%d) len=%d desc=%d\n",
                            fast, slow, m.row, m.col, m.len, (int)m.is_descending);
            ++compared;
            if (fast > bestPts) { bestPts = fast; best = &m; }
        }
        const RawMove m = *best;                       // copy; apply it
        std::vector<Placed> placed;
        for (int i = 0; i < m.len; ++i) {
            int r = m.row + (m.is_descending ? i : 0), c = m.col + (m.is_descending ? 0 : i);
            if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) continue;
            placeTile(b, r, c, letterFromChar(char('A' + m.letters[i] - 1)), (m.blank_mask >> i) & 1);
            Placed p; p.r = r; p.c = c; placed.push_back(p);
        }
        onPlay(b, al, g, init, placed.data(), (int)placed.size(), !m.is_descending);
    }
    CHECK(bad == 0);
    CHECK(compared > 1000);
}

TEST(scorerawfull_matches_evaluator_fields) {
    const Gaddag& g = testGaddag();
    Board b{}; AnchorLines al; rebuild(b, al, g, g.get_init());
    MoveEvaluator ev; ev.setBoard(&b);
    Tray tray{}; tray.addTiles("AEINRST");
    MoveList sink; generateMoves(g, b, al, tray, sink);
    int bingos = 0;
    for (const RawMove& m : sink.moves) {
        const MoveScore f = scoreRawFull(b, m), s = ev.score(m);
        CHECK(f.points == s.points && f.tilesUsed == s.tilesUsed && f.bingo == s.bingo);
        bingos += f.bingo;
    }
    CHECK(bingos > 0);
}