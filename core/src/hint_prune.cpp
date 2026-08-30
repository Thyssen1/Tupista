#include "hint_prune.hpp"

#include <algorithm>
#include <cstddef>  // std::size_t

#include "sudoku/candidates.hpp"
#include "sudoku/units.hpp"

namespace sudoku::detail {

namespace {

bool contains(const std::vector<CellRef>& cells, CellRef target) {
    return std::find(cells.begin(), cells.end(), target) != cells.end();
}

// Every digit a cell must NOT have, given the candidates it was recorded with.
// Used by the techniques whose pattern is a claim about the pattern cells
// themselves: naked sets ("these cells hold only these digits"), XY-Wing and
// XY-Chain ("every cell in this chain has exactly two candidates").
void demandCellsHoldOnly(const Finding& finding, std::vector<Elimination>& out) {
    const std::size_t count = std::min(finding.pattern.size(), finding.patternDigits.size());
    for (std::size_t i = 0; i < count; ++i)
        for (int digit = 1; digit <= kGridSize; ++digit)
            if (!finding.patternDigits[i].test(digit))
                out.push_back({finding.pattern[i].row, finding.pattern[i].col, digit});
}

// Every digit that must be absent from the rest of a unit. Used by the
// techniques whose pattern is a claim about where a digit CANNOT be: pointing,
// claiming, hidden sets, and each base line of an X-Wing.
void demandUnitIsClear(const Finding& finding, const Board& board, UnitKind unit,
                       int unitIndex, std::vector<Elimination>& out) {
    for (int slot = 0; slot < kGridSize; ++slot) {
        const CellRef cell = unitCell(unit, unitIndex, slot);
        if (board.valueAt(cell.row, cell.col) != 0) continue;
        if (contains(finding.pattern, cell)) continue;
        for (int digit : finding.digits) out.push_back({cell.row, cell.col, digit});
    }
}

}

std::vector<Elimination> requiredAbsences(const Finding& finding, const Board& board) {
    std::vector<Elimination> out;

    switch (finding.technique) {
        case Technique::NakedSingle:
        case Technique::HiddenSingle:
            break;  // placements, not patterns

        case Technique::Pointing:
        case Technique::Claiming:
        case Technique::HiddenPair:
        case Technique::HiddenTriple:
            if (finding.unitIndex >= 0)
                demandUnitIsClear(finding, board, finding.unit, finding.unitIndex, out);
            break;

        case Technique::NakedPair:
        case Technique::NakedTriple:
        case Technique::XYWing:
        case Technique::XYChain:
            demandCellsHoldOnly(finding, out);
            break;

        case Technique::XWing: {
            // The pattern is four cells spanning two base lines. `unit` says
            // whether those lines are rows or columns; each of them must have
            // no other spot for the digit.
            std::vector<int> lines;
            for (const CellRef& cell : finding.pattern) {
                const int line = finding.unit == UnitKind::Row ? cell.row : cell.col;
                if (std::find(lines.begin(), lines.end(), line) == lines.end())
                    lines.push_back(line);
            }
            for (int line : lines)
                demandUnitIsClear(finding, board, finding.unit, line, out);
            break;
        }
    }

    return out;
}

std::vector<Finding> pruneChain(const Board& board,
                                const std::vector<Finding>& chain,
                                const Placement& target) {
    const Candidates opening = Candidates::compute(board);

    // What does the placement itself need? A naked single needs every other
    // candidate gone from its cell; a hidden single needs its digit gone from
    // every other cell of its unit.
    std::vector<Elimination> wanted;
    if (target.technique == Technique::NakedSingle) {
        for (int digit = 1; digit <= kGridSize; ++digit)
            if (digit != target.value && opening.has(target.row, target.col, digit))
                wanted.push_back({target.row, target.col, digit});
    } else if (target.unitIndex >= 0) {
        for (int slot = 0; slot < kGridSize; ++slot) {
            const CellRef cell = unitCell(target.unit, target.unitIndex, slot);
            if (cell.row == target.row && cell.col == target.col) continue;
            if (board.valueAt(cell.row, cell.col) != 0) continue;
            if (opening.has(cell.row, cell.col, target.value))
                wanted.push_back({cell.row, cell.col, target.value});
        }
    }

    // Work item: an absence we need, plus how far back in the chain we may
    // look for whoever produced it. Restricting the search to EARLIER findings
    // is what stops a dependency cycle: a step can only ever rest on steps
    // that came before it.
    struct Need {
        Elimination absence;
        std::size_t before;
    };

    std::vector<Need> work;
    for (const Elimination& absence : wanted) work.push_back({absence, chain.size()});

    std::vector<bool> keep(chain.size(), false);

    while (!work.empty()) {
        const Need need = work.back();
        work.pop_back();

        // Never a candidate in the first place, so nothing had to remove it.
        if (!opening.has(need.absence.row, need.absence.col, need.absence.digit)) continue;

        std::size_t owner = chain.size();
        for (std::size_t i = 0; i < need.before; ++i) {
            const auto& cuts = chain[i].eliminations;
            if (std::find(cuts.begin(), cuts.end(), need.absence) != cuts.end()) {
                owner = i;
                break;
            }
        }
        if (owner == chain.size()) continue;  // nothing in the chain removed it
        if (keep[owner]) continue;

        keep[owner] = true;
        for (const Elimination& absence : requiredAbsences(chain[owner], board))
            work.push_back({absence, owner});
    }

    std::vector<Finding> pruned;
    for (std::size_t i = 0; i < chain.size(); ++i)
        if (keep[i]) pruned.push_back(chain[i]);
    return pruned;
}

}
