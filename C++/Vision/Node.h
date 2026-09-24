#pragma once

#include <vector>
#include "Arc.h"

struct Node {
    uint32_t arc_mask = 0;     // bit i set => arc exists for char index i
    std::vector<Arc> arcs;     // dense, ordered by popcount rank of arc_mask, typically 1-5 entries, not 27

    Arc* next_arc(char letter) {
        uint32_t bit = 1u << char_to_index(letter);
        if (!(arc_mask & bit)) return nullptr; //If the set doesn't contain bit
        uint32_t rank = __builtin_popcount(arc_mask & (bit - 1)); //size
        return &arcs[rank];
    }

    // Returns existing arc for `letter`, or inserts a fresh one at the
    // correct rank. Safe to hold the returned reference and mutate it
    // immediately, but do NOT hold it across another insertion into
    // this same node's arcs the vector can reallocate.
    Arc& get_or_create_arc(char letter) {
        uint32_t bit = 1u << char_to_index(letter);
        uint32_t rank = __builtin_popcount(arc_mask & (bit - 1));
        if (arc_mask & bit) return arcs[rank];
        arc_mask |= bit; // Add bit to set
        return *arcs.insert(arcs.begin() + rank, Arc{});
    }
};

inline Arc* Arc::next_arc(char letter) const {
    return destination_state->next_arc(letter);
}


