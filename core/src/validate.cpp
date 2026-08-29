#include "sudoku/validate.hpp"

#include <utility>  // std::move

#include "sudoku/units.hpp"  // unitCell — was a private copy here until the
                             // hint engine needed the same geometry

namespace sudoku {

std::vector<Conflict> validate(const Board& board) {
    std::vector<Conflict> conflicts;

    // Range-for over a braced list: iterates the three enum values in turn.
    for (UnitKind unit : {UnitKind::Row, UnitKind::Col, UnitKind::Box}) {
        for (int unitIndex = 0; unitIndex < kGridSize; ++unitIndex) {
            // Starting at 1, not 0: an empty cell holds 0, and "many cells are
            // empty" is not a conflict.
            for (int value = 1; value <= kGridSize; ++value) {
                std::vector<CellRef> holders;
                for (int slot = 0; slot < kGridSize; ++slot) {
                    CellRef ref = unitCell(unit, unitIndex, slot);
                    if (board.valueAt(ref.row, ref.col) == value)
                        holders.push_back(ref);
                }

                // Two or more cells in one unit holding the same digit = illegal.
                if (holders.size() >= 2) {
                    // std::move hands the vector's internal buffer to the
                    // Conflict instead of copying it; holders is left empty
                    // and is destroyed at the end of this iteration anyway.
                    conflicts.push_back({unit, unitIndex, value, std::move(holders)});
                }
            }
        }
    }

    return conflicts;
}

}