# Fixtures + Static Looks Slint lab

This directory is the product-shaped Slint-first experiment required by issue
#37 and decision D-091. It is deliberately not the EmberLights product shell,
not installed by the normal package, and not evidence that Slint has been
accepted. The legacy Win32 app remains only the frozen transitional bridge.

The lab projects the renderer-neutral `FixturesLooksShellModel` and invokes the
existing Static Look authoring functions. It opens no hardware or network DMX
output. Its sample project and edits remain in memory.

## Pinned dependency

Use Slint **1.17.1 exactly**. The C++ integration requires C++20 and CMake 3.21
or newer. Obtain the official C++ binary package from the Slint 1.17.1 GitHub
release, then point CMake at its installation prefix. Do not silently float the
version: the generated header and linked runtime must match exactly.

Before redistribution, retain the package's license notices and resolve the
project's Slint license path. This lab does not constitute a licensing decision.

## Windows x64 lab build

From PowerShell with Visual Studio 2022 and CMake on `PATH`:

```powershell
./native-core/slint-lab/build_windows_lab.ps1 -SlintPrefix C:/path/to/slint-1.17.1
```

The script configures the opt-in target, builds it, runs its headless model
smoke, and creates
`build/slint-fixtures-looks-lab/EmberLights-Fixtures-Looks-Slint-Lab-win-x64.zip`.
That ZIP is a lab artifact, not an installer and not the product preview line.

The path-scoped pull-request/manual GitHub workflow
`slint-fixtures-looks-lab.yml` performs the same route on Windows 2022. It
downloads the official 1.17.1 MSVC x64 package, requires its pinned SHA-256,
and retains the lab ZIP for 14 days. It does not publish a Release or change
the product's latest-installer pointer.

## Source and compiler checks

```bash
cd native-core
make slint-lab-source-check
SLINT_COMPILER=/path/to/slint-compiler make slint-lab-source-check
```

The second form additionally compiles the markup and rejects any compiler
version other than 1.17.1.

## Remaining acceptance evidence

The toolkit gate remains open until the Windows/MSVC lab records:

- 1366x768 and 1920x1080 screenshots at 100%, 125%, 150%, and 200% scaling;
- keyboard traversal, focus visibility, UI Automation, and Narrator results;
- cold start, idle memory, resize/repaint, list scaling, and software renderer;
- adapter coverage for durable project save, physical preview, undo/history,
  validation errors, read-only state, and Runner/Live coexistence;
- installer/runtime-size delta and the required license/attribution decision;
- a measured comparison against the Direct2D Safe baseline and the WinUI lab.
