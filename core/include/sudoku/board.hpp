#pragma once

#include <array>        
#include <cstdint>      
#include <optional>   
#include <string>
#include <string_view>  

namespace sudoku {

// Guarantee one entity of kGridSize and kCellCount everywhere
inline constexpr int kGridSize = 9;
inline constexpr int kCellCount = 81;

enum class CellKind : std::uint8_t { Empty, Given, User };

struct Cell {
    std::uint8_t value = 0;
    CellKind kind = CellKind::Empty;

    // The player's own pencil marks: bit n set means they wrote digit n in the
    // corner of this cell. Same bit layout as DigitSet, but a plain integer so
    // Cell stays trivially copyable and easy to hand across the C ABI.
    //
    // THE ENGINE NEVER READS THIS TO SOLVE. Candidates are always recomputed
    // from placed digits; if solving trusted these, one slip by the player
    // would poison every hint. They exist so auditPencilMarks can compare the
    // player's notes against reality.
    std::uint16_t marks = 0;

    // Ask compiler to generate body (default lets the compiler generate it)
    bool operator==(const Cell&) const = default;
};

struct Board {
    // std::array = fixed-size array that knows its size and can be copied
    // and compared, unlike a raw C array. {} value-initializes every Cell.
    // One flat array of 81, addressed row-major, rather than 9x9
    std::array<Cell, kCellCount> cells{};

    // Row-major mapping: cell (row, col) lives at index row * 9 + col.
    Cell& at(int row, int col) { return cells[row * kGridSize + col]; }
    const Cell& at(int row, int col) const { return cells[row * kGridSize + col]; }

    int valueAt(int row, int col) const { return at(row, col).value; }

    // Pencil-mark access. Digits are 1..9, matching the bit index.
    bool hasMark(int row, int col, int digit) const {
        return (at(row, col).marks & (1u << digit)) != 0;
    }
    void setMark(int row, int col, int digit) {
        at(row, col).marks |= static_cast<std::uint16_t>(1u << digit);
    }
    void clearMark(int row, int col, int digit) {
        at(row, col).marks &= static_cast<std::uint16_t>(~(1u << digit));
    }
    void clearMarks(int row, int col) { at(row, col).marks = 0; }

    // Declared here, defined in board.cpp — no body means "implementation
    // elsewhere". The trailing const means "does not modify this object".
    bool full() const;

    static std::optional<Board> fromString(std::string_view text);
    std::string toString() const;
    
    bool operator==(const Board&) const = default;
};

}
