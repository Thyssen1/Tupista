#pragma once

#include <optional>

#include "sudoku/board.hpp"

namespace sudoku {

// Brute-force solve by backtracking. This is the engine's GROUND TRUTH: it
// ignores human technique entirely and just searches, so it can answer "what
// is the real solution" for any puzzle, including ones the hint ladder cannot
// crack. Later stages cross-check the human solver against this.
//
// Returns the completed board (cells it filled are marked CellKind::User, so
// givens stay distinguishable), or nullopt if the board has no solution.
//
// A board that already has conflicts is reported as unsolvable — see the note
// on countSolutions below for why that guard is not optional.
std::optional<Board> solveBacktracking(const Board& board);

// How many different solutions does this board have, counting no further than
// `cap`? Returns min(actual number of solutions, cap).
//
// With the default cap of 2 the answer reads:
//   0 = unsolvable (or illegal)
//   1 = exactly one solution: a proper puzzle
//   2 = two or more: underdetermined, so hints would be guesswork
//
// Capping matters because an underdetermined grid can have billions of
// solutions; we only ever need to know "is it more than one".
//
// WHY BOTH FUNCTIONS VALIDATE FIRST: canPlace only guards digits we are about
// to place; it never re-examines digits that were already on the board, so the
// search is blind to a duplicate sitting among the GIVENS.
//
// As it turns out such a board is uncompletable anyway (every unit the search
// fills ends up holding all nine digits, which forces a counting contradiction
// against the duplicated unit), so the search does eventually answer "no
// solution" on its own. The problem is how long that takes: proving it by
// exhaustion on a sparse board runs for minutes and does not visibly stop.
// Measured on a board with two 5s in one box, the unguarded search had not
// finished after 90 seconds; with the guard the answer is immediate.
//
// So this is a termination guard first and a correctness belt second — and it
// matters most in exactly the case a user hits by accident: a typo, or later a
// misread digit from the photo scanner, freezing the UI.
int countSolutions(const Board& board, int cap = 2);

}