#include <doctest/doctest.h>
#include <string>
#include "sudoku/board.hpp"

// Pulls sudoku:: into scope for this file only. Fine in a .cpp; never do
// this in a header (it would leak into every includer).
using namespace sudoku;

// Anonymous namespace = internal linkage: these names exist only inside this
// .cpp, so they can never collide with identically-named constants in other
// test files at link time (the C++ equivalent of "file-private").
namespace {
const std::string kPuzzle =
    "029000610050017000001090005060940081000786304000120006008002000300801062000409000";
const std::string kSolvedGrid =
    "829354617654217839731698425567943281192786354483125796978562143345871962216439578";
}

// TEST_CASE registers the block with doctest's runner at static-init time —
// no test class, no attribute, no manual registration list.
//
// REQUIRE vs CHECK: REQUIRE aborts THIS test case on failure (the following
// lines would be meaningless or crash — e.g. dereferencing an empty
// optional); CHECK records the failure and keeps going so one run reports
// every broken assertion, not just the first.
TEST_CASE("fromString/toString round-trip") {
    // "auto" deduces std::optional<Board> from the return type.
    auto board = Board::fromString(kPuzzle);
    REQUIRE(board.has_value());
    // optional supports -> and * to reach the contained value, pointer-style.
    CHECK(board->toString() == kPuzzle);
}

TEST_CASE("digits parse as givens, zeros and dots as empty") {
    std::string s(81, '.');   // 81 dots = an all-empty board using the '.' spelling
    s[0] = '5';
    s[80] = '0';
    auto board = Board::fromString(s);
    REQUIRE(board.has_value());
    CHECK(board->at(0, 0).value == 5);
    CHECK(board->at(0, 0).kind == CellKind::Given);
    CHECK(board->at(8, 8).value == 0);
    CHECK(board->at(8, 8).kind == CellKind::Empty);
    CHECK(board->at(4, 4).kind == CellKind::Empty);
}

TEST_CASE("fromString rejects bad input") {
    CHECK_FALSE(Board::fromString("123").has_value());                 // too short
    CHECK_FALSE(Board::fromString(std::string(82, '0')).has_value());  // too long
    std::string bad(81, '0');
    bad[10] = 'x';                                                     // illegal character
    CHECK_FALSE(Board::fromString(bad).has_value());
}

TEST_CASE("full() detects a completed grid") {
    auto empty = Board::fromString(std::string(81, '0'));
    REQUIRE(empty.has_value());
    CHECK_FALSE(empty->full());

    auto solved = Board::fromString(kSolvedGrid);
    REQUIRE(solved.has_value());
    CHECK(solved->full());
}

TEST_CASE("at() maps row/col to the right cell") {
    std::string s(81, '0');
    s[1 * 9 + 7] = '3';       // write via raw index math on the string...
    auto board = Board::fromString(s);
    REQUIRE(board.has_value());
    CHECK(board->valueAt(1, 7) == 3);  // ...read back via at(); catches a
    CHECK(board->valueAt(7, 1) == 0);  // transposed row/col mapping
}