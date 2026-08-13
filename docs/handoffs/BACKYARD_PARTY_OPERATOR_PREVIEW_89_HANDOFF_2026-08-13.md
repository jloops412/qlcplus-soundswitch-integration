# Backyard-party operator preview 89 handoff — 2026-08-13

Status: source-complete and locally contract-tested. Windows installer `0.1.0-preview.89.0` was built from clean source `faf4adebd25af70633b80b9f7affa9d97ac9793f`.

## Primary outcome

This preview replaces the fixture-profile raw channel text area with a structured, selectable channel workbench and safe common templates. White/Amber correction is now an explicit final channel assignment, not a blind toggle.

## What changed

- Channel mappings are presented as CH / Property / Encoding / DMX range / Default / Fine columns.
- Selecting a row loads its structured editor fields; Add/Replace and Remove update the validated draft.
- Six common safe templates can replace a channel map without writing encoded parameter rows.
- White and Amber selectors state the desired final physical assignment. Reapplying the active state changes nothing.
- Immutable built-in/imported corrections still fork a Local snapshot and atomically rebind patched fixtures; Local profiles update in place.
- The backend profile editor model is toolkit-neutral for the future skins renderer and Skin Designer.
- Existing QXF import and official OFL search/download remain available and quarantine imported snapshots for manual review.

## Surface-contract reconciliation

- Project schema, compiler, renderer, persistence, and public skin registries: unchanged.
- One transitional authoring action (`Replace Channel Map`) was added to Win32 and recorded in the bypass ledger.
- The existing White/Amber correction surface changed from relative-toggle presentation to absolute final assignment.
- No QLC+, Qt, OFL, or SoundSwitch UI source/artwork was copied.

## Verified before packaging

- warnings-as-errors native build: passed;
- portable CTest suite: 32/32 passed;
- focused profile editor and fixture/catalog/static-look regressions: passed;
- Windows x64 Zig/Clang warnings-as-errors GUI and package targets: passed;
- `git diff --check`: passed.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.89.0-Setup.exe`
- Size: `1,789,177` bytes
- SHA-256: `fd79693bbb930a79e63898aa9622b8be9aa40cb2060e957cbbf48a6c2a072692`
- Payload manifest SHA-256: `64b34649c56f0cd7247d519bf69107098b6d72bfbbff9a16138cb20cf798cd11`
- Source: `faf4adebd25af70633b80b9f7affa9d97ac9793f`
- Version: `0.1.0-preview.89.0`
- Evidence: 17/17 package-contract regressions; exact 18-file payload manifest; repeated byte-identical NSIS compilation from the verified stage; 7-Zip archive test/extraction; exact 19-file staged/extracted payload comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Native Windows clean/upgrade install, launch, Installed Apps uninstall, shortcut/registry cleanup, project/settings preservation, GUI/DPI/accessibility behavior, and physical hardware remain tester gates.

## First tester route

1. Open Studio → Fixture Profiles and select the exact profile shown in Fixture Patch for an IR-4.
2. Confirm the raw encoded text area is gone and the table clearly shows each channel/property mapping.
3. Select White and Amber rows and confirm the structured fields load without typing parameter strings.
4. For a disposable new Local profile, apply RGBWA+UV 6CH and confirm CH4 is White and CH5 is Amber; save and reopen it.
5. On the profile actually patched to the physical IR-4, set the two final assignment selectors to what the fixture produces, apply once, and review every affected fixture in the confirmation.
6. Apply the same final assignment a second time. Confirm EmberLights reports that nothing changed rather than swapping it back.
7. In Static Looks while normal Live is stopped, preview isolated White and isolated Amber at low output and record physical results plus the fixture display mode.
8. Search OFL by exact manufacturer/model. Import only an exact result; confirm it remains read-only/unreviewed and can be duplicated for local editing.

## Remaining boundaries

- physical IR-4 6CH/10CH evidence and mode verification;
- capability-range editor for multi-function, strobe/shutter, mover, and pixel channels;
- pinned offline fixture corpus and richer manufacturer/model search/filtering;
- registered Studio authoring commands/state replacing transitional Win32 callbacks;
- production toolkit/skins runtime, fully responsive Studio layouts, accessibility evidence, and native Windows lifecycle qualification.
