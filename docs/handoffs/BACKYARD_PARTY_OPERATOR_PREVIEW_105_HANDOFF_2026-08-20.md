# Backyard-party operator Preview 105 handoff — 2026-08-20

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows lifecycle and physical DMX evidence
remain pending.

## Primary outcome

Preview 105 stacks Preview 104's application-owned OS2L service and first exact
current-SoundSwitch 2026 Autoloop import slice with the first general Advanced
manual DMX channel tester.

The installed **Advanced Manual DMX Test** is fixture-agnostic. It can select
logical SoundSwitch Micro universe 1 or 2, send deterministic one- or
multi-channel literal values, merge or clear a held channel, show the exact
held projection and 512-byte frame SHA-256, and blackout immediately. It does
not require or modify an EmberLights project or fixture profile.

Output remains bounded by a digest-bound typed acknowledgement, repeated
blackout before initial arming and preset replacement, a configurable 1–30
second per-preset expiry, a ten-minute whole-session limit, 64 listed channels,
device-presence checks, and terminal blackout/close on Quit, Escape/Ctrl+C,
device loss, write failure, timeout, cancellation, or destruction. It reuses
the exact production `SoundSwitchMicroSession` used by Runner and the existing
qualification workflow; there is no second USB driver.

The existing **EmberLights Hardware Test** remains the separate sealed,
evidence-bound one-fixture qualification/audit path. Raw DMX has not been added
to ordinary fixture, Static Look, Autoloop, command-registry, or skin
vocabulary, and the frozen Win32 product shell gained no new editor.

## Installer evidence

- Version: `0.1.0-preview.105.0`
- Installer source commit: `e966df06bf7a91203be03d773d99b69c553e4a43`
- Installer source tree: `5717754fcc673192a94948da76b2a1d3922a9584`
- Source branch: `agent/manual-dmx-test`
- Draft stacked PR: `#96`
- Immutable GitHub artifact branch: `artifacts/windows-preview-105`
- Artifact commit: `2787fda7a597707950df176cbc42378b79edec5d`
- GitHub bundle: `EmberLights-0.1.0-preview.105.0-Windows-testing-preview.zip`
- GitHub bundle SHA-256: `5a35ab4057c5e9a71de57a6e1b663fd656d1cb53ce6def68488196cab90e4157`
- GitHub bundle size: `2,082,709` bytes
- Installer: `EmberLights-0.1.0-preview.105.0-Setup.exe`
- Installer SHA-256: `bdaba2f94633719512e75b195f8d45a2450556632febf9b97ab016f325ab066c`
- Installer size: `2,101,104` bytes
- Payload manifest: `EmberLights-0.1.0-preview.105.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `70811823ae5bdd6337789cdb5384a2b08b4d7f5da8773aaf9682229ea898eb92`
- Payload manifest size: `4,130` bytes
- Checksum evidence: `EmberLights-0.1.0-preview.105.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `423565b70edebcbdc7831b3fd7c85fa001fda3f5df33329b0dc0ad13fcc9fec9`

Immutable bundle URL:

`https://github.com/jloops412/EmberLights/raw/2787fda7a597707950df176cbc42378b79edec5d/EmberLights-0.1.0-preview.105.0-Windows-testing-preview.zip`

## Verification evidence

- warning-fatal aggregate native Make tests passed, including the full existing
  core/fixture/Static Look/Autoloop/OS2L/migration suites;
- focused manual-DMX tests passed for plan identity, acknowledgement denial,
  deterministic preset construction, blackout-before, preset replacement,
  explicit and automatic blackout, whole-session timeout, cancellation,
  device loss, open/write/blackout faults, normal stop, and destructor close;
- UI-direction and registry checks, DMX/WinMM syntax checks, and Runner dry-run
  smoke passed;
- Windows package-contract regressions passed 19/19;
- clean Windows x64 Zig/Clang builds passed for the application, the manual-DMX
  tool, and every supported payload tool;
- the exact 22-file CMake-staged payload plus generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly;
- the three-file GitHub bundle passed ZIP integrity and its published blob size
  and Git object identity match the locally verified bundle.

The bundled installer helper currently omits the already-required
`os2l_capture` target from its explicit build list. That existing target was
compiled explicitly before rerunning the unchanged source-bound gate. The
final stage, payload contract, NSIS archive verifier, and bundle all require and
independently verified the resulting `Tools/os2l_capture.exe`.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, physically qualified, accessibility-qualified,
gig-qualified, signed, or a public release.

PR #96's Native core and non-product Slint lab workflows were marked failed
before any job created a step or log; both packaging jobs were consequently
skipped. This matches the repository's existing hosted-runner/infrastructure
gate and is neither product failure evidence nor passing CI. The complete local
native, cross-build, package, reproducibility, and archive evidence above
remains this preview's release basis.

## Joshua's test tonight

1. Download the immutable Preview 105 bundle and verify its SHA-256 with
   `Get-FileHash`. Extract it and confirm the installer hash above.
2. Close EmberLights and SoundSwitch. Run the unsigned installer and confirm
   About reports version `0.1.0-preview.105.0` and source commit `e966df06...`.
3. For the manual-DMX test, isolate one non-hazardous fixture whose address and
   mode you have physically verified. Disconnect fog, haze, spark, laser,
   motion, strobe-sensitive, and every unrelated fixture. Keep an independent
   blackout/controller path ready.
4. Open **Start → EmberLights → Advanced Manual DMX Test**, choose universe 1
   and a five-second hold, review the plan, and paste its exact displayed
   `ARM RAW DMX ...` acknowledgement.
5. Try `SET 1 32`. Confirm the console names exactly channel 1 at 32, shows a
   frame hash and countdown, and automatically returns to all-zero blackout
   after five seconds. Record only what the isolated fixture actually does.
6. Try `APPLY 1=32 2=64`, then `SHOW`, `CLEAR 2`, and `BLACKOUT NOW`. Confirm
   each display matches the intended held values and that replacement is
   visibly preceded by blackout. Use low values until the channel's function
   and safe range are known.
7. Enter `QUIT` and confirm terminal state reports blackout success and a
   closed adapter. Repeat once with a nonzero value and Escape; repeat once
   after unplugging the Micro. Share the exact terminal state/error and whether
   the fixture blacked out.
8. Preserve Preview 104's OS2L and exact import checks: verify listener/client
   state across show stop/start, and import/reopen the exact reviewed
   `Red - Smooth Pulse` source slice with the correct IR-4/tube patch.

Do not connect or explore an unknown hazardous channel. A host-side accepted
write or correct frame hash is software evidence only; the physical fixture,
cable, interface, and emergency blackout still require direct observation.

## Remaining boundaries and next order

1. Reconcile installed Windows, VirtualDJ, manual-DMX, and physical blackout
   evidence; fix only reproduced lifecycle, transport, or safety defects.
2. Compare the single imported source slice before expanding to the other 111
   Autoloops, SoundSwitch Static Looks, movement, or effects.
3. Finish issue #37 and activate the first modern Fixtures + Static Looks
   replacement-shell journey with complete profile-backed controls, RGBWAUV
   picking, patch/profile flows, save, and Undo; raw DMX remains Advanced.
4. Expand Autoloop/Static Look authoring, Live control, mappings, then the
   visual `.emberskin` and Ember Actions designer over the shared registry.

Native clean install/upgrade/uninstall, project/settings preservation, physical
output and Control One, Windows DPI/keyboard/Narrator, soak, Authenticode, and
gig qualification all remain open evidence gates.
