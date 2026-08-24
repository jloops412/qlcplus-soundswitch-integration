# QLC+ SoundSwitch V23 Live Console

V23 is the simplest current Windows package for running SoundSwitch Micro or Control One hardware directly from QLC+, with a Control One-style live surface and no bridge or second lighting application.

## Fast upgrade from V21 or V22

If the SoundSwitch plug-in already works in the pinned QLC+ build:

1. Run `Test-V23Package.ps1`.
2. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.
3. Leave the installed plug-in alone; V23 uses the same tested DLL.

That is the entire upgrade.

## First installation

1. Install a complete QLC+ build matching **5.3.0 GIT a124abe**. Do not mix executables, Qt DLLs, or plug-ins from different QLC+ builds.
2. Install the normal manufacturer driver/software for the SoundSwitch device first, then close SoundSwitch before opening QLC+.
3. Extract this whole package to one folder.
4. Open PowerShell in that folder and run:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V23Package.ps1
   ```

5. With QLC+ closed, install the plug-in:

   ```powershell
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

6. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.
7. In QLC+ Input/Output, select the desired SoundSwitch output:
   - Micro: one DMX universe;
   - Control One DMX 1: first port;
   - Control One DMX 2: second port.
8. For Control One MIDI, select the SoundSwitch Control One input and associate `SoundSwitch-Control-One-Performance.qxi` if QLC+ did not retain it.
9. If VirtualDJ and QLC+ share the laptop, follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md`.

Do not replace the entire Control One composite USB driver with a generic driver. Doing so can remove its MIDI interface.

## What V23 fixes

- One persistent `AUTOLOOPS ⇄ PRIORITY LOOKS` mouse button now works from either pad mode. V22 had two copies observing the same feedback channel, so one click could be interpreted twice.
- Every performance pad now has a four-dot live rail for Banks 1–4. QLC+'s amber running border follows the raw Chaser during manual playback, Auto Bank, Auto All, and seek, including Auto All bank transitions.
- The native Autoplay tracker is large enough to show and automatically select the exact current loop name and step.
- The Live page is rebuilt as a 1600×900 dark performance surface with consistent spacing, larger targets, clearer grouping, a non-overlapping transport status indicator, better bank/scope controls, and a cleaner override/intensity rail.
- All 2,090 lighting Functions, fixtures, addresses, I/O routes, Priority Looks, public Function IDs, and Control One logical channels are unchanged from V22.

## Live workflow

| Control | Behavior |
|---|---|
| 32 pads | Latch/replace an Autoloop or toggle one Priority Look |
| Auto Loop | Toggle the shared surface between Autoloops and Priority Looks |
| Banks 1–4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank / Start All | Automatic sequential or random playback |
| Dwell | 1, 2, 4, 8, or 16 measures; changes while running |
| Speed | 0.25×, 0.5×, 1×, 2×, or 4×; independent from dwell |
| Priority Look | Sole full-frame output until released; underlying Autoloop continues |
| Color override | Changes color only while intensity/movement continue |
| Play/Pause | Starts or pauses the selected manual/automatic owner |
| Stop | Emergency global stop |

The four live-rail dots beside each pad are Bank 1, 2, 3, and 4 from top to bottom. The amber dot is the raw Chaser currently running. The tracker below the grid gives the complete name.

## Included example rig

The workspace is ready for:

- four Both Lighting IR-4 fixtures, 10-channel mode, addresses 1, 11, 21, and 31;
- four Both Lighting BO-TUBE192 fixtures, 40-channel mode, addresses 175, 215, 255, and 295; and
- private copies on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal. Do not route it directly to a physical DMX output.

Another fixture rig can still use the plug-in and Control One mapping, but its QLC+ fixture patch and creative Functions must be adapted. See the repository's `docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md` before editing the template.

## Exact compatibility

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| `soundswitch.dll` SHA-256 | `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D` |

The DLL is a QLC+ binary module, not a universal driver. The installer rejects a different QLC+ executable unless deliberately overridden after a compatible rebuild/test.

## Rollback

V23 does not overwrite V21 or V22. To roll back the show, close V23 and open the earlier `.qxw`.

If the plug-in itself was installed and must be restored, close QLC+ and run:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

## Five-minute first test

1. Click `AUTOLOOPS ⇄ PRIORITY LOOKS` twice and confirm the page changes both ways.
2. Latch one manual pad in each bank.
3. Start Auto Bank, then Auto All. Confirm the amber live rail and selected tracker row advance.
4. Change dwell and speed while Autoplay stays running.
5. Toggle one still and one moving Priority Look; release it and confirm the underlying Autoloop returns.
6. Check one color override plus Global, IR-4, and tube intensity.
7. Confirm the chosen Micro or Control One port reaches the fixtures.

V23 is structurally validated and reuses the software-tested V21/V22 runtime. The new UI feedback and layout still require this short owner observation. Repeated hot-plug, simultaneous dual-port operation, and the combined two-hour DJ workload remain gig-qualification gates.

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, or QLC+.
