#pragma once

#include "board_rules.h"

// Part of the main.cpp split. Not standalone: main.cpp includes these in order.
#include <unordered_set>

#include "state/state.h"

// ================================================================ move
// generation panel
static bool MoveLess(const ScoredMove& a, const ScoredMove& b, int mode) {
  auto pos = [](const ScoredMove& m) {
    return std::make_tuple((int)m.rm.is_descending, (int)m.rm.row,
                           (int)m.rm.col);
  };
  switch (mode) {
    case 1:
      if (a.rm.len != b.rm.len)
        return a.rm.len > b.rm.len;
      if (a.upper != b.upper)
        return a.upper < b.upper;
      return pos(a) < pos(b);
    case 2:
      if (a.upper != b.upper)
        return a.upper < b.upper;
      return pos(a) < pos(b);
    case 3:
      if (pos(a) != pos(b))
        return pos(a) < pos(b);
      return a.upper < b.upper;
    default:
      if (a.sc.points != b.sc.points)
        return a.sc.points > b.sc.points;
      if (a.upper != b.upper)
        return a.upper < b.upper;
      // Same word and same score: vertical before horizontal.
      if (a.rm.is_descending != b.rm.is_descending)
        return a.rm.is_descending;
      if (a.rm.row != b.rm.row)
        return a.rm.row < b.rm.row;
      return a.rm.col < b.rm.col;
  }
}

// Rebuilds the three derived views from g_mg.allMoves:
//   uniqueMoves -- best placement per distinct word, capped at topN.
//   topNMoves   -- topN highest-scoring placements, duplicates allowed.
// Both are then ordered by the current sort mode.
static void RebuildMoveViews() {
  // Unique: iterate the score-sorted list, keep the first (highest-scoring)
  // placement for each distinct word, cap at topN.
  std::vector<int> idx(g_mg.allMoves.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(), [](int a, int b) {
    return MoveLess(g_mg.allMoves[a], g_mg.allMoves[b], 0);
  });

  g_mg.uniqueMoves.clear();
  std::unordered_set<std::string> seen;
  for (int i : idx) {
    if (!seen.insert(g_mg.allMoves[i].upper).second)
      continue;
    g_mg.uniqueMoves.push_back(g_mg.allMoves[i]);
    if (g_mg.topN > 0 && (int)g_mg.uniqueMoves.size() >= g_mg.topN)
      break;
  }

  // TopN: highest-scoring placements, duplicates allowed, cap at topN.
  g_mg.topNMoves.clear();
  for (int i = 0; i < (int)g_mg.allMoves.size() &&
                  (g_mg.topN <= 0 || (int)g_mg.topNMoves.size() < g_mg.topN);
       ++i)
    g_mg.topNMoves.push_back(g_mg.allMoves[i]);

  auto sortByMode = [](std::vector<ScoredMove>& v) {
    std::sort(v.begin(), v.end(), [](const ScoredMove& a, const ScoredMove& b) {
      return MoveLess(a, b, g_mg.moveSortMode);
    });
  };
  sortByMode(g_mg.uniqueMoves);
  sortByMode(g_mg.topNMoves);

  g_mg.movesDirty = true;
}

static void RunMoveGeneration() {
  MoveList sink;
  generateMovesFromScratch(*g_gaddag, g_ex.board, g_ex.trayA,
                           sink);  // rebuilds cross-checks itself
  g_mg.allMoves.clear();
  g_mg.allMoves.reserve(sink.moves.size());
  for (auto& rm : sink.moves) {
    g_mg.allMoves.push_back(
        {rm, scoreRawFull(g_ex.board, rm), std::string(), std::string()});
    ScoredMove& sm = g_mg.allMoves.back();
    sm.word = toMove(rm).word;
    sm.upper = sm.word;
    for (char& ch : sm.upper)
      ch = (char)std::toupper((unsigned char)ch);
  }
  g_mg.moveSortMode = 0;
  std::sort(g_mg.allMoves.begin(), g_mg.allMoves.end(),
            [](const ScoredMove& a, const ScoredMove& b) {
              return MoveLess(a, b, 0);
            });
  RebuildMoveViews();
  g_mg.hlMoveIdx = -1;
  g_mg.hlCells.clear();
  g_mg.movesStale = false;
  g_ui.lastAction = "Generated moves.";
}

static void HighlightRaw(const RawMove& rm) {
  g_mg.hlCells.clear();
  Move mv = toMove(rm);
  for (const auto& pl : mv.letterPositions()) {
    char ch = toChar(pl.letter);
    if (pl.is_blank)
      ch = (char)std::tolower((unsigned char)ch);
    g_mg.hlCells.push_back({pl.row, pl.col, ch});
  }
}

static void SelectMove(const ScoredMove& sm) {
  // Reject if the user has already staged tiles this turn.
  if (HasUncommitted(g_ex)) {
    g_mg.hlMoveIdx = -1;
    g_mg.hlCells.clear();
    g_ui.lastAction = "Cannot highlight: clear staged tiles first.";
    return;
  }
  g_bot.clearSel();  // un-mark any Bot Search row
  HighlightRaw(sm.rm);
}

// Bot Search row callback: nullptr clears the highlight.
static void BotSelect(const RawMove* rm) {
  g_mg.hlMoveIdx = -1;
  g_mg.hlCells.clear();
  if (!rm)
    return;
  if (HasUncommitted(g_ex)) {
    g_bot.clearSel();
    g_ui.lastAction = "Cannot highlight: clear staged tiles first.";
    return;
  }
  HighlightRaw(*rm);
}

// The Bot Search row to play, or nullptr. Only valid while the board tiles and
// rack still match the search and nothing is staged (ApplyHighlightedMove would
// otherwise remove tiles the rack lacks).
static const RawMove* BotPlayable() {
  const RawMove* m = g_bot.selected();
  if (!m || g_bot.running() || HasUncommitted(g_ex))
    return nullptr;
  if (g_ex.board.letters_row != g_bot.rootBoard.letters_row ||
      g_ex.board.is_blank != g_bot.rootBoard.is_blank ||
      !TraysEqual(g_ex.trayA, g_bot.rootRack))
    return nullptr;
  return m;
}

static void DrawMoveGenerationTab() {
  const bool canGenerate = g_ex.boardValid && g_gaddag;
  if (!canGenerate)
    ImGui::BeginDisabled();
  if (ImGui::Button("Run Move Generation"))
    RunMoveGeneration();
  if (!canGenerate)
    ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("Board must be valid.");
  if (g_mg.movesStale && !g_mg.allMoves.empty())
    ImGui::TextColored(ImVec4(0.85f, 0.6f, 0.2f, 1.0f),
                       "Board changed since these moves were generated.");

  ImGui::Separator();

  // View selector: Unique (deduped by word) or All (every placement).
  const bool isUnique = (g_mg.view == MoveView::Unique);
  if (ImGui::RadioButton("Unique", isUnique)) {
    g_mg.view = MoveView::Unique;
    g_mg.hlMoveIdx = -1;
    g_mg.hlCells.clear();
    g_mg.movesDirty = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("All", !isUnique)) {
    g_mg.view =
        MoveView::TopN;  // reuse TopN = "every placement, dupes allowed"
    g_mg.hlMoveIdx = -1;
    g_mg.hlCells.clear();
    g_mg.movesDirty = true;
  }
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(16, 0));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90);
  if (ImGui::InputInt("##topn", &g_mg.topN, 5, 40)) {
    g_mg.topN = std::clamp(g_mg.topN, 1, 100000);
    RebuildMoveViews();
  }
  ImGui::SameLine();
  if (ImGui::Button("Max")) {
    g_mg.topN = std::max(1, (int)g_mg.allMoves.size());
    RebuildMoveViews();
  }

  ImGui::Spacing();

  // Filter and sort on their own row so nothing is cramped.
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##filter", "filter word...", g_mg.moveFilter,
                           sizeof g_mg.moveFilter);
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(16, 0));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(160);
  const char* sortItems[] = {"Sort: Score", "Sort: Length", "Sort: A-Z",
                             "Sort: Position"};
  if (ImGui::Combo("##sort", &g_mg.moveSortMode, sortItems, 4)) {
    std::sort(g_mg.allMoves.begin(), g_mg.allMoves.end(),
              [](const ScoredMove& a, const ScoredMove& b) {
                return MoveLess(a, b, g_mg.moveSortMode);
              });
    RebuildMoveViews();
    g_mg.hlMoveIdx = -1;
    g_mg.hlCells.clear();
  }

  // Pick which list the user is looking at.
  const std::vector<ScoredMove>* listPtr = &g_mg.allMoves;
  if (g_mg.view == MoveView::Unique)
    listPtr = &g_mg.uniqueMoves;
  else if (g_mg.view == MoveView::TopN)
    listPtr = &g_mg.topNMoves;
  const auto& list = *listPtr;

  // Rebuild the filtered index list only when something that affects it
  // changed.
  static std::string lastFilter;
  if (g_mg.movesDirty || lastFilter != g_mg.moveFilter) {
    lastFilter = g_mg.moveFilter;
    std::string pat = g_mg.moveFilter;
    for (char& ch : pat)
      ch = (char)std::toupper((unsigned char)ch);
    g_mg.filtered.clear();
    for (int i = 0; i < (int)list.size(); ++i)
      if (pat.empty() || list[i].upper.find(pat) != std::string::npos)
        g_mg.filtered.push_back(i);
    g_mg.movesDirty = false;
  }

  ImGui::Spacing();
  ImGui::Text("%zu / %zu", g_mg.filtered.size(), list.size());
  if (g_mg.hlMoveIdx >= 0) {
    ImGui::SameLine();
    if (ImGui::Button("Clear highlight")) {
      g_mg.hlMoveIdx = -1;
      g_mg.hlCells.clear();
    }
  }
  ImGui::Separator();

  ImGui::BeginChild("##moves", ImVec2(0, 0), true);
  {
    char header[80];
    std::snprintf(header, sizeof header, "%-3s %4s  %-15.15s  %-9s  %s", "#",
                  "Pts", "Word", "Dir & Pos", "Bingo");
    ImGui::TextUnformatted(header);
    ImGui::Separator();

    ImGuiListClipper clipper;
    clipper.Begin((int)g_mg.filtered.size());
    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const int i = g_mg.filtered[row];
        const ScoredMove& sm = list[i];
        char label[160];
        std::snprintf(label, sizeof label,
                      "%-3d %+4d  %-15.15s  %c (%2d,%2d)  %s", row + 1,
                      sm.sc.points, sm.word.c_str(),
                      sm.rm.is_descending ? 'V' : 'H', (int)sm.rm.row,
                      (int)sm.rm.col, sm.sc.bingo ? "**" : "");
        ImGui::PushID(i);

        if (ImGui::Selectable(label, g_mg.hlMoveIdx == i)) {
          if (g_mg.hlMoveIdx == i) {
            g_mg.hlMoveIdx = -1;
            g_mg.hlCells.clear();
          } else {
            g_mg.hlMoveIdx = i;
            SelectMove(sm);
          }
        }

        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
}
