// Tests the C ABI through the header a host would actually use.
//
// These matter more than they look: the engine can be perfectly correct and
// still be unusable from C# if the boundary drops data, mis-sizes a buffer, or
// mixes up an enum. Everything here goes through tupista.h only — no C++ API.
#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

#include "Fixtures.hpp"
#include "tupista.h"

namespace {

std::string asC(std::string_view text) { return std::string(text); }

}

TEST_CASE("capi: the ABI reports a version") {
    CHECK(tupista_abi_version() == 1);
}

TEST_CASE("capi: bad boards are rejected, not crashed on") {
    CHECK(tupista_check_board(asC(fixtures::kPuzzle).c_str()) == TUPISTA_OK);
    CHECK(tupista_check_board("123") == TUPISTA_ERR_BAD_BOARD);
    CHECK(tupista_check_board(nullptr) == TUPISTA_ERR_BAD_BOARD);
    CHECK(tupista_check_board(std::string(81, 'x').c_str()) == TUPISTA_ERR_BAD_BOARD);
    CHECK(tupista_check_board(std::string(82, '0').c_str()) == TUPISTA_ERR_BAD_BOARD);
}

TEST_CASE("capi: validate reports conflicts through flat structs") {
    std::string bad = asC(fixtures::kPuzzle);
    bad[3] = '2';

    const int32_t total = tupista_validate(bad.c_str(), nullptr, 0);
    REQUIRE(total > 0);

    std::vector<TupistaConflict> buffer(static_cast<std::size_t>(total));
    CHECK(tupista_validate(bad.c_str(), buffer.data(), total) == total);
    CHECK(buffer[0].value == 2);
    CHECK(buffer[0].cellCount >= 2);
}

TEST_CASE("capi: a small buffer truncates but still reports the true total") {
    // Two 2s side by side break a row AND a box, so this board yields two
    // conflicts — enough for a buffer of one to be genuinely too small.
    std::string bad = asC(fixtures::kPuzzle);
    bad[0] = '2';

    const int32_t total = tupista_validate(bad.c_str(), nullptr, 0);
    REQUIRE(total >= 2);

    // Deliberately one slot short of what is needed: the host must be able to
    // learn the real size and call again without having overflowed anything.
    std::vector<TupistaConflict> tooSmall(1);
    CHECK(tupista_validate(bad.c_str(), tooSmall.data(), 1) == total);
}

TEST_CASE("capi: solutions can be counted and produced") {
    const std::string puzzle = asC(fixtures::kPuzzle);
    CHECK(tupista_count_solutions(puzzle.c_str(), 2) == 1);

    char solved[82] = {};
    CHECK(tupista_solve(puzzle.c_str(), solved) == 1);
    CHECK(std::string(solved) == asC(fixtures::kPuzzleSolved));
    CHECK(std::strlen(solved) == 81);
}

TEST_CASE("capi: candidates come back as bitmasks") {
    uint16_t masks[81] = {};
    REQUIRE(tupista_candidates(asC(fixtures::kPuzzle).c_str(), masks) == TUPISTA_OK);

    CHECK(masks[1] == 0);              // R1C2 is a given, so no candidates
    CHECK((masks[0] & (1u << 4)) != 0);  // R1C1 can be 4, 7 or 8
    CHECK((masks[0] & (1u << 7)) != 0);
    CHECK((masks[0] & (1u << 8)) != 0);
    CHECK((masks[0] & (1u << 2)) == 0);
    CHECK((masks[0] & 1u) == 0);       // bit 0 is never used
}

TEST_CASE("capi: pencil marks are audited") {
    uint16_t marks[81] = {};
    marks[0] = static_cast<uint16_t>((1u << 4) | (1u << 7));  // missing the 8

    TupistaMarkAudit report[8] = {};
    const int32_t count =
        tupista_audit_marks(asC(fixtures::kPuzzle).c_str(), marks, report, 8);

    REQUIRE(count == 1);
    CHECK(report[0].row == 0);
    CHECK(report[0].col == 0);
    CHECK(report[0].missing == static_cast<uint16_t>(1u << 8));
    CHECK(report[0].stale == 0);
}

TEST_CASE("capi: a full solve is readable through its handle") {
    TupistaSolve* solve = tupista_solve_human(asC(fixtures::kPuzzle).c_str());
    REQUIRE(solve != nullptr);

    CHECK(tupista_solve_is_solved(solve) == 1);
    CHECK(tupista_solve_tier(solve) == 3);
    CHECK(tupista_solve_placement_count(solve) == 49);

    char name[64] = {};
    CHECK(tupista_solve_tier_name(solve, name, 64) == 18);
    CHECK(std::string(name) == "naked/hidden pairs");

    char grid[82] = {};
    CHECK(tupista_solve_grid(solve, grid) == TUPISTA_OK);
    CHECK(std::string(grid) == asC(fixtures::kPuzzleSolved));

    TupistaPlacement first{};
    REQUIRE(tupista_solve_placement(solve, 0, &first) == TUPISTA_OK);
    CHECK(first.row == 7);
    CHECK(first.col == 2);
    CHECK(first.value == 5);
    CHECK(first.technique == TUPISTA_NAKED_SINGLE);

    CHECK(tupista_solve_placement(solve, 999, &first) == TUPISTA_ERR_BAD_INDEX);
    tupista_solve_free(solve);
}

TEST_CASE("capi: a truncated string still reports the length it needed") {
    TupistaSolve* solve = tupista_solve_human(asC(fixtures::kPuzzle).c_str());
    REQUIRE(solve != nullptr);

    char tiny[5] = {};
    CHECK(tupista_solve_tier_name(solve, tiny, 5) == 18);  // the full length
    CHECK(std::string(tiny) == "nake");                    // 4 chars plus NUL
    CHECK(tiny[4] == '\0');

    tupista_solve_free(solve);
}

TEST_CASE("capi: a hint arrives with its chain intact") {
    TupistaHint* hint = tupista_hint(asC(fixtures::kPuzzle).c_str());
    REQUIRE(hint != nullptr);

    CHECK(tupista_hint_status(hint) == TUPISTA_HINT_OK);
    CHECK(tupista_hint_has_placement(hint) == 1);

    TupistaPlacement placement{};
    REQUIRE(tupista_hint_placement(hint, &placement) == TUPISTA_OK);
    CHECK(placement.row == 7);
    CHECK(placement.col == 2);
    CHECK(placement.value == 5);

    const int32_t steps = tupista_hint_step_count(hint);
    REQUIRE(steps == 4);

    for (int32_t i = 0; i < steps; ++i) {
        CAPTURE(i);
        TupistaStep step{};
        REQUIRE(tupista_hint_step(hint, i, &step) == TUPISTA_OK);
        CHECK(step.tier >= 2);
        CHECK(step.digitCount > 0);
        CHECK(step.patternCount > 0);
        CHECK(step.eliminationCount > 0);

        std::vector<TupistaCell> cells(static_cast<std::size_t>(step.patternCount));
        CHECK(tupista_hint_step_cells(hint, i, cells.data(), step.patternCount) ==
              step.patternCount);

        std::vector<TupistaElimination> cuts(static_cast<std::size_t>(step.eliminationCount));
        CHECK(tupista_hint_step_eliminations(hint, i, cuts.data(), step.eliminationCount) ==
              step.eliminationCount);
        for (const TupistaElimination& cut : cuts) {
            CHECK(cut.digit >= 1);
            CHECK(cut.digit <= 9);
        }
    }

    tupista_hint_free(hint);
}

TEST_CASE("capi: the three disclosure levels differ and grow") {
    TupistaHint* hint = tupista_hint(asC(fixtures::kPuzzle).c_str());
    REQUIRE(hint != nullptr);

    std::string text[3];
    for (int32_t level = 0; level < 3; ++level) {
        const int32_t needed = tupista_hint_describe(hint, level, nullptr, 0);
        REQUIRE(needed > 0);
        std::vector<char> buffer(static_cast<std::size_t>(needed) + 1);
        CHECK(tupista_hint_describe(hint, level, buffer.data(), needed + 1) == needed);
        text[level] = buffer.data();
    }

    CHECK(text[0].size() < text[1].size());
    CHECK(text[1].size() < text[2].size());
    CHECK(tupista_hint_describe(hint, 99, nullptr, 0) == TUPISTA_ERR_BAD_INDEX);

    tupista_hint_free(hint);
}

TEST_CASE("capi: an illegal board yields conflicts rather than a hint") {
    std::string bad = asC(fixtures::kPuzzle);
    bad[3] = '2';

    TupistaHint* hint = tupista_hint(bad.c_str());
    REQUIRE(hint != nullptr);

    CHECK(tupista_hint_status(hint) == TUPISTA_HINT_BOARD_INVALID);
    CHECK(tupista_hint_has_placement(hint) == 0);
    CHECK(tupista_hint_conflicts(hint, nullptr, 0) > 0);

    tupista_hint_free(hint);
}

TEST_CASE("capi: a position beyond the engine says so") {
    TupistaHint* hint = tupista_hint(asC(fixtures::kRegressions[4].finalGrid).c_str());
    REQUIRE(hint != nullptr);
    CHECK(tupista_hint_status(hint) == TUPISTA_HINT_STUCK);
    tupista_hint_free(hint);
}

TEST_CASE("capi: null handles are survivable") {
    // A host with a bug must get an error code back, never a crash.
    CHECK(tupista_hint_status(nullptr) == TUPISTA_ERR_NULL_ARG);
    CHECK(tupista_hint_step_count(nullptr) == TUPISTA_ERR_NULL_ARG);
    CHECK(tupista_solve_tier(nullptr) == TUPISTA_ERR_NULL_ARG);
    CHECK(tupista_hint(nullptr) == nullptr);
    CHECK(tupista_solve_human("nope") == nullptr);

    // Freeing null is a no-op, matching free() and C# SafeHandle expectations.
    tupista_hint_free(nullptr);
    tupista_solve_free(nullptr);
}
