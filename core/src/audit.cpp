#include "sudoku/audit.hpp"

#include <utility>  // std::move

#include "sudoku/candidates.hpp"

namespace sudoku {

std::vector<MarkAudit> auditPencilMarks(const Board& board) {
    const Candidates truth = Candidates::compute(board);
    std::vector<MarkAudit> report;

    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (board.at(row, col).marks == 0) continue;  // nothing noted here

            MarkAudit audit;
            audit.cell = {row, col};

            for (int digit = 1; digit <= kGridSize; ++digit) {
                const bool marked = board.hasMark(row, col, digit);

                // A filled cell has no candidates at all, so every mark on it
                // is a leftover; Candidates::compute already gives us that for
                // free by leaving filled cells empty.
                const bool possible = truth.has(row, col, digit);

                if (marked && !possible) audit.stale.push_back(digit);
                if (!marked && possible) audit.missing.push_back(digit);
            }

            if (!audit.missing.empty() || !audit.stale.empty())
                report.push_back(std::move(audit));
        }
    }

    return report;
}

}
