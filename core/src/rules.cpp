#include "sudoku/rules.hpp"

namespace sudoku {

bool canPlace(const Board& board, int row, int col, int value) {
    // One loop covers both the row and the column: at step i we look at the
    // i-th cell of this row and the i-th cell of this column.
    for (int i = 0; i < kGridSize; ++i) {
        if (board.valueAt(row, i) == value) return false;
        if (board.valueAt(i, col) == value) return false;
    }

    // Top-left corner of the 3x3 box containing (row, col). Subtracting the
    // remainder rounds down to the nearest multiple of 3: 0,1,2 -> 0;
    // 3,4,5 -> 3; 6,7,8 -> 6.
    const int boxRow = row - row % 3;
    const int boxCol = col - col % 3;
    for (int r = boxRow; r < boxRow + 3; ++r)
        for (int c = boxCol; c < boxCol + 3; ++c)
            if (board.valueAt(r, c) == value) return false;

    return true;
}

}