using Tupista.Storage;
using Tupista.ViewModels;

using Xunit;

namespace Tupista.Tests;

/// <summary>
/// The view model, driven the way the window drives it.
///
/// These exist because MainViewModel has no UI-framework reference — that was
/// the whole point of keeping Avalonia out of it. Every one of these runs
/// without a window, and would run unchanged against an iOS front end.
///
/// Each test gets a throwaway database so they cannot interfere with each
/// other or with the real one in AppData.
/// </summary>
public sealed class MainViewModelTests : IDisposable
{
    private const string Puzzle =
        "029000610050017000001090005060940081000786304000120006008002000300801062000409000";
    private const string Solved =
        "829354617654217839731698425567943281192786354483125796978562143345871962216439578";

    private readonly string _databasePath =
        Path.Combine(Path.GetTempPath(), $"tupista-tests-{Guid.NewGuid():N}.db");

    private MainViewModel NewViewModel() => new(new BoardRepository(_databasePath));

    public void Dispose()
    {
        // Pooling is off in BoardRepository, so the file really is closed and
        // this delete succeeds. It did not, before that was fixed.
        if (File.Exists(_databasePath)) File.Delete(_databasePath);
    }

    private static void TypeIn(MainViewModel viewModel, string board)
    {
        for (var i = 0; i < 81; i++)
        {
            viewModel.Select(viewModel.Cells[i]);
            viewModel.EnterDigitCommand.Execute(board[i].ToString());
        }
    }

    [Fact]
    public void TypingDigitsBuildsTheBoard()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);

        Assert.Equal(Puzzle, viewModel.CurrentBoard());
        Assert.Equal(32, viewModel.Cells.Count(c => c.IsGiven));
    }

    [Fact]
    public void ArrowMovementClampsAtTheEdges()
    {
        var viewModel = NewViewModel();

        viewModel.MoveCommand.Execute("up");        // already on the top row
        Assert.Equal(0, viewModel.Selected.Row);

        viewModel.MoveCommand.Execute("right");
        Assert.Equal(1, viewModel.Selected.Col);

        for (var i = 0; i < 20; i++) viewModel.MoveCommand.Execute("down");
        Assert.Equal(8, viewModel.Selected.Row);    // stops, does not wrap
    }

    [Fact]
    public void GivensAreProtectedOncePlaying()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[1]);       // a given holding 2
        viewModel.EnterDigitCommand.Execute("7");

        Assert.Equal(2, viewModel.Cells[1].Value);
    }

    [Fact]
    public void ClearReturnsToTheStartingPosition()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);       // an empty cell
        viewModel.EnterDigitCommand.Execute("8");
        Assert.Equal(8, viewModel.Cells[0].Value);

        viewModel.ClearCommand.Execute(null);

        Assert.Equal(Puzzle, viewModel.CurrentBoard());
        Assert.Equal(32, viewModel.Cells.Count(c => c.IsGiven));
    }

    [Fact]
    public void EscapeResetsTheCellIncludingItsMarks()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("8");
        viewModel.Cells[0].Marks = 0b0000_0001_1001_0000;   // pretend notes

        viewModel.ResetCellCommand.Execute(null);

        Assert.Equal(0, viewModel.Cells[0].Value);
        Assert.Equal(0, viewModel.Cells[0].Marks);
    }

    [Fact]
    public void EscapeLeavesGivensAlone()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[1]);    // a given holding 2
        viewModel.ResetCellCommand.Execute(null);

        Assert.Equal(2, viewModel.Cells[1].Value);
    }

    [Fact]
    public void PencilMarksSurviveSaveAndReload()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.Cells[0].Marks = (1 << 4) | (1 << 7) | (1 << 8);
        viewModel.SaveCommand.Execute(null);

        viewModel.ClearCommand.Execute(null);
        Assert.Equal(0, viewModel.Cells[0].Marks);

        viewModel.LoadCommand.Execute(viewModel.SavedBoards[0]);
        Assert.Equal((1 << 4) | (1 << 7) | (1 << 8), viewModel.Cells[0].Marks);
    }

    [Fact]
    public void CheckReportsAUniqueSolution()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);

        viewModel.CheckCommand.Execute(null);

        Assert.Contains("one solution", viewModel.Status);
    }

    [Fact]
    public void ConflictingDigitsAreFlaggedOnTheCells()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("2");   // duplicates the 2 beside it

        Assert.Equal(2, viewModel.Cells.Count(c => c.IsConflicted));
    }

    [Fact]
    public void HintEscalatesRatherThanRerolling()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.HintCommand.Execute(null);
        var vague = viewModel.HintText;
        viewModel.HintCommand.Execute(null);
        var mechanism = viewModel.HintText;
        viewModel.HintCommand.Execute(null);
        var full = viewModel.HintText;

        Assert.NotEqual(vague, mechanism);
        Assert.NotEqual(mechanism, full);
        Assert.True(full.Length > mechanism.Length);

        // Only the full level paints the board.
        Assert.Contains(viewModel.Cells, c => c.IsHintPlacement);
    }

    [Fact]
    public void EditingTheBoardDiscardsTheHintOnScreen()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        Assert.NotEmpty(viewModel.HintText);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("4");

        // The hint reasoned about a board that no longer exists.
        Assert.Empty(viewModel.HintText);
        Assert.DoesNotContain(viewModel.Cells, c => c.IsHintPlacement);
    }

    [Fact]
    public void SolveFillsTheGridAndNamesTheTechnique()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.SolveCommand.Execute(null);

        Assert.DoesNotContain(viewModel.Cells, c => c.Value == 0);
        Assert.Contains("naked/hidden pairs", viewModel.Status);
    }

    [Fact]
    public void SavingThenReloadingRestoresTheStartingPosition()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.BoardName = "Book puzzle";
        viewModel.SaveCommand.Execute(null);

        Assert.Single(viewModel.SavedBoards);

        viewModel.SolveCommand.Execute(null);       // wander off the saved state
        viewModel.LoadCommand.Execute(viewModel.SavedBoards[0]);

        Assert.Equal(Puzzle, viewModel.CurrentBoard());
        Assert.Equal("Book puzzle", viewModel.BoardName);
    }

    [Fact]
    public void SavingTwiceUpdatesTheSameRow()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.BoardName = "Once";
        viewModel.SaveCommand.Execute(null);
        viewModel.SaveCommand.Execute(null);

        Assert.Single(viewModel.SavedBoards);
    }

    [Fact]
    public void DeletingRemovesTheBoard()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.SaveCommand.Execute(null);

        viewModel.DeleteCommand.Execute(viewModel.SavedBoards[0]);

        Assert.Empty(viewModel.SavedBoards);
    }

    [Fact]
    public void PencilModeNotesInsteadOfPlacing()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.TogglePencilCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("4");
        viewModel.EnterDigitCommand.Execute("7");

        Assert.Equal(0, viewModel.Cells[0].Value);           // nothing placed
        Assert.Equal((1 << 4) | (1 << 7), viewModel.Cells[0].Marks);
    }

    [Fact]
    public void NotingTheSameDigitTwiceRemovesIt()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.NoteDigitCommand.Execute("4");
        Assert.Equal(1 << 4, viewModel.Cells[0].Marks);

        viewModel.NoteDigitCommand.Execute("4");
        Assert.Equal(0, viewModel.Cells[0].Marks);
    }

    [Fact]
    public void PlacingADigitClearsThatCellsOwnNotesOnly()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.NoteDigitCommand.Execute("4");
        viewModel.Select(viewModel.Cells[3]);
        viewModel.NoteDigitCommand.Execute("4");

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("8");

        Assert.Equal(0, viewModel.Cells[0].Marks);           // this cell is decided
        Assert.Equal(1 << 4, viewModel.Cells[3].Marks);      // others are the player's business
    }

    [Fact]
    public void NotesCannotBeAddedToACellThatAlreadyHasADigit()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[1]);                // a given
        viewModel.NoteDigitCommand.Execute("5");

        Assert.Equal(0, viewModel.Cells[1].Marks);
    }

    [Fact]
    public void TheAuditFindsStaleMarks()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        // R1C1 can hold 4, 7 or 8. Note all three, then place a 4 in the same
        // row so one of those notes goes stale.
        viewModel.Select(viewModel.Cells[0]);
        foreach (var digit in new[] { "4", "7", "8" }) viewModel.NoteDigitCommand.Execute(digit);
        viewModel.Select(viewModel.Cells[3]);
        viewModel.EnterDigitCommand.Execute("4");

        viewModel.AuditMarksCommand.Execute(null);

        Assert.Contains("stale", viewModel.Status);
        Assert.Contains("R1C1", viewModel.Status);
    }

    [Fact]
    public void TheAuditIsQuietWhenTheMarksAreRight()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.Select(viewModel.Cells[0]);
        foreach (var digit in new[] { "4", "7", "8" }) viewModel.NoteDigitCommand.Execute(digit);

        viewModel.AuditMarksCommand.Execute(null);

        Assert.Contains("correct", viewModel.Status);
    }

    [Fact]
    public void NotesRenderAsUserMarksWhenThePlayerHasWrittenAny()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.ShowCandidates = true;

        var cell = viewModel.Cells[0];
        Assert.True(cell.HasNotes);
        Assert.All(cell.Notes.Where(n => n.Shown), n => Assert.False(n.IsUserMark));

        viewModel.Select(cell);
        viewModel.NoteDigitCommand.Execute("4");

        // The player's own notes take over from the engine's candidates.
        Assert.All(cell.Notes.Where(n => n.Shown), n => Assert.True(n.IsUserMark));
        Assert.Single(cell.Notes, n => n.Shown);
    }

    [Fact]
    public void FinishingThePuzzleAnnouncesItself()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.BoardName = "Book puzzle";
        Assert.False(viewModel.IsSolved);

        // Fill in the answer the way a player would, one cell at a time.
        for (var i = 0; i < 81; i++)
        {
            if (viewModel.Cells[i].Value != 0) continue;
            viewModel.Select(viewModel.Cells[i]);
            viewModel.EnterDigitCommand.Execute(Solved[i].ToString());
        }

        Assert.True(viewModel.IsSolved);
        Assert.Contains("Solved!", viewModel.CompletionMessage);
        Assert.Contains("Book puzzle", viewModel.CompletionMessage);
    }

    [Fact]
    public void AFullButIllegalGridIsNotComplete()
    {
        var viewModel = NewViewModel();

        // 81 digits, but row 1 is nine 1s: full and thoroughly wrong.
        TypeIn(viewModel, new string('1', 81));

        Assert.False(viewModel.IsSolved);
        Assert.Empty(viewModel.CompletionMessage);
    }

    [Fact]
    public void TheEngineSolvingItIsNotCongratulated()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        viewModel.SolveCommand.Execute(null);

        Assert.True(viewModel.IsSolved);
        Assert.Contains("engine", viewModel.CompletionMessage);
        Assert.DoesNotContain("Solved!", viewModel.CompletionMessage);
    }

    [Fact]
    public void RemovingADigitUndoesCompletion()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.SolveCommand.Execute(null);
        Assert.True(viewModel.IsSolved);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("0");

        Assert.False(viewModel.IsSolved);
        Assert.Empty(viewModel.CompletionMessage);
    }

    // --- multi-select -------------------------------------------------------

    private static MainViewModel Playing(MainViewModel viewModel)
    {
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        return viewModel;
    }

    [Fact]
    public void APlainClickCollapsesTheSelection()
    {
        var viewModel = Playing(NewViewModel());

        viewModel.Select(viewModel.Cells[0]);
        viewModel.ExtendSelect(viewModel.Cells[4]);
        Assert.Equal(2, viewModel.SelectionCount);

        viewModel.Select(viewModel.Cells[8]);

        Assert.Equal(1, viewModel.SelectionCount);
        Assert.Same(viewModel.Cells[8], viewModel.Selected);
    }

    [Fact]
    public void CtrlClickAddsThenRemovesACell()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);

        viewModel.ExtendSelect(viewModel.Cells[4]);
        Assert.Equal(2, viewModel.SelectionCount);
        Assert.True(viewModel.HasMultiSelection);

        viewModel.ExtendSelect(viewModel.Cells[4]);
        Assert.Equal(1, viewModel.SelectionCount);
        Assert.False(viewModel.Cells[4].IsSelected);
    }

    [Fact]
    public void TheSelectionIsNeverLeftEmpty()
    {
        // With nothing selected there is no anchor, so the keyboard would have
        // nowhere to go and typing would silently do nothing.
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);

        viewModel.ExtendSelect(viewModel.Cells[0]);

        Assert.Equal(1, viewModel.SelectionCount);
        Assert.True(viewModel.Cells[0].IsSelected);
    }

    [Fact]
    public void DeselectingTheAnchorMovesItToACellStillSelected()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);
        viewModel.ExtendSelect(viewModel.Cells[4]);   // anchor is now cell 4

        viewModel.ExtendSelect(viewModel.Cells[4]);   // drop the anchor itself

        Assert.True(viewModel.Selected.IsSelected);
        Assert.Same(viewModel.Cells[0], viewModel.Selected);
    }

    [Fact]
    public void ShiftClickSelectsTheRectangleBetweenAnchorAndCell()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[1 * 9 + 1]);      // R2C2

        viewModel.SelectRange(viewModel.Cells[3 * 9 + 4]); // R4C5

        // Rows 1-3, columns 1-4 inclusive: 3 x 4 cells.
        Assert.Equal(12, viewModel.SelectionCount);
        Assert.All(viewModel.SelectedCells, cell =>
        {
            Assert.InRange(cell.Row, 1, 3);
            Assert.InRange(cell.Col, 1, 4);
        });

        // The anchor stays put, so the far corner can be adjusted again.
        Assert.Same(viewModel.Cells[1 * 9 + 1], viewModel.Selected);
    }

    [Fact]
    public void ARectangleWorksBackwardsToo()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[5 * 9 + 6]);      // bottom-right first

        viewModel.SelectRange(viewModel.Cells[3 * 9 + 4]); // up and to the left

        Assert.Equal(9, viewModel.SelectionCount);         // rows 3-5, cols 4-6
    }

    [Fact]
    public void DraggingBackOverACellDoesNotDeselectIt()
    {
        // AddToSelection adds without toggling, so sweeping back across a cell
        // during a drag leaves it selected.
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);

        viewModel.AddToSelection(viewModel.Cells[1]);
        viewModel.AddToSelection(viewModel.Cells[1]);

        Assert.True(viewModel.Cells[1].IsSelected);
        Assert.Equal(2, viewModel.SelectionCount);
    }

    [Fact]
    public void ShiftArrowGrowsTheSelectionAndPlainArrowCollapsesIt()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);

        viewModel.ExtendMoveCommand.Execute("right");
        viewModel.ExtendMoveCommand.Execute("right");

        Assert.Equal(3, viewModel.SelectionCount);
        Assert.Same(viewModel.Cells[2], viewModel.Selected);

        viewModel.MoveCommand.Execute("down");

        Assert.Equal(1, viewModel.SelectionCount);
        Assert.Same(viewModel.Cells[1 * 9 + 2], viewModel.Selected);
    }

    [Fact]
    public void ADigitNotesAcrossTheSelectionRatherThanPlacing()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);
        viewModel.ExtendSelect(viewModel.Cells[3]);

        viewModel.EnterDigitCommand.Execute("7");

        Assert.Equal(0, viewModel.Cells[0].Value);        // nothing placed
        Assert.Equal(0, viewModel.Cells[3].Value);
        Assert.Equal(1 << 7, viewModel.Cells[0].Marks);
        Assert.Equal(1 << 7, viewModel.Cells[3].Marks);
    }

    [Fact]
    public void BulkNotesAreAllOrNothing()
    {
        // A mixed selection must end up uniform, not inverted cell by cell.
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);
        viewModel.NoteDigitCommand.Execute("7");          // only this one has a 7

        viewModel.ExtendSelect(viewModel.Cells[3]);
        viewModel.EnterDigitCommand.Execute("7");

        Assert.Equal(1 << 7, viewModel.Cells[0].Marks);
        Assert.Equal(1 << 7, viewModel.Cells[3].Marks);

        // Now everyone has it, so a second press takes it off everyone.
        viewModel.EnterDigitCommand.Execute("7");
        Assert.Equal(0, viewModel.Cells[0].Marks);
        Assert.Equal(0, viewModel.Cells[3].Marks);
    }

    [Fact]
    public void ShiftDigitAlsoNotesAcrossTheSelection()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);
        viewModel.ExtendSelect(viewModel.Cells[3]);

        viewModel.NoteDigitCommand.Execute("4");

        Assert.Equal(1 << 4, viewModel.Cells[0].Marks);
        Assert.Equal(1 << 4, viewModel.Cells[3].Marks);
    }

    [Fact]
    public void BulkNotesSkipGivensAndFilledCells()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);             // empty
        viewModel.ExtendSelect(viewModel.Cells[1]);       // a given holding 2

        viewModel.EnterDigitCommand.Execute("7");

        Assert.Equal(1 << 7, viewModel.Cells[0].Marks);
        Assert.Equal(0, viewModel.Cells[1].Marks);
        Assert.Equal(2, viewModel.Cells[1].Value);        // untouched
    }

    [Fact]
    public void EscapeClearsEverySelectedCell()
    {
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("8");         // single select: places
        viewModel.Select(viewModel.Cells[3]);
        viewModel.EnterDigitCommand.Execute("5");

        viewModel.Select(viewModel.Cells[0]);
        viewModel.ExtendSelect(viewModel.Cells[3]);
        viewModel.ResetCellCommand.Execute(null);

        Assert.Equal(0, viewModel.Cells[0].Value);
        Assert.Equal(0, viewModel.Cells[3].Value);
    }

    [Fact]
    public void SingleCellBehaviourIsUnchanged()
    {
        // The whole point is that nothing changes until you actually
        // multi-select.
        var viewModel = Playing(NewViewModel());
        viewModel.Select(viewModel.Cells[0]);

        viewModel.EnterDigitCommand.Execute("8");
        Assert.Equal(8, viewModel.Cells[0].Value);
        Assert.False(viewModel.HasMultiSelection);
    }

    [Fact]
    public void HintTextIsSplitIntoClickableCellReferences()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);

        // Escalate to the full hint, which is the level that names cells.
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);

        Assert.NotEmpty(viewModel.HintLines);
        var cells = viewModel.HintLines
            .SelectMany(line => line.Segments)
            .Where(segment => segment.IsCell)
            .ToList();

        Assert.NotEmpty(cells);
        Assert.All(cells, segment =>
        {
            Assert.InRange(segment.Row, 0, 8);
            Assert.InRange(segment.Col, 0, 8);
        });
    }

    [Fact]
    public void TheSegmentsReassembleIntoTheOriginalText()
    {
        // The split must be lossless: spacing and punctuation around a cell
        // name have to survive, or the hint reads wrong on screen.
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);

        var rebuilt = string.Join("\n", viewModel.HintLines
            .Select(line => string.Concat(line.Segments.Select(s => s.Text))));

        Assert.Equal(viewModel.HintText.Replace("\r", ""), rebuilt);
    }

    [Fact]
    public void ClickingACellReferenceSelectsThatCell()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        viewModel.HintCommand.Execute(null);

        var target = viewModel.HintLines
            .SelectMany(line => line.Segments)
            .First(segment => segment.IsCell);

        viewModel.FocusHintCellCommand.Execute(target);

        Assert.Equal(target.Row, viewModel.Selected.Row);
        Assert.Equal(target.Col, viewModel.Selected.Col);
        Assert.True(viewModel.Selected.IsSelected);
    }

    [Fact]
    public void TheHintLinesClearWithTheHint()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);
        viewModel.TogglePlayCommand.Execute(null);
        viewModel.HintCommand.Execute(null);
        Assert.NotEmpty(viewModel.HintLines);

        viewModel.Select(viewModel.Cells[0]);
        viewModel.EnterDigitCommand.Execute("4");

        Assert.Empty(viewModel.HintLines);
    }

    [Fact]
    public void CandidatesComeFromTheEngineNotTheUi()
    {
        var viewModel = NewViewModel();
        TypeIn(viewModel, Puzzle);

        viewModel.ShowCandidates = true;
        Assert.Equal("478", viewModel.Cells[0].Candidates);

        viewModel.ShowCandidates = false;
        Assert.Equal(string.Empty, viewModel.Cells[0].Candidates);
    }

    [Fact]
    public void TheThemeChoiceSurvivesARestart()
    {
        var viewModel = NewViewModel();
        Assert.Equal(AppTheme.System, viewModel.Theme);

        viewModel.Theme = AppTheme.Dark;

        // A second view model over the same database is what "restarting" means.
        Assert.Equal(AppTheme.Dark, NewViewModel().Theme);
    }

    [Fact]
    public void ThePaletteChoiceSurvivesARestart()
    {
        var viewModel = NewViewModel();
        Assert.Equal(AppPalette.InkVermilion, viewModel.Palette);

        viewModel.Palette = AppPalette.Monochrome;

        Assert.Equal(AppPalette.Monochrome, NewViewModel().Palette);
    }

    [Fact]
    public void PaletteAndThemeAreIndependent()
    {
        // The two settings multiply rather than compete: every palette has a
        // light and a dark version, so choosing one must not disturb the other.
        var viewModel = NewViewModel();

        viewModel.Palette = AppPalette.EmeraldGraphite;
        viewModel.Theme = AppTheme.Dark;
        Assert.Equal(AppPalette.EmeraldGraphite, viewModel.Palette);

        viewModel.Palette = AppPalette.Original;
        Assert.Equal(AppTheme.Dark, viewModel.Theme);

        var restarted = NewViewModel();
        Assert.Equal(AppPalette.Original, restarted.Palette);
        Assert.Equal(AppTheme.Dark, restarted.Theme);
    }

    [Fact]
    public void EveryPaletteIsOffered()
    {
        var viewModel = NewViewModel();
        Assert.Equal(4, viewModel.PaletteOptions.Count);
        Assert.Contains(AppPalette.Original, viewModel.PaletteOptions);
    }
}
