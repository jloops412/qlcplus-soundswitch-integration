# Backyard-party operator Preview 109 handoff — 2026-08-21

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows, VirtualDJ lifecycle, accessibility, and
physical DMX evidence remain pending.

## Primary outcome

Preview 109 advances the accepted Slint 1.17.1 Default shell along the #33/#87
beta journey instead of extending the frozen Win32 compatibility surface.

- A saved, clean, validated project can start and stop the production Runner
  from `EmberLights.exe`.
- The persistent Live strip reports Runner state, sync/BPM, U1/U2 output
  health, active content, and manual-override count.
- Start/Stop, Blackout, Work Light, Release Overrides, and selected-Static-Look
  take/release route through the canonical `UiCommandFacade`.
- Starting Live compiles the persisted Autoloop source and writes the existing
  last-known-good activation snapshot. Stopping retains Runner's terminal
  blackout behavior.
- Studio editing and preview remain locked while Live owns output. Selecting a
  saved Static Look remains available so the operator can take or release it
  over the advancing lower Autoloop/script layer.
- **Compatibility Tools** replaces the misleading **Switch to Safe / Live**
  label. The handoff stops the modern Runner and OS2L service before launching
  `EmberLights-Safe.exe`.

Connections, Autoloop selection, migration, AutoScript, advanced diagnostics,
hardware qualification, and broader Live parity remain in Compatibility Tools
until each workflow crosses its own product and evidence gate.

## Installer evidence

- Version: `0.1.0-preview.109.0`
- Installer source commit: `aae885257d8245e532e6ef629a59896e47b7f549`
- Installer source tree: `6abc913ee64703a3837d133cbd8bbb6c6947ad1e`
- Source branch: `agent/replacement-shell-preview-108`
- Draft PR: `#99`
- Installer: `EmberLights-0.1.0-preview.109.0-Setup.exe`
- Installer SHA-256: `79a5c2b8232f5fc35a5835f6abedbf739afac9507d53c4ed230ca5f098880964`
- Installer size: `11,789,856` bytes
- Payload manifest: `EmberLights-0.1.0-preview.109.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `2344fd48ad4233b7b116684be664079409a1de69ac59d6e87fae638b373b6974`
- Payload manifest size: `5,474` bytes
- Checksum evidence: `EmberLights-0.1.0-preview.109.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `c829bfb2b140e5cd227b2394002f4f6b3bd02904c50f22ff80d490a4a94e2c1a`

## Verification evidence

- 47/47 native CTest targets passed from a fresh Release build;
- pinned Slint 1.17.1 markup compilation and 13/13 focused
  Slint/product-shell contract tests passed;
- 13/13 registry-compatibility and 4/4 UI-direction checks passed;
- 19/19 Windows payload contract regressions passed;
- the exact installer source commit and tree were clean before packaging;
- warning-fatal Windows x64 Zig/Clang compilation passed for the modern and
  Safe shells plus all required payload tools;
- the 30-file staged payload and its generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly.

The installer helper still omits the required `os2l_capture` target from its
explicit build list. As in Previews 105 and 108, that declared target was built
explicitly with the same configured source identity before rerunning the
unchanged source-bound installer gate.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, physically qualified, accessibility-qualified,
gig-qualified, signed, or a public release.

## First Windows checks

1. Verify the installer SHA-256 above, close existing EmberLights processes,
   and install Preview 109.0.
2. Launch EmberLights normally. Confirm the modern shell opens and its footer
   reports Preview 109.0 plus source commit `aae88525...`.
3. Copy/open an output-disabled project, save it, and confirm **Start Live** is
   enabled only while the saved project is clean and preview is stopped.
4. Start Live. Confirm the strip reaches **Running**, Studio controls and
   preview lock, U1/U2 report the expected disabled state, and the active
   content reports **Base layers**.
5. Select a saved Static Look, choose **Take Look Live**, then release it.
   Confirm active-content feedback follows the activation and the lower content
   remains available after release.
6. Exercise Work Light, Release Overrides, and BLACKOUT in an output-disabled
   project. Confirm BLACKOUT is persistent and explicit before releasing it.
7. Stop Live and confirm the shell reports Stopped with the terminal-blackout
   message. Reopen the project and verify the last-known-good activation file
   does not replace authored content.
8. With VirtualDJ closed, confirm **Listening**; start VirtualDJ and record the
   transition to **Connected**, sync state, and BPM. Packet/choreography truth
   still requires the separate capture checklist.
9. Start Live again, choose **Compatibility Tools**, and confirm the modern
   Runner stops before the legacy workspace opens. Confirm only one UI process
   owns OS2L after the handoff.
10. Record clean install/upgrade/uninstall behavior, 1024×640 and 1366×768
    layout, 125%/150% DPI, keyboard focus, and Narrator observations.

Do not treat a Running/Connected status as proof of correct DMX or fixture
response. Installed packet capture, physical fixtures, blackout observation,
soak, and gig qualification remain separate evidence gates.
