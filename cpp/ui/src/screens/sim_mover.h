#pragma once
// Async mover logic for Sim mode: bridges SimGame's async step interface with UI state.
// Depends on sim_game.h only (no UI headers). Callers pass an onTurnComplete callback
// to avoid a circular dependency with sim_ui.h (which defines SimPushHistory).

#include <functional>

#include "sim_game.h"

struct SimUiState;  // full definition in sim_ui.h

// Advance the sim one turn using the async step machine.
// Returns true when a turn was completed (onTurnComplete is invoked).
// Returns false if still thinking or game over.
inline bool SimTryAdvanceAsync(SimGame& sim, SimUiState& ui,
                               std::function<void(const SimGame::Turn&)> onTurnComplete) {
    if (sim.gameOver) return false;
    const int before = (int)sim.turns.size();
    const int p = (int)(sim.turns.size() & 1);

    if (sim.movers[p].kind == MoverKind::Greedy) {
        if (!sim.step()) return false;
        if ((int)sim.turns.size() > before) onTurnComplete(sim.turns.back());
        return true;
    }

    if (!sim.movers[p].search.running() && !sim.movers[p].search.haveResult) {
        sim.beginStep();
    }
    const auto st = sim.pollStep();
    if (st == SimGame::StepState::Done) {
        if ((int)sim.turns.size() > before) onTurnComplete(sim.turns.back());
        return true;
    }
    return false;
}

// Record one synchronous greedy turn.
// Returns true when a turn was completed (onTurnComplete is invoked).
inline bool SimStepAndRecord(SimGame& sim, SimUiState& ui,
                             std::function<void(const SimGame::Turn&)> onTurnComplete) {
    if (sim.gameOver) return false;
    const int before = (int)sim.turns.size();
    if (!sim.step()) return false;
    if ((int)sim.turns.size() > before) onTurnComplete(sim.turns.back());
    return true;
}