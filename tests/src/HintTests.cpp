#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

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

// Is every step of the chain derivable at the moment it is used?
//
// Written independently of the pruner rather than calling its own helper: this
// re-derives each technique's precondition from first principles, so a pruner
// that drops an enabling step gets caught here instead of agreeing with itself.
//
// Two families of precondition:
//   cell-based  (naked sets, XY-Wing, XY-Chain) — the pattern cells must hold
//               exactly the candidates the finding was recorded with
//   unit-based  (pointing, claiming, hidden sets, X-Wing) — the digits must
//               have no home in the unit outside the pattern
bool isCellBased(Technique technique) {
    switch (technique) {
        case Technique::NakedPair:
        case Technique::NakedTriple:
        case Technique::XYWing:
        case Technique::XYChain: return true;
        default: return false;
    }
}

bool chainIsSelfContained(const Board& board, const std::vector<Finding>& chain) {
    Candidates state = Candidates::compute(board);

    for (const Finding& finding : chain) {
        if (isCellBased(finding.technique)) {
            if (finding.patternDigits.size() != finding.pattern.size()) return false;
            for (std::size_t i = 0; i < finding.pattern.size(); ++i) {
                const CellRef cell = finding.pattern[i];
                if (state.at(cell.row, cell.col) != finding.patternDigits[i]) return false;
            }
        } else {
            // Which unit(s) does the claim cover? Normally the one recorded on
            // the finding; an X-Wing covers both of its base lines.
            std::vector<int> lines;
            if (finding.technique == Technique::XWing) {
                for (const CellRef& cell : finding.pattern) {
                    const int line = finding.unit == UnitKind::Row ? cell.row : cell.col;
                    if (std::find(lines.begin(), lines.end(), line) == lines.end())
                        lines.push_back(line);
                }
            } else {
                if (finding.unitIndex < 0) return false;
                lines.push_back(finding.unitIndex);
            }

            for (int line : lines) {
                for (int slot = 0; slot < kGridSize; ++slot) {
                    const CellRef cell = unitCell(finding.unit, line, slot);
                    if (board.valueAt(cell.row, cell.col) != 0) continue;
                    if (std::find(finding.pattern.begin(), finding.pattern.end(), cell) !=
                        finding.pattern.end())
                        continue;
                    for (int digit : finding.digits)
                        if (state.has(cell.row, cell.col, digit)) return false;
                }
            }
        }

        for (const Elimination& cut : finding.eliminations)
            state.remove(cut.row, cut.col, cut.digit);
    }

    return true;
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
    // Technique names are stored lower case but capitalised at the start of a
    // sentence, so compare case-insensitively.
    std::string lowered = mechanism;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    CHECK(lowered.find(std::string(nameOf(hint.eliminations.front().technique))) !=
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

TEST_CASE("every hint chain is self-contained") {
    // The property that makes a hint honest: each step must be derivable from
    // the player's own board plus the steps shown before it. If pruning ever
    // drops an enabling deduction, this fails.
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const Board board = boardFrom(fixture.puzzle);
        const Hint hint = nextHint(board);
        if (hint.status != HintStatus::Ok) continue;
        CHECK(chainIsSelfContained(board, hint.eliminations));
    }
}

TEST_CASE("chains stay self-contained all the way through a solve") {
    Board board = boardFrom(fixtures::kPuzzle);
    while (!board.full()) {
        const Hint hint = nextHint(board);
        REQUIRE(hint.status == HintStatus::Ok);
        REQUIRE(chainIsSelfContained(board, hint.eliminations));
        REQUIRE(chainJustifiesPlacement(board, hint));

        board.at(hint.placement.row, hint.placement.col).value =
            static_cast<std::uint8_t>(hint.placement.value);
        board.at(hint.placement.row, hint.placement.col).kind = CellKind::User;
    }
    CHECK(board.toString() == fixtures::kPuzzleSolved);
}

TEST_CASE("pruning keeps chains short enough to be worth reading") {
    // Before pruning these were 9, 8 and 6 steps of mostly irrelevant work.
    // The bound is deliberately loose: it guards against the chain creeping
    // back to "apply everything the ladder found", not against small changes.
    const int expected[] = {4, -1, 5, 3, -1};

    for (int i = 0; i < 5; ++i) {
        const auto& fixture = fixtures::kRegressions[i];
        CAPTURE(fixture.name);
        const Hint hint = nextHint(boardFrom(fixture.puzzle));
        if (expected[i] < 0) continue;
        CHECK(static_cast<int>(hint.eliminations.size()) == expected[i]);
        CHECK(hint.eliminations.size() <= 6);
    }
}

TEST_CASE("every kept step contributes something") {
    // No step may be dead weight: each one must remove at least one candidate.
    for (const auto& fixture : fixtures::kRegressions) {
        CAPTURE(fixture.name);
        const Hint hint = nextHint(boardFrom(fixture.puzzle));
        for (const Finding& finding : hint.eliminations)
            CHECK_FALSE(finding.eliminations.empty());
    }
}

TEST_CASE("cell and unit names are 1-based") {
    CHECK(cellName({0, 0}) == "R1C1");
    CHECK(cellName({8, 4}) == "R9C5");
    CHECK(unitName(UnitKind::Row, 0) == "row 1");
    CHECK(unitName(UnitKind::Col, 5) == "column 6");
    CHECK(unitName(UnitKind::Box, 8) == "box 9");
}
