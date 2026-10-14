// Board.h
#pragma once

#include <cstdint>
#include <bitset>
#include <array>

constexpr int BOARD_LENGTH = 15;
constexpr int NUM_SQUARES  = BOARD_LENGTH * BOARD_LENGTH;

constexpr int sq(int row, int col) { return row * BOARD_LENGTH + col; } //Returns index from 0 - 224

struct alignas(64) Board {
    // Using arrays to hold the entire board instead of 2D grid
    std::array<uint8_t, NUM_SQUARES> letters_row{}; // idx = row*15+col, (255)
    std::array<uint8_t, NUM_SQUARES> letters_col{}; // idx = col*15+row, (255)

    // Row/Column Occupancy
    std::array<uint16_t, BOARD_LENGTH> row_occ{}; // bit c set => (row,c) occupied, (15)
    std::array<uint16_t, BOARD_LENGTH> col_occ{}; // bit r set => (r,col) occupied, (15)

    // Blank tiles
    std::bitset<NUM_SQUARES> is_blank{}; // (255)

    // Cross-check masks, idx = [orient][line*15+pos]: bit i set => letter i is legal here for
    // the perpendicular word. [0] horizontal generation (line=row, pos=col); [1] vertical
    // generation (line=col, pos=row). Only meaningful at empty squares on anchored lines.
    std::array<std::array<uint32_t, NUM_SQUARES>, 2> cross_check{};

    // Incremental Zobrist hash for TT / MCTS node keys
    //uint64_t zobrist = 0;
};
