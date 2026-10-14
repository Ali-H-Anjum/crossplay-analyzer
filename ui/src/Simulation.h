#pragma once

// Greedy two-player simulation. Stateless with respect to UI history.
// The UI owns all history (board/trays/bag/scores per entry); this class
// only knows how to (a) start a fresh game from a seed, and (b) produce
// the next turn from the current live state.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <bitset>
#include <random>
#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "Letter.h"
#include "Board.h"
#include "Move.h"
#include "Tray.h"
#include "BoardOps.h"
#include "CrossChecks.h"
#include "Gaddag.h"
#include "TileBag.h"
#include "MoveGen.h"
#include "ScoreFast.h"

struct GreedySim {
    uint64_t seed = 12345;

    // ----- Live state (the "current turn is playable" state) -----
    Board      board{};
    Tray       trays[2]{};
    std::vector<Letter> trayOrder[2];
    TileBag*   bag = nullptr;
    uint16_t   bagSize = 0;
    int        score[2] = {0,0};
    bool       gameOver = false;
    int        winner = -1;
    bool       finalRoundActive = false;
    bool       finalTurnTaken[2] = {false,false};
    int        consecutivePasses = 0;

    // Turn log (label + who played). Mirrors the UI history 1:1 but only the
    // parts the sim needs; UI keeps the deeper snapshot.
    struct Turn {
        int     turnNumber = 0;
        int     player = 0;
        bool    passed = false;
        bool    isFinal = false;
        Move    move{};
        int     score = 0;
    };
    std::vector<Turn> turns;

    Gaddag*       gd = nullptr;

    struct ScoredMove { RawMove rm; MoveScore sc; };
    ScoredMove bestScratch{};

    ~GreedySim() { delete bag; }

    // ---------------- setup ----------------
    void reset(Gaddag* g, uint64_t newSeed) {
        gd   = g;
        seed = newSeed;
        delete bag;
        bag = new TileBag(seed);

        board = Board{};
        for (int i = 0; i < 2; ++i) { trays[i] = Tray{}; trayOrder[i].clear(); }
        bag->drawInto(trays[0], 7);
        bag->drawInto(trays[1], 7);
        bagSize = bag->size();

        for (int i = 0; i < 2; ++i) { score[i] = 0; finalTurnTaken[i] = false; }
        gameOver = false;
        winner = -1;
        finalRoundActive = false;
        consecutivePasses = 0;

        turns.clear();
        syncTrayOrderAll();
        rebuildCrossState();
    }

    // ---------------- one step ----------------
    // Applies exactly one legal turn from the current live state. Returns
    // false when the game is already over.
    bool step() {
        if (gameOver) return false;

        const int p = (int)(turns.size() & 1);

        Turn t{};
        t.turnNumber = (int)turns.size() + 1;
        t.player     = p;
        t.isFinal    = finalRoundActive;

        Move mv; int sc = 0; MoveScore det{};
        bool found = bestMoveFor(p, mv, sc, det);

        if (!found) {
            t.passed = true;
            ++consecutivePasses;
        } else {
            t.move  = mv;
            t.score = sc;
            consecutivePasses = 0;
            applyMove(p, mv);
            score[p] += sc;
        }
        turns.push_back(t);

        if (!finalRoundActive && bagSize == 0) {
            finalRoundActive = true;
            finalTurnTaken[0] = finalTurnTaken[1] = false;
        }
        if (finalRoundActive) finalTurnTaken[p] = true;

        if (finalRoundActive && finalTurnTaken[0] && finalTurnTaken[1]) {
            gameOver = true;
        } else if (!finalRoundActive && consecutivePasses >= 2) {
            gameOver = true;
        }
        if (gameOver) {
            winner = (score[0] > score[1]) ? 0 :
                     (score[1] > score[0]) ? 1 : -1;
        }
        return true;
    }

    // ---------------- display helpers ----------------
    std::array<uint8_t, NUM_LETTER_SLOTS> bagCounts() const {
        return bag ? bag->counts() : std::array<uint8_t, NUM_LETTER_SLOTS>{};
    }
    std::string boardAscii() const {
        std::string out;
        for (int r = 0; r < BOARD_LENGTH; ++r) {
            for (int c = 0; c < BOARD_LENGTH; ++c) {
                const int idx = sq(r, c);
                uint8_t code = board.letters_row[idx];
                char ch = ' ';
                if (code != EMPTY_SQUARE) {
                    ch = toChar(fromBoardCode(code));
                    if (board.is_blank.test(idx))
                        ch = (char)std::tolower((unsigned char)ch);
                }
                out.push_back(' '); out.push_back(ch); out.push_back(' ');
                if (c < BOARD_LENGTH - 1) out.push_back('|');
            }
            out.push_back('\n');
        }
        return out;
    }

    // ---------------- internals ----------------
    void rebuildCrossState() {
        AnchorLines al;
        rebuild(board, al, *gd, gd->get_init());
    }

    bool bestMoveFor(int p, Move& outMove, int& outScore, MoveScore& outDetail) {
        MoveList sink;
        generateMovesFromScratch(*gd, board, trays[p], sink);   // rebuilds cross-checks itself

        const ScoredMove* best = nullptr;
        for (auto& rm : sink.moves) {
            MoveScore sc = scoreRawFull(board, rm);              // pre-move board, as required
            if (!best || sc.points > best->sc.points) {
                bestScratch.rm = rm;
                bestScratch.sc = sc;
                best = &bestScratch;
            }
        }
        if (!best) return false;
        outMove   = toMove(best->rm);
        outScore  = best->sc.points;
        outDetail = best->sc;
        return true;
    }

    void applyMove(int p, const Move& mv) {
        for (const auto& pl : mv.letterPositions()) {
            const int idx = sq(pl.row, pl.col);
            if (board.letters_row[idx] != EMPTY_SQUARE) continue;

            placeTile(board, pl.row, pl.col, pl.letter, pl.is_blank);

            const Letter trayLetter = pl.is_blank ? Letter::Blank : pl.letter;
            trays[p].removeTile(trayLetter);
            auto& ord = trayOrder[p];
            for (size_t i = 0; i < ord.size(); ++i) {
                if (ord[i] == trayLetter) { ord.erase(ord.begin() + (long)i); break; }
            }
        }
        int need = 7 - (int)trays[p].size();
        if (need > 0) bag->drawInto(trays[p], (uint8_t)need);
        bagSize = bag->size();
        rebuildTrayMirrorFromCounts(p);
    }

    void rebuildTrayMirrorFromCounts(int p) {
        auto& ord = trayOrder[p];
        ord.clear();
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
            for (uint8_t k = 0; k < trays[p].counts[i]; ++k)
                ord.push_back(fromIndex((uint8_t)i));
    }
    void syncTrayOrderAll() { for (int i = 0; i < 2; ++i) rebuildTrayMirrorFromCounts(i); }
};