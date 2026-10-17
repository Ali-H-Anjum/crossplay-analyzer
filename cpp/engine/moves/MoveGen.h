//MoveGen.h
#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include "Move.h"
#include "Tray.h"
#include "Gaddag.h"
#include "CrossChecks.h"

// Steven Gordon's GADDAG move generation, in the two-phase shape the
// Python used: extendLeft from the anchor, '^' turn, extendRight.
// Generation never writes to the Board's tiles; the only mutable state
// is the rack counts, decremented and restored around each recursion.

inline constexpr bool DEDUPE_ANCHORS     = true;
inline constexpr bool DEDUPE_SINGLE_TILE = true;

inline constexpr int BLANK_SLOT = toIndex(Letter::Blank);

struct RawMove {
    std::array<uint8_t, BOARD_LENGTH> letters;
    uint16_t blank_mask;
    uint8_t  row, col, len;
    bool     is_descending;
};

inline Move toMove(const RawMove& m) {
    std::string w;
    w.reserve(m.len);
    for (int i = 0; i < m.len; ++i) {
        char ch = toChar(fromBoardCode(m.letters[i]));
        if ((m.blank_mask >> i) & 1u)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        w.push_back(ch);
    }
    return Move{ w, m.row, m.col, m.is_descending };
}

struct MoveList {
    std::vector<RawMove> moves;
    void operator()(const RawMove& m) { moves.push_back(m); }
};

struct GenState {
    const Gaddag*     gd = nullptr;      // NEW: needed for step / node lookups
    const uint8_t*    line = nullptr;    // letters along this line
    const uint32_t*   cross = nullptr;   // cross-check masks along this line
    std::array<uint8_t, NUM_LETTER_SLOTS> rack{};
    uint32_t          rack_mask = 0;
    uint16_t          occ = 0;
    uint16_t          line_anchors = 0;
    int               anchor_pos = 0;
    uint8_t           line_idx = 0;
    bool              vertical = false;
    bool              right_clear = false;
    bool              skip_single_tile = false;
    std::array<uint8_t, BOARD_LENGTH> word{};
};

template <class Sink> void extendLeft (GenState& s, Sink& out, int pos, const Arc& arc, uint16_t blanks);
template <class Sink> void extendRight(GenState& s, Sink& out, int pos, const Arc& arc, uint16_t blanks, int lo);

inline bool clearBefore(const GenState& s, int pos) {
    return pos == 0 || !((s.occ >> (pos - 1)) & 1u);
}
inline bool clearAfter(const GenState& s, int pos) {
    return pos == BOARD_LENGTH - 1 || !((s.occ >> (pos + 1)) & 1u);
}

template <class Sink>
inline void emitMove(const GenState& s, Sink& out, int lo, int hi, uint16_t blanks) {
    if constexpr (DEDUPE_SINGLE_TILE) {
        if (s.skip_single_tile) {
            const uint32_t span = ((2u << hi) - 1u) & ~((1u << lo) - 1u);
            if (__builtin_popcount(span & ~static_cast<uint32_t>(s.occ)) == 1) return;
        }
    }
    RawMove m{};
    m.len = static_cast<uint8_t>(hi - lo + 1);
    for (int i = 0; i < m.len; ++i) m.letters[i] = s.word[lo + i];
    m.blank_mask = static_cast<uint16_t>(blanks >> lo);
    if (s.vertical) { m.row = static_cast<uint8_t>(lo); m.col = s.line_idx; m.is_descending = true; }
    else            { m.row = s.line_idx; m.col = static_cast<uint8_t>(lo); m.is_descending = false; }
    out(m);
}

template <class Step>
inline void tryRack(GenState& s, uint32_t cand, int pos, uint16_t blanks, Step&& step) {
    for (uint32_t m = cand & s.rack_mask; m; m &= m - 1) {
        const int idx = __builtin_ctz(m);
        if (--s.rack[idx] == 0) s.rack_mask &= ~(1u << idx);
        step(idx, blanks);
        if (s.rack[idx]++ == 0) s.rack_mask |= (1u << idx);
    }
    if (s.rack[BLANK_SLOT]) {
        --s.rack[BLANK_SLOT];
        const uint16_t with_blank = static_cast<uint16_t>(blanks | (1u << pos));
        for (uint32_t m = cand; m; m &= m - 1) step(__builtin_ctz(m), with_blank);
        ++s.rack[BLANK_SLOT];
    }
}

// Left half. `arc` is the arc INTO the current state; `node` its destination.
// `next` is the arc OUT of the state we just landed on, on letter idx.
template <class Sink>
inline void stepLeft(GenState& s, Sink& out, int pos, int idx,
                     const Arc& arc, const Node& node, uint16_t blanks) {
    s.word[pos] = static_cast<uint8_t>(idx + 1);
    const bool left_clear = clearBefore(s, pos);

    if (((arc.letter_set_mask >> idx) & 1u) && left_clear && s.right_clear)
        emitMove(s, out, pos, s.anchor_pos, blanks);

    const Arc* next = s.gd->arc_at(node, idx);
    if (!next) return;

    if (pos > 0) extendLeft(s, out, pos - 1, *next, blanks);

    if (left_clear && s.anchor_pos < BOARD_LENGTH - 1) {
        if (const Arc* turn = s.gd->step(*next, SEPARATOR_INDEX))
            extendRight(s, out, s.anchor_pos + 1, *turn, blanks, pos);
    }
}

template <class Sink>
inline void stepRight(GenState& s, Sink& out, int pos, int idx,
                      const Arc& arc, const Node& node, uint16_t blanks, int lo) {
    s.word[pos] = static_cast<uint8_t>(idx + 1);

    if (((arc.letter_set_mask >> idx) & 1u) && clearAfter(s, pos))
        emitMove(s, out, lo, pos, blanks);

    if (pos < BOARD_LENGTH - 1) {
        if (const Arc* next = s.gd->arc_at(node, idx))
            extendRight(s, out, pos + 1, *next, blanks, lo);
    }
}

template <class Sink>
void extendLeft(GenState& s, Sink& out, int pos, const Arc& arc, uint16_t blanks) {
    const Node& node = s.gd->node(arc.destination_state);

    if ((s.occ >> pos) & 1u) {
        stepLeft(s, out, pos, s.line[pos] - 1, arc, node, blanks);
        return;
    }
    if constexpr (DEDUPE_ANCHORS) {
        if (pos < s.anchor_pos && ((s.line_anchors >> pos) & 1u)) return;
    }
    const uint32_t cand = (arc.letter_set_mask | node.arc_mask) & s.cross[pos] & ALL_LETTERS;
    if (!cand) return;
    tryRack(s, cand, pos, blanks, [&](int idx, uint16_t bl) {
        stepLeft(s, out, pos, idx, arc, node, bl);
    });
}

template <class Sink>
void extendRight(GenState& s, Sink& out, int pos, const Arc& arc, uint16_t blanks, int lo) {
    const Node& node = s.gd->node(arc.destination_state);

    if ((s.occ >> pos) & 1u) {
        stepRight(s, out, pos, s.line[pos] - 1, arc, node, blanks, lo);
        return;
    }
    const uint32_t cand = (arc.letter_set_mask | node.arc_mask) & s.cross[pos] & ALL_LETTERS;
    if (!cand) return;
    tryRack(s, cand, pos, blanks, [&](int idx, uint16_t bl) {
        stepRight(s, out, pos, idx, arc, node, bl, lo);
    });
}

template <class Sink>
inline void generateMoves(const Gaddag& g, const Board& b, const AnchorLines& al,
                          const Tray& tray, Sink& out) {
    const Arc& init = g.get_init();

    GenState s;
    s.gd = &g;                       // NEW
    s.rack = tray.counts;
    s.rack_mask = tray.uniqueLetterMask();

    // Horizontal: line = row, pos = col.
    s.vertical = false;
    s.skip_single_tile = false;
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        uint16_t bits = al.by_row[r];
        if (!bits) continue;
        s.line_idx = static_cast<uint8_t>(r);
        s.line = &b.letters_row[sq(r, 0)];
        s.cross = &b.cross_check[0][sq(r, 0)];
        s.occ = b.row_occ[r];
        s.line_anchors = bits;
        for (; bits; bits = static_cast<uint16_t>(bits & (bits - 1))) {
            s.anchor_pos = __builtin_ctz(bits);
            s.right_clear = clearAfter(s, s.anchor_pos);
            extendLeft(s, out, s.anchor_pos, init, 0);
        }
    }

    // Vertical: line = col, pos = row.
    s.vertical = true;
    for (int c = 0; c < BOARD_LENGTH; ++c) {
        uint16_t bits = al.by_col[c];
        if (!bits) continue;
        s.line_idx = static_cast<uint8_t>(c);
        s.line = &b.letters_col[sq(c, 0)];
        s.cross = &b.cross_check[1][sq(c, 0)];
        s.occ = b.col_occ[c];
        s.line_anchors = bits;
        for (; bits; bits = static_cast<uint16_t>(bits & (bits - 1))) {
            const int r = __builtin_ctz(bits);
            s.anchor_pos = r;
            s.right_clear = clearAfter(s, r);
            const uint32_t ro = b.row_occ[r];
            s.skip_single_tile = ((((ro << 1) | (ro >> 1)) >> c) & 1u) != 0;
            extendLeft(s, out, r, init, 0);
        }
    }
}

template <class Sink>
inline void generateMovesFromScratch(const Gaddag& g, Board& b, const Tray& tray, Sink& out) {
    AnchorLines al;
    rebuild(b, al, g, g.get_init());
    generateMoves(g, b, al, tray, out);
}
