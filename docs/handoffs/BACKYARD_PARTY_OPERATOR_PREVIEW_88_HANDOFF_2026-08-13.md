# Backyard-party operator preview 88 handoff — 2026-08-13

Status: source-complete and locally contract-tested. Windows installer `0.1.0-preview.88.0` was built from clean source `7feff7b63d4bfe6ce2f44bd07f78ee4dc0acfa84`.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.88.0-Setup.exe`
- Size: `1,778,531` bytes
- SHA-256: `3f3eac1b3a0e3586be1716f1e1ad1b8d279e34961d73f41010098452dd42d53f`
- Payload manifest SHA-256: `49f776c4c203a8d3a1d0f353485e6d829ed609e8a050277d5ea31b33ca9e03e1`
- Source: `7feff7b63d4bfe6ce2f44bd07f78ee4dc0acfa84`
- Version: `0.1.0-preview.88.0`
- Evidence: 17/17 package-contract regressions; warnings-as-errors x64 Windows target build; exact 18-file payload manifest; package verification; byte-identical repeated NSIS compilation; 7-Zip archive test/extraction; exact 19-file staged/extracted byte comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Native Windows install/upgrade/launch/uninstall, project/settings preservation, and physical hardware behavior remain tester gates.

## Primary outcome

This preview makes the existing Studio/Live/Setup build materially easier to read under pressure while establishing a reusable visual contract for the next production-toolkit spike. Lighting behavior, fixture/profile logic, Static Look preview, Autoloops, Live overrides, migration, and the project schema are unchanged from preview 87.

## What changed

### Default 2.1 operator shell

- Every workspace now retains a compact gig-health strip for Project save state, Runner/Studio preview and active content, sync/BPM, output readiness, override count, and safety state.
- Persistent Start/Stop and Blackout actions remain isolated at the right edge and invoke the existing registered commands rather than parallel callbacks.
- Live, Studio, and Setup navigation now uses Windows-supplied Segoe Fluent icon glyphs with a Segoe MDL2 fallback and permanent text labels.
- Selected workspace, selected page, Runner-active content, keyboard focus, warnings, primary actions, destructive actions, and Blackout have distinct visual treatments.
- Page content sits on a raised card surface; classic sunken list/edit chrome and list-view grid walls were removed.
- List rows are taller and show keyboard selection separately from the Look, Autoloop, or Track Script currently active in Runner.
- The outer shell now has tested Compact, Standard, and Wide geometry for navigation, health, page, and status areas.

### Reusable resource path

- `emberlights/ui_visual.hpp` is a toolkit-neutral bridge for the 27 Ember Dark semantic tokens, stable navigation metadata, Fluent glyph assignments, health tones, and shell geometry.
- Slint 1.17.1 remains the first production-shaped issue #37 spike candidate; it is not linked or shipped here.
- Microsoft Fluent UI System Icons is the qualified future MIT vector source. This preview uses only the Windows-supplied Segoe fonts and bundles no font or repository SVG.
- QLC+ remains an Apache-2.0 workflow and selective audited-source reference. No QLC+, Qt, or SoundSwitch source, artwork, font, icon, screenshot, or trade dress was copied.

## Surface-contract reconciliation

- Commands/states/capabilities/components added, changed, deprecated, or removed: none.
- Project schema, generation, persistence, mappings, migration, Runner arbitration, and DMX behavior: unchanged.
- Direct callback bypasses: zero added and zero removed.
- UI registry generation and digest: unchanged.
- Compatibility: additive presentation-only checkpoint over preview 87.

## Verified locally

- Native warnings-as-errors build: passed.
- Portable CTest suite: 31/31 passed.
- `ui_visual_tests`: passed for token identity, navigation/accessibility metadata, status tones, and Compact/Standard/Wide layout.
- UI registry lifecycle tests: 12/12 passed; generated registry and Ember Action adapter checks passed with the existing digest unchanged.
- Windows x64 Zig/Clang warnings-as-errors build: `EmberLights.exe` and `ui_visual_tests` compiled and linked successfully as PE32+ targets.
- `git diff --check`: passed.

No native Windows GUI, installed lifecycle, DPI/Narrator/UI Automation inspection, or physical lighting hardware was available on the build host. Those remain tester gates.

## First tester route

1. Install preview 88 over preview 87 and confirm the existing project reopens unchanged.
2. Resize the window down to its minimum and back to a wide desktop size. Confirm health cards, Start/Stop, Blackout, navigation, page content, and the bottom status line remain usable.
3. Switch repeatedly among Live, Studio, and Setup. Confirm each workspace remembers its last page and that every icon retains a readable text label.
4. In Live, select one Static Look while another is active. Confirm blue selection and orange Runner-active feedback remain distinguishable.
5. Repeat with an Autoloop and Track Script. Confirm active feedback changes without flicker and clears when content stops.
6. Start/stop both a normal show and a bounded Studio fixture preview from the persistent top action. Confirm its label and Runner health card track the actual state.
7. Toggle Blackout, Work Light, one hazard arm, and a Live override. Confirm the Safety and Overrides cards communicate each state without relying on color alone.
8. Check Windows display scaling at 100%, 125%, and 150%, plus keyboard Tab/focus navigation. Record any clipped control, unreadable focus ring, font fallback, or card overlap with the page and scaling level.

## Remaining boundaries

- production-shaped Slint spike and measured toolkit acceptance for issue #37;
- actual `.emberskin` runtime, Default/Reference/Safe packages, overlays, and Skin Designer;
- fully responsive dense Studio pages and reusable table/range/effect editors;
- native Windows DPI/accessibility evidence and clean/upgrade/uninstall lifecycle;
- broader fixture, mover, pixel, SoundSwitch parity, hardware, and live-event qualification gates already tracked by continuity.
