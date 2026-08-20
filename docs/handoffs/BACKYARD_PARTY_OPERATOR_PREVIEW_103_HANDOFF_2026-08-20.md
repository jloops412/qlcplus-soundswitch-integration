# Backyard-party operator Preview 103 handoff — 2026-08-20

Status: contract-tested non-Windows unsigned testing preview, packaged for
Windows x64. Native installed-Windows lifecycle, VirtualDJ behavior, and
physical hardware evidence remain pending.

## Primary outcome

Preview 103 is a deliberately bounded OS2L reliability installer. The listener,
DNS-SD advertisement, accepted client, parsing, and blackout feedback now belong
to a renderer-neutral application service instead of Runner. OS2L can therefore
listen while the show is stopped and remain available across show stop/start.
Runner attaches as a bounded consumer; session epochs and consumer generations
discard detached or stale actions rather than replaying them into a later show.

Diagnostics now separate listener, discovery, client, beat-traffic, queue, and
feedback facts. The installed `Tools/os2l_capture.exe` records ordered sequence
numbers, Unix-millisecond timestamps, connection/fault events, parsed message
kinds, and the exact bounded inbound JSON for one controlled diagnosis.

This is not a new Win32 UI milestone. It adds no form or editor surface, does
not install the Slint lab, and does not accept or activate a replacement-shell
toolkit. The current Win32 shell remains a frozen transitional bridge while the
service and state contracts become renderer-neutral.

## Installer evidence

- Version: `0.1.0-preview.103.0`
- Source commit: `4e7c71084f7d67ac5b796c64b9ee97895618b282`
- Source tree: `9f1a14d1bc9581406e93d9bf2ea85f572f1795d0`
- Draft integration PR: `#94`
- Immutable GitHub artifact branch: `artifacts/windows-preview-103`
- Artifact commit: `3a9faa8c4d492252bea0d89e1127cc9bc7baca47`
- GitHub bundle: `EmberLights-0.1.0-preview.103.0-Windows-testing-preview.zip`
- GitHub bundle SHA-256: `2acdf712d7382fb899170920a6e605008e76f436d46fcb363bb9670a11505a14`
- GitHub bundle size: `2,022,580` bytes
- Installer: `EmberLights-0.1.0-preview.103.0-Setup.exe`
- Installer SHA-256: `b16476f577ea4b250d8fe359a7da0d4362b2bc2263981c4a5efa96618efde70c`
- Installer size: `2,041,152` bytes
- Payload manifest: `EmberLights-0.1.0-preview.103.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `6d2573550115314ea459616c04db6580042142e528f1fa854517a83fc60f4493`
- Checksum evidence: `EmberLights-0.1.0-preview.103.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `2afa7d74465272c02b403a9d28f7239e8efb4f8f646e8e3069f69b407e6b842c`

## Verification evidence

- warning-fatal native core build and tests passed on the exact source tree;
- the clean CMake/CTest lane passed 45/45 and the aggregate Make gates passed;
- Windows package-contract regressions passed 18/18;
- clean Windows x64 Zig/Clang builds passed for the application and every
  explicit supported payload tool, including `os2l_capture.exe`;
- the exact 21-file payload plus generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and the normalized installed payload
  matched the staged bytes exactly;
- the output-disabled V1 template and editable output-disabled IR-4 bench
  project remain included;
- `EmberLightsSlintLab` and every replacement-shell experiment remain excluded.

GitHub Actions for PR #94 failed before any workflow step was created, matching
the same runner/infrastructure failure on adjacent PRs. This handoff does not
misstate that as a product test failure or as passing CI.

Evidence class: **contract-tested non-Windows unsigned testing preview**. This
is not installed-tested, accessibility-qualified, physically qualified,
gig-qualified, signed, or a public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch. Run the Preview 103 installer on Windows
   11 and allow it to upgrade the existing per-user installation.
2. Confirm About reports `0.1.0-preview.103.0` and source commit
   `4e7c71084f7d67ac5b796c64b9ee97895618b282`; confirm the prior project and
   settings still open.
3. Keep physical DMX output disabled. In Connections, enable OS2L at
   `127.0.0.1:9996`, save, and leave the show stopped. With VirtualDJ set to
   `os2l=Auto` and `os2lDirectIp` blank, confirm EmberLights reaches listener
   **Waiting/Ready** and then client **Ready** without pressing Start Show.
4. Start, stop, and restart the show while VirtualDJ remains open. The OS2L
   listener must remain available; a connected client or clean reconnect must
   remain usable, and a button sent while stopped must not replay after restart.
5. If any transition is ambiguous, close EmberLights and run
   `%LOCALAPPDATA%\Programs\EmberLights\Tools\os2l_capture.exe`, exercise one
   load/play/stop/song-replacement sequence plus `blackout` on/off, then stop it
   with Ctrl+C. Review the capture before sharing it.
6. Report installer/launch behavior, the ordered capture, Diagnostics text, and
   the exact step where observed behavior diverged.

Keep an independent backup controller and do not use this preview as the only
lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps
  uninstall, shortcut/registry cleanup, and project/settings preservation;
- installed-Windows VirtualDJ discovery, direct-IP fallback, reconnect, and raw
  capture evidence;
- physical output, MIDI/Control One hotplug, controller feedback, and owned-
  fixture observations;
- Windows DPI, keyboard, Narrator/UI Automation, and long-session evidence;
- replacement-shell toolkit acceptance, activation, servicing, and licensing;
- eight-hour soak, shadow rehearsals, Authenticode signing, and gig
  qualification.
