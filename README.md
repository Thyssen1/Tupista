# Tupista

A desktop Sudoku assistant that explains its hints instead of handing over
answers.

*Tupista* is **tu pista** — Spanish for "your hint", and a portmanteau of
*Sudoku* and *pista*.

Most Sudoku apps will tell you a cell's answer. Tupista tells you **why the
answer is findable from where you are standing**. Ask for a hint on a hard
position and you get the chain of deductions that makes the next placement
visible, one step at a time, revealed only as far as you ask for.

---

## Why the hints are different

An engine can spot "R8C3 = 5, naked single" only after quietly eliminating
candidates in the background. Telling you that, while your own pencil marks
still show three possibilities in that cell, is useless — worse, it makes you
feel like you missed something obvious.

So a Tupista hint carries **the eliminations that get you there**, pruned to
only the deductions the answer actually rests on:

> 1. Hidden pair on 3 and 8 at R3C2 and R6C2 (column 2) removes 4, 7 from R3C2 …
> 2. Pointing on 7 at R1C1 and R3C1 (box 1) removes 7 from R4C1, R6C1 …
> 3. Pointing on 7 at R4C3 and R6C3 (box 4) removes 7 from R8C3 …
> 4. Claiming on 4 at R7C2 and R8C2 (column 2) removes 4 from R8C3
>
> Then place R8C3 = 5 (naked single).

And it's revealed in three stages, so you can take the smallest nudge that
gets you moving:

| Level | What you get |
|---|---|
| Vague | "Look at column 2 for digits 3 and 8." |
| Mechanism | "Hidden pair on 3 and 8 in column 2 rules candidates out elsewhere." |
| Full | Every step above, and the placement. |

The full answer is never shown unprompted.

## What it does

- **Enter a puzzle** from the keyboard, then lock it and start solving — givens
  can't be edited by accident once you're playing
- **Hint**, with the escalating disclosure above
- **Is there a solution?** — reports no solution, exactly one, or more than one
- **Solve it**, reporting the hardest technique the puzzle actually needed
- **Pencil marks**, with an audit that flags notes you're missing and notes a
  later placement has made stale
- **Save puzzles** to a local library; both the original puzzle and your
  progress are kept, so you can restart
- **Four colour schemes**, each in light and dark

Difficulty is reported as a tier from 1 (singles) to 7 (XY-Chain). A puzzle
beyond the engine's techniques is reported as such rather than guessed at —
AI Escargot, for instance, is correctly declared out of reach.

## Building

The Sudoku engine is C++ and the app is .NET, so you need both. **Build the
engine first** — the app loads it at runtime.

```bash
cmake -S . -B out && cmake --build out
```

```bash
dotnet build Tupista.sln -c Release
```

```bash
dotnet run --project app/Tupista.Desktop -c Release
```

After the first CMake configure, `dotnet build` drives CMake itself, so
day-to-day you only need the .NET command.

**Requirements:** a C++20 compiler, CMake 3.24+, and the .NET 8 SDK or newer.

### Tests

```bash
dotnet test Tupista.sln
```

Runs the .NET tests *and* the C++ suite, which is bridged through CTest so
everything appears in one list. To run only the C++ side:

```bash
ctest --test-dir out --output-on-failure
```

## How it's put together

```
core/              C++20 engine — all Sudoku logic, no platform dependencies
core/capi/         C ABI: the boundary any language can call
bindings/dotnet/   P/Invoke layer and safe C# wrappers
app/               Storage, view models, and the Avalonia desktop app
tests/             C++ tests; tests/dotnet holds the .NET ones
```

The engine deliberately knows nothing about any UI, and the view models
deliberately know nothing about Avalonia — the compiler enforces both, because
neither project references what it must not depend on. That is groundwork for
an iOS version sharing everything except the views.

The engine is verified on GCC, Clang, Apple Clang and MSVC on every push.

## Status

Version 1.0.0, Windows desktop. Planned next: undo/redo, puzzle generation,
and eventually an iPhone app that reads a puzzle from a photograph — which is
the reason the engine was built to be portable from the first line.
