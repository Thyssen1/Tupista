#include "logic_solver.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <utility>

#include "sudoku/units.hpp"

namespace sudoku::detail {

LogicSolver::LogicSolver(const Board& start) : board(start) { recompute(); }

void LogicSolver::recompute() { cand = Candidates::compute(board); }

bool LogicSolver::solved() const { return board.full(); }

void LogicSolver::bumpTier(int tier) { if (tier > maxTier) maxTier = tier; }

bool LogicSolver::record(Finding finding) {
    bumpTier(tierOf(finding.technique));
    findings.push_back(std::move(finding));
    return stopAfterFinding();
}

// ---------------------------------------------------------------------------
// Tier 1: singles
// ---------------------------------------------------------------------------
//
// Naked single:  a cell with exactly one candidate left.
// Hidden single: a digit that fits in only one cell of some unit, even though
//                that cell may have several candidates of its own.
//
// Exactly one digit is placed per call, then candidates are rebuilt. Placing
// one at a time is what keeps the placement list a readable narrative.
std::optional<Placement> LogicSolver::findSingle() const {
    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            if (board.valueAt(row, col) != 0) continue;
            const int digit = onlyDigit(cand.at(row, col));
            if (digit == 0) continue;
            return Placement{row, col, digit, Technique::NakedSingle, {}, -1};
        }
    }

    // Digit is the OUTER loop and unit the inner one, matching the original
    // engine: it changes which hidden single is found first, and therefore the
    // whole downstream sequence of placements.
    for (int digit = 1; digit <= kGridSize; ++digit) {
        for (int flatUnit = 0; flatUnit < kUnitCount; ++flatUnit) {
            int seen = 0;
            CellRef last{};
            for (int slot = 0; slot < kGridSize; ++slot) {
                const CellRef ref = unitCellAt(flatUnit, slot);
                if (board.valueAt(ref.row, ref.col) != 0) continue;
                if (!cand.has(ref.row, ref.col, digit)) continue;
                ++seen;
                last = ref;
            }
            if (seen != 1) continue;

            return Placement{last.row, last.col, digit, Technique::HiddenSingle,
                             unitKindAt(flatUnit), unitIndexAt(flatUnit)};
        }
    }

    return std::nullopt;
}

bool LogicSolver::placeSingle() {
    const std::optional<Placement> single = findSingle();
    if (!single) return false;

    board.at(single->row, single->col).value = static_cast<std::uint8_t>(single->value);
    board.at(single->row, single->col).kind = CellKind::User;
    placements.push_back(*single);
    bumpTier(1);
    recompute();
    return true;
}

// ---------------------------------------------------------------------------
// Tier 2: pointing and claiming
// ---------------------------------------------------------------------------
//
// Pointing: inside one box, every remaining spot for a digit lies in a single
// row (or column). The digit must go in this box, so it cannot appear anywhere
// else along that row/column.
//
// Claiming: the mirror image. Inside one row (or column), every remaining spot
// for a digit lies in a single box, so the digit is ruled out of the rest of
// that box.
bool LogicSolver::pointingClaiming() {
    bool changed = false;

    for (int box = 0; box < kGridSize; ++box) {
        const int boxRow = (box / 3) * 3;
        const int boxCol = (box % 3) * 3;

        for (int digit = 1; digit <= kGridSize; ++digit) {
            std::vector<CellRef> spots;
            for (int row = boxRow; row < boxRow + 3; ++row)
                for (int col = boxCol; col < boxCol + 3; ++col)
                    if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                        spots.push_back({row, col});
            if (spots.size() < 2) continue;

            bool sameRow = true, sameCol = true;
            for (const CellRef& s : spots) {
                if (s.row != spots[0].row) sameRow = false;
                if (s.col != spots[0].col) sameCol = false;
            }

            if (sameRow) {
                std::vector<Elimination> cuts;
                const int row = spots[0].row;
                for (int col = 0; col < kGridSize; ++col) {
                    if (col >= boxCol && col < boxCol + 3) continue;
                    if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                        cuts.push_back({row, col, digit});
                }
                if (!cuts.empty()) {
                    for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);
                    changed = true;
                    if (record({Technique::Pointing, spots, {digit},
                                UnitKind::Box, box, cuts})) return true;
                }
            }

            if (sameCol) {
                std::vector<Elimination> cuts;
                const int col = spots[0].col;
                for (int row = 0; row < kGridSize; ++row) {
                    if (row >= boxRow && row < boxRow + 3) continue;
                    if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                        cuts.push_back({row, col, digit});
                }
                if (!cuts.empty()) {
                    for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);
                    changed = true;
                    if (record({Technique::Pointing, spots, {digit},
                                UnitKind::Box, box, cuts})) return true;
                }
            }
        }
    }

    // Claiming from a row into a box.
    for (int row = 0; row < kGridSize; ++row) {
        for (int digit = 1; digit <= kGridSize; ++digit) {
            std::vector<CellRef> spots;
            for (int col = 0; col < kGridSize; ++col)
                if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                    spots.push_back({row, col});
            if (spots.size() < 2) continue;

            bool sameBand = true;
            for (const CellRef& s : spots)
                if (s.col / 3 != spots[0].col / 3) sameBand = false;
            if (!sameBand) continue;

            const int boxRow = (row / 3) * 3;
            const int boxCol = (spots[0].col / 3) * 3;
            std::vector<Elimination> cuts;
            for (int r = boxRow; r < boxRow + 3; ++r)
                for (int c = boxCol; c < boxCol + 3; ++c)
                    if (r != row && board.valueAt(r, c) == 0 && cand.has(r, c, digit))
                        cuts.push_back({r, c, digit});
            if (cuts.empty()) continue;

            for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);
            changed = true;
            if (record({Technique::Claiming, spots, {digit},
                        UnitKind::Row, row, cuts})) return true;
        }
    }

    // Claiming from a column into a box.
    for (int col = 0; col < kGridSize; ++col) {
        for (int digit = 1; digit <= kGridSize; ++digit) {
            std::vector<CellRef> spots;
            for (int row = 0; row < kGridSize; ++row)
                if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                    spots.push_back({row, col});
            if (spots.size() < 2) continue;

            bool sameBand = true;
            for (const CellRef& s : spots)
                if (s.row / 3 != spots[0].row / 3) sameBand = false;
            if (!sameBand) continue;

            const int boxRow = (spots[0].row / 3) * 3;
            const int boxCol = (col / 3) * 3;
            std::vector<Elimination> cuts;
            for (int r = boxRow; r < boxRow + 3; ++r)
                for (int c = boxCol; c < boxCol + 3; ++c)
                    if (c != col && board.valueAt(r, c) == 0 && cand.has(r, c, digit))
                        cuts.push_back({r, c, digit});
            if (cuts.empty()) continue;

            for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);
            changed = true;
            if (record({Technique::Claiming, spots, {digit},
                        UnitKind::Col, col, cuts})) return true;
        }
    }

    return changed;
}

// ---------------------------------------------------------------------------
// Tiers 3 and 4: naked sets
// ---------------------------------------------------------------------------
//
// N cells in one unit that between them hold exactly N candidates must consume
// those N digits, so no other cell in the unit can use any of them.
//
// The search stops at the first productive set, mirroring the original engine:
// the early exit is load-bearing, because applying every naked set at once
// would change which placements come next and therefore the difficulty rating.
bool LogicSolver::nakedSets(int size) {
    bool changed = false;

    for (int flatUnit = 0; flatUnit < kUnitCount; ++flatUnit) {
        std::vector<CellRef> empties;
        for (int slot = 0; slot < kGridSize; ++slot) {
            const CellRef ref = unitCellAt(flatUnit, slot);
            if (board.valueAt(ref.row, ref.col) == 0) empties.push_back(ref);
        }

        std::vector<int> combo;
        // std::function so the lambda can call itself. A plain auto lambda
        // cannot name its own type, and this recursion is over combinations
        // of cells: pick `size` of them, then test the union of candidates.
        std::function<void(int, int)> search = [&](int start, int depth) {
            if (changed) return;

            if (depth == size) {
                DigitSet united;
                for (int index : combo) united |= cand.at(empties[index].row, empties[index].col);
                if (static_cast<int>(united.count()) != size) return;

                std::vector<Elimination> cuts;
                for (int index = 0; index < static_cast<int>(empties.size()); ++index) {
                    if (std::find(combo.begin(), combo.end(), index) != combo.end()) continue;
                    const CellRef ref = empties[index];
                    for (int digit = 1; digit <= kGridSize; ++digit)
                        if (united.test(digit) && cand.has(ref.row, ref.col, digit))
                            cuts.push_back({ref.row, ref.col, digit});
                }
                if (cuts.empty()) return;

                for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);

                std::vector<CellRef> pattern;
                for (int index : combo) pattern.push_back(empties[index]);
                std::vector<int> digits;
                for (int digit = 1; digit <= kGridSize; ++digit)
                    if (united.test(digit)) digits.push_back(digit);

                changed = true;
                record({size == 2 ? Technique::NakedPair : Technique::NakedTriple,
                        pattern, digits, unitKindAt(flatUnit), unitIndexAt(flatUnit), cuts});
                return;
            }

            for (int index = start; index < static_cast<int>(empties.size()); ++index) {
                const CellRef ref = empties[index];
                const int n = cand.count(ref.row, ref.col);
                if (n < 2 || n > size) continue;
                combo.push_back(index);
                search(index + 1, depth + 1);
                combo.pop_back();
                if (changed) return;
            }
        };

        search(0, 0);
        if (changed) break;
    }

    return changed;
}

// ---------------------------------------------------------------------------
// Tiers 3 and 4: hidden sets
// ---------------------------------------------------------------------------
//
// The dual of a naked set: N digits that between them fit in only N cells of a
// unit. Those cells must take exactly those digits, so every OTHER candidate
// is stripped out of them.
bool LogicSolver::hiddenSets(int size) {
    bool changed = false;

    for (int flatUnit = 0; flatUnit < kUnitCount; ++flatUnit) {
        std::vector<int> digits;
        std::function<void(int, int)> search = [&](int start, int depth) {
            if (changed) return;

            if (depth == size) {
                // Every digit in the combination must actually appear somewhere
                // in this unit, otherwise the "set" is vacuous.
                for (int digit : digits) {
                    bool present = false;
                    for (int slot = 0; slot < kGridSize && !present; ++slot) {
                        const CellRef ref = unitCellAt(flatUnit, slot);
                        if (board.valueAt(ref.row, ref.col) == 0 &&
                            cand.has(ref.row, ref.col, digit))
                            present = true;
                    }
                    if (!present) return;
                }

                std::vector<CellRef> spots;
                for (int slot = 0; slot < kGridSize; ++slot) {
                    const CellRef ref = unitCellAt(flatUnit, slot);
                    if (board.valueAt(ref.row, ref.col) != 0) continue;
                    for (int digit : digits)
                        if (cand.has(ref.row, ref.col, digit)) { spots.push_back(ref); break; }
                }
                if (static_cast<int>(spots.size()) != size) return;

                DigitSet keep;
                for (int digit : digits) keep.set(digit);

                std::vector<Elimination> cuts;
                for (const CellRef& ref : spots)
                    for (int digit = 1; digit <= kGridSize; ++digit)
                        if (!keep.test(digit) && cand.has(ref.row, ref.col, digit))
                            cuts.push_back({ref.row, ref.col, digit});
                if (cuts.empty()) return;

                for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);

                changed = true;
                record({size == 2 ? Technique::HiddenPair : Technique::HiddenTriple,
                        spots, digits, unitKindAt(flatUnit), unitIndexAt(flatUnit), cuts});
                return;
            }

            for (int digit = start; digit <= kGridSize; ++digit) {
                digits.push_back(digit);
                search(digit + 1, depth + 1);
                digits.pop_back();
                if (changed) return;
            }
        };

        search(1, 0);
        if (changed) break;
    }

    return changed;
}

// ---------------------------------------------------------------------------
// Tier 5: X-Wing
// ---------------------------------------------------------------------------
//
// A digit has exactly two possible spots in each of two rows, and both rows use
// the SAME pair of columns. Whichever way it resolves, those two columns are
// spoken for, so the digit leaves every other cell of both columns.
// The same argument runs with rows and columns swapped, which is what `base`
// switches between.
bool LogicSolver::xWing() {
    bool changed = false;

    for (int digit = 1; digit <= kGridSize; ++digit) {
        for (int base = 0; base < 2; ++base) {
            std::array<std::vector<int>, kGridSize> lines;
            for (int i = 0; i < kGridSize; ++i)
                for (int j = 0; j < kGridSize; ++j) {
                    const int row = base == 0 ? i : j;
                    const int col = base == 0 ? j : i;
                    if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                        lines[i].push_back(j);
                }

            for (int i = 0; i < kGridSize - 1; ++i) {
                if (lines[i].size() != 2) continue;
                for (int j = i + 1; j < kGridSize; ++j) {
                    if (lines[j] != lines[i]) continue;

                    std::vector<Elimination> cuts;
                    for (int k = 0; k < kGridSize; ++k) {
                        if (k == i || k == j) continue;
                        for (int m : lines[i]) {
                            const int row = base == 0 ? k : m;
                            const int col = base == 0 ? m : k;
                            if (board.valueAt(row, col) == 0 && cand.has(row, col, digit))
                                cuts.push_back({row, col, digit});
                        }
                    }
                    if (cuts.empty()) continue;

                    for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);

                    std::vector<CellRef> pattern;
                    for (int line : {i, j})
                        for (int m : lines[i])
                            pattern.push_back(base == 0 ? CellRef{line, m} : CellRef{m, line});

                    changed = true;
                    if (record({Technique::XWing, pattern, {digit}, UnitKind::Row, -1, cuts}))
                        return true;
                }
            }
        }
    }

    return changed;
}

// ---------------------------------------------------------------------------
// Tier 6: XY-Wing
// ---------------------------------------------------------------------------
//
// Three cells with exactly two candidates each: a pivot {X,Y} that sees a wing
// {X,Z} and another wing {Y,Z}. Whichever digit the pivot takes, one of the
// wings is forced to Z — so any cell seeing BOTH wings cannot be Z.
bool LogicSolver::xyWing() {
    bool changed = false;

    std::vector<CellRef> bivalue;
    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col)
            if (board.valueAt(row, col) == 0 && cand.count(row, col) == 2)
                bivalue.push_back({row, col});

    for (const CellRef& pivot : bivalue) {
        int x = 0, y = 0;
        for (int digit = 1; digit <= kGridSize; ++digit)
            if (cand.has(pivot.row, pivot.col, digit)) { if (x == 0) x = digit; else y = digit; }

        for (const CellRef& wing1 : bivalue) {
            if (!sees(pivot, wing1)) continue;
            for (const CellRef& wing2 : bivalue) {
                if (wing2 == wing1 || !sees(pivot, wing2)) continue;

                for (int z = 1; z <= kGridSize; ++z) {
                    if (z == x || z == y) continue;

                    DigitSet needed1, needed2;
                    needed1.set(x); needed1.set(z);
                    needed2.set(y); needed2.set(z);
                    if (cand.at(wing1.row, wing1.col) != needed1) continue;
                    if (cand.at(wing2.row, wing2.col) != needed2) continue;

                    std::vector<Elimination> cuts;
                    for (int row = 0; row < kGridSize; ++row)
                        for (int col = 0; col < kGridSize; ++col) {
                            const CellRef here{row, col};
                            if (board.valueAt(row, col) != 0) continue;
                            if (here == pivot || here == wing1 || here == wing2) continue;
                            if (sees(here, wing1) && sees(here, wing2) &&
                                cand.has(row, col, z))
                                cuts.push_back({row, col, z});
                        }
                    if (cuts.empty()) continue;

                    for (const Elimination& e : cuts) cand.remove(e.row, e.col, e.digit);
                    changed = true;
                    if (record({Technique::XYWing, {pivot, wing1, wing2}, {x, y, z},
                                UnitKind::Row, -1, cuts})) return true;
                }
            }
        }
    }

    return changed;
}

// ---------------------------------------------------------------------------
// Tier 7: XY-Chain
// ---------------------------------------------------------------------------
//
// A chain of bivalue cells, each seeing the next, that carries an implication
// from one end to the other: "if this end is not X, the other end is X".
// Both ends therefore cannot both be non-X, so any cell seeing BOTH ends
// cannot be X.
//
// Explored breadth-first from every bivalue cell and every starting digit,
// with a hard depth cap so a pathological board cannot hang the search.
bool LogicSolver::xyChains() {
    std::vector<CellRef> bivalue;
    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col)
            if (board.valueAt(row, col) == 0 && cand.count(row, col) == 2)
                bivalue.push_back({row, col});

    // The other of a bivalue cell's two candidates.
    auto partnerDigit = [&](const CellRef& ref, int digit) {
        for (int other = 1; other <= kGridSize; ++other)
            if (other != digit && cand.has(ref.row, ref.col, other)) return other;
        return 0;
    };

    struct Node {
        CellRef cell;
        int carry = 0;
        std::vector<CellRef> path;
    };

    for (const CellRef& start : bivalue) {
        for (int x = 1; x <= kGridSize; ++x) {
            if (!cand.has(start.row, start.col, x)) continue;

            std::vector<Node> queue{{start, partnerDigit(start, x), {start}}};
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const Node current = queue[head];
                if (current.path.size() > 8) continue;

                for (const CellRef& next : bivalue) {
                    if (!sees(current.cell, next)) continue;
                    if (!cand.has(next.row, next.col, current.carry)) continue;

                    const bool alreadyOnPath =
                        std::find(current.path.begin(), current.path.end(), next) !=
                        current.path.end();
                    if (alreadyOnPath) continue;

                    std::vector<CellRef> path = current.path;
                    path.push_back(next);
                    const int carry = partnerDigit(next, current.carry);

                    if (carry == x && path.size() >= 3) {
                        std::vector<Elimination> cuts;
                        for (int row = 0; row < kGridSize; ++row)
                            for (int col = 0; col < kGridSize; ++col) {
                                const CellRef here{row, col};
                                if (board.valueAt(row, col) != 0) continue;
                                if (!cand.has(row, col, x)) continue;
                                if (std::find(path.begin(), path.end(), here) != path.end())
                                    continue;
                                if (sees(here, start) && sees(here, next))
                                    cuts.push_back({row, col, x});
                            }

                        if (!cuts.empty()) {
                            for (const Elimination& e : cuts)
                                cand.remove(e.row, e.col, e.digit);
                            record({Technique::XYChain, path, {x}, UnitKind::Row, -1, cuts});
                            return true;
                        }
                    }

                    queue.push_back({next, carry, std::move(path)});
                }
            }
        }
    }

    return false;
}

bool LogicSolver::eliminateOnce() {
    if (pointingClaiming()) return true;
    if (nakedSets(2)) return true;
    if (hiddenSets(2)) return true;
    if (nakedSets(3)) return true;
    if (hiddenSets(3)) return true;
    if (xWing()) return true;
    if (xyWing()) return true;
    if (xyChains()) return true;
    return false;
}

bool LogicSolver::run() {
    while (!solved()) {
        if (placeSingle()) continue;
        if (eliminateOnce()) continue;
        return false;
    }
    return true;
}

}
