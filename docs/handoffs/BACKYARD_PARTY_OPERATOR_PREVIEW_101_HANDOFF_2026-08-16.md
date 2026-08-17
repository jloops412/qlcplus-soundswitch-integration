# Backyard-party operator Preview 101 handoff — 2026-08-16

Status: contract-tested unsigned Windows x64 testing preview. Native installed-
Windows lifecycle and physical hardware evidence remain pending.

## Primary outcome

Preview 101 is a deliberately small core safety/defect installer. Static Look
activations and binding leases owned by MIDI/controllers or OS2L now carry an
opaque connection-session epoch. Disconnect or transport error clears only the
matching abandoned session, revealing the still-advancing Autoloop. A delayed
release or owner-loss signal from an older connection cannot clear a newer
activation after reconnect.

This is not a UI checkpoint. It adds no Win32 editor or form, does not install
the Slint lab, does not accept a replacement-shell toolkit, and does not change
the public command/state registry, project format, Runner scheduling, output
routing, blackout, or hazard authority.

## Installer evidence

- Version: `0.1.0-preview.101.0`
- Source commit: `135d6a76f334784de3eba1b49fb842165b344bec`
- Installer: `EmberLights-0.1.0-preview.101.0-Setup.exe`
- Installer SHA-256: `3ac86e34bfc0b92bdfe8ccb73d97c89c3b9eee7de33a0c549492a10f67d7af7c`
- Installer size: `1,999,150` bytes
- Payload manifest: `EmberLights-0.1.0-preview.101.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `811fb399b415e734c86d1f8cb0bcfe572ab339589c2c25efbaf5e5192e84ce21`
- Checksum evidence: `EmberLights-0.1.0-preview.101.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `04997e48975c4b6f191928f9564a0b5c752d10d46b7e7b5a8bb3c14c7726aaa2`

## Verification evidence

- warning-fatal full native Make build and complete native test suite passed on
  the exact packaged source, including transport session/lifecycle regressions;
- Windows package-contract regressions passed 18/18;
- clean Windows x64 Zig/Clang application and supported tool targets built;
- exact 20-file payload plus generated manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and its normalized 21-file product
  payload matched the staged bytes exactly;
- the output-disabled V1 template and editable output-disabled IR-4 bench
  project remain included;
- `EmberLightsSlintLab` and every replacement-shell experiment remain excluded.

Evidence class: **contract-tested unsigned testing preview** on a non-Windows
host. This is not installed-tested, accessibility-qualified, physically
qualified, gig-qualified, or a public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch. Run the Preview 101 installer on Windows
   11 and allow it to upgrade the existing per-user installation.
2. Confirm About reports `0.1.0-preview.101.0` and source commit
   `135d6a76f334784de3eba1b49fb842165b344bec`; confirm the prior project and
   settings still open.
3. Keep physical output disabled or use an isolated visualizer. Start an
   Autoloop, then hold/select a Static Look from the configured MIDI device.
   Disconnect the device without releasing. The abandoned Look must clear and
   the advancing Autoloop must reappear.
4. Reconnect and select a new Look. A delayed release/loss from the older
   session must not clear the new activation. Repeat the same disconnect,
   reconnect, and stale-release sequence through OS2L/VirtualDJ.
5. Report the exact MIDI device, OS2L route, active Look/Autoloop, observed
   state, Diagnostics text, and any upgrade/launch/uninstall issue verbatim.

Keep an independent backup controller and do not use this preview as the only
lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps
  uninstall, shortcut/registry cleanup, and project/settings preservation;
- installed-Windows MIDI/Control One hotplug and physical controller feedback;
- Windows DPI, keyboard, Narrator/UI Automation, and long-session evidence;
- replacement-shell toolkit acceptance, activation, servicing, and licensing;
- owned-fixture/controller observations, eight-hour soak, shadow rehearsals,
  Authenticode signing, and gig qualification.
