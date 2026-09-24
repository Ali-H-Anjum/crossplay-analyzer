#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Node.h"

class Gaddag {
public:
    explicit Gaddag(const std::string& path = "wordList.txt");

    Arc& get_init() { return init; }
    size_t getSize() const { return size; }

    void add_word(const std::string& word);

private:
    std::vector<std::unique_ptr<Node>> nodes; //Holds States
    Node* root = nullptr;
    Arc init;
    size_t size = 1;

    Node* makeNode();
    Node* add_arc(Node* st, char ch);
    Node* add_final_arc(Node* st, char c1, char c2);
    Arc&  force_arc(Node* st, char ch, Node* force_st);
};


