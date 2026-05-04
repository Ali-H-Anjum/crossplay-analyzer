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

## Current: Architecture for AI Search (May 3–)

**Problem:** Search algorithms (minimax, MCTS) need to explore hypothetical game states. Current OOP design has mutable objects with tangled state. Deep copying entire game objects per search node is expensive and error-prone.

**Options under evaluation:**

1. Immutable state objects with copy-on-write semantics
2. State/memento pattern exporting game state separately from logic
3. Bitboard representation for compact, fast-copy state

Will benchmark before choosing.

---

## Key Technical Decisions

| Decision | Rationale |
|----------|-----------|
| No NumPy for board | Avoid dependency; nested lists sufficient for 15×15 |
| GADDAG over DAWG | DAWG requires two-pass search (prefix then suffix); GADDAG handles both in one traversal |
| Python over C (for now) | Faster iteration during research phase; port bottlenecks after AI is working |

---

## Open Questions

- Mutable vs immutable state architecture (active investigation)
- Whether the inverted y-axis coordinate choice should be reverted before search implementation
- GADDAG memory optimization under Python's object overhead