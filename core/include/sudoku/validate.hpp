#pragma once

#include <cstdint>
#include <vector>

#include "sudoku/board.hpp"

namespace sudoku {

// The three kinds of "unit" (a group of 9 cells that must hold the digits 1-9
// exactly once). Every cell belongs to exactly one row, one column, one box.
enum class UnitKind : std::uint8_t { Row, Col, Box };

// A coordinate pair. We could have used std::pair<int,int>, but then every
// use site reads .first/.second and you have to remember which is which.
// Named members make the call sites self-documenting.
struct CellRef {
    int row = 0;
    int col = 0;

    bool operator==(const CellRef&) const = default;
};

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