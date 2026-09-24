#pragma once

#include <cstdint>

struct Node; //Forward declaration

inline constexpr int char_to_index(char c) {
    return (c == '^') ? 26 : (c - 'A');
}

struct Arc {
    uint32_t letter_mask = 0; //data members
    Node* destination_state = nullptr;

    Arc() = default;  //Default Constructor
    explicit Arc(Node* dest) : destination_state(dest) {} //1 parameter Constructor

    Arc* next_arc(char letter) const;  //method declaration
};


