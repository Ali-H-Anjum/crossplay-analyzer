//Gaddag.h
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "arc.h"   // NULL_NODE
#include "node.h"

class Gaddag {
public:
    explicit Gaddag(const std::string& path = "wordList.txt");

    // Root's "init" arc - the entry point for lookups.
    const Arc& get_init() const { return init; }

    // ---- arena accessors (read side) ------------------------------------

    std::span<const Arc> arc_span() const { return pool; }

    const Node& node(uint32_t i) const { return nodes[i]; }

    const Arc* arc_at(uint32_t node_idx, int idx) const {
        return nodes[node_idx].arc_at(pool, idx);
    }

    const Arc* arc_at(const Node& n, int idx) const {
        return n.arc_at(pool, idx);
    }

    // Primitive step: resolve the arc's destination, then look up its arc on idx.
    const Arc* step(const Arc& a, int idx) const {
        if (a.destination_state == NULL_NODE) return nullptr;
        return arc_at(a.destination_state, idx);
    }

    const Arc* next_arc(const Arc& a, char letter) const {
        return step(a, char_to_index(letter));
    }

private:
    std::vector<Node> nodes;   // thin nodes, reserved exactly, never reallocated
    std::vector<Arc>  scratch; // 27-slot block per node during build, freed by pack()
    std::vector<Arc>  pool;    // packed arcs, one ascending span per node
    uint32_t root = NULL_NODE; // index of the root node
    Arc init;

    // ---- build side (all index-based) -----------------------------------

    void add_word(const std::string& word);   // build-only, called from the constructor

    uint32_t makeNode();
    uint32_t add_arc(uint32_t st, char ch);
    uint32_t add_final_arc(uint32_t st, char c1, char c2);
    Arc& force_arc(uint32_t st, char ch, uint32_t force_st);
    Arc& get_or_create_arc(uint32_t st, char ch);
    void pack();
};
