#pragma once

#include "sim_ui.h"

#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;

// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

// ================================================================ save / load
static fs::path SaveDir() {
    const char* home = std::getenv("USERPROFILE");
    if (!home) home = std::getenv("HOME");
    std::error_code ec;
    if (home) {
        fs::path d = fs::path(home) / "Downloads";
        if (fs::is_directory(d, ec)) return d;
        return fs::path(home);
    }
    return fs::current_path(ec);
}

static std::string SanitizeName(const char* s) {
    std::string out;
    for (const char* p = s; *p && out.size() < 40; ++p)
        if (std::isalnum((unsigned char)*p)) out.push_back(*p);
    return out.empty() ? std::string("session") : out;
}
static int AlnumFilter(ImGuiInputTextCallbackData* d) {
    return (d->EventChar < 128 && std::isalnum((int)d->EventChar)) ? 0 : 1;
}

static const char* ModeName(AppMode m) {
    switch (m) {
        case AppMode::Exploration:
            return "exploration";
        case AppMode::PlayerVsBot:
            return "pvb";
        case AppMode::BotVsBot:
            return "sim";
        default:
            return "unknown";
    }
}

static void WriteSnapshot(std::ostream& o, const Snapshot& s) {
    std::string brd(NUM_SQUARES, '.');
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const int idx = sq(r, c);
            const uint8_t code = s.board.letters_row[idx];
            if (code == EMPTY_SQUARE) continue;
            char ch = toChar(fromBoardCode(code));
            if (s.board.is_blank.test(idx)) ch = (char)std::tolower((unsigned char)ch);
            brd[r * BOARD_LENGTH + c] = ch;
        }
    }
    o << "board " << brd << "\n";
    o << "trayA";
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.trayA.counts[i];
    o << "\n";
    o << "trayB";
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.trayB.counts[i];
    o << "\n";
    o << "bagcounts";
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) o << ' ' << (int)s.bagCounts[i];
    o << "\n";
    o << "bag " << s.bagTotal;
    for (int i = 0; i < (int)s.bagTotal && i < TOTAL_TILES; ++i)
        o << ' ' << (int)toIndex(s.bagTiles[i]);
    o << "\n";
    o << "rng " << s.bagRng << "\n";
    o << "score " << s.score[0] << ' ' << s.score[1] << "\n";
}

static bool ReadSnapshot(std::istream& in, Snapshot& s) {
    std::string tag, brd;
    if (!(in >> tag >> brd) || tag != "board" || (int)brd.size() != NUM_SQUARES) return false;
    s = Snapshot{};
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const char ch = brd[r * BOARD_LENGTH + c];
            if (ch == '.') continue;
            if (!std::isalpha((unsigned char)ch)) return false;
            const bool blank = std::islower((unsigned char)ch) != 0;
            const int li = std::toupper((unsigned char)ch) - 'A';
            if (li < 0 || li >= NUM_LETTERS) return false;
            placeTile(s.board, r, c, fromIndex((uint8_t)li), blank);
        }
    }
    auto readTray = [&](const char* want, Tray& t) {
        if (!(in >> tag) || tag != want) return false;
        t = Tray{};
        for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
            int n;
            if (!(in >> n) || n < 0 || n > TOTAL_TILES) return false;
            for (int k = 0; k < n; ++k) t.addTile(fromIndex((uint8_t)i));
        }
        return true;
    };
    if (!readTray("trayA", s.trayA) || !readTray("trayB", s.trayB)) return false;
    if (!(in >> tag) || tag != "bagcounts") return false;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i) {
        int n;
        if (!(in >> n) || n < 0 || n > 255) return false;
        s.bagCounts[i] = (uint8_t)n;
    }
    int total;
    if (!(in >> tag >> total) || tag != "bag" || total < 0 || total > TOTAL_TILES) return false;
    s.bagTotal = (uint16_t)total;
    for (int i = 0; i < total; ++i) {
        int v;
        if (!(in >> v) || v < 0 || v >= NUM_LETTER_SLOTS) return false;
        s.bagTiles[i] = fromIndex((uint8_t)v);
    }
    if (!(in >> tag) || tag != "rng" || !(in >> s.bagRng)) return false;
    if (!(in >> tag >> s.score[0] >> s.score[1]) || tag != "score") return false;
    return true;
}

static bool SaveState(const fs::path& file, std::string& err) {
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        err = "Could not open file for writing.";
        return false;
    }
    ModeState& ms = StateForMode(g_mode);
    out << "VISION_STATE 3\n";
    out << "mode " << ModeName(g_mode) << "\n";
    out << "cursor " << ms.cursor << "\n";
    if (g_mode == AppMode::PlayerVsBot)
        out << "pvb " << g_pvbUi.turn << ' ' << g_pvbUi.consecutivePasses << ' '
            << (int)g_pvbUi.gameOver << ' ' << g_pvbUi.winner << "\n";
    else
        out << "pvb 0 0 0 -1\n";
    out << "current\n";
    WriteSnapshot(out, SnapshotOf(ms));
    out << "initial\n";
    WriteSnapshot(out, ms.initial);
    out << "entries " << ms.history.size() << "\n";
    for (const Entry& e : ms.history) {
        out << "entry " << e.turnNumber << ' ' << e.player << "\n";
        out << "label " << e.label << "\n";
        WriteSnapshot(out, e);
    }
    out.flush();
    if (!out) {
        err = "Write failed.";
        return false;
    }
    return true;
}

struct LoadedState {
    std::string mode;
    int cursor = 0;
    int pvbTurn = 0, pvbPasses = 0, pvbOver = 0, pvbWinner = -1;
    Snapshot current, initial;
    std::vector<Entry> entries;
};

static bool ReadStateFile(const fs::path& file, LoadedState& L, std::string& err) {
    std::ifstream in(file);
    if (!in) {
        err = "Could not open file.";
        return false;
    }
    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "VISION_STATE") {
        err = "Not a Vision save file.";
        return false;
    }
    if (version != 3) {
        err = "Unsupported save version (" + std::to_string(version) + ").";
        return false;
    }
    if (!(in >> tag >> L.mode) || tag != "mode") {
        err = "Corrupt file (mode).";
        return false;
    }
    if (!(in >> tag >> L.cursor) || tag != "cursor") {
        err = "Corrupt file (cursor).";
        return false;
    }
    if (!(in >> tag >> L.pvbTurn >> L.pvbPasses >> L.pvbOver >> L.pvbWinner) || tag != "pvb") {
        err = "Corrupt file (pvb).";
        return false;
    }
    if (!(in >> tag) || tag != "current" || !ReadSnapshot(in, L.current)) {
        err = "Corrupt file (current).";
        return false;
    }
    if (!(in >> tag) || tag != "initial" || !ReadSnapshot(in, L.initial)) {
        err = "Corrupt file (initial).";
        return false;
    }
    int n;
    if (!(in >> tag >> n) || tag != "entries" || n < 0 || n > 100000) {
        err = "Corrupt file (entries).";
        return false;
    }
    for (int i = 0; i < n; ++i) {
        Entry e;
        if (!(in >> tag >> e.turnNumber >> e.player) || tag != "entry") {
            err = "Corrupt file (entry).";
            return false;
        }
        std::string line;
        std::getline(in, line);  // rest of the "entry" line
        std::getline(in, line);  // "label ..."
        e.label = line.size() > 6 ? line.substr(6) : std::string();
        Snapshot s;
        if (!ReadSnapshot(in, s)) {
            err = "Corrupt file (entry snapshot).";
            return false;
        }
        static_cast<Snapshot&>(e) = s;
        L.entries.push_back(std::move(e));
    }
    L.cursor = std::clamp(L.cursor, 0, (int)L.entries.size());
    return true;
}

static bool ApplyLoadedState(LoadedState& L, std::string& err) {
    if (L.mode != ModeName(g_mode)) {
        err = "That save is for mode '" + L.mode + "'; open that mode to load it.";
        return false;
    }
    ModeState& ms = StateForMode(g_mode);
    if (g_mode != AppMode::BotVsBot && !ms.bag) ms.bag = std::make_unique<TileBag>(uint64_t{1});
    ms.history = std::move(L.entries);
    ms.initial = L.initial;
    ms.cursor = L.cursor;
    EndDrag();
    if (g_mode == AppMode::BotVsBot) {
        g_simUi.started = true;
        g_simUi.replay = true;
        g_simUi.autoRunning = false;
        SimSyncView();
    } else {
        ApplySnapshot(ms, L.current);
        if (g_mode == AppMode::PlayerVsBot) {
            g_pvbUi.turn = L.pvbTurn;
            g_pvbUi.consecutivePasses = L.pvbPasses;
            g_pvbUi.gameOver = L.pvbOver != 0;
            g_pvbUi.gameOverDismissed = false;
            g_pvbUi.winner = L.pvbWinner;
            g_pvbUi.botWait = kBotWaitFrames;
        }
    }
    return true;
}

static void RefreshLoadList() {
    g_ui.loadFiles.clear();
    std::error_code ec;
    for (fs::directory_iterator it(SaveDir(), ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (!it->is_regular_file(ec2)) continue;
        const std::string fn = it->path().filename().string();
        if (fn.size() > 14 && fn.rfind("vision_", 0) == 0 &&
            fn.compare(fn.size() - 7, 7, ".vstate") == 0)
            g_ui.loadFiles.push_back(fn);
    }
    std::sort(g_ui.loadFiles.begin(), g_ui.loadFiles.end());
    g_ui.loadListDirty = false;
}
