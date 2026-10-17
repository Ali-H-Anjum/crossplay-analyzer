#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

// ================================================================ board scanning / validation
template <class F>
static void ForEachRun(const Board& b, F&& fn) {
    for (int dir = 0; dir < 2; ++dir) {
        const bool desc = (dir == 1);
        for (int line = 0; line < BOARD_LENGTH; ++line) {
            auto at = [&](int k) { return b.letters_row[desc ? sq(k, line) : sq(line, k)]; };
            int i = 0;
            while (i < BOARD_LENGTH) {
                if (at(i) == EMPTY_SQUARE) {
                    ++i;
                    continue;
                }
                const int start = i;
                std::string w;
                while (i < BOARD_LENGTH && at(i) != EMPTY_SQUARE) {
                    w.push_back(toChar(fromBoardCode(at(i))));
                    ++i;
                }
                if (w.size() >= 2) fn(desc, line, start, i - 1, w);
            }
        }
    }
}

static void ValidateBoardFull(ModeState& ms) {
    ms.invalidRuns.clear();
    ms.boardValid = true;
    if (!g_gaddag) return;
    ForEachRun(ms.board, [&](bool desc, int line, int s, int e, const std::string& w) {
        if (GaddagContains(*g_gaddag, w)) return;
        ms.boardValid = false;
        if (desc)
            ms.invalidRuns.push_back({s, line, e, line, true, w});
        else
            ms.invalidRuns.push_back({line, s, line, e, false, w});
    });
}

[[maybe_unused]] static bool IsSquareInInvalidRun(const ModeState& ms, int r, int c) {
    for (const auto& run : ms.invalidRuns) {
        if (run.descending) {
            if (c == run.c0 && r >= run.r0 && r <= run.r1) return true;
        } else {
            if (r == run.r0 && c >= run.c0 && c <= run.c1) return true;
        }
    }
    return false;
}

static bool IsAnchorAgainst(const Board& ref, int r, int c) {
    if (ref.letters_row[sq(r, c)] != EMPTY_SQUARE) return false;
    auto occ = [&](int rr, int cc) {
        return rr >= 0 && cc >= 0 && rr < BOARD_LENGTH && cc < BOARD_LENGTH &&
               ref.letters_row[sq(rr, cc)] != EMPTY_SQUARE;
    };
    if (occ(r - 1, c) || occ(r + 1, c) || occ(r, c - 1) || occ(r, c + 1)) return true;
    // No occupied neighbour: only the center of an empty board is an anchor.
    for (int i = 0; i < NUM_SQUARES; ++i)
        if (ref.letters_row[i] != EMPTY_SQUARE) return false;
    return r == BOARD_LENGTH / 2 && c == BOARD_LENGTH / 2;
}

static void FullCrossRecompute(ModeState& ms) {
    if (!g_gaddag) return;
    rebuild(ms.board, ms.anchors, *g_gaddag, g_gaddag->get_init());
}

// Every board mutation made through the UI funnels through here.
static void RefreshDerived(ModeState& ms) {
    FullCrossRecompute(ms);
    ValidateBoardFull(ms);
    if (&ms == &g_ex) g_mg.Invalidate();
}

// ================================================================ placement rules + scoring
static bool BoardsEqual(const Board& a, const Board& b) {
    for (int i = 0; i < NUM_SQUARES; ++i) {
        if (a.letters_row[i] != b.letters_row[i]) return false;
        if (a.is_blank.test(i) != b.is_blank.test(i)) return false;
    }
    return true;
}
static bool TraysEqual(const Tray& a, const Tray& b) {
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        if (a.counts[i] != b.counts[i]) return false;
    return true;
}
// A turn is "uncommitted" when tiles have been placed on the board since the last
// committed position. Tray changes alone (adding tiles, right-click returning a
// tile) don't count as a staged move.
static bool HasUncommitted(const ModeState& ms) {
    return !BoardsEqual(ms.board, ms.turnStart().board);
}

static int TileValueAt(const Board& b, int idx) {
    return b.is_blank.test(idx) ? 0 : LetterPoints(fromBoardCode(b.letters_row[idx]));
}

static void FindRun(const Board& b, int r, int c, int dr, int dc, int& sr, int& sc, int& len) {
    sr = r;
    sc = c;
    while (sr - dr >= 0 && sc - dc >= 0 && b.letters_row[sq(sr - dr, sc - dc)] != EMPTY_SQUARE) {
        sr -= dr;
        sc -= dc;
    }
    len = 0;
    int rr = sr, cc = sc;
    while (rr < BOARD_LENGTH && cc < BOARD_LENGTH && b.letters_row[sq(rr, cc)] != EMPTY_SQUARE) {
        ++len;
        rr += dr;
        cc += dc;
    }
}

struct Placement {
    bool ok = false;
    std::string error;
    int points = 0;
    std::string word;
    int row = 0, col = 0;
    bool desc = false;
    int tiles = 0;
};

// Compares `b` against `ref` (the board at the start of the turn). The squares that
// are newly occupied are the tiles being played. Checks geometry/connectivity and
// scores the move; premium squares are applied only to the new tiles.
// Dictionary validity is checked separately (ModeState::boardValid).
static Placement EvaluatePlacement(const Board& b, const Board& ref) {
    Placement p;
    std::bitset<NUM_SQUARES> isNew;
    std::vector<std::pair<int, int>> placed;
    int occupied = 0;
    for (int r = 0; r < BOARD_LENGTH; ++r) {
        for (int c = 0; c < BOARD_LENGTH; ++c) {
            const int idx = sq(r, c);
            const bool now = b.letters_row[idx] != EMPTY_SQUARE;
            const bool was = ref.letters_row[idx] != EMPTY_SQUARE;
            if (now) ++occupied;
            if (was && !now) {
                p.error = "A previously committed tile was removed.";
                return p;
            }
            if (now && !was) {
                isNew.set(idx);
                placed.push_back({r, c});
            }
        }
    }
    if (placed.empty()) {
        p.error = "Place at least one tile.";
        return p;
    }

    bool sameRow = true, sameCol = true;
    for (auto& pl : placed) {
        if (pl.first != placed[0].first) sameRow = false;
        if (pl.second != placed[0].second) sameCol = false;
    }
    if (!sameRow && !sameCol) {
        p.error = "New tiles must lie in one row or one column.";
        return p;
    }

    bool desc;
    if (placed.size() == 1) {
        int sr, sc, h, v;
        FindRun(b, placed[0].first, placed[0].second, 0, 1, sr, sc, h);
        FindRun(b, placed[0].first, placed[0].second, 1, 0, sr, sc, v);
        if (h < 2 && v < 2) {
            p.error = "A move must form a word of 2 or more letters.";
            return p;
        }
        desc = (h < 2);
    } else {
        desc = sameCol;
    }
    const int dr = desc ? 1 : 0, dc = desc ? 0 : 1;

    int sr, sc, len;
    FindRun(b, placed[0].first, placed[0].second, dr, dc, sr, sc, len);
    for (auto& pl : placed) {
        const bool inRun = desc ? (pl.second == sc && pl.first >= sr && pl.first < sr + len)
                                : (pl.first == sr && pl.second >= sc && pl.second < sc + len);
        if (!inRun) {
            p.error = "Gap in the new tiles.";
            return p;
        }
    }

    bool touchesOld = false;
    auto scoreRun = [&](int r0, int c0, int n, int rdr, int rdc) {
        int sum = 0, wmul = 1;
        for (int k = 0; k < n; ++k) {
            const int r = r0 + k * rdr, c = c0 + k * rdc, idx = sq(r, c);
            int v = TileValueAt(b, idx);
            if (isNew.test(idx)) {
                switch (premiumAt(r, c)) {
                    case MULT_DL:
                        v *= 2;
                        break;
                    case MULT_TL:
                        v *= 3;
                        break;
                    case MULT_DW:
                        wmul *= 2;
                        break;
                    case MULT_TW:
                        wmul *= 3;
                        break;
                    default:
                        break;
                }
            } else
                touchesOld = true;
            sum += v;
        }
        return sum * wmul;
    };

    int total = scoreRun(sr, sc, len, dr, dc);
    std::string word;
    for (int k = 0; k < len; ++k)
        word.push_back(toChar(fromBoardCode(b.letters_row[sq(sr + k * dr, sc + k * dc)])));

    for (auto& pl : placed) {
        int cr, cc, cl;
        FindRun(b, pl.first, pl.second, dc, dr, cr, cc, cl);  // perpendicular run
        if (cl >= 2) total += scoreRun(cr, cc, cl, dc, dr);
    }

    if (occupied == (int)placed.size()) {  // first move of the game
        const int C = BOARD_LENGTH / 2;
        if (!isNew.test(sq(C, C))) {
            p.error = "First move must cover the center square.";
            return p;
        }
    } else if (!touchesOld) {
        p.error = "Move must connect to existing tiles.";
        return p;
    }
    if ((int)placed.size() == kRackSize) total += kBingoBonus;

    p.ok = true;
    p.points = total;
    p.word = word;
    p.row = sr;
    p.col = sc;
    p.desc = desc;
    p.tiles = (int)placed.size();
    return p;
}

static std::string FormatMoveLabel(const std::string& word, int row, int col, bool desc,
                                   int points) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s (%d,%d) %s +%d", word.c_str(), row, col, desc ? "V" : "H",
                  points);
    return buf;
}
static std::string FormatMove(const Placement& p) {
    return FormatMoveLabel(p.word, p.row, p.col, p.desc, p.points);
}

// Places a generated move on the board and removes its tiles from the tray.
// Used by the Exploration "Play Move" button when a move-generation row is
// highlighted. Staged tiles must be empty (caller checks). Assumes g_gaddag
// is loaded and the move is valid for the current board.
static void ApplyHighlightedMove(ModeState& ms, const RawMove& rm) {
    const int dr = rm.is_descending ? 1 : 0;
    const int dc = rm.is_descending ? 0 : 1;
    int r = rm.row, c = rm.col;
    for (int i = 0; i < rm.len; ++i, r += dr, c += dc) {
        const int idx = sq(r, c);
        if (ms.board.letters_row[idx] != EMPTY_SQUARE) continue;  // pre-existing tile
        const bool isBlank = (rm.blank_mask >> i) & 1u;
        const Letter L = fromBoardCode(rm.letters[i]);
        placeTile(ms.board, r, c, L, isBlank);
        ms.trayA.removeTile(isBlank ? Letter::Blank : L);
    }
    RefreshDerived(ms);
    g_ui.lastAction = "Played highlighted move.";
}

// A tile can be dropped on any empty, in-bounds square. Connectivity / center-square
// rules are enforced at commit time by EvaluatePlacement (which gates Play Move).
static bool CanDropTile(const Board& b, int r, int c, std::string& reason) {
    if (r < 0 || c < 0 || r >= BOARD_LENGTH || c >= BOARD_LENGTH) {
        reason = "Out of bounds.";
        return false;
    }
    if (b.letters_row[sq(r, c)] != EMPTY_SQUARE) {
        reason = "Square already occupied.";
        return false;
    }
    return true;
}

// ================================================================ snapshots / history
// Fixed-capacity tray listing. No heap allocation.
struct TrayOrder {
    std::array<Letter, TOTAL_TILES> v{};
    int n = 0;
    int size() const { return n; }
    Letter operator[](int i) const { return v[i]; }
};

static TrayOrder TrayLetters(const Tray& t) {
    TrayOrder o;
    for (int i = 0; i < NUM_LETTER_SLOTS; ++i)
        for (uint8_t k = 0; k < t.counts[i]; ++k) o.v[o.n++] = fromIndex((uint8_t)i);
    return o;
}

static Snapshot MakeSnapshot(const Board& board, const Tray& a, const Tray& b, TileBag* bag, int s0,
                             int s1) {
    Snapshot s;
    s.board = board;
    s.trayA = a;
    s.trayB = b;
    s.score[0] = s0;
    s.score[1] = s1;
    if (bag) {
        s.bagTiles = bag->tilesCopy();
        s.bagTotal = bag->totalCount();
        s.bagRng = bag->rngCopy();
        s.bagCounts = bag->counts();
    }
    return s;
}
static Snapshot SnapshotOf(ModeState& ms) {
    return MakeSnapshot(ms.board, ms.trayA, ms.trayB, ms.bag.get(), ms.score[0], ms.score[1]);
}

// Board / trays / score only (no bag, no derived data). Used by the sim viewer.
static void ApplyVisible(ModeState& ms, const Snapshot& s) {
    ms.board = s.board;
    ms.trayA = s.trayA;
    ms.trayB = s.trayB;
    ms.score[0] = s.score[0];
    ms.score[1] = s.score[1];
}
static void ApplySnapshot(ModeState& ms, const Snapshot& s) {
    ApplyVisible(ms, s);
    if (ms.bag) {
        ms.bag->restoreFrom(s.bagTiles, s.bagTotal);
        ms.bag->restoreRng(s.bagRng);
    }
    RefreshDerived(ms);
}

static void PushEntry(ModeState& ms, const std::string& label, int player) {
    if (ms.cursor < (int)ms.history.size())
        ms.history.erase(ms.history.begin() + ms.cursor, ms.history.end());
    Entry e;
    static_cast<Snapshot&>(e) = SnapshotOf(ms);
    e.label = label;
    e.turnNumber = (int)ms.history.size() + 1;
    e.player = player;
    ms.history.push_back(std::move(e));
    ms.cursor = (int)ms.history.size();
}

static void GoToCursor(ModeState& ms, int target) {
    target = std::clamp(target, 0, (int)ms.history.size());
    ms.cursor = target;
    EndDrag();
    g_ui.blankPopupOpen = false;  // a pending blank assignment refers to the position we just left
    if (target == 0)
        ApplySnapshot(ms, ms.initial);
    else
        ApplySnapshot(ms, ms.history[target - 1]);
}

static bool BagReturn(TileBag* bag, Letter L) {
    if (!bag) return false;
    auto tiles = bag->tilesCopy();
    const uint16_t total = bag->totalCount();
    if (total >= TOTAL_TILES) return false;
    tiles[total] = L;
    bag->restoreFrom(tiles, (uint16_t)(total + 1));
    return true;
}
