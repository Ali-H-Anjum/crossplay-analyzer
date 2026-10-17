#pragma once
// SimGame: two-player bot-vs-bot simulation game logic.
// Self-contained: includes all engine/UI headers it directly depends on.

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/common.h"
#include "det_search.h"
#include "eval.h"
#include "mcts.h"
#include "move_gen.h"
#include "panels/bot_search_panel.h"
#include "score_fast.h"
#include "search.h"
#include "state/board_rules.h"
#include "zobrist.h"

// ================================================================ Mover config
struct MoverConfig {
    MoverKind kind = MoverKind::Greedy;
    BotPanel search;
    MoverConfig() {
        search.abSeconds = 60;
        search.mctsSeconds = 60;
    }
};

// What a rack could do this turn (filled while generating moves).
struct RackScan {
    int best = 0, nMoves = 0, n40 = 0, n50 = 0;
    int bestTW = 0, bestDW = 0, bestTL = 0, bestDL = 0;
};

// What the search reported for the move it chosen.
struct SearchStat {
    bool used = false, fellBack = false;
    int algo = 0;
    int depth = 0, samples = 0, rootMoves = 0, timeMs = 0, limitMs = 0;
    uint64_t nodes = 0;
    int value = 0;
    int greedyRank = -1, greedyValue = 0;
};

static int OpenBigAnchors(const Board& b) {
    int n = 0;
    for (uint8_t i : kBigPremium) {
        if (b.letters_row[i] != EMPTY_SQUARE) continue;
        const int r = i / BOARD_LENGTH, c = i % BOARD_LENGTH;
        const bool touch =
            (r > 0 && b.letters_row[i - BOARD_LENGTH] != EMPTY_SQUARE) ||
            (r < BOARD_LENGTH - 1 && b.letters_row[i + BOARD_LENGTH] != EMPTY_SQUARE) ||
            (c > 0 && b.letters_row[i - 1] != EMPTY_SQUARE) ||
            (c < BOARD_LENGTH - 1 && b.letters_row[i + 1] != EMPTY_SQUARE);
        n += touch;
    }
    return n;
}

struct PerfStat {
    bool used = false, found = false;
    int best = 0, chosen = 0, depth = 0;
};

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
struct SimGame {
    uint64_t seed = 12345;

    Board board{};
    Tray trays[2]{};
    std::vector<Letter> trayOrder[2];
    std::unique_ptr<TileBag> bag;
    uint16_t bagSize = 0;
    int score[2] = {0, 0};
    bool gameOver = false;
    int winner = -1;
    bool finalRoundActive = false;
    bool finalTurnTaken[2] = {false, false};
    int consecutivePasses = 0;
    int consecutiveScoreless = 0;
    bool perfInfo = false;
    const std::atomic<bool>* abortFlag = nullptr;

    struct Turn {
        int turnNumber = 0;
        int player = 0;
        bool passed = false;
        bool swapped = false;
        std::string swapLetters;
        bool isFinal = false;
        Move move{};
        int score = 0;
        int greedyBest = 0;
        int tilesUsed = 0;
        bool bingo = false;
        int bagBefore = 0;
        int scoreDiffBefore = 0;
        int rackLeaveBefore = 0, oppLeaveBefore = 0;
        int leaveKept = 0;
        int keptSize = 0, keptS = 0, keptBlank = 0, keptVowels = 0, keptDups = 0;
        int rackS = 0, rackBlank = 0;
        std::string kept;
        int oppBestBefore = 0;
        int openBigBefore = 0;
        int nMoves = 0, n40 = 0, n50 = 0;
        int bestTW = 0, bestDW = 0, bestTL = 0, bestDL = 0;
        int usedBig = 0;
        SearchStat ss;
        std::string oppRack;
        int oppS = 0, oppBlank = 0, poolSize = 0;
        float expS = 0, expBlank = 0;
        double possRacks = -1;
        PerfStat pi;
    };

    std::vector<Turn> turns;
    Gaddag* gd = nullptr;
    MoverConfig movers[2];
    struct ScoredMove {
        RawMove rm;
        MoveScore sc;
    };
    ScoredMove bestScratch{};

    ~SimGame() = default;

    void reset(Gaddag* g, uint64_t newSeed) {
        gd = g;
        seed = newSeed;
        bag = std::make_unique<TileBag>(seed);
        board = Board{};
        for (int i = 0; i < 2; ++i) {
            trays[i] = Tray{};
            trayOrder[i].clear();
            movers[i].search.shutdown();
            movers[i].search.haveResult = false;
        }
        bag->drawInto(trays[0], 7);
        bag->drawInto(trays[1], 7);
        bagSize = bag->size();
        for (int i = 0; i < 2; ++i) {
            score[i] = 0;
            finalTurnTaken[i] = false;
        }
        gameOver = false;
        winner = -1;
        finalRoundActive = false;
        consecutivePasses = 0;
        consecutiveScoreless = 0;
        turns.clear();
        syncTrayOrderAll();
        rebuildCrossState();
    }

    static void fillKept(Turn& t, const Tray& k) {
        static const int kVowel[] = {0, 4, 8, 14, 20};
        t.leaveKept = leaveValue(k);
        t.keptSize = (int)k.size();
        t.keptS = k.counts[18];
        t.keptBlank = k.counts[BLANK_SLOT];
        for (int v : kVowel) t.keptVowels += k.counts[v];
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
            if (k.counts[i] > 1) t.keptDups += k.counts[i] - 1;
            t.kept.append(k.counts[i], i == BLANK_SLOT ? '?' : (char)('A' + i));
        }
    }

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
        auto tiles = bag->tilesCopy();
        const uint16_t total = bag->totalCount();
        for (int j = 0; j < k; ++j) tiles[total + j] = ret[j];
        bag->restoreFrom(tiles, (uint16_t)(total + k));
        bagSize = bag->size();
        rebuildTrayMirrorFromCounts(p);
        return true;
    }

    void finishTurn(int p, bool found, const Move& mv, int sc, const Tray* swapThrown = nullptr,
                    const RackScan* scan = nullptr, const SearchStat* ss = nullptr,
                    const PerfStat* pi = nullptr) {
        Turn t{};
        t.turnNumber = (int)turns.size() + 1;
        t.player = p;
        t.isFinal = finalRoundActive;
        RackScan own;
        if (!scan) {
            Move gm;
            int gs = 0;
            MoveScore gd{};
            bestMoveFor(p, gm, gs, gd, &own);
            scan = &own;
        }
        RackScan opp;
        {
            Move gm;
            int gs = 0;
            MoveScore gd{};
            bestMoveFor(p ^ 1, gm, gs, gd, &opp);
        }
        t.greedyBest = scan->best;
        t.nMoves = scan->nMoves;
        t.n40 = scan->n40;
        t.n50 = scan->n50;
        t.bestTW = scan->bestTW;
        t.bestDW = scan->bestDW;
        t.bestTL = scan->bestTL;
        t.bestDL = scan->bestDL;
        t.oppBestBefore = opp.best;
        t.openBigBefore = OpenBigAnchors(board);
        t.bagBefore = (int)bagSize;
        t.scoreDiffBefore = score[p] - score[p ^ 1];
        t.rackLeaveBefore = leaveValue(trays[p]);
        t.oppLeaveBefore = leaveValue(trays[p ^ 1]);
        t.rackS = trays[p].counts[18];
        t.rackBlank = trays[p].counts[BLANK_SLOT];
        if (ss) t.ss = *ss;
        if (pi) t.pi = *pi;
        {
            const Tray& o = trays[p ^ 1];
            t.oppS = o.counts[18];
            t.oppBlank = o.counts[BLANK_SLOT];
            for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
                t.oppRack.append(o.counts[i], i == BLANK_SLOT ? '?' : (char)('A' + i));
            const auto pool = unseenPool(board, trays[p]);
            int tot = 0;
            for (uint8_t v : pool) tot += v;
            t.poolSize = tot;
            const int oppN = std::min(kRackSize, tot);
            if (tot > 0) {
                t.expS = (float)oppN * pool[18] / tot;
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
            t.move = mv;
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
                        t.swapLetters.append(swapThrown->counts[i],
                                             i == BLANK_SLOT ? '?' : (char)('A' + i));
                    consecutivePasses = 0;
                    ++consecutiveScoreless;
                }
            }
            if (!swapped) {
                t.passed = true;
                ++consecutivePasses;
                ++consecutiveScoreless;
            }
            fillKept(t, kept);
        }
        turns.push_back(t);
        const bool wasFinal = finalRoundActive;
        if (!finalRoundActive && bagSize == 0) {
            finalRoundActive = true;
            finalTurnTaken[0] = finalTurnTaken[1] = false;
        }
        if (wasFinal) finalTurnTaken[p] = true;
        if (finalRoundActive && finalTurnTaken[0] && finalTurnTaken[1])
            gameOver = true;
        else if (!finalRoundActive && consecutivePasses >= 2)
            gameOver = true;
        else if (consecutiveScoreless >= 6)
            gameOver = true;
        if (gameOver) winner = (score[0] > score[1]) ? 0 : (score[1] > score[0]) ? 1 : -1;
    }

    bool step() {
        if (gameOver) return false;
        const int p = (int)(turns.size() & 1);
        RackScan scan;
        Move mv;
        int sc = 0;
        MoveScore det{};
        const bool found = bestMoveFor(p, mv, sc, det, &scan);
        finishTurn(p, found, mv, sc, nullptr, &scan);
        return true;
    }

    enum class StepState { Idle, Thinking, Done, Failed };

    StepState beginStep() {
        if (gameOver) return StepState::Failed;
        const int p = (int)(turns.size() & 1);
        if (movers[p].kind == MoverKind::Greedy) {
            return step() ? StepState::Done : StepState::Failed;
        }
        BotPanel& bp = movers[p].search;
        if (!bp.running() && !bp.haveResult) {
            const uint64_t s =
                mix64(seed ^ (0xA24BAED4963EE407ull * (uint64_t)(turns.size() + 1)) ^ (uint64_t)p);
            bp.dc.seed = s;
            bp.mc.seed = s;
            bp.start(*gd, board, trays[p]);
        }
        return StepState::Thinking;
    }

    StepState pollStep() {
        if (gameOver) return StepState::Failed;
        const int p = (int)(turns.size() & 1);
        BotPanel& bp = movers[p].search;
        bp.poll();
        if (!bp.haveResult) return StepState::Thinking;
        const bool det = (bp.resultAlgo == 0);
        const SMove* best = nullptr;
        if (det) {
            if (bp.detRes.hasMove) best = &bp.detRes.best;
        } else {
            if (bp.mctsRes.hasMove) best = &bp.mctsRes.best;
        }
        RackScan scan;
        Move gMv;
        int gSc = 0;
        MoveScore gDet{};
        const bool gFound = bestMoveFor(p, gMv, gSc, gDet, &scan);
        SearchStat ss;
        ss.used = true;
        ss.algo = det ? 0 : 1;
        ss.fellBack = (best == nullptr);
        ss.limitMs = det ? (bp.iterative ? bp.abSeconds * 1000 : 0) : bp.mctsSeconds * 1000;
        if (det) {
            const DetResult& r = bp.detRes;
            ss.depth = r.depth;
            ss.nodes = r.nodes;
            ss.samples = r.samples;
            ss.timeMs = r.timeMs;
            ss.rootMoves = (int)r.ranked.size();
            if (best) {
                ss.value = r.value;
                for (size_t i = 0; i < r.ranked.size(); ++i) {
                    const DetMove& d = r.ranked[i];
                    if (d.m.kind == MK_PLACE && scoreRaw(board, d.m.m) == scan.best) {
                        ss.greedyRank = (int)i;
                        ss.greedyValue = (int)std::lround(d.avg);
                        break;
                    }
                }
            }
        } else {
            const MctsResult& r = bp.mctsRes;
            ss.nodes = r.sims;
            ss.timeMs = r.timeMs;
            ss.rootMoves = (int)r.ranked.size();
            if (best) {
                ss.value = (int)std::lround(r.winRate * 1000.0);
                for (size_t i = 0; i < r.ranked.size(); ++i) {
                    const MctsMove& d = r.ranked[i];
                    if (d.m.kind == MK_PLACE && scoreRaw(board, d.m.m) == scan.best) {
                        ss.greedyRank = (int)i;
                        ss.greedyValue = (int)std::lround(d.winRate * 1000.0);
                        break;
                    }
                }
            }
        }
        Move mv;
        int sc = 0;
        bool found = false;
        Tray thrown;
        const Tray* swap = nullptr;
        if (!best) {
            found = gFound;
            mv = gMv;
            sc = gSc;
        } else if (best->kind == MK_PLACE) {
            mv = toMove(best->m);
            sc = scoreRawFull(board, best->m).points;
            found = true;
        } else if (best->kind == MK_SWAP) {
            thrown = det ? bp.detRes.swapThrown : bp.mctsRes.swapThrown;
            swap = &thrown;
        }
        PerfStat pi;
        if (perfInfo) {
            SMove chosen;
            bool have = false;
            if (best) {
                chosen = *best;
                have = true;
            } else if (gFound) {
                chosen.m = bestScratch.rm;
                have = true;
            }
            if (have) pi = perfectRef(p, bp, chosen);
        }
        bp.shutdown();
        bp.haveResult = false;
        finishTurn(p, found, mv, sc, swap, &scan, &ss, &pi);
        return StepState::Done;
    }

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
                    if (board.is_blank.test(idx)) ch = (char)std::tolower((unsigned char)ch);
                }
                out.push_back(' ');
                out.push_back(ch);
                out.push_back(' ');
                if (c < BOARD_LENGTH - 1) out.push_back('|');
            }
            out.push_back('\n');
        }
        return out;
    }

    void rebuildCrossState() {
        AnchorLines al;
        rebuild(board, al, *gd, gd->get_init());
    }

    PerfStat perfectRef(int p, const BotPanel& bp, const SMove& chosen) {
        PerfStat out;
        Board rb = board;
        Position root{};
        rebuild(rb, root.al, *gd, gd->get_init());
        root.b = &rb;
        root.g = gd;
        root.init = &gd->get_init();
        root.tray[0] = trays[p];
        SearchConfig c = bp.sc;
        c.iterative = bp.iterative;
        c.depth = std::max(1, bp.sc.depth);
        c.timeLimitMs = std::clamp(bp.abSeconds, 1, 86400) * 1000;
        DetConfig d = bp.dc;
        d.threads = std::max(0, bp.threads);
        d.rootMaxMoves = std::max(0, d.rootMaxMoves);
        d.seed = mix64(seed ^ (0x9FB21C651E98DF25ull * (uint64_t)(turns.size() + 1)));
        d.knownOpp = true;
        d.oppRack = trays[p ^ 1];
        DetSearch ds(root, c, d);
        while (!ds.finished()) {
            if (abortFlag && abortFlag->load()) {
                ds.cancel();
                return out;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        const DetResult r = ds.take();
        if (!r.hasMove || r.ranked.empty()) return out;
        const int topSamples = r.ranked[0].samples;
        double best = -1e18, ch = 0;
        bool found = false;
        for (const DetMove& m : r.ranked) {
            if (m.samples == topSamples) best = std::max(best, m.avg);
            if (sameMove(m.m, chosen)) {
                ch = m.avg;
                found = true;
            }
        }
        if (found) best = std::max(best, ch);
        out.used = true;
        out.found = found;
        out.depth = r.depth;
        out.best = (int)std::lround(best);
        out.chosen = (int)std::lround(ch);
        return out;
    }

    void scanMove(RackScan& s, const RawMove& rm, int pts) const {
        const int dr = rm.is_descending ? 1 : 0, dc = 1 - dr;
        bool tw = false, dw = false, tl = false, dl = false;
        int r = rm.row, c = rm.col;
        for (int i = 0; i < rm.len; ++i, r += dr, c += dc) {
            const int idx = sq(r, c);
            if (board.letters_row[idx] != EMPTY_SQUARE) continue;
            switch (kPremium[idx]) {
                case MULT_TW:
                    tw = true;
                    break;
                case MULT_DW:
                    dw = true;
                    break;
                case MULT_TL:
                    tl = true;
                    break;
                case MULT_DL:
                    dl = true;
                    break;
                default:
                    break;
            }
        }
        if (tw) s.bestTW = std::max(s.bestTW, pts);
        if (dw) s.bestDW = std::max(s.bestDW, pts);
        if (tl) s.bestTL = std::max(s.bestTL, pts);
        if (dl) s.bestDL = std::max(s.bestDL, pts);
        if (pts >= 40) ++s.n40;
        if (pts >= 50) ++s.n50;
    }

    bool bestMoveFor(int p, Move& outMove, int& outScore, MoveScore& outDetail,
                     RackScan* scan = nullptr) {
        MoveList sink;
        generateMovesFromScratch(*gd, board, trays[p], sink);
        if (scan) *scan = RackScan{};
        const ScoredMove* best = nullptr;
        for (auto& rm : sink.moves) {
            MoveScore sc = scoreRawFull(board, rm);
            if (!best || sc.points > best->sc.points) {
                bestScratch.rm = rm;
                bestScratch.sc = sc;
                best = &bestScratch;
            }
            if (scan) scanMove(*scan, rm, sc.points);
        }
        if (scan) {
            scan->nMoves = (int)sink.moves.size();
            scan->best = best ? best->sc.points : 0;
        }
        if (!best) return false;
        outMove = toMove(best->rm);
        outScore = best->sc.points;
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
                if (ord[i] == trayLetter) {
                    ord.erase(ord.begin() + (long)i);
                    break;
                }
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
            for (uint8_t k = 0; k < trays[p].counts[i]; ++k) ord.push_back(fromIndex((uint8_t)i));
    }
    void syncTrayOrderAll() {
        for (int i = 0; i < 2; ++i) rebuildTrayMirrorFromCounts(i);
    }
};