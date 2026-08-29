#pragma once

#include <cstdint>

#include "sudoku/board.hpp"

namespace sudoku {

// A "unit" is a group of 9 cells that must hold the digits 1-9 exactly once.
// Every cell belongs to exactly one row, one column and one box.
enum class UnitKind : std::uint8_t { Row, Col, Box };

// There are 27 units: 9 rows, then 9 columns, then 9 boxes. Several algorithms
// want to sweep all of them in that fixed order, so the count is named here.
inline constexpr int kUnitCount = 27;

struct CellRef {
    int row = 0;
    int col = 0;

    bool operator==(const CellRef&) const = default;
};

// Which cell sits at slot 0..8 of the given unit?
//
//   Row 3, slots 0..8 -> (3,0) .. (3,8)
//   Col 3, slots 0..8 -> (0,3) .. (8,3)
//   Box 3, slots 0..8 -> that 3x3 block, left-to-right then top-to-bottom
//
// Boxes are numbered row-major, so box b starts at row (b/3)*3, column
// (b%3)*3, and slot s within it sits at offset (s/3, s%3).
CellRef unitCell(UnitKind unit, int unitIndex, int slot);

// Same idea, but addressing all 27 units with one index: 0-8 rows,
// 9-17 columns, 18-26 boxes. Handy for "try every unit" loops.
CellRef unitCellAt(int flatUnitIndex, int slot);
UnitKind unitKindAt(int flatUnitIndex);
int unitIndexAt(int flatUnitIndex);

int boxIndexOf(int row, int col);

// Do these two cells share a unit? This is the relationship the chain-based
// techniques are built on: if `a` sees `b`, they can never hold the same digit.
// A cell does NOT see itself.
bool sees(CellRef a, CellRef b);

}
