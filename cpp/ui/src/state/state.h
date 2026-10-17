#pragma once
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

// ================================================================ dictionary
// helper One GADDAG path per split in [0, n-1]. The probe at split == n is not
// used: it would consume all n letters and then test the last letter's bit,
// which can never be a valid encoding (and could false-positive).
static bool TrySplit(const Gaddag& g, const std::string& w, size_t split) {
  const size_t n = w.size();
  const Arc* a = &g.get_init();
  for (size_t i = split; i-- > 0;) {
    a = g.next_arc(*a, w[i]);
    if (!a)
      return false;
  }
  a = g.step(*a, SEPARATOR_INDEX);
  if (!a)
    return false;
  for (size_t i = split; i + 1 < n; ++i) {
    a = g.next_arc(*a, w[i]);
    if (!a)
      return false;
  }
  const uint32_t bit = 1u << (w[n - 1] - 'A');
  return (a->letter_set_mask & bit) != 0;
}
static bool GaddagContains(const Gaddag& g, const std::string& word) {
  if (word.empty())
    return false;
  for (char ch : word)
    if (ch < 'A' || ch > 'Z')
      return false;
  for (size_t s = 0; s < word.size(); ++s)
    if (TrySplit(g, word, s))
      return true;
  return false;
}

static Gaddag* g_gaddag = nullptr;  // owned by main()

// ================================================================ state types
using BagCounts = std::array<uint8_t, NUM_LETTER_SLOTS>;

struct Snapshot {
  Board board{};
  Tray trayA{};
  Tray trayB{};
  std::array<Letter, TOTAL_TILES> bagTiles{};
  uint16_t bagTotal = 0;
  BagCounts bagCounts{};
  std::mt19937_64 bagRng{};
  int score[2] = {0, 0};
};
struct Entry : Snapshot {
  std::string label;
  int turnNumber = 0;
  int player = 0;  // 0/1 for PvB and Sim, -1 for Exploration
};
struct InvalidRun {
  int r0, c0, r1, c1;
  bool descending;
  std::string word;
};

struct ModeState {
  Board board{};
  AnchorLines anchors{};
  Tray trayA{};
  Tray trayB{};
  std::unique_ptr<TileBag> bag;
  int score[2] = {0, 0};

  Snapshot initial;
  std::vector<Entry> history;
  int cursor = 0;  // number of entries currently applied

  std::vector<InvalidRun> invalidRuns;
  bool boardValid = true;

  bool atEnd() const { return cursor == (int)history.size(); }
  const Snapshot& turnStart() const {
    if (cursor == 0 || history.empty())
      return initial;
    return history[cursor - 1];
  }
};

inline ModeState g_ex;
inline ModeState g_pvb;
inline ModeState g_simState;
inline BotPanel g_bot;

static ModeState& StateForMode(AppMode m) {
  return m == AppMode::Exploration   ? g_ex
         : m == AppMode::PlayerVsBot ? g_pvb
                                     : g_simState;
}

// ================================================================ shared UI
// state
enum class ConfirmAction { Title, Rematch };
enum class DragSource { None, Tray, Board };

struct DragState {
  DragSource src = DragSource::None;
  Letter letter = Letter::A;
  bool isBlank = false;
  int fromR = 0, fromC = 0;
};

struct UiState {
  std::string lastAction = "(no action yet)";
  bool dockBuiltEx = false, dockBuiltPvb = false, dockBuiltSim = false;
  bool howToPlayOpen = false;

  bool confirmBackOpen = false;
  ConfirmAction confirmAction = ConfirmAction::Title;

  DragState drag;
  bool dragging = false;

  bool blankPopupOpen = false;
  ModeState* blankMs = nullptr;
  int blankDropR = 0, blankDropC = 0;

  bool addTilePopupOpen = false;

  bool savePopupOpen = false;
  bool loadPopupOpen = false;
  char saveName[64] = "session";
  std::string saveMessage;
  std::string loadMessage;
  std::vector<std::string> loadFiles;
  bool loadListDirty = true;

  float lastViewportW = 0.0f;
  float lastViewportH = 0.0f;
};

inline UiState g_ui;

static void EndDrag() {
  g_ui.dragging = false;
  g_ui.drag.src = DragSource::None;
}

// Board hover state, captured inside the board child window and consumed by the
// tooltip that is drawn outside it (see DrawBoardHoverTooltip).
static int g_boardHoverR = -1;
static int g_boardHoverC = -1;
static bool g_boardHoverOk = false;

// PvB turn state
struct PvbUiState {
  bool firstPlayerDialog = false;
  int turn = 0;  // whose turn: 0 = human, 1 = bot
  int consecutivePasses = 0;
  bool gameOver = false;
  int winner = -1;
  int botWait = 0;
  bool gameOverDismissed =
      false;  // "Game Over" card closed so the final board can be inspected
};

inline PvbUiState g_pvbUi;

// ================================================================
// move-generation panel state
struct ScoredMove {
  RawMove rm;
  MoveScore sc;
  std::string word;
  std::string upper;
};
struct HlCell {
  int r, c;
  char ch;
};

// Three views for the move list:
//   All    -- every legal placement, no dedupe, no cap.
//   Unique -- best placement per distinct word, capped at topN.
//   TopN   -- topN highest-scoring placements, duplicates allowed.
enum class MoveView { All, Unique, TopN };

struct MoveGenState {
  std::vector<ScoredMove> allMoves;
  std::vector<ScoredMove>
      uniqueMoves;  // deduped by word, capped at topN, sorted by moveSortMode
  std::vector<ScoredMove>
      topNMoves;  // topN highest-scoring placements, dupes allowed
  std::vector<int> filtered;
  std::vector<HlCell> hlCells;
  bool movesDirty = true;   // filtered list must be rebuilt
  bool movesStale = false;  // board changed since generation
  MoveView view = MoveView::Unique;
  int hlMoveIdx = -1;
  int topN = 40;
  int moveSortMode = 0;
  char moveFilter[64] = "";

  void Invalidate() {
    movesStale = true;
    hlMoveIdx = -1;
    hlCells.clear();
  }
};

inline MoveGenState g_mg;