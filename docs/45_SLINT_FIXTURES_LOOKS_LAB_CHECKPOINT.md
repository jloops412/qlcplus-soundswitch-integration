# Slint Fixtures + Static Looks lab checkpoint

Date: 2026-08-14
Status: source-green issue #37 implementation checkpoint; not a product preview,
toolkit decision, installer, or hardware qualification.

## Outcome

The first replacement-shell slice now exists as working source rather than
another Win32 property form or a decorative mockup. It combines fixture profile
provenance, patch/group targets, Static Look selection, profile-backed visual
controls, explicit ownership, validation, and Advanced diagnostics in one
product-shaped Slint surface. The second source pass removes the lab's original
Intensity/RGBWA/Pan/Tilt/Focus allowlist: category navigation, bounded search,
and every ordinary control now project from the selected target's complete
profile-backed catalog under D-095. The third source pass applies D-096: all
direct values, continuous named ranges, exact selector choices, ownership,
coverage, mixed state, availability, and safety for one semantic property now
render in one stable parameter-family card instead of disconnected value,
ownership, and choice sections.

The legacy Win32 presentation remains frozen under D-091. The lab is opt-in,
is not installed by CMake, does not replace `EmberLights.exe`, opens no output
by default, and defaults to an unsaved sample. Passing `--project <file>` opens an existing
project or assigns a path to the sample so the lab can exercise the existing
atomic Studio save/history path. D-092 pins Slint 1.17.1 for this experiment
only. D-094 additionally permits an explicit
`--project <file> --allow-physical-preview` process to exercise the existing bounded authority;
the project must still configure an output and Live must remain stopped.

## Architecture

| Layer | New artifact | Authority |
| --- | --- | --- |
| Renderer-neutral composition | `FixturesLooksShellModel` v3 | Joins existing profile, patch, Static Look, validation, fixture-function, and control-surface snapshots; projects stable category navigation, bounded parameter search, semantic parameter-family cards, exact bindings, counts, control kinds, coverage, safety, values, and ownership without a renderer allowlist |
| Domain mutation | Existing Static Look authoring functions and `StudioDocumentService` | Applies/removes properties and exact profile choices, then commits one generation-checked Undo transaction; Slint owns no lighting semantics or document state |
| Lab presentation | `fixtures_looks_lab.slint` | Layout, styling, accessibility declarations, task controls, responsive center workspace, and Advanced drawer |
| Renderer adapter | `fixtures_looks_lab.cpp` | Projects immutable model snapshots, forwards typed user intent, and refreshes the lab |
| Preview command boundary | Registry `1.9.0` (preview contract retained from `1.8.0`), `UiCommandFacade`, and `StaticLookPreviewCoordinator` | Runs compilation/output work off the UI thread, coalesces draft updates, publishes one coherent status snapshot, and excludes preview from MIDI/keyboard/Actions |
| Physical authority | Existing `StaticLookPhysicalPreviewService` | Owns the production-Runner lease, selected-target isolation, 35% cap, hazard rejection, 30-second timeout, faults, and terminal blackout; the renderer cannot replace it |
| Build boundary | `EMBERLIGHTS_BUILD_SLINT_LAB=OFF` by default | Finds Slint 1.17.1 exactly and builds a separate non-installed executable only when requested |

The shell model intentionally contains no Slint type. A future accepted Default,
Safe surface, Reference skin, test harness, or alternate renderer can project
the same stable state and invoke the same domain path.

## Implemented workflow

The lab currently supports:

1. fixture-profile browsing with maker/model/mode, footprint, source, and revision;
2. profile search plus fixture/group patch-target selection;
3. Static Look search, selection, draft creation, and duplication with guarded
   selection changes;
4. profile-derived Intensity, Color, Position, Beam, Image, Effect,
   Atmosphere, and Advanced-only Custom category navigation with stable IDs and
   bounded cross-field search;
5. one property-family card per stable semantic parameter, keeping its direct
   value, continuous ranges, exact choices, ownership, coverage, mixed state,
   availability, and safety together; dynamic faders remain available for every
   direct or continuous profile parameter, alongside a
   horizontally scalable emitter mixer for RGBWAUV/CMY/Lime/Indigo, and a
   Pan/Tilt XY pad when both axes are present;
6. exact profile function/range choices plus explicit `Release`, `Set`, and
   `Force zero` ownership for both continuous and selector properties;
7. position-aware authoring for continuous named ranges and exact per-profile
   value preservation for selector choices, without raw DMX entering the Look;
8. target coverage, unavailable/mixed state, validation, read-only, stale, and
   explicit empty-state projection in the renderer-neutral model;
9. asynchronous offline **Simulate** with frame digest and no adapter output;
10. explicitly armed **Preview fixtures** with visible mode, lifecycle, cap,
   countdown, fixture count, exact fault token, coalesced edits, and one
   **Blackout & stop** exit;
11. raw channel/range evidence and unclassified controls only behind explicit
    Advanced disclosure;
12. persistent stable choice IDs from the same catalog used by Static Looks,
    Live, Autoloops, mappings, Ember Actions, migration, and future skins;
13. `Save Look` as one generation-checked Studio Undo transaction, draft-aware
    Undo/Redo controls, and visible generation/history counts;
14. `Save Project` through `save_project_atomic`, followed by an exact-generation
    durable-baseline acknowledgement only after the write succeeds.

The renderer receives booleans, labels, immutable projections, and callbacks.
It never owns a `ProjectDocument`, filesystem path, history stack, Runner,
scheduler, or output backend. A default launch remains an unsaved sample and
honestly disables `Save Project`; `--project <file>` enables the bounded durable
workflow without adding a file dialog to the toolkit evaluation.

## Local verification

Passed on the source host:

- warning-fatal native compile and `fixtures_looks_shell_tests`;
- control-surface v2 and shell-model v3 tests for stable property-family IDs,
  multi-choice grouping, mixed continuous/selector families, category IDs,
  profile-parameter search, complete control reachability, and named-choice/
  direct-attribute distinction;
- warning-fatal `static_look_preview_coordinator_tests` covering asynchronous
  simulation/update/stop, latest-wins edits, explicit physical arming,
  cap/countdown, timeout/restart isolation, and stop-to-black Runner release;
- registry `1.9.0` generation/check, V1 compatible-additive diff, facade tests,
  and Ember Action exclusion tests;
- `test_ui_direction.py`;
- all eight `test_slint_lab_contract.py` checks;
- warning-free Slint markup compilation with the SHA-256-verified official
  `slint-compiler 1.17.1` and embedded Fluent resources;
- warning-fatal C++20 compilation of the nested-family adapter against the
  generated 1.17.1 header and SHA-256-verified official C++ headers;
- preview telemetry polling no longer rebuilds the profile/library/control
  projection at 10 Hz when its coherent status token has not changed.

The host lacks Slint's Linux `libinput.so.10` runtime dependency, so a linked
Linux window/model-smoke executable was not claimed. That does not substitute
for the required Windows/MSVC evidence. The checked-in PowerShell route builds
the separate Windows x64 lab, runs `--model-smoke`, copies its runtime/license
files, and creates an explicitly labeled ZIP once run on the target host. A
path-scoped pull-request/manual Windows 2022 GitHub workflow downloads the
official package only after checking its pinned SHA-256, then performs that
same route without publishing a Release or moving the product installer
pointer.

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
- Windows evidence for Studio save/history/undo plus complete validation/fault
  UX, read-only behavior, and the now-wired bounded physical preview while Live
  is stopped, including timeout/fault terminal blackout;
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

Run and capture Windows evidence for the now-wired profile-driven parameter
deck, Studio document/history, and bounded-preview paths, including long
fixture profiles, every category, search, mixed groups, default output lock,
explicit arming, edit coalescing, timeout/fault, and terminal blackout. Build
the WinUI comparison from the same model/query/preview command fixture so the
toolkit decision is measured on equal workflows.
