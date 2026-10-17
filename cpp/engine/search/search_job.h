#pragma once
#include <thread>

#include "search.h"

// Owns a private copy of the position. Hold it in a std::unique_ptr (it embeds a 4 KB Board).
class SearchJob {
   public:
    SearchJob(const Position& src, const SearchConfig& c)
        : board(*src.b), bag(*src.bag), pos(src), cfg(c) {
        pos.b = &board;
        pos.bag = &bag;
        th = std::thread([this] { result = Searcher(pos, cfg, &progress).run(); });
    }
    SearchJob(const SearchJob&) = delete;
    SearchJob& operator=(const SearchJob&) = delete;
    ~SearchJob() {
        cancel();
        if (th.joinable()) th.join();
    }

    void cancel() { progress.stop.store(true); }
    bool finished() const { return progress.done.load(); }
    const SearchProgress& live() const { return progress; }  // UI polls this each frame
    SearchResult take() {
        if (th.joinable()) th.join();
        return result;
    }

   private:
    Board board;
    TileBag bag;
    Position pos;
    SearchConfig cfg;
    SearchProgress progress;
    SearchResult result;
    std::thread th;
};