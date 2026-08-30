#include <doctest/doctest.h>

#include <string>

#include "Fixtures.hpp"
#include "sudoku/audit.hpp"
#include "sudoku/board.hpp"
#include "sudoku/candidates.hpp"
#include "sudoku/hint.hpp"
#include "sudoku/human_solver.hpp"

using namespace sudoku;

namespace {

Board boardFrom(std::string_view text) {
    auto board = Board::fromString(text);
    REQUIRE(board.has_value());
    return *board;
}

// Pencil in exactly the digits a perfect player would have written.
void markCorrectly(Board& board, int row, int col) {
    const Candidates truth = Candidates::compute(board);
    for (int digit = 1; digit <= kGridSize; ++digit)
        if (truth.has(row, col, digit)) board.setMark(row, col, digit);
}

}

TEST_CASE("marks round-trip through the board") {
    Board board = boardFrom(fixtures::kEmpty);
    CHECK_FALSE(board.hasMark(2, 3, 5));

    board.setMark(2, 3, 5);
    board.setMark(2, 3, 9);
    CHECK(board.hasMark(2, 3, 5));
    CHECK(board.hasMark(2, 3, 9));
    CHECK_FALSE(board.hasMark(2, 3, 4));
    CHECK_FALSE(board.hasMark(3, 2, 5));   // not the transposed cell

    board.clearMark(2, 3, 5);
    CHECK_FALSE(board.hasMark(2, 3, 5));
    CHECK(board.hasMark(2, 3, 9));

    board.clearMarks(2, 3);
    CHECK_FALSE(board.hasMark(2, 3, 9));
}

TEST_CASE("an unmarked board reports nothing") {
    // Silence is correct here: a player who has written no marks has not made
    // a mistake, they simply have not started.
    CHECK(auditPencilMarks(boardFrom(fixtures::kPuzzle)).empty());
}

TEST_CASE("perfect marks report nothing") {
    Board board = boardFrom(fixtures::kPuzzle);
    markCorrectly(board, 0, 0);
    markCorrectly(board, 7, 2);
    CHECK(auditPencilMarks(board).empty());
}

TEST_CASE("a missing mark is reported") {
    // R1C1 can hold 4, 7 or 8. Note only two of them.
    Board board = boardFrom(fixtures::kPuzzle);
    board.setMark(0, 0, 4);
    board.setMark(0, 0, 7);

    const auto report = auditPencilMarks(board);
    REQUIRE(report.size() == 1);
    CHECK(report[0].cell == CellRef{0, 0});
    CHECK(report[0].missing == std::vector<int>{8});
    CHECK(report[0].stale.empty());
}

TEST_CASE("a mark that is no longer possible is reported as stale") {
    Board board = boardFrom(fixtures::kPuzzle);
    markCorrectly(board, 0, 0);
    board.setMark(0, 0, 2);   // row 1 already holds a 2

    const auto report = auditPencilMarks(board);
    REQUIRE(report.size() == 1);
    CHECK(report[0].cell == CellRef{0, 0});
    CHECK(report[0].stale == std::vector<int>{2});
    CHECK(report[0].missing.empty());
}

TEST_CASE("marks go stale when a digit is placed nearby") {
    // The real-world case: the player pencils a cell correctly, then fills in
    // another cell that kills one of those candidates and forgets to erase.
    Board board = boardFrom(fixtures::kPuzzle);
    markCorrectly(board, 0, 0);          // 4, 7, 8
    REQUIRE(auditPencilMarks(board).empty());

    board.at(0, 3).value = 4;            // a 4 in row 1 kills the 4 at R1C1
    board.at(0, 3).kind = CellKind::User;

    const auto report = auditPencilMarks(board);
    REQUIRE(report.size() == 1);
    CHECK(report[0].cell == CellRef{0, 0});
    CHECK(report[0].stale == std::vector<int>{4});
}

TEST_CASE("marks left on a solved cell are all stale") {
    Board board = boardFrom(fixtures::kPuzzle);
    markCorrectly(board, 0, 0);
    board.at(0, 0).value = 8;            // the player commits to a digit...
    board.at(0, 0).kind = CellKind::User;
                                          // ...but leaves the notes behind
    const auto report = auditPencilMarks(board);
    REQUIRE(report.size() == 1);
    CHECK(report[0].stale == std::vector<int>{4, 7, 8});
    CHECK(report[0].missing.empty());
}

TEST_CASE("the audit never changes the board") {
    Board board = boardFrom(fixtures::kPuzzle);
    markCorrectly(board, 0, 0);
    board.setMark(0, 0, 2);
    const Board before = board;

    auditPencilMarks(board);
    CHECK(board == before);
}

TEST_CASE("marks do not affect solving") {
    // The load-bearing guarantee: nonsense marks must not change one thing
    // about the hints or the solve.
    Board clean = boardFrom(fixtures::kPuzzle);
    Board scribbled = clean;
    for (int digit = 1; digit <= kGridSize; ++digit) {
        scribbled.setMark(0, 0, digit);
        scribbled.setMark(4, 4, digit);
    }

    CHECK(Candidates::compute(clean).cells == Candidates::compute(scribbled).cells);
    CHECK(solveHuman(clean).board.toString() == solveHuman(scribbled).board.toString());
    CHECK(nextHint(clean).placement.value == nextHint(scribbled).placement.value);
}
