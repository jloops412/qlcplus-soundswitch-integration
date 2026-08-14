# Slint Fixtures + Static Looks lab checkpoint

Date: 2026-08-14
Status: source-green issue #37 implementation checkpoint; not a product preview,
toolkit decision, installer, or hardware qualification.

## Outcome

The first replacement-shell slice now exists as working source rather than
another Win32 property form or a decorative mockup. It combines fixture profile
provenance, patch/group targets, Static Look selection, profile-backed visual
controls, explicit ownership, validation, and Advanced diagnostics in one
product-shaped Slint surface.

The legacy Win32 presentation remains frozen under D-091. The lab is opt-in,
is not installed by CMake, does not replace `EmberLights.exe`, opens no output,
and keeps all edits in memory. D-092 pins Slint 1.17.1 for this experiment only.

## Architecture

| Layer | New artifact | Authority |
| --- | --- | --- |
| Renderer-neutral composition | `FixturesLooksShellModel` | Joins existing profile, patch, Static Look, validation, fixture-function, and control-surface snapshots with stable IDs |
| Domain mutation | Existing Static Look authoring functions | Applies/removes properties and exact profile choices; Slint owns no lighting semantics |
| Lab presentation | `fixtures_looks_lab.slint` | Layout, styling, accessibility declarations, task controls, responsive center workspace, and Advanced drawer |
| Renderer adapter | `fixtures_looks_lab.cpp` | Projects immutable model snapshots, forwards typed user intent, and refreshes the lab |
| Build boundary | `EMBERLIGHTS_BUILD_SLINT_LAB=OFF` by default | Finds Slint 1.17.1 exactly and builds a separate non-installed executable only when requested |

The shell model intentionally contains no Slint type. A future accepted Default,
Safe surface, Reference skin, test harness, or alternate renderer can project
the same stable state and invoke the same domain path.

## Implemented workflow

The lab currently supports:

1. fixture-profile browsing with maker/model/mode, footprint, source, and revision;
2. profile search plus fixture/group patch-target selection;
3. Static Look search, selection, creation, and duplication in memory;
4. master intensity with `Release`, `Set`, and `Force zero` ownership;
5. independent RGBWA emitter controls rather than a hard-coded repair action;
6. a Pan/Tilt XY pad, Focus fader, and profile-backed Gobo choice tiles;
7. target coverage, unavailable/mixed state, validation, read-only, stale, and
   explicit empty-state projection in the renderer-neutral model;
8. bounded preview simulation with an explicit start/stop state and no output;
9. raw channel/range evidence only inside the nonmodal Advanced drawer;
10. persistent stable choice IDs from the same catalog used by Static Looks,
    Live, Autoloops, mappings, Ember Actions, migration, and future skins.

The visible Save action deliberately acknowledges only in-memory lab state. It
does not pretend that project persistence, history, or atomic save has been
wired.

## Local verification

Passed on the source host:

- warning-fatal native compile and `fixtures_looks_shell_tests`;
- `test_ui_direction.py`;
- all seven `test_slint_lab_contract.py` checks;
- warning-free Slint markup compilation with official
  `slint-compiler 1.17.1` and embedded Fluent resources;
- warning-fatal C++20 compilation of the adapter against the generated 1.17.1
  header and official C++ headers.

The host lacks Slint's Linux `libinput.so.10` runtime dependency, so a linked
Linux window/model-smoke executable was not claimed. That does not substitute
for the required Windows/MSVC evidence. The checked-in PowerShell route builds
the separate Windows x64 lab, runs `--model-smoke`, copies its runtime/license
files, and creates an explicitly labeled ZIP once run on the target host. A
manual Windows 2022 GitHub workflow downloads the official package only after
checking its pinned SHA-256, then performs that same route without publishing a
Release or moving the product installer pointer.

## Dependency and license record

- exact lab version: Slint 1.17.1;
- language/build floor: C++20 and CMake 3.21 for the opt-in lab;
- official binary-package route retained; generated header/runtime versions
  must match exactly;
- Slint's GPL, royalty-free, and commercial license paths require an explicit
  project decision before redistribution;
- package license and third-party notices must remain with every lab artifact.

No Slint acceptance or redistribution license decision is made here.

## Open acceptance gates

Before Slint can be selected or a product installer can advance, issue #37
still requires:

- Windows/MSVC launch and interaction at 1366x768 and 1920x1080;
- screenshots at 100%, 125%, 150%, and 200% scaling;
- complete keyboard traversal, visible focus, UI Automation, and Narrator;
- cold start, idle memory, resize/repaint, long-list, software-renderer,
  scheduler-jitter, and packaged-size measurements;
- durable Studio project save/history/undo, validation/fault UX, read-only
  behavior, and bounded physical preview while Live is stopped;
- Safe/Runner independence and skin activation/failure continuity;
- measured WinUI 3 control comparison and Direct2D/Win32 Safe baseline;
- explicit dependency/license/attribution and installer servicing decisions;
- issue #38 core/hardware gates before broad product rollout.

## Installer classification

This pass does not advance the product preview line. The code can produce an
explicit `EmberLights-Fixtures-Looks-Slint-Lab-win-x64.zip` for issue #37
evaluation, but it is not a product installer. The next product installer must
activate an accepted replacement-shell slice or remain withheld under D-091.

## Next bounded pass

Run and capture the Windows lab evidence first. Then wire durable Studio
document/history mutation and the existing bounded Static Look physical-preview
authority through typed adapter commands, without giving the renderer direct
filesystem, Runner, scheduler, or output ownership. Build the WinUI comparison
from the same model/query fixture so the toolkit decision is measured on equal
workflows.
