# Backyard-party operator preview 87 handoff — 2026-08-13

Status: source-complete and locally contract-tested. Windows installer `0.1.0-preview.87.0` was built from clean source `bcd3204caa41733f9f12bc8ac33f2fa6f13700bc`.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.87.0-Setup.exe`
- Size: `1,769,564` bytes
- SHA-256: `b65026dad75ba2b688686d2b91e8ace14175f0f9726d1c871f02cfea2aaf341d`
- Payload manifest SHA-256: `2423f6f582b459f15b89763ec885b5acf9465ecfa3bd75b677578d6981f1dd6e`
- Source: `bcd3204caa41733f9f12bc8ac33f2fa6f13700bc`
- Version: `0.1.0-preview.87.0`
- Evidence: 17/17 package-contract regressions; warnings-as-errors x64 Windows target build; exact 18-file payload manifest; package verification; byte-identical repeated NSIS compilation; 7-Zip archive test/extraction; exact 19-file staged/extracted byte comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Native Windows install/upgrade/launch/uninstall, project/settings preservation, and physical hardware behavior remain tester gates.

## Primary outcome

This preview turns the fixture/Profile → Static Look → simple Autoloop → Live path into a usable hardware-feedback loop without representing the build as gig-qualified or parity-complete.

## What changed

### Fixture profiles and catalog

- Profiles can search the official Open Fixture Library from inside Studio, select an exact fixture, and download/import its QLC+ export.
- Search/download run on a bounded authoring worker and never enter Runner or the DMX scheduler.
- Accepted modes remain immutable and unreviewed and retain the OFL key, source/download URLs, MIT attribution, exact QXF SHA-256, and adapter version.
- Unsafe/malformed modes still pass through the existing quarantine and project-validation boundary.
- An absent exact match stays absent. OFL currently returns no exact BO-IR4/IR-4 result; Chauvet DJ WashFX is not a substitute for Wash FX Hex.
- Local profile channel management now has structured add/replace and remove actions alongside the ordered mapping summary.

### White / Amber physical correction

- **Correct Physical White / Amber...** is now a reviewed project transaction, not an editor-draft swap.
- The dialog shows exact before/after channels and the number of affected patched fixtures.
- Imported/built-in sources remain immutable; EmberLights creates one corrected Local snapshot and atomically rebinds every fixture using the source.
- A Local source updates in place.
- The source/profile/fixture set is stale-checked, the candidate validates and compiles, and the document changes once for one Undo.
- Compiler/renderer regressions prove White and Amber follow the active profile offsets. There is no global application inversion; physical mode/manual truth remains an observation gate.

### Static Look hardware preview

- While normal Live is stopped, **Preview Selected Target on Fixtures** leases the production Runner for a selected fixture/group.
- It retains only the selected target, zeros profile defaults, removes OS2L/MIDI/Autoloops/Track Scripts, caps direct emitters/master at 35%, and refuses positive hazard/custom output and unsafe nonzero constant channels.
- Applying Full Color, swatches, explicit properties, or removal updates the physical target in realtime through immutable package activation without extending the deadline.
- The preview starts black, lasts at most 30 seconds, and stops through Runner's terminal blackout frames on Stop, timeout, page departure, rejected update, Runner/output fault, or destruction.
- Live visibly says **STUDIO HARDWARE PREVIEW** and disables normal override/bank operations while the lease is active.
- This is visual authoring feedback, not fixture qualification; Raw Hardware Test remains the qualification authority.

### Incremental operator UX

- Static Look ownership is labeled as **Set value (included)**, **Hard off (included at zero)**, and **Follow lower content (release)**.
- Static Look target selection defaults to a patched target and saved Looks reopen on their first assigned fixture.
- Live Overrides adds a 0–100 slider plus 0/25/50/100 presets.
- Autoloop quick authoring adds **Remove Last Step** and **Clear Draft Steps**.
- Default 2.0 Live/Studio/Setup separation and the Ember-dark Win32 bridge remain; this is still not the `.emberskin` runtime or Skin Designer.

## Verified locally

- Native warnings-as-errors build: passed.
- Portable CTest suite: 30/30 passed.
- UI registry lifecycle tests: 12/12 passed; generated registry digest unchanged and direct callbacks remain reconciled under the existing Authoring bypass area.
- Windows x64 warnings-as-errors cross-build: `EmberLights.exe` linked successfully with WinHTTP.
- `core_tests`: passed, including atomic White/Amber rebind and exact rendered slots.
- `static_look_physical_preview_tests`: passed, including isolation, cap, Live interlock, realtime update, timeout, output fault, and terminal stop.
- `ofl_fixture_catalog_tests`: passed, including bounded search/download parsing, provenance, quarantine, and save/reopen evidence.
- `git diff --check`: passed.

No native Windows GUI, installed lifecycle, owned SoundSwitch Micro output, or physical IR-4 observation was available on the build host. Those remain tester gates.

## First tester route

1. Install preview 87 over the prior test build. Confirm the existing project reopens and projects/settings remain intact.
2. In **Studio → Fixture Profiles**, select the profile actually used in Patch and read the ordered mapping summary.
3. If White and Amber are physically reversed, choose **Correct Physical White / Amber...**, review the affected fixtures, apply, then save. Do not manually repatch; the transaction rebinds them.
4. For unrelated fixtures, search the official catalog by exact manufacturer/model. Import only an exact match, then verify the selected mode and every channel against the manufacturer manual.
5. Configure the intended output in Connections and save it, but keep normal Live stopped.
6. In Static Looks select one IR-4, apply **White**, start the bounded physical preview, observe, press **STOP PREVIEW**, then repeat with **Amber**. Record fixture display mode, address, profile ID, and physical emitter for each.
7. Build two or three basic Looks. While preview is active, change swatches/properties and confirm the fixture follows in realtime and unselected fixtures remain black.
8. Build a four-beat Autoloop from those saved Looks; use Remove Last/Clear Draft if needed, then save.
9. Start normal Live, launch the Look/Autoloop, use the Override slider/presets, release the property, and Release All.
10. Run **File → Review Current SoundSwitch Migration** against the exact source before trusting migrated areas; hashes prove source identity only.

## Remaining boundaries

- installed Windows clean/upgrade/launch/uninstall and DPI/accessibility evidence;
- physical White/Amber truth on both owned IR-4 fixtures and exact display modes;
- pinned offline OFL corpus/direct richer transformer and multi-cell/mover/pixel semantics;
- full row/table profile function/range editor and qualification invalidation;
- V2 Autoloop grid/timeline and broader effects/director authoring;
- actual `.emberskin` runtime, Safe fallback, Reference package, overlays, and Designer;
- reproducible broader SoundSwitch decoding and full parity evidence.
