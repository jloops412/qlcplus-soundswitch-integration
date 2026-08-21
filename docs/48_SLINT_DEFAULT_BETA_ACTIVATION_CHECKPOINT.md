# Slint Default beta activation checkpoint

Status: accepted first replacement-shell product slice for the unsigned Windows testing-preview line.

## What changed

D-097 accepts pinned Slint 1.17.1 for the first product-shaped EmberLights replacement-shell journey. On Windows, `EmberLights.exe` now opens the modern Fixtures + Static Looks Studio surface. The earlier Win32 application is still installed as `EmberLights-Safe.exe` and is reachable through **Compatibility Tools** in the new header and through the Start menu.

This is a deliberate staged replacement, not a claim that the new shell already contains the whole workstation.

## Accepted journey

The default shell provides:

- New, Open, Save, and Save Project As for `.emberlights` projects;
- durable atomic save plus the existing project recovery boundary;
- bounded in-session Undo/Redo;
- fixture-profile search and profile selection;
- patched fixture/group targeting;
- Static Look creation, duplication, editing, and commit;
- the complete profile-derived Fixture Control catalog grouped into semantic parameter families;
- direct color emitters including RGBWAUV, Pan/Tilt, continuous controls, named choices, coverage/safety state, and `Release` / `Set` / `Force zero` ownership;
- output-free simulation through the production renderer; and
- the existing bounded selected-target physical preview only when the process is explicitly launched with a project and that authority is explicitly armed.

The Preview 108 slice also moved the application-owned OS2L listener into the
default shell. Its header reports whether VirtualDJ is disabled, listening,
connected, or faulted, plus the configured address and port. The listener
remains output-safe and publishes authoritative Blackout.

Preview 109 adds the first bounded modern Live operator strip under D-098. A
saved, validated project can start and stop the production Runner without
leaving the default shell. The persistent strip reports Runner state, sync and
BPM, U1/U2 output health, active content, and override count. It routes
Blackout, Work Light, Release Overrides, and selected-Static-Look take/release
through the canonical `UiCommandFacade`; the Slint adapter does not own output
or duplicate command semantics. Starting Live persists the existing
last-known-good activation snapshot. Stopping Live retains Runner's terminal
blackout behavior.

While Live owns output, Studio mutations and preview are locked. The operator
may select a saved Static Look and take or release it over the advancing lower
Autoloop/script layer.

Preview 110 adds the bounded Autoloop Live surface under D-099. The default
shell projects both format-1 and persisted Autoloops V2 through the existing
toolkit-neutral `LiveViewModel`, including stable bank/slot identity, content
name, V2 provenance/evidence label, repeat mode, active progress, and completed
cycles. The operator can page the full 64-bank workspace, select one of 32
slots in a bank, launch the selected populated slot, move Previous/Next through
enabled banks, clear active Autoloop playback, and choose all-bank or
selected-bank navigation. Every runtime action uses the existing canonical
`UiCommandFacade`; the UI adds no playback engine, importer, or output path.

This advances the narrow #33/#87 beta journey. It is not a claim that
Autoloop authoring/placement, connection editing, migration review, or full
Live parity have crossed into the replacement shell.

The workspace chrome now separates project/runtime status from navigation and
actions, uses narrower sidebars, and supports a 1024×640 minimum window while
retaining 1366×768 as the preferred operator canvas.

Raw DMX remains an Advanced diagnostic. No renderer owns output, safety, fixture semantics, project persistence, or command authority.

## Compatibility Tools bridge

The frozen Win32 application remains the compatibility route for Connections,
migration, Autoloop authoring/placement, AutoScript, advanced Live operation,
Diagnostics, hardware qualification, and the other workflows not yet accepted
in the replacement shell. It receives only safety, defect, accessibility, and
bridge-removal changes under D-091.

The bridge is an explicit workspace handoff rather than a second concurrent
application. The default shell first stops its active Runner and OS2L service, launches
`EmberLights-Safe.exe` with the current project when available, then exits only
after launch succeeds. If launch fails, the default shell restores its OS2L
listener and remains open. Unsaved edits and active previews continue to block
the handoff; an active modern Live session is stopped through the canonical
show command before the bridge launches.

The installer carries both executables. Normal application launch and `.emberlights` file association select the replacement shell; the IR-4 hardware bench shortcut deliberately selects Safe / Live. Removing the bridge requires each remaining workflow to cross its own product and evidence gate.

## Runtime and licensing

The source and package require Slint 1.17.1 exactly. The default shell visibly displays **Powered by Slint 1.17.1**. The installer includes `slint_cpp.dll`, the complete upstream Slint license, and its third-party notices. The package contract rejects a payload missing any of these files.

## Evidence boundary

The non-Windows builder may prove source identity, warning-fatal cross-compilation, model and package regressions, PE x64/runtime closure, deterministic NSIS construction, and exact archive extraction. It cannot prove native installed-Windows behavior, GPU/rendering compatibility, Windows DPI, keyboard navigation, Narrator/accessibility, physical fixture response, Authenticode, soak, or gig readiness.

Every such artifact must be labeled **contract-tested non-Windows unsigned testing preview** until those Windows and physical gates are run and recorded.
