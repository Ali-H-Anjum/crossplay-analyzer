#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include "Position.h"

inline uint64_t mix64(uint64_t x) {                       // splitmix64 finalizer
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31; return x;
}

struct ZKeys {
    uint64_t tile[NUM_SQUARES][27]{};                      // [square][board letter code 1..26]
    uint64_t blank[NUM_SQUARES]{};                         // xor'd in when that tile is a blank
    uint64_t tray[2][NUM_LETTER_SLOTS][8]{};               // [side][letter slot][count 1..7]
    uint64_t side = 0;
    uint64_t finalT[3]{};
    uint64_t bagTotal[TOTAL_TILES + 1]{};
};

inline const ZKeys& zkeys() {
    static const ZKeys z = [] {
        ZKeys k; uint64_t s = 0x2545F4914F6CDD1Dull;
        auto nx = [&] { return mix64(s += 0x9E3779B97F4A7C15ull); };
        for (auto& row : k.tile) for (auto& v : row) v = nx();
        for (auto& v : k.blank) v = nx();
        for (auto& a : k.tray) for (auto& b : a) for (auto& v : b) v = nx();
        k.side = nx();
        for (auto& v : k.finalT) v = nx();
        for (auto& v : k.bagTotal) v = nx();
        return k;
    }();
    return z;
}

// Everything evaluate() and the move generator depend on, plus the upcoming draws
// (bag size + bagH, the swap-history term). Score enters as a difference from `me`'s view.
// Recomputed from scratch: ~100 tile reads + 54 tray reads per interior node.
inline uint64_t hashPosition(const Position& p, int me, uint64_t bagH) {
    const ZKeys& z = zkeys();
    const Board& b = *p.b;
    uint64_t h = bagH;
    for (int r = 0; r < BOARD_LENGTH; ++r)
        for (uint32_t m = b.row_occ[r]; m; m &= m - 1) {
            const int idx = sq(r, std::countr_zero(m));
            h ^= z.tile[idx][b.letters_row[idx]];
            if (b.is_blank.test(idx)) h ^= z.blank[idx];
        }
    for (int s = 0; s < 2; ++s)
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
            const int n = p.tray[s].count(fromIndex(static_cast<uint8_t>(i)));
            if (n) h ^= z.tray[s][i][std::min(n, 7)];
        }
    if (p.side) h ^= z.side;
    h ^= z.finalT[std::min(p.finalTurns, 2)];
    h ^= z.bagTotal[p.bag->size()];
    h ^= mix64(static_cast<uint64_t>(static_cast<int64_t>(p.score[me] - p.score[me ^ 1])) ^ 0xA5A5A5A55A5A5A5Aull);
    return h;
}

// New swap-history term after a swap: order-dependent, so different swap histories differ.
inline uint64_t swapHash(uint64_t bagH, int bagSizeBefore, const Tray& thrown) {
    uint64_t h = mix64((bagH ^ 0x9E3779B97F4A7C15ull) + static_cast<uint64_t>(bagSizeBefore));
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        const int n = thrown.count(fromIndex(static_cast<uint8_t>(i)));
        if (n) h = mix64(h ^ ((static_cast<uint64_t>(i) << 8) | static_cast<uint64_t>(n)));
    }
    return h;
}