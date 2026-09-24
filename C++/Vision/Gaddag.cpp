#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include "Gaddag.h"

static constexpr char END_MARKER = '^';

Gaddag::Gaddag(const std::string& path) {
    root = makeNode();          // nodes[0] is root; size starts at 1
    init.destination_state = root;

    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            for (char& c : word) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            add_word(word);
        }
    }
}

Node* Gaddag::makeNode() {
    nodes.push_back(std::make_unique<Node>());
    return nodes.back().get();
}

Node* Gaddag::add_arc(Node* st, char ch) {
    Arc& arc = st->get_or_create_arc(ch);
    if (arc.destination_state == nullptr) {
        arc.destination_state = makeNode();
        ++size;
    }
    return arc.destination_state;
}

Node* Gaddag::add_final_arc(Node* st, char c1, char c2) {
    Arc& arc = st->get_or_create_arc(c1);
    if (arc.destination_state == nullptr) {
        arc.destination_state = makeNode();
        ++size;
    }
    arc.letter_mask |= (1u << char_to_index(c2));
    return arc.destination_state;
}

Arc& Gaddag::force_arc(Node* st, char ch, Node* force_st) {
    Arc& arc = st->get_or_create_arc(ch);
    if (arc.destination_state != nullptr && arc.destination_state != force_st) {
        throw std::runtime_error("Conflict: existing destination is not the forced destination");
    }
    arc.destination_state = force_st;
    return arc;
}

void Gaddag::add_word(const std::string& word) {
    const int n = static_cast<int>(word.size());

    Node* current_state = root;
    for (int i = n - 1; i >= 2; --i)            // range(n-1, 1, -1)
        current_state = add_arc(current_state, word[i]);
    add_final_arc(current_state, word[1], word[0]);

    current_state = root;
    for (int i = n - 2; i >= 0; --i)            // range(n-2, -1, -1)
        current_state = add_arc(current_state, word[i]);
    current_state = add_final_arc(current_state, END_MARKER, word[n - 1]);

    bool first_iteration = true;
    for (int m = n - 2; m >= 1; --m) {          // range(n-2, 0, -1)
        Node* force_st = current_state;
        current_state = root;
        for (int i = m - 1; i >= 0; --i)
            current_state = add_arc(current_state, word[i]);
        current_state = add_arc(current_state, END_MARKER);
        Arc& arc = force_arc(current_state, word[m], force_st);

        if (first_iteration) {
            arc.letter_mask |= (1u << char_to_index(word[n - 1]));
            first_iteration = false;
        }
    }
}
