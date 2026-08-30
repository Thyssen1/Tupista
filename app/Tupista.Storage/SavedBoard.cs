namespace Tupista.Storage;

/// <summary>
/// A puzzle as saved by the player.
///
/// Two grids, not one, and the distinction matters: <see cref="Givens"/> is the
/// puzzle as it was first entered and never changes, while
/// <see cref="Current"/> is how far they have got. Keeping both means "restart
/// this puzzle" is possible, and the UI can still tell which digits are the
/// player's own after a reload.
/// </summary>
/// <param name="Id">Database key; 0 for a board that has never been saved.</param>
/// <param name="Marks">81 pencil-mark bitmasks, row-major. Bit n = digit n.</param>
public sealed record SavedBoard(
    long Id,
    string Name,
    string Givens,
    string Current,
    ushort[] Marks,
    DateTime CreatedUtc,
    DateTime UpdatedUtc)
{
    public const int CellCount = 81;

    public static SavedBoard NewNamed(string name, string givens, string current, ushort[] marks)
        => new(0, name, givens, current, marks, DateTime.UtcNow, DateTime.UtcNow);

    /// <summary>How many digits are filled in, for the "recent puzzles" list.</summary>
    public int FilledCount => Current.Count(c => c is >= '1' and <= '9');
}
