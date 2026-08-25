# Validation and Maintenance

## Fast regression after any workspace change

1. Confirm only one QLC+ process is running.
2. Open the intended `.qxw` and verify Universe 1 output.
3. Click every Bank and confirm the 4×8 pad grid changes while pad inputs remain 0–31.
4. Toggle the persistent Autoloop/Priority Looks mouse control twice and confirm both directions.
5. Latch one manual loop in every bank; confirm Note Off does not stop it, same-pad stops it, and another pad replaces it.
6. Start Auto Bank and Auto All in sequential and random modes.
7. While Autoplay runs, confirm the amber four-bank live rail and selected tracker row advance, change dwell through 1/2/4/8/16, and seek with several pads. The parent must remain running and both readouts must follow the raw Chaser.
8. Change chase speed separately through 0.25x–4x.
9. Apply and release a still and moving Priority Look. It must be sole authority while the underlying loop continues.
10. Apply every color override and confirm non-color behavior continues.
11. Check Global, Group 1 IR-4, and Group 3 Tubes intensity; reserved groups must not affect fixtures.
12. Confirm VirtualDJ OS2L BPM and Control One LEDs.

For V24, first confirm all four clickable Bank selectors, the two-way mode switch, one manual pad per bank, latched and advancing Auto Bank/Auto All live rails and tracker rows, live dwell changes, all five chase-speed presets, one still and one moving Priority Look, color overrides, Global/IR-4/tube intensity, and the selected DMX output. Then launch VirtualDJ before QLC+ and confirm OS2L connects within five seconds; unplug/replug Control One while a manual loop is selected; confirm mouse controls remain available, MIDI returns without restarting QLC+, the known LEDs restore, Play/Pause works, and DMX resumes. Repeat once with QLC+ launched first.

## Automated structural checks

Run the V24 local release check from the repository root. V24 is intentionally published without GitHub Actions:

```powershell
releases/qlcplus-control-one/v24/Test-V24Package.ps1 `
  -SourceWorkspace releases/qlcplus-control-one/v23/IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw
```

The packaged script is also safe to run without `-SourceWorkspace` after downloading the standalone release archive.

- Parse workspace and profile XML.
- Require unique Function IDs and actual Virtual Console widget IDs.
- Resolve all Chaser/Collection Function references and all Scene fixture IDs.
- Require exactly 128 raw Autoloops, 128 manual owner Collections, ten Autoplay controls, and ten Autoplay parents.
- Require the exact 22 Variety Pro roots and their 176 imported Scene steps.
- Require 32 visible disabled live-loop frames to cover all 128 raw Chasers exactly once, remain outside the owner SoloFrame, own no external Input, and provide four non-overlapping bank indicators per pad.
- Require one persistent Function `1993` / logical channel `811` mode control and reject the duplicate V22 widget.
- Require all four bank UI channels on every bank page, one unified Surface feedback line, and the separate private Priority output.
- Require all 128 raw Autoloops to receive the 0.25x/0.5x/1x/2x/4x speed presets exactly once and all ten Autoplay parents to receive 1/2/4/8/16-measure dwell.
- Require the 1600×900 Live page, ten enlarged native trackers, and a clickable Play/Pause target unobscured by its status panel.
- Require the four physical IR-4 fixtures at addresses 1/11/21/31 in 10-channel mode.
- Require the four physical BO-TUBE192 fixtures at 175/215/255/295 in 40-channel mode.
- Require matching private Priority Layer duplicates on Universe 3.
- Reject personal paths, usernames, hardware serials, tokens, or secrets from published artifacts.
- Preserve every V23 lighting Function, fixture, and Priority Look. V24's only Input/Output delta is removal of the duplicate Priority feedback declaration.

## Hardware qualification

- Micro: start attached; attach after launch; active unplug/replug; blackout/restore; two-hour output.
- Control One: MIDI plus DMX 1; DMX 2 independently; both ports with different data; active unplug/replug.
- Combined: VirtualDJ audio, OS2L, Control One MIDI/feedback, Control One DMX 1/2, and Micro when available for at least two hours.
- Confirm there is no repeating USB error storm, frozen MIDI, visible DMX flicker, UI starvation, or audio disruption.

## Upgrade rule

The DLL is not a version-independent drop-in. V24 includes a new binary pinned to QLC+ commit `a124abebe0b5ad6077727c561a5a0e1f3730810c` and the installed core executable hash. Keep the executable, QLC plug-ins, Qt DLLs, FFmpeg/runtime DLLs, `.qxi`, and `.qxw` as one rollback bundle.

Use the release installer only while QLC+ is closed. It verifies the core and plug-in hashes, saves the previous DLL with a receipt, and verifies the installed copy. Use the paired rollback script to restore that receipt-backed DLL. For a newer official QLC+ build, install side-by-side and rebuild first; do not force the old DLL into the new core for show use.

Never solve missing-DLL or corrupted-text problems by mixing files from different QLC+ builds. Use a complete, coherent installation.

## Safe editing rules

- Preserve public Function IDs and logical channels; Control One and Virtual Console mappings depend on them.
- Back up the `.qxw` before editing.
- Use actual workspace fixture IDs, not inferred addresses, when changing Scenes.
- For creative-only passes, do not alter the Virtual Console or control layer.
- For UI/control-only passes, prove creative Functions are unchanged.
- Keep machine paths and hardware serials out of repository workspaces.
