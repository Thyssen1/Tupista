namespace Tupista.ViewModels;

/// <summary>
/// Which colour scheme the user has chosen.
///
/// A plain enum rather than Avalonia's ThemeVariant on purpose: this assembly
/// must not reference a UI framework, or the iOS head cannot reuse it. The
/// view translates this into whatever its own toolkit understands.
/// </summary>
public enum AppTheme
{
    /// <summary>Follow whatever the operating system is set to.</summary>
    System,
    Light,
    Dark,
}
