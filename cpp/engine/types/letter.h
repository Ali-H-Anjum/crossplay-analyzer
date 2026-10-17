// Letter.h

#pragma once

#include <cassert>  // assert in fromBoardCode
#include <cstdint>  // uint8_t

// Canonical letter representation shared by TileBag, Tray and Board.
//   Letter:      A-Z = 0-25, Blank = 26      (Tray counts, TileBag, move letters)
//   Board code:  empty = 0, A-Z = 1-26       (Board::letters_row / letters_col)
//   Gaddag:      A-Z = 0-25, '^' = 26        (arc indices; see Arc.h)
// A blank on the board is stored as the letter it stands for, with is_blank set.

enum class Letter : uint8_t {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Blank
};  // 1-byte English alphabet (A=0 to Z=25) + Blank (26)

constexpr int NUM_LETTERS = 26;
constexpr int NUM_LETTER_SLOTS = 27;

constexpr uint8_t toIndex(Letter l) {  // Converts Letter to Byte | (A-Z, Blank) -> (0-25, 26)
    return static_cast<uint8_t>(l);
}

constexpr Letter fromIndex(uint8_t i) {  // Converts Byte to Letter | (0-25, 26) -> (A-Z, Blank)
    return static_cast<Letter>(i);
}

// Could be turned into 2 functions between Char and Letter
constexpr char BLANK_CHAR = '?';

constexpr char toChar(Letter l) {  // Converts Letter to Char | (A-Z, Blank) -> ('A'-'Z', '?')
    return (l == Letter::Blank) ? BLANK_CHAR : static_cast<char>('A' + toIndex(l));
}

constexpr Letter letterFromChar(
    char c) {  // Converts Char to Letter | ('A'-'Z', '?') -> (A-Z, Blank)
    return (c == BLANK_CHAR) ? Letter::Blank : fromIndex(static_cast<std::uint8_t>(c - 'A'));
}

// Board's square: (0, A-Z)
constexpr uint8_t EMPTY_SQUARE = 0;

constexpr uint8_t toBoardCode(Letter l) {  // Converts Letter to Byte | (A-Z) -> (1-26)
    return static_cast<uint8_t>(toIndex(l) + 1);
}

constexpr Letter fromBoardCode(
    uint8_t c) {  // Converts Byte to Letter (Skips 0/Empty) | (1-26) -> (A-Z)
    assert(c != EMPTY_SQUARE);
    return fromIndex(static_cast<uint8_t>(c - 1));
}

constexpr uint8_t boardCodeToIndex(uint8_t c) {  // (1-26) -> (0-25), the index Gaddag arcs use
    return toIndex(fromBoardCode(c));            // fromBoardCode asserts c != EMPTY_SQUARE
}
