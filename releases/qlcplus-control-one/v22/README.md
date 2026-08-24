# QLC+ SoundSwitch V22 Unified Pro

V22 is the unified Windows release of the QLC+ SoundSwitch show. It combines the proven V21 Control One/UI/reliability line with the later **All Banks Variety Pro** creative pass, without replacing the working playback architecture.

At show time there is still one lighting application: QLC+. The package supports SoundSwitch Micro and Control One DMX, the Control One performance workflow, VirtualDJ OS2L timing, four banks of 32 Autoloops, Priority Looks, parameter overrides, and group intensity.

## What was merged

- V21 remains the host for fixtures, I/O, Control One MIDI, LED feedback, reconnect, Autoplay ownership, dwell, chase speed, Priority Look authority, and the complete mouse workflow.
- Variety Pro replaces 22 older raw Autoloops in the Colorful and Flashy banks.
- All 176 Scene steps required by those 22 Autoloops are included.
- Seventeen donor helper IDs that collided with V21 UI-control Functions are remapped to private IDs. No public Function ID or logical Control One channel changes.
- The 32 Priority Looks are retained from V21 because the Variety Pro donor did not change them; this preserves their private-Universe full-frame takeover and release behavior.
- A read-only active-loop outline is bound directly to all 128 raw Chasers. It follows manual playback, Auto Bank, and Auto All as the actual running Chaser changes.

V20 and V21 remain untouched rollback points.

## Package contents

| File | Purpose |
|---|---|
| `IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw` | Unified portable QLC+ show for the current IR-4/tube rig |
| `SoundSwitch-Control-One-Performance.qxi` | Named Control One input profile |
| `soundswitch.dll` | Native SoundSwitch Micro/Control One QLC+ plug-in; binary-identical to V21 |
| `Install-SoundSwitchPlugin.ps1` | Hash-checked installation with automatic backup and receipt |
| `Rollback-SoundSwitchPlugin.ps1` | Verified restoration of the previous plug-in |
| `Test-V22Package.ps1` | Read-only package, XML, creative-merge, fixture, reference, and hash validation |
| `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` | Same-laptop direct OS2L configuration and keepalive |
| `RELEASE_NOTES.md` | Merge details, compatibility, evidence, and qualification boundary |
| `SHA256SUMS.txt` | SHA-256 for every distributed file except the manifest itself |
| `LICENSE-APACHE-2.0.txt` | Apache License 2.0 for the plug-in and included QLC+ interface code |

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

The V22 workspace changes show content and Virtual Console presentation only. The V21 plug-in is reused unchanged, so the exact QLC+ compatibility rule is unchanged too. Do not install this DLL into an arbitrary QLC+ build.

## Install or upgrade

If V21 is already installed against the pinned QLC+ build, no plug-in replacement is required; validate the archive and open the V22 workspace. The installer is included for a clean machine or recovery.

1. Extract the complete V22 archive.
2. Validate it in PowerShell:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V22Package.ps1
   ```

3. If the matching plug-in is not installed, close QLC+ and run:

   ```powershell
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

4. Open `IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw`.
5. Select the desired SoundSwitch Micro or Control One DMX output once if QLC+ does not retain it.
6. Associate `SoundSwitch-Control-One-Performance.qxi` with the Control One MIDI input if needed.
7. Follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` when VirtualDJ and QLC+ share the same laptop.

The portable workspace intentionally contains no personal USB serial number or machine path.

## Roll back

The safest show rollback is simply to close V22 and reopen the known-good V21 or V20 `.qxw`. V22 does not overwrite either workspace.

If the plug-in itself was installed and must be restored, close QLC+ and run:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

## Focused first test

1. Start one manual pad in each bank. Confirm latch, same-pad off, replacement, and the active outline.
2. Start Auto Bank. Confirm the outline advances with each raw Chaser and that dwell can change while running.
3. Start Auto All. Confirm the outline continues advancing through bank transitions and the native Now Playing strip reports the active Function.
4. Seek with several pads while Autoplay remains active.
5. Apply and release one still and one moving Priority Look. The Look must be sole DMX authority while the hidden Autoloop continues.
6. Confirm color overrides, Global intensity, IR-4 group intensity, and tube group intensity.
7. Confirm Micro or the chosen Control One DMX port reaches the fixtures.

## Current show patch

- Four Both Lighting IR-4 fixtures, 10-channel mode, addresses 1, 11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, addresses 175, 215, 255, and 295.
- Matching private duplicates on QLC+ Universe 3 for full-frame Priority Looks. Universe 3 is internal and must not be routed directly to physical DMX.

## Evidence boundary

V22 is structurally validated. Its runtime/control base is the software-tested V21 package, and its unchanged hardware path inherits the preceding physical evidence for Micro, each Control One DMX port independently, Control One MIDI, OS2L, core pad behavior, and Priority Look takeover/release.

The merged 22-loop creative content and the new active-loop outline still need the focused owner observation above. Simultaneous dual-port operation, repeated hot-plug, and the combined two-hour DJ workload remain gig-qualification gates.

This is independent community interoperability work and is not an official SoundSwitch, inMusic, or QLC+ release.
