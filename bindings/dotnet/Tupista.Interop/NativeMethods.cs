using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Tupista.Interop;

/// <summary>
/// The raw C entry points from tupista.h, one C# declaration per C function.
///
/// Nothing outside this file should call these. They deal in byte buffers,
/// capacities and error codes; turning that into pleasant C# is
/// <see cref="SudokuEngine"/>'s job.
///
/// [LibraryImport] is the modern replacement for [DllImport]: a source
/// generator writes the marshalling code at compile time, so mistakes surface
/// as build errors rather than as memory corruption at runtime. It requires
/// the containing class and each method to be partial.
/// </summary>
internal static partial class NativeMethods
{
    /// <summary>
    /// Resolved as "tupista.dll" on Windows, "libtupista.dylib" on macOS and
    /// "libtupista.so" on Linux — .NET adds the prefix and extension per
    /// platform, which is why the CMake build strips the "lib" prefix on
    /// Windows only.
    /// </summary>
    private const string Library = "tupista";

    [LibraryImport(Library)]
    internal static partial int tupista_abi_version();

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_check_board(string board81);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_validate(string board81,
        [Out] TupistaConflict[]? buffer, int capacity);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_count_solutions(string board81, int cap);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_solve(string board81, [Out] byte[] solution82);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_candidates(string board81, [Out] ushort[] out81);

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int tupista_audit_marks(string board81, ushort[] marks81,
        [Out] TupistaMarkAudit[]? buffer, int capacity);

    // --- full solve ---------------------------------------------------------

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial SolveHandle tupista_solve_human(string board81);

    // Freeing takes a raw pointer, not the SafeHandle: SafeHandle.ReleaseHandle
    // runs during finalization, when the wrapper object is already being torn
    // down and must not be marshalled again.
    [LibraryImport(Library)]
    internal static partial void tupista_solve_free(IntPtr solve);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_is_solved(SolveHandle solve);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_tier(SolveHandle solve);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_tier_name(SolveHandle solve,
        [Out] byte[]? buffer, int capacity);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_grid(SolveHandle solve, [Out] byte[] out82);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_placement_count(SolveHandle solve);

    [LibraryImport(Library)]
    internal static partial int tupista_solve_placement(SolveHandle solve, int index,
        out TupistaPlacement placement);

    // --- hints --------------------------------------------------------------

    [LibraryImport(Library, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial HintHandle tupista_hint(string board81);

    [LibraryImport(Library)]
    internal static partial void tupista_hint_free(IntPtr hint);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_status(HintHandle hint);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_tier(HintHandle hint);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_has_placement(HintHandle hint);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_placement(HintHandle hint,
        out TupistaPlacement placement);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_conflicts(HintHandle hint,
        [Out] TupistaConflict[]? buffer, int capacity);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_step_count(HintHandle hint);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_step(HintHandle hint, int index,
        out TupistaStep step);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_step_cells(HintHandle hint, int index,
        [Out] TupistaCell[]? buffer, int capacity);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_step_eliminations(HintHandle hint, int index,
        [Out] TupistaElimination[]? buffer, int capacity);

    [LibraryImport(Library)]
    internal static partial int tupista_hint_describe(HintHandle hint, int level,
        [Out] byte[]? buffer, int capacity);
}

// ---------------------------------------------------------------------------
// Structs, laid out to match tupista.h exactly.
//
// LayoutKind.Sequential tells the runtime to keep the fields in declaration
// order rather than reordering them for packing. Every field is a fixed-width
// type for the same reason the C header uses int32_t: "int" must mean the same
// number of bytes on both sides of the boundary.
// ---------------------------------------------------------------------------

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaCell
{
    public int Row;
    public int Col;
}

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaElimination
{
    public int Row;
    public int Col;
    public int Digit;
}

/// <summary>
/// C's <c>TupistaCell cells[9]</c> embedded inside a struct.
///
/// C# cannot use the <c>fixed</c> keyword for arrays of custom structs, only
/// primitives. [InlineArray] is the .NET 8 answer: this declares nine
/// TupistaCells stored inline, indexable like an array.
/// </summary>
[InlineArray(9)]
internal struct CellArray9
{
    private TupistaCell _element0;
}

[InlineArray(9)]
internal struct IntArray9
{
    private int _element0;
}

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaConflict
{
    public int Unit;
    public int UnitIndex;
    public int Value;
    public int CellCount;
    public CellArray9 Cells;
}

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaPlacement
{
    public int Row;
    public int Col;
    public int Value;
    public int Technique;
    public int Unit;
    public int UnitIndex;
}

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaStep
{
    public int Technique;
    public int Tier;
    public int Unit;
    public int UnitIndex;
    public int PatternCount;
    public int EliminationCount;
    public int DigitCount;
    public IntArray9 Digits;
}

[StructLayout(LayoutKind.Sequential)]
internal struct TupistaMarkAudit
{
    public int Row;
    public int Col;
    public ushort Missing;
    public ushort Stale;
}
