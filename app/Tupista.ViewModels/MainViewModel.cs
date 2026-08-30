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
    private AppPalette _palette;
    private bool _isPencilMode;
    private bool _isSolved;
    private string _completionMessage = string.Empty;

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
        ResetCellCommand = new RelayCommand(ResetSelectedCell);
        TogglePencilCommand = new RelayCommand(() => IsPencilMode = !IsPencilMode);
        AuditMarksCommand = new RelayCommand(AuditMarks);

        // Shift+digit notes a digit without leaving placement mode — the same
        // key, one modifier, so a single note does not cost two mode switches.
        NoteDigitCommand = new RelayCommand<string>(text =>
        {
            if (int.TryParse(text, out var digit)) ToggleMark(digit);
        });
        EnterDigitCommand = new RelayCommand<string>(text =>
        {
            if (int.TryParse(text, out var digit)) EnterDigit(digit);
        });

        // Preferences are read straight from storage rather than defaulted and
        // then overwritten, so the first frame is already correct — no flash of
        // the wrong theme while something catches up.
        _theme = Enum.TryParse<AppTheme>(_repository.GetSetting(ThemeSettingKey, nameof(AppTheme.System)),
                                        out var storedTheme) ? storedTheme : AppTheme.System;
        _palette = Enum.TryParse<AppPalette>(_repository.GetSetting(PaletteSettingKey, nameof(AppPalette.InkVermilion)),
                                        out var storedPalette) ? storedPalette : AppPalette.InkVermilion;

        RefreshSavedBoards();
    }

    private const string ThemeSettingKey = "theme";
    private const string PaletteSettingKey = "palette";

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

    /// <summary>The choices offered in Settings; also the ComboBox's items.</summary>
    public IReadOnlyList<AppPalette> PaletteOptions { get; } = Enum.GetValues<AppPalette>();

    /// <summary>
    /// Colour scheme, remembered between runs and orthogonal to
    /// <see cref="Theme"/>. The view watches this and swaps its resources.
    /// </summary>
    public AppPalette Palette
    {
        get => _palette;
        set
        {
            if (!Set(ref _palette, value)) return;
            _repository.SetSetting(PaletteSettingKey, value.ToString());
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
    public RelayCommand ResetCellCommand { get; }
    public RelayCommand TogglePencilCommand { get; }
    public RelayCommand AuditMarksCommand { get; }
    public RelayCommand<string> NoteDigitCommand { get; }

    /// <summary>
    /// While on, typing a digit notes it instead of placing it.
    ///
    /// A mode rather than only a modifier because note-taking comes in bursts:
    /// you pencil a whole row at once. Shift+digit stays available for one-offs.
    /// </summary>
    public bool IsPencilMode
    {
        get => _isPencilMode;
        set
        {
            if (!Set(ref _isPencilMode, value)) return;
            Raise(nameof(PencilLabel));
            Status = value
                ? "Pencil mode: digits are noted, not placed."
                : "Placing digits.";
        }
    }

    public string PencilLabel => IsPencilMode ? "Pencil: on" : "Pencil: off";

    /// <summary>
    /// The grid is full and legal — the puzzle is finished.
    ///
    /// Nothing to press: a puzzle you have just completed should say so by
    /// itself. Making someone ask whether they have won is the sort of thing
    /// that makes an app feel unfinished.
    ///
    /// Full plus legal really is correct here. Every puzzle the app will solve
    /// or hint on has been checked for a unique solution, so there is only one
    /// way to fill it legally.
    /// </summary>
    public bool IsSolved
    {
        get => _isSolved;
        private set => Set(ref _isSolved, value);
    }

    public string CompletionMessage
    {
        get => _completionMessage;
        private set => Set(ref _completionMessage, value);
    }

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

        // In pencil mode a digit is a note, not a placement. Never while
        // entering the puzzle itself: givens are facts, not guesses.
        if (IsPencilMode && !IsEditingGivens && digit != 0)
        {
            ToggleMark(digit);
            return;
        }

        Selected.Value = digit;
        // Committing to a digit makes this cell's own notes moot. Notes in
        // OTHER cells are left alone deliberately — silently tidying them would
        // do the player's thinking for them and make the mark audit pointless.
        if (digit != 0) Selected.Marks = 0;
        if (IsEditingGivens) Selected.IsGiven = digit != 0;

        // Any edit invalidates the hint on screen: it was reasoning about a
        // board that no longer exists.
        BoardChanged();
    }

    /// <summary>
    /// Put the selected cell back to how it started: no digit, no pencil marks.
    ///
    /// Broader than typing 0, which only clears the digit. Once marks are
    /// editable this is the difference between "I got that number wrong" and
    /// "forget everything I thought about this cell".
    ///
    /// A given is left alone while playing, for the same reason it cannot be
    /// typed over: it is the puzzle, not your work.
    /// </summary>
    private void ResetSelectedCell()
    {
        if (!IsEditingGivens && Selected.IsGiven)
        {
            Status = "That digit is part of the puzzle and cannot be changed.";
            return;
        }

        Selected.Value = 0;
        Selected.Marks = 0;
        if (IsEditingGivens) Selected.IsGiven = false;
        Selected.RefreshNotes(0, ShowCandidates);

        BoardChanged();
    }

    /// <summary>
    /// Everything that has to happen after the grid changes, in one place.
    ///
    /// These four always belong together: a stale hint is about a board that no
    /// longer exists, conflicts and notes must reflect what is on screen now,
    /// and completion has to be noticed the instant it happens. Calling them
    /// separately at seven call sites is how one of them ends up forgotten.
    /// </summary>
    private void BoardChanged()
    {
        ClearHint();
        RefreshConflicts();
        RefreshCandidates();
        CheckCompletion();
    }

    /// <summary>
    /// Notice a finished puzzle. Says who finished it: being congratulated for
    /// pressing "Solve it" would be hollow, and worse, misleading.
    /// </summary>
    private void CheckCompletion(bool byEngine = false)
    {
        var full = Cells.All(cell => cell.Value != 0);
        IsSolved = full && SudokuEngine.Validate(CurrentBoard()).Count == 0;

        if (!IsSolved)
        {
            CompletionMessage = string.Empty;
            return;
        }
        var name = string.IsNullOrWhiteSpace(BoardName) ? "this puzzle" : $"\"{BoardName.Trim()}\"";
        CompletionMessage = byEngine
            ? $"{name} is complete — filled in by the engine."
            : $"Solved! {name} is complete and every digit checks out.";
    }

    /// <summary>Add or remove one pencil mark in the selected cell.</summary>
    public void ToggleMark(int digit)
    {
        if (digit is < 1 or > 9) return;
        if (Selected.IsGiven || Selected.Value != 0)
        {
            Status = "That cell already has a digit.";
            return;
        }

        Selected.Marks ^= (ushort)(1 << digit);
        RefreshCandidates();
    }

    /// <summary>
    /// Compare the player's notes against what the board actually allows.
    ///
    /// The one feature that reads the marks at all. Stale marks matter more
    /// than missing ones: a mark that a later placement has ruled out makes a
    /// cell look more open than it is, so the player never revisits it.
    /// </summary>
    private void AuditMarks()
    {
        var marks = Marks();
        if (marks.All(mask => mask == 0))
        {
            Status = "No pencil marks to check yet.";
            return;
        }

        var report = SudokuEngine.AuditMarks(CurrentBoard(), marks);
        if (report.Count == 0)
        {
            Status = "Every pencil mark is correct.";
            return;
        }

        var stale = report.Count(entry => entry.Stale.Count > 0);
        var missing = report.Count(entry => entry.Missing.Count > 0);

        ClearHighlights();
        foreach (var entry in report)
            Cells[entry.Cell.Row * 9 + entry.Cell.Col].Highlight =
                entry.Stale.Count > 0 ? CellHighlight.Eliminated : CellHighlight.Pattern;

        var first = report[0];
        var detail = first.Stale.Count > 0
            ? $"R{first.Cell.Row + 1}C{first.Cell.Col + 1} still notes {string.Join(", ", first.Stale)}, no longer possible"
            : $"R{first.Cell.Row + 1}C{first.Cell.Col + 1} is missing {string.Join(", ", first.Missing)}";

        Status = $"{stale} cell(s) with stale marks, {missing} with missing ones. {detail}.";
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

        BoardChanged();

        CheckCompletion(byEngine: true);

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
            cell.Marks = 0;
            if (IsEditingGivens) cell.IsGiven = false;
        }
        BoardChanged();
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
            cell.Marks = 0;
            cell.IsGiven = false;
        }
        BoardChanged();
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
            Cells[i].Marks = i < board.Marks.Length ? board.Marks[i] : (ushort)0;
        }

        _currentBoardId = board.Id;
        BoardName = board.Name;
        _isEditingGivens = false;
        Raise(nameof(IsEditingGivens));
        Raise(nameof(ModeLabel));

        BoardChanged();
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

    /// <summary>Pencil marks, row-major, for saving alongside the grid.</summary>
    private ushort[] Marks()
    {
        var marks = new ushort[81];
        for (var i = 0; i < 81; i++) marks[i] = Cells[i].Marks;
        return marks;
    }

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

    /// <summary>
    /// Rebuild the small digits in every cell: the player's pencil marks where
    /// they have made any, otherwise the engine's candidates when those are
    /// switched on.
    ///
    /// Runs even when candidates are hidden, because marks still have to be
    /// drawn. Candidates always come from the engine, computed from placed
    /// digits — the UI never invents them, so what you see is exactly what the
    /// hint engine is reasoning about.
    /// </summary>
    private void RefreshCandidates()
    {
        var masks = SudokuEngine.Candidates(CurrentBoard());

        for (var i = 0; i < 81; i++)
        {
            Cells[i].RefreshNotes(masks[i], ShowCandidates);

            var digits = new List<char>();
            if (ShowCandidates)
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
