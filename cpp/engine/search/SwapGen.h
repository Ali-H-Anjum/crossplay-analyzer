#pragma once
#include "Eval.h"

// Up to 2 swap candidates for the side to move. Returns how many were written.
inline int swapCandidates(const Position& p, Tray out[2]) {
    const Tray& rack = p.tray[p.side];
    int n = 0;
    if (canSwap(p, rack)) out[n++] = rack;
    Tray bad{};
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        if (LEAVE[i] > 0) continue;
        const Letter l = fromIndex(static_cast<uint8_t>(i));
        for (uint8_t c = rack.count(l); c; --c) bad.addTile(l);
    }
    if (bad.size() > 0 && bad.size() < rack.size() && canSwap(p, bad)) out[n++] = bad;
    return n;
}