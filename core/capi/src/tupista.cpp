// Implementation of the C ABI. This is the ONLY file allowed to know about
// both worlds: it speaks C++ inwards and C outwards.
//
// Three jobs, and nothing else:
//   1. translate C arguments into engine types,
//   2. translate engine results back into flat structs,
//   3. make sure no exception ever crosses the boundary.
//
// There is no logic here. Anything that looks like a decision belongs in the
// engine, where it can be unit tested without a C shim in the way.
#include "tupista.h"

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sudoku/audit.hpp"
#include "sudoku/board.hpp"
#include "sudoku/candidates.hpp"
#include "sudoku/hint.hpp"
#include "sudoku/human_solver.hpp"
#include "sudoku/solver.hpp"
#include "sudoku/validate.hpp"

namespace {

// Length of a C string, giving up after `limit` characters.
//
// Written out rather than using strlen: this memory comes from another
// language's runtime, and a host that forgets the NUL terminator must get an
// error back, not send us walking off the end of their buffer. (strnlen would
// do it, but it is POSIX rather than standard C++ and does not exist on every
// toolchain we target.)
std::size_t boundedLength(const char* text, std::size_t limit) {
    std::size_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

std::optional<sudoku::Board> parse(const char* board81) {
    if (board81 == nullptr) return std::nullopt;
    if (boundedLength(board81, sudoku::kCellCount + 1) != sudoku::kCellCount)
        return std::nullopt;
    return sudoku::Board::fromString(std::string_view(board81, sudoku::kCellCount));
}

int32_t copyString(const std::string& text, char* out, int32_t capacity) {
    if (out != nullptr && capacity > 0) {
        const std::size_t room = static_cast<std::size_t>(capacity) - 1;
        const std::size_t taken = text.size() < room ? text.size() : room;
        std::memcpy(out, text.data(), taken);
        out[taken] = '\0';
    }
    return static_cast<int32_t>(text.size());
}

void fillConflict(const sudoku::Conflict& from, TupistaConflict& to) {
    to.unit = static_cast<int32_t>(from.unit);
    to.unitIndex = from.unitIndex;
    to.value = from.value;
    to.cellCount = 0;
    for (const sudoku::CellRef& cell : from.cells) {
        if (to.cellCount >= 9) break;
        to.cells[to.cellCount].row = cell.row;
        to.cells[to.cellCount].col = cell.col;
        ++to.cellCount;
    }
}

void fillPlacement(const sudoku::Placement& from, TupistaPlacement& to) {
    to.row = from.row;
    to.col = from.col;
    to.value = from.value;
    to.technique = static_cast<int32_t>(from.technique);
    to.unit = static_cast<int32_t>(from.unit);
    to.unitIndex = from.unitIndex;
}

uint16_t toMask(const sudoku::DigitSet& digits) {
    uint16_t mask = 0;
    for (int digit = 1; digit <= sudoku::kGridSize; ++digit)
        if (digits.test(digit)) mask = static_cast<uint16_t>(mask | (1u << digit));
    return mask;
}

uint16_t toMask(const std::vector<int>& digits) {
    uint16_t mask = 0;
    for (int digit : digits) mask = static_cast<uint16_t>(mask | (1u << digit));
    return mask;
}

}  // namespace

// The engine itself does not throw in normal use, but a bad_alloc or a future
// change must not unwind into C. Every entry point is wrapped.
#define TUPISTA_GUARD_BEGIN try {
#define TUPISTA_GUARD_END(failure) \
    }                              \
    catch (...) { return (failure); }

struct TupistaSolve {
    sudoku::HumanSolveResult result;
    std::string tierName;
};

struct TupistaHint {
    sudoku::Hint hint;
};

extern "C" {

int32_t tupista_abi_version(void) { return 1; }

int32_t tupista_check_board(const char* board81) {
    TUPISTA_GUARD_BEGIN
    return parse(board81) ? TUPISTA_OK : TUPISTA_ERR_BAD_BOARD;
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

int32_t tupista_validate(const char* board81, TupistaConflict* out, int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return TUPISTA_ERR_BAD_BOARD;

    const std::vector<sudoku::Conflict> conflicts = sudoku::validate(*board);
    if (out != nullptr)
        for (std::size_t i = 0; i < conflicts.size() && i < static_cast<std::size_t>(capacity); ++i)
            fillConflict(conflicts[i], out[i]);
    return static_cast<int32_t>(conflicts.size());
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

int32_t tupista_count_solutions(const char* board81, int32_t cap) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return TUPISTA_ERR_BAD_BOARD;
    return static_cast<int32_t>(sudoku::countSolutions(*board, cap));
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

int32_t tupista_solve(const char* board81, char* solution82) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return TUPISTA_ERR_BAD_BOARD;
    if (solution82 == nullptr) return TUPISTA_ERR_NULL_ARG;

    const auto solved = sudoku::solveBacktracking(*board);
    if (!solved) return 0;

    const std::string text = solved->toString();
    std::memcpy(solution82, text.data(), text.size());
    solution82[text.size()] = '\0';
    return 1;
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

int32_t tupista_candidates(const char* board81, uint16_t* out81) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return TUPISTA_ERR_BAD_BOARD;
    if (out81 == nullptr) return TUPISTA_ERR_NULL_ARG;

    const sudoku::Candidates cand = sudoku::Candidates::compute(*board);
    for (int i = 0; i < sudoku::kCellCount; ++i) out81[i] = toMask(cand.cells[i]);
    return TUPISTA_OK;
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

int32_t tupista_audit_marks(const char* board81, const uint16_t* marks81,
                            TupistaMarkAudit* out, int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    auto board = parse(board81);
    if (!board) return TUPISTA_ERR_BAD_BOARD;
    if (marks81 == nullptr) return TUPISTA_ERR_NULL_ARG;

    for (int i = 0; i < sudoku::kCellCount; ++i) board->cells[i].marks = marks81[i];

    const std::vector<sudoku::MarkAudit> report = sudoku::auditPencilMarks(*board);
    if (out != nullptr)
        for (std::size_t i = 0; i < report.size() && i < static_cast<std::size_t>(capacity); ++i) {
            out[i].row = report[i].cell.row;
            out[i].col = report[i].cell.col;
            out[i].missing = toMask(report[i].missing);
            out[i].stale = toMask(report[i].stale);
        }
    return static_cast<int32_t>(report.size());
    TUPISTA_GUARD_END(TUPISTA_ERR_BAD_BOARD)
}

TupistaSolve* tupista_solve_human(const char* board81) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return nullptr;

    auto* handle = new TupistaSolve{};
    handle->result = sudoku::solveHuman(*board);
    handle->tierName = std::string(sudoku::tierName(handle->result.maxTier));
    return handle;
    TUPISTA_GUARD_END(nullptr)
}

void tupista_solve_free(TupistaSolve* solve) { delete solve; }

int32_t tupista_solve_is_solved(const TupistaSolve* solve) {
    return solve == nullptr ? TUPISTA_ERR_NULL_ARG : (solve->result.solved ? 1 : 0);
}

int32_t tupista_solve_tier(const TupistaSolve* solve) {
    return solve == nullptr ? TUPISTA_ERR_NULL_ARG : solve->result.maxTier;
}

int32_t tupista_solve_tier_name(const TupistaSolve* solve, char* out, int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    if (solve == nullptr) return TUPISTA_ERR_NULL_ARG;
    return copyString(solve->tierName, out, capacity);
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_solve_grid(const TupistaSolve* solve, char* out82) {
    TUPISTA_GUARD_BEGIN
    if (solve == nullptr || out82 == nullptr) return TUPISTA_ERR_NULL_ARG;
    const std::string text = solve->result.board.toString();
    std::memcpy(out82, text.data(), text.size());
    out82[text.size()] = '\0';
    return TUPISTA_OK;
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_solve_placement_count(const TupistaSolve* solve) {
    return solve == nullptr ? TUPISTA_ERR_NULL_ARG
                            : static_cast<int32_t>(solve->result.placements.size());
}

int32_t tupista_solve_placement(const TupistaSolve* solve, int32_t index,
                                TupistaPlacement* out) {
    TUPISTA_GUARD_BEGIN
    if (solve == nullptr || out == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (index < 0 || static_cast<std::size_t>(index) >= solve->result.placements.size())
        return TUPISTA_ERR_BAD_INDEX;
    fillPlacement(solve->result.placements[index], *out);
    return TUPISTA_OK;
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

TupistaHint* tupista_hint(const char* board81) {
    TUPISTA_GUARD_BEGIN
    const auto board = parse(board81);
    if (!board) return nullptr;

    auto* handle = new TupistaHint{};
    handle->hint = sudoku::nextHint(*board);
    return handle;
    TUPISTA_GUARD_END(nullptr)
}

void tupista_hint_free(TupistaHint* hint) { delete hint; }

int32_t tupista_hint_status(const TupistaHint* hint) {
    return hint == nullptr ? TUPISTA_ERR_NULL_ARG : static_cast<int32_t>(hint->hint.status);
}

int32_t tupista_hint_tier(const TupistaHint* hint) {
    return hint == nullptr ? TUPISTA_ERR_NULL_ARG : hint->hint.tier();
}

int32_t tupista_hint_has_placement(const TupistaHint* hint) {
    return hint == nullptr ? TUPISTA_ERR_NULL_ARG : (hint->hint.hasPlacement ? 1 : 0);
}

int32_t tupista_hint_placement(const TupistaHint* hint, TupistaPlacement* out) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr || out == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (!hint->hint.hasPlacement) return TUPISTA_ERR_BAD_INDEX;
    fillPlacement(hint->hint.placement, *out);
    return TUPISTA_OK;
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_hint_conflicts(const TupistaHint* hint, TupistaConflict* out,
                               int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr) return TUPISTA_ERR_NULL_ARG;
    const auto& conflicts = hint->hint.conflicts;
    if (out != nullptr)
        for (std::size_t i = 0; i < conflicts.size() && i < static_cast<std::size_t>(capacity); ++i)
            fillConflict(conflicts[i], out[i]);
    return static_cast<int32_t>(conflicts.size());
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_hint_step_count(const TupistaHint* hint) {
    return hint == nullptr ? TUPISTA_ERR_NULL_ARG
                           : static_cast<int32_t>(hint->hint.eliminations.size());
}

int32_t tupista_hint_step(const TupistaHint* hint, int32_t index, TupistaStep* out) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr || out == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (index < 0 || static_cast<std::size_t>(index) >= hint->hint.eliminations.size())
        return TUPISTA_ERR_BAD_INDEX;

    const sudoku::Finding& finding = hint->hint.eliminations[index];
    out->technique = static_cast<int32_t>(finding.technique);
    out->tier = sudoku::tierOf(finding.technique);
    out->unit = static_cast<int32_t>(finding.unit);
    out->unitIndex = finding.unitIndex;
    out->patternCount = static_cast<int32_t>(finding.pattern.size());
    out->eliminationCount = static_cast<int32_t>(finding.eliminations.size());
    out->digitCount = 0;
    for (int digit : finding.digits) {
        if (out->digitCount >= 9) break;
        out->digits[out->digitCount++] = digit;
    }
    return TUPISTA_OK;
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_hint_step_cells(const TupistaHint* hint, int32_t index, TupistaCell* out,
                                int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (index < 0 || static_cast<std::size_t>(index) >= hint->hint.eliminations.size())
        return TUPISTA_ERR_BAD_INDEX;

    const auto& pattern = hint->hint.eliminations[index].pattern;
    if (out != nullptr)
        for (std::size_t i = 0; i < pattern.size() && i < static_cast<std::size_t>(capacity); ++i) {
            out[i].row = pattern[i].row;
            out[i].col = pattern[i].col;
        }
    return static_cast<int32_t>(pattern.size());
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_hint_step_eliminations(const TupistaHint* hint, int32_t index,
                                       TupistaElimination* out, int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (index < 0 || static_cast<std::size_t>(index) >= hint->hint.eliminations.size())
        return TUPISTA_ERR_BAD_INDEX;

    const auto& cuts = hint->hint.eliminations[index].eliminations;
    if (out != nullptr)
        for (std::size_t i = 0; i < cuts.size() && i < static_cast<std::size_t>(capacity); ++i) {
            out[i].row = cuts[i].row;
            out[i].col = cuts[i].col;
            out[i].digit = cuts[i].digit;
        }
    return static_cast<int32_t>(cuts.size());
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

int32_t tupista_hint_describe(const TupistaHint* hint, int32_t level, char* out,
                              int32_t capacity) {
    TUPISTA_GUARD_BEGIN
    if (hint == nullptr) return TUPISTA_ERR_NULL_ARG;
    if (level < TUPISTA_LEVEL_VAGUE || level > TUPISTA_LEVEL_FULL)
        return TUPISTA_ERR_BAD_INDEX;
    return copyString(sudoku::describe(hint->hint, static_cast<sudoku::HintLevel>(level)),
                      out, capacity);
    TUPISTA_GUARD_END(TUPISTA_ERR_NULL_ARG)
}

}  // extern "C"
