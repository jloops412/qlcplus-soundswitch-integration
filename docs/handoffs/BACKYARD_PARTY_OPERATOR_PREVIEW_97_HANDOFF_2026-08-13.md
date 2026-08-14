# Backyard-party operator Preview 97 handoff — 2026-08-13

Status: contract-tested unsigned Windows x64 testing preview. Native
installed-Windows lifecycle and physical hardware evidence remain pending.

## Primary outcome

Preview 97 restores a durable tester installer for the Preview 96 product
checkpoint after the earlier package bytes became unavailable. It does not add
a new lighting feature or broaden any physical qualification claim.

The packaged source adds only the Preview 96 evidence record after the feature
commit. Product behavior remains the Preview 96 software CR-4 evidence join:
Diagnostics can bind a recognized IR-4 Runner frame and Micro host-accept result
to intact embedded Raw Hardware Test evidence while continuing to deny current
fixture-side observation.

## Installer evidence

- Version: `0.1.0-preview.97.0`
- Source commit: `c9421dda3920d75ee70d817652aa1f001601acf3`
- Installer: `EmberLights-0.1.0-preview.97.0-Setup.exe`
- Installer SHA-256: `70f9c26c667aa206b8cc34d9ed991206e6b41a9e5645e69af20d4f62d34d7ee0`
- Installer size: `1,957,764` bytes
- Payload manifest: `EmberLights-0.1.0-preview.97.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `8519e997243fc30b3cd8283700dcf139b78b0eb051a349c9795f32bedf0ff209`
- Checksum evidence: `EmberLights-0.1.0-preview.97.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `7c10cc2babeac471e4ffba262eb9a6e64e4eea13345603cfb0f94efd6c97ef46`

## Contract evidence

- clean canonical worktree and exact 40-character source identity;
- package-contract Python regressions: 18/18 passed;
- Windows x64 Zig/Clang application and supported tool targets built;
- exact 20-file CMake stage plus generated payload manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test and extraction passed;
- normalized extracted 21-file product payload matched the staged bytes and
  passed the package contract again;
- output-disabled V1 template and editable output-disabled IR-4 bench project
  are present.

Evidence class: **contract-tested unsigned testing preview** on a non-Windows
host. This is not installed-tested, physically qualified, gig-qualified, or a
public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch.
2. Run the Preview 97 installer on Windows 11 and allow it to replace the prior
   per-user EmberLights install.
3. Confirm launch, version `0.1.0-preview.97.0` in About/Diagnostics, and that
   the prior project/settings remain available.
4. Confirm **Installed apps** contains EmberLights and the Start-menu entries
   open the application, Hardware Test, and IR-4 editable bench.
5. Report any installer/SmartScreen message verbatim before enabling output.

After install/launch preservation is confirmed, use the bounded isolated IR-4
workflow in `MORNING_HARDWARE_TEST.md` and `IR4_6CH_RUNNER_FRAME_TEST.md`.
Keep an independent backup controller and do not use this preview as the only
lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps
  uninstall, shortcut/registry cleanup, and projects/settings preservation;
- installed Raw Hardware Test and exact graduated-project reopen;
- physical observations for both IR-4 units, blackout/no-spill, and
  disconnect/reconnect behavior;
- Authenticode signing, soak, shadow rehearsals, and gig qualification.
