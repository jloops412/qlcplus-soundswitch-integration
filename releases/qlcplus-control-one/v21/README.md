# QLC+ SoundSwitch V21

V21 is the reproducible Windows release of the EmberLights QLC+ show and focused SoundSwitch hardware integration. It runs as one lighting application—QLC+—with native output through a SoundSwitch Micro or Control One and a SoundSwitch-familiar Control One performance workflow.

V21 preserves every V20 creative Function exactly. Its changes are limited to reconnect reliability, LED-state restoration, mouse fallback controls, direct local OS2L startup/reconnect, and release packaging.

## Package contents

| File | Purpose |
|---|---|
| `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw` | Complete portable QLC+ show for the current IR-4/tube rig |
| `SoundSwitch-Control-One-Performance.qxi` | Named Control One input profile |
| `soundswitch.dll` | Native SoundSwitch Micro/Control One QLC+ plugin |
| `Install-SoundSwitchPlugin.ps1` | Hash-checked installation with automatic backup and receipt |
| `Rollback-SoundSwitchPlugin.ps1` | Verified restoration of the previous plugin |
| `Test-V21Package.ps1` | Read-only package, XML, fixture, reference, and hash validation |
| `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` | Same-laptop direct OS2L configuration and keepalive |
| `RELEASE_NOTES.md` | Changes, compatibility, evidence, and qualification boundary |
| `SHA256SUMS.txt` | SHA-256 for every distributed file except the manifest itself |
| `LICENSE-APACHE-2.0.txt` | Apache License 2.0 for the plugin and included QLC+ interface code |

## Exact compatibility tuple

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Build headers | Qt `6.8.1` |
| Qualified installed runtime | Qt `6.10.2` |
| Compiler | MSYS2 MinGW-w64 GCC `16.2.0` |
| `soundswitch.dll` SHA-256 | `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D` |

Do not drop this DLL into an arbitrary QLC+ installation. QLC+ plugins are build-matched. Install future QLC+ versions side-by-side, rebuild the plugin against that exact source, and qualify the result before changing a production system.

## Install

Keep the existing QLC+ installation and V20 project as the recovery baseline.

1. Extract the complete V21 archive to a normal folder.
2. In PowerShell, validate it:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V21Package.ps1
   ```

3. Close QLC+ and install the plugin into the exact matching QLC+ directory:

   ```powershell
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

4. Open `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw`.
5. In QLC+ Input/Output, select the desired SoundSwitch Micro or Control One DMX port once. The portable workspace intentionally contains no personal USB serial number.
6. Select/import `SoundSwitch-Control-One-Performance.qxi` for the Control One MIDI input if QLC+ has not already associated it.
7. Follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` when VirtualDJ and QLC+ share the same laptop.

The installer refuses to run while QLC+ is open, verifies the exact QLC+ executable by default, backs up any existing `soundswitch.dll`, writes an install receipt, copies V21, and verifies the installed hash.

## Roll back

Close QLC+, then run:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

The latest verified installer backup is restored. A specific receipt directory can be supplied with `-BackupDirectory` when more than one installation was made.

## Five-minute first-plug check

This is the useful minimum before depending on V21:

1. Start a manual Autoloop in each bank; confirm same-pad off and cross-pad replacement.
2. Start Auto Bank and Auto All; switch sequential/random, seek with a pad, and change dwell without restarting.
3. Apply and release one Priority Look; it must be sole DMX authority while the hidden Autoloop keeps advancing.
4. Confirm color override, Global intensity, IR-4 group intensity, and tube group intensity.
5. Confirm Play/Pause, performance-mode switch, order, bank, dwell, and chase speed from both Control One and the mouse.
6. With QLC+ left open, unplug/replug Control One; MIDI operation and known LEDs should return without restarting the application.
7. Confirm the selected DMX port reaches the fixtures. If using both Control One ports, confirm them together.

## Current show patch

- Four Both Lighting IR-4 fixtures, 10-channel mode, addresses 1, 11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, addresses 175, 215, 255, and 295.
- Private duplicates on QLC+ Universe 3 provide full-frame Priority Looks. Universe 3 must remain internal and must not be routed directly to physical DMX.

The BO-TUBE192 40-channel mode consists of eight RGBWY zones with no master dimmer. V21 therefore contains current-rig intensity scaling in the plugin; this must become configuration before the plugin can be called general-purpose.

## Release status

V21 is structurally validated and software-tested. Micro, Control One DMX 1, Control One DMX 2, Control One MIDI, core pad workflow, Priority Look takeover/release, and OS2L have physical evidence across the preceding baseline. Repeated V21 hot-plug/LED restoration, both DMX ports simultaneously, and the combined two-hour DJ workload remain field-qualification gates. This release is not yet labeled gig-qualified.

See `RELEASE_NOTES.md` for the complete boundary and the repository documentation for the workflow and maintenance model.

This is independent community interoperability work and is not an official SoundSwitch, inMusic, or QLC+ release.
