#include <doctest/doctest.h>

#include <string>

#include "Fixtures.hpp"
#include "sudoku/board.hpp"
#include "sudoku/validate.hpp"

using namespace sudoku;

namespace {

// Helper: build a board from a fixture string, failing the test if the string
// is malformed. Returns a Board (not an optional), so the tests below stay
// free of has_value() noise.
Board boardFrom(std::string_view text) {
    auto board = Board::fromString(text);
    REQUIRE(board.has_value());
    return *board;
}

// Helper: does the conflict list contain exactly this unit/digit combination?
bool hasConflict(const std::vector<Conflict>& conflicts, UnitKind unit,
                 int unitIndex, int value) {
    for (const Conflict& c : conflicts)
        if (c.unit == unit && c.unitIndex == unitIndex && c.value == value)
            return true;
    return false;
}

}

TEST_CASE("a legal board reports no conflicts") {
    CHECK(validate(boardFrom(fixtures::kPuzzle)).empty());
    CHECK(validate(boardFrom(fixtures::kPuzzleSolved)).empty());
}

TEST_CASE("empty cells never conflict with each other") {
    // 81 zeros: if validate treated 0 as a digit it would report a conflict in
    // every single unit. This is the test that pins "start counting at 1".
    CHECK(validate(boardFrom(fixtures::kEmpty)).empty());
}

TEST_CASE("duplicate in a row is reported with both cells") {
    std::string s(fixtures::kEmpty);
    s[0 * 9 + 2] = '4';   // R1C3
    s[0 * 9 + 7] = '4';   // R1C8 — same row, different column, different box

    auto conflicts = validate(boardFrom(s));
    REQUIRE(conflicts.size() == 1);

    const Conflict& c = conflicts[0];
    CHECK(c.unit == UnitKind::Row);
    CHECK(c.unitIndex == 0);
    CHECK(c.value == 4);
    REQUIRE(c.cells.size() == 2);
    CHECK(c.cells[0] == CellRef{0, 2});
    CHECK(c.cells[1] == CellRef{0, 7});
}

TEST_CASE("duplicate in a column is reported") {
    std::string s(fixtures::kEmpty);
    s[1 * 9 + 4] = '7';   // R2C5
    s[6 * 9 + 4] = '7';   // R7C5 — same column, far enough apart to share no box

    auto conflicts = validate(boardFrom(s));
    REQUIRE(conflicts.size() == 1);
    CHECK(conflicts[0].unit == UnitKind::Col);
    CHECK(conflicts[0].unitIndex == 4);
    CHECK(conflicts[0].value == 7);
}

TEST_CASE("duplicate inside a box is caught even across rows and columns") {
    std::string s(fixtures::kEmpty);
    s[3 * 9 + 3] = '5';   // R4C4
    s[4 * 9 + 4] = '5';   // R5C5 — different row AND column, but the same box

    auto conflicts = validate(boardFrom(s));
    REQUIRE(conflicts.size() == 1);
    CHECK(conflicts[0].unit == UnitKind::Box);
    CHECK(conflicts[0].unitIndex == 4);  // middle box, numbered row-major
    CHECK(conflicts[0].value == 5);
}

TEST_CASE("one duplicate can break two units at once") {
    // Adjacent cells share both a row and a box, so this must produce TWO
    // conflicts. That is intentional: the UI highlights every reason a digit
    // is wrong, not just the first one found.
    std::string s(fixtures::kEmpty);
    s[0] = '3';   // R1C1
    s[1] = '3';   // R1C2

    auto conflicts = validate(boardFrom(s));
    CHECK(conflicts.size() == 2);
    CHECK(hasConflict(conflicts, UnitKind::Row, 0, 3));
    CHECK(hasConflict(conflicts, UnitKind::Box, 0, 3));
    CHECK_FALSE(hasConflict(conflicts, UnitKind::Col, 0, 3));
}

TEST_CASE("validate says nothing about solvability") {
    // A single digit on an otherwise empty board is legal but nowhere near
    // solved. validate() only answers "are the rules broken right now"; the
    // "can this be finished" question belongs to countSolutions.
    std::string s(fixtures::kEmpty);
    s[40] = '6';
    CHECK(validate(boardFrom(s)).empty());
}