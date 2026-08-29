#pragma once

#include <string>
#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/human_solver.hpp"
#include "sudoku/technique.hpp"
#include "sudoku/validate.hpp"

namespace sudoku {

enum class HintStatus : std::uint8_t {
    Ok,            // a next step is available
    Solved,        // nothing left to do
    BoardInvalid,  // duplicate digits — the conflicts field says where
    NotUnique,     // no solution, or more than one: hints would be guesswork
    Stuck,         // legal, unique, but beyond our technique ladder
};

// One next step for the player.
//
// THE POINT OF THIS TYPE: `eliminations` is not decoration. The engine finds a
// placement like "R9C5 = 3, naked single" only after background eliminations
// have quietly pruned candidates. Telling a player "naked single at R9C5" when
// their own pencil marks still show three candidates there is gaslighting —
// so a hint carries the ordered chain of eliminations that makes the final
// step true starting from what the player can actually see.
//
// An empty chain is the good case: it means the step is visible with no
// preparation at all (a tier-1 hint).
struct Hint {
    HintStatus status = HintStatus::Stuck;
    std::vector<Finding> eliminations;  // in the order they must be applied
    Placement placement;                // the step the chain unlocks
    bool hasPlacement = false;
    std::vector<Conflict> conflicts;    // populated when status == BoardInvalid

    // The hardest technique the chain needed, 0 when nothing was required.
    int tier() const;
};

// Work out the single most important next step.
//
// Always validates first: an illegal or non-unique board makes every hint
// meaningless, so those cases report the problem instead of inventing advice.
// This doubles as scan-error detection once puzzles arrive from a photo.
//
// The engine never reads the player's pencil marks — candidates are always
// recomputed from placed digits.
Hint nextHint(const Board& board);

// How much to give away. The UI escalates on demand and must never jump
// straight to Full.
enum class HintLevel : std::uint8_t {
    Vague,      // "look at column 6 for digits 1 and 2"
    Mechanism,  // "1 and 2 fit only two cells in column 6: hidden pair"
    Full,       // exact cells, eliminations and the resulting placement
};

std::string describe(const Hint& hint, HintLevel level);

// "R4C7", 1-based, the way players and puzzle books write it.
std::string cellName(CellRef cell);

// "row 4", "column 7", "box 5" — also 1-based.
std::string unitName(UnitKind unit, int unitIndex);

}
