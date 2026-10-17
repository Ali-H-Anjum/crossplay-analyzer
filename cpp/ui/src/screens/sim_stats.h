#pragma once
// Batch runner and statistics for Sim mode.
// Depends on sim_game.h (SimGame, MoverConfig, SimGame::Turn).
// BatchRun method implementations live in sim_csv.h.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "sim_game.h"

// Game result for one batch game (was BatchRun::GameRes).
struct BatchGameRes {
    bool ok = false;
    int  flip = 0, s[2] = {0, 0};            // s[]: Bot 1 / Bot 2
    int  left[2] = {0, 0}, out = -1;         // rack points left per bot; bot that went out (-1 = nobody)
    std::vector<SimGame::Turn> tv;
};

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
    double    openBig = 0, nMoves = 0;
    long long has40 = 0, has50 = 0, usedBig = 0;
    double    premBest[4] = {};  long long premTurns[4] = {};
    long long laneN[4] = {};     double laneSum[4] = {};
    double    oppBefore = 0, oppAfter = 0; long long oppN = 0;
    long long srN = 0, srFell = 0, srAB = 0, srMC = 0, srTimed = 0, srGapN = 0, srNoGreedy = 0;
    double    srRoot = 0, srTime = 0, srTimedT = 0, srLimit = 0, srDepth = 0, srNodes = 0, srSamples = 0;
    double    srGap = 0, srRank = 0, srSims = 0, mcWin = 0, mcGWin = 0;
    long long mcWinN = 0, mcGN = 0;
};

// Plays N games headless on worker threads.
struct BatchRun {
    std::vector<std::thread> pool;
    std::atomic<bool>  cancel{false};
    std::atomic<bool>  finished{false};
    std::atomic<int>   done{0}, next{0}, alive{0};
    bool        running = false, showSummary = false;
    bool        perfInfo = false;
    int         total = 0, parallel = 1;
    uint64_t    baseSeed = 0;
    std::string runId;
    MoverConfig cfg[2];
    std::vector<BatchGameRes> res;
    std::string summary;
    std::chrono::steady_clock::time_point t0;

    ~BatchRun();
    void stop();
    void worker(Gaddag* gd);
    void playGame(Gaddag* gd, int g);
    void sectionsC(std::string& s, int games);
    void summarize();       // defined in sim_csv.h (calls writeCsv)
};

// CSV export: declared here, defined in sim_csv.h.
void writeCsv(const std::string& runId, const MoverConfig (&cfg)[2],
              const std::vector<BatchGameRes>& res, std::string& summaryOut);