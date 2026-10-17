#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>
#include "det_search.h"

// ---- EDIT HERE: everything the user picks before an MCTS search starts ----
struct MctsConfig {
    int      simulations     = 20000;            // per thread
    int      simsPerTree     = 2000;             // re-determinize (new opponent rack + bag order) after this many
    int      timeLimitMs     = 10 * 60 * 1000;   // default 10 minutes
    double   C               = 1.41;             // UCB1 balance constant
    double   epsilon         = 0.2;              // rollout: chance of a random move instead of the top scoreRaw move
    int      maxChildren     = 40;               // placed moves kept per tree node, best scoreRaw first; 0 = all
    int      rolloutMaxPlies = 14;               // 0 = play to game end (hard cap 128)
    double   cutScale        = 300.0;            // rollout cut off early: reward = logistic(evaluate / cutScale); UNTUNED
    int      threads         = 0;                // 0 = cores - 1 (at least 1)
    uint64_t seed            = 12345;
};

// Reward of a finished rollout, from the root side's view, in [0,1].
// To use score margin instead of win/draw/loss, change only these two functions.
inline double terminalResult(const Position& p, int me) {
    const int d = p.score[me] - p.score[me ^ 1];
    return d > 0 ? 1.0 : (d < 0 ? 0.0 : 0.5);
}
inline double cutResult(const Position& p, int me, double scale) {
    return 1.0 / (1.0 + std::exp(-static_cast<double>(evaluate(p, me)) / scale));
}
// ---------------------------------------------------------------------------

struct MctsProgress {
    std::atomic<bool>     stop{false};
    std::atomic<bool>     done{false};
    std::atomic<uint64_t> sims{0};
    std::atomic<int>      elapsedMs{0};
};

struct MRoot { SMove m{}; Tray thrown{}; double wins = 0; uint64_t visits = 0; };

struct MNode {
    SMove    move{};                 // move that led here
    double   wins = 0;               // reward from the view of the side that made `move`
    uint32_t visits = 0;
    int32_t  parent = -1, first = -1;
    uint16_t n = 0;                  // number of children, at tree[first .. first+n)
    uint8_t  mover = 0;
    bool     expanded = false;
};

// One thread: runs trees until `simulations` are done. `pos` is mutated during the run
// (opponent rack + bag are re-determinized) but board, own rack, scores and side come back exact.
class MctsWorker {
public:
    MctsWorker(Position& pos, const MctsConfig& c, uint64_t seed, MctsProgress* prog = nullptr)
        : p(pos), cfg(c), me(pos.side), rng(seed), progress(prog) {
        pool = unseenPool(*p.b, p.tray[me]);
        int t = 0; for (uint8_t v : pool) t += v;
        oppN = static_cast<uint8_t>(std::min(7, t));
        tree.reserve(1 << 15); path.reserve(128); roll.reserve(128);
        ml.moves.reserve(1024); scratch.reserve(1024);
    }

    std::vector<MRoot> run() {
        startT = clock::now();
        deadline = startT + std::chrono::milliseconds(cfg.timeLimitMs);
        int done = 0;
        while (done < cfg.simulations && !stopNow()) {
            determinize();
            done += runTree(std::min(cfg.simsPerTree, cfg.simulations - done));
        }
        if (progress) progress->elapsedMs.store(elapsedMs());
        return roots;
    }

private:
    using clock = std::chrono::steady_clock;
    struct Rng {
        uint64_t s; explicit Rng(uint64_t x) : s(x) {}
        uint64_t next() { return mix64(s += 0x9E3779B97F4A7C15ull); }
        double uni() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
        uint32_t below(uint32_t n) { return static_cast<uint32_t>((static_cast<uint64_t>(next() >> 32) * n) >> 32); }
    };
    struct Rec { PlyRec ply; SwapRec sw; MoveKind kind = MK_PLACE; };
    struct Sink {
        std::vector<SMove>& v;
        void operator()(const RawMove& m) { SMove s; s.m = m; v.push_back(s); }
    };

    Position& p; MctsConfig cfg; int me; Rng rng; MctsProgress* progress;
    std::array<uint8_t, NUM_LETTER_SLOTS> pool{}; uint8_t oppN = 0;
    std::vector<MNode> tree; std::vector<Rec> path, roll;
    MoveList ml; std::vector<SMove> scratch;
    Tray rootSw[2]; int rootNsw = 0;
    std::vector<MRoot> roots;
    clock::time_point startT, deadline; bool timeUp = false; unsigned chk = 0;

    int elapsedMs() const {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - startT).count());
    }
    bool stopNow() {
        if (progress && progress->stop.load(std::memory_order_relaxed)) return true;
        if ((++chk & 15) == 0) {
            if (clock::now() >= deadline) timeUp = true;
            if (progress) progress->elapsedMs.store(elapsedMs(), std::memory_order_relaxed);
        }
        return timeUp;
    }

    void determinize() {
        Tray& opp = p.tray[me ^ 1];
        opp = Tray{};
        p.bag->setPool(pool); p.bag->shuffle();
        p.bag->drawNextInto(opp, oppN);
    }

    int runTree(int n) {
        tree.clear(); tree.emplace_back();
        rootNsw = 0;
        int i = 0;
        for (; i < n; ++i) {
            if (stopNow()) break;
            simulate();
            if (progress) progress->sims.fetch_add(1, std::memory_order_relaxed);
        }
        harvest();
        return i;
    }

    void harvest() {
        const MNode& r = tree[0];
        for (int k = 0; k < r.n; ++k) {
            const MNode& c = tree[static_cast<size_t>(r.first + k)];
            if (c.visits == 0) continue;
            MRoot* t = nullptr;
            for (MRoot& x : roots) if (sameMove(x.m, c.move)) { t = &x; break; }
            if (!t) {
                roots.emplace_back(); t = &roots.back(); t->m = c.move;
                if (c.move.kind == MK_SWAP) t->thrown = rootSw[c.move.swapIdx];
            }
            t->visits += c.visits; t->wins += c.wins;
        }
    }

    void applyChild(const SMove& m, Rec& r) {
        r.kind = m.kind;
        if (m.kind == MK_PLACE) makePly(p, m.m, r.ply);
        else if (m.kind == MK_SWAP) { Tray sw[2]; swapCandidates(p, sw); makeSwap(p, sw[m.swapIdx], r.sw); }
        else makePass(p, r.ply);
    }
    void undoRec(const Rec& r) {
        if (r.kind == MK_PLACE) unmakePly(p, r.ply);
        else if (r.kind == MK_SWAP) unmakeSwap(p, r.sw);
        else unmakePass(p, r.ply);
    }

    void expand(int ni) {
        scratch.clear();
        Sink sink{scratch};
        generateMoves(*p.g, *p.b, p.al, p.tray[p.side], sink);
        for (SMove& s : scratch) s.pts = scoreRaw(*p.b, s.m);
        const auto byPts = [](const SMove& a, const SMove& b) { return a.pts > b.pts; };
        if (cfg.maxChildren > 0 && scratch.size() > static_cast<size_t>(cfg.maxChildren)) {
            std::partial_sort(scratch.begin(), scratch.begin() + cfg.maxChildren, scratch.end(), byPts);
            scratch.resize(static_cast<size_t>(cfg.maxChildren));
        } else {
            std::sort(scratch.begin(), scratch.end(), byPts);
        }
        Tray sw[2];
        const int nsw = swapCandidates(p, sw);
        for (int i = 0; i < nsw; ++i) { SMove s; s.kind = MK_SWAP; s.swapIdx = static_cast<uint8_t>(i); scratch.push_back(s); }
        if (scratch.empty()) { SMove s; s.kind = MK_PASS; scratch.push_back(s); }
        if (ni == 0) { rootNsw = nsw; for (int i = 0; i < nsw; ++i) rootSw[i] = sw[i]; }

        const int first = static_cast<int>(tree.size());
        const uint8_t mover = static_cast<uint8_t>(p.side);
        for (const SMove& s : scratch) {
            MNode c; c.move = s; c.parent = ni; c.mover = mover;
            tree.push_back(c);
        }
        tree[static_cast<size_t>(ni)].first = first;
        tree[static_cast<size_t>(ni)].n = static_cast<uint16_t>(scratch.size());
        tree[static_cast<size_t>(ni)].expanded = true;
    }

    // UCB1: unvisited children first (best scoreRaw first), else wins/visits + C*sqrt(ln(parent)/visits).
    int select(int ni) const {
        const MNode& nd = tree[static_cast<size_t>(ni)];
        const double lp = std::log(std::max<double>(nd.visits, 1.0));
        int best = -1; double bs = -1.0;
        for (int k = 0; k < nd.n; ++k) {
            const int ci = nd.first + k;
            const MNode& c = tree[static_cast<size_t>(ci)];
            if (c.visits == 0) return ci;
            const double s = c.wins / c.visits + cfg.C * std::sqrt(lp / c.visits);
            if (s > bs) { bs = s; best = ci; }
        }
        return best;
    }

    void simulate() {
        int node = 0; path.clear();
        while (true) {
            if (isTerminal(p)) break;
            if (!tree[static_cast<size_t>(node)].expanded) expand(node);
            if (tree[static_cast<size_t>(node)].n == 0) break;
            const int c = select(node);
            const bool fresh = tree[static_cast<size_t>(c)].visits == 0;
            path.emplace_back();
            applyChild(tree[static_cast<size_t>(c)].move, path.back());
            node = c;
            if (fresh) break;                          // new leaf: simulate from here
        }
        const double res = rollout();
        for (int i = node; i >= 0; i = tree[static_cast<size_t>(i)].parent) {   // backpropagate
            MNode& n = tree[static_cast<size_t>(i)];
            ++n.visits;
            if (i != 0) n.wins += (n.mover == me) ? res : 1.0 - res;
        }
        for (size_t i = path.size(); i-- > 0;) undoRec(path[i]);
    }

    // Epsilon-greedy playout from the current position; restores the position before returning.
    double rollout() {
        const int cap = cfg.rolloutMaxPlies > 0 ? std::min(cfg.rolloutMaxPlies, 128) : 128;
        roll.clear();
        for (int plies = 0; plies < cap && !isTerminal(p); ++plies) {
            ml.moves.clear();
            generateMoves(*p.g, *p.b, p.al, p.tray[p.side], ml);
            roll.emplace_back();
            Rec& r = roll.back();
            if (!ml.moves.empty()) {
                const RawMove* pick = nullptr;
                if (rng.uni() < cfg.epsilon) {
                    pick = &ml.moves[rng.below(static_cast<uint32_t>(ml.moves.size()))];
                } else {
                    int bestPts = -1;
                    for (const RawMove& m : ml.moves) {
                        const int s = scoreRaw(*p.b, m);
                        if (s > bestPts) { bestPts = s; pick = &m; }
                    }
                }
                r.kind = MK_PLACE; makePly(p, *pick, r.ply);
            } else {
                Tray sw[2];
                if (swapCandidates(p, sw) > 0) { r.kind = MK_SWAP; makeSwap(p, sw[0], r.sw); }
                else                           { r.kind = MK_PASS; makePass(p, r.ply); }
            }
        }
        const double res = isTerminal(p) ? terminalResult(p, me) : cutResult(p, me, cfg.cutScale);
        for (size_t i = roll.size(); i-- > 0;) undoRec(roll[i]);
        roll.clear();
        return res;
    }
};

struct MctsMove { SMove m{}; Tray thrown{}; uint64_t visits = 0; double winRate = 0; };
struct MctsResult {
    SMove best{}; Tray swapThrown{};   // swapThrown is valid when best.kind == MK_SWAP
    double winRate = 0;                // best move's win rate for the root side, 0..1
    uint64_t sims = 0; int threads = 0; bool hasMove = false;
    std::vector<MctsMove> ranked;      // most visited first (for the UI)
    int timeMs = 0;
};

// Parallel root search: one thread per worker, each with its own board copy and determinizations.
class MctsSearch {
public:
    static int defaultThreads() { return DetSearch::defaultThreads(); }

    MctsSearch(const Position& root, const MctsConfig& c) {
        const int n = c.threads > 0 ? c.threads : defaultThreads();
        for (int i = 0; i < n; ++i)
            jobs.push_back(std::make_unique<Job>(root, c, mix64(c.seed + static_cast<uint64_t>(i))));
    }
    MctsSearch(const MctsSearch&) = delete;
    MctsSearch& operator=(const MctsSearch&) = delete;

    int  threads() const { return static_cast<int>(jobs.size()); }
    void cancel() { for (auto& j : jobs) j->prog.stop.store(true); }
    bool finished() const { for (auto& j : jobs) if (!j->prog.done.load()) return false; return true; }
    uint64_t sims() const { uint64_t s = 0; for (auto& j : jobs) s += j->prog.sims.load(); return s; }
    int elapsedMs() const { int m = 0; for (auto& j : jobs) m = std::max(m, j->prog.elapsedMs.load()); return m; }

    MctsResult take() {
        MctsResult out; std::vector<MRoot> all;
        for (auto& j : jobs) {
            if (j->th.joinable()) j->th.join();
            out.sims += j->prog.sims.load();
            for (const MRoot& r : j->out) {
                MRoot* t = nullptr;
                for (MRoot& x : all) if (sameMove(x.m, r.m)) { t = &x; break; }
                if (!t) { all.push_back(r); continue; }
                t->visits += r.visits; t->wins += r.wins;
            }
        }
        out.threads = threads();
        out.timeMs = elapsedMs();
        std::stable_sort(all.begin(), all.end(), [](const MRoot& a, const MRoot& b) {
            if (a.visits != b.visits) return a.visits > b.visits;
            return a.wins / a.visits > b.wins / b.visits;
        });
        for (const MRoot& r : all) {
            MctsMove m; m.m = r.m; m.thrown = r.thrown; m.visits = r.visits; m.winRate = r.wins / r.visits;
            out.ranked.push_back(m);
        }
        if (!out.ranked.empty()) {
            out.best = out.ranked[0].m; out.swapThrown = out.ranked[0].thrown;
            out.winRate = out.ranked[0].winRate; out.hasMove = true;
        }
        return out;
    }

private:
    struct Job {
        Board board; TileBag bag; Position pos; MctsProgress prog; std::vector<MRoot> out; std::thread th;
        Job(const Position& root, const MctsConfig& c, uint64_t seed) : board(*root.b), bag(seed), pos(root) {
            pos.b = &board; pos.bag = &bag;
            th = std::thread([this, c, seed] {
                out = MctsWorker(pos, c, seed ^ 0xD1B54A32D192ED03ull, &prog).run();
                prog.done.store(true);
            });
        }
        ~Job() { prog.stop.store(true); if (th.joinable()) th.join(); }
    };
    std::vector<std::unique_ptr<Job>> jobs;
};