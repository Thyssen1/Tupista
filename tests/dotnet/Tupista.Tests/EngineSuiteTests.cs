using System.Diagnostics;

using Xunit;
using Xunit.Abstractions;

namespace Tupista.Tests;

/// <summary>
/// Runs the C++ test suite through CTest and fails if it does.
///
/// Why bridge it instead of just running sudoku_tests.exe by hand: it puts the
/// 73 engine tests into the same list as the C# ones, so "run all tests" in the
/// IDE really does mean all of them. It also needs no per-machine setup — a
/// hand-made run configuration lives in .idea, which is gitignored and would
/// have to be recreated on every clone and on the Mac.
///
/// This does not replace the C++ tests, it surfaces them.
/// </summary>
public sealed class EngineSuiteTests(ITestOutputHelper output)
{
    [Fact]
    public void CppEngineSuitePasses()
    {
        var buildDir = FindCMakeBuildDirectory();

        // A hard failure rather than a skip: the app cannot run without the
        // native engine either, so "no CMake build here" is a broken setup, not
        // an excuse to pass quietly.
        Assert.True(buildDir is not null,
            "No CMake build directory found. Configure one first: cmake -S . -B out");

        var process = Process.Start(new ProcessStartInfo
        {
            FileName = "ctest",
            // --output-on-failure so a failure reports which assertion broke,
            // rather than just a non-zero exit code.
            Arguments = $"--test-dir \"{buildDir}\" --output-on-failure",
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        });

        Assert.NotNull(process);
        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit();

        output.WriteLine(stdout);
        if (!string.IsNullOrWhiteSpace(stderr)) output.WriteLine(stderr);

        Assert.True(process.ExitCode == 0, $"The C++ engine test suite failed.\n{stdout}\n{stderr}");
    }

    /// <summary>
    /// The build directory is wherever it happens to be: `out` from the command
    /// line, `cmake-build-debug` from Rider, `build` on a Mac or in CI.
    /// </summary>
    private static string? FindCMakeBuildDirectory()
    {
        var root = RepositoryRoot();
        if (root is null) return null;

        foreach (var candidate in new[] { "out", "cmake-build-debug", "build", "cmake-build-release" })
        {
            var path = Path.Combine(root, candidate);
            if (File.Exists(Path.Combine(path, "CMakeCache.txt"))) return path;
        }
        return null;
    }

    /// <summary>
    /// Walks up from the test binary until it finds the repository root. The
    /// test runs from bin/x64/Debug/net8.0, and hard-coding "../../../../.."
    /// breaks the moment the output layout changes.
    /// </summary>
    private static string? RepositoryRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            if (File.Exists(Path.Combine(directory.FullName, "Tupista.sln"))) return directory.FullName;
            directory = directory.Parent;
        }
        return null;
    }
}
