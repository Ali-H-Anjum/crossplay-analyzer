#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <ctime>
#include <filesystem>
#include <fstream>
#include "Eval.h"
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.
// Requires State.h to be included before this file (uses ModeState, Snapshot, Entry).

// ================================================================ Mover config
struct MoverConfig {
    MoverKind kind = MoverKind::Greedy;
    BotPanel  search;   // used only when kind == Search
    MoverConfig() { search.abSeconds = 60; search.mctsSeconds = 60; }
};

// What a rack could do this turn (filled while generating moves).
struct RackScan {
    int best = 0, nMoves = 0, n40 = 0, n50 = 0;
    int bestTW = 0, bestDW = 0, bestTL = 0, bestDL = 0;   // best score of a move covering that premium (0 = none)
};

// What the search reported for the move it chose.
// value: AB = tenths of a point, MCTS = win rate x 1000.
struct SearchStat {
    bool used = false, fellBack = false;
    int  algo = 0;                          // 0 = AB, 1 = MCTS
    int  depth = 0, samples = 0, rootMoves = 0, timeMs = 0, limitMs = 0;   // limitMs 0 = depth-limited
    uint64_t nodes = 0;                     // AB nodes, MCTS simulations
    int  value = 0;
    int  greedyRank = -1, greedyValue = 0;  // best-ranked root move scoring Greedy's best; -1 = not in the root list
};

// DW/TW squares that are empty and touch a tile.
static int OpenBigAnchors(const Board& b) {
    int n = 0;
    for (uint8_t i : kBigPremium) {
        if (b.letters_row[i] != EMPTY_SQUARE) continue;
        const int r = i / BOARD_LENGTH, c = i % BOARD_LENGTH;
        const bool touch = (r > 0 && b.letters_row[i - BOARD_LENGTH] != EMPTY_SQUARE) ||
                           (r < BOARD_LENGTH - 1 && b.letters_row[i + BOARD_LENGTH] != EMPTY_SQUARE) ||
                           (c > 0 && b.letters_row[i - 1] != EMPTY_SQUARE) ||
                           (c < BOARD_LENGTH - 1 && b.letters_row[i + 1] != EMPTY_SQUARE);
        n += touch;
    }
    return n;
}

// Alpha-beta re-run with the opponent's true rack known (batch diagnostics). Values in tenths of a point.
struct PerfStat { bool used = false, found = false; int best = 0, chosen = 0, depth = 0; };

// Distinct racks of k tiles that can be drawn from `pool` (multisets).
static double CountRacks(const std::array<uint8_t, NUM_LETTER_SLOTS>& pool, int k) {
    double dp[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        double nd[8] = {};
        for (int j = 0; j <= k; ++j)
            for (int c = 0; c <= (int)pool[i] && c <= j; ++c) nd[j] += dp[j - c];
        for (int j = 0; j <= k; ++j) dp[j] = nd[j];
    }
    return dp[k];
}

// ================================================================ SimGame
// Two-player simulation. Stateless with respect to UI history.
// The UI owns all history (board/trays/bag/scores per entry); this class
// only knows how to (a) start a fresh game from a seed, and (b) produce
// the next turn from the current live state.

struct SimGame {
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
    int        consecutiveScoreless = 0;   // passes + swaps in a row
    bool       perfInfo = false;                  // batch diagnostics: second search with the opponent's rack known
    const std::atomic<bool>* abortFlag = nullptr; // lets that second search be cancelled

    // Turn log (label + who played). Mirrors the UI history 1:1 but only the
    // parts the sim needs; UI keeps the deeper snapshot.
    struct Turn {
        int         turnNumber = 0;
        int         player = 0;
        bool        passed = false;
        bool        swapped = false;
        std::string swapLetters;      // tiles thrown back, A-Z order, '?' = blank
        bool        isFinal = false;
        Move        move{};
        int         score = 0;
        // ---- diagnostics, captured before the turn is applied
        int         greedyBest = 0;               // best Greedy score available from this rack
        int         tilesUsed = 0;
        bool        bingo = false;
        int         bagBefore = 0;
        int         scoreDiffBefore = 0;          // mover - opponent
        int         rackLeaveBefore = 0, oppLeaveBefore = 0;   // LEAVE sum, tenths of a point
        int         leaveKept = 0;                // LEAVE of the tiles kept (before drawing), tenths
        int         keptSize = 0, keptS = 0, keptBlank = 0, keptVowels = 0, keptDups = 0;
        int         rackS = 0, rackBlank = 0;     // S / blank in the rack before the turn
        std::string kept;                         // tiles kept, A-Z order, '?' = blank
        // ---- lanes (pass B)
        int         oppBestBefore = 0;            // opponent's best Greedy score on the pre-move board, true rack
        int         openBigBefore = 0;            // open DW/TW anchors before the move
        int         nMoves = 0, n40 = 0, n50 = 0; // moves available, and how many score >= 40 / >= 50
        int         bestTW = 0, bestDW = 0, bestTL = 0, bestDL = 0;
        int         usedBig = 0;                  // DW/TW squares covered by the move played
        SearchStat  ss;
        // ---- hidden information and perfect-info reference (pass C)
        std::string oppRack;                      // opponent's true rack, A-Z order, '?' = blank
        int         oppS = 0, oppBlank = 0, poolSize = 0;
        float       expS = 0, expBlank = 0;       // expected in the opponent's rack under a uniform draw from the unseen pool
        double      possRacks = -1;               // distinct opponent racks consistent with what the mover sees (bag <= 7), else -1
        PerfStat    pi;
    };

    std::vector<Turn> turns;

    Gaddag*       gd = nullptr;
    MoverConfig   movers[2];

    struct ScoredMove { RawMove rm; MoveScore sc; };
    ScoredMove bestScratch{};

    ~SimGame() { delete bag; }

    // ---------------- setup ----------------
    void reset(Gaddag* g, uint64_t newSeed) {
        gd   = g;
        seed = newSeed;
        delete bag;
        bag = new TileBag(seed);

        board = Board{};
        for (int i = 0; i < 2; ++i) {
            trays[i] = Tray{}; trayOrder[i].clear();
            movers[i].search.shutdown();
            movers[i].search.haveResult = false;
        }
        bag->drawInto(trays[0], 7);
        bag->drawInto(trays[1], 7);
        bagSize = bag->size();

        for (int i = 0; i < 2; ++i) { score[i] = 0; finalTurnTaken[i] = false; }
        gameOver = false;
        winner = -1;
        finalRoundActive = false;
        consecutivePasses = 0;
        consecutiveScoreless = 0;

        turns.clear();
        syncTrayOrderAll();
        rebuildCrossState();
    }

    // Leave statistics for the tiles kept after a turn (before drawing).
    static void fillKept(Turn& t, const Tray& k) {
        static const int kVowel[] = { 0, 4, 8, 14, 20 };   // A E I O U
        t.leaveKept = leaveValue(k);
        t.keptSize  = (int)k.size();
        t.keptS     = k.counts[18];                        // 'S' - 'A'
        t.keptBlank = k.counts[BLANK_SLOT];
        for (int v : kVowel) t.keptVowels += k.counts[v];
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
            if (k.counts[i] > 1) t.keptDups += k.counts[i] - 1;
            t.kept.append(k.counts[i], i == BLANK_SLOT ? '?' : (char)('A' + i));
        }
    }

    // Exchanges the tiles in `thrown` for new draws: remove, draw, then return
    // (so a thrown tile can't be redrawn). Returns false if the swap isn't possible.
    bool applySwap(int p, const Tray& thrown) {
        const int k = (int)thrown.size();
        if (k <= 0 || k > kRackSize || k > (int)bag->size()) return false;
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
            if (thrown.counts[i] > trays[p].counts[i]) return false;

        Letter ret[kRackSize];
        int n = 0;
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
            for (uint8_t c = 0; c < thrown.counts[i]; ++c) {
                ret[n] = fromIndex((uint8_t)i);
                trays[p].removeTile(ret[n]);
                ++n;
            }
        bag->drawInto(trays[p], (uint8_t)k);

        // Slots [total, total+k) hold the tiles just drawn; overwrite them with the returned ones.
        auto tiles = bag->tilesCopy();
        const uint16_t total = bag->totalCount();
        for (int j = 0; j < k; ++j) tiles[total + j] = ret[j];
        bag->restoreFrom(tiles, (uint16_t)(total + k));

        bagSize = bag->size();
        rebuildTrayMirrorFromCounts(p);
        return true;
    }

     // Records a finished turn (move, swap or pass) and updates end-of-game state.
    // `scan` (this rack's move scan) is computed here if the caller didn't have one.
    void finishTurn(int p, bool found, const Move& mv, int sc, const Tray* swapThrown = nullptr,
                    const RackScan* scan = nullptr, const SearchStat* ss = nullptr, const PerfStat* pi = nullptr) {
        Turn t{};
        t.turnNumber = (int)turns.size() + 1;
        t.player     = p;
        t.isFinal    = finalRoundActive;

        // Diagnostics, captured before the turn changes anything.
        RackScan own;
        if (!scan) { Move gm; int gs = 0; MoveScore gd{}; bestMoveFor(p, gm, gs, gd, &own); scan = &own; }
        RackScan opp;
        { Move gm; int gs = 0; MoveScore gd{}; bestMoveFor(p ^ 1, gm, gs, gd, &opp); }

        t.greedyBest      = scan->best;
        t.nMoves = scan->nMoves; t.n40 = scan->n40; t.n50 = scan->n50;
        t.bestTW = scan->bestTW; t.bestDW = scan->bestDW; t.bestTL = scan->bestTL; t.bestDL = scan->bestDL;
        t.oppBestBefore   = opp.best;
        t.openBigBefore   = OpenBigAnchors(board);
        t.bagBefore       = (int)bagSize;
        t.scoreDiffBefore = score[p] - score[p ^ 1];
        t.rackLeaveBefore = leaveValue(trays[p]);
        t.oppLeaveBefore  = leaveValue(trays[p ^ 1]);
        t.rackS           = trays[p].counts[18];
        t.rackBlank       = trays[p].counts[BLANK_SLOT];
        if (ss) t.ss = *ss;
        if (pi) t.pi = *pi;
        {
            const Tray& o = trays[p ^ 1];
            t.oppS = o.counts[18]; t.oppBlank = o.counts[BLANK_SLOT];
            for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
                t.oppRack.append(o.counts[i], i == BLANK_SLOT ? '?' : (char)('A' + i));
            const auto pool = unseenPool(board, trays[p]);
            int tot = 0; for (uint8_t v : pool) tot += v;
            t.poolSize = tot;
            const int oppN = std::min(kRackSize, tot);
            if (tot > 0) {
                t.expS     = (float)oppN * pool[18] / tot;
                t.expBlank = (float)oppN * pool[BLANK_SLOT] / tot;
            }
            if (bagSize <= kRackSize) t.possRacks = CountRacks(pool, oppN);
        }

        if (found) {
            Tray kept = trays[p];
            for (const auto& pl : mv.letterPositions()) {
                const int idx = sq(pl.row, pl.col);
                if (board.letters_row[idx] != EMPTY_SQUARE) continue;
                kept.removeTile(pl.is_blank ? Letter::Blank : pl.letter);
                ++t.tilesUsed;
                if (kPremium[idx] == MULT_DW || kPremium[idx] == MULT_TW) ++t.usedBig;
            }
            t.bingo = (t.tilesUsed == kRackSize);
            fillKept(t, kept);
            t.move  = mv;
            t.score = sc;
            consecutivePasses = 0;
            consecutiveScoreless = 0;
            applyMove(p, mv);
            score[p] += sc;
        } else {
            Tray kept = trays[p];
            bool swapped = false;
            if (swapThrown) {
                Tray after = trays[p];
                for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
                    for (uint8_t c = 0; c < swapThrown->counts[i] && after.counts[i] > 0; ++c)
                        after.removeTile(fromIndex((uint8_t)i));
                if (applySwap(p, *swapThrown)) {
                    swapped = true;
                    kept = after;
                    t.swapped = true;
                    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
                        t.swapLetters.append(swapThrown->counts[i], i == BLANK_SLOT ? '?' : (char)('A' + i));
                    consecutivePasses = 0;
                    ++consecutiveScoreless;
                }
            }
            if (!swapped) { t.passed = true; ++consecutivePasses; ++consecutiveScoreless; }
            fillKept(t, kept);
        }
        turns.push_back(t);

        // Final round: once the bag is empty, the opponent moves, then the player who emptied
        // it moves again, and the game ends. The emptying move itself doesn't count.
        const bool wasFinal = finalRoundActive;
        if (!finalRoundActive && bagSize == 0) {
            finalRoundActive = true;
            finalTurnTaken[0] = finalTurnTaken[1] = false;
        }
        if (wasFinal) finalTurnTaken[p] = true;

        if (finalRoundActive && finalTurnTaken[0] && finalTurnTaken[1]) gameOver = true;
        else if (!finalRoundActive && consecutivePasses >= 2)           gameOver = true;
        else if (consecutiveScoreless >= 6)                             gameOver = true;
        if (gameOver)
            winner = (score[0] > score[1]) ? 0 : (score[1] > score[0]) ? 1 : -1;
    }
    
    // Applies exactly one greedy turn. Returns false when the game is already over.
    bool step() {
        if (gameOver) return false;
        const int p = (int)(turns.size() & 1);
        RackScan scan; Move mv; int sc = 0; MoveScore det{};
        const bool found = bestMoveFor(p, mv, sc, det, &scan);
        finishTurn(p, found, mv, sc, nullptr, &scan);
        return true;
    }

    // ---------------- async step ----------------
    enum class StepState { Idle, Thinking, Done, Failed };

    // Start one turn. If the current mover is Search, kicks off its async
    // search and returns Thinking. Otherwise plays the greedy move and
    // returns Done (or Failed if the game is already over).
    StepState beginStep() {
        if (gameOver) return StepState::Failed;
        const int p = (int)(turns.size() & 1);
        if (movers[p].kind == MoverKind::Greedy) {
            return step() ? StepState::Done : StepState::Failed;
        }
        BotPanel& bp = movers[p].search;
        if (!bp.running() && !bp.haveResult) {
            // Per-game, per-turn seed: reproducible, and not the same random stream every move.
            const uint64_t s = mix64(seed ^ (0xA24BAED4963EE407ull * (uint64_t)(turns.size() + 1)) ^ (uint64_t)p);
            bp.dc.seed = s; bp.mc.seed = s;
            bp.start(*gd, board, trays[p]);
        }
        return StepState::Thinking;
    }

    // Poll an in-flight search. Thinking while still searching, Done when the move
    // was applied, Failed if the game is over.
    StepState pollStep() {
        if (gameOver) return StepState::Failed;
        const int p = (int)(turns.size() & 1);
        BotPanel& bp = movers[p].search;
        bp.poll();
        if (!bp.haveResult) return StepState::Thinking;

        const bool det = (bp.resultAlgo == 0);
        const SMove* best = nullptr;
        if (det) { if (bp.detRes.hasMove)  best = &bp.detRes.best; }
        else     { if (bp.mctsRes.hasMove) best = &bp.mctsRes.best; }

        // One scan of this rack: Greedy's best, premium lanes, and the fallback move.
        RackScan scan; Move gMv; int gSc = 0; MoveScore gDet{};
        const bool gFound = bestMoveFor(p, gMv, gSc, gDet, &scan);

        SearchStat ss;
        ss.used = true; ss.algo = det ? 0 : 1; ss.fellBack = (best == nullptr);
        ss.limitMs = det ? (bp.iterative ? bp.abSeconds * 1000 : 0) : bp.mctsSeconds * 1000;
        if (det) {
            const DetResult& r = bp.detRes;
            ss.depth = r.depth; ss.nodes = r.nodes; ss.samples = r.samples; ss.timeMs = r.timeMs;
            ss.rootMoves = (int)r.ranked.size();
            if (best) {
                ss.value = r.value;
                for (size_t i = 0; i < r.ranked.size(); ++i) {
                    const DetMove& d = r.ranked[i];
                    if (d.m.kind == MK_PLACE && scoreRaw(board, d.m.m) == scan.best) {
                        ss.greedyRank = (int)i; ss.greedyValue = (int)std::lround(d.avg); break;
                    }
                }
            }
        } else {
            const MctsResult& r = bp.mctsRes;
            ss.nodes = r.sims; ss.timeMs = r.timeMs; ss.rootMoves = (int)r.ranked.size();
            if (best) {
                ss.value = (int)std::lround(r.winRate * 1000.0);
                for (size_t i = 0; i < r.ranked.size(); ++i) {
                    const MctsMove& d = r.ranked[i];
                    if (d.m.kind == MK_PLACE && scoreRaw(board, d.m.m) == scan.best) {
                        ss.greedyRank = (int)i; ss.greedyValue = (int)std::lround(d.winRate * 1000.0); break;
                    }
                }
            }
        }

        Move mv; int sc = 0; bool found = false;
        Tray thrown; const Tray* swap = nullptr;
        if (!best) {
            found = gFound; mv = gMv; sc = gSc;   // search produced nothing (e.g. stopped early): greedy
        } else if (best->kind == MK_PLACE) {
            mv = toMove(best->m);
            sc = scoreRawFull(board, best->m).points;
            found = true;
        } else if (best->kind == MK_SWAP) {
            thrown = det ? bp.detRes.swapThrown : bp.mctsRes.swapThrown;
            swap = &thrown;
        }

        // Perfect-information reference: same board and rack, opponent's true rack known.
        PerfStat pi;
        if (perfInfo) {
            SMove chosen; bool have = false;
            if (best)        { chosen = *best; have = true; }
            else if (gFound) { chosen.m = bestScratch.rm; have = true; }
            if (have) pi = perfectRef(p, bp, chosen);
        }

        bp.shutdown();
        bp.haveResult = false;      // consumed; otherwise the next turn replays this result
        finishTurn(p, found, mv, sc, swap, &scan, &ss, &pi);
        return StepState::Done;
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
    // Blocking alpha-beta with the opponent's true rack. Uses the bot's AB settings. Batch only.
    PerfStat perfectRef(int p, const BotPanel& bp, const SMove& chosen) {
        PerfStat out;
        Board rb = board;
        Position root{};
        rebuild(rb, root.al, *gd, gd->get_init());
        root.b = &rb; root.g = gd; root.init = &gd->get_init(); root.tray[0] = trays[p];

        SearchConfig c = bp.sc;
        c.iterative = bp.iterative; c.depth = std::max(1, bp.sc.depth);
        c.timeLimitMs = std::clamp(bp.abSeconds, 1, 86400) * 1000;
        DetConfig d = bp.dc;
        d.threads = std::max(0, bp.threads);
        d.rootMaxMoves = std::max(0, d.rootMaxMoves);
        d.seed = mix64(seed ^ (0x9FB21C651E98DF25ull * (uint64_t)(turns.size() + 1)));
        d.knownOpp = true; d.oppRack = trays[p ^ 1];

        DetSearch ds(root, c, d);
        while (!ds.finished()) {
            if (abortFlag && abortFlag->load()) { ds.cancel(); return out; }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const DetResult r = ds.take();
        if (!r.hasMove || r.ranked.empty()) return out;

        const int topSamples = r.ranked[0].samples;       // ranked by sample count first
        double best = -1e18, ch = 0; bool found = false;
        for (const DetMove& m : r.ranked) {
            if (m.samples == topSamples) best = std::max(best, m.avg);
            if (sameMove(m.m, chosen)) { ch = m.avg; found = true; }
        }
        if (found) best = std::max(best, ch);
        out.used = true; out.found = found; out.depth = r.depth;
        out.best = (int)std::lround(best); out.chosen = (int)std::lround(ch);
        return out;
    }

    // Premium squares covered by the new tiles of `rm`, folded into the rack scan.
    void scanMove(RackScan& s, const RawMove& rm, int pts) const {
        const int dr = rm.is_descending ? 1 : 0, dc = 1 - dr;
        bool tw = false, dw = false, tl = false, dl = false;
        int r = rm.row, c = rm.col;
        for (int i = 0; i < rm.len; ++i, r += dr, c += dc) {
            const int idx = sq(r, c);
            if (board.letters_row[idx] != EMPTY_SQUARE) continue;
            switch (kPremium[idx]) {
                case MULT_TW: tw = true; break;
                case MULT_DW: dw = true; break;
                case MULT_TL: tl = true; break;
                case MULT_DL: dl = true; break;
                default: break;
            }
        }
        if (tw) s.bestTW = std::max(s.bestTW, pts);
        if (dw) s.bestDW = std::max(s.bestDW, pts);
        if (tl) s.bestTL = std::max(s.bestTL, pts);
        if (dl) s.bestDL = std::max(s.bestDL, pts);
        if (pts >= 40) ++s.n40;
        if (pts >= 50) ++s.n50;
    }

    bool bestMoveFor(int p, Move& outMove, int& outScore, MoveScore& outDetail, RackScan* scan = nullptr) {
        MoveList sink;
        generateMovesFromScratch(*gd, board, trays[p], sink);   // rebuilds cross-checks itself
        if (scan) *scan = RackScan{};

        const ScoredMove* best = nullptr;
        for (auto& rm : sink.moves) {
            MoveScore sc = scoreRawFull(board, rm);              // pre-move board, as required
            if (!best || sc.points > best->sc.points) {
                bestScratch.rm = rm;
                bestScratch.sc = sc;
                best = &bestScratch;
            }
            if (scan) scanMove(*scan, rm, sc.points);
        }
        if (scan) { scan->nMoves = (int)sink.moves.size(); scan->best = best ? best->sc.points : 0; }
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

// ================================================================ Batch runner
static void CopyMoverSettings(MoverConfig& d, const MoverConfig& s) {
    d.kind = s.kind;
    BotPanel& a = d.search; const BotPanel& b = s.search;
    a.algo = b.algo; a.threads = b.threads; a.iterative = b.iterative;
    a.abSeconds = b.abSeconds; a.mctsSeconds = b.mctsSeconds;
    a.sc = b.sc; a.dc = b.dc; a.mc = b.mc;
}

static std::string DescribeMover(const MoverConfig& m) {
    if (m.kind == MoverKind::Greedy) return "Greedy (best-scoring move)";
    const BotPanel& b = m.search;
    auto yn = [](bool v) { return v ? "on" : "off"; };
    char buf[480];
    if (b.algo == 0) {
        const std::string limit = b.iterative ? "iterative, " + std::to_string(b.abSeconds) + " s/move"
                                              : "fixed depth " + std::to_string(b.sc.depth);
        std::snprintf(buf, sizeof buf,
            "Determinized AB (%s), threads %d, root moves %d\n"
            "    ordering %s, killers %s, history %s, LMR %s, ProbCut %s, beam %s, TT %s",
            limit.c_str(), b.threads, (int)b.dc.rootMaxMoves, yn(b.sc.orderStatic), yn(b.sc.killers),
            yn(b.sc.history), yn(b.sc.lmr), yn(b.sc.probCut), yn(b.sc.useBeam), yn(b.sc.useTT));
    } else {
        std::snprintf(buf, sizeof buf,
            "MCTS (%d s/move), threads %d\n"
            "    sims/thread %d, sims/tree %d, max children %d, C %.2f, epsilon %.2f, rollout plies %d",
            b.mctsSeconds, b.threads, (int)b.mc.simulations, (int)b.mc.simsPerTree, (int)b.mc.maxChildren,
            (double)b.mc.C, (double)b.mc.epsilon, (int)b.mc.rolloutMaxPlies);
    }
    return buf;
}

#ifndef DIAG_DATA_DIR
#define DIAG_DATA_DIR "diagnostic_data"
#endif

// Changes whenever any Eval weight or leave value changes, so pasted summaries are comparable.
static std::string EvalFingerprint() {
    uint32_t h = 2166136261u;
    auto mix = [&](int v) { h ^= (uint32_t)v; h *= 16777619u; };
    for (int w : W_BAG) mix(w);
    for (int w : W_ENDGAME) mix(w);
    for (int v : LEAVE) mix(v);
    char b[16]; std::snprintf(b, sizeof b, "%08x", h);
    return b;
}

static std::string ShortMover(const MoverConfig& m) {
    if (m.kind == MoverKind::Greedy) return "Greedy";
    const BotPanel& b = m.search;
    char buf[64];
    if (b.algo == 0) {
        if (b.iterative) std::snprintf(buf, sizeof buf, "AB-iter-%ds-t%d", b.abSeconds, b.threads);
        else             std::snprintf(buf, sizeof buf, "AB-d%d-t%d", (int)b.sc.depth, b.threads);
    } else {
        std::snprintf(buf, sizeof buf, "MCTS-%ds-t%d", b.mctsSeconds, b.threads);
    }
    return buf;
}

static std::string TimeStamp() {
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y%m%d_%H%M%S", std::localtime(&t));
    return b;
}

struct BotAgg {
    long long turns = 0, scoreSum = 0, moves = 0, bingos = 0, tiles = 0, matches = 0;
    double    regretMove = 0, regretAll = 0;
    long long swaps = 0, passes = 0;
    double    swapTurn = 0, swapForgone = 0, swapBag = 0;
    double    leaveSum = 0, keptSize = 0, dups = 0, vowels = 0;
    long long sTurns = 0, sKept = 0, bTurns = 0, bKept = 0;
    long long bucketN[5] = {}, bucketSum[5] = {};
    // lanes
    double    openBig = 0, nMoves = 0;
    long long has40 = 0, has50 = 0, usedBig = 0;
    double    premBest[4] = {};  long long premTurns[4] = {};     // TW, DW, TL, DL
    long long laneN[4] = {};     double laneSum[4] = {};          // opp best-reply change by open-anchor change
    double    oppBefore = 0, oppAfter = 0; long long oppN = 0;
    // search
    long long srN = 0, srFell = 0, srAB = 0, srMC = 0, srTimed = 0, srGapN = 0, srNoGreedy = 0;
    double    srRoot = 0, srTime = 0, srTimedT = 0, srLimit = 0, srDepth = 0, srNodes = 0, srSamples = 0;
    double    srGap = 0, srRank = 0, srSims = 0, mcWin = 0, mcGWin = 0;
    long long mcWinN = 0, mcGN = 0;
};

// Plays N games headless on worker threads. Fields are noted by who writes them.
struct BatchRun {
    struct GameRes {
        bool ok = false;
        int  flip = 0, s[2] = {0, 0};            // s[]: Bot 1 / Bot 2
        int  left[2] = {0, 0}, out = -1;         // rack points left per bot; bot that went out (-1 = nobody)
        std::vector<SimGame::Turn> tv;
    };

    std::vector<std::thread> pool;
    std::atomic<bool>  cancel{false};
    std::atomic<bool>  finished{false};
    std::atomic<int>   done{0}, next{0}, alive{0};
    bool        running = false, showSummary = false;     // UI thread only
    bool        perfInfo = false;                          // UI thread sets before start
    int         total = 0, parallel = 1;
    uint64_t    baseSeed = 0;
    std::string runId;
    MoverConfig cfg[2];                                    // snapshot taken at start
    std::vector<GameRes> res;                              // slot g written only by the worker playing game g
    std::string summary;                                   // written by the last worker before finished = true
    std::chrono::steady_clock::time_point t0;

    ~BatchRun() { stop(); }
    void stop() {
        cancel = true;
        for (auto& t : pool) if (t.joinable()) t.join();
        pool.clear();
    }

    void worker(Gaddag* gd) {
        for (;;) {
            const int g = next++;
            if (g >= total || cancel) break;
            playGame(gd, g);
        }
        if (--alive == 0) { summarize(); finished = true; }
    }

    void playGame(Gaddag* gd, int g) {
        const int flip = g & 1;                           // odd games: Bot 2 moves first
        SimGame sim;
        for (int i = 0; i < 2; ++i) CopyMoverSettings(sim.movers[i], cfg[i ^ flip]);
        sim.perfInfo = perfInfo; sim.abortFlag = &cancel;
        sim.reset(gd, baseSeed + (uint64_t)(g >> 1) * 0x9E3779B97F4A7C15ull);

        while (!sim.gameOver) {
            auto st = sim.beginStep();
            while (st == SimGame::StepState::Thinking) {
                if (cancel) { for (auto& m : sim.movers) m.search.shutdown(); return; }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                st = sim.pollStep();
            }
            if (st == SimGame::StepState::Failed) break;
        }

        GameRes& r = res[g];
        r.flip = flip;
        r.s[0] = sim.score[flip]; r.s[1] = sim.score[flip ^ 1];   // Bot 1 is player `flip`
        for (int b = 0; b < 2; ++b) {
            const Tray& t = sim.trays[flip ^ b];
            for (int i = 0; i < NUM_LETTER_SLOTS; ++i) r.left[b] += POINTS_PER_TILE[i] * t.counts[i];
            if (r.out < 0 && t.size() == 0) r.out = b;
        }
        r.tv = std::move(sim.turns);
        r.ok = true;
        ++done;
    }

    // Pass C sections: perfect-info regret, hidden information, game end, Eval calibration.
    void sectionsC(std::string& s, int games) {
        auto add = [&](const char* fmt, auto... a) { char b[512]; std::snprintf(b, sizeof b, fmt, a...); s += b; };
        auto head = [&](const char* title) { add("\n== %s ==\n", title); };
        auto cell = [](char* o, size_t z, const char* f, double v, long long n) {
            if (n > 0) std::snprintf(o, z, f, v); else std::snprintf(o, z, "-");
        };
        auto two = [&](const char* label, const char* f, double a, long long na, double b, long long nb) {
            char x[40], y[40]; cell(x, sizeof x, f, a, na); cell(y, sizeof y, f, b, nb);
            add("  %-40s%10s   %10s\n", label, x, y);
        };
        auto phaseOf = [](int bag) { return bag > 50 ? 0 : bag >= 8 ? 1 : 2; };

        struct PI  { long long n = 0, found = 0, zero = 0, phN[3] = {}; double gap = 0, phGap[3] = {}; } pi[2];
        struct Opp { long long n = 0, endN = 0; double s = 0, es = 0, b = 0, eb = 0, logR = 0, pool = 0; } op[2];
        struct Reg {
            long long n = 0; double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
            void put(double x, double y) { ++n; sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y; }
        } allReg, phReg[3], bk[5];
        double left[2] = {0, 0}; long long outN[2] = {0, 0}, noOut = 0, gN = 0;

        for (const GameRes& r : res) {
            if (!r.ok) continue;
            ++gN;
            for (int b = 0; b < 2; ++b) { left[b] += r.left[b]; if (r.out == b) ++outN[b]; }
            if (r.out < 0) ++noOut;
            bool seenEnd[2] = {false, false};
            const int fin[2] = { r.s[0] - r.s[1], r.s[1] - r.s[0] };   // final margin from Bot 1 / Bot 2's side
            for (const auto& t : r.tv) {
                const int bot = t.player ^ r.flip;
                const int pidx = phaseOf(t.bagBefore);
                if (t.pi.used) {
                    PI& q = pi[bot]; ++q.n;
                    if (t.pi.found) {
                        const double gap = (t.pi.best - t.pi.chosen) / 10.0;
                        ++q.found; q.gap += gap; if (t.pi.best <= t.pi.chosen) ++q.zero;
                        ++q.phN[pidx]; q.phGap[pidx] += gap;
                    }
                }
                if (t.bagBefore > 0) {
                    Opp& o = op[bot]; ++o.n;
                    o.s += t.oppS; o.es += t.expS; o.b += t.oppBlank; o.eb += t.expBlank;
                    if (t.possRacks > 0 && !seenEnd[bot]) {
                        seenEnd[bot] = true; ++o.endN; o.logR += std::log10(t.possRacks); o.pool += t.poolSize;
                    }
                }
                const SearchStat& q = t.ss;
                if (q.used && q.algo == 0 && !q.fellBack) {
                    const double x = q.value / 10.0, y = fin[bot];
                    allReg.put(x, y); phReg[pidx].put(x, y);
                    bk[x < -60 ? 0 : x < -20 ? 1 : x < 20 ? 2 : x < 60 ? 3 : 4].put(x, y);
                }
            }
        }

        // ---- perfect-information regret
        if (perfInfo) {
            head("PERFECT-INFORMATION REGRET (chosen move vs alpha-beta that sees the opponent's true rack)");
            add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
            two("Search turns measured", "%.0f", (double)pi[0].n, pi[0].n, (double)pi[1].n, pi[1].n);
            two("Chosen move found in the reference list", "%.1f%%", 100.0 * pi[0].found / std::max(1LL, pi[0].n), pi[0].n,
                100.0 * pi[1].found / std::max(1LL, pi[1].n), pi[1].n);
            two("Avg regret (pts)", "%.2f", pi[0].gap / std::max(1LL, pi[0].found), pi[0].found,
                pi[1].gap / std::max(1LL, pi[1].found), pi[1].found);
            two("Turns where chosen = perfect-info best", "%.1f%%", 100.0 * pi[0].zero / std::max(1LL, pi[0].found), pi[0].found,
                100.0 * pi[1].zero / std::max(1LL, pi[1].found), pi[1].found);
            static const char* kPh[3] = { "Avg regret, opening (bag > 50)", "Avg regret, midgame (bag 8-50)", "Avg regret, endgame (bag < 8)" };
            for (int k = 0; k < 3; ++k)
                two(kPh[k], "%.2f", pi[0].phGap[k] / std::max(1LL, pi[0].phN[k]), pi[0].phN[k],
                    pi[1].phGap[k] / std::max(1LL, pi[1].phN[k]), pi[1].phN[k]);
            s += "  (Reference = same AB settings, true opponent rack, bag order still unknown. Single-thread bots sample one bag order, so it is noisy.)\n";
        } else {
            s += "\n(Perfect-information regret: off. Enable the 'Perfect-info' checkbox before starting a batch.)\n";
        }

        // ---- hidden information
        head("OPPONENT'S HIDDEN RACK (turns with tiles in the bag; column = the bot moving)");
        add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
        two("Opp S per rack: actual", "%.3f", op[0].s / std::max(1LL, op[0].n), op[0].n, op[1].s / std::max(1LL, op[1].n), op[1].n);
        two("Opp S per rack: uniform-draw expectation", "%.3f", op[0].es / std::max(1LL, op[0].n), op[0].n, op[1].es / std::max(1LL, op[1].n), op[1].n);
        two("Opp blank per rack: actual", "%.3f", op[0].b / std::max(1LL, op[0].n), op[0].n, op[1].b / std::max(1LL, op[1].n), op[1].n);
        two("Opp blank per rack: uniform-draw expectation", "%.3f", op[0].eb / std::max(1LL, op[0].n), op[0].n, op[1].eb / std::max(1LL, op[1].n), op[1].n);
        two("First turn with bag <= 7: possible opp racks", "%.0f",
            std::pow(10.0, op[0].logR / std::max(1LL, op[0].endN)), op[0].endN,
            std::pow(10.0, op[1].logR / std::max(1LL, op[1].endN)), op[1].endN);
        two("   ... unseen tiles at that point", "%.1f", op[0].pool / std::max(1LL, op[0].endN), op[0].endN,
            op[1].pool / std::max(1LL, op[1].endN), op[1].endN);
        s += "  (Actual below expectation = the opponent keeps/plays that tile more than a uniform guess assumes; the search samples uniformly.)\n";

        // ---- game end
        head("GAME END (no rack penalty is applied in Sim)");
        add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
        two("Avg rack points left at game end", "%.1f", left[0] / std::max(1LL, gN), gN, left[1] / std::max(1LL, gN), gN);
        two("Went out (rack empty at game end)", "%.1f%%", 100.0 * outN[0] / std::max(1LL, gN), gN, 100.0 * outN[1] / std::max(1LL, gN), gN);
        add("  Nobody went out: %lld of %lld games\n", noOut, gN);

        // ---- Eval calibration
        if (allReg.n > 0) {
            head("EVAL CALIBRATION (AB value of the chosen move vs realised final margin, pts, mover's view)");
            add("  %-14s%8s %10s %10s %10s %8s %6s\n", "", "n", "avg pred", "avg real", "real-pred", "slope", "corr");
            auto line = [&](const char* label, const Reg& g) {
                if (g.n < 2) { add("  %-14s%8lld\n", label, g.n); return; }
                const double n = (double)g.n, cov = n * g.sxy - g.sx * g.sy;
                const double vx = n * g.sxx - g.sx * g.sx, vy = n * g.syy - g.sy * g.sy;
                add("  %-14s%8lld %10.1f %10.1f %10.1f %8.2f %6.2f\n", label, g.n, g.sx / n, g.sy / n, (g.sy - g.sx) / n,
                    vx > 0 ? cov / vx : 0.0, (vx > 0 && vy > 0) ? cov / std::sqrt(vx * vy) : 0.0);
            };
            static const char* kB[5] = { "pred < -60", "-60 .. -20", "-20 .. 20", "20 .. 60", "pred >= 60" };
            for (int k = 0; k < 5; ++k) line(kB[k], bk[k]);
            line("ALL", allReg);
            line("opening", phReg[0]); line("midgame", phReg[1]); line("endgame", phReg[2]);
            s += "  (slope near 1 and real-pred near 0 = well calibrated. Slope > 1: Eval is underconfident. Slope < 1: overconfident.)\n";
        }
        (void)games;
    }

    void summarize() {
        int wins[2] = {0, 0}, ties = 0, games = 0, firstGames[2] = {0, 0}, firstWins[2] = {0, 0};
        double phase[3] = {0, 0, 0};
        BotAgg ag[2];
        std::vector<std::string> swapLog;

        for (int g = 0; g < (int)res.size(); ++g) {
            const GameRes& r = res[g];
            if (!r.ok) continue;
            ++games; ++firstGames[r.flip];
            if (r.s[0] == r.s[1]) ++ties;
            else { const int w = r.s[0] > r.s[1] ? 0 : 1; ++wins[w]; if (w == r.flip) ++firstWins[w]; }

            const auto& tv = r.tv;
            for (size_t i = 0; i < tv.size(); ++i) {
                const SimGame::Turn& t = tv[i];
                const int bot = t.player ^ r.flip;
                BotAgg& a = ag[bot];
                const int regret = t.greedyBest - t.score;   // a swap or pass forgoes the full Greedy best
                ++a.turns; a.scoreSum += t.score; a.regretAll += regret;
                phase[t.bagBefore > 50 ? 0 : t.bagBefore >= 8 ? 1 : 2] += (bot == 0 ? t.score : -t.score);

                // lanes available this turn
                a.openBig += t.openBigBefore; a.nMoves += t.nMoves;
                if (t.n40 > 0) ++a.has40;
                if (t.n50 > 0) ++a.has50;
                const int pb[4] = { t.bestTW, t.bestDW, t.bestTL, t.bestDL };
                for (int k = 0; k < 4; ++k) if (pb[k] > 0) { a.premBest[k] += pb[k]; ++a.premTurns[k]; }

                if (!t.swapped && !t.passed) {
                    ++a.moves; a.tiles += t.tilesUsed; a.bingos += t.bingo ? 1 : 0;
                    a.regretMove += regret;
                    if (regret <= 0) ++a.matches;
                    if (t.usedBig > 0) ++a.usedBig;
                    if (i + 2 < tv.size()) {                       // this bot's next turn
                        const int lv = t.leaveKept;
                        const int bk = lv < -50 ? 0 : lv < 0 ? 1 : lv < 50 ? 2 : lv < 100 ? 3 : 4;
                        ++a.bucketN[bk]; a.bucketSum[bk] += tv[i + 2].score;
                    }
                    if (i + 1 < tv.size()) {                       // what the move gave the opponent
                        const int d = tv[i + 1].openBigBefore - t.openBigBefore;
                        const int lb = d < 0 ? 0 : d == 0 ? 1 : d == 1 ? 2 : 3;
                        const int delta = tv[i + 1].greedyBest - t.oppBestBefore;
                        ++a.laneN[lb]; a.laneSum[lb] += delta;
                        a.oppBefore += t.oppBestBefore; a.oppAfter += tv[i + 1].greedyBest; ++a.oppN;
                    }
                } else if (t.swapped) {
                    ++a.swaps; a.swapTurn += t.turnNumber; a.swapForgone += t.greedyBest; a.swapBag += t.bagBefore;
                    if (swapLog.size() < 20) {
                        char b[200];
                        std::snprintf(b, sizeof b, "  game %d turn %d Bot %d: threw %s, kept %s (Greedy best %d, bag %d)",
                                      g, t.turnNumber, bot + 1, t.swapLetters.c_str(), t.kept.c_str(),
                                      t.greedyBest, t.bagBefore);
                        swapLog.push_back(b);
                    }
                } else {
                    ++a.passes;
                }

                a.leaveSum += t.leaveKept / 10.0;
                a.keptSize += t.keptSize; a.dups += t.keptDups; a.vowels += t.keptVowels;
                if (t.rackS > 0)     { ++a.sTurns; if (t.keptS > 0)     ++a.sKept; }
                if (t.rackBlank > 0) { ++a.bTurns; if (t.keptBlank > 0) ++a.bKept; }

                // search statistics
                const SearchStat& q = t.ss;
                if (q.used) {
                    ++a.srN; a.srRoot += q.rootMoves; a.srTime += q.timeMs;
                    if (q.fellBack) ++a.srFell;
                    if (q.limitMs > 0) { ++a.srTimed; a.srTimedT += q.timeMs; a.srLimit += q.limitMs; }
                    if (q.algo == 0) {
                        ++a.srAB; a.srDepth += q.depth; a.srNodes += (double)q.nodes; a.srSamples += q.samples;
                        if (!q.fellBack) {
                            if (q.greedyRank >= 0) { ++a.srGapN; a.srGap += (q.value - q.greedyValue) / 10.0; a.srRank += q.greedyRank + 1; }
                            else ++a.srNoGreedy;
                        }
                    } else {
                        ++a.srMC; a.srSims += (double)q.nodes;
                        if (!q.fellBack) {
                            a.mcWin += q.value / 10.0; ++a.mcWinN;
                            if (q.greedyRank >= 0) { a.mcGWin += q.greedyValue / 10.0; ++a.mcGN; }
                        }
                    }
                }
            }
        }

        // ---- paired statistics: average the two seatings of each deal
        std::vector<double> pm;
        for (int k = 0; 2 * k + 1 < (int)res.size(); ++k)
            if (res[2 * k].ok && res[2 * k + 1].ok)
                pm.push_back(0.5 * ((res[2 * k].s[0] - res[2 * k].s[1]) + (res[2 * k + 1].s[0] - res[2 * k + 1].s[1])));
        const int P = (int)pm.size();
        double pmean = 0, psd = 0;
        for (double v : pm) pmean += v;
        if (P) pmean /= P;
        if (P > 1) { for (double v : pm) psd += (v - pmean) * (v - pmean); psd = std::sqrt(psd / (P - 1)); }
        const double pci = P > 1 ? 1.96 * psd / std::sqrt((double)P) : 0.0;
        auto need = [&](double X) { return psd > 0 ? 2 * (int)std::ceil(std::pow(1.96 * psd / X, 2)) : 0; };

        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::string s;
        auto add = [&](const char* fmt, auto... a) { char b[512]; std::snprintf(b, sizeof b, fmt, a...); s += b; };
        auto dv = [](double a, double b) { return b > 0 ? a / b : 0.0; };
        auto head = [&](const char* title) { add("\n== %s ==\n", title); };
        auto cols = [&]() { add("  %-36s%10s   %10s\n", "", "Bot 1", "Bot 2"); };
        auto row2 = [&](const char* label, const char* f, double a, double b) {
            add((std::string("  %-36s") + f + "   " + f + "\n").c_str(), label, a, b);
        };
        // Like row2, but prints "-" for a bot whose sample count is zero.
        auto rowN = [&](const char* label, const char* f, double a, long long na, double b, long long nb) {
            char x[40], y[40];
            if (na > 0) std::snprintf(x, sizeof x, f, a); else std::snprintf(x, sizeof x, "-");
            if (nb > 0) std::snprintf(y, sizeof y, f, b); else std::snprintf(y, sizeof y, "-");
            add("  %-36s%10s   %10s\n", label, x, y);
        };
        const double n = games;

        add("BOT VS BOT - %d game%s%s\n", games, games == 1 ? "" : "s", games < total ? " (cancelled early)" : "");
        add("Run %s   Eval fingerprint %s   Cores %u\n", runId.c_str(), EvalFingerprint().c_str(),
            std::thread::hardware_concurrency());
        add("Base seed: %llu (games 2k and 2k+1 share a deal, sides swapped)\n", (unsigned long long)baseSeed);
        if (parallel > 1) add("Parallel games: %d (each search forced to 1 thread)\n", parallel);
        else              s += "Parallel games: 1 (search threads as configured)\n";
        s += "Bot 1: " + DescribeMover(cfg[0]) + "\n";
        s += "Bot 2: " + DescribeMover(cfg[1]) + "\n";

        if (games == 0) {
            s += "\nNo games completed.\n";
        } else {
            // ---------------------------------------------------------- results
            head("RESULTS");
            add("Wins: Bot 1 %d (%.1f%%)   Bot 2 %d (%.1f%%)   ties %d\n",
                wins[0], 100.0 * wins[0] / n, wins[1], 100.0 * wins[1] / n, ties);
            const double z = 1.96, p = (wins[0] + 0.5 * ties) / n, den = 1 + z * z / n;
            const double ctr = (p + z * z / (2 * n)) / den;
            const double hw  = z * std::sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / den;
            add("Bot 1 score rate (tie = 0.5): %.1f%%  [Wilson 95%%: %.1f%% .. %.1f%%]\n",
                100 * p, 100 * (ctr - hw), 100 * (ctr + hw));
            if (P > 1) {
                add("Margin Bot 1 - Bot 2, paired by deal: %+.1f +/- %.1f (95%% CI, %d pairs, pair sd %.1f)\n",
                    pmean, pci, P, psd);
                add("Games needed for a +/-10 pt CI: ~%d   for +/-5 pt: ~%d\n", need(10), need(5));
            } else {
                s += "Margin: need at least 2 complete pairs (use an even number of games).\n";
            }
            add("First mover: Bot 1 won %d of %d as first, Bot 2 won %d of %d as first\n",
                firstWins[0], firstGames[0], firstWins[1], firstGames[1]);
            add("Avg moves/game: %.1f   Wall time per game: %.1f s   Total: %.0f s\n",
                dv((double)(ag[0].turns + ag[1].turns), n), secs / n, secs);

            // ---------------------------------------------------------- scoring
            head("SCORING (per bot)"); cols();
            row2("Avg final score", "%10.1f", dv(ag[0].scoreSum, n), dv(ag[1].scoreSum, n));
            row2("Points per turn", "%10.1f", dv(ag[0].scoreSum, ag[0].turns), dv(ag[1].scoreSum, ag[1].turns));
            row2("Points per scoring move", "%10.1f", dv(ag[0].scoreSum, ag[0].moves), dv(ag[1].scoreSum, ag[1].moves));
            row2("Tiles played per scoring move", "%10.2f", dv(ag[0].tiles, ag[0].moves), dv(ag[1].tiles, ag[1].moves));
            row2("Bingos per game", "%10.2f", dv(ag[0].bingos, n), dv(ag[1].bingos, n));
            row2("Turns per game", "%10.1f", dv(ag[0].turns, n), dv(ag[1].turns, n));

            // ---------------------------------------------------------- decision quality
            head("DECISION QUALITY vs Greedy (regret = Greedy best - points scored)"); cols();
            row2("Turns matching Greedy's best score", "%9.1f%%", 100 * dv(ag[0].matches, ag[0].turns), 100 * dv(ag[1].matches, ag[1].turns));
            row2("Avg regret on scoring moves (pts)", "%10.2f", dv(ag[0].regretMove, ag[0].moves), dv(ag[1].regretMove, ag[1].moves));
            row2("Total regret per game (pts)", "%10.1f", dv(ag[0].regretAll, n), dv(ag[1].regretAll, n));
            s += "  (Greedy should show 100% / 0 / 0. A negative regret would mean a bug.)\n";

            // ---------------------------------------------------------- swaps
            head("SWAPS"); cols();
            row2("Swaps per game", "%10.2f", dv(ag[0].swaps, n), dv(ag[1].swaps, n));
            row2("Passes per game", "%10.2f", dv(ag[0].passes, n), dv(ag[1].passes, n));
            row2("Avg turn number of a swap", "%10.1f", dv(ag[0].swapTurn, ag[0].swaps), dv(ag[1].swapTurn, ag[1].swaps));
            row2("Avg Greedy best forgone at a swap", "%10.1f", dv(ag[0].swapForgone, ag[0].swaps), dv(ag[1].swapForgone, ag[1].swaps));
            row2("Avg bag size at a swap", "%10.1f", dv(ag[0].swapBag, ag[0].swaps), dv(ag[1].swapBag, ag[1].swaps));
            if (!swapLog.empty()) {
                s += "  First swaps:\n";
                for (const auto& l : swapLog) s += l + "\n";
            }

            // ---------------------------------------------------------- leave
            head("LEAVE (tiles kept after each turn, before drawing)"); cols();
            row2("Avg leave value (pts)", "%10.2f", dv(ag[0].leaveSum, ag[0].turns), dv(ag[1].leaveSum, ag[1].turns));
            row2("Avg tiles kept", "%10.2f", dv(ag[0].keptSize, ag[0].turns), dv(ag[1].keptSize, ag[1].turns));
            row2("Avg duplicate tiles kept", "%10.2f", dv(ag[0].dups, ag[0].turns), dv(ag[1].dups, ag[1].turns));
            row2("Vowel share of kept tiles", "%9.1f%%", 100 * dv(ag[0].vowels, ag[0].keptSize), 100 * dv(ag[1].vowels, ag[1].keptSize));
            row2("S kept when held", "%9.1f%%", 100 * dv(ag[0].sKept, ag[0].sTurns), 100 * dv(ag[1].sKept, ag[1].sTurns));
            row2("Blank kept when held", "%9.1f%%", 100 * dv(ag[0].bKept, ag[0].bTurns), 100 * dv(ag[1].bKept, ag[1].bTurns));
            s += "  Next-turn score by kept-leave value (pts), scoring moves only:\n";
            add("  %-14s%8s %8s      %8s %8s\n", "leave", "B1 n", "B1 avg", "B2 n", "B2 avg");
            static const char* kBucket[5] = { "< -5", "-5 .. 0", "0 .. 5", "5 .. 10", ">= 10" };
            for (int b = 0; b < 5; ++b)
                add("  %-14s%8lld %8.1f      %8lld %8.1f\n", kBucket[b],
                    ag[0].bucketN[b], dv((double)ag[0].bucketSum[b], (double)ag[0].bucketN[b]),
                    ag[1].bucketN[b], dv((double)ag[1].bucketSum[b], (double)ag[1].bucketN[b]));

            // ---------------------------------------------------------- lanes
            head("LANES AND WHAT YOUR MOVE GIVES THE OPPONENT"); cols();
            row2("Open DW/TW anchors at turn start", "%10.2f", dv(ag[0].openBig, ag[0].turns), dv(ag[1].openBig, ag[1].turns));
            row2("Moves available per turn", "%10.1f", dv(ag[0].nMoves, ag[0].turns), dv(ag[1].nMoves, ag[1].turns));
            row2("Turns with a >= 40 move available", "%9.1f%%", 100 * dv(ag[0].has40, ag[0].turns), 100 * dv(ag[1].has40, ag[1].turns));
            row2("Turns with a >= 50 move available", "%9.1f%%", 100 * dv(ag[0].has50, ag[0].turns), 100 * dv(ag[1].has50, ag[1].turns));
            static const char* kPrem[4] = { "TW", "DW", "TL", "DL" };
            for (int k = 0; k < 4; ++k) {
                const std::string lab = std::string("Best move covering ") + kPrem[k] + " (when one exists)";
                rowN(lab.c_str(), "%.1f", dv(ag[0].premBest[k], (double)ag[0].premTurns[k]), ag[0].premTurns[k],
                     dv(ag[1].premBest[k], (double)ag[1].premTurns[k]), ag[1].premTurns[k]);
            }
            row2("Scoring moves covering a DW/TW", "%9.1f%%", 100 * dv(ag[0].usedBig, ag[0].moves), 100 * dv(ag[1].usedBig, ag[1].moves));
            rowN("Opp best reply before your move (pts)", "%.1f", dv(ag[0].oppBefore, (double)ag[0].oppN), ag[0].oppN,
                 dv(ag[1].oppBefore, (double)ag[1].oppN), ag[1].oppN);
            rowN("Opp best reply after your move (pts)", "%.1f", dv(ag[0].oppAfter, (double)ag[0].oppN), ag[0].oppN,
                 dv(ag[1].oppAfter, (double)ag[1].oppN), ag[1].oppN);
            s += "  Opponent's best reply, change after your scoring move, by change in open DW/TW anchors:\n";
            add("  %-14s%8s %8s      %8s %8s\n", "lanes", "B1 n", "B1 avg", "B2 n", "B2 avg");
            static const char* kLane[4] = { "closed", "unchanged", "+1 opened", "+2 or more" };
            for (int b = 0; b < 4; ++b)
                add("  %-14s%8lld %8.1f      %8lld %8.1f\n", kLane[b],
                    ag[0].laneN[b], dv(ag[0].laneSum[b], (double)ag[0].laneN[b]),
                    ag[1].laneN[b], dv(ag[1].laneSum[b], (double)ag[1].laneN[b]));
            s += "  (Opponent's reply uses their true rack. Use this to check what the lanes term in Eval should be worth.)\n";

            // ---------------------------------------------------------- search
            if (ag[0].srN + ag[1].srN > 0) {
                head("SEARCH (turns where the bot searched)"); cols();
                row2("Search turns", "%10.0f", (double)ag[0].srN, (double)ag[1].srN);
                rowN("Fell back to Greedy (no result)", "%.1f%%", 100 * dv((double)ag[0].srFell, (double)ag[0].srN), ag[0].srN,
                     100 * dv((double)ag[1].srFell, (double)ag[1].srN), ag[1].srN);
                rowN("Avg time used (s)", "%.2f", dv(ag[0].srTime, (double)ag[0].srN) / 1000.0, ag[0].srN,
                     dv(ag[1].srTime, (double)ag[1].srN) / 1000.0, ag[1].srN);
                rowN("Time used / limit (timed searches)", "%.1f%%", 100 * dv(ag[0].srTimedT, ag[0].srLimit), ag[0].srTimed,
                     100 * dv(ag[1].srTimedT, ag[1].srLimit), ag[1].srTimed);
                rowN("Avg root moves", "%.1f", dv(ag[0].srRoot, (double)ag[0].srN), ag[0].srN,
                     dv(ag[1].srRoot, (double)ag[1].srN), ag[1].srN);
                s += "  Alpha-beta:\n";
                rowN("Avg depth reached", "%.2f", dv(ag[0].srDepth, (double)ag[0].srAB), ag[0].srAB,
                     dv(ag[1].srDepth, (double)ag[1].srAB), ag[1].srAB);
                rowN("Avg nodes per move", "%.0f", dv(ag[0].srNodes, (double)ag[0].srAB), ag[0].srAB,
                     dv(ag[1].srNodes, (double)ag[1].srAB), ag[1].srAB);
                rowN("Avg determinizations", "%.1f", dv(ag[0].srSamples, (double)ag[0].srAB), ag[0].srAB,
                     dv(ag[1].srSamples, (double)ag[1].srAB), ag[1].srAB);
                rowN("Value: chosen - Greedy-equivalent (pts)", "%.2f", dv(ag[0].srGap, (double)ag[0].srGapN), ag[0].srGapN,
                     dv(ag[1].srGap, (double)ag[1].srGapN), ag[1].srGapN);
                rowN("Greedy-equivalent not in root list", "%.1f%%",
                     100 * dv((double)ag[0].srNoGreedy, (double)(ag[0].srGapN + ag[0].srNoGreedy)), ag[0].srGapN + ag[0].srNoGreedy,
                     100 * dv((double)ag[1].srNoGreedy, (double)(ag[1].srGapN + ag[1].srNoGreedy)), ag[1].srGapN + ag[1].srNoGreedy);
                rowN("Avg list position of Greedy-equivalent", "%.1f", dv(ag[0].srRank, (double)ag[0].srGapN), ag[0].srGapN,
                     dv(ag[1].srRank, (double)ag[1].srGapN), ag[1].srGapN);
                s += "  MCTS:\n";
                rowN("Avg simulations per move", "%.0f", dv(ag[0].srSims, (double)ag[0].srMC), ag[0].srMC,
                     dv(ag[1].srSims, (double)ag[1].srMC), ag[1].srMC);
                rowN("Chosen move win rate", "%.1f%%", dv(ag[0].mcWin, (double)ag[0].mcWinN), ag[0].mcWinN,
                     dv(ag[1].mcWin, (double)ag[1].mcWinN), ag[1].mcWinN);
                rowN("Greedy-equivalent win rate", "%.1f%%", dv(ag[0].mcGWin, (double)ag[0].mcGN), ag[0].mcGN,
                     dv(ag[1].mcGWin, (double)ag[1].mcGN), ag[1].mcGN);
            }

            sectionsC(s, games);

            // ---------------------------------------------------------- phases
            head("WHERE GAMES ARE WON (avg margin gained per game by Bot 1 - Bot 2)");
            add("  Opening (bag > 50):  %+.1f\n  Midgame (bag 8-50):  %+.1f\n  Endgame (bag < 8):   %+.1f\n",
                phase[0] / n, phase[1] / n, phase[2] / n);

            // ---------------------------------------------------------- machine-readable line
            s += "\n";
            add("RESULT: run=%s eval=%s games=%d b1=%s b2=%s w1=%d w2=%d ties=%d margin=%+.1f ci=%.1f pairs=%d\n",
                runId.c_str(), EvalFingerprint().c_str(), games, ShortMover(cfg[0]).c_str(),
                ShortMover(cfg[1]).c_str(), wins[0], wins[1], ties, pmean, pci, P);
        }
        writeCsv(s);
        summary = std::move(s);
    }

    // games.csv: one row per game. turns.csv: one row per turn (regression dataset for Eval fitting).
    // opp_best_after and open_big_after come from the NEXT turn's record (-1 = none).
    // search_value / pi_*: AB = tenths of a point, MCTS search_value = win rate x 1000.
    // greedy_rank = position in the search's ranked list. poss_racks = -1 when the bag has > 7 tiles.
    void writeCsv(std::string& s) {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path dir(DIAG_DATA_DIR);
        fs::create_directories(dir, ec);
        const fs::path gp = dir / ("run_" + runId + "_games.csv");
        const fs::path tp = dir / ("run_" + runId + "_turns.csv");
        std::ofstream gf(gp), tf(tp);
        if (!gf || !tf) { s += "\nCSV export FAILED: " + dir.string() + "\n"; return; }

        gf << "run,game,deal,first_bot,bot1_score,bot2_score,margin,turns,swaps_bot1,swaps_bot2,passes_bot1,passes_bot2,winner,"
              "left_bot1,left_bot2,out_bot\n";
        tf << "run,game,deal,bot,bot_cfg,player,turn,type,word,row,col,dir,score,greedy_best,tiles_used,bingo,"
              "bag_before,score_diff_before,rack_leave_before,opp_leave_before,leave_kept,kept_size,kept_s,"
              "kept_blank,kept_vowels,kept_dups,thrown,kept,final_margin,"
              "opp_best_before,opp_best_after,open_big_before,open_big_after,n_moves,n40,n50,"
              "best_tw,best_dw,best_tl,best_dl,used_big,"
              "search_algo,depth,nodes,samples,root_moves,time_ms,limit_ms,fell_back,search_value,greedy_value,greedy_rank,"
              "opp_rack,opp_s,opp_blank,pool_size,exp_s,exp_blank,poss_racks,pi_used,pi_found,pi_best,pi_chosen,pi_depth\n";
        const std::string name[2] = { ShortMover(cfg[0]), ShortMover(cfg[1]) };

        for (int g = 0; g < (int)res.size(); ++g) {
            const GameRes& r = res[g];
            if (!r.ok) continue;
            int sw[2] = {0, 0}, pa[2] = {0, 0};
            for (size_t i = 0; i < r.tv.size(); ++i) {
                const SimGame::Turn& t = r.tv[i];
                const SimGame::Turn* nx = i + 1 < r.tv.size() ? &r.tv[i + 1] : nullptr;
                const SearchStat& q = t.ss;
                const int bot = t.player ^ r.flip;
                const bool mv = !t.swapped && !t.passed;
                if (t.swapped) ++sw[bot]; else if (t.passed) ++pa[bot];
                tf << runId << ',' << g << ',' << (g >> 1) << ',' << (bot + 1) << ',' << name[bot] << ','
                   << t.player << ',' << t.turnNumber << ','
                   << (t.swapped ? "swap" : t.passed ? "pass" : "move") << ','
                   << (mv ? t.move.word : std::string()) << ','
                   << (mv ? (int)t.move.row : -1) << ',' << (mv ? (int)t.move.col : -1) << ','
                   << (mv ? (t.move.is_descending ? 'V' : 'H') : '-') << ','
                   << t.score << ',' << t.greedyBest << ',' << t.tilesUsed << ',' << (int)t.bingo << ','
                   << t.bagBefore << ',' << t.scoreDiffBefore << ',' << t.rackLeaveBefore << ',' << t.oppLeaveBefore << ','
                   << t.leaveKept << ',' << t.keptSize << ',' << t.keptS << ',' << t.keptBlank << ','
                   << t.keptVowels << ',' << t.keptDups << ',' << t.swapLetters << ',' << t.kept << ','
                   << (bot == 0 ? r.s[0] - r.s[1] : r.s[1] - r.s[0]) << ','
                   << t.oppBestBefore << ',' << (nx ? nx->greedyBest : -1) << ','
                   << t.openBigBefore << ',' << (nx ? nx->openBigBefore : -1) << ','
                   << t.nMoves << ',' << t.n40 << ',' << t.n50 << ','
                   << t.bestTW << ',' << t.bestDW << ',' << t.bestTL << ',' << t.bestDL << ',' << t.usedBig << ','
                   << (q.used ? (q.algo == 0 ? "ab" : "mcts") : "") << ','
                   << q.depth << ',' << q.nodes << ',' << q.samples << ',' << q.rootMoves << ','
                   << q.timeMs << ',' << q.limitMs << ',' << (int)q.fellBack << ','
                   << q.value << ',' << q.greedyValue << ',' << q.greedyRank << ','
                   << t.oppRack << ',' << t.oppS << ',' << t.oppBlank << ',' << t.poolSize << ','
                   << t.expS << ',' << t.expBlank << ',' << t.possRacks << ','
                   << (int)t.pi.used << ',' << (int)t.pi.found << ',' << t.pi.best << ',' << t.pi.chosen << ',' << t.pi.depth << '\n';
            }
            gf << runId << ',' << g << ',' << (g >> 1) << ',' << (r.flip == 0 ? 1 : 2) << ','
               << r.s[0] << ',' << r.s[1] << ',' << (r.s[0] - r.s[1]) << ',' << r.tv.size() << ','
               << sw[0] << ',' << sw[1] << ',' << pa[0] << ',' << pa[1] << ','
               << (r.s[0] == r.s[1] ? 0 : r.s[0] > r.s[1] ? 1 : 2) << ','
               << r.left[0] << ',' << r.left[1] << ',' << (r.out < 0 ? 0 : r.out + 1) << '\n';
        }
        s += "\nData files:\n  " + gp.string() + "\n  " + tp.string() + "\n";
    }
};

// ================================================================ Sim UI state
struct SimUiState {
    float  speedMs = 60.0f;
    double lastStep = 0.0;
    bool   autoRunning = false;
    bool   stepPending = false;   // a single Step is waiting for its search to finish
    bool   replay = false;        // loaded from a file: view only
    bool   started = false;
    int    nGames = 20;           // batch size
    BatchRun batch;
    SimGame sim;
    int    nParallel = 1;         // games run at once (1 = sequential, multi-thread search)
    bool   perfInfo = false;      // batch: also run the perfect-information reference search
};

static SimUiState g_simUi;

// ================================================================ Sim UI
static const Snapshot& SimViewSnapshot() {
    return g_simState.turnStart();
}
// The sim view is always drawn from snapshots (initial / history entries), never from the
// live SimGame, so stepping the sim never changes what a history view shows.
static void SimSyncView() { ApplyVisible(g_simState, SimViewSnapshot()); }

static void SimGoTo(int target) {
    g_simState.cursor = std::clamp(target, 0, (int)g_simState.history.size());
    SimSyncView();
}

static void SimPushHistory(const SimGame::Turn& t) {
    Entry e;
    static_cast<Snapshot&>(e) = MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                                             g_simUi.sim.bag, g_simUi.sim.score[0], g_simUi.sim.score[1]);
    e.turnNumber = t.turnNumber;
    e.player = t.player;
    if (t.swapped)      e.label = "SWAP " + t.swapLetters;
    else if (t.passed)  e.label = "PASS";
    else                e.label = FormatMoveLabel(t.move.word, t.move.row, t.move.col, t.move.is_descending, t.score);
    g_simState.history.push_back(std::move(e));
    g_simState.cursor = (int)g_simState.history.size();
    SimSyncView();
}

static bool SimStepAndRecord() {
    if (g_simUi.sim.gameOver) return false;
    const int before = (int)g_simUi.sim.turns.size();
    if (!g_simUi.sim.step()) return false;
    if ((int)g_simUi.sim.turns.size() > before) SimPushHistory(g_simUi.sim.turns.back());
    return true;
}

// Advance the sim one turn using the async step machine. Returns:
//   true  = a turn was completed (history pushed)
//   false = still thinking, or game over
static bool SimTryAdvanceAsync() {
    auto& sim = g_simUi.sim;
    if (sim.gameOver) return false;

    const int before = (int)sim.turns.size();
    const int p = (int)(sim.turns.size() & 1);

    if (sim.movers[p].kind == MoverKind::Greedy) {
        return SimStepAndRecord();
    }

    if (!sim.movers[p].search.running() && !sim.movers[p].search.haveResult) {
        sim.beginStep();
    }
    const auto st = sim.pollStep();
    if (st == SimGame::StepState::Done) {
        if ((int)sim.turns.size() > before) SimPushHistory(sim.turns.back());
        return true;
    }
    return false;
}

static void SimReset() {
    std::random_device rd;
    const uint64_t seed = ((uint64_t)rd() << 32) ^ rd();
    g_simUi.sim.reset(g_gaddag, seed);
    g_simState.history.clear();
    g_simState.cursor = 0;
    g_simState.initial = MakeSnapshot(g_simUi.sim.board, g_simUi.sim.trays[0], g_simUi.sim.trays[1],
                                      g_simUi.sim.bag, g_simUi.sim.score[0], g_simUi.sim.score[1]);
    SimSyncView();
    g_simUi.started = true;
    g_simUi.replay = false;
    g_simUi.autoRunning = false;
    g_simUi.stepPending = false;
    g_simUi.lastStep = ImGui::GetTime();
}

static void BatchStart(int n, int parallel) {
    BatchRun& b = g_simUi.batch;
    if (b.running || !g_gaddag) return;
    b.stop();                                    // reaps finished threads, if any
    b.cancel = false; b.finished = false; b.done = 0; b.next = 0;
    b.total = std::clamp(n, 1, 1000);
    if (b.total > 1 && (b.total & 1)) ++b.total; // even, so every deal is played from both seats
    const int cores = std::max(1, (int)std::thread::hardware_concurrency());
    b.parallel = std::clamp(parallel, 1, std::min(cores, b.total));
    b.perfInfo = g_simUi.perfInfo;
    b.summary.clear(); b.showSummary = false;
    for (int i = 0; i < 2; ++i) {
        CopyMoverSettings(b.cfg[i], g_simUi.sim.movers[i]);
        if (b.parallel > 1) b.cfg[i].search.threads = 1;   // games are the parallelism
    }
    std::random_device rd;
    b.baseSeed = ((uint64_t)rd() << 32) ^ rd();
    b.runId = TimeStamp();
    b.res.assign(b.total, BatchRun::GameRes{});
    b.t0 = std::chrono::steady_clock::now();
    g_simUi.autoRunning = false; g_simUi.stepPending = false;
    b.running = true;
    b.alive = b.parallel;
    for (int i = 0; i < b.parallel; ++i) b.pool.emplace_back([&b] { b.worker(g_gaddag); });
}

// Call once per frame: reaps the worker when it finishes and opens the summary.
static void BatchPoll() {
    BatchRun& b = g_simUi.batch;
    if (b.running && b.finished) { b.stop(); b.running = false; b.showSummary = true; }
}

static void DrawBatchPopup() {
    BatchRun& b = g_simUi.batch;
    if (!b.running && !b.showSummary) return;
    CenterNextWindow();
    ImGui::Begin(b.running ? "Running batch##sim" : "Batch results##sim", nullptr,
                 POPUP_FLAGS | ImGuiWindowFlags_AlwaysAutoResize);
    if (b.running) {
        const int done = b.done;
        const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - b.t0).count();
        ImGui::Text("Game %d / %d    %.0f s elapsed", std::min(done + 1, b.total), b.total, el);
        ImGui::ProgressBar(b.total ? (float)done / (float)b.total : 0.0f, ImVec2(420, 0));
        if (ImGui::Button("Cancel", ImVec2(100, 28))) b.cancel = true;
    } else {
        ImGui::InputTextMultiline("##sum", b.summary.data(), b.summary.size() + 1, ImVec2(900, 520),
                                  ImGuiInputTextFlags_ReadOnly);
        static double copiedAt = -10.0;
        if (ImGui::Button("Copy", ImVec2(100, 28))) { ImGui::SetClipboardText(b.summary.c_str()); copiedAt = ImGui::GetTime(); }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 28))) b.showSummary = false;
        if (ImGui::GetTime() - copiedAt < 2.0) { ImGui::SameLine(); ImGui::TextDisabled("Copied."); }
    }
    ImGui::End();
}

static void SimTopOnBack() {
    BatchRun& b = g_simUi.batch;
    b.stop(); b.running = false; b.showSummary = false;
    LeaveToTitle();
}
static void SimTopOnRematch() { SimReset(); }

static void SimTopExtra() {
    const bool batching = g_simUi.batch.running;
    const bool live = !g_simUi.replay && !g_simUi.sim.gameOver && !batching;
    const bool thinking = g_simUi.sim.movers[(int)(g_simUi.sim.turns.size() & 1)].search.running();

    if (!live || thinking) ImGui::BeginDisabled();
    if (ImGui::Button(g_simUi.autoRunning ? "Pause" : "Run")) {
        g_simUi.autoRunning = !g_simUi.autoRunning;
        if (g_simUi.autoRunning) {
            SimGoTo((int)g_simState.history.size());
            g_simUi.lastStep = ImGui::GetTime();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        g_simUi.autoRunning = false;
        SimGoTo((int)g_simState.history.size());
        g_simUi.stepPending = true;      // DrawSimMode pumps it until one turn completes
    }
    if (!live || thinking) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12, 0));
    ImGui::SameLine();
    if (batching || thinking || !g_gaddag) ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(70);
    ImGui::InputInt("Games", &g_simUi.nGames, 0, 0);
    g_simUi.nGames = std::clamp(g_simUi.nGames, 1, 1000);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    ImGui::InputInt("Parallel", &g_simUi.nParallel, 0, 0);
    g_simUi.nParallel = std::clamp(g_simUi.nParallel, 1, std::max(1, (int)std::thread::hardware_concurrency()));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Games played at once.\n"
            "1 = one game at a time, each search uses its configured threads.\n"
            ">1 = that many games at once, each search forced to 1 thread.\n"
            "Keep at or below your core count: timed searches lose depth when oversubscribed.");
    ImGui::SameLine();
    ImGui::Checkbox("Perfect-info", &g_simUi.perfInfo);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Batch only. After each search turn, re-run alpha-beta with the opponent's TRUE rack\n"
            "known and record how much the chosen move loses against it.\n"
            "Roughly doubles the time of every search turn.");
    ImGui::SameLine();
    if (ImGui::Button("Batch")) BatchStart(g_simUi.nGames, g_simUi.nParallel);
    if (batching || thinking || !g_gaddag) ImGui::EndDisabled();
}

static void DrawSimTop() {
    TopStripCallbacks cb;
    cb.onBack    = SimTopOnBack;
    cb.onRematch = SimTopOnRematch;
    cb.extra     = SimTopExtra;
    cb.modeName  = "Bot vs Bot";
    DrawModeTopStrip(g_simState, cb);
}

// Shown while bot `p` is searching: Stop plays the best move found so far, then halts Run.
static void DrawBotThinkingBar(int p) {
    BotPanel& bp = g_simUi.sim.movers[p].search;
    if (!bp.running()) return;
    const double sec = bp.det ? bp.det->elapsedMs() / 1000.0
                     : bp.mcts ? bp.mcts->elapsedMs() / 1000.0 : 0.0;
    if (ImGui::Button("Stop", ImVec2(80, 28))) {
        g_simUi.autoRunning = false;
        g_simUi.stepPending = true;   // the pump finishes this turn once the search reports
        bp.cancel();
    }
    ImGui::SameLine();
    ImGui::Text("Thinking... %.1f s", sec);
    ImGui::Separator();
}

static void DrawSimCream() {
    PushCreamStyle();
    ImGui::Begin("Cream##sim", nullptr, PANEL_FLAGS);

    DrawBoardAndBag(BoardKind::Sim, g_simState, false, SnapshotBagView(SimViewSnapshot()));

    ImGui::Separator();

    const float trayCell = 46.0f, trayGap = 8.0f;
    const float trayW = trayCell * kRackSize + trayGap * (kRackSize - 1);
    const float availW = ImGui::GetContentRegionAvail().x;
    const float baseX = ImGui::GetCursorPosX();
    const float scoreRowTop = ImGui::GetCursorPosY();
    const float trayRowTop = scoreRowTop + ImGui::GetTextLineHeight() * 2.0f;

    // Score above its tray, left-aligned to the tray.
    auto drawPlayer = [&](const char* name, int score, const Tray& tray, float x) {
        ImGui::SetCursorPosY(scoreRowTop);
        ImGui::SetCursorPosX(x);
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("%s: %d", name, score);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPosY(trayRowTop);
        ImGui::SetCursorPosX(x);
        DrawTrayStrip(TrayLetters(tray), false, nullptr);
    };
    drawPlayer("P1", g_simState.score[0], g_simState.trayA, baseX);
    drawPlayer("P2", g_simState.score[1], g_simState.trayB, baseX + availW - trayW);

    ImGui::Separator();

    // Status text centered.
    const auto& sim = g_simUi.sim;
    const int  p = (int)(sim.turns.size() & 1);
    const bool atEnd = g_simState.atEnd();
    const bool thinking = !g_simUi.replay && !sim.gameOver && sim.movers[p].search.running();
    const char* statusTxt = !atEnd ? "VIEWING HISTORY" :
                            g_simUi.replay ? "REPLAY" :
                            sim.gameOver ? "GAME OVER" :
                            thinking ? (p == 0 ? "P1 THINKING" : "P2 THINKING") :
                            (g_simState.history.empty() ? "READY" : "RUNNING");
    const ImU32 statusCol = !atEnd ? IM_COL32(200, 160, 60, 255) :
                            (g_simUi.replay || sim.gameOver) ? IM_COL32(220, 80, 80, 255) :
                            thinking ? IM_COL32(80, 150, 220, 255) :
                            IM_COL32(80, 180, 80, 255);
    {
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::SetWindowFontScale(1.8f);
        const float tw = ImGui::CalcTextSize(statusTxt).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
        ImGui::TextUnformatted(statusTxt);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::End();
    PopCreamStyle();
}

static void DrawSimMode() {
    if (!g_simUi.started) SimReset();
    BatchPoll();

    if (!BeginModeScreen(AppMode::BotVsBot, kTopSim, g_ui.dockBuiltSim,
                         "Left Panel##sim", "Cream##sim", "Right Panel##sim", 300.0f, 440.0f,
                         DrawSimTop))
        return;

    DrawSimCream();

    MoverConfig* mv = g_simUi.sim.movers;

    ImGui::Begin("Left Panel##sim", nullptr, PANEL_FLAGS);
    ImGui::TextUnformatted("Bot 1");
    ImGui::Separator();
    DrawBotThinkingBar(0);
    DrawBotConfigPanel(mv[0].search, mv[0].search.running(), &mv[0].kind);
    ImGui::End();

    ImGui::Begin("Right Panel##sim", nullptr, PANEL_FLAGS);
    DrawBotThinkingBar(1);   // above the tabs so Stop is visible on either tab
    static int simTab = 0;
    if (ImGui::Button(simTab == 0 ? "[ Bot 2 ]" : "Bot 2")) simTab = 0;
    ImGui::SameLine();
    if (ImGui::Button(simTab == 1 ? "[ Move History ]" : "Move History")) simTab = 1;
    ImGui::Separator();
    if (simTab == 0) {
        DrawBotConfigPanel(mv[1].search, mv[1].search.running(), &mv[1].kind);
    } else {
        const int t = DrawHistoryNavBar(g_simState);
        if (t >= 0) SimGoTo(t);
        ImGui::Separator();
        static size_t seen = 0;
        DrawMoveHistoryTab(g_simState, seen);
    }
    ImGui::End();

    // Pump the sim: Run advances every frame; Step/Stop advance until one turn completes.
    if (g_simUi.replay || g_simUi.sim.gameOver) {
        g_simUi.autoRunning = false;
        g_simUi.stepPending = false;
    } else if ((g_simUi.autoRunning || g_simUi.stepPending) && g_simState.atEnd()) {
        if (SimTryAdvanceAsync()) g_simUi.stepPending = false;
    }

    DrawBatchPopup();
}