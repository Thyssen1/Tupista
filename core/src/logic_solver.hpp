// INTERNAL header — lives in src/, never installed, not part of the public API.
//
// One engine serves two masters:
//   * solveHuman  runs the whole ladder to the end and reports difficulty.
//   * nextHint    needs to apply ONE deduction at a time so it can show the
//                 user the chain of eliminations that makes a placement true.
//
// Rather than writing every technique twice, each technique honours `mode`:
//   ApplyMode::All -> behave exactly like the original verified engine
//   ApplyMode::One -> stop after the first pattern that eliminates anything
//
// Everything a technique does is also appended to `findings`, which is what
// the hint payload is built from.
#pragma once

#include <optional>
#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/candidates.hpp"
#include "sudoku/human_solver.hpp"
#include "sudoku/technique.hpp"

namespace sudoku::detail {

enum class ApplyMode { All, One };

class LogicSolver {
public:
    explicit LogicSolver(const Board& start);

    Board board;
    Candidates cand;
    ApplyMode mode = ApplyMode::All;

    std::vector<Finding> findings;
    std::vector<Placement> placements;
    int maxTier = 0;

    // Recompute every candidate from the placed digits.
    //
    // Called after each placement. Note what this implies: eliminations made
    // by the techniques are TRANSIENT — the next placement wipes them and
    // rebuilds candidates from scratch. That is the original engine's
    // behaviour, it is sound (it only ever forgets deductions, never invents
    // them), and it is why the hint tracer has to re-derive its chain from the
    // user's current board rather than from engine state.
    void recompute();

    bool solved() const;

    // Is a single available right now, given the candidates as they stand?
    // Does not touch the board — the hint tracer asks this after every single
    // elimination to find out whether the chain is long enough yet.
    std::optional<Placement> findSingle() const;

    // Each returns true if it changed something.
    bool placeSingle();
    bool pointingClaiming();
    bool nakedSets(int size);
    bool hiddenSets(int size);
    bool xWing();
    bool xyWing();
    bool xyChains();

    // One sweep down the elimination part of the ladder (no placements),
    // stopping at the first technique that bites.
    bool eliminateOnce();

    // The full ladder until solved or stuck.
    bool run();

private:
    void bumpTier(int tier);
    bool stopAfterFinding() const { return mode == ApplyMode::One; }

    // Records a finding and bumps the tier. Returns true when the caller
    // should stop early (ApplyMode::One).
    bool record(Finding finding);
};

}
