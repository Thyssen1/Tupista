#include "sudoku/hint.hpp"

#include <algorithm>
#include <utility>

#include "logic_solver.hpp"
#include "sudoku/solver.hpp"

namespace sudoku {

namespace {

std::string joinDigits(const std::vector<int>& digits) {
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0) out += (i + 1 == digits.size()) ? " and " : ", ";
        out += std::to_string(digits[i]);
    }
    return out;
}

std::string joinCells(const std::vector<CellRef>& cells) {
    std::string out;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) out += (i + 1 == cells.size()) ? " and " : ", ";
        out += cellName(cells[i]);
    }
    return out;
}

// Where should the player look? For a pattern that lives in one unit, name it.
// X-Wings and chains span the grid, so they get the cells instead.
std::string whereToLook(const Finding& finding) {
    if (finding.unitIndex >= 0) return unitName(finding.unit, finding.unitIndex);
    return "cells " + joinCells(finding.pattern);
}

std::string describePlacement(const Placement& placement) {
    std::string out = cellName({placement.row, placement.col}) + " = " +
                      std::to_string(placement.value) + " (" +
                      std::string(nameOf(placement.technique));
    if (placement.technique == Technique::HiddenSingle && placement.unitIndex >= 0)
        out += " in " + unitName(placement.unit, placement.unitIndex);
    return out + ")";
}

}

std::string cellName(CellRef cell) {
    return "R" + std::to_string(cell.row + 1) + "C" + std::to_string(cell.col + 1);
}

std::string unitName(UnitKind unit, int unitIndex) {
    switch (unit) {
        case UnitKind::Row: return "row " + std::to_string(unitIndex + 1);
        case UnitKind::Col: return "column " + std::to_string(unitIndex + 1);
        case UnitKind::Box: return "box " + std::to_string(unitIndex + 1);
    }
    return "unknown unit";
}

int Hint::tier() const {
    int highest = 0;
    for (const Finding& finding : eliminations)
        highest = std::max(highest, tierOf(finding.technique));
    if (hasPlacement) highest = std::max(highest, tierOf(placement.technique));
    return highest;
}

Hint nextHint(const Board& board) {
    Hint hint;

    // Rule 3 of the hint design: validate before hinting. A board with
    // duplicates, no solution, or several solutions cannot yield honest
    // advice, so say what is wrong instead of guessing.
    hint.conflicts = validate(board);
    if (!hint.conflicts.empty()) {
        hint.status = HintStatus::BoardInvalid;
        return hint;
    }
    if (board.full()) {
        hint.status = HintStatus::Solved;
        return hint;
    }
    if (countSolutions(board, 2) != 1) {
        hint.status = HintStatus::NotUnique;
        return hint;
    }

    detail::LogicSolver solver(board);

    // Rule 4: check for a raw single first. If one is already visible, no
    // elimination chain is needed and this is a genuine tier-1 hint.
    if (const auto single = solver.findSingle()) {
        hint.status = HintStatus::Ok;
        hint.placement = *single;
        hint.hasPlacement = true;
        return hint;
    }

    // Rule 1: the one-step tracer. Apply exactly ONE elimination at a time,
    // cheapest technique first, and after each one ask whether a single has
    // appeared. The log of eliminations IS the hint.
    //
    // This terminates: every recorded finding removes at least one candidate,
    // candidates are never recomputed inside this loop, and there are only
    // 81 x 9 of them to remove.
    solver.mode = detail::ApplyMode::One;
    while (solver.eliminateOnce()) {
        if (const auto single = solver.findSingle()) {
            hint.status = HintStatus::Ok;
            hint.eliminations = std::move(solver.findings);
            hint.placement = *single;
            hint.hasPlacement = true;
            return hint;
        }
    }

    hint.status = HintStatus::Stuck;
    return hint;
}

std::string describe(const Hint& hint, HintLevel level) {
    switch (hint.status) {
        case HintStatus::Solved:
            return "The puzzle is finished.";
        case HintStatus::BoardInvalid: {
            std::string out = "That grid breaks the rules — fix it before asking for a hint.";
            if (level != HintLevel::Vague && !hint.conflicts.empty()) {
                const Conflict& first = hint.conflicts.front();
                out += " " + std::to_string(first.value) + " appears more than once in " +
                       unitName(first.unit, first.unitIndex);
                if (level == HintLevel::Full) out += " (" + joinCells(first.cells) + ")";
                out += ".";
            }
            return out;
        }
        case HintStatus::NotUnique:
            return "This grid does not have exactly one solution, so any hint would be "
                   "guesswork. Check the digits you entered.";
        case HintStatus::Stuck:
            return "No step found: this position is beyond the techniques the engine knows.";
        case HintStatus::Ok:
            break;
    }

    // A hint with no elimination chain is directly visible to the player.
    if (hint.eliminations.empty()) {
        const Placement& p = hint.placement;
        switch (level) {
            case HintLevel::Vague:
                return p.technique == Technique::HiddenSingle && p.unitIndex >= 0
                           ? "Look at " + unitName(p.unit, p.unitIndex) + " for digit " +
                                 std::to_string(p.value) + "."
                           : "One cell has only a single candidate left.";
            case HintLevel::Mechanism:
                return p.technique == Technique::HiddenSingle && p.unitIndex >= 0
                           ? "Digit " + std::to_string(p.value) + " fits only one cell of " +
                                 unitName(p.unit, p.unitIndex) + ": a hidden single."
                           : "A cell is down to one candidate: a naked single.";
            case HintLevel::Full:
                return "Place " + describePlacement(p) + ".";
        }
    }

    const Finding& first = hint.eliminations.front();
    switch (level) {
        case HintLevel::Vague:
            return "Look at " + whereToLook(first) + " for " +
                   (first.digits.size() == 1 ? "digit " : "digits ") +
                   joinDigits(first.digits) + ".";

        case HintLevel::Mechanism:
            return std::string(nameOf(first.technique)) + " on " +
                   (first.digits.size() == 1 ? "digit " : "digits ") +
                   joinDigits(first.digits) + " in " + whereToLook(first) +
                   " rules candidates out elsewhere. " +
                   (hint.eliminations.size() == 1
                        ? "That is enough to place a digit."
                        : "Chain " + std::to_string(hint.eliminations.size()) +
                              " deductions like this to place a digit.");

        case HintLevel::Full: {
            std::string out;
            for (std::size_t i = 0; i < hint.eliminations.size(); ++i) {
                const Finding& finding = hint.eliminations[i];
                out += std::to_string(i + 1) + ". " + std::string(nameOf(finding.technique)) +
                       " on " + joinDigits(finding.digits) + " at " +
                       joinCells(finding.pattern);
                if (finding.unitIndex >= 0)
                    out += " (" + unitName(finding.unit, finding.unitIndex) + ")";
                out += " removes ";
                for (std::size_t k = 0; k < finding.eliminations.size(); ++k) {
                    const Elimination& cut = finding.eliminations[k];
                    if (k > 0) out += ", ";
                    out += std::to_string(cut.digit) + " from " + cellName({cut.row, cut.col});
                }
                out += ".\n";
            }
            out += "Then place " + describePlacement(hint.placement) + ".";
            return out;
        }
    }

    return {};
}

}
