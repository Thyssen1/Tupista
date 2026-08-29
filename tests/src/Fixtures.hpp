#pragma once

#include <string_view>

// Shared puzzle data for the test files.
//
// inline constexpr is the header-constant idiom: constexpr = known at compile
// time, inline = one shared entity even though several .cpp files include this.
// std::string_view rather than std::string because string_view can be constexpr
// (no allocation), and Board::fromString takes a string_view anyway.
namespace fixtures {

// Book puzzle, uniquely solvable.
inline constexpr std::string_view kPuzzle =
    "029000610050017000001090005060940081000786304000120006008002000300801062000409000";

inline constexpr std::string_view kPuzzleSolved =
    "829354617654217839731698425567943281192786354483125796978562143345871962216439578";

inline constexpr std::string_view kEmpty =
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000";

// ---------------------------------------------------------------------------
// Regression set: THE CONTRACT for the technique ladder.
//
// Every expected value below was produced by running Christian's original
// verified hint_engine.cpp unmodified over the same puzzle. If a change to the
// engine breaks one of these, the change altered engine behaviour — which may
// be fine, but it has to be a decision, not an accident.
// ---------------------------------------------------------------------------
struct Regression {
    std::string_view name;
    std::string_view puzzle;
    bool solves;              // does the human ladder finish?
    int tier;                 // hardest tier used (meaningful even when stuck)
    std::string_view tierName;
    int placements;           // how many digits the ladder placed
    std::string_view finalGrid;  // solved grid, or the partial grid at the stall
};

inline constexpr Regression kRegressions[] = {
    {"book puzzle, pairs",
     "029000610050017000001090005060940081000786304000120006008002000300801062000409000",
     true, 3, "naked/hidden pairs", 49,
     "829354617654217839731698425567943281192786354483125796978562143345871962216439578"},

    {"mid-solve app puzzle, triples",
     "048601000100007000650900010010000209800090001294165008563819427081500006000006185",
     true, 4, "naked/hidden triples", 41,
     "748621593139457862652938714316784259875293641294165378563819427481572936927346185"},

    {"book puzzle, no raw singles at the start",
     "000050801904218760000000000002070010090600000047120300500043900400000000700000040",
     true, 3, "naked/hidden pairs", 54,
     "273456891954218763681397524362875419195634278847129356516743982429581637738962145"},

    {"app puzzle, needs an XY-Chain",
     "016000240450007061028641530005410782201870453874500196642700015100000674507164320",
     true, 7, "XY-Chain", 28,
     "316958247459327861728641539935416782261879453874532196642783915183295674597164328"},

    // AI Escargot is beyond the ladder. Reporting "stuck" is the CORRECT
    // answer here, not a bug: the puzzle needs techniques we do not implement.
    {"AI Escargot, beyond the engine",
     "100007090030020008009600500005300900010080002600004000300000010040000007007000300",
     false, 1, "singles", 1,
     "100007090030020008009600500005300900010080002600004000300000010041000007007000300"},
};

}
