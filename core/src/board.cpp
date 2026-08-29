#include "sudoku/board.hpp"

namespace sudoku {

bool Board::full() const {
    for (const Cell& cell : cells)
        if (cell.value == 0) return false;
    return true;
}

std::optional<Board> Board::fromString(std::string_view text) {
    if (text.size() != kCellCount) return std::nullopt;

    Board board;  // stack value, default-initialized: 81 empty cells
    for (int i = 0; i < kCellCount; ++i) {
        char ch = text[i];
        if (ch == '0' || ch == '.') continue;          // empty cell, nothing to store
        if (ch < '1' || ch > '9') return std::nullopt; // any other character = bad input
        
        // char digit -> number: subtracting '0' works because '0'..'9' are
        // contiguous in every encoding C++ supports. The cast silences the
        // narrowing warning (int -> uint8_t) and documents it is intentional.
        board.cells[i].value = static_cast<std::uint8_t>(ch - '0');
        board.cells[i].kind = CellKind::Given;
    }

    return board;
}

std::string Board::toString() const {
    std::string out(kCellCount, '0');
    for (int i = 0; i < kCellCount; ++i)
        out[i] = static_cast<char>('0' + cells[i].value);
    return out;
}

}