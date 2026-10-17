//Tray.h
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "letter.h"

// A rack stored as one count per letter (A-Z plus Blank) rather than a
// list of tiles. Order never matters for move generation, and "how many
// E's do I hold" is a single indexed load.
struct Tray {
    std::array<uint8_t, NUM_LETTER_SLOTS> counts{};
    uint8_t total = 0;

    void addTile(Letter l) {
        ++counts[toIndex(l)];
        ++total;
    }

    void removeTile(Letter l) {
        --counts[toIndex(l)];
        --total;
    }

    // 'A'-'Z' for letters, '?' for a blank.
    void addTiles(const std::string& tiles) {
        for (char c : tiles) addTile(letterFromChar(c));
    }

    uint8_t count(Letter l) const { return counts[toIndex(l)]; }
    uint8_t size() const { return total; }

    // Bitmask of distinct non-blank letters held, computed on demand.
    uint32_t uniqueLetterMask() const {
        uint32_t mask = 0;
        for (int i = 0; i < NUM_LETTERS; ++i)
            if (counts[i]) mask |= (1u << i);
        return mask;
    }
};



