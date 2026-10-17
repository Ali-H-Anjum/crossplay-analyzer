//TileBag.h
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <utility>
#include "letter.h"
#include "tray.h"

inline constexpr std::array<uint8_t, NUM_LETTER_SLOTS> DEFAULT_TILE_COUNTS = {
    /* A */ 9, /* B */ 2, /* C */ 2, /* D */ 4, /* E */ 12, /* F */ 2,
    /* G */ 3, /* H */ 3, /* I */ 8, /* J */ 1, /* K */ 1, /* L */ 4,
    /* M */ 2, /* N */ 5, /* O */ 8, /* P */ 2, /* Q */ 1, /* R */ 6,
    /* S */ 5, /* T */ 6, /* U */ 3, /* V */ 2, /* W */ 2, /* X */ 1,
    /* Y */ 2, /* Z */ 1, /* Blank */ 3
};

inline constexpr uint16_t TOTAL_TILES = 100;

// The starting bag, one entry per physical tile, built at compile time.
inline constexpr std::array<Letter, TOTAL_TILES> FULL_BAG = [] {
    std::array<Letter, TOTAL_TILES> bag{};
    int k = 0;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        for (uint8_t n = 0; n < DEFAULT_TILE_COUNTS[i]; ++n)
            bag[k++] = fromIndex(static_cast<uint8_t>(i));
    return bag;
}();

struct SwapUndo { std::array<Letter, 7> old{}; uint8_t k = 0; };

// tiles[0 .. total)            tiles still in the bag
// tiles[total .. TOTAL_TILES)  tiles out of the bag, most recent draw first
//
// Drawing swaps a random bag tile to just past the live region, so the
// bag's own array remembers what was drawn and undoing is O(1) per tile.
// Undo is strictly last-in-first-out.
struct TileBag {
    explicit TileBag(uint64_t seed) : rng(seed) {}

    // Draws up to n tiles straight into the tray. Returns how many were drawn.
    uint8_t drawInto(Tray& tray, uint8_t n) {
        n = static_cast<uint8_t>(std::min<uint16_t>(n, total));
        for (uint8_t i = 0; i < n; ++i) {
            std::uniform_int_distribution<uint32_t> dist(0, total - 1);
            const uint32_t r = dist(rng);
            std::swap(tiles[r], tiles[--total]);
            tray.addTile(tiles[total]);
        }
        return n;
    }

    // Reverses the most recent drawInto(tray, n): takes those n tiles back
    // out of the tray and returns them to the bag.
    void undoDraw(Tray& tray, uint8_t n) {
        for (uint8_t i = 0; i < n; ++i)
            tray.removeTile(tiles[total + i]);
        total += n;
    }

    // Pops the next n tiles from the fixed order. No RNG call, no reordering.
    // Undo with undoDraw(tray, returnedN), strictly LIFO.
    uint8_t drawNextInto(Tray& tray, uint8_t n) {
        n = static_cast<uint8_t>(std::min<uint16_t>(n, total));
        for (uint8_t i = 0; i < n; ++i) tray.addTile(tiles[--total]);
        return n;
    }

    // Root determinization: load the unseen pool, then shuffle once.
    void setPool(const std::array<uint8_t, NUM_LETTER_SLOTS>& counts) {
        total = 0;
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
            for (uint8_t n = 0; n < counts[i]; ++n)
                tiles[total++] = fromIndex(static_cast<uint8_t>(i));
    }
    void shuffle() { std::shuffle(tiles.begin(), tiles.begin() + total, rng); }

    uint16_t size() const { return total; }
    bool empty() const { return total == 0; }

    // --- add these, public ---
    uint8_t count(Letter l) const {
        uint8_t n = 0;
        for (uint16_t i = 0; i < total; ++i) if (tiles[i] == l) ++n;
        return n;
    }
    std::array<uint8_t, NUM_LETTER_SLOTS> counts() const {
        std::array<uint8_t, NUM_LETTER_SLOTS> c{};
        for (uint16_t i = 0; i < total; ++i) ++c[toIndex(tiles[i])];
        return c;
    }

    // Remove a specific letter from the bag (e.g. when the script places a tile
    // that isn't coming from the tray). Returns true if one was removed.
    bool removeLetter(Letter l) {
        for (uint16_t i = 0; i < total; ++i) {
            if (tiles[i] == l) {
                std::swap(tiles[i], tiles[--total]);
                return true;
            }
        }
        return false;
    }

    // --- snapshot / restore (used by the simulation rewind) ---
    std::array<Letter, TOTAL_TILES> tilesCopy() const { return tiles; }
    uint16_t totalCount() const { return total; }
    void restoreFrom(const std::array<Letter, TOTAL_TILES>& t, uint16_t tot) {
        tiles = t;
        total = tot;
    }

    // RNG snapshot for deterministic replay after rewind.
    std::mt19937_64 rngCopy() const { return rng; }
    void restoreRng(const std::mt19937_64& r) { rng = r; }

        // Swap: draw k from the top, then put ret[0..k) back at the bottom (displaced
    // bottom tiles move to the top). Requires k <= size(). Undo is strictly LIFO.
    void swapIn(Tray& tray, const Letter* ret, uint8_t k, SwapUndo& u) {
        u.k = k;
        for (uint8_t i = 0; i < k; ++i) tray.addTile(tiles[--total]);
        for (uint8_t j = 0; j < k; ++j) {
            const uint16_t top = total + j;
            u.old[j] = tiles[top];
            tiles[top] = ret[j];
            std::swap(tiles[top], tiles[j]);
        }
        total += k;
    }
    void undoSwap(Tray& tray, const SwapUndo& u) {
        total -= u.k;
        for (int j = u.k; j-- > 0;) {
            const uint16_t top = total + j;
            std::swap(tiles[top], tiles[j]);
            tiles[top] = u.old[j];
        }
        undoDraw(tray, u.k);
    }

private:
    std::array<Letter, TOTAL_TILES> tiles = FULL_BAG;
    uint16_t total = TOTAL_TILES;
    std::mt19937_64 rng;
};