#pragma once

#include <string_view>

// Shared puzzle data for the test files. This grows in stage 4 into the full
// regression set (5 puzzles with their expected difficulty tiers).
//
// inline constexpr is exactly the header-constant idiom: constexpr = known at
// compile time, inline = one shared entity even though several .cpp files
// include this header. std::string_view rather than std::string because
// string_view can be constexpr (no allocation), and Board::fromString takes
// a string_view anyway.
namespace fixtures {

// Book puzzle, uniquely solvable.
inline constexpr std::string_view kPuzzle =
    "029000610050017000001090005060940081000786304000120006008002000300801062000409000";

inline constexpr std::string_view kPuzzleSolved =
    "829354617654217839731698425567943281192786354483125796978562143345871962216439578";

inline constexpr std::string_view kEmpty =
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000";

}