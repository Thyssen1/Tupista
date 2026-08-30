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
            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainViewModel(),
            };
        }

        base.OnFrameworkInitializationCompleted();
    }
}
