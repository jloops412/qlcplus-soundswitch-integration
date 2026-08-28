# QLC+ SoundSwitch V26 — Autoplay Clarity

V26 is the current Windows alpha package for using SoundSwitch Micro or Control One hardware directly from QLC+ with a SoundSwitch-familiar live workflow.

QLC+ owns the complete show workflow. V26 adds only the two build-matched plug-ins and the project/profile files described below.

## Lean architecture

```text
VirtualDJ ── direct OS2L ───────────────▶ QLC+
Control One ── MIDI ───────────────────▶ QLC+
QLC+ Chasers / Scenes / Virtual Console │
QLC+ ── SoundSwitch output plug-in ─────┼─▶ Micro DMX
                                        ├─▶ Control One DMX 1
                                        └─▶ Control One DMX 2
```

QLC+ owns the show: fixtures, Scenes, Chasers, Autoplay order, beat counting, dwell, speed, priority behavior, bank pages, and live Function state. The SoundSwitch plug-in remains limited to hardware transport, Control One MIDI/LED/reconnect behavior, Priority Look ownership, and the current rig's intensity routing.

V26 includes one additional build-matched QLC+ plug-in fix: `os2l.dll` clocks QLC+ from the BPM reported by OS2L instead of treating bursty network-packet arrival as the beat clock. There is still no second timing process.

## Upgrade from V21–V25

1. Close SoundSwitch and every QLC+ window.
2. Extract the complete V26 folder.
3. In File Explorer, open the folder containing your pinned `qlcplus5.exe`.
   The normal Windows location is `C:\QLC+`.
4. Open its `Plugins` folder and back up the existing `soundswitch.dll` and
   `os2l.dll`.
5. Copy the V26 `soundswitch.dll` and `os2l.dll` into that `Plugins`
   folder, replacing the old copies.
6. Keep `SoundSwitch-Control-One-Performance.qxi` beside
   `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`, then open the `.qxw`
   file in QLC+.

V26 replaces no executable and needs no PowerShell command for normal use.

## First installation

1. Install the complete QLC+ **5.3.0 GIT a124abe** build, normally at
   `C:\QLC+`. Do not mix the executable, Qt DLLs, or plug-ins from different
   QLC+ builds.
2. Install the normal manufacturer software/driver for the SoundSwitch device,
   then close SoundSwitch and QLC+.
3. Back up `C:\QLC+\Plugins\soundswitch.dll` and
   `C:\QLC+\Plugins\os2l.dll` if they exist. If QLC+ is installed
   elsewhere, use the `Plugins` folder beside that installation's
   `qlcplus5.exe`.
4. Copy the V26 `soundswitch.dll` and `os2l.dll` into that `Plugins`
   folder.
5. Leave `SoundSwitch-Control-One-Performance.qxi` beside the V26 workspace
   and open `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
6. In QLC+ Input/Output, select SoundSwitch Micro, Control One DMX 1, or
   Control One DMX 2.
7. Associate `SoundSwitch-Control-One-Performance.qxi` with Control One MIDI
   if QLC+ did not retain it.
8. Follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` when VirtualDJ shares the
   computer.

QLC+ searches for custom input profiles beside the project, so the `.qxi`
does not need a system-wide installer. The included PowerShell files are
optional maintainer tools for checksum validation, automated backups, and
repeatable lab deployment.

Do not replace the whole Control One composite USB driver with a generic driver;
that can remove its MIDI interface.

## What V26 changes

- Keeps the reviewed V25 Engine byte-for-byte: all **2,090 Functions**, fixtures, addresses, creative content, Priority Looks, mappings, and I/O routing are unchanged.
- Removes the visible/heavy tracker concept. QLC+'s ten Autoplay Chasers are the sequencing engine; ten clipped native Cue Lists retain absolute seek; 128 read-only Function monitors provide current-loop feedback.
- Expands the tiny monitor rails into a clear full-width strip below every pad. Quiet inactive segments make QLC+'s stock amber Monitoring border unmistakable as manual play, Start Bank, or Start All advances.
- Restores visible dwell choices on every QLC+ multipage state: **1M, 2M, 4M, 8M, 16M**.
- Keeps dwell musical and live-adjustable: **4, 8, 16, 32, or 64 beats**, assuming four beats per measure.
- Keeps Chase Speed independent from dwell: **0.25×, 0.5×, 1×, 2×, 4×**.
- Prevents OS2L packet bursts from producing impossible readings such as a 75 BPM song appearing near 240 BPM.

### Active-loop color

Stock QLC+ displays a Function started by another Chaser in its native **amber Monitoring** state. V26 deliberately uses that built-in state rather than patching the QLC+ executable merely to recolor it. A manually latched button still uses QLC+'s normal active state. The full-width strip makes the current Autoplay loop obvious while preserving an updateable stock core.

## Live workflow

| Control | Behavior |
|---|---|
| 32 pads | Latch/replace one Autoloop, or toggle one exclusive Priority Look |
| Auto Loop | Switch the shared pad surface between Autoloops and Priority Looks |
| Banks 1–4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank | Run the selected bank from the current loop, sequentially or randomly |
| Start All | Run Banks 1→4 continuously from the current loop, sequentially or randomly |
| Dwell | Stay on each loop for 1/2/4/8/16 measures; change it while Autoplay is running |
| Chase Speed | Scale loop motion at 0.25×–4× without changing dwell or restarting Autoplay |
| Priority Look | Take sole full-frame authority; the underlying Autoloop keeps advancing and returns on release |
| Color override | Replace color only; unrelated intensity or movement continues |
| Play/Pause | Control the selected playback owner |
| Stop | Emergency global stop |

QLC+ Chasers provide the actual bank/all-bank playback. The project contains no independent state poller. The read-only strips observe the 128 raw Chasers directly, so the indicator follows whichever loop is genuinely running.

## Included rig

- 4 × Both Lighting IR-4, 10-channel mode, DMX addresses **1, 11, 21, 31**;
- 4 × Both Lighting BO-TUBE192, 40-channel mode, DMX addresses **175, 215, 255, 295**; and
- matching private fixtures on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal. Do not route it directly to physical DMX.

Another DJ can reuse the plug-ins and Control One profile. A different fixture rig needs its own QLC+ patch and creative Functions.

## OS2L behavior

The pinned stock OS2L plug-in receives a structured beat message containing BPM, but it ignores that BPM and emits a QLC+ beat for every message received. When messages arrive in a burst, QLC+ measures the burst interval instead of the song tempo.

V26's build-matched `os2l.dll`:

- reads the sender's reported BPM;
- emits one precise QLC+ beat at that interval;
- treats later packets as tempo/keepalive updates instead of extra beats;
- stops the clock when the source disconnects or goes silent; and
- retains a compatibility fallback for senders that omit BPM.

VirtualDJ remains the source and connects directly to QLC+. The Chase Speed multiplier is still available for intentional half-time/double-time show feel; it does not rewrite the detected song BPM.

## Five-minute owner check

1. Confirm all five dwell values remain visible and the selected value changes without stopping Autoplay.
2. Latch one manual pad in each bank.
3. Start Bank and watch the native amber strip move to the next pad after the selected dwell.
4. Start All and confirm the running strip progresses through Bank 1, then Bank 2, Bank 3, and Bank 4.
5. Change dwell and Chase Speed while playback continues.
6. Toggle one still and one moving Priority Look, then release it and confirm the underlying Autoloop returns.
7. With a known song in VirtualDJ, confirm QLC+ holds approximately the reported BPM instead of jumping to a packet-burst rate.
8. Confirm the selected Micro or Control One DMX port reaches fixtures.

## Exact compatibility

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` stock core |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Qt build headers | `6.8.1` |
| `soundswitch.dll` SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |
| `os2l.dll` SHA-256 | `EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513` |
| Workspace SHA-256 | `ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570` |

Both DLLs are QLC+ modules matched to this exact build, not universal Windows drivers. Install future QLC+ versions side-by-side, rebuild the two plug-ins against that source, and qualify the resulting tuple before changing a show machine.

## Validation status

V26 is structurally validated and both modified components compile against the pinned source. Prior owner testing confirmed Micro output, Control One DMX 1, Control One DMX 2 independently, Control One MIDI, the essential LED workflow, OS2L connectivity, Priority Look takeover/release, and the core Autoloop workflow.

The final V26 owner check above still requires observation in the real QLC+ window and, for light output, the physical rig. Repeated hot-plug, both Control One DMX ports simultaneously, and a combined two-hour VirtualDJ/audio/OS2L/MIDI/LED/DMX workload remain gig-qualification gates.

## Rollback

1. Close QLC+.
2. Remove the V26 `soundswitch.dll` and `os2l.dll` from
   `C:\QLC+\Plugins` (or your actual QLC+ `Plugins` folder).
3. Copy your backed-up DLLs into that folder. If no backup exists, reinstall the
   exact pinned QLC+ build.
4. Open V25, V24, or another preserved `.qxw` file.

The workspace itself is non-destructive. `Rollback-V26.ps1` remains available
only as an optional maintainer convenience when V26 was installed with the
matching scripted backup flow.

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, VirtualDJ, or QLC+.
