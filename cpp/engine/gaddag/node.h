// Node.h
#pragma once

#include <bit>
#include <cstdint>
#include <span>

#include "arc.h"  // Arc, NUM_LETTERS, NULL_NODE

inline constexpr int ARC_SLOTS = NUM_LETTERS + 1;  // 27

struct Node {
  uint32_t arc_mask =
      0;  // bit i set => arc exists for index i (0-25 letters, 26 '^')
  uint32_t arc_offset = 0;  // index into the shared arc arena (Gaddag::pool)

  // Arc leaving this node on index `idx`, or nullptr.
  const Arc* arc_at(std::span<const Arc> arcs, int idx) const {
    const uint32_t bit = 1u << idx;
    if (!(arc_mask & bit))
      return nullptr;
    return &arcs[arc_offset + std::popcount(arc_mask & (bit - 1))];
  }
};