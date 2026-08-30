using System.Text;

namespace Tupista.Interop;

/// <summary>
/// The engine, as C# sees it. This is the only type the rest of the
/// application uses; everything below it is buffers and error codes.
///
/// Boards are always 81 characters, row-major, '0' for empty — the same dumb
/// wire format the C ABI takes, so nothing here has to mirror the engine's
/// internal layout.
/// </summary>
public static class SudokuEngine
{
    /// <summary>The ABI revision this assembly was written against.</summary>
    private const int ExpectedAbiVersion = 1;

    public const int GridSize = 9;
    public const int CellCount = 81;

    private static int _verified;

    /// <summary>
    /// Confirms the native library is present and speaks the ABI we expect.
    ///
    /// Worth doing explicitly at startup rather than discovering it at the
    /// first hint: a stale tupista.dll left in an output folder is otherwise a
    /// baffling bug, because the calls still resolve and simply return
    /// nonsense. Failing here names the actual problem.
    /// </summary>
    public static void EnsureAvailable()
    {
        if (Interlocked.Exchange(ref _verified, 1) == 1) return;

        int version;
        try
        {
            version = NativeMethods.tupista_abi_version();
        }
        catch (DllNotFoundException ex)
        {
            Interlocked.Exchange(ref _verified, 0);
            throw new TupistaException(
                "The native library could not be loaded. Build the C++ core first " +
                "(cmake --build <builddir>), which writes it to artifacts/native.", ex);
        }

        if (version != ExpectedAbiVersion)
        {
            Interlocked.Exchange(ref _verified, 0);
            throw new TupistaException(
                $"Native library speaks ABI version {version}, this build expects " +
                $"{ExpectedAbiVersion}. The tupista library next to the app is out of date.");
        }
    }

    /// <summary>Is this a parseable 81-character grid?</summary>
    public static bool IsWellFormed(string board)
    {
        EnsureAvailable();
        return board.Length == CellCount && NativeMethods.tupista_check_board(board) == 0;
    }

    /// <summary>Duplicate digits in rows, columns and boxes. Empty means legal.</summary>
    public static IReadOnlyList<Conflict> Validate(string board)
    {
        EnsureAvailable();
        Require(board);

        // The two-call pattern the ABI uses throughout: ask for the count, then
        // allocate exactly enough and ask again. The native side never
        // allocates on our behalf, so it can never own memory we have to
        // remember to free.
        var count = NativeMethods.tupista_validate(board, null, 0);
        ThrowIfError(count);
        if (count == 0) return Array.Empty<Conflict>();

        var buffer = new TupistaConflict[count];
        NativeMethods.tupista_validate(board, buffer, count);

        var result = new List<Conflict>(count);
        foreach (var raw in buffer)
        {
            var cells = new List<Cell>(raw.CellCount);
            for (var i = 0; i < raw.CellCount; i++)
                cells.Add(new Cell(raw.Cells[i].Row, raw.Cells[i].Col));
            result.Add(new Conflict((UnitKind)raw.Unit, raw.UnitIndex, raw.Value, cells));
        }
        return result;
    }

    /// <summary>
    /// How many solutions, counted no further than <paramref name="cap"/>.
    /// With the default cap: 0 = unsolvable or illegal, 1 = a proper puzzle,
    /// 2 = ambiguous.
    /// </summary>
    public static int CountSolutions(string board, int cap = 2)
    {
        EnsureAvailable();
        Require(board);
        var count = NativeMethods.tupista_count_solutions(board, cap);
        ThrowIfError(count);
        return count;
    }

    /// <summary>The brute-force solution, or null when there is none.</summary>
    public static string? Solve(string board)
    {
        EnsureAvailable();
        Require(board);

        var buffer = new byte[CellCount + 1];   // room for the NUL the C side writes
        var status = NativeMethods.tupista_solve(board, buffer);
        ThrowIfError(status);
        return status == 1 ? Encoding.ASCII.GetString(buffer, 0, CellCount) : null;
    }

    /// <summary>
    /// Candidates per cell as bitmasks: bit n set means digit n is still
    /// possible. Filled cells get 0. Always computed from placed digits, never
    /// from the player's pencil marks.
    /// </summary>
    public static ushort[] Candidates(string board)
    {
        EnsureAvailable();
        Require(board);

        var masks = new ushort[CellCount];
        ThrowIfError(NativeMethods.tupista_candidates(board, masks));
        return masks;
    }

    /// <summary>Compare pencil marks against what the board actually allows.</summary>
    public static IReadOnlyList<MarkAudit> AuditMarks(string board, ushort[] marks)
    {
        EnsureAvailable();
        Require(board);
        ArgumentOutOfRangeException.ThrowIfNotEqual(marks.Length, CellCount);

        var count = NativeMethods.tupista_audit_marks(board, marks, null, 0);
        ThrowIfError(count);
        if (count == 0) return Array.Empty<MarkAudit>();

        var buffer = new TupistaMarkAudit[count];
        NativeMethods.tupista_audit_marks(board, marks, buffer, count);

        return buffer
            .Select(raw => new MarkAudit(new Cell(raw.Row, raw.Col),
                                         DigitsOf(raw.Missing), DigitsOf(raw.Stale)))
            .ToList();
    }

    /// <summary>Solve the way a person would, and report what it took.</summary>
    public static SolveResult SolveLikeAHuman(string board)
    {
        EnsureAvailable();
        Require(board);

        // "using" here is what makes the native handle safe: the solve is freed
        // when this method returns, however it returns.
        using var handle = NativeMethods.tupista_solve_human(board);
        if (handle.IsInvalid) throw new TupistaException("The engine rejected that board.");

        var placementCount = NativeMethods.tupista_solve_placement_count(handle);
        var placements = new List<Placement>(Math.Max(placementCount, 0));
        for (var i = 0; i < placementCount; i++)
        {
            NativeMethods.tupista_solve_placement(handle, i, out var raw);
            placements.Add(ToPlacement(raw));
        }

        var grid = new byte[CellCount + 1];
        NativeMethods.tupista_solve_grid(handle, grid);

        return new SolveResult(
            NativeMethods.tupista_solve_is_solved(handle) == 1,
            NativeMethods.tupista_solve_tier(handle),
            ReadString((buf, cap) => NativeMethods.tupista_solve_tier_name(handle, buf, cap)),
            Encoding.ASCII.GetString(grid, 0, CellCount),
            placements);
    }

    /// <summary>
    /// The single most important next step, with the chain of eliminations that
    /// justifies it. All three disclosure levels are fetched up front so the UI
    /// can escalate without another round trip.
    /// </summary>
    public static HintResult NextHint(string board)
    {
        EnsureAvailable();
        Require(board);

        using var handle = NativeMethods.tupista_hint(board);
        if (handle.IsInvalid) throw new TupistaException("The engine rejected that board.");

        var status = (HintStatus)NativeMethods.tupista_hint_status(handle);

        Placement? placement = null;
        if (NativeMethods.tupista_hint_has_placement(handle) == 1 &&
            NativeMethods.tupista_hint_placement(handle, out var rawPlacement) == 0)
            placement = ToPlacement(rawPlacement);

        var steps = new List<HintStep>();
        var stepCount = NativeMethods.tupista_hint_step_count(handle);
        for (var i = 0; i < stepCount; i++)
        {
            NativeMethods.tupista_hint_step(handle, i, out var raw);

            var digits = new List<int>(raw.DigitCount);
            for (var d = 0; d < raw.DigitCount; d++) digits.Add(raw.Digits[d]);

            var pattern = new TupistaCell[raw.PatternCount];
            NativeMethods.tupista_hint_step_cells(handle, i, pattern, raw.PatternCount);

            var cuts = new TupistaElimination[raw.EliminationCount];
            NativeMethods.tupista_hint_step_eliminations(handle, i, cuts, raw.EliminationCount);

            steps.Add(new HintStep(
                (Technique)raw.Technique, raw.Tier, (UnitKind)raw.Unit, raw.UnitIndex, digits,
                pattern.Select(c => new Cell(c.Row, c.Col)).ToList(),
                cuts.Select(c => new Elimination(c.Row, c.Col, c.Digit)).ToList()));
        }

        var conflicts = new List<Conflict>();
        var conflictCount = NativeMethods.tupista_hint_conflicts(handle, null, 0);
        if (conflictCount > 0)
        {
            var buffer = new TupistaConflict[conflictCount];
            NativeMethods.tupista_hint_conflicts(handle, buffer, conflictCount);
            foreach (var raw in buffer)
            {
                var cells = new List<Cell>(raw.CellCount);
                for (var i = 0; i < raw.CellCount; i++)
                    cells.Add(new Cell(raw.Cells[i].Row, raw.Cells[i].Col));
                conflicts.Add(new Conflict((UnitKind)raw.Unit, raw.UnitIndex, raw.Value, cells));
            }
        }

        return new HintResult(status, NativeMethods.tupista_hint_tier(handle), placement,
            steps, conflicts,
            Describe(handle, HintLevel.Vague),
            Describe(handle, HintLevel.Mechanism),
            Describe(handle, HintLevel.Full));
    }

    // --- plumbing -----------------------------------------------------------

    private static string Describe(HintHandle handle, HintLevel level) =>
        ReadString((buf, cap) => NativeMethods.tupista_hint_describe(handle, (int)level, buf, cap));

    /// <summary>
    /// The ABI's string convention: a call returns the length it WOULD write,
    /// so ask with no buffer, then allocate exactly that plus room for the NUL.
    /// </summary>
    private static string ReadString(Func<byte[]?, int, int> call)
    {
        var length = call(null, 0);
        if (length <= 0) return string.Empty;

        var buffer = new byte[length + 1];
        call(buffer, buffer.Length);
        return Encoding.UTF8.GetString(buffer, 0, length);
    }

    private static Placement ToPlacement(TupistaPlacement raw) =>
        new(raw.Row, raw.Col, raw.Value, (Technique)raw.Technique,
            (UnitKind)raw.Unit, raw.UnitIndex);

    private static List<int> DigitsOf(ushort mask)
    {
        var digits = new List<int>();
        for (var digit = 1; digit <= GridSize; digit++)
            if ((mask & (1 << digit)) != 0) digits.Add(digit);
        return digits;
    }

    private static void Require(string board)
    {
        if (board.Length != CellCount)
            throw new ArgumentException(
                $"A board must be exactly {CellCount} characters, got {board.Length}.",
                nameof(board));
    }

    /// <summary>Negative returns are the ABI's error codes; see tupista.h.</summary>
    private static void ThrowIfError(int code)
    {
        if (code >= 0) return;
        throw new TupistaException(code switch
        {
            -1 => "The engine could not parse that board.",
            -2 => "A required argument was null.",
            -3 => "An index was out of range.",
            _ => $"The engine reported error {code}.",
        });
    }
}

public sealed class TupistaException : Exception
{
    public TupistaException(string message) : base(message) { }
    public TupistaException(string message, Exception inner) : base(message, inner) { }
}
