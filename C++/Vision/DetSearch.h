#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include "SearchJob.h"

// Tiles the searching side cannot see (opponent rack + bag): full set - board - own rack.
inline std::array<uint8_t, NUM_LETTER_SLOTS> unseenPool(const Board& b, const Tray& own) {
    std::array<uint8_t, NUM_LETTER_SLOTS> pool = DEFAULT_TILE_COUNTS;
    for (int r = 0; r < BOARD_LENGTH; ++r)
        for (uint32_t m = b.row_occ[r]; m; m &= m - 1) {
            const int idx = sq(r, std::countr_zero(m));
            const int slot = b.is_blank.test(idx) ? BLANK_SLOT : b.letters_row[idx] - 1;
            if (pool[slot]) --pool[slot];
        }
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        const uint8_t c = own.count(fromIndex(static_cast<uint8_t>(i)));
        pool[i] = pool[i] > c ? static_cast<uint8_t>(pool[i] - c) : 0;
    }
    return pool;
}

// ---- EDIT HERE ------------------------------------------------------------
struct DetConfig {
    int      threads = 0;        // parallel determinizations, one thread each; 0 = cores - 1 (at least 1)
    uint64_t seed    = 12345;    // same seed + fixed depth = same result
    int rootMaxMoves = 60;
};
// ---------------------------------------------------------------------------

struct DetMove { SMove m{}; Tray thrown{}; double avg = 0; int samples = 0; };
struct DetResult {
    SMove best{}; Tray swapThrown{};   // swapThrown is valid when best.kind == MK_SWAP
    int value = 0;                     // average over determinizations, tenths of a point, root side's view
    uint64_t nodes = 0; int samples = 0; bool hasMove = false;
    std::vector<DetMove> ranked;       // every root move, best first (for the UI)
    int depth = 0, timeMs = 0; 
};

// root.side is the searching side, root.tray[side] is its real rack. root.bag is ignored:
// each job gets its own determinized bag and opponent rack. The root board is only read
// inside the constructor (each job copies it), so the caller may change it afterwards.
class DetSearch {
public:
    static int defaultThreads() {
        const unsigned c = std::thread::hardware_concurrency();
        return c > 1 ? static_cast<int>(c) - 1 : 1;
    }

    DetSearch(const Position& root, const SearchConfig& c, const DetConfig& d = DetConfig{}) {
        const int me = root.side;
        const int n = d.threads > 0 ? d.threads : defaultThreads();
        SearchConfig jc = c; jc.rootValues = true; jc.rootMaxMoves = d.rootMaxMoves;

        const auto pool = unseenPool(*root.b, root.tray[me]);
        int total = 0; for (uint8_t v : pool) total += v;
        const uint8_t oppN = static_cast<uint8_t>(std::min(7, total));   // refilled to 7 while the bag has tiles

        for (int i = 0; i < n; ++i) {
            TileBag bag(mix64(d.seed + static_cast<uint64_t>(i)));
            bag.setPool(pool); bag.shuffle();
            Position pos = root;
            pos.bag = &bag;
            pos.tray[me ^ 1] = Tray{};
            bag.drawNextInto(pos.tray[me ^ 1], oppN);
            jobs.push_back(std::make_unique<SearchJob>(pos, jc));        // copies board + bag, starts its thread
        }
    }
    DetSearch(const DetSearch&) = delete;
    DetSearch& operator=(const DetSearch&) = delete;

    int  threads() const { return static_cast<int>(jobs.size()); }
    void cancel() { for (auto& j : jobs) j->cancel(); }
    bool finished() const { for (auto& j : jobs) if (!j->finished()) return false; return true; }

    // live numbers for the ImGui panel
    uint64_t nodes() const { uint64_t s = 0; for (auto& j : jobs) s += j->live().nodes.load(); return s; }
    int minDepth() const { int m = 1 << 30; for (auto& j : jobs) m = std::min(m, j->live().depth.load()); return m; }
    int elapsedMs() const { int m = 0; for (auto& j : jobs) m = std::max(m, j->live().elapsedMs.load()); return m; }
    const SearchProgress& live(int i) const { return jobs[static_cast<size_t>(i)]->live(); }

    // Joins all jobs and averages each root move's value over the determinizations.
    DetResult take() {
        DetResult out;
        std::vector<SearchResult> rs;
        for (auto& j : jobs) {
            SearchResult r = j->take();
            out.nodes += r.nodes;
            if (r.hasMove) rs.push_back(std::move(r));
        }
        out.samples = static_cast<int>(rs.size());
        out.depth = minDepth(); out.timeMs = elapsedMs();
        if (rs.empty()) return out;

        std::vector<DetMove> acc; std::vector<double> sum;
        std::unordered_multimap<uint16_t, size_t> idx;                    // moveSig -> acc index
        for (const SearchResult& r : rs)
            for (const RootVal& rv : r.rootVals) {
                const uint16_t sig = moveSig(rv.m);
                size_t k = acc.size();
                auto range = idx.equal_range(sig);
                for (auto it = range.first; it != range.second; ++it)
                    if (sameMove(acc[it->second].m, rv.m)) { k = it->second; break; }
                if (k == acc.size()) {
                    DetMove dm; dm.m = rv.m; dm.thrown = rv.thrown;
                    acc.push_back(dm); sum.push_back(0.0); idx.emplace(sig, k);
                }
                sum[k] += rv.value; ++acc[k].samples;
            }
        for (size_t k = 0; k < acc.size(); ++k) acc[k].avg = sum[k] / acc[k].samples;
        std::stable_sort(acc.begin(), acc.end(), [](const DetMove& a, const DetMove& b) {
            return a.samples != b.samples ? a.samples > b.samples : a.avg > b.avg;   // seen by most samples, then best average
        });
        out.ranked = std::move(acc);
        if (!out.ranked.empty()) {
            const DetMove& b = out.ranked[0];
            out.best = b.m; out.swapThrown = b.thrown;
            out.value = static_cast<int>(std::lround(b.avg)); out.hasMove = true;
        }
        return out;
    }

private:
    std::vector<std::unique_ptr<SearchJob>> jobs;
};