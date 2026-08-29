#include "sudoku/technique.hpp"

namespace sudoku {

int tierOf(Technique technique) {
    switch (technique) {
        case Technique::NakedSingle:
        case Technique::HiddenSingle: return 1;
        case Technique::Pointing:
        case Technique::Claiming:     return 2;
        case Technique::NakedPair:
        case Technique::HiddenPair:   return 3;
        case Technique::NakedTriple:
        case Technique::HiddenTriple: return 4;
        case Technique::XWing:        return 5;
        case Technique::XYWing:       return 6;
        case Technique::XYChain:      return 7;
    }
    return 0;
}

std::string_view nameOf(Technique technique) {
    switch (technique) {
        case Technique::NakedSingle:  return "naked single";
        case Technique::HiddenSingle: return "hidden single";
        case Technique::Pointing:     return "pointing";
        case Technique::Claiming:     return "claiming";
        case Technique::NakedPair:    return "naked pair";
        case Technique::HiddenPair:   return "hidden pair";
        case Technique::NakedTriple:  return "naked triple";
        case Technique::HiddenTriple: return "hidden triple";
        case Technique::XWing:        return "X-Wing";
        case Technique::XYWing:       return "XY-Wing";
        case Technique::XYChain:      return "XY-Chain";
    }
    return "unknown";
}

std::string_view tierName(int tier) {
    switch (tier) {
        case 0: return "none";
        case 1: return "singles";
        case 2: return "pointing/claiming";
        case 3: return "naked/hidden pairs";
        case 4: return "naked/hidden triples";
        case 5: return "X-Wing";
        case 6: return "XY-Wing";
        case 7: return "XY-Chain";
    }
    return "unknown";
}

}
