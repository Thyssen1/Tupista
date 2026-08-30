using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Tupista.Desktop;

/// <summary>
/// Deliberately empty.
///
/// Avalonia needs a partial class to back x:Class, so the file has to exist —
/// but nothing beyond loading the XAML belongs here. Every click and keystroke
/// reaches the view model as a command, and every piece of visible state gets
/// there by binding. If logic ever appears in this file, it is logic that
/// cannot be tested and cannot be reused by the iOS head later.
/// </summary>
public partial class MainWindow : Window
{
    public MainWindow() => AvaloniaXamlLoader.Load(this);
}
