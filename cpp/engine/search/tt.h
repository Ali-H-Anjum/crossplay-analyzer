#pragma once
#include <cstdint>
#include <vector>

enum TTFlag : uint8_t { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    uint64_t key = 0;
    int32_t value = 0;
    uint16_t move = 0;  // move signature, used for ordering only (0 = none)
    int8_t depth = 0;   // remaining plies the entry was searched to
    uint8_t bits = 0;   // bits 0-1 flag, bit 2 "solved" (subtree had no depth-limit leaves)
    TTFlag flag() const { return static_cast<TTFlag>(bits & 3); }
    bool solved() const { return (bits & 4) != 0; }
};
static_assert(sizeof(TTEntry) == 16);

class TransTable {
   public:
    explicit TransTable(int bits) : mask((size_t(1) << bits) - 1), t(size_t(1) << bits) {}
    const TTEntry& probe(uint64_t key) const { return t[key & mask]; }
    void store(uint64_t key, int value, uint16_t move, int depth, TTFlag f, bool solved) {
        TTEntry& e = t[key & mask];
        if (e.key == key && e.depth > depth && !solved)
            return;  // keep the deeper entry for the same position
        e.key = key;
        e.value = value;
        e.move = move;
        e.depth = static_cast<int8_t>(depth);
        e.bits = static_cast<uint8_t>(f | (solved ? 4 : 0));
    }

   private:
    size_t mask;
    std::vector<TTEntry> t;
};