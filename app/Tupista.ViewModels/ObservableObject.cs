using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace Tupista.ViewModels;

/// <summary>
/// The smallest useful INotifyPropertyChanged base.
///
/// Data binding works by the view subscribing to PropertyChanged and refreshing
/// whatever named property fired. Without this, a view model can change all it
/// likes and the screen never updates.
///
/// Hand-written rather than taken from CommunityToolkit.Mvvm on purpose: it is
/// about forty lines, there is no source generator doing invisible work, and it
/// keeps this assembly free of dependencies that might not follow us to iOS.
/// </summary>
public abstract class ObservableObject : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>
    /// Assigns a field and raises the change notification, but only when the
    /// value actually differs — otherwise a self-referential binding can loop.
    ///
    /// [CallerMemberName] makes the compiler fill in the property name at the
    /// call site, so <c>Set(ref _name, value)</c> inside the Name setter passes
    /// "Name" without it ever being typed as a string that could go stale.
    /// </summary>
    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        Raise(propertyName);
        return true;
    }

    protected void Raise([CallerMemberName] string? propertyName = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
