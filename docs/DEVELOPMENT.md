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
 
## Current: Cache-Friendly Board Representation (September 25–)
 
Turning to the board, tray, and tile bag, since the Python 2D-grid board is likely another performance bottleneck. Plan is to replace it with large static arrays rather than nested lists, using `alignas` and similar layout strategies to keep these structs cache-friendly ahead of the upcoming move generation work. Building a dedicated benchmark for these structures, the same way GADDAG traversal was benchmarked in Phase 6.
 
---