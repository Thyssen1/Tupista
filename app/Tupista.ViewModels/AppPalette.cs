namespace Tupista.ViewModels;

/// <summary>
/// Which colour scheme to use. Independent of <see cref="AppTheme"/>: every
/// palette has both a light and a dark variant, so the two settings multiply
/// rather than compete.
///
/// A plain enum for the same reason AppTheme is one — this assembly must not
/// know what a brush is, or the iOS head cannot reuse it.
///
/// Each accent is paired with a surface of matching temperature. A warm accent
/// (vermilion) on cool ground looks like two designs collided; that mistake is
/// what an earlier crimson-on-slate scheme got wrong and why it is gone.
/// </summary>
public enum AppPalette
{
    /// <summary>The blue scheme the app started with.</summary>
    Original,
    /// <summary>Warm paper, deep vermilion, teal for your own digits.</summary>
    InkVermilion,
    /// <summary>Graphite surfaces, emerald accent, sky blue for your digits.</summary>
    EmeraldGraphite,
    /// <summary>Greys everywhere; red is the only colour outside the board.</summary>
    Monochrome,
}
