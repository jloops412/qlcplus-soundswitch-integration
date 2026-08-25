# QLC+ SoundSwitch V24 Runtime Feedback

V24 is the current Windows alpha package for running SoundSwitch Micro or Control One hardware directly from QLC+. It keeps QLC+ as the only lighting application and fixes the mouse-control and live-feedback failures found in V23.

## Upgrade from V21, V22, or V23

V24 includes a new plug-in. Install it even if an earlier package already works.

1. Close QLC+.
2. Extract the complete V24 folder.
3. Open PowerShell in that folder and run:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V24Package.ps1
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

4. Open `IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw`.

The installer verifies the exact QLC+ core and plug-in hashes, saves the previous DLL, and writes a rollback receipt.

## First installation

1. Install one complete QLC+ build matching **5.3.0 GIT a124abe**. Never mix its executable, Qt/runtime DLLs, or plug-ins with another build.
2. Install the normal manufacturer driver/software for the SoundSwitch device, then close SoundSwitch.
3. Follow the upgrade commands above.
4. In QLC+ Input/Output, choose SoundSwitch Micro, Control One DMX 1, or Control One DMX 2.
5. For Control One MIDI, associate `SoundSwitch-Control-One-Performance.qxi` if QLC+ did not retain it.
6. If VirtualDJ shares the computer, follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md`.

Do not replace the entire Control One composite USB driver with a generic driver; that can remove its MIDI interface.

## What V24 fixes

- The on-screen Bank 1–4 buttons now select their native bank pages.
- `AUTOLOOPS ⇄ PRIORITY LOOKS` switches correctly in both directions.
- Mouse commands are processed once; an empty QLC+ command Scene's trailing zero no longer repeats the action.
- QLC+'s single feedback destination now carries both surface commands and Priority Look ownership safely.
- Start Bank and Start All stay latched.
- The currently running raw Autoloop is visible for manual play, Auto Bank, Auto All, and seek. The indicator moves as the parent advances.
- Chase speed remains independent from autoplay dwell: **0.25×, 0.5×, 1×, 2×, 4×** versus **1, 2, 4, 8, 16 measures**.
- Mouse controls remain usable when Control One is temporarily unplugged; MIDI/LED reconnect continues in the background.

V24 preserves all 2,090 lighting Functions, fixtures, addresses, Priority Looks, Autoloops, public Function IDs, and logical channels from V23.

## Live workflow

| Control | Behavior |
|---|---|
| 32 pads | Latch/replace one Autoloop, or toggle one exclusive Priority Look |
| Auto Loop | Toggle the shared pad surface between Autoloops and Priority Looks |
| Banks 1–4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank / Start All | Automatic sequential or random playback |
| Dwell | Stay on each loop for 1/2/4/8/16 measures; adjustable while running |
| Chase speed | Scale the loop at 0.25×/0.5×/1×/2×/4× without changing dwell |
| Priority Look | Sole full-frame authority; the underlying loop keeps advancing |
| Color override | Replace color only; unrelated intensity/movement continues |
| Play/Pause | Control the selected playback owner |
| Stop | Emergency global stop |

The four small indicators beside each pad represent Banks 1–4 from top to bottom. QLC+'s amber Monitoring state marks the raw Chaser that is currently running. The tracker under the grid shows the complete loop name and step.

## Included rig

- 4 × Both Lighting IR-4, 10-channel mode, addresses **1, 11, 21, 31**;
- 4 × Both Lighting BO-TUBE192, 40-channel mode, addresses **175, 215, 255, 295**; and
- matching private fixtures on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal. Do not route it directly to physical DMX.

Another DJ can reuse the plug-in and Control One profile, but a different fixture rig needs its own QLC+ patch and creative Functions. See the repository's community migration guide.

## BPM and speed

VirtualDJ OS2L remains the authoritative beat source. A BPM display can move when the source changes its estimate, especially during transitions, weak intros, or tempo analysis corrections. Do not add a second beat generator to the same OS2L port.

Use the separate Chase Speed control when VirtualDJ reports half-time/double-time or when a look should move faster or slower. It scales every one of the 128 raw Autoloops while preserving beat alignment and does not restart or change autoplay dwell.

## Five-minute check

1. Click all four Bank buttons and confirm the 4×8 pad surface changes.
2. Click `AUTOLOOPS ⇄ PRIORITY LOOKS` twice.
3. Latch one manual pad in each bank.
4. Start Auto Bank and Auto All; confirm the Start button stays active and the live indicator advances.
5. Change dwell and chase speed while autoplay remains running.
6. Toggle one still and one moving Priority Look, then release it and confirm the underlying loop returns.
7. Check one color override plus Global, IR-4, and tube intensity.
8. Confirm the selected Micro or Control One DMX port reaches fixtures.

## Exact compatibility

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| `soundswitch.dll` SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |

The DLL is a build-matched QLC+ module, not a universal driver. Rebuild and qualify it before using another QLC+/Qt build.

## Validation status

V24 is structurally validated and software-tested against the pinned build. The isolated runtime test confirmed Bank selection, two-way mode switching, live speed selection, latched Start Bank/All, parent progression, and the current-loop indicator following that progression without sending data to VirtualDJ or physical DMX.

Previous releases provide physical evidence for Micro, each Control One DMX port independently, Control One MIDI/LEDs, OS2L, and the core Priority Look workflow. V24 still needs the short owner check above with fixtures. Repeated hot-plug, simultaneous dual-port operation, and the combined two-hour DJ workload remain gig-qualification gates.

## Rollback

Close QLC+ and run:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

You can also close V24 and reopen the preserved V23, V22, or V21 `.qxw` file.

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, or QLC+.
