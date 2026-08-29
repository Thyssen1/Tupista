#include <doctest/doctest.h>

#include <string>

#include "Fixtures.hpp"
#include "sudoku/board.hpp"
#include "sudoku/candidates.hpp"
#include "sudoku/human_solver.hpp"
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

TEST_CASE("candidates come from placed digits only") {
    Board board = boardFrom(fixtures::kPuzzle);
    Candidates cand = Candidates::compute(board);

    // A filled cell carries no candidates at all.
    CHECK(cand.count(0, 1) == 0);

    // R1C1 is empty. Its row holds 2,9,6,1; its column holds 3; its box holds
    // 2,9,5,1. So only 4, 7 and 8 remain.
    CHECK(cand.count(0, 0) == 3);
    CHECK(cand.has(0, 0, 4));
    CHECK(cand.has(0, 0, 7));
    CHECK(cand.has(0, 0, 8));
    CHECK_FALSE(cand.has(0, 0, 2));
}

TEST_CASE("onlyDigit identifies a naked single") {
    DigitSet one;
    one.set(6);
    CHECK(onlyDigit(one) == 6);

    DigitSet two;
    two.set(3);
    two.set(6);
    CHECK(onlyDigit(two) == 0);

    CHECK(onlyDigit(DigitSet{}) == 0);
}

TEST_CASE("regression: the ladder reproduces the reference engine exactly") {
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const HumanSolveResult result = solveHuman(boardFrom(fixture.puzzle));

        CHECK(result.solved == fixture.solves);
        CHECK(result.maxTier == fixture.tier);
        CHECK(result.placements.size() == static_cast<std::size_t>(fixture.placements));
        CHECK(result.board.toString() == fixture.finalGrid);
    }
}

TEST_CASE("regression: the opening sequence matches the reference step for step") {
    // The count-and-final-grid check above would still pass if our ladder took
    // a different route to the same answer. These are the reference engine's
    // literal first placements for fixture 1, so this pins the ORDER too.
    const HumanSolveResult result = solveHuman(boardFrom(fixtures::kPuzzle));
    REQUIRE(result.placements.size() >= 6);

    struct Expected { int row, col, value; Technique technique; };
    const Expected opening[] = {
        {7, 2, 5, Technique::NakedSingle},
        {4, 2, 2, Technique::NakedSingle},
        {7, 4, 7, Technique::NakedSingle},
        {3, 6, 2, Technique::HiddenSingle},
        {8, 0, 2, Technique::HiddenSingle},
        {3, 0, 5, Technique::NakedSingle},
    };

    for (int i = 0; i < 6; ++i) {
        CAPTURE(i);
        CHECK(result.placements[i].row == opening[i].row);
        CHECK(result.placements[i].col == opening[i].col);
        CHECK(result.placements[i].value == opening[i].value);
        CHECK(result.placements[i].technique == opening[i].technique);
    }
}

TEST_CASE("regression: solved fixtures agree with the backtracking ground truth") {
    for (const auto& fixture : fixtures::kRegressions) {
        if (!fixture.solves) continue;
        CAPTURE(fixture.name);
        const auto truth = solveBacktracking(boardFrom(fixture.puzzle));
        REQUIRE(truth.has_value());
        CHECK(truth->toString() == fixture.finalGrid);
    }
}

TEST_CASE("regression: every fixture is a proper puzzle") {
    // Including AI Escargot. "Stuck" must mean "beyond our technique ladder",
    // never "the puzzle was broken".
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        CHECK(countSolutions(boardFrom(fixture.puzzle)) == 1);
    }
}

TEST_CASE("AI Escargot stalls after exactly one hidden single") {
    const auto& fixture = fixtures::kRegressions[4];
    const HumanSolveResult result = solveHuman(boardFrom(fixture.puzzle));

    REQUIRE(result.placements.size() == 1);
    const Placement& only = result.placements[0];
    CHECK(only.row == 7);           // R8C3 = 1, hidden single in a column
    CHECK(only.col == 2);
    CHECK(only.value == 1);
    CHECK(only.technique == Technique::HiddenSingle);
    CHECK(only.unit == UnitKind::Col);
}

TEST_CASE("every placement the ladder makes is legal") {
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const HumanSolveResult result = solveHuman(boardFrom(fixture.puzzle));
        CHECK(validate(result.board).empty());
    }
}

TEST_CASE("rateDifficulty reports the hardest tier used") {
    Rating pairs = rateDifficulty(boardFrom(fixtures::kRegressions[0].puzzle));
    CHECK_FALSE(pairs.beyondEngine);
    CHECK(pairs.tier == 3);
    CHECK(pairs.tierName == "naked/hidden pairs");

    Rating chain = rateDifficulty(boardFrom(fixtures::kRegressions[3].puzzle));
    CHECK_FALSE(chain.beyondEngine);
    CHECK(chain.tier == 7);
    CHECK(chain.tierName == "XY-Chain");

    Rating stuck = rateDifficulty(boardFrom(fixtures::kRegressions[4].puzzle));
    CHECK(stuck.beyondEngine);
}

TEST_CASE("an already-solved board needs no technique at all") {
    const HumanSolveResult result = solveHuman(boardFrom(fixtures::kPuzzleSolved));
    CHECK(result.solved);
    CHECK(result.placements.empty());
    CHECK(result.maxTier == 0);
    CHECK(rateDifficulty(boardFrom(fixtures::kPuzzleSolved)).tierName == "none");
}
