# Backyard-party operator Preview 104 handoff — 2026-08-20

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows lifecycle, VirtualDJ behavior, and
physical hardware evidence remain pending.

## Primary outcome

Preview 104 is the first stacked beta that combines the application-owned OS2L
service from Preview 103 with a product-callable current-SoundSwitch import
slice. OS2L can listen while the show is stopped and remain available across
show stop/start. The installed `Tools/os2l_capture.exe` records ordered socket,
message, and parse evidence for tonight's VirtualDJ diagnosis.

**File → Import SoundSwitch 2026 Project (Output-Disabled)** and the installed
`emberlights_migrate convert-2026-red-smooth` command now accept the exact
reviewed current source identity and create a separate project containing one
evidence-backed Autoloop: `Medium / slot 1 / Red - Smooth Pulse / 8 bars`.
Catalog placement, project-local target IDs, and A/B timeline records are
decoded; unclaimed bytes stay preserved and missing source-local uplight color
stays explicit rather than inventing red.

The generated current-rig destination has four BO-IR4 fixtures at universe 1
addresses `001/011/021/031` and four BO-TUBE192 physical blocks beginning at
`041/121/201/281`. It creates no duplicate generic Both Lighting uplights and
keeps every physical output disabled. Its 18 Static Looks are clearly retained
EmberLights review content, not claimed SoundSwitch imports.

This is not the replacement-shell UI milestone. The existing File command only
adapts the renderer-neutral source proposal and generation-checked Studio
document transaction. The planned Advanced arbitrary DMX channel tester and
modern Fixtures/Static Looks shell are the next product slices, not Preview 104
claims.

## Installer evidence

- Version: `0.1.0-preview.104.0`
- Installer source commit: `9e96c44dff794a16fbdfcb4f5efc68e2f02645fe`
- Installer source tree: `49472da35d84777e405c7ff2cc3005c394a3e104`
- Source branch: `agent/beta-preview-104`
- Draft integration PR: `#95`
- Immutable GitHub artifact branch: `artifacts/windows-preview-104`
- Artifact commit: `9d9ab1da9d3ae8115fc3e2ff0b4b4800bcaf5380`
- GitHub bundle: `EmberLights-0.1.0-preview.104.0-Windows-testing-preview.zip`
- GitHub bundle SHA-256: `0fdb4ef2e84e01111bb1ac1da10336f6b2ec5528297d8cf2377b6500cd4bb6d4`
- GitHub bundle size: `2,043,952` bytes
- Installer: `EmberLights-0.1.0-preview.104.0-Setup.exe`
- Installer SHA-256: `a8e6ec31bcc55231d1c5f52b66f6f3bf50cdd4f3378fa7fa654bbc76e4c6ed08`
- Installer size: `2,062,420` bytes
- Payload manifest: `EmberLights-0.1.0-preview.104.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `9adb9232fd07e4c762ae06afc415bc5a3a38c9f5460d190015c3886889bc91de`
- Checksum evidence: `EmberLights-0.1.0-preview.104.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `5cc2f05c04d45d6404184e858ed13b97a84173f6d5c421a0d2ac205bc03327fd`

## Verification evidence

- warning-fatal native core build, aggregate Make tests, UI-direction and
  registry checks, DMX/WinMM syntax checks, and smoke gate passed on the exact
  source tree;
- the exact authorized current-2026 source corpus passed its proposal,
  translation, output-disabled preview, Studio commit, save/reopen, idempotent
  re-import, and product-project regressions;
- the focused clean CMake/CTest lane passed 3/3;
- Windows package-contract regressions passed 18/18;
- clean Windows x64 Zig/Clang builds passed for the application and every
  explicit supported payload tool;
- the exact 21-file payload plus generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly;
- the GitHub bundle contains only the installer, payload manifest, and checksum
  evidence, and its ZIP integrity test passed.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, accessibility-qualified, physically qualified,
gig-qualified, signed, or a public release.

PR #95's Native core workflow was marked failed before either the Linux or
Windows job created a single step; both installer jobs were consequently
skipped. That matches the repository's existing hosted-runner/infrastructure
gate and is neither product failure evidence nor passing CI. The local and
cross-build evidence above remains the release basis.

## Joshua's test tonight

1. Download the immutable Preview 104 bundle and check its SHA-256 with
   `Get-FileHash`. Extract it and confirm the installer hash matches the value
   above.
2. Close EmberLights and SoundSwitch. Run the unsigned installer on Windows 11
   and allow it to upgrade the existing per-user installation. Confirm About
   reports version `0.1.0-preview.104.0` and source commit `9e96c44...`.
3. Keep Art-Net, sACN, USB DMX, and physical fixtures disabled. In Connections,
   enable OS2L at `127.0.0.1:9996`, save, and leave the show stopped. With
   VirtualDJ explicitly set to `os2l=Yes`, verify EmberLights can reach listener
   and client Ready before Start Show. Start, stop, and restart the show; a
   stopped-period button must not replay after restart.
4. If OS2L behavior is ambiguous, close EmberLights and run
   `%LOCALAPPDATA%\Programs\EmberLights\Tools\os2l_capture.exe`. Exercise one
   load/play/stop/song-replacement sequence plus `blackout` on/off, stop with
   Ctrl+C, and review the capture before sharing it.
5. Still with all outputs disabled, choose **File → Import SoundSwitch 2026
   Project (Output-Disabled)** and select the extracted root of the exact
   current `2026.ssproj` source. The review must identify exactly one imported
   `Red - Smooth Pulse` Autoloop and the missing-color limitation; decline if it
   unexpectedly falls back to the broad V1 approximation.
6. Save under a new `.emberlights` filename. Confirm Patch has IR-4 addresses
   `001/011/021/031`, tube starts `041/121/201/281`, and no `uplight-*`
   fixtures. Confirm Medium slot 1, save, close, and reopen; the one imported
   loop must persist.
7. Report installer/launch behavior, OS2L Diagnostics or capture, the import
   review/counts, patch screenshot, reopen result, and the exact step of any
   divergence.

Keep SoundSwitch and an independent backup controller available. Do not use
this preview as the only lighting controller at an event.

## Remaining boundaries and next order

1. Reconcile tonight's native Windows/VirtualDJ evidence and fix only reproduced
   OS2L lifecycle or feedback defects.
2. Compare the single imported source slice before expanding to the other 111
   Autoloops, SoundSwitch Static Looks, movement, or effects.
3. Add the Advanced-only arbitrary universe/channel tester with explicit arm,
   timeout, held-output visibility, adapter ownership, and terminal blackout.
4. Activate the first modern Fixtures + Static Looks replacement-shell journey
   with complete profile-backed controls, RGBWAUV picking, patch/profile flows,
   save, and Undo; raw DMX remains behind Advanced.
5. Expand Autoloop/Static Look authoring, Live control, mappings, then the visual
   `.emberskin` and Ember Actions designer over the shared registry.

Native clean install/upgrade/uninstall, project/settings preservation, physical
output and Control One, Windows DPI/keyboard/Narrator, soak, Authenticode, and
gig qualification all remain open evidence gates.
