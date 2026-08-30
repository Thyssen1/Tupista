using Avalonia;
using Avalonia.Markup.Xaml.Styling;

using Tupista.ViewModels;

namespace Tupista.Desktop;

/// <summary>
/// Swaps the application's colour palette at runtime.
///
/// This is view code by necessity: choosing which ResourceDictionary is merged
/// cannot be expressed as a binding, and the view model must not know Avalonia
/// exists. So the view model owns the CHOICE (an <see cref="AppPalette"/> enum)
/// and this class owns the CONSEQUENCE.
///
/// Note what this deliberately does not touch: light versus dark. Each palette
/// file carries both variants, and the window's RequestedThemeVariant picks
/// between them. The two settings are independent, which is why you can run
/// Ink &amp; Vermilion in dark mode.
/// </summary>
internal static class PaletteLoader
{
    /// <summary>
    /// Index of the palette inside Application.Resources.MergedDictionaries.
    /// App.axaml merges exactly one dictionary, so the palette is always first
    /// and swapping means replacing this slot rather than appending — appending
    /// would leave every previous palette in the lookup chain.
    /// </summary>
    private const int PaletteSlot = 0;

    public static void Apply(Application application, AppPalette palette)
    {
        var uri = new Uri($"avares://Tupista.Desktop/Themes/{FileNameFor(palette)}.axaml");
        var dictionary = new ResourceInclude(uri) { Source = uri };

        var merged = application.Resources.MergedDictionaries;
        if (merged.Count > PaletteSlot) merged[PaletteSlot] = dictionary;
        else merged.Add(dictionary);
    }

    private static string FileNameFor(AppPalette palette) => palette switch
    {
        AppPalette.Original => "Original",
        AppPalette.EmeraldGraphite => "EmeraldGraphite",
        AppPalette.Monochrome => "Monochrome",
        _ => "InkVermilion",
    };
}
