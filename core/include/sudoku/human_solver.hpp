#pragma once

#include <string>
#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/technique.hpp"

namespace sudoku {

// One digit written into one cell, and the reason it was justified.
struct Placement {
    int row = 0;
    int col = 0;
    int value = 0;
    Technique technique{};   // NakedSingle or HiddenSingle
    UnitKind unit{};         // for a hidden single: the unit it was hidden in
    int unitIndex = -1;      // -1 for a naked single
};

struct HumanSolveResult {
    bool solved = false;
    int maxTier = 0;                    // hardest tier the ladder had to use
    std::vector<Placement> placements;  // in the order they were made
    Board board;                        // solved, or the partial grid where it stalled
};

// Solve the way a person would: repeatedly place whatever single is available,
// and when none is, apply the cheapest elimination technique that bites, then
// look for singles again.
//
// The ladder is ordered by human difficulty: singles, pointing/claiming, naked
// pairs, hidden pairs, naked triples, hidden triples, X-Wing, XY-Wing,
// XY-Chain. When nothing on the ladder applies, the result comes back with
// solved == false and the grid as far as it got. That is a correct answer, not
// a bug: some published puzzles genuinely need guessing.
HumanSolveResult solveHuman(const Board& board);

struct Rating {
    bool beyondEngine = false;  // the ladder stalled before finishing
    int tier = 0;               // hardest tier used (still meaningful when stuck)
    std::string tierName;
};

// Difficulty = the hardest technique the solve actually needed.
//
// Worth knowing: the ladder is greedy. When a technique fires it applies every
// elimination it can see, which may be more than strictly necessary, so the
// rating is "hardest tier this engine used" rather than a proof of the
// theoretical minimum. Consistent and useful for ranking puzzles; not a
// mathematical claim.
Rating rateDifficulty(const Board& board);

}
