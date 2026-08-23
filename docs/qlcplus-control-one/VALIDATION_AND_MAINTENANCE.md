# Validation and Maintenance

## Fast regression after any workspace change

1. Confirm only one QLC+ process is running.
2. Open the intended `.qxw` and verify Universe 1 output.
3. Click every Bank and confirm the 4×8 pad grid changes while pad inputs remain 0–31.
4. Latch one manual loop in every bank; confirm Note Off does not stop it, same-pad stops it, and another pad replaces it.
5. Start Auto Bank and Auto All in sequential and random modes.
6. While Autoplay runs, change dwell through 1/2/4/8/16 and seek with several pads. The parent must remain running.
7. Change chase speed separately through 0.25x–4x.
8. Apply and release a still and moving Priority Look. It must be sole authority while the underlying loop continues.
9. Apply every color override and confirm non-color behavior continues.
10. Check Global, Group 1 IR-4, and Group 3 Tubes intensity; reserved groups must not affect fixtures.
11. Confirm VirtualDJ OS2L BPM and Control One LEDs.

## Automated structural checks

- Parse workspace and profile XML.
- Require unique Function IDs and actual Virtual Console widget IDs.
- Resolve all Chaser/Collection Function references and all Scene fixture IDs.
- Require exactly 128 raw Autoloops, 128 manual owner Collections, and ten Autoplay owners.
- Require the four physical IR-4 fixtures at addresses 1/11/21/31 in 10-channel mode.
- Require the four physical BO-TUBE192 fixtures at 175/215/255/295 in 40-channel mode.
- Require matching private Priority Layer duplicates on Universe 3.
- Reject personal paths, usernames, hardware serials, tokens, or secrets from published artifacts.
- Compare creative Function signatures before and after control/UI-only changes.

## Hardware qualification

- Micro: start attached; attach after launch; active unplug/replug; blackout/restore; two-hour output.
- Control One: MIDI plus DMX 1; DMX 2 independently; both ports with different data; active unplug/replug.
- Combined: VirtualDJ audio, OS2L, Control One MIDI/feedback, Control One DMX 1/2, and Micro when available for at least two hours.
- Confirm there is no repeating USB error storm, frozen MIDI, visible DMX flicker, UI starvation, or audio disruption.

## Upgrade rule

The DLL is not a version-independent drop-in. Pin one official QLC+ release or commit, build the plug-in with the same compiler/Qt/runtime set, and keep the executable, QLC plug-ins, Qt DLLs, FFmpeg/runtime DLLs, `.qxi`, and `.qxw` as one rollback bundle.

Never solve missing-DLL or corrupted-text problems by mixing files from different QLC+ builds. Use a complete, coherent installation.

## Safe editing rules

- Preserve public Function IDs and logical channels; Control One and Virtual Console mappings depend on them.
- Back up the `.qxw` before editing.
- Use actual workspace fixture IDs, not inferred addresses, when changing Scenes.
- For creative-only passes, do not alter the Virtual Console or control layer.
- For UI/control-only passes, prove creative Functions are unchanged.
- Keep machine paths and hardware serials out of repository workspaces.
