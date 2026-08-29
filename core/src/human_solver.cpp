#include "sudoku/human_solver.hpp"

#include <utility>

#include "logic_solver.hpp"

namespace sudoku {

HumanSolveResult solveHuman(const Board& board) {
    detail::LogicSolver solver(board);

    HumanSolveResult result;
    result.solved = solver.run();
    result.maxTier = solver.maxTier;
    result.placements = std::move(solver.placements);
    result.board = solver.board;
    return result;
}

Rating rateDifficulty(const Board& board) {
    const HumanSolveResult result = solveHuman(board);
    return {!result.solved, result.maxTier, std::string(tierName(result.maxTier))};
}

}
