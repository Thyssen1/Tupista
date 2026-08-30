namespace Tupista.ViewModels;

/// <summary>How a cell is being called out right now. The view maps these to colours.</summary>
public enum CellHighlight
{
    None,
    /// <summary>The cell the current hint says to fill in.</summary>
    Placement,
    /// <summary>Part of the pattern that justifies the hint.</summary>
    Pattern,
    /// <summary>A candidate is being ruled out here by the hint.</summary>
    Eliminated,
}

/// <summary>
/// One square of the grid.
///
/// There are 81 of these and they live for the lifetime of the window — the
/// view binds to them once and they mutate in place. Recreating the collection
/// on every change would work but would throw away focus and selection each
/// time, which makes keyboard entry unusable.
/// </summary>
public sealed class CellViewModel(int row, int col) : ObservableObject
{
    private int _value;
    private bool _isGiven;
    private bool _isSelected;
    private bool _isConflicted;
    private CellHighlight _highlight;
    private string _candidates = string.Empty;

    public int Row { get; } = row;
    public int Col { get; } = col;
    public int Index { get; } = row * 9 + col;

    /// <summary>0 when empty, otherwise 1-9.</summary>
    public int Value
    {
        get => _value;
        set
        {
            if (Set(ref _value, value)) Raise(nameof(Display));
        }
    }

    /// <summary>Part of the original puzzle, so the player cannot overwrite it.</summary>
    public bool IsGiven
    {
        get => _isGiven;
        set => Set(ref _isGiven, value);
    }

    public bool IsSelected
    {
        get => _isSelected;
        set => Set(ref _isSelected, value);
    }

    /// <summary>This digit duplicates another in the same row, column or box.</summary>
    public bool IsConflicted
    {
        get => _isConflicted;
        set => Set(ref _isConflicted, value);
    }

    public CellHighlight Highlight
    {
        get => _highlight;
        set
        {
            if (!Set(ref _highlight, value)) return;
            Raise(nameof(IsHintPlacement));
            Raise(nameof(IsHintPattern));
            Raise(nameof(IsHintEliminated));
        }
    }

    // The enum split into three flags. A view can bind a style class straight
    // to a bool, which saves writing a converter and keeps the XAML readable.
    public bool IsHintPlacement => Highlight == CellHighlight.Placement;
    public bool IsHintPattern => Highlight == CellHighlight.Pattern;
    public bool IsHintEliminated => Highlight == CellHighlight.Eliminated;

    /// <summary>Small digits shown in an empty cell when candidates are on.</summary>
    public string Candidates
    {
        get => _candidates;
        set => Set(ref _candidates, value);
    }

    /// <summary>What the view actually prints: empty rather than a literal "0".</summary>
    public string Display => Value == 0 ? string.Empty : Value.ToString();

    // The two 3x3 boundaries. Exposed as data so the view can draw thicker
    // borders there instead of hard-coding which cells are special.
    public bool IsBoxRightEdge => Col is 2 or 5;
    public bool IsBoxBottomEdge => Row is 2 or 5;
}
