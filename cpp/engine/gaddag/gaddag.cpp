// Gaddag.cpp
#include "gaddag.h"  // own header first

#include <bit>        // popcount, countr_zero
#include <cctype>     // toupper
#include <fstream>    // ifstream
#include <sstream>    // istringstream
#include <stdexcept>  // runtime_error

Gaddag::Gaddag(const std::string& path) {
    constexpr size_t NODE_COUNT = 811989;
    nodes.reserve(NODE_COUNT);
    scratch.reserve(NODE_COUNT * ARC_SLOTS);

    root = makeNode();  // node 0
    init.destination_state = root;

    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open word list: " + path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            for (char& c : word) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            add_word(word);
        }
    }
    pack();
}

// Append a node with a fresh 27-slot scratch block.
uint32_t Gaddag::makeNode() {
    const uint32_t idx = static_cast<uint32_t>(nodes.size());
    Node& n = nodes.emplace_back();
    n.arc_offset = static_cast<uint32_t>(scratch.size());
    scratch.resize(scratch.size() + ARC_SLOTS);
    return idx;
}

// Build-only: scratch slot for `ch` at node `st`, marking it present.
Arc& Gaddag::get_or_create_arc(uint32_t st, char ch) {
    const int idx = char_to_index(ch);
    Node& n = nodes[st];
    n.arc_mask |= (1u << idx);
    return scratch[n.arc_offset + idx];  // scratch layout: offset + idx
}

// Compact scratch into one pool, arcs ascending by idx ('^' last), then free scratch.
void Gaddag::pack() {
    size_t total = 0;
    for (const Node& n : nodes) total += std::popcount(n.arc_mask);
    pool.reserve(total);

    for (Node& n : nodes) {
        const uint32_t old_off = n.arc_offset;
        const uint32_t new_off = static_cast<uint32_t>(pool.size());
        for (uint32_t m = n.arc_mask; m; m &= m - 1) {
            pool.push_back(scratch[old_off + std::countr_zero(m)]);
        }
        n.arc_offset = new_off;
    }
    std::vector<Arc>().swap(scratch);
}

uint32_t Gaddag::add_arc(uint32_t st, char ch) {
    if (get_or_create_arc(st, ch).destination_state == NULL_NODE) {
        const uint32_t d = makeNode();
        get_or_create_arc(st, ch).destination_state = d;
    }
    return get_or_create_arc(st, ch).destination_state;
}

uint32_t Gaddag::add_final_arc(uint32_t st, char c1, char c2) {
    if (get_or_create_arc(st, c1).destination_state == NULL_NODE) {
        const uint32_t d = makeNode();
        get_or_create_arc(st, c1).destination_state = d;
    }
    Arc& arc = get_or_create_arc(st, c1);  // no allocation after this point
    arc.letter_set_mask |= (1u << char_to_index(c2));
    return arc.destination_state;
}

// Force arc's destination to `force_st`, erroring on conflict. Returns the arc.
Arc& Gaddag::force_arc(uint32_t st, char ch, uint32_t force_st) {
    Arc& arc = get_or_create_arc(st, ch);
    if (arc.destination_state != NULL_NODE && arc.destination_state != force_st) {
        throw std::runtime_error("Conflict: existing destination is not the forced destination");
    }
    arc.destination_state = force_st;
    return arc;
}

void Gaddag::add_word(const std::string& word) {
    const int n = static_cast<int>(word.size());

    uint32_t current_state = root;
    for (int i = n - 1; i >= 2; --i)  // range(n-1, 1, -1)
        current_state = add_arc(current_state, word[i]);
    add_final_arc(current_state, word[1], word[0]);

    current_state = root;
    for (int i = n - 2; i >= 0; --i)  // range(n-2, -1, -1)
        current_state = add_arc(current_state, word[i]);
    current_state = add_final_arc(current_state, SEPARATOR_CHAR, word[n - 1]);

    bool first_iteration = true;
    for (int m = n - 2; m >= 1; --m) {  // range(n-2, 0, -1)
        uint32_t force_st = current_state;
        current_state = root;
        for (int i = m - 1; i >= 0; --i) current_state = add_arc(current_state, word[i]);
        current_state = add_arc(current_state, SEPARATOR_CHAR);
        Arc& arc = force_arc(current_state, word[m], force_st);

        if (first_iteration) {
            arc.letter_set_mask |= (1u << char_to_index(word[n - 1]));
            first_iteration = false;
        }
    }
}
