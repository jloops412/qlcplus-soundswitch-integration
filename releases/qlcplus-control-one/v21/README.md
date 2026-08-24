# QLC+ SoundSwitch V21 Reliability Candidate

V21 is a control-and-reliability update on top of the V20 creative baseline. It does not alter the existing fixtures, 128 Autoloops, Autoplay parents, Priority Looks, manual functions, or their public IDs.

## Contents

- `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw` — V20 show plus mouse Play/Pause, order, performance-mode, and chase-speed controls.
- `SoundSwitch-Control-One-Performance.qxi` — Control One input profile.
- `soundswitch.dll` — native Micro + Control One plug-in.
- `Install-SoundSwitchPlugin.ps1` — hash-checked install with an automatic backup.
- `Rollback-SoundSwitchPlugin.ps1` — restore the latest or a specified backup.
- `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` — direct local OS2L and keepalive setup.

## Pinned compatibility tuple

- QLC+ UI version: `5.3.0 GIT a124abe`
- QLC+ source commit: `a124abebe0b5ad6077727c561a5a0e1f3730810c`
- Installed `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`
- QLC+ interface headers: exact source commit above
- Qt build headers: `6.8.1`; installed runtime: `6.10.2` (Qt 6 minor-version binary compatibility)
- Compiler: MSYS2 MinGW-w64 GCC `16.2.0`
- Plug-in SHA-256: `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D`

The installer rejects a different QLC+ executable by default. For a future official QLC+ update, install it side-by-side, rebuild this plug-in against that exact source commit, and update the compatibility tuple before switching production.

Install or roll back by passing the side-by-side QLC+ directory explicitly:

```powershell
.\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe-Official'
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe-Official'
```

## Reliability changes

- WinMM MIDI input and LED handles are validated by device ID, not only by a possibly stale device name.
- Windows input/output close callbacks invalidate dead handles.
- A failed LED write detaches, reconnects, and retries once.
- Selected bank, performance mode, playback, order, overrides, group target, and known active pads are restored after reconnect.
- VirtualDJ targets QLC+ directly on localhost and sends a lightweight five-second keepalive.

## Qualification status

The plug-in compiles against the pinned QLC+ commit. V20 remains the owner-confirmed creative baseline until V21 receives a short physical regression. Do not call V21 gig-qualified until Control One unplug/replug, LED recovery, both DMX ports together, and normal VirtualDJ workload have been exercised.
