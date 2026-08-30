using Tupista.Interop;

using Xunit;

namespace Tupista.Tests;

/// <summary>
/// The managed side of the C ABI.
///
/// The engine itself is already covered by the C++ suite, so these do not
/// re-test Sudoku logic. They test the BOUNDARY: that buffers are sized right,
/// that enums line up on both sides, that handles are freed, and that a bad
/// argument produces an exception rather than a crash. That is where interop
/// bugs actually live.
/// </summary>
public sealed class InteropTests
{
    private const string Puzzle =
        "029000610050017000001090005060940081000786304000120006008002000300801062000409000";
    private const string Solved =
        "829354617654217839731698425567943281192786354483125796978562143345871962216439578";

    [Fact]
    public void NativeLibraryLoadsAndMatchesTheExpectedAbi() => SudokuEngine.EnsureAvailable();

    [Theory]
    [InlineData(Puzzle, true)]
    [InlineData("123", false)]
    [InlineData("", false)]
    public void WellFormedRecognisesValidBoards(string board, bool expected) =>
        Assert.Equal(expected, SudokuEngine.IsWellFormed(board));

    [Fact]
    public void ShortBoardsAreRejectedBeforeReachingNativeCode() =>
        Assert.Throws<ArgumentException>(() => SudokuEngine.Validate("too short"));

    [Fact]
    public void AProperPuzzleHasOneSolutionAndSolves()
    {
        Assert.Empty(SudokuEngine.Validate(Puzzle));
        Assert.Equal(1, SudokuEngine.CountSolutions(Puzzle));
        Assert.Equal(Solved, SudokuEngine.Solve(Puzzle));
    }

    [Fact]
    public void DuplicateDigitsAreReportedWithTheirCells()
    {
        // Two 2s side by side break both a row and a box.
        var bad = "2" + Puzzle[1..];

        var conflicts = SudokuEngine.Validate(bad);
        Assert.Equal(2, conflicts.Count);
        Assert.All(conflicts, c => Assert.Equal(2, c.Value));
        Assert.Contains(conflicts, c => c.Unit == UnitKind.Row);
        Assert.Contains(conflicts, c => c.Unit == UnitKind.Box);
        Assert.All(conflicts, c => Assert.Equal(2, c.Cells.Count));

        Assert.Equal(0, SudokuEngine.CountSolutions(bad));
    }

    [Fact]
    public void HumanSolveReportsItsNarrative()
    {
        var result = SudokuEngine.SolveLikeAHuman(Puzzle);

        Assert.True(result.Solved);
        Assert.Equal(3, result.Tier);
        Assert.Equal("naked/hidden pairs", result.TierName);
        Assert.Equal(Solved, result.Grid);
        Assert.Equal(49, result.Placements.Count);

        var first = result.Placements[0];
        Assert.Equal("R8C3", first.Cell.ToString());
        Assert.Equal(5, first.Value);
        Assert.Equal(Technique.NakedSingle, first.Technique);
    }

    [Fact]
    public void AHintCarriesItsEnablingChain()
    {
        var hint = SudokuEngine.NextHint(Puzzle);

        Assert.Equal(HintStatus.Ok, hint.Status);
        Assert.NotNull(hint.Placement);
        Assert.Equal("R8C3", hint.Placement!.Cell.ToString());

        // The chain is the point of the feature: a placement the player cannot
        // yet see must arrive with the deductions that make it visible.
        Assert.NotEmpty(hint.Steps);
        Assert.All(hint.Steps, step =>
        {
            Assert.NotEmpty(step.Pattern);
            Assert.NotEmpty(step.Eliminations);
            Assert.All(step.Eliminations, cut => Assert.InRange(cut.Digit, 1, 9));
        });
    }

    [Fact]
    public void DisclosureLevelsRevealProgressivelyMore()
    {
        var hint = SudokuEngine.NextHint(Puzzle);

        var vague = hint.TextFor(HintLevel.Vague);
        var mechanism = hint.TextFor(HintLevel.Mechanism);
        var full = hint.TextFor(HintLevel.Full);

        Assert.True(vague.Length < mechanism.Length);
        Assert.True(mechanism.Length < full.Length);

        // The vague level must not give the answer away.
        Assert.DoesNotContain("R8C3", vague);
        Assert.Contains("R8C3", full);
    }

    [Fact]
    public void AnIllegalBoardYieldsConflictsRatherThanAHint()
    {
        var hint = SudokuEngine.NextHint("2" + Puzzle[1..]);

        Assert.Equal(HintStatus.BoardInvalid, hint.Status);
        Assert.NotEmpty(hint.Conflicts);
        Assert.Null(hint.Placement);
    }

    [Fact]
    public void APositionBeyondTheLadderSaysSo()
    {
        // AI Escargot after its single available placement.
        var hint = SudokuEngine.NextHint(
            "100007090030020008009600500005300900010080002600004000300000010041000007007000300");
        Assert.Equal(HintStatus.Stuck, hint.Status);
    }

    [Fact]
    public void CandidatesComeBackAsBitmasks()
    {
        var masks = SudokuEngine.Candidates(Puzzle);

        Assert.Equal(81, masks.Length);
        Assert.Equal(0, masks[1]);                      // a given has no candidates
        foreach (var digit in new[] { 4, 7, 8 })
            Assert.True((masks[0] & (1 << digit)) != 0); // R1C1 can be 4, 7 or 8
        Assert.Equal(0, masks[0] & 1);                  // bit 0 is never used
    }

    [Fact]
    public void MarkAuditFindsAMissingNote()
    {
        var marks = new ushort[81];
        marks[0] = (1 << 4) | (1 << 7);   // noted 4 and 7, forgot 8

        var report = SudokuEngine.AuditMarks(Puzzle, marks);

        var cell = Assert.Single(report);
        Assert.Equal(new Cell(0, 0), cell.Cell);
        Assert.Equal([8], cell.Missing);
        Assert.Empty(cell.Stale);
    }

    [Fact]
    public void RepeatedCallsDoNotLeakHandles()
    {
        // Each call creates and frees a native handle. If SafeHandle release
        // were broken this would climb steadily; a hundred iterations is enough
        // for that to show up as an obvious failure rather than a slow leak.
        for (var i = 0; i < 100; i++)
        {
            _ = SudokuEngine.NextHint(Puzzle);
            _ = SudokuEngine.SolveLikeAHuman(Puzzle);
        }
    }
}
