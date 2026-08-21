# Backyard-party operator Preview 108 handoff — 2026-08-21

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows, VirtualDJ lifecycle, accessibility, and
physical DMX evidence remain pending.

## Primary outcome

Preview 108 advances the accepted Slint 1.17.1 Default shell instead of adding
another workflow to the frozen Win32 compatibility surface.

- `EmberLights.exe` now owns the application-level OS2L listener and reports
  Disabled, Listening, Connected, or fault state in its header.
- The header shows the configured OS2L address/port and connected-session
  identity without opening the legacy application.
- The modern workspace publishes authoritative Blackout; it does not claim
  Live choreography ownership while no Runner consumer is attached.
- **Switch to Safe / Live** stops modern OS2L, launches
  `EmberLights-Safe.exe` with the current project, and exits the modern process
  only after a successful launch. A failed launch restores modern OS2L.
- Project/runtime status and workspace actions now occupy separate header rows,
  sidebars are narrower, and the supported minimum window is 1024×640.

## Installer evidence

- Version: `0.1.0-preview.108.0`
- Installer source commit: `68503f88538771f2e7ad234143da0e578c37c929`
- Installer source tree: `4e9e678e4089d348b473fa4b4244cf31558323f2`
- Source branch: `agent/replacement-shell-preview-108`
- Draft PR: `#99`
- Installer: `EmberLights-0.1.0-preview.108.0-Setup.exe`
- Installer SHA-256: `4dfc98cd11a53c5bef453459f9721919c3b26d9693853dbcf35c0635744788aa`
- Installer size: `11,766,763` bytes
- Payload manifest: `EmberLights-0.1.0-preview.108.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `e45e74ab76a0305776dd33293c59a7ff3bf96cc3cf1149ab131c74216f4c3cf5`
- Payload manifest size: `5,474` bytes
- Checksum evidence: `EmberLights-0.1.0-preview.108.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `96b4b857ffcdd6be5a26fb6e08faa3c1ffeb9d005736e27dc96c1cda0806d6b2`

## Verification evidence

- pinned Slint 1.17.1 markup compilation passed without warnings;
- 12/12 focused Slint/product-shell contract tests passed;
- 19/19 Windows payload contract regressions passed;
- the exact source commit was clean before packaging;
- warning-fatal Windows x64 Zig/Clang compilation passed for the modern and
  Safe shells plus all required payload tools;
- the 30-file staged payload and its generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly.

The installer helper still omits the required `os2l_capture` target from its
explicit build list. As in Preview 105, that target was compiled explicitly
with the same configured source identity before rerunning the unchanged
source-bound installer gate.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, physically qualified, accessibility-qualified,
gig-qualified, signed, or a public release.

## First Windows checks

1. Verify the installer SHA-256 above, close existing EmberLights processes,
   and install Preview 108.0.
2. Launch EmberLights normally. Confirm the modern Fixtures + Static Looks
   shell opens at a usable size and its footer reports Preview 108.0 plus source
   commit `68503f88...`.
3. With VirtualDJ closed, confirm the header reports **VirtualDJ Listening** on
   the project's configured address/port. Start VirtualDJ and record whether
   the state changes to **Connected** without opening Safe / Live.
4. Save the project, choose **Switch to Safe / Live**, and confirm the modern
   window closes after the compatibility workspace opens. Confirm there is one
   EmberLights UI process and that VirtualDJ can reconnect.
5. Close Safe / Live, reopen EmberLights normally, and confirm the modern
   listener can bind again. Record any socket error, stale Connected state, or
   second process.
6. Check the 1024×640 window size, keyboard focus, 125%/150% DPI, project open,
   Static Look edit/save, Undo/Redo, simulation, and bounded physical preview
   only under the existing explicit authority rules.

Do not treat a Connected status as proof of correct beat choreography or DMX.
Installed packet capture, physical fixtures, blackout observation, soak, and
gig qualification remain separate evidence gates.
