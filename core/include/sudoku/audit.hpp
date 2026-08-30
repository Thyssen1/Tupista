#pragma once

#include <vector>

#include "sudoku/board.hpp"
#include "sudoku/units.hpp"

namespace sudoku {

// What is wrong with the pencil marks in one cell.
//
// `missing` — digits that are genuinely still possible here but the player has
//             not written down. Overlooking one of these is how people miss a
//             hidden single.
// `stale`   — digits the player pencilled in that a later placement has since
//             ruled out. These are worse than missing ones: stale marks make a
//             cell look more open than it is, so the player never revisits it.
struct MarkAudit {
    CellRef cell;
    std::vector<int> missing;
    std::vector<int> stale;
};

// Compare the player's pencil marks against the candidates the board actually
// supports.
//
// Only cells the player has marked are reported, and only when something is
// wrong — an unmarked cell is not an error, it is a cell they have not got to
// yet. A cell that already holds a digit but still carries marks has all of
// them reported as stale: the question is settled, the notes are leftovers.
//
// This is the one place the engine looks at pencil marks at all.
std::vector<MarkAudit> auditPencilMarks(const Board& board);

}
