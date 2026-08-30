namespace Tupista.ViewModels;

/// <summary>
/// One of the nine small digits shown inside a cell.
///
/// There are always nine of these per cell and they never move: digit n sits
/// at row (n-1)/3, column (n-1)%3 of a 3x3 mini-grid. That fixed position is
/// the whole point — you learn where to look for a 5 instead of reading a list
/// and counting. It is how paper Sudoku is annotated, and how every good
/// Sudoku app renders candidates.
///
/// They are shown or hidden rather than added and removed, so nothing reflows
/// as marks change.
/// </summary>
public sealed class CellNote(int digit) : ObservableObject
{
    private bool _shown;
    private bool _isUserMark;

    public int Digit { get; } = digit;
    public string Text { get; } = digit.ToString();

    public bool Shown
    {
        get => _shown;
        set => Set(ref _shown, value);
    }

    /// <summary>
    /// True when this is the player's own pencil mark, false when it is a
    /// candidate the engine worked out. The two are drawn differently: your
    /// notes are yours, and must never be mistaken for the engine's opinion.
    /// </summary>
    public bool IsUserMark
    {
        get => _isUserMark;
        set => Set(ref _isUserMark, value);
    }
}
