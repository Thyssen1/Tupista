using System.Runtime.InteropServices;

namespace Tupista.Interop;

/// <summary>
/// Owns a native handle and guarantees it is freed exactly once.
///
/// Why not just an IntPtr and a try/finally? Because a SafeHandle is
/// registered with the runtime's critical finalization machinery: it is freed
/// even if the owning code throws, is aborted, or is simply forgotten, and the
/// runtime will not let the handle be collected while a P/Invoke call using it
/// is still in flight. An IntPtr passed to a native call can be collected
/// mid-call if nothing else references the wrapper — a race that shows up as
/// an impossible-looking crash under load.
///
/// ownsHandle: true means "this object is responsible for freeing it".
/// The invalid value is IntPtr.Zero, which is what the C side returns on
/// failure — so a failed create produces a handle that reports IsInvalid.
/// </summary>
internal sealed class SolveHandle() : SafeHandle(IntPtr.Zero, ownsHandle: true)
{
    public override bool IsInvalid => handle == IntPtr.Zero;

    protected override bool ReleaseHandle()
    {
        NativeMethods.tupista_solve_free(handle);
        return true;
    }
}

internal sealed class HintHandle() : SafeHandle(IntPtr.Zero, ownsHandle: true)
{
    public override bool IsInvalid => handle == IntPtr.Zero;

    protected override bool ReleaseHandle()
    {
        NativeMethods.tupista_hint_free(handle);
        return true;
    }
}
