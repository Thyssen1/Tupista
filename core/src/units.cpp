#include "sudoku/units.hpp"

namespace sudoku {

CellRef unitCell(UnitKind unit, int unitIndex, int slot) {
    switch (unit) {
        case UnitKind::Row: return {unitIndex, slot};
        case UnitKind::Col: return {slot, unitIndex};
        case UnitKind::Box: return {(unitIndex / 3) * 3 + slot / 3,
                                    (unitIndex % 3) * 3 + slot % 3};
    }
    return {};
}

UnitKind unitKindAt(int flatUnitIndex) {
    if (flatUnitIndex < kGridSize) return UnitKind::Row;
    if (flatUnitIndex < 2 * kGridSize) return UnitKind::Col;
    return UnitKind::Box;
}

int unitIndexAt(int flatUnitIndex) { return flatUnitIndex % kGridSize; }

CellRef unitCellAt(int flatUnitIndex, int slot) {
    return unitCell(unitKindAt(flatUnitIndex), unitIndexAt(flatUnitIndex), slot);
}

int boxIndexOf(int row, int col) { return (row / 3) * 3 + col / 3; }

bool sees(CellRef a, CellRef b) {
    if (a == b) return false;
    return a.row == b.row
        || a.col == b.col
        || boxIndexOf(a.row, a.col) == boxIndexOf(b.row, b.col);
}

}
