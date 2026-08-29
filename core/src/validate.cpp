#include "sudoku/validate.hpp"

#include <utility>  // std::move

namespace sudoku {

// Anonymous namespace = internal linkage: unitCell exists only inside this
// .cpp. It is an implementation detail, so it stays out of the header and out
// of the linker's global name pool.
namespace {

// Maps "slot number within a unit" to a board coordinate, so the three unit
// kinds can share one loop below instead of three near-identical ones.
//
//   Row 3, slot 0..8  -> (3, 0) .. (3, 8)
//   Col 3, slot 0..8  -> (0, 3) .. (8, 3)
//   Box 3, slot 0..8  -> the 3x3 block, read left-to-right, top-to-bottom
//
// Box arithmetic: boxes are numbered row-major, so box b starts at row
// (b / 3) * 3 and column (b % 3) * 3. Within the box, slot s sits at row
// offset s / 3 and column offset s % 3.
CellRef unitCell(UnitKind unit, int unitIndex, int slot) {
    switch (unit) {
        case UnitKind::Row: return {unitIndex, slot};
        case UnitKind::Col: return {slot, unitIndex};
        case UnitKind::Box: return {(unitIndex / 3) * 3 + slot / 3,
                                    (unitIndex % 3) * 3 + slot % 3};
    }
    return {};  // unreachable, but GCC warns about "control reaches end" without it
}

}

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