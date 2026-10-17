#pragma once
#include <array>
#include <cstdint>
#include "Position.h"

// ---- EDIT HERE: features and weights -------------------------------------
enum Feat : int { F_SCORE, F_LEAVE, F_LANES, F_COUNT };
using Weights = std::array<int, F_COUNT>;
//                                    SCORE LEAVE LANES      (tenths of a point per unit)
inline constexpr Weights W_BAG     = {  10,    1,   15 };    // bag still has tiles
inline constexpr Weights W_ENDGAME = {  10,    0,    0 };    // bag empty

// PLACEHOLDER leave values (tenths of a point), Scrabble-ish, NOT tuned for Crossplay.
// Index = letter A..Z, then blank. Same order as POINTS_PER_TILE.
inline constexpr std::array<int16_t, NUM_LETTER_SLOTS> LEAVE = {
    /*A*/ 5,  /*B*/ -20, /*C*/ 0,   /*D*/ 0,   /*E*/ 10,  /*F*/ -20,
    /*G*/ -20,/*H*/ 5,   /*I*/ -5,  /*J*/ -30, /*K*/ -10, /*L*/ 0,
    /*M*/ 0,  /*N*/ 5,   /*O*/ -10, /*P*/ -5,  /*Q*/ -70, /*R*/ 10,
    /*S*/ 80, /*T*/ 0,   /*U*/ -30, /*V*/ -50, /*W*/ -40, /*X*/ 30,
    /*Y*/ -20,/*Z*/ 20,  /*Blank*/ 250
};
// ---------------------------------------------------------------------------

inline int leaveValue(const Tray& t) {
    int v = 0;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) v += LEAVE[i] * t.counts[i];
    return v;
}

inline const std::array<uint8_t, 16> kBigPremium = [] {   // all DW + TW squares
    std::array<uint8_t, 16> a{}; int n = 0;
    for (int i = 0; i < NUM_SQUARES; ++i)
        if (kPremium[i] == MULT_DW || kPremium[i] == MULT_TW) a[n++] = static_cast<uint8_t>(i);
    return a;
}();

// DW/TW squares that are currently anchors (empty and touching a tile).
inline int openBigPremiums(const AnchorLines& al) {
    int n = 0;
    for (uint8_t i : kBigPremium) n += (al.by_row[i / BOARD_LENGTH] >> (i % BOARD_LENGTH)) & 1;
    return n;
}

// Value for `me`, in tenths of a point.
inline int evaluate(const Position& p, int me) {
    const int opp = me ^ 1;
    std::array<int, F_COUNT> f{};
    f[F_SCORE] = p.score[me] - p.score[opp];
    f[F_LEAVE] = leaveValue(p.tray[me]) - leaveValue(p.tray[opp]);
    f[F_LANES] = openBigPremiums(p.al) * (p.side == me ? 1 : -1);
    const Weights& w = p.bag->empty() ? W_ENDGAME : W_BAG;
    int v = 0;
    for (int i = 0; i < F_COUNT; ++i) v += w[i] * f[i];
    return v;
}