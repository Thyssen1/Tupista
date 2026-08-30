/* Tupista C ABI — the boundary between the C++ engine and any host language.
 *
 * WHY A C ABI AT ALL: C++ has no stable binary interface. Name mangling, class
 * layout and exception handling all differ between compilers and even between
 * versions of the same compiler. C does have a stable ABI, so every language
 * that can call a shared library can call this header — .NET through P/Invoke,
 * Swift directly.
 *
 * RULES THIS HEADER FOLLOWS
 *  - extern "C": no name mangling, no overloading.
 *  - Plain fixed-width types and flat structs only. No std::string, no
 *    std::vector, no classes, nothing that needs a C++ runtime to interpret.
 *  - No exceptions escape. Every entry point returns an error code instead.
 *  - Caller-owned buffers: you pass memory in, we fill it. Where a result is
 *    genuinely variable-sized (a hint), you get an opaque handle to free.
 *
 * BOARDS ARE STRINGS. Every function takes the grid as 81 characters,
 * row-major, '0' or '.' for empty. Keeping the wire format this dumb means the
 * host never has to mirror the engine's internal layout.
 *
 * INDEXING. Rows and columns are 0-based here; presenting "R1C1" is the UI's
 * job. Digits are 1-9. Digit sets are bitmasks with bit n meaning digit n, so
 * bit 0 is always clear.
 */
#ifndef TUPISTA_H
#define TUPISTA_H

#include <stdint.h>

#if defined(_WIN32) && defined(TUPISTA_SHARED)
#if defined(TUPISTA_EXPORTS)
#define TUPISTA_API __declspec(dllexport)
#else
#define TUPISTA_API __declspec(dllimport)
#endif
#else
#define TUPISTA_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- results -*/

enum {
    TUPISTA_OK = 0,
    TUPISTA_ERR_BAD_BOARD = -1, /* not 81 chars, or an illegal character */
    TUPISTA_ERR_NULL_ARG = -2,
    TUPISTA_ERR_BAD_INDEX = -3
};

/* Unit kinds, matching sudoku::UnitKind. */
enum { TUPISTA_UNIT_ROW = 0, TUPISTA_UNIT_COL = 1, TUPISTA_UNIT_BOX = 2 };

/* Techniques, matching sudoku::Technique. */
enum {
    TUPISTA_NAKED_SINGLE = 0,
    TUPISTA_HIDDEN_SINGLE = 1,
    TUPISTA_POINTING = 2,
    TUPISTA_CLAIMING = 3,
    TUPISTA_NAKED_PAIR = 4,
    TUPISTA_HIDDEN_PAIR = 5,
    TUPISTA_NAKED_TRIPLE = 6,
    TUPISTA_HIDDEN_TRIPLE = 7,
    TUPISTA_XWING = 8,
    TUPISTA_XYWING = 9,
    TUPISTA_XYCHAIN = 10
};

/* Hint status, matching sudoku::HintStatus. */
enum {
    TUPISTA_HINT_OK = 0,
    TUPISTA_HINT_SOLVED = 1,
    TUPISTA_HINT_BOARD_INVALID = 2,
    TUPISTA_HINT_NOT_UNIQUE = 3,
    TUPISTA_HINT_STUCK = 4
};

/* Disclosure levels, matching sudoku::HintLevel. */
enum { TUPISTA_LEVEL_VAGUE = 0, TUPISTA_LEVEL_MECHANISM = 1, TUPISTA_LEVEL_FULL = 2 };

/* ---------------------------------------------------------------- structs -*/

typedef struct {
    int32_t row;
    int32_t col;
} TupistaCell;

typedef struct {
    int32_t row;
    int32_t col;
    int32_t digit;
} TupistaElimination;

/* A duplicated digit in one unit. A unit holds nine cells, so the offenders
 * always fit inline and the host never needs a second call. */
typedef struct {
    int32_t unit;      /* TUPISTA_UNIT_* */
    int32_t unitIndex; /* 0-8 */
    int32_t value;     /* the duplicated digit */
    int32_t cellCount;
    TupistaCell cells[9];
} TupistaConflict;

typedef struct {
    int32_t row;
    int32_t col;
    int32_t value;
    int32_t technique; /* TUPISTA_NAKED_SINGLE or TUPISTA_HIDDEN_SINGLE */
    int32_t unit;      /* for a hidden single: the unit it was hidden in */
    int32_t unitIndex; /* -1 for a naked single */
} TupistaPlacement;

/* One deduction in a hint chain. Cells and eliminations vary in size, so they
 * come from tupista_hint_step_cells / tupista_hint_step_eliminations. */
typedef struct {
    int32_t technique;
    int32_t tier;
    int32_t unit;
    int32_t unitIndex; /* -1 when the pattern spans units */
    int32_t patternCount;
    int32_t eliminationCount;
    int32_t digitCount;
    int32_t digits[9];
} TupistaStep;

/* Pencil marks that disagree with the board, as bitmasks. */
typedef struct {
    int32_t row;
    int32_t col;
    uint16_t missing; /* possible here but not pencilled in */
    uint16_t stale;   /* pencilled in but no longer possible */
} TupistaMarkAudit;

/* --------------------------------------------------------------- one-shots -*/

TUPISTA_API int32_t tupista_abi_version(void);

/* Is this string a parseable grid? TUPISTA_OK or TUPISTA_ERR_BAD_BOARD. */
TUPISTA_API int32_t tupista_check_board(const char* board81);

/* Duplicate digits in rows, columns and boxes.
 * Returns the TOTAL number of conflicts, which may exceed `capacity`; only the
 * first `capacity` are written. Pass capacity 0 to ask for the count first. */
TUPISTA_API int32_t tupista_validate(const char* board81, TupistaConflict* out,
                                     int32_t capacity);

/* Number of solutions, counted no further than `cap`.
 * With cap 2: 0 = unsolvable or illegal, 1 = proper puzzle, 2 = ambiguous. */
TUPISTA_API int32_t tupista_count_solutions(const char* board81, int32_t cap);

/* Brute-force solution. Writes 81 chars plus a NUL into `solution82`.
 * Returns 1 when solved, 0 when there is no solution. */
TUPISTA_API int32_t tupista_solve(const char* board81, char* solution82);

/* Candidates for every cell as 81 bitmasks, row-major. Filled cells get 0. */
TUPISTA_API int32_t tupista_candidates(const char* board81, uint16_t* out81);

/* Compare pencil marks against reality. `marks81` is 81 bitmasks, row-major.
 * Returns the TOTAL number of cells with a discrepancy (see tupista_validate
 * for how capacity works). */
TUPISTA_API int32_t tupista_audit_marks(const char* board81, const uint16_t* marks81,
                                        TupistaMarkAudit* out, int32_t capacity);

/* ------------------------------------------------------------- full solve -*/
/* Opaque handle: a solve carries a variable-length list of placements, so the
 * host holds a handle and reads through it, then frees it exactly once. */

typedef struct TupistaSolve TupistaSolve;

TUPISTA_API TupistaSolve* tupista_solve_human(const char* board81);
TUPISTA_API void tupista_solve_free(TupistaSolve* solve);

TUPISTA_API int32_t tupista_solve_is_solved(const TupistaSolve* solve);
TUPISTA_API int32_t tupista_solve_tier(const TupistaSolve* solve);
/* Tier name ("naked/hidden pairs"). Returns the length that WOULD be written,
 * excluding the NUL; the buffer is always NUL-terminated when capacity > 0. */
TUPISTA_API int32_t tupista_solve_tier_name(const TupistaSolve* solve, char* out,
                                            int32_t capacity);
TUPISTA_API int32_t tupista_solve_grid(const TupistaSolve* solve, char* out82);
TUPISTA_API int32_t tupista_solve_placement_count(const TupistaSolve* solve);
TUPISTA_API int32_t tupista_solve_placement(const TupistaSolve* solve, int32_t index,
                                            TupistaPlacement* out);

/* ------------------------------------------------------------------ hints -*/

typedef struct TupistaHint TupistaHint;

/* Never returns NULL for a parseable board — an unhintable position still
 * comes back as a handle whose status explains why. NULL means bad input. */
TUPISTA_API TupistaHint* tupista_hint(const char* board81);
TUPISTA_API void tupista_hint_free(TupistaHint* hint);

TUPISTA_API int32_t tupista_hint_status(const TupistaHint* hint);
TUPISTA_API int32_t tupista_hint_tier(const TupistaHint* hint);
TUPISTA_API int32_t tupista_hint_has_placement(const TupistaHint* hint);
TUPISTA_API int32_t tupista_hint_placement(const TupistaHint* hint, TupistaPlacement* out);

/* Conflicts, when the status is TUPISTA_HINT_BOARD_INVALID. */
TUPISTA_API int32_t tupista_hint_conflicts(const TupistaHint* hint, TupistaConflict* out,
                                           int32_t capacity);

/* The enabling chain: the deductions that make the placement true starting
 * from what the player can see. Zero steps means it is directly visible. */
TUPISTA_API int32_t tupista_hint_step_count(const TupistaHint* hint);
TUPISTA_API int32_t tupista_hint_step(const TupistaHint* hint, int32_t index,
                                      TupistaStep* out);
TUPISTA_API int32_t tupista_hint_step_cells(const TupistaHint* hint, int32_t index,
                                            TupistaCell* out, int32_t capacity);
TUPISTA_API int32_t tupista_hint_step_eliminations(const TupistaHint* hint, int32_t index,
                                                   TupistaElimination* out,
                                                   int32_t capacity);

/* Prose at one of the three disclosure levels. Same length convention as
 * tupista_solve_tier_name. */
TUPISTA_API int32_t tupista_hint_describe(const TupistaHint* hint, int32_t level, char* out,
                                          int32_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* TUPISTA_H */
