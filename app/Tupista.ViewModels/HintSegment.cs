namespace Tupista.ViewModels;

/// <summary>
/// One piece of a hint sentence: either ordinary words, or a reference to a
/// cell that the player can click to jump to.
///
/// The engine hands us prose ("Pointing on 7 at R1C1 and R3C1 ..."), and the
/// cell names in it are the most useful thing on screen — but only if you can
/// act on them. Splitting the sentence here, rather than in the view, keeps
/// the parsing testable and leaves the view with nothing to decide.
/// </summary>
public sealed class HintSegment
{
    private HintSegment(string text, int row, int col)
    {
        Text = text;
        Row = row;
        Col = col;
    }

    public string Text { get; }

    /// <summary>0-based board coordinates; both -1 for plain text.</summary>
    public int Row { get; }
    public int Col { get; }

    public bool IsCell => Row >= 0;

    public static HintSegment Words(string text) => new(text, -1, -1);

    /// <summary>From a 1-based "R4C7" as the engine writes it.</summary>
    public static HintSegment Cell(string text, int row1Based, int col1Based) =>
        new(text, row1Based - 1, col1Based - 1);
}

/// <summary>
/// One line of a hint, already split into segments.
///
/// Lines are kept apart rather than flattened because the full hint is a
/// numbered list, and the steps have to stay on their own lines. Within a
/// line the segments wrap like ordinary text.
/// </summary>
public sealed class HintLine(IReadOnlyList<HintSegment> segments)
{
    public IReadOnlyList<HintSegment> Segments { get; } = segments;
}