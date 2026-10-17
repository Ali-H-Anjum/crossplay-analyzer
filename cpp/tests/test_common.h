#pragma once
#include <memory>
#include "Gaddag.h"

#ifndef WORDLIST_PATH
#error "WORDLIST_PATH not defined (see CMakeLists)"
#endif

inline const Gaddag& testGaddag() {
    static const std::unique_ptr<Gaddag> g = std::make_unique<Gaddag>(WORDLIST_PATH);
    return *g;
}