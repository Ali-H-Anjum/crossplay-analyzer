#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include "swap_gen.h"
#include "zobrist.h"
#include "tt.h"

enum MoveKind : uint8_t { MK_PLACE, MK_SWAP, MK_PASS };
struct SMove { RawMove m{}; int pts = 0; int ord = 0; MoveKind kind = MK_PLACE; uint8_t swapIdx = 0; };

inline bool sameMove(const SMove& a, const SMove& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == MK_SWAP) return a.swapIdx == b.swapIdx;
    if (a.kind == MK_PASS) return true;
    return a.m.row == b.m.row && a.m.col == b.m.col && a.m.len == b.m.len &&
           a.m.is_descending == b.m.is_descending && a.m.blank_mask == b.m.blank_mask &&
           std::equal(a.m.letters.begin(), a.m.letters.begin() + a.m.len, b.m.letters.begin());
}

// 16-bit move signature for the TT move. Never 0. A collision only mis-orders, never changes values.
inline uint16_t moveSig(const SMove& s) {
    uint64_t h = 0x1234567ull + s.kind;
    if (s.kind == MK_SWAP) {
        h = mix64(h ^ (0x100ull + s.swapIdx));
    } else if (s.kind == MK_PLACE) {
        h = mix64(h ^ ((uint64_t(s.m.row) << 16) | (uint64_t(s.m.col) << 8) | s.m.len | (s.m.is_descending ? 1ull << 40 : 0)));
        h = mix64(h ^ s.m.blank_mask);
        for (int i = 0; i < s.m.len; ++i) h = (h * 0x100000001B3ull) ^ s.m.letters[i];
        h = mix64(h);
    }
    const uint16_t v = static_cast<uint16_t>(h >> 48);
    return v ? v : 1;
}

// ---- EDIT HERE: everything the user picks before a search starts ----------
enum class Algo { Minimax, AlphaBeta };
struct SearchConfig {
    Algo algo        = Algo::AlphaBeta;
    int  depth       = 3;                 // plies, fixed-depth mode
    bool orderStatic = true;              // try placed moves in scoreRaw order, best first
    bool killers     = false;             // try the 2 moves that last caused a cutoff at this ply first
    bool history     = false;             // bonus for moves (start square + direction) that caused cutoffs before
    bool useTT       = false;             // transposition table (cutoffs + TT move tried first)
    int  ttBits      = 20;                // 2^20 entries x 16 bytes = 16 MB per search
    bool iterative   = false;             // true: deepen 1,2,3.. until time runs out
    int  maxDepth    = 64;                // iterative cap
    int  timeLimitMs = 10 * 60 * 1000;    // iterative limit, default 10 minutes
    bool rootValues  = false;             // exact value for EVERY root move (no root narrowing); DetSearch needs it
    int  rootMaxMoves = 0;                // with rootValues: keep only the top-N placed root moves by scoreRaw (0 = all)

    // --- forward pruning (all heuristics: they can change the result) ---
    bool useBeam = false;                 // keep only the top-N placed moves per ply (ranked by scoreRaw + killers/history)
    std::array<int, 8> beam = {30, 15, 8, 5, 5, 5, 5, 5};   // N per ply from the root; last entry repeats; 0 = unlimited
    bool lmr         = false;             // late move reduction
    int  lmrAfter    = 4;                 // moves tried at full depth before reducing
    int  lmrMinDepth = 3;                 // only reduce at nodes with at least this much depth left
    int  lmrReduce   = 1;                 // plies removed from late moves; re-searched at full depth if they look good
    bool probCut     = false;             // shallow search predicts a cutoff at the full depth
    int  pcMinDepth  = 4;                 // only try at nodes with at least this much depth left
    int  pcReduce    = 2;                 // shallow depth = depth - pcReduce (keep it even: same side at the leaves)
    int  pcMargin    = 300;               // tenths of a point (30 pts); UNTUNED, bigger = safer
};
// ---------------------------------------------------------------------------

// Written by the search thread, read by the UI every frame.
struct SearchProgress {
    std::atomic<bool>     stop{false};    // UI sets this to cancel
    std::atomic<bool>     done{false};
    std::atomic<int>      depth{0};       // last COMPLETED depth
    std::atomic<int>      value{0};       // tenths of a point, root side's view
    std::atomic<uint64_t> nodes{0};       // live, updated every ~2k nodes
    std::atomic<int>      elapsedMs{0};
    mutable std::mutex    mu;             // guards the three fields below
    SMove best{}; Tray thrown{}; bool hasBest = false;

    void reset() {
        stop = false; done = false; depth = 0; value = 0; nodes = 0; elapsedMs = 0;
        std::lock_guard<std::mutex> lk(mu); best = SMove{}; thrown = Tray{}; hasBest = false;
    }
};

struct RootVal { SMove m{}; Tray thrown{}; int value = 0; };   // thrown is valid for MK_SWAP

struct SearchResult {
    SMove best{}; Tray swapThrown{};      // swapThrown is valid when best.kind == MK_SWAP
    int value = 0; uint64_t nodes = 0; uint64_t ttHits = 0; bool hasMove = false;
    std::vector<RootVal> rootVals;        // filled only when cfg.rootValues
};

class Searcher {
public:
    Searcher(Position& pos, const SearchConfig& c, SearchProgress* prog = nullptr)
        : p(pos), cfg(c), me(pos.side), progress(prog),
          buf((c.iterative ? c.maxDepth : c.depth) + 1), kill(buf.size()), tt(c.useTT ? c.ttBits : 1) {
        for (auto& v : buf) v.reserve(256);
    }
    SearchResult run() { return cfg.iterative ? runIterative() : runFixed(); }

private:
    using clock = std::chrono::steady_clock;
    static constexpr int INF = 1 << 29;
    struct Rec { PlyRec ply; SwapRec sw; };
    struct Sink {
        std::vector<SMove>& v;
        void operator()(const RawMove& m) { SMove s; s.m = m; v.push_back(s); }
    };

    Position& p; SearchConfig cfg; int me; SearchProgress* progress;
    std::vector<std::vector<SMove>> buf;       // one preallocated list per ply

    struct Killers { SMove m[2]; uint8_t n = 0; };
    std::vector<Killers> kill;                 // declared right after buf
    TransTable tt;                             // declared right after kill (initializer order)
    std::array<int, 2 * 2 * NUM_SQUARES> hist{};
    static constexpr int KILLER0 = 1 << 20, KILLER1 = 1 << 19, HIST_DIV = 8, HIST_CAP = 100000;

    static int histIdx(int side, const RawMove& m) {
        return (side * 2 + (m.is_descending ? 1 : 0)) * NUM_SQUARES + sq(m.row, m.col);
    }
    void recordCutoff(int ply, int depth, const SMove& m) {
        if (cfg.killers) {
            Killers& k = kill[ply];
            if (!(k.n > 0 && sameMove(k.m[0], m))) { k.m[1] = k.m[0]; k.m[0] = m; if (k.n < 2) ++k.n; }
        }
        if (cfg.history) {
            int& h = hist[histIdx(p.side, m.m)];
            h = std::min(h + depth * depth, HIST_CAP * HIST_DIV);
        }
    }

    SearchResult res;
    uint64_t nodes = 0, total = 0, cutoffLeaves = 0, ttHits = 0;
    uint64_t bagH = 0;                         // swap-history term of the current path
    int curDepth = 0; bool limited = false, aborted = false, hasPrev = false;
    SMove prevBest{};
    clock::time_point startT, deadline;

    SearchResult runFixed() {
        startT = clock::now(); total = 0;
        curDepth = cfg.depth; limited = false; aborted = false; hasPrev = false;
        res = SearchResult{}; nodes = 0; bagH = 0;
        const int v = node(0, curDepth, -INF, INF);
        res.nodes = nodes; res.ttHits = ttHits;
        if (!aborted) { res.value = v; total = nodes; publish(cfg.depth, res); }
        if (progress) progress->done.store(true);
        return res;                                    // hasMove is false if cancelled
    }

    SearchResult runIterative() {
        startT = clock::now();
        deadline = startT + std::chrono::milliseconds(cfg.timeLimitMs);
        limited = true; total = 0; bagH = 0;
        SearchResult last{};
        for (int d = 1; d <= cfg.maxDepth; ++d) {
            curDepth = d; nodes = 0; cutoffLeaves = 0; aborted = false;
            res = SearchResult{};
            prevBest = last.best; hasPrev = last.hasMove;
            const int v = node(0, d, -INF, INF);
            total += nodes;
            if (aborted) break;                        // discard the unfinished iteration
            res.value = v; res.nodes = total;
            last = res;
            publish(d, last);
            if (!last.hasMove) break;                  // root is terminal
            if (cutoffLeaves == 0) break;              // whole tree reached game end: solved
            if (clock::now() >= deadline) break;
        }
        last.nodes = total; last.ttHits = ttHits;
        if (progress) progress->done.store(true);
        return last;
    }

    int elapsedMs() const {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - startT).count());
    }
    void publish(int d, const SearchResult& r) {
        if (!progress) return;
        { std::lock_guard<std::mutex> lk(progress->mu);
          progress->best = r.best; progress->thrown = r.swapThrown; progress->hasBest = r.hasMove; }
        progress->value.store(r.value); progress->nodes.store(total);
        progress->elapsedMs.store(elapsedMs()); progress->depth.store(d);
    }

    void poll() {                                      // every 2048 nodes; iterative depth 1 never aborts
        if (limited && curDepth <= 1) return;
        if (progress) {
            progress->nodes.store(total + nodes, std::memory_order_relaxed);
            progress->elapsedMs.store(elapsedMs(), std::memory_order_relaxed);
            if (progress->stop.load(std::memory_order_relaxed)) aborted = true;
        }
        if (limited && clock::now() >= deadline) aborted = true;
    }

    void make(const SMove& m, const Tray* sw, Rec& r) {
        if (m.kind == MK_PLACE)     makePly(p, m.m, r.ply);
        else if (m.kind == MK_SWAP) makeSwap(p, sw[m.swapIdx], r.sw);
        else                        makePass(p, r.ply);
    }
    void unmake(const SMove& m, Rec& r) {
        if (m.kind == MK_PLACE)     unmakePly(p, r.ply);
        else if (m.kind == MK_SWAP) unmakeSwap(p, r.sw);
        else                        unmakePass(p, r.ply);
    }

    // depth = plies still to search below this node.
    int node(int ply, int depth, int alpha, int beta, bool allowPC = true) {
        ++nodes;
        if ((nodes & 63) == 0) poll();      // was 2047: interior nodes now cost ~0.3 ms each
        if (aborted) return 0;
        if (isTerminal(p)) return evaluate(p, me);
        if (depth <= 0) { ++cutoffLeaves; return evaluate(p, me); }

        const int alpha0 = alpha, beta0 = beta;
        const uint64_t leavesBefore = cutoffLeaves;

        // ---- transposition table probe (cutoffs only below the root) ----
        uint64_t key = 0; uint16_t ttSig = 0;
        if (cfg.useTT) {
            key = hashPosition(p, me, bagH);
            const TTEntry& e = tt.probe(key);
            if (ply > 0 && e.key == key) {
                ttSig = e.move;
                if (e.solved() || e.depth >= depth) {
                    const TTFlag f = e.flag();
                    if (f == TT_EXACT || (f == TT_LOWER && e.value >= beta) || (f == TT_UPPER && e.value <= alpha)) {
                        ++ttHits;
                        if (!e.solved()) ++cutoffLeaves;   // a depth-limited result: this tree is not "solved"
                        return e.value;
                    }
                }
            }
        }

        // ---- ProbCut: a shallow null-window search predicts the deep result ----
        if (cfg.probCut && allowPC && ply > 0 && depth >= cfg.pcMinDepth) {
            const uint64_t cl = cutoffLeaves;
            const int sd = depth - cfg.pcReduce, T = cfg.pcMargin;
            if (beta < INF) {
                const int v = node(ply, sd, beta + T - 1, beta + T, false);
                if (aborted) return 0;
                if (v >= beta + T) { ++cutoffLeaves; return beta; }
            }
            if (alpha > -INF) {
                const int v = node(ply, sd, alpha - T, alpha - T + 1, false);
                if (aborted) return 0;
                if (v <= alpha - T) { ++cutoffLeaves; return alpha; }
            }
            cutoffLeaves = cl;                         // no cut: the real search counts its own leaves
        }

        std::vector<SMove>& mv = buf[ply];
        mv.clear();
        Sink sink{mv};
        generateMoves(*p.g, *p.b, p.al, p.tray[p.side], sink);

        if (ply == 0 && cfg.rootValues && cfg.rootMaxMoves > 0 && static_cast<int>(mv.size()) > cfg.rootMaxMoves) {
            for (SMove& s : mv) s.pts = scoreRaw(*p.b, s.m);
            std::partial_sort(mv.begin(), mv.begin() + cfg.rootMaxMoves, mv.end(),
                              [](const SMove& a, const SMove& b) { return a.pts > b.pts; });
            mv.resize(static_cast<size_t>(cfg.rootMaxMoves));
        }

        if (cfg.orderStatic || cfg.killers || cfg.history || cfg.useBeam) {
            const Killers& k = kill[ply];
            for (SMove& s : mv) {
                s.pts = scoreRaw(*p.b, s.m);
                int o = (cfg.orderStatic || cfg.useBeam) ? s.pts : 0;   // beam needs a static ranking
                if (cfg.killers) {
                    if (k.n > 0 && sameMove(s, k.m[0])) o += KILLER0;
                    else if (k.n > 1 && sameMove(s, k.m[1])) o += KILLER1;
                }
                if (cfg.history) o += std::min(hist[histIdx(p.side, s.m)] / HIST_DIV, HIST_CAP);
                s.ord = o;
            }
            std::sort(mv.begin(), mv.end(), [](const SMove& a, const SMove& b) { return a.ord > b.ord; });
        }
        if (cfg.useBeam) {                              // keep the top-N placed moves; swaps/pass are added below
            const int w = cfg.beam[std::min<size_t>(static_cast<size_t>(ply), cfg.beam.size() - 1)];
            if (w > 0 && static_cast<int>(mv.size()) > w) mv.resize(static_cast<size_t>(w));
        }

        Tray sw[2];
        const int nsw = swapCandidates(p, sw);
        for (int i = 0; i < nsw; ++i) { SMove s; s.kind = MK_SWAP; s.swapIdx = static_cast<uint8_t>(i); mv.push_back(s); }
        if (mv.empty()) { SMove s; s.kind = MK_PASS; mv.push_back(s); }
        if (ply == 0 && hasPrev)                        // previous iteration's best goes first
            for (size_t i = 1; i < mv.size(); ++i)
                if (sameMove(mv[i], prevBest)) { std::rotate(mv.begin(), mv.begin() + i, mv.begin() + i + 1); break; }
        if (ttSig)                                      // TT move goes first
            for (size_t i = 0; i < mv.size(); ++i)
                if (moveSig(mv[i]) == ttSig) { std::rotate(mv.begin(), mv.begin() + i, mv.begin() + i + 1); break; }

        const bool maxing  = (p.side == me);
        const bool rootAll = (ply == 0 && cfg.rootValues);   // exact value per root move: keep the root window open
        const uint64_t bagSaved = bagH;
        int best = maxing ? -INF : INF, bestI = 0;
        Rec r;
        for (size_t i = 0; i < mv.size(); ++i) {
            if (mv[i].kind == MK_SWAP) bagH = swapHash(bagSaved, p.bag->size(), sw[mv[i].swapIdx]);
            make(mv[i], sw, r);
            int v;
            const bool reduce = cfg.lmr && mv[i].kind == MK_PLACE &&
                                static_cast<int>(i) >= cfg.lmrAfter && depth >= cfg.lmrMinDepth;
            if (reduce) {
                v = node(ply + 1, depth - 1 - cfg.lmrReduce, alpha, beta);
                if (!aborted && (maxing ? v > alpha : v < beta))   // looks good after all: full depth
                    v = node(ply + 1, depth - 1, alpha, beta);
            } else {
                v = node(ply + 1, depth - 1, alpha, beta);
            }
            unmake(mv[i], r);                           // always unmake, even when aborting
            bagH = bagSaved;
            if (aborted) return 0;
            if (rootAll) {
                RootVal rv; rv.m = mv[i]; rv.value = v;
                if (rv.m.kind == MK_SWAP) rv.thrown = sw[rv.m.swapIdx];
                res.rootVals.push_back(rv);
            }
            if (maxing ? v > best : v < best) { best = v; bestI = static_cast<int>(i); }
            if (!rootAll) {
                if (maxing) alpha = std::max(alpha, best); else beta = std::min(beta, best);
                if (cfg.algo == Algo::AlphaBeta && alpha >= beta) {
                    if (mv[i].kind == MK_PLACE) recordCutoff(ply, depth, mv[i]);
                    break;
                }
            }
        }

        if (cfg.useTT) {
            const TTFlag f = best <= alpha0 ? TT_UPPER : (best >= beta0 ? TT_LOWER : TT_EXACT);
            tt.store(key, best, moveSig(mv[bestI]), depth, f, cutoffLeaves == leavesBefore);
        }
        if (ply == 0) {
            res.best = mv[bestI]; res.hasMove = true;
            if (res.best.kind == MK_SWAP) res.swapThrown = sw[res.best.swapIdx];
        }
        return best;
    }
};