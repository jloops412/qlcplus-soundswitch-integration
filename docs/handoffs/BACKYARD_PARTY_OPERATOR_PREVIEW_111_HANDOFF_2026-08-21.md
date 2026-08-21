# Backyard-party operator Preview 111 handoff — 2026-08-21

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows, VirtualDJ lifecycle, accessibility, and
physical DMX evidence remain pending.

## Primary outcome

Preview 111 advances the accepted Slint 1.17.1 Default shell along the #33/#87
beta journey with a bounded operator-composition pass under D-100.

- Studio and Live are explicit top-level workspaces: **Build and rehearse** and
  **Perform the show**.
- The canonical Live health/safety strip remains pinned in both workspaces.
- Live now presents the selected bank as a dense 4×8 matrix backed by the
  existing 32-pad `LiveViewModel` projection.
- Selected, actively playing, disabled-by-filter, and empty pads are visually
  and accessibly distinct; active playback carries authoritative progress.
- Page, bank, all-bank/selected-bank filter, Launch, Previous, Next, and Clear
  controls retain the existing `UiCommandFacade` path.
- Runner, sync/BPM, and output health are visible beside the performance
  surface rather than compressed into a header strip.
- Saved Static Looks share the Live canvas and retain the existing sparse
  take/release ownership contract over lower Autoloop or Track Script content.
- The 222px product header was reduced to 150px so Studio retains more vertical
  authoring space at the supported 1024×640 minimum.

Public SoundSwitch Autoloop documentation and QLC+ Virtual Console patterns
were reviewed before implementation. The pass copies no vendor code, assets,
or trade dress, adds no Qt dependency, and does not create a second engine,
registry, persistence path, or output authority.

Autoloop authoring/placement, SoundSwitch import/review, Connections,
AutoScript, advanced diagnostics, hardware qualification, and broader Live
parity remain in Compatibility Tools until each workflow crosses its own gate.

## Installer evidence

- Version: `0.1.0-preview.111.0`
- Installer source commit: `4c149892c486df6ec6b72c5884ebc22eccd10220`
- Installer source tree: `c7422186f501d15d797aea61d8cbd1e9cf854075`
- Source branch: `agent/replacement-shell-preview-108`
- Draft PR: `#99`
- Installer: `EmberLights-0.1.0-preview.111.0-Setup.exe`
- Installer SHA-256: `67c9f457f167b1eb2b315d9c81c54ea76342bc8814130cbce94f38d29e3ea6bc`
- Installer size: `11,995,978` bytes
- Payload manifest: `EmberLights-0.1.0-preview.111.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `2b323de34a0ec7e1e6bbf68a57d12c0791ef810ffd828a4d60a09d90b42e6129`
- Payload manifest size: `5,474` bytes
- Checksum evidence: `EmberLights-0.1.0-preview.111.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `725d4865be9a3f4b66519eee849e12408cae04d471b47b1508f69b63ee2bc104`

## Verification evidence

- the complete native Makefile suite passed across Runner, Autoloops V2,
  Static Look authoring/preview, migration, hardware boundaries, and UI models;
- pinned Slint 1.17.1 markup compilation and all 15 focused
  Slint/product-shell contract tests passed;
- 13/13 registry-compatibility and 4/4 UI-direction checks passed;
- 19/19 Windows payload contract regressions passed;
- the exact installer source commit and tree were clean before packaging;
- warning-fatal Windows x64 Zig/Clang compilation passed for the modern and
  Safe shells plus every payload tool;
- `EmberLights.exe`, `EmberLights-Safe.exe`, and `slint_cpp.dll` are PE32+ x64;
- the 30-file staged payload and its generated manifest verified;
- repeated NSIS construction was byte-for-byte identical; and
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly.

The installer helper still omits the required `os2l_capture` target from its
explicit build list. As in Preview 110, that declared target was built
explicitly with the same configured source identity before rerunning the
unchanged source-bound installer gate.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, physically qualified, accessibility-qualified,
gig-qualified, signed, or a public release. Output remains disabled by default,
self-tests remain non-outputting, and physical qualification is not claimed.

## First Windows checks

1. Verify the installer SHA-256 above, close existing EmberLights processes,
   and install Preview 111.0.
2. Launch EmberLights normally. Confirm the modern shell opens and its footer
   reports Preview 111.0 plus source commit `4c149892...`.
3. At 1024×640 and 1366×768, switch repeatedly between **STUDIO** and **LIVE**.
   Confirm the canvas changes without starting Runner or dirtying the project,
   and the Live health/safety strip stays visible.
4. In Studio, confirm the existing Fixtures + Static Looks journey retains
   usable vertical space and Advanced diagnostics remain Studio-only.
5. Open the saved output-disabled project containing the reviewed Autoloop.
   Confirm Live shows four rows of eight pads, four visible bank buttons, and
   selected, empty, and disabled states before output is enabled.
6. Start Live with physical output still disabled. Launch the selected
   Autoloop and confirm `PLAYING`, progress, active bank `LIVE`, active detail,
   Runner health, sync, and BPM update coherently.
7. Exercise Previous/Next, page navigation, **All banks**, **Selected bank**,
   and **Clear Autoloop**. Confirm disabled banks remain visibly `OFF` and
   empty pads never launch.
8. Select a saved Static Look, take it Live, and release it. Confirm only the
   Look's explicit properties change ownership and the Autoloop continues.
9. Exercise Work Light, Release Overrides, and BLACKOUT only with output
   disabled for this first UI check; confirm each remains accessible from both
   workspaces.
10. Record clean install/upgrade/uninstall behavior, 125%/150% DPI,
    horizontal matrix scrolling at minimum width, keyboard focus, and Narrator
    observations.

Do not treat Running, Connected, or a correct UI transition as proof of DMX or
fixture response. Installed packet capture, physical fixtures, blackout
observation, soak, and gig qualification remain separate evidence gates.
