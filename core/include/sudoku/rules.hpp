#pragma once

#include "sudoku/board.hpp"

namespace sudoku {

// Would placing `value` at (row, col) be legal given what is on the board now?
//
// Asks only about the three units the cell belongs to: no digit equal to
// `value` may already sit in its row, its column, or its 3x3 box.
//
// Two things it deliberately does NOT do:
//  * It does not check whether (row, col) is already occupied. Callers only
//    ask about empty cells. (Consequence: if the cell itself already holds
//    `value`, this returns false, because the scan finds that very digit.)
//  * It does not look ahead. "Legal right now" is not "leads to a solution" —
//    that is what the backtracking search in solver.hpp is for.
//
// This is the single rule primitive of the whole engine: the solver uses it to
// drive its search, and the hint engine will use it to compute candidates.
bool canPlace(const Board& board, int row, int col, int value);

}