namespace Tupista.Interop;

// Managed mirrors of the engine's vocabulary. Everything above this layer works
// with these, never with the flat interop structs.
//
// The numeric values must match the enums in tupista.h. They are checked
// against the native side by SudokuEngine's startup verification.

public enum UnitKind
{
    Row = 0,
    Col = 1,
    Box = 2,
}

public enum Technique
{
    NakedSingle = 0,
    HiddenSingle = 1,
    Pointing = 2,
    Claiming = 3,
    NakedPair = 4,
    HiddenPair = 5,
    NakedTriple = 6,
    HiddenTriple = 7,
    XWing = 8,
    XYWing = 9,
    XYChain = 10,
}

public enum HintStatus
{
    /// <summary>A next step is available.</summary>
    Ok = 0,
    /// <summary>Nothing left to do.</summary>
    Solved = 1,
    /// <summary>Duplicate digits on the board; see the conflicts.</summary>
    BoardInvalid = 2,
    /// <summary>No solution, or more than one — any hint would be guesswork.</summary>
    NotUnique = 3,
    /// <summary>Legal and unique, but beyond the technique ladder.</summary>
    Stuck = 4,
}

/// <summary>How much of a hint to reveal. Never jump straight to Full.</summary>
public enum HintLevel
{
    Vague = 0,
    Mechanism = 1,
    Full = 2,
}

public readonly record struct Cell(int Row, int Col)
{
    /// <summary>"R4C7" — 1-based, the way puzzle books write it.</summary>
    public override string ToString() => $"R{Row + 1}C{Col + 1}";
}

public readonly record struct Elimination(int Row, int Col, int Digit)
{
    public Cell Cell => new(Row, Col);
}

/// <summary>A digit appearing more than once in one row, column or box.</summary>
public sealed record Conflict(UnitKind Unit, int UnitIndex, int Value, IReadOnlyList<Cell> Cells);

/// <summary>A digit placed, and the reason it was justified.</summary>
public sealed record Placement(int Row, int Col, int Value, Technique Technique,
    UnitKind Unit, int UnitIndex)
{
    public Cell Cell => new(Row, Col);
}

/// <summary>
/// One deduction in a hint chain: the pattern that was spotted, and what it
/// ruled out. The pattern matters as much as the eliminations — it is what
/// lets the UI highlight why the step is believable.
/// </summary>
public sealed record HintStep(Technique Technique, int Tier, UnitKind Unit, int UnitIndex,
    IReadOnlyList<int> Digits, IReadOnlyList<Cell> Pattern,
    IReadOnlyList<Elimination> Eliminations);

/// <summary>
/// One next step for the player, together with the chain of eliminations that
/// makes it true from what they can currently see. An empty chain is the good
/// case: the step needs no preparation at all.
/// </summary>
public sealed record HintResult(HintStatus Status, int Tier, Placement? Placement,
    IReadOnlyList<HintStep> Steps, IReadOnlyList<Conflict> Conflicts,
    string Vague, string Mechanism, string Full)
{
    public string TextFor(HintLevel level) => level switch
    {
        HintLevel.Vague => Vague,
        HintLevel.Mechanism => Mechanism,
        _ => Full,
    };
}

/// <summary>The full technique-ladder solve, with its narrative.</summary>
public sealed record SolveResult(bool Solved, int Tier, string TierName, string Grid,
    IReadOnlyList<Placement> Placements);

/// <summary>
/// Pencil marks in one cell that disagree with the board: digits still possible
/// but not written down, and digits written down that are no longer possible.
/// </summary>
public sealed record MarkAudit(Cell Cell, IReadOnlyList<int> Missing, IReadOnlyList<int> Stale);
