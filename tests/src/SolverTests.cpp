#include <doctest/doctest.h>

#include <string>

#include "Fixtures.hpp"
#include "sudoku/board.hpp"
#include "sudoku/rules.hpp"
#include "sudoku/solver.hpp"
#include "sudoku/validate.hpp"

using namespace sudoku;

namespace {

Board boardFrom(std::string_view text) {
    auto board = Board::fromString(text);
    REQUIRE(board.has_value());
    return *board;
}

}

TEST_CASE("canPlace rejects a digit already in the row, column or box") {
    std::string s(fixtures::kEmpty);
    s[0 * 9 + 5] = '4';   // R1C6 — same row as R1C1
    s[5 * 9 + 0] = '6';   // R6C1 — same column as R1C1
    s[1 * 9 + 1] = '8';   // R2C2 — same box as R1C1
    Board board = boardFrom(s);

    CHECK_FALSE(canPlace(board, 0, 0, 4));   // blocked by the row
    CHECK_FALSE(canPlace(board, 0, 0, 6));   // blocked by the column
    CHECK_FALSE(canPlace(board, 0, 0, 8));   // blocked by the box
    CHECK(canPlace(board, 0, 0, 1));         // nothing in the way
}

TEST_CASE("canPlace only looks at the cell's own three units") {
    std::string s(fixtures::kEmpty);
    s[8 * 9 + 8] = '5';   // R9C9: shares no row, column or box with R1C1
    Board board = boardFrom(s);
    CHECK(canPlace(board, 0, 0, 5));
}

TEST_CASE("canPlace reports false for a digit already sitting in that cell") {
    // Documented quirk: the scan covers the whole row, including the cell
    // itself. Callers only ever ask about EMPTY cells, so this never bites in
    // practice — but it would surprise anyone using it as "is this move legal".
    std::string s(fixtures::kEmpty);
    s[0] = '3';
    Board board = boardFrom(s);
    CHECK_FALSE(canPlace(board, 0, 0, 3));
}

TEST_CASE("backtracking solves a proper puzzle") {
    auto solved = solveBacktracking(boardFrom(fixtures::kPuzzle));
    REQUIRE(solved.has_value());
    CHECK(solved->toString() == fixtures::kPuzzleSolved);
    CHECK(solved->full());
}

TEST_CASE("solving preserves givens and marks the rest as user-placed") {
    Board start = boardFrom(fixtures::kPuzzle);
    auto solved = solveBacktracking(start);
    REQUIRE(solved.has_value());

    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (start.valueAt(row, col) != 0) {
                // A given must keep both its digit and its Given status.
                CHECK(solved->valueAt(row, col) == start.valueAt(row, col));
                CHECK(solved->at(row, col).kind == CellKind::Given);
            } else {
                CHECK(solved->at(row, col).kind == CellKind::User);
            }
        }
    }
}

TEST_CASE("solving an already-complete board returns it unchanged") {
    Board done = boardFrom(fixtures::kPuzzleSolved);
    auto solved = solveBacktracking(done);
    REQUIRE(solved.has_value());
    CHECK(*solved == done);
}

TEST_CASE("an empty board is solvable") {
    auto solved = solveBacktracking(boardFrom(fixtures::kEmpty));
    REQUIRE(solved.has_value());
    CHECK(solved->full());
    CHECK(validate(*solved).empty());
}

TEST_CASE("a board with duplicate givens is not solvable") {
    // Both entry points run validate() first. Without that guard the search is
    // blind to duplicates among the givens and grinds through a huge space
    // before concluding "no solution" — see the note in solver.hpp.
    std::string s(fixtures::kPuzzle);
    s[3] = '2';   // row 1 already holds a 2 at column 2
    CHECK_FALSE(solveBacktracking(boardFrom(s)).has_value());
    CHECK(countSolutions(boardFrom(s)) == 0);
}

TEST_CASE("a proper puzzle has exactly one solution") {
    CHECK(countSolutions(boardFrom(fixtures::kPuzzle)) == 1);
    CHECK(countSolutions(boardFrom(fixtures::kPuzzleSolved)) == 1);
}

TEST_CASE("removing givens can make a puzzle underdetermined") {
    // These two indices (the 2 and the 9 in row 1) were found by exhaustive
    // search: they are the first pair whose removal admits more than one
    // solution. Most pairs leave the puzzle unique, so the choice is not
    // arbitrary.
    std::string s(fixtures::kPuzzle);
    s[1] = '0';
    s[2] = '0';
    CHECK(countSolutions(boardFrom(s)) == 2);
}

TEST_CASE("a legal but unfinishable board has zero solutions") {
    // Every unit is conflict-free, yet R1C1 can hold nothing: its row supplies
    // 1-6, its column supplies 7 and 8, and its box supplies 9.
    std::string s(fixtures::kEmpty);
    s[3] = '1'; s[4] = '2'; s[5] = '3';
    s[6] = '4'; s[7] = '5'; s[8] = '6';   // row 1
    s[3 * 9] = '7';                        // column 1
    s[4 * 9] = '8';                        // column 1
    s[1 * 9 + 1] = '9';                    // box 1

    Board board = boardFrom(s);
    REQUIRE(validate(board).empty());      // genuinely legal...
    CHECK(countSolutions(board) == 0);     // ...and genuinely hopeless
}

TEST_CASE("countSolutions stops at the cap") {
    Board empty = boardFrom(fixtures::kEmpty);
    CHECK(countSolutions(empty, 1) == 1);
    CHECK(countSolutions(empty, 2) == 2);
    CHECK(countSolutions(empty, 7) == 7);
    CHECK(countSolutions(empty, 0) == 0);
}