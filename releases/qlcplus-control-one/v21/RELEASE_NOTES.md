# QLC+ SoundSwitch V21 — Reliability and Control

V21 turns the working V20 show into a reproducible, rollback-safe release for the SoundSwitch Micro and Control One. It is a control and reliability release: all 1,906 V20 Functions are preserved exactly, including the 128 Autoloops, Autoplay parents, Priority Looks, manual controls, fixtures, and their public bindings.

## Highlights

- Control One MIDI input and LED output recover from stale Windows handles and USB close events without intentionally resetting the running show state.
- LED writes reconnect and retry once, then restore the selected bank, transport, order, mode, intensity target, overrides, and known active pads.
- VirtualDJ targets QLC+ directly on localhost and periodically sends a harmless OS2L keepalive so either program can start or restart late.
- Essential operations are available from the mouse: Bank, dwell, Play/Pause, sequential/random order, Autoloop/Priority Looks mode, chase-speed multiplier, intensity target, and the 4×8 pad surface.
- The native plug-in is pinned to an exact QLC+ core, Qt/compiler tuple, and SHA-256 checksum.
- Install and rollback scripts verify hashes, refuse to replace a running QLC+, and preserve the previous plug-in with a receipt.
- `Test-V21Package.ps1` validates the package, project XML, fixture patch, Function references, input-profile channels, scripts, hashes, and optional V20 preservation evidence.

## Supported hardware in this release

| Hardware | V21 role | Evidence before release |
|---|---|---|
| SoundSwitch Micro | Universe 1 DMX output | Physical fixture output confirmed |
| SoundSwitch Control One DMX 1 | Universe output | Physical fixture output confirmed |
| SoundSwitch Control One DMX 2 | Second universe output | Physical fixture output confirmed independently |
| SoundSwitch Control One MIDI | Pads, controls, transport and workflow | Core workflow physically confirmed on the V19/V20 baseline |
| SoundSwitch Control One LEDs | State feedback and reconnect restore | Software-tested; final USB hot-plug observation remains |

## Compatibility

- QLC+ UI: `5.3.0 GIT a124abe`
- QLC+ source: `a124abebe0b5ad6077727c561a5a0e1f3730810c`
- Required `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`
- Plug-in SHA-256: `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D`
- Platform: Windows x64 with the matching MinGW/Qt QLC+ build

The plug-in is not ABI-portable across arbitrary QLC+ builds. The installer rejects a different core unless the operator explicitly supplies `-AllowCompatibleCore` after performing their own compatibility validation.

## Upgrade from V20

1. Keep V20 and the existing QLC+ directory as the rollback baseline.
2. Run `Test-V21Package.ps1`.
3. Close QLC+.
4. Run `Install-SoundSwitchPlugin.ps1` with the exact QLC+ directory.
5. Open `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw` and select the desired Micro or Control One output once if necessary.
6. Import/select `SoundSwitch-Control-One-Performance.qxi` for the Control One input.
7. Perform the short physical checklist in the package README.

Rollback is one command with `Rollback-SoundSwitchPlugin.ps1`; it restores the latest verified backup made by the installer.

## Qualification boundary

V21 is structurally validated and software-tested. Micro, each Control One DMX port independently, OS2L, MIDI, and the core performer workflow have physical evidence from the preceding baseline. V21 is not yet labeled gig-qualified: simultaneous dual-port operation, repeated live USB hot-plug/LED restoration, and the combined two-hour VirtualDJ/audio/OS2L/MIDI/DMX workload remain explicit field gates.

This is independent community interoperability work and is not affiliated with or endorsed by SoundSwitch, inMusic, or the QLC+ project.
