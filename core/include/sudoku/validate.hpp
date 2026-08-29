#pragma once

#include <cstdint>
#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/units.hpp"  // UnitKind, CellRef — shared with the hint engine

namespace sudoku {

// One duplicated digit inside one unit.
//
// Deliberately rich rather than a bare bool: the UI has to highlight exactly
// which cells are at fault, and the future photo pipeline uses the same data
// to point at cells the scan probably misread.
struct Conflict {
    UnitKind unit;
    int unitIndex;              // 0..8; boxes are numbered row-major, top-left = 0
    int value;                  // the digit that appears more than once
    std::vector<CellRef> cells; // every cell in this unit holding it (always >= 2)

    bool operator==(const Conflict&) const = default;
};

// Rule check only: finds duplicated digits in rows, columns and boxes.
//
// It does NOT ask whether the puzzle is solvable — an empty board and a board
// missing a needed digit both come back clean. Solvability is countSolutions'
// job (see sudoku/solver.hpp).
//
// Empty cells (value 0) are ignored; only placed digits can conflict.
// Returns an empty vector when the board is legal.
std::vector<Conflict> validate(const Board& board);

}