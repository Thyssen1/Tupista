using System.Windows.Input;

using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;

using Tupista.ViewModels;

namespace Tupista.Desktop;

/// <summary>
/// Turns pointer gestures on a cell into selection commands.
///
/// This exists because a Button's Command binding cannot see modifier keys or
/// drags — it only knows that a click happened. Rather than put a handler in
/// the window's code-behind, the knowledge lives here: a behaviour attached in
/// XAML that reads the gesture and calls the matching command on the view
/// model. The view model still makes every decision about what a gesture
/// means; this only reports which gesture it was.
///
/// Attach it with CellSelectionBehavior.Cell="{Binding}" on the cell button.
/// </summary>
public static class CellSelectionBehavior
{
    /// <summary>
    /// True while the pointer is down and sweeping. Static because there is
    /// exactly one board and one pointer; a drag is a property of the session,
    /// not of any one cell.
    /// </summary>
    private static bool _dragging;

    public static readonly AttachedProperty<CellViewModel?> CellProperty =
        AvaloniaProperty.RegisterAttached<Control, CellViewModel?>(
            "Cell", typeof(CellSelectionBehavior));

    public static void SetCell(Control control, CellViewModel? value) =>
        control.SetValue(CellProperty, value);

    public static CellViewModel? GetCell(Control control) => control.GetValue(CellProperty);

    /// <summary>
    /// The same idea for a cell name inside the hint text: a plain click jumps
    /// to that cell, Ctrl+click adds it to the selection. No drag — sweeping
    /// across a sentence does not describe a set of cells.
    /// </summary>
    public static readonly AttachedProperty<HintSegment?> HintCellProperty =
        AvaloniaProperty.RegisterAttached<Control, HintSegment?>(
            "HintCell", typeof(CellSelectionBehavior));

    public static void SetHintCell(Control control, HintSegment? value) =>
        control.SetValue(HintCellProperty, value);

    public static HintSegment? GetHintCell(Control control) => control.GetValue(HintCellProperty);

    static CellSelectionBehavior()
    {
        CellProperty.Changed.AddClassHandler<Control>((control, args) =>
        {
            if (args.NewValue is null) return;

            // Tunnel, so this runs BEFORE the Button handles the press itself.
            // On the bubble strategy the Button would already have dealt with
            // it and a Ctrl+click would arrive looking like a plain one.
            control.AddHandler(InputElement.PointerPressedEvent, OnPressed,
                RoutingStrategies.Tunnel);
            control.AddHandler(InputElement.PointerEnteredEvent, OnEntered,
                RoutingStrategies.Bubble);
            control.AddHandler(InputElement.PointerReleasedEvent, OnReleased,
                RoutingStrategies.Tunnel);
            control.AddHandler(InputElement.PointerCaptureLostEvent, OnCaptureLost,
                RoutingStrategies.Bubble);
        });

        HintCellProperty.Changed.AddClassHandler<Control>((control, args) =>
        {
            if (args.NewValue is null) return;
            control.AddHandler(InputElement.PointerPressedEvent, OnHintPressed,
                RoutingStrategies.Tunnel);
        });
    }

    private static void OnHintPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Control control) return;
        if (GetHintCell(control) is not { IsCell: true } segment) return;
        if (ViewModelOf(control) is not { } viewModel) return;
        if (!e.GetCurrentPoint(control).Properties.IsLeftButtonPressed) return;

        var cell = viewModel.Cells[segment.Row * 9 + segment.Col];
        Run(e.KeyModifiers.HasFlag(KeyModifiers.Control)
                ? viewModel.ExtendSelectCommand
                : viewModel.SelectCommand,
            cell);
    }

    private static void OnPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Control control) return;
        if (GetCell(control) is not { } cell) return;
        if (ViewModelOf(control) is not { } viewModel) return;
        if (!e.GetCurrentPoint(control).Properties.IsLeftButtonPressed) return;

        var modifiers = e.KeyModifiers;

        if (modifiers.HasFlag(KeyModifiers.Control))
        {
            Run(viewModel.ExtendSelectCommand, cell);
        }
        else if (modifiers.HasFlag(KeyModifiers.Shift))
        {
            Run(viewModel.SelectRangeCommand, cell);
        }
        else
        {
            // A plain press both selects and opens a drag. The drag only
            // becomes visible if the pointer then moves onto another cell.
            Run(viewModel.SelectCommand, cell);
            _dragging = true;
        }
    }

    private static void OnEntered(object? sender, PointerEventArgs e)
    {
        if (!_dragging) return;
        if (sender is not Control control) return;
        if (GetCell(control) is not { } cell) return;
        if (ViewModelOf(control) is not { } viewModel) return;

        Run(viewModel.AddToSelectionCommand, cell);
    }

    private static void OnReleased(object? sender, PointerReleasedEventArgs e) => _dragging = false;

    private static void OnCaptureLost(object? sender, PointerCaptureLostEventArgs e) =>
        _dragging = false;

    /// <summary>
    /// The window's view model, not the cell's. The cell template's DataContext
    /// is the CellViewModel, so the commands have to be reached through the
    /// visual tree.
    /// </summary>
    private static MainViewModel? ViewModelOf(Control control) =>
        (control.GetVisualRoot() as Control)?.DataContext as MainViewModel;

    private static void Run(ICommand command, CellViewModel cell)
    {
        if (command.CanExecute(cell)) command.Execute(cell);
    }
}