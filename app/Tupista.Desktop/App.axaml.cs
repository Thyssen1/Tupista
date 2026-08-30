using System.ComponentModel;

using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;

using Tupista.ViewModels;

namespace Tupista.Desktop;

/// <summary>
/// Application startup: builds the view model and hands it to the window.
///
/// This is the composition root — the one place allowed to know about both the
/// view and the view model. Everywhere else the window only ever sees its
/// DataContext, which is what keeps the two sides independent.
/// </summary>
public partial class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var viewModel = new MainViewModel();

            // Apply the saved palette before the window is built, so the first
            // frame is already right rather than flashing the default.
            PaletteLoader.Apply(this, viewModel.Palette);

            // Light/dark is a binding on the window; the palette is not, because
            // swapping a ResourceDictionary is not something a binding can do.
            // Watching the property here is the smallest bridge that keeps the
            // view model free of Avalonia.
            viewModel.PropertyChanged += OnViewModelChanged;

            desktop.MainWindow = new MainWindow { DataContext = viewModel };
        }

        base.OnFrameworkInitializationCompleted();
    }

    private void OnViewModelChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(MainViewModel.Palette) && sender is MainViewModel viewModel)
            PaletteLoader.Apply(this, viewModel.Palette);
    }
}
