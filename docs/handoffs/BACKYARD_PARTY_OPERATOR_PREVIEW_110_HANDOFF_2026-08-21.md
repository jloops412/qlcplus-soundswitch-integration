# Backyard-party operator Preview 110 handoff — 2026-08-21

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows, VirtualDJ lifecycle, accessibility, and
physical DMX evidence remain pending.

## Primary outcome

Preview 110 advances the accepted Slint 1.17.1 Default shell along the #33/#87
beta journey without adding a second Autoloop engine, guessed SoundSwitch
content, or a new Win32 surface.

- The modern Live workspace now projects format-1 and persisted Autoloops V2
  through the existing toolkit-neutral `LiveViewModel`.
- The operator can page all 64 Autoloop banks, select any of 32 slots in a bank,
  and see stable bank/slot identity, content name, repeat mode, active state,
  progress, and completed cycles.
- Persisted V2 pads expose their native/content-pack/generated/migrated origin
  plus the stored evidence-status label instead of presenting imported content
  as unexplained local choreography.
- Launch, Previous, Next, Clear, all-bank navigation, and selected-bank-only
  navigation route through the canonical `UiCommandFacade`.
- A valid Autoloop is selected automatically when the active project changes;
  selection stays view-local and does not mutate the project.
- The existing Runner, Autoloops V2 compiler/runtime, bank filters, output
  authority, terminal blackout, and Compatibility Tools handoff remain
  unchanged.

Autoloop authoring/placement, SoundSwitch import/review, Connections,
AutoScript, advanced diagnostics, hardware qualification, and broader Live
parity remain in Compatibility Tools until each workflow crosses its own gate.

## Installer evidence

- Version: `0.1.0-preview.110.0`
- Installer source commit: `08770ffeee2eefa37736b2bf51c0ddaa3d28b71b`
- Installer source tree: `3e9ad4cc852677679e5e0e1890b23905922dcd94`
- Source branch: `agent/replacement-shell-preview-108`
- Draft PR: `#99`
- Installer: `EmberLights-0.1.0-preview.110.0-Setup.exe`
- Installer SHA-256: `f830208baa94592758aff5aed5e299d7277d33b210044a49be03cea30c2e1e26`
- Installer size: `11,843,509` bytes
- Payload manifest: `EmberLights-0.1.0-preview.110.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `36bade3010b0907732f9198260cfa7f29aa9e4b3e5c4583e2ff58c6fbc72238e`
- Payload manifest size: `5,474` bytes
- Checksum evidence: `EmberLights-0.1.0-preview.110.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `ca96d28403236a8e7e9ce8d70347f0ab4ca16895fea0d9d58e749f141760a850`

## Verification evidence

- 47/47 native CTest targets passed;
- pinned Slint 1.17.1 markup compilation and 14/14 focused
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
explicit build list. As in Preview 109, that declared target was built
explicitly with the same configured source identity before rerunning the
unchanged source-bound installer gate.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, physically qualified, accessibility-qualified,
gig-qualified, signed, or a public release.

## First Windows checks

1. Verify the installer SHA-256 above, close existing EmberLights processes,
   and install Preview 110.0.
2. Launch EmberLights normally. Confirm the modern shell opens and its footer
   reports Preview 110.0 plus source commit `08770ffe...`.
3. Open the saved output-disabled project containing the reviewed Autoloop.
   Confirm **LIVE AUTOLOOPS** shows its bank, slot, name, V2 origin/evidence,
   and repeat mode before Live starts.
4. Page backward and forward through the 16 four-bank pages, select populated
   and empty slots, and confirm Launch enables only for populated content while
   Live is running.
5. Start Live and launch the selected Autoloop. Confirm its name becomes active,
   progress advances, cycles increment, and the existing Static Look can be
   taken and released over it without stopping the Autoloop.
6. Exercise Previous/Next with **All**, then choose **Only bank** and repeat.
   Confirm disabled banks are labeled `OFF` and navigation stays within the
   selected bank until **All** is restored.
7. Clear the Autoloop and confirm lower/base content remains authoritative.
   Exercise Work Light and BLACKOUT only with output disabled for this first
   UI check.
8. Stop Live, reopen the project, and verify Autoloop selection caused no
   project dirty state or persistence change.
9. With VirtualDJ closed, confirm **Listening**; start VirtualDJ and record the
   transition to **Connected**, sync state, and BPM. Packet/choreography truth
   still requires the separate capture checklist.
10. Record clean install/upgrade/uninstall behavior, 1024×640 and 1366×768
    layout, 125%/150% DPI, horizontal pad scrolling, keyboard focus, and
    Narrator observations.

Do not treat a Running/Connected status as proof of correct DMX or fixture
response. Installed packet capture, physical fixtures, blackout observation,
soak, and gig qualification remain separate evidence gates.
