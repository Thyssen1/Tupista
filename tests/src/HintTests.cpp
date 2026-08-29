#include <doctest/doctest.h>

#include <string>

#include "Fixtures.hpp"
#include "sudoku/board.hpp"
#include "sudoku/candidates.hpp"
#include "sudoku/hint.hpp"
#include "sudoku/rules.hpp"
#include "sudoku/solver.hpp"

using namespace sudoku;

namespace {

Board boardFrom(std::string_view text) {
    auto board = Board::fromString(text);
    REQUIRE(board.has_value());
    return *board;
}

// Replays a hint by hand: apply every logged elimination to freshly computed
// candidates, then confirm the promised placement really is a single.
// This is the test that stops the engine from claiming a step the player
// cannot actually derive.
bool chainJustifiesPlacement(const Board& board, const Hint& hint) {
    Candidates cand = Candidates::compute(board);
    for (const Finding& finding : hint.eliminations)
        for (const Elimination& cut : finding.eliminations)
            cand.remove(cut.row, cut.col, cut.digit);

    const Placement& p = hint.placement;
    if (p.technique == Technique::NakedSingle)
        return onlyDigit(cand.at(p.row, p.col)) == p.value;

    int spots = 0;
    for (int slot = 0; slot < kGridSize; ++slot) {
        const CellRef ref = unitCell(p.unit, p.unitIndex, slot);
        if (board.valueAt(ref.row, ref.col) == 0 && cand.has(ref.row, ref.col, p.value))
            ++spots;
    }
    return spots == 1;
}

}

TEST_CASE("a solved board has nothing to hint") {
    const Hint hint = nextHint(boardFrom(fixtures::kPuzzleSolved));
    CHECK(hint.status == HintStatus::Solved);
    CHECK(describe(hint, HintLevel::Full) == "The puzzle is finished.");
}

TEST_CASE("an illegal board reports the conflict instead of a hint") {
    std::string s(fixtures::kPuzzle);
    s[3] = '2';   // second 2 in row 1
    const Hint hint = nextHint(boardFrom(s));

    CHECK(hint.status == HintStatus::BoardInvalid);
    CHECK_FALSE(hint.conflicts.empty());
    CHECK_FALSE(hint.hasPlacement);

    // Even the vague level must not pretend there is a move to make.
    CHECK(describe(hint, HintLevel::Vague).find("breaks the rules") != std::string::npos);
    CHECK(describe(hint, HintLevel::Full).find("appears more than once") != std::string::npos);
}

TEST_CASE("a board with several solutions refuses to hint") {
    std::string s(fixtures::kPuzzle);
    s[1] = '0';
    s[2] = '0';
    const Hint hint = nextHint(boardFrom(s));
    CHECK(hint.status == HintStatus::NotUnique);
    CHECK_FALSE(hint.hasPlacement);
}

TEST_CASE("a visible single needs no elimination chain") {
    // Blank one cell of a finished grid: that cell now has exactly one
    // candidate, so the hint needs no preparation at all and comes back with
    // an empty chain. This is the tier-1 path.
    std::string s(fixtures::kPuzzleSolved);
    s[40] = '0';
    const Hint hint = nextHint(boardFrom(s));

    REQUIRE(hint.status == HintStatus::Ok);
    CHECK(hint.hasPlacement);
    CHECK(hint.eliminations.empty());
    CHECK(hint.tier() == 1);
    CHECK(hint.placement.value == fixtures::kPuzzleSolved[40] - '0');
}

TEST_CASE("even fixture 1 needs eliminations before its first placement") {
    // Worth pinning, because it is the reason the tracer exists at all. R8C3
    // still has candidates 4, 5 and 7 on the opening grid; the engine only
    // sees "R8C3 = 5, naked single" after pointing/claiming has pruned them.
    // Announcing that placement without the chain would be gaslighting.
    const Board board = boardFrom(fixtures::kPuzzle);
    const Candidates opening = Candidates::compute(board);
    CHECK(opening.count(7, 2) == 3);

    const Hint hint = nextHint(board);
    REQUIRE(hint.status == HintStatus::Ok);
    CHECK_FALSE(hint.eliminations.empty());
    CHECK(chainJustifiesPlacement(board, hint));
}

TEST_CASE("a hint's placement is always the truth") {
    // Cross-check against the real solution: a hint that names the wrong digit
    // would be worse than no hint at all.
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const Board board = boardFrom(fixture.puzzle);
        const Hint hint = nextHint(board);
        if (!hint.hasPlacement) continue;

        const auto truth = solveBacktracking(board);
        REQUIRE(truth.has_value());
        CHECK(truth->valueAt(hint.placement.row, hint.placement.col) == hint.placement.value);
    }
}

TEST_CASE("the enabling chain really does justify the placement") {
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const Board board = boardFrom(fixture.puzzle);
        const Hint hint = nextHint(board);
        if (hint.status != HintStatus::Ok) continue;
        CHECK(chainJustifiesPlacement(board, hint));
    }
}

TEST_CASE("a puzzle with no raw singles produces an elimination chain") {
    // Fixture 3 was chosen precisely because nothing is placeable at the start:
    // the opening move requires eliminations first. This is the case that made
    // the whole tracer necessary.
    const Board board = boardFrom(fixtures::kRegressions[2].puzzle);
    const Hint hint = nextHint(board);

    REQUIRE(hint.status == HintStatus::Ok);
    CHECK(hint.hasPlacement);
    CHECK_FALSE(hint.eliminations.empty());
    CHECK(hint.tier() >= 2);
    CHECK(chainJustifiesPlacement(board, hint));
}

TEST_CASE("escalating disclosure gives away more at each level") {
    const Board board = boardFrom(fixtures::kRegressions[2].puzzle);
    const Hint hint = nextHint(board);
    REQUIRE(hint.status == HintStatus::Ok);

    const std::string vague = describe(hint, HintLevel::Vague);
    const std::string mechanism = describe(hint, HintLevel::Mechanism);
    const std::string full = describe(hint, HintLevel::Full);

    CHECK(vague.length() < mechanism.length());
    CHECK(mechanism.length() < full.length());

    // The vague level points somewhere without naming the technique or the
    // answer; only the full level may reveal the placement.
    CHECK(vague.find("Look at") != std::string::npos);
    CHECK(mechanism.find(std::string(nameOf(hint.eliminations.front().technique))) !=
          std::string::npos);
    CHECK(full.find(cellName({hint.placement.row, hint.placement.col})) != std::string::npos);
    CHECK(vague.find(cellName({hint.placement.row, hint.placement.col})) == std::string::npos);
}

TEST_CASE("hinting repeatedly walks a puzzle all the way to solved") {
    // The strongest end-to-end check: follow nothing but hints and the puzzle
    // must finish, with every step legal.
    Board board = boardFrom(fixtures::kPuzzle);
    int steps = 0;

    while (!board.full()) {
        const Hint hint = nextHint(board);
        REQUIRE(hint.status == HintStatus::Ok);
        REQUIRE(hint.hasPlacement);
        REQUIRE(chainJustifiesPlacement(board, hint));

        board.at(hint.placement.row, hint.placement.col).value =
            static_cast<std::uint8_t>(hint.placement.value);
        board.at(hint.placement.row, hint.placement.col).kind = CellKind::User;

        REQUIRE(++steps < 100);
    }

    CHECK(board.toString() == fixtures::kPuzzleSolved);
}

TEST_CASE("a position beyond the ladder reports stuck, not nonsense") {
    // AI Escargot after its one available placement: legal, unique, unsolvable
    // by our techniques.
    Board board = boardFrom(fixtures::kRegressions[4].finalGrid);
    const Hint hint = nextHint(board);
    CHECK(hint.status == HintStatus::Stuck);
    CHECK_FALSE(hint.hasPlacement);
    CHECK(describe(hint, HintLevel::Full).find("beyond the techniques") != std::string::npos);
}

TEST_CASE("cell and unit names are 1-based") {
    CHECK(cellName({0, 0}) == "R1C1");
    CHECK(cellName({8, 4}) == "R9C5");
    CHECK(unitName(UnitKind::Row, 0) == "row 1");
    CHECK(unitName(UnitKind::Col, 5) == "column 6");
    CHECK(unitName(UnitKind::Box, 8) == "box 9");
}
