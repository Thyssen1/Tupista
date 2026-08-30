using Avalonia;

namespace Tupista.Desktop;

/// <summary>
/// Process entry point. This is bootstrap, not view logic — it runs once before
/// any window exists, so there is nothing here for MVVM to separate.
/// </summary>
internal static class Program
{
    // STAThread is required by the Windows clipboard and file dialogs.
    [STAThread]
    public static void Main(string[] args) =>
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

    // Also called by Avalonia's XAML previewer, which is why it is public and
    // separate from Main.
    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
}
