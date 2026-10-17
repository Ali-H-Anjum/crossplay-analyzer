# Development Log

## Phase 1: Brute Force (March 12–24)

Built a naive move generator by iterating through a 150K word dictionary with progressive filtering. Performance was acceptable for an empty board but collapsed when handling intersections with existing moves. Spent two weeks patching edge cases before accepting the approach doesn't scale.

**Lesson:** When you're writing case-by-case logic for move intersections on paper and running out of paper, the algorithm is wrong.

---

## Phase 2: GADDAG Implementation (April 1–18)

Implemented Gordon's GADDAG data structure for O(1) prefix/suffix lookup during board traversal.

**Hardest problems:**

- **Memory footprint.** A naive trie storing all character-anchored variants of every word was infeasible. Solved by sharing suffix paths and using delimiter markers to indicate reversal points.
- **Correctness verification.** After two weeks of whack-a-mole debugging, I took the algorithm off-screen. Wrote out the full traversal by hand for specific board states. Found three conceptual gaps in my cross-check logic that code inspection had missed entirely.
- **Integration.** Replacing the word-list pipeline meant rethinking how horizontal/vertical move generation shared logic. The GADDAG's anchor-point traversal naturally unifies both directions—one traversal function handles both.
**Result:** Move generation correctness verified against an external solver using fixed board/rack test cases. Now runs as a reproducible test suite.

---

## Phase 3: Complete Game Engine (April 18–27)

Built out remaining systems: tile bag with proper distribution, scoring with multiplier rules, turn management. Engine simulates full games with both players taking highest-scoring moves.

---

## Phase 4: Restructuring, the Immutable-State Question & a Hiatus (April 27 – July 9)

Before starting on search algorithms, spent time on general cleanup: better object responsibility, less redundancy, more readable code. Also started tracking progress in git/GitHub instead of a personal notebook.

Realized partway through that the planned search techniques (minimax, alpha-beta, MCTS) don't fit the existing OOP design well. Search needs to explore many hypothetical states cheaply, but the current objects only expose a "state" that then has to be used to build a new object or a copy — clunky, and error-prone to keep consistent. Concluded that game states should probably be immutable, but still need an object representation for other logic, without a clear design for reconciling the two.

Took a hiatus from the project to spend time with family before resolving this. Work became on-and-off after returning rather than a steady sprint.

---

## Phase 5: GADDAG Minimization (July 9 – August 20)

Coming back to the project, realized the GADDAG from Phase 2 didn't actually match Gordon's paper as closely as intended: it was functionally a plain trie with 2,768,298 nodes, wasn't using letter sets to record which letters complete a word at each node, and its arc traversal was sloppier than it should have been.

Rebuilt the GADDAG and move generation to track the paper more closely, adding letter sets and building the structure in its minimized form.

**Result:** Node count dropped from 2,768,298 to 811,989 — a 71% reduction — with identical move-generation behavior.

---

## Phase 6: Porting to C++ (September 8–23)

With the engine and a properly minimized GADDAG in place, set four goals for the next stage: port the project from Python to C++, use bitmasking wherever possible (letter sets first), cache the cross-checks computed before each move generation pass, and replace the 2D board grid with a flatter, more cache-friendly representation.

Prototyped bitmasking in Python first — a small class wrapping a bitset with bitwise helpers for adding/checking letters — to validate the design before committing to C++. The Python bitmask's actual speedup wasn't worth measuring (interpreter overhead dominates), but testing after every small, incremental change rather than batching changes turned out to save real time on later, harder changes.

Ported the GADDAG/Node/Arc structures to C++ first. Resolved a circular dependency between the two structs by giving `Node` the full definitions and having `Arc` hold a forward-declared pointer back to `Node`.

**Hardest problem — cache performance of arc traversal.** Used Intel PCM to profile `next_arc()` over a 93M-hop benchmark (50 passes):

| Version | `Node.arcs` | `Arc.letter_set` | IPC | L3 hit rate | Wall time | Hops/sec |
|---|---|---|---|---|---|---|
| Initial | `unordered_map` | `unordered_set` | 0.145 | 0.56 | ~12.5s | ~7.3M |
| Optimized | `vector` | 32-bit bitmask | 0.38 | 0.68 | ~3.1s | ~29.9M |

Switching to a vector for child arcs and a bitmask for letter sets roughly quadrupled traversal throughput and cut cache misses substantially. The GADDAG now builds in about 0.5 seconds.

**Result:** GADDAG/Arc/Node port complete and measurably faster than the Python original. Board, tray, and tile bag representations are next.

---

## Phase 7: Cache-Friendly Core Data Structures (September 25 – October 5)

Extended the profiling methodology from Phase 6 to the board, tray, and tile bag. Replaced the Python 2D-grid board with dual row/column `uint8` arrays, a flat `uint32` cross-check mask array, and a bitset for blank flags, laid out to stay L1-resident. Tray became a single 27-slot `uint8` count array with a `uint16` total; tile bag became a flat 100-tile `Letter` array with embedded `mt19937_64` and swap-remove draws.

Benchmarked each with Intel PCM against dedicated microbenchmarks:

| Structure | Throughput |
|---|---|
| Board | ~51.9M ops/sec, all L1-resident |
| Tray | ~2.33B ops/sec |
| TileBag | ~7.4M ops/sec per draw(7)-and-return cycle |
| Cross-checks | ~55M hops/sec |

Then made nodes and arcs fully contiguous in memory — every node pre-allocated with 27 arc slots — cutting GADDAG build time to 0.33s and raising hop rate to **55.3M hops/sec** (1.85× the vector-only version).

**Result:** Core data structures benchmarked and cache-tuned. Ready to build the search layer.

---

## Phase 8: Frontend — Vision (October 6–15)

After a school break, decided the project needed a real UI. The backend work was solid but I had no fast way to inspect positions or watch games. Picked ImGui + SDL2. No prior experience with either, nor with vcpkg or CMake — worked through the setup with AI assistance, which cut the time-to-working-app from what would have been weeks to about a day.

Named the app **Vision**. Three modes:

- **Exploration** — sandbox for move generation and search, with configurable board and rack
- **Player vs Greedy** — play against a bot
- **Bot vs Bot** — simulate full games and export statistics

Went through four iterations:

- **V1** — functional but ugly. Focused on making move generation output match the pre-port Python test case.
- **V2** — established the panel layout and workflow (build in Exploration, then wire into Simulation).
- **V3** — pursued visual fidelity to NYT's design: tile silhouettes, premium square colors, board feel.
- **V4** — added adversarial search configuration (alpha-beta and MCTS settings panels) and the batch simulation runner.

**Hardest problem — layout.** ImGui is immediate-mode, so every widget's position is a runtime computation. Matching a target visual design required more hand-tuning than expected; the tile bag grid alone cost a full day.

**Result:** Three-mode application with real-time board visualization, move generation inspection, and playable bot opponent.

---

## Phase 9: Adversarial Search & Batch Simulation (October 13–16)

Added determinized alpha-beta and MCTS. Both are parameterizable from the UI: alpha-beta exposes iterative deepening, move ordering options, LMR, ProbCut, beam search, transposition table size; MCTS exposes simulations, tree-reuse interval, UCB1 constant, epsilon-greedy rollouts, rollout depth. Search is parallelized across threads (determinizations for AB, independent trees for MCTS).

With search in place, built the batch runner for generating evaluation-function training data. Two bots play N games (paired seatings per deal), and every turn writes a full diagnostic row to CSV:

- Pre-move state (board, rack, bag, scores)
- Greedy best available score from this rack
- Search result (algo, depth, nodes, samples, root move count, value)
- Chosen move's score and rank within the search's root list
- Opponent's true rack (for offline perfect-information analysis)
- Leave value and kept-tile composition
- Premium-square lanes available, lanes covered, opponent's best reply before vs. after

A summary pass computes aggregate statistics — win rates, regret vs. Greedy, leave-value calibration, search depth/time distributions.

1,000 Greedy-vs-Greedy games complete in a few seconds on the current hardware. The data pipeline works; analysis does not.

**Hardest problem — deciding what to record.** The turn-level data has 60+ fields. Most of it is speculative — I don't yet know which features will matter for a better eval. The batch infrastructure errs on the side of collecting too much and winnowing later.

**Result:** Search layer complete, batch simulation infrastructure in place, first large-scale data generation ready to run.

---

## Phase 10: Repository Restructure (October 17)

`main.cpp` had grown past 3,000 lines. Restructured the entire repository before continuing.

**Before:**
crossplay_bot/
├── C++/Vision/ (all engine headers, flat)
├── imgui/ (vendored)
├── python/
├── ui/src/ (3,000-line main.cpp + 15 headers)
└── docs/


**After:**
crossplay_bot/
├── cpp/
│ ├── engine/ (pure game logic, 8 subfolders)
│ ├── ui/ (ImGui + SDL frontend, 5 subfolders)
│ ├── tests/
│ ├── main.cpp
│ └── CMakeLists.txt
├── python/ (legacy, not maintained)
├── data/
├── docs/
└── imgui/


**Changes:**

- **Python** consolidated under `python/`. Frozen — C++ supersedes it. Kept for teaching and prototype value.
- **C++** split into `engine/` (no SDL/imgui) and `ui/` (frontend). Engine can be compiled standalone, wrapped, run headless.
- **Engine subfolders**: `types`, `data`, `gaddag`, `board`, `moves`, `scoring`, `state`, `search`. Every folder depends only on folders to its left.
- **UI subfolders**: `common`, `state`, `widgets`, `panels`, `screens`. Screens call panels, panels call widgets, widgets call state/common.
- **Filenames** converted to Google C++ Style (lowercase snake_case; acronyms lowercase).
- **Wordlists and diagnostic data** moved into `data/` — both gitignored (copyright; regeneratable).
- **Tests** moved from `ui/tests/` to `cpp/tests/` (they cover engine code).
- **Git history** preserved via `git mv` throughout.

Build verified green on MinGW + vcpkg SDL2 (x64-mingw-static). All engine tests pass.

---

## Current State (October 17)

**Working:**

- Complete engine: GADDAG move generation, scoring, board state, tray/bag management
- Determinized alpha-beta with configurable heuristics (LMR, ProbCut, killers, history, beam, TT)
- MCTS with configurable rollout policy
- ImGui frontend with three modes and real-time visualization
- Batch simulation runner (multi-threaded, CSV export, aggregate summary)
- Test suite (`engine_tests`, 0 failures)

**In progress:**

- Evaluation-function training from batch data
- Migration to Ubuntu (more cores, native GCC toolchain)
- Repository polish: README, devlog consolidation

**Up next:**

- Ubuntu build setup and verification
- Public GitHub hosting for AI-assisted code review
- Optionally: split the two largest UI files (`widgets.h` at ~800 lines, `sim.h` at ~2,000 lines) into smaller components
