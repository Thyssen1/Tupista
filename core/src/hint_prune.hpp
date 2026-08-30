// INTERNAL header — lives in src/, not part of the public API.
//
// The tracer produces an honest chain but a useless one: it applies whatever
// the ladder finds first, so most of the log has nothing to do with the
// placement it eventually unlocks. On the book puzzle it logged nine
// eliminations when two of them do the work.
//
// This is the pruner. It answers "which of these steps did the answer actually
// depend on?" and throws the rest away.
#pragma once

#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/human_solver.hpp"
#include "sudoku/technique.hpp"

namespace sudoku::detail {

// The candidates a pattern needs to be ABSENT in order to hold.
//
// A pattern is never just "these cells" — it is "these cells given what is
// still possible around them". A naked pair on {3,5} stops being a naked pair
// the moment one of its cells could also be 7. So each technique has a set of
// (cell, digit) pairs whose absence it leans on, and those absences are what
// tie one deduction to the ones before it.
std::vector<Elimination> requiredAbsences(const Finding& finding, const Board& board);

// Keep only the findings the placement genuinely rests on.
//
// Works backwards from the target: which eliminations does the placement need?
// Which findings performed those? What did THOSE findings depend on? Repeat
// until closed. Anything never reached was noise and is dropped.
//
// Order is preserved, and a kept finding's dependencies are always kept too —
// so the result is still a chain the player can follow start to finish.
std::vector<Finding> pruneChain(const Board& board,
                                const std::vector<Finding>& chain,
                                const Placement& target);

}
