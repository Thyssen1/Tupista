#include "sudoku/candidates.hpp"

#include "sudoku/rules.hpp"

namespace sudoku {

Candidates Candidates::compute(const Board& board) {
    Candidates out;
    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col) {
            if (board.valueAt(row, col) != 0) continue;
            for (int digit = 1; digit <= kGridSize; ++digit)
                if (canPlace(board, row, col, digit))
                    out.at(row, col).set(digit);
        }
    return out;
}

int onlyDigit(const DigitSet& digits) {
    if (digits.count() != 1) return 0;
    for (int digit = 1; digit <= kGridSize; ++digit)
        if (digits.test(digit)) return digit;
    return 0;
}

}
