using System.Collections.ObjectModel;

using Tupista.Interop;
using Tupista.Storage;

namespace Tupista.ViewModels;

/// <summary>
/// Everything the main window can do, with no reference to any UI framework.
///
/// The window binds to this and forwards keystrokes; all the decisions live
/// here, which is what makes them testable and what lets an iOS view reuse the
/// lot unchanged.
/// </summary>
public sealed class MainViewModel : ObservableObject
{
    private readonly BoardRepository _repository;

    private CellViewModel _selected;
    private string _status = "Type digits to enter a puzzle, then press Play.";
    private string _hintText = string.Empty;
    private string _boardName = "Untitled";
    private bool _isEditingGivens = true;
    private bool _showCandidates;
    private long _currentBoardId;
    private AppTheme _theme;

    // The hint currently on screen, and how much of it has been revealed.
    // Pressing Hint again escalates rather than fetching a new one — the rule
    // that the full answer is never shown unprompted.
    private HintResult? _hint;
    private HintLevel _hintLevel = HintLevel.Vague;

    public MainViewModel(BoardRepository? repository = null)
    {
        _repository = repository ?? new BoardRepository();

        var cells = new List<CellViewModel>(81);
        for (var row = 0; row < 9; row++)
            for (var col = 0; col < 9; col++)
                cells.Add(new CellViewModel(row, col));
        Cells = new ReadOnlyCollection<CellViewModel>(cells);

        _selected = Cells[0];
        _selected.IsSelected = true;

        SolveCommand = new RelayCommand(Solve);
        HintCommand = new RelayCommand(NextHint);
        CheckCommand = new RelayCommand(Check);
        ClearCommand = new RelayCommand(Clear);
        NewCommand = new RelayCommand(NewBoard);
        SaveCommand = new RelayCommand(Save);
        LoadCommand = new RelayCommand<SavedBoard>(Load);
        DeleteCommand = new RelayCommand<SavedBoard>(Delete);
        TogglePlayCommand = new RelayCommand(() => IsEditingGivens = !IsEditingGivens);

        // Input arrives as commands too, so the view needs no event handlers:
        // clicking a cell binds to SelectCommand, and the window's KeyBindings
        // map keystrokes onto MoveCommand and EnterDigitCommand.
        //
        // Both take a string because that is what a XAML CommandParameter is.
        // Parsing it here keeps the markup free of type-conversion ceremony.
        SelectCommand = new RelayCommand<CellViewModel>(Select);
        MoveCommand = new RelayCommand<string>(Move);
        EnterDigitCommand = new RelayCommand<string>(text =>
        {
            if (int.TryParse(text, out var digit)) EnterDigit(digit);
        });

        // Preferences are read straight from storage rather than defaulted and
        // then overwritten, so the first frame is already correct — no flash of
        // the wrong theme while something catches up.
        _theme = Enum.TryParse<AppTheme>(_repository.GetSetting(ThemeSettingKey, nameof(AppTheme.System)),
                                        out var stored) ? stored : AppTheme.System;

        RefreshSavedBoards();
    }

    private const string ThemeSettingKey = "theme";

    public IReadOnlyList<CellViewModel> Cells { get; }

    /// <summary>The choices offered in Settings; also the ComboBox's items.</summary>
    public IReadOnlyList<AppTheme> ThemeOptions { get; } = Enum.GetValues<AppTheme>();

    /// <summary>
    /// Colour scheme, remembered between runs. The view binds its theme to
    /// this through a converter; nothing here knows what a brush is.
    /// </summary>
    public AppTheme Theme
    {
        get => _theme;
        set
        {
            if (!Set(ref _theme, value)) return;
            _repository.SetSetting(ThemeSettingKey, value.ToString());
        }
    }
    public ObservableCollection<SavedBoard> SavedBoards { get; } = [];

    public RelayCommand SolveCommand { get; }
    public RelayCommand HintCommand { get; }
    public RelayCommand CheckCommand { get; }
    public RelayCommand ClearCommand { get; }
    public RelayCommand NewCommand { get; }
    public RelayCommand SaveCommand { get; }
    public RelayCommand TogglePlayCommand { get; }
    public RelayCommand<SavedBoard> LoadCommand { get; }
    public RelayCommand<SavedBoard> DeleteCommand { get; }
    public RelayCommand<CellViewModel> SelectCommand { get; }
    public RelayCommand<string> MoveCommand { get; }
    public RelayCommand<string> EnterDigitCommand { get; }

    public string Status
    {
        get => _status;
        private set => Set(ref _status, value);
    }

    public string HintText
    {
        get => _hintText;
        private set => Set(ref _hintText, value);
    }

    public string BoardName
    {
        get => _boardName;
        set => Set(ref _boardName, value);
    }

    /// <summary>
    /// True while typing the puzzle itself; false once playing.
    ///
    /// The distinction is not cosmetic. Digits entered as givens are locked
    /// afterwards, so an accidental keystroke cannot silently corrupt the
    /// puzzle you are trying to solve.
    /// </summary>
    public bool IsEditingGivens
    {
        get => _isEditingGivens;
        set
        {
            if (!Set(ref _isEditingGivens, value)) return;
            Raise(nameof(ModeLabel));
            LockGivens();
            Status = value
                ? "Editing the puzzle. Digits you type become givens."
                : "Playing. The puzzle digits are locked.";
        }
    }

    public string ModeLabel => IsEditingGivens ? "Play" : "Edit puzzle";

    public bool ShowCandidates
    {
        get => _showCandidates;
        set
        {
            if (Set(ref _showCandidates, value)) RefreshCandidates();
        }
    }

    public CellViewModel Selected
    {
        get => _selected;
        private set
        {
            if (ReferenceEquals(_selected, value)) return;
            _selected.IsSelected = false;
            _selected = value;
            _selected.IsSelected = true;
            Raise();
        }
    }

    // --- input --------------------------------------------------------------

    public void Select(CellViewModel cell) => Selected = cell;

    /// <summary>Arrow-key movement, named so a KeyBinding can pass a direction.</summary>
    private void Move(string direction)
    {
        switch (direction)
        {
            case "up": MoveSelection(-1, 0); break;
            case "down": MoveSelection(1, 0); break;
            case "left": MoveSelection(0, -1); break;
            case "right": MoveSelection(0, 1); break;
        }
    }

    public void MoveSelection(int rowDelta, int colDelta)
    {
        // Clamp rather than wrap: arrowing off the edge should stop, not
        // teleport you to the far side of the grid.
        var row = Math.Clamp(Selected.Row + rowDelta, 0, 8);
        var col = Math.Clamp(Selected.Col + colDelta, 0, 8);
        Selected = Cells[row * 9 + col];
    }

    /// <summary>Type a digit into the selected cell. 0 clears it.</summary>
    public void EnterDigit(int digit)
    {
        if (digit is < 0 or > 9) return;
        if (!IsEditingGivens && Selected.IsGiven)
        {
            Status = "That digit is part of the puzzle and cannot be changed.";
            return;
        }

        Selected.Value = digit;
        if (IsEditingGivens) Selected.IsGiven = digit != 0;

        // Any edit invalidates the hint on screen: it was reasoning about a
        // board that no longer exists.
        ClearHint();
        RefreshConflicts();
        RefreshCandidates();
    }

    // --- commands -----------------------------------------------------------

    private void Check()
    {
        var board = CurrentBoard();
        var conflicts = SudokuEngine.Validate(board);
        RefreshConflicts(conflicts);

        if (conflicts.Count > 0)
        {
            Status = $"Not legal: {Describe(conflicts[0])}.";
            return;
        }

        Status = SudokuEngine.CountSolutions(board) switch
        {
            0 => "No solution. Something entered here makes the puzzle impossible.",
            1 => "Exactly one solution — this is a proper puzzle.",
            _ => "More than one solution. There are not enough digits to pin it down.",
        };
    }

    private void Solve()
    {
        var board = CurrentBoard();
        if (!EnsureUsable(board)) return;

        var result = SudokuEngine.SolveLikeAHuman(board);
        for (var i = 0; i < 81; i++)
        {
            var digit = result.Grid[i] - '0';
            if (Cells[i].Value == 0 && digit > 0) Cells[i].Value = digit;
        }

        ClearHint();
        RefreshConflicts();
        RefreshCandidates();

        Status = result.Solved
            ? $"Solved using {result.TierName} (tier {result.Tier}) in {result.Placements.Count} steps."
            : $"Stuck after {result.Placements.Count} steps — this needs a technique the engine does not know.";
    }

    /// <summary>
    /// Show a hint, or reveal more of the one already showing.
    ///
    /// Escalating disclosure: the first press names where to look, the second
    /// explains the mechanism, the third gives the answer. The full step is
    /// never shown unasked.
    /// </summary>
    private void NextHint()
    {
        if (_hint is { Status: HintStatus.Ok } && _hintLevel != HintLevel.Full)
        {
            _hintLevel = _hintLevel == HintLevel.Vague ? HintLevel.Mechanism : HintLevel.Full;
            HintText = _hint.TextFor(_hintLevel);
            if (_hintLevel == HintLevel.Full) ApplyHintHighlights(_hint);
            return;
        }

        var board = CurrentBoard();
        _hint = SudokuEngine.NextHint(board);
        _hintLevel = HintLevel.Vague;
        ClearHighlights();

        HintText = _hint.TextFor(HintLevel.Vague);
        Status = _hint.Status switch
        {
            HintStatus.Ok => "Press Hint again for more detail.",
            HintStatus.Solved => "Nothing left to do.",
            HintStatus.BoardInvalid => "Fix the highlighted digits first.",
            HintStatus.NotUnique => "This grid has no single answer, so a hint would be a guess.",
            _ => "Beyond the engine's techniques.",
        };

        if (_hint.Status == HintStatus.BoardInvalid) RefreshConflicts(_hint.Conflicts);
    }

    private void Clear()
    {
        foreach (var cell in Cells)
        {
            if (cell.IsGiven && !IsEditingGivens) continue;   // keep the puzzle, drop the work
            cell.Value = 0;
            if (IsEditingGivens) cell.IsGiven = false;
        }
        ClearHint();
        RefreshConflicts();
        RefreshCandidates();
        Status = IsEditingGivens ? "Board cleared." : "Your entries were cleared.";
    }

    private void NewBoard()
    {
        _currentBoardId = 0;
        BoardName = "Untitled";
        IsEditingGivens = true;
        foreach (var cell in Cells)
        {
            cell.Value = 0;
            cell.IsGiven = false;
        }
        ClearHint();
        RefreshConflicts();
        RefreshCandidates();
        Status = "New puzzle. Type the givens, then press Play.";
    }

    private void Save()
    {
        var board = SavedBoard.NewNamed(
            string.IsNullOrWhiteSpace(BoardName) ? "Untitled" : BoardName.Trim(),
            GivensBoard(), CurrentBoard(), Marks()) with { Id = _currentBoardId };

        var saved = _repository.Save(board);
        _currentBoardId = saved.Id;
        RefreshSavedBoards();
        Status = $"Saved \"{saved.Name}\".";
    }

    private void Load(SavedBoard board)
    {
        for (var i = 0; i < 81; i++)
        {
            var given = board.Givens[i] - '0';
            var current = board.Current[i] - '0';
            Cells[i].IsGiven = given > 0;
            Cells[i].Value = current > 0 ? current : 0;
        }

        _currentBoardId = board.Id;
        BoardName = board.Name;
        _isEditingGivens = false;
        Raise(nameof(IsEditingGivens));
        Raise(nameof(ModeLabel));

        ClearHint();
        RefreshConflicts();
        RefreshCandidates();
        Status = $"Loaded \"{board.Name}\".";
    }

    private void Delete(SavedBoard board)
    {
        _repository.Delete(board.Id);
        if (_currentBoardId == board.Id) _currentBoardId = 0;
        RefreshSavedBoards();
        Status = $"Deleted \"{board.Name}\".";
    }

    private void RefreshSavedBoards()
    {
        SavedBoards.Clear();
        foreach (var board in _repository.LoadAll()) SavedBoards.Add(board);
    }

    // --- board state --------------------------------------------------------

    /// <summary>The grid as the engine wants it: 81 characters, '0' for empty.</summary>
    public string CurrentBoard() => string.Create(81, Cells, (span, cells) =>
    {
        for (var i = 0; i < 81; i++) span[i] = (char)('0' + cells[i].Value);
    });

    /// <summary>Only the puzzle's own digits, for saving the starting position.</summary>
    private string GivensBoard() => string.Create(81, Cells, (span, cells) =>
    {
        for (var i = 0; i < 81; i++)
            span[i] = cells[i].IsGiven ? (char)('0' + cells[i].Value) : '0';
    });

    // Pencil marks are stored and round-tripped but not yet editable in the UI;
    // the audit feature is what will drive them.
    private static ushort[] Marks() => new ushort[81];

    private void LockGivens()
    {
        if (IsEditingGivens) return;
        foreach (var cell in Cells) cell.IsGiven = cell.Value != 0;
    }

    private bool EnsureUsable(string board)
    {
        var conflicts = SudokuEngine.Validate(board);
        if (conflicts.Count > 0)
        {
            RefreshConflicts(conflicts);
            Status = $"Fix the board first: {Describe(conflicts[0])}.";
            return false;
        }

        // Rule 3 of the hint design, applied to solving too: a grid without
        // exactly one answer cannot be solved honestly.
        var solutions = SudokuEngine.CountSolutions(board);
        if (solutions == 1) return true;

        Status = solutions == 0
            ? "This puzzle has no solution."
            : "This puzzle has more than one solution — add more digits first.";
        return false;
    }

    private static string Describe(Conflict conflict)
    {
        var unit = conflict.Unit switch
        {
            UnitKind.Row => "row",
            UnitKind.Col => "column",
            _ => "box",
        };
        return $"{conflict.Value} appears twice in {unit} {conflict.UnitIndex + 1}";
    }

    // --- visual state -------------------------------------------------------

    private void RefreshConflicts(IReadOnlyList<Conflict>? conflicts = null)
    {
        conflicts ??= SudokuEngine.Validate(CurrentBoard());

        foreach (var cell in Cells) cell.IsConflicted = false;
        foreach (var conflict in conflicts)
            foreach (var cell in conflict.Cells)
                Cells[cell.Row * 9 + cell.Col].IsConflicted = true;
    }

    private void RefreshCandidates()
    {
        if (!ShowCandidates)
        {
            foreach (var cell in Cells) cell.Candidates = string.Empty;
            return;
        }

        // Candidates always come from the engine, computed from placed digits.
        // The UI never invents them, so what is shown is exactly what the hint
        // engine is reasoning about.
        var masks = SudokuEngine.Candidates(CurrentBoard());
        for (var i = 0; i < 81; i++)
        {
            var digits = new List<char>();
            for (var digit = 1; digit <= 9; digit++)
                if ((masks[i] & (1 << digit)) != 0) digits.Add((char)('0' + digit));
            Cells[i].Candidates = new string(digits.ToArray());
        }
    }

    private void ApplyHintHighlights(HintResult hint)
    {
        ClearHighlights();

        foreach (var step in hint.Steps)
        {
            foreach (var cell in step.Pattern)
                Cells[cell.Row * 9 + cell.Col].Highlight = CellHighlight.Pattern;
            foreach (var cut in step.Eliminations)
            {
                var target = Cells[cut.Row * 9 + cut.Col];
                if (target.Highlight == CellHighlight.None)
                    target.Highlight = CellHighlight.Eliminated;
            }
        }

        if (hint.Placement is { } placement)
            Cells[placement.Row * 9 + placement.Col].Highlight = CellHighlight.Placement;
    }

    private void ClearHighlights()
    {
        foreach (var cell in Cells) cell.Highlight = CellHighlight.None;
    }

    private void ClearHint()
    {
        _hint = null;
        _hintLevel = HintLevel.Vague;
        HintText = string.Empty;
        ClearHighlights();
    }
}
