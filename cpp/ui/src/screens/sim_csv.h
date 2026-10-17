#pragma once
// CSV export and BatchRun method implementations.
// Includes sim_stats.h for BatchRun, BatchGameRes, MoverConfig, SimGame::Turn.

#include "sim_stats.h"

inline void BatchRun::stop() {
    cancel = true;
    for (auto& t : pool) if (t.joinable()) t.join();
    pool.clear();
}

inline void BatchRun::worker(Gaddag* gd) {
    for (;;) {
        const int g = next++;
        if (g >= total || cancel) break;
        playGame(gd, g);
    }
    if (--alive == 0) { summarize(); finished = true; }
}

inline void BatchRun::playGame(Gaddag* gd, int g) {
    const int flip = g & 1;
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

    BatchGameRes& r = res[g];
    r.flip = flip;
    r.s[0] = sim.score[flip]; r.s[1] = sim.score[flip ^ 1];
    for (int b = 0; b < 2; ++b) {
        const Tray& t = sim.trays[flip ^ b];
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) r.left[b] += POINTS_PER_TILE[i] * t.counts[i];
        if (r.out < 0 && t.size() == 0) r.out = b;
    }
    r.tv = std::move(sim.turns);
    r.ok = true;
    ++done;
}

inline void BatchRun::sectionsC(std::string& s, int games) {
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

    for (const BatchGameRes& r : res) {
        if (!r.ok) continue;
        ++gN;
        for (int b = 0; b < 2; ++b) { left[b] += r.left[b]; if (r.out == b) ++outN[b]; }
        if (r.out < 0) ++noOut;
        bool seenEnd[2] = {false, false};
        const int fin[2] = { r.s[0] - r.s[1], r.s[1] - r.s[0] };
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

    if (perfInfo) {
        head("PERFECT-INFORMATION REGRET (chosen move vs alpha-beta that sees the opponent's true rack)");
        add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
        two("Search turns measured", "%.0f", (double)pi[0].n, pi[0].n,
            100.0 * pi[1].n / std::max(1LL, pi[1].n), pi[1].n);
        two("Chosen move found in the reference list", "%.1f%%",
            100.0 * pi[0].found / std::max(1LL, pi[0].n), pi[0].n,
            100.0 * pi[1].found / std::max(1LL, pi[1].n), pi[1].n);
        two("Avg regret (pts)", "%.2f", pi[0].gap / std::max(1LL, pi[0].found), pi[0].found,
            pi[1].gap / std::max(1LL, pi[1].found), pi[1].found);
        two("Turns where chosen = perfect-info best", "%.1f%%",
            100.0 * pi[0].zero / std::max(1LL, pi[0].found), pi[0].found,
            100.0 * pi[1].zero / std::max(1LL, pi[1].found), pi[1].found);
        static const char* kPh[3] = { "Avg regret, opening (bag > 50)", "Avg regret, midgame (bag 8-50)", "Avg regret, endgame (bag < 8)" };
        for (int k = 0; k < 3; ++k)
            two(kPh[k], "%.2f", pi[0].phGap[k] / std::max(1LL, pi[0].phN[k]), pi[0].phN[k],
                pi[1].phGap[k] / std::max(1LL, pi[1].phN[k]), pi[1].phN[k]);
        s += "  (Reference = same AB settings, true opponent rack, bag order still unknown. Single-thread bots sample one bag order, so it is noisy.)\n";
    } else {
        s += "\n(Perfect-information regret: off. Enable the 'Perfect-info' checkbox before starting a batch.)\n";
    }
    head("OPPONENT'S HIDDEN RACK (turns with tiles in the bag; column = the bot moving)");
    add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
    two("Opp S per rack: actual", "%.3f", op[0].s / std::max(1LL, op[0].n), op[0].n, op[1].s / std::max(1LL, op[1].n), op[1].n);
    two("Opp S per rack: uniform-draw expectation", "%.3f", op[0].es / std::max(1LL, op[0].n), op[0].n, op[1].es / std::max(1LL, op[1].n), op[1].n);
    two("Opp blank per rack: actual", "%.3f", op[0].b / std::max(1LL, op[0].n), op[0].n, op[1].b / std::max(1LL, op[1].n), op[1].n);
    two("Opp blank per rack: uniform-draw expectation", "%.3f", op[0].eb / std::max(1LL, op[0].n), op[0].n, op[1].eb / std::max(1LL, op[1].n), op[1].n);
    two("First turn with bag <= 7: possible opp racks", "%.0f",
        std::pow(10.0, op[0].logR / std::max(1LL, op[0].endN)), op[0].endN,
        std::pow(10.0, op[1].logR / std::max(1LL, op[1].endN)), op[1].endN);
    two("   ... unseen tiles at that point", "%.1f",
        op[0].pool / std::max(1LL, op[0].endN), op[0].endN,
        op[1].pool / std::max(1LL, op[1].endN), op[1].endN);
    s += "  (Actual below expectation = the opponent keeps/plays that tile more than a uniform guess assumes; the search samples uniformly.)\n";

    head("GAME END (no rack penalty is applied in Sim)");
    add("  %-40s%10s   %10s\n", "", "Bot 1", "Bot 2");
    two("Avg rack points left at game end", "%.1f", left[0] / std::max(1LL, gN), gN, left[1] / std::max(1LL, gN), gN);
    two("Went out (rack empty at game end)", "%.1f%%", 100.0 * outN[0] / std::max(1LL, gN), gN, 100.0 * outN[1] / std::max(1LL, gN), gN);
    add("  Nobody went out: %lld of %lld games\n", noOut, gN);

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
}

inline void BatchRun::summarize() {
    int wins[2] = {0, 0}, ties = 0, games = 0, firstGames[2] = {0, 0}, firstWins[2] = {0, 0};
    double phase[3] = {0, 0, 0};
    BotAgg ag[2];
    std::vector<std::string> swapLog;

    for (int g = 0; g < (int)res.size(); ++g) {
        const BatchGameRes& r = res[g];
        if (!r.ok) continue;
        ++games; ++firstGames[r.flip];
        if (r.s[0] == r.s[1]) ++ties;
        else { const int w = r.s[0] > r.s[1] ? 0 : 1; ++wins[w]; if (w == r.flip) ++firstWins[w]; }

        const auto& tv = r.tv;
        for (size_t i = 0; i < tv.size(); ++i) {
            const SimGame::Turn& t = tv[i];
            const int bot = t.player ^ r.flip;
            BotAgg& a = ag[bot];
            const int regret = t.greedyBest - t.score;
            ++a.turns; a.scoreSum += t.score; a.regretAll += regret;
            phase[t.bagBefore > 50 ? 0 : t.bagBefore >= 8 ? 1 : 2] += (bot == 0 ? t.score : -t.score);

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
                if (i + 2 < tv.size()) {
                    const int lv = t.leaveKept;
                    const int bk = lv < -50 ? 0 : lv < 0 ? 1 : lv < 50 ? 2 : lv < 100 ? 3 : 4;
                    ++a.bucketN[bk]; a.bucketSum[bk] += tv[i + 2].score;
                }
                if (i + 1 < tv.size()) {
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

    std::vector<double> pm;
    for (int k = 0; 2 * k + 1 < (int)res.size(); ++k)
        if (res[2 * k].ok && res[2 * k + 1].ok)
            pm.push_back(0.5 * ((res[2 * k].s[0] - res[2 * k].s[1]) + (res[2 * k + 1].s[0] - res[2 * k + 1].s[1])));
    const int P = (int)pm.size();
    double pmean = 0, psd = 0;
    for (double v : pm) pmean += v;
    if (P) pmean /= P;
    if (P > 1) { for (double v : pm) psd += (v - pmean) * (v - pmean); psd = std::sqrt(psd / (double)(P - 1)); }
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
        head("RESULTS");
        add("Wins: Bot 1 %d (%.1f%%)   Bot 2 %d (%.1f%%)   ties %d\n",
            wins[0], 100.0 * wins[0] / n, wins[1], 100.0 * wins[1] / n, ties);
        (void)firstGames; (void)firstWins;

        head("SCORING (per bot)"); cols();
        row2("Avg final score", "%10.1f", dv(ag[0].scoreSum, n), dv(ag[1].scoreSum, n));
        row2("Points per turn", "%10.1f", dv(ag[0].scoreSum, ag[0].turns), dv(ag[1].scoreSum, ag[1].turns));
        row2("Points per scoring move", "%10.1f", dv(ag[0].scoreSum, ag[0].moves), dv(ag[1].scoreSum, ag[1].moves));
        row2("Tiles played per scoring move", "%10.2f", dv(ag[0].tiles, ag[0].moves), dv(ag[1].tiles, ag[1].moves));
        row2("Bingos per game", "%10.2f", dv(ag[0].bingos, n), dv(ag[1].bingos, n));
        row2("Turns per game", "%10.1f", dv(ag[0].turns, n), dv(ag[1].turns, n));

        head("DECISION QUALITY vs Greedy (regret = Greedy best - points scored)"); cols();
        row2("Turns matching Greedy's best score", "%9.1f%%", 100 * dv(ag[0].matches, ag[0].turns), 100 * dv(ag[1].matches, ag[1].turns));
        row2("Avg regret on scoring moves (pts)", "%10.2f", dv(ag[0].regretMove, ag[0].moves), dv(ag[1].regretMove, ag[1].moves));
        row2("Total regret per game (pts)", "%10.1f", dv(ag[0].regretAll, n), dv(ag[1].regretAll, n));
        s += "  (Greedy should show 100% / 0 / 0. A negative regret would mean a bug.)\n";

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

        if (ag[0].srN + ag[1].srN > 0) {
            head("SEARCH (turns where the bot searched)"); cols();
            row2("Search turns", "%10.0f", (double)ag[0].srN, (double)ag[1].srN);
            rowN("Fell back to Greedy (no result)", "%.1f%%", 100 * dv((double)ag[0].srFell, (double)ag[0].srN), ag[0].srN,
                 100 * dv((double)ag[1].srFell, (double)ag[1].srN), ag[1].srN);
            rowN("Avg time used (s)", "%.2f", dv(ag[0].srTime, (double)ag[0].srN) / 1000.0, ag[0].srN,
                 dv(ag[1].srTime, (double)ag[1].srN) / 1000.0, ag[1].srN);
            rowN("Time used / limit (timed searches)", "%.1f%%", 100 * dv(ag[0].srTimedT, ag[0].srLimit), ag[0].srTimed,
                 100 * dv(ag[1].srTimedT, ag[1].srLimit), ag[1].srTimed);
            s += "  (Search statistics shown for the searching side only.)\n";
        }
    }

    sectionsC(s, games);
    writeCsv(runId, cfg, res, s);
    summary = std::move(s);
}

inline void writeCsv(const std::string& runId, const MoverConfig (&cfg)[2],
                     const std::vector<BatchGameRes>& res, std::string& summaryOut) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir(DIAG_DATA_DIR);
    fs::create_directories(dir, ec);
    const fs::path gp = dir / ("run_" + runId + "_games.csv");
    const fs::path tp = dir / ("run_" + runId + "_turns.csv");
    std::ofstream gf(gp), tf(tp);
    if (!gf || !tf) { summaryOut += "\nCSV export FAILED: " + dir.string() + "\n"; return; }

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
        const BatchGameRes& r = res[g];
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
    summaryOut += "\nData files:\n  " + gp.string() + "\n  " + tp.string() + "\n";
}

// BatchRun destructor definition (declared in sim_stats.h).
inline BatchRun::~BatchRun() { stop(); }
