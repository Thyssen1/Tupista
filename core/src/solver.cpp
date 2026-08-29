#include "sudoku/solver.hpp"

#include "sudoku/rules.hpp"
#include "sudoku/validate.hpp"

namespace sudoku {

namespace {

// Classic backtracking search.
//
// Board& (a mutable reference) rather than by value: the recursion writes a
// digit, recurses, and — if that path fails — erases it again. Passing by
// value would copy all 81 cells at every level and lose the undo.
//
// Reads as: find the first empty cell; try each legal digit in it; recurse.
// If no digit works, return false and let the CALLER undo its own guess.
bool solveInPlace(Board& board) {
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (board.valueAt(row, col) != 0) continue;

            for (int value = 1; value <= kGridSize; ++value) {
                if (!canPlace(board, row, col, value)) continue;

                board.at(row, col).value = static_cast<std::uint8_t>(value);
                board.at(row, col).kind = CellKind::User;

                if (solveInPlace(board)) return true;

                // Dead end: undo and try the next digit. This single pair of
                // lines is what makes it "backtracking".
                board.at(row, col).value = 0;
                board.at(row, col).kind = CellKind::Empty;
            }

            // No digit fits here, so whatever we assumed earlier was wrong.
            // Returning from inside the loops is deliberate: only the FIRST
            // empty cell is ever considered at this level of recursion.
            return false;
        }
    }

    // Fell through both loops without finding an empty cell => board is full,
    // and every digit on it passed canPlace when it was written. Solved.
    return true;
}

// Same search, but instead of stopping at the first solution it keeps going.
// `remaining` is how many more solutions we still care about; once it hits 0
// the recursion unwinds without exploring the rest of the tree.
int countInPlace(Board& board, int remaining) {
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (board.valueAt(row, col) != 0) continue;

            int found = 0;
            for (int value = 1; value <= kGridSize; ++value) {
                if (!canPlace(board, row, col, value)) continue;

                // Only `value` is touched here, not `kind`: this board is a
                // scratch copy that never escapes the function, and canPlace
                // reads nothing but values.
                board.at(row, col).value = static_cast<std::uint8_t>(value);
                found += countInPlace(board, remaining - found);
                board.at(row, col).value = 0;

                if (found >= remaining) return found;
            }
            return found;
        }
    }

    // No empty cell left: the board in hand IS one complete solution.
    return 1;
}

}

std::optional<Board> solveBacktracking(const Board& board) {
    if (!validate(board).empty()) return std::nullopt;

    Board working = board;  // copy, so the caller's board is left untouched
    if (!solveInPlace(working)) return std::nullopt;
    return working;
}

int countSolutions(const Board& board, int cap) {
    if (cap <= 0) return 0;
    if (!validate(board).empty()) return 0;

    Board working = board;
    return countInPlace(working, cap);
}

}