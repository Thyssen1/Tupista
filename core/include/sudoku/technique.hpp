#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "sudoku/candidates.hpp"
#include "sudoku/units.hpp"

namespace sudoku {

// The solving techniques the engine knows, in difficulty order.
// Difficulty is reported as a TIER (1-7); several techniques share a tier
// because they are about equally hard for a human to spot.
enum class Technique : std::uint8_t {
    NakedSingle,   // tier 1
    HiddenSingle,  // tier 1
    Pointing,      // tier 2
    Claiming,      // tier 2
    NakedPair,     // tier 3
    HiddenPair,    // tier 3
    NakedTriple,   // tier 4
    HiddenTriple,  // tier 4
    XWing,         // tier 5
    XYWing,        // tier 6
    XYChain,       // tier 7
};

inline constexpr int kMaxTier = 7;

int tierOf(Technique technique);

// "naked single", "pointing", "X-Wing", ...
std::string_view nameOf(Technique technique);

// The name of a whole tier: "singles", "pointing/claiming",
// "naked/hidden pairs", "naked/hidden triples", "X-Wing", "XY-Wing",
// "XY-Chain". Tier 0 is "none". This is what rateDifficulty reports.
std::string_view tierName(int tier);

// One candidate removed from one cell.
struct Elimination {
    int row = 0;
    int col = 0;
    int digit = 0;

    bool operator==(const Elimination&) const = default;
};

// One application of one technique: the pattern that was spotted, and what it
// let us rule out.
//
// This is deliberately descriptive rather than minimal. The hint UI has to be
// able to say "1 and 2 fit only two cells in column 6" and highlight exactly
// those cells — so the pattern that justifies the deduction is part of the
// record, not just its consequences.
struct Finding {
    Technique technique{};
    std::vector<CellRef> pattern;   // the cells forming the pattern
    std::vector<int> digits;        // the digits the pattern is about
    UnitKind unit{};                // the unit it was found in...
    int unitIndex = -1;             // ...or -1 when the pattern spans units
                                    // (X-Wing, XY-Wing, XY-Chain). For those,
                                    // `unit` still records the orientation the
                                    // pattern was read in.
    std::vector<Elimination> eliminations;

    // What each pattern cell held at the moment the pattern was spotted,
    // parallel to `pattern`.
    //
    // Needed because a pattern is not just "these cells" — it is "these cells
    // WITH these candidates". A naked pair on {3,5} is only a naked pair while
    // both cells are down to 3 and 5. Recording that lets the hint pruner work
    // out which earlier eliminations a pattern actually depended on, instead of
    // dragging the whole history along.
    // Filled in by the engine when the finding is recorded, so the technique
    // call sites leave it out of their initialiser lists; the explicit default
    // below is what keeps -Wmissing-field-initializers quiet about that.
    std::vector<DigitSet> patternDigits = {};
};

}
