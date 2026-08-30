using System.Globalization;

using Avalonia.Data.Converters;
using Avalonia.Styling;

using Tupista.ViewModels;

namespace Tupista.Desktop;

/// <summary>
/// Translates the view model's <see cref="AppTheme"/> into Avalonia's
/// <see cref="ThemeVariant"/>.
///
/// This is the seam that lets the view model stay framework-free. It says
/// "Dark"; only this class knows what dark means to Avalonia. An iOS view would
/// write its own equivalent and share everything else.
///
/// Binding the result to a Window's RequestedThemeVariant re-themes that
/// window's whole visual tree, which is why no code-behind is needed to switch.
/// </summary>
public sealed class ThemeVariantConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        value switch
        {
            AppTheme.Light => ThemeVariant.Light,
            AppTheme.Dark => ThemeVariant.Dark,
            _ => ThemeVariant.Default,   // Default means "follow the OS"
        };

    public object ConvertBack(object? value, Type targetType, object? parameter,
                              CultureInfo culture) =>
        value switch
        {
            ThemeVariant variant when variant == ThemeVariant.Light => AppTheme.Light,
            ThemeVariant variant when variant == ThemeVariant.Dark => AppTheme.Dark,
            _ => AppTheme.System,
        };
}
