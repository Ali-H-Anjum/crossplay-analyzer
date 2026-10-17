// Arc.h
#pragma once

#include <cstdint>  // UINT32_MAX

#include "letter.h"  // NUM_LETTERS, toIndex, letterFromChar, <cstdint>, uint32_t

inline constexpr char SEPARATOR_CHAR = '^';
inline constexpr int SEPARATOR_INDEX = NUM_LETTERS;
inline constexpr uint32_t ALL_LETTERS = (1u << NUM_LETTERS) - 1;

// Sentinel index for "no destination". Real nodes are < NULL_NODE.
inline constexpr uint32_t NULL_NODE = UINT32_MAX;

inline constexpr int char_to_index(char c) {
  return (c == SEPARATOR_CHAR) ? SEPARATOR_INDEX : toIndex(letterFromChar(c));
}

struct Arc {
  uint32_t letter_set_mask = 0;
  uint32_t destination_state = NULL_NODE;  // index into the node arena

  Arc() = default;
};
