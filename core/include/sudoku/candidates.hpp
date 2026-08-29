#pragma once

#include <array>
#include <bitset>

#include "sudoku/board.hpp"

namespace sudoku {

// A set of digits, stored as bits. Bit n means "digit n is in the set"; bit 0
// is deliberately wasted so the bit index IS the digit and no +1/-1 juggling
// creeps into the technique code.
//
// std::bitset gives us set/reset/test plus the whole-set operators (& | ~) and
// count(), which is what makes the technique code short: "do these three cells
// between them hold exactly three digits?" is one OR and one count().
using DigitSet = std::bitset<10>;

// Which digits could still legally go in each cell.
//
// THE RULE THAT MAKES THIS A HINT ENGINE, NOT A SOLVER: candidates are always
// derived from the digits actually placed on the board. The user's own pencil
// marks are never consulted for solving — if they were, a mistake in their
// marks would silently corrupt every hint. Marks exist only so a later stage
// can audit them against this.
struct Candidates {
    std::array<DigitSet, kCellCount> cells{};

    DigitSet& at(int row, int col) { return cells[row * kGridSize + col]; }
    const DigitSet& at(int row, int col) const { return cells[row * kGridSize + col]; }

    bool has(int row, int col, int digit) const { return at(row, col).test(digit); }
    int count(int row, int col) const { return static_cast<int>(at(row, col).count()); }
    void remove(int row, int col, int digit) { at(row, col).reset(digit); }

    // Filled cells get an empty set — they need no candidates.
    static Candidates compute(const Board& board);
};

// If the set holds exactly one digit, return it; otherwise 0.
// This is the whole of "naked single" once candidates exist.
int onlyDigit(const DigitSet& digits);

}
