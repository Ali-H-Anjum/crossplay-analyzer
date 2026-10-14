//MoveEval.h

#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

#include "Board.h"
#include "Letter.h"
#include "Move.h"
#include "MoveGen.h"

// Points per letter (A..Z + blank). Index = toIndex(Letter).
inline constexpr std::array<uint8_t, NUM_LETTER_SLOTS> POINTS_PER_TILE = {
    /* A */ 1, /* B */ 4, /* C */ 3, /* D */ 2, /* E */ 1, /* F */ 4,
    /* G */ 4, /* H */ 3, /* I */ 1, /* J */ 10,/* K */ 6, /* L */ 2,
    /* M */ 3, /* N */ 1, /* O */ 1, /* P */ 3, /* Q */ 10,/* R */ 1,
    /* S */ 1, /* T */ 1, /* U */ 2, /* V */ 6, /* W */ 5, /* X */ 8,
    /* Y */ 4, /* Z */ 10,/* Blank */ 0
};

enum Mult : uint8_t { MULT_NONE = 0, MULT_DL = 1, MULT_TL = 2, MULT_DW = 3, MULT_TW = 4 };

// Premium layout keyed by (row, col) -- row 0 = top, col 0 = left.
// Matches the printed board layout.
inline const std::unordered_map<int, Mult>& premiumMap() {
    static const std::unordered_map<int, Mult> m = [] {
        std::unordered_map<int, Mult> t;
        auto put = [&](int r, int c, Mult v) { t[r * BOARD_LENGTH + c] = v; };

        // DL
        put(9,7,MULT_DL); put(7,9,MULT_DL); put(5,7,MULT_DL); put(7,5,MULT_DL);
        put(14,7,MULT_DL); put(7,14,MULT_DL); put(0,7,MULT_DL); put(7,0,MULT_DL);
        put(12,10,MULT_DL); put(11,11,MULT_DL); put(10,12,MULT_DL); put(4,12,MULT_DL);
        put(3,11,MULT_DL); put(2,10,MULT_DL);
        put(2,4,MULT_DL); put(3,3,MULT_DL); put(4,2,MULT_DL); put(10,2,MULT_DL);
        put(11,3,MULT_DL); put(12,4,MULT_DL);

        // TL
        put(10,9,MULT_TL); put(9,10,MULT_TL); put(5,10,MULT_TL); put(4,9,MULT_TL);
        put(4,5,MULT_TL); put(5,4,MULT_TL); put(9,4,MULT_TL); put(10,5,MULT_TL);
        put(13,6,MULT_TL); put(13,8,MULT_TL); put(8,13,MULT_TL); put(6,13,MULT_TL);
        put(1,8,MULT_TL); put(1,6,MULT_TL); put(6,1,MULT_TL); put(8,1,MULT_TL);
        put(14,0,MULT_TL); put(14,14,MULT_TL); put(0,14,MULT_TL); put(0,0,MULT_TL);

        // DW
        put(11,7,MULT_DW); put(7,11,MULT_DW); put(3,7,MULT_DW); put(7,3,MULT_DW);
        put(13,1,MULT_DW); put(13,13,MULT_DW); put(1,13,MULT_DW); put(1,1,MULT_DW);

        // TW
        put(14,11,MULT_TW); put(11,14,MULT_TW); put(3,14,MULT_TW); put(0,11,MULT_TW);
        put(0,3,MULT_TW);
        put(3,0,MULT_TW); put(11,0,MULT_TW); put(14,3,MULT_TW);

        return t;
    }();
    return m;
}

inline const std::array<Mult, NUM_SQUARES> kPremium = [] {
    std::array<Mult, NUM_SQUARES> t{};           // MULT_NONE == 0
    for (const auto& [k, v] : premiumMap()) t[k] = v;
    return t;
}();

inline Mult premiumAt(int row, int col) { return kPremium[sq(row, col)]; }

struct MoveScore {
    int  points    = 0;
    int  tilesUsed = 0;
    bool bingo     = false;
};

struct MoveEvaluator {
    const Board* board = nullptr;

    void setBoard(const Board* b) { board = b; }

    // Walk along the axis PERPENDICULAR to the move, collecting already-placed
    // letters adjacent to (r,c). C++ board: row 0 = top, col 0 = left.
    //   move horizontal (is_descending=false) -> cross word runs vertically
    //   move vertical   (is_descending=true)  -> cross word runs horizontally
    void surroundingWords(int r, int c, bool is_descending,
                          std::string& before, std::string& after) const {
        before.clear(); after.clear();
        if (!board) return;

        // Perpendicular axis: vertical if the move is horizontal, else horizontal.
        const int sdr = is_descending ? 0 : 1;   // vertical cross -> step row
        const int sdc = is_descending ? 1 : 0;   // horizontal cross -> step col

        int rr = r, cc = c;
        while (true) {
            rr -= sdr; cc -= sdc;
            if (rr < 0 || cc < 0 || rr >= BOARD_LENGTH || cc >= BOARD_LENGTH) break;
            uint8_t code = board->letters_row[sq(rr, cc)];
            if (code == EMPTY_SQUARE) break;
            char ch = toChar(fromBoardCode(code));
            if (board->is_blank.test(sq(rr, cc)))
                ch = (char)std::tolower((unsigned char)ch);
            before.insert(before.begin(), ch);
        }

        rr = r; cc = c;
        while (true) {
            rr += sdr; cc += sdc;
            if (rr < 0 || cc < 0 || rr >= BOARD_LENGTH || cc >= BOARD_LENGTH) break;
            uint8_t code = board->letters_row[sq(rr, cc)];
            if (code == EMPTY_SQUARE) break;
            char ch = toChar(fromBoardCode(code));
            if (board->is_blank.test(sq(rr, cc)))
                ch = (char)std::tolower((unsigned char)ch);
            after.push_back(ch);
        }
    }

    // Score the "main word" of a Move, plus tiles used and any bingo.
    MoveScore scoreMainWord(const Move& move) const {
        MoveScore out{};
        int n = 1;
        for (const auto& p : move.letterPositions()) {
            const int idx = sq(p.row, p.col);
            const bool occupied = board && board->letters_row[idx] != EMPTY_SQUARE;
            const bool isBlank  = p.is_blank ||
                                  (board && board->is_blank.test(idx));
            const uint8_t val  = POINTS_PER_TILE[toIndex(p.letter)];
            const Mult    mult = premiumAt(p.row, p.col);

            if (!occupied) {
                ++out.tilesUsed;
                if (mult == MULT_DW) n *= 2;
                else if (mult == MULT_TW) n *= 3;

                if (!isBlank) {
                    if (mult == MULT_DL)      out.points += val * 2;
                    else if (mult == MULT_TL) out.points += val * 3;
                    else                      out.points += val;
                }
            } else {
                if (!isBlank) out.points += val;
            }
        }
        out.points *= n;
        out.bingo   = (out.tilesUsed == 7);
        if (out.bingo) out.points += 40;
        return out;
    }

    int scoreCrossWord(const LetterPlacement& p, bool moveIsDescending) const {
        if (!board) return 0;
        const int idx = sq(p.row, p.col);
        if (board->letters_row[idx] != EMPTY_SQUARE) return 0;

        std::string before, after;
        surroundingWords(p.row, p.col, moveIsDescending, before, after);
        if (before.empty() && after.empty()) return 0;

        std::string crossWord = before;
        crossWord.push_back(toChar(p.letter));
        crossWord += after;

        // The cross word runs perpendicular to the move:
        //  - move horizontal -> cross word runs vertically (col fixed)
        //  - move vertical   -> cross word runs horizontally (row fixed)
        int crossRow, crossCol;
        bool crossDesc;
        if (moveIsDescending) {
            // cross word runs horizontally; letters before are to the LEFT
            crossRow = p.row;
            crossCol = p.col - (int)before.size();
            crossDesc = false;
        } else {
            // cross word runs vertically; letters before are ABOVE
            crossRow = p.row - (int)before.size();
            crossCol = p.col;
            crossDesc = true;
        }
        Move cross{ crossWord, crossRow, crossCol, crossDesc };
        return scoreMainWord(cross).points;
    }

    MoveScore score(const Move& move) const {
        MoveScore out = scoreMainWord(move);
        // Main word's bingo bonus is inside scoreMainWord; don't double it.
        const int bingoBonus = out.bingo ? 40 : 0;
        out.points -= bingoBonus;

        for (const auto& p : move.letterPositions())
            out.points += scoreCrossWord(p, move.is_descending);

        if (out.bingo) out.points += 40;
        return out;
    }

    MoveScore score(const RawMove& rm) const { return score(toMove(rm)); }
};