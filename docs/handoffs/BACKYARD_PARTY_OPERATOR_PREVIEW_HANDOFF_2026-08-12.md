# Backyard-party operator preview handoff — 2026-08-12

Status: source-complete and locally contract-tested. Windows installer `0.1.0-preview.86.0` was built from clean source `4f251b717ccc3d248814c15f11dfdabf71fc786a`.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.86.0-Setup.exe`
- Size: `1,712,108` bytes
- SHA-256: `744de642990fba3ecb22e9f6bebc0ac4c8bbd73df5d1ad98e666f4bfb45fd087`
- Payload manifest SHA-256: `f712a73242a96eb7c2e3d553c771ab53f4286ca06251197471b97e654aafc7fd`
- Source: `4f251b717ccc3d248814c15f11dfdabf71fc786a`
- Version: `0.1.0-preview.86.0`
- Evidence: 17/17 package-contract regressions; warnings-as-errors x64 Windows target build; exact 18-file payload manifest; package verification; byte-identical repeated NSIS compilation; 7-Zip archive test/extraction; exact 19-file staged/extracted byte comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Native Windows install/upgrade/launch/uninstall, layout/accessibility, project/settings preservation, and physical hardware behavior remain deferred to the tester.

The packaging route was also captured as the installed personal skill `build-emberlights-windows-installer`, including dry-run preflight, guarded staging, exact target selection, manifest checks, reproducible NSIS build, and independent archive verification.

## Primary outcome

Deliver a Windows testing preview that is materially easier to operate for a basic wash-light rig: manage and correct fixture profiles, build Static Looks, assemble simple Autoloops, and launch/override content from a clearly separated Live workspace. Preserve every existing Runner/safety/output boundary.

## Implemented slice

### Default 2.0 bridge

- explicit **Live**, **Studio**, and **Setup** workspace switcher with last-page memory;
- focused per-workspace navigation instead of one thirteen-item legacy form rail;
- Ember-dark application/chrome/panel/control tokens, modern title frame where Windows supports it, larger Segoe UI Variable typography, rounded owner-drawn buttons, semantic primary/danger states, and physical-emitter swatches;
- Live double-click launch for Static Looks, Autoloops, and Track Scripts;
- the shell identifies itself as `DEFAULT 2.0 PREVIEW • EMBER DARK`.

This remains the trusted Win32 strangler view. It does not load `.emberskin` packages, switch skins, provide Safe fallback qualification, or implement the Skin Designer.

### Fixture/profile management

- profile list identifies `VERIFIED BUILT-IN`, `IMPORTED / REVIEW`, and `LOCAL` sources plus footprint;
- ordered human-readable mapping summary reports validation, channel/fine-channel, property, encoding, range, default, and White/Amber positions;
- structured mapping assistant adds or replaces a channel using channel/property/encoding/fine/min/max/default fields while retaining the advanced row editor;
- Patch profile choices include footprint and exact W/A channel positions;
- `Restore Verified IR-4 6CH + 10CH` safely appends the immutable manual-backed definitions to older/imported projects without overwriting conflicting user data;
- guarded `Swap White ↔ Amber Labels` works only on a Local profile, refuses missing/ambiguous/invalid/read-only profiles, confirms the physical symptom, and changes no offsets/ranges/defaults/unrelated mappings.

Manual-backed IR-4 truth retained by tests:

| Physical mode | White | Amber |
| --- | ---: | ---: |
| 6CH | CH4 | CH5 |
| 10CH | CH5 | CH6 |

The user's report—White UI producing amber and Amber UI producing white—is consistent with reversed semantic labels in the selected project profile. It is not yet a physically observed EmberLights result. Duplicate the active profile if it is imported, run the correction, save, repatch if needed, then test White and Amber separately at low intensity.

### Static Looks, Autoloops, and Live

- existing capability-aware RGBWAUV/Master Look authoring and exact output-disabled DMX preview remain intact and are presented inside the Studio workspace;
- pure White and Amber swatches remain independent direct emitters;
- Autoloops now has a quick-step builder: choose a saved Static Look, enter a beat, choose Cut/Linear, add, and let Studio order the rows;
- raw `beat, look-id, cut|linear` editing remains available for advanced/recovery use;
- Live keeps Runner-owned Toggle/Clear, Autoloop navigation/bank filters, overrides, hazards, blackout, work light, BPM, and metrics under the existing typed command facade.

### SoundSwitch migration

- File > Import SoundSwitch Project (V1 Preview) exposes the existing qualified 2.10.x color-rig converter;
- import creates a separate `.emberlights` project, displays all approximation warnings before saving, leaves every output disabled, and opens Fixture Profiles first;
- File > Review Current SoundSwitch Migration inspects the selected source read-only and reports source identity, project validation, output safety, and seven areas: profiles, patch, Looks, Autoloops, scripted tracks, audio, and MIDI;
- exact hashes establish identity only. The converter remains narrow; mover/pixel/script binary semantics and general foreign rigs are not claimed.

## Verification reproduced locally

- Windows x64 warnings-as-errors cross-build: `EmberLights.exe` linked successfully as PE32+ GUI.
- `core_tests`: passed.
- `live_ui_tests`: passed.
- `studio_foundation_tests`: passed from its required working directory.
- `static_look_authoring_tests`: passed, including IR-4 frames.
- `studio_authoring_tests`: passed.
- `autoloop_v2_studio_tests`: passed.
- `autoloop_autoscript_workflow_tests`: passed.
- Windows package-contract regression: 17/17 passed.
- `emberlights_migrate --help`: passed with migration-review contract text.
- `git diff --check`: passed.

No Wine/native Windows GUI runtime was available on the build host. Installed launch/layout/accessibility, clean install/upgrade/uninstall, owned Micro output, and physical IR-4 observations remain tester gates.

## First tester route

1. Open the existing project; go to **Studio > Fixture Profiles**.
2. Select the profile actually used in **Fixture Patch** and read its W/A positions.
3. If it is imported/read-only, Duplicate it. If White/Amber labels are reversed, use the guarded swap and Save Profile.
4. Reassign patched IR-4 fixtures to the corrected Local or verified built-in 6CH/10CH profile. Confirm the fixture display mode matches.
5. With safe output/rehearsal conditions, test Blackout, White only, Blackout, Amber only, Blackout. Record which physical emitter responds.
6. Build and save two or three Static Looks. Use exact offline preview before output.
7. In Autoloops, use Quick step builder to sequence those Looks over four beats; save it.
8. Start Show in Live, launch by double-click/button, test Clear, then verify a transient Fixture Override and Release All.
9. For migrated projects, run **File > Review Current SoundSwitch Migration** against the exact source and keep every area marked approximate/missing in the test notes.

## Explicit next work

- convert the profile mapping assistant into a row/table editor with insert/delete/reorder, named functions/ranges, import-side mode selection, and immutable evidence/revision qualification;
- finish the general fixture catalog/OFL adapter and multi-cell/mover/pixel profile model;
- replace the quick Autoloop bridge with the V2 grid/timeline/canvas and broader director/property support;
- implement/qualify the actual `.emberskin` runtime, Safe fallback, Default/Reference packages, overlays, and Designer;
- expand SoundSwitch decoding only from reproducible source evidence, never inferred binary meanings;
- run native Windows lifecycle, DPI/accessibility, hardware, reconnect, blackout, and soak evidence before any gig-qualified claim.
