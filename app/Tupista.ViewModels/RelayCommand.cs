using System.Windows.Input;

namespace Tupista.ViewModels;

/// <summary>
/// Wraps a method as an ICommand so a button can bind to it.
///
/// ICommand is two things: something to run, and whether it can run right now.
/// The second half is what greys a button out — the view asks CanExecute and
/// re-asks whenever CanExecuteChanged fires.
/// </summary>
public sealed class RelayCommand(Action execute, Func<bool>? canExecute = null) : ICommand
{
    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => canExecute?.Invoke() ?? true;

    public void Execute(object? parameter) => execute();

    /// <summary>
    /// Tell the view to re-check CanExecute. Call this after changing state
    /// that a command's availability depends on.
    /// </summary>
    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}

/// <summary>The same, for commands the view passes an argument to.</summary>
public sealed class RelayCommand<T>(Action<T> execute, Func<T, bool>? canExecute = null) : ICommand
{
    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) =>
        parameter is T value ? canExecute?.Invoke(value) ?? true : parameter is null;

    public void Execute(object? parameter)
    {
        if (parameter is T value) execute(value);
    }

    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
