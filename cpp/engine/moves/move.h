//Move.h

#pragma once

#include <string>
#include <vector>
#include <cctype>
#include "Letter.h"

// A single resolved letter of a Move, at a board square.
struct LetterPlacement {
    Letter letter;
    int row;
    int col;
    bool is_blank; // true if this letter came from a blank tile
};

// A placed word in UI-facing form. Coordinates are (row, col), row 0 = top.
// is_descending = true: vertical, row increasing; false: horizontal, col increasing.
// Lowercase letters in `word` are blanks. Allocates, so it is for the UI and the sim only;
// the search and the move generator use RawMove.
struct Move {
    std::string word;
    int row;
    int col;
    bool is_descending;

    std::vector<LetterPlacement> letterPositions() const {
        std::vector<LetterPlacement> positions;
        positions.reserve(word.size());
        int r = row, c = col;
        for (char ch : word) {
            bool blank = std::islower(static_cast<unsigned char>(ch)) != 0;
            char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            positions.push_back({ letterFromChar(upper), r, c, blank });
            if (is_descending) ++r; else ++c;
        }
        return positions;
    }
};




