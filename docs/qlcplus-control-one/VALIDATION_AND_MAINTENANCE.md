# Validation and Maintenance

## Fast regression after any workspace change

1. Confirm only one QLC+ process is running.
2. Open the intended `.qxw` and verify Universe 1 output.
3. Click every Bank and confirm the 4 x 8 pad grid changes while pad inputs
   remain 0-31.
4. Toggle the persistent Autoloop/Priority Looks mouse control twice and confirm
   both directions.
5. Latch one manual loop in every bank; confirm Note Off does not stop it,
   same-pad stops it, and another pad replaces it.
6. Start Auto Bank and Auto All in sequential and random modes.
7. While Autoplay runs, confirm the full-width native amber strip advances,
   change dwell through 1M/2M/4M/8M/16M, and seek with several pads. The parent
   must remain running and the strip must follow the raw Chaser.
8. Change Chase Speed separately through 0.25x-4x.
9. Apply and release a still and moving Priority Look. It must be sole authority
   while the underlying loop continues.
10. Apply every color override and confirm non-color behavior continues.
11. Check Global, Group 1 IR-4, and Group 3 Tubes intensity; reserved groups
    must not affect fixtures.
12. Confirm VirtualDJ OS2L BPM and Control One LEDs.

For V26, also use a known VirtualDJ track and confirm QLC+ stays near its
reported BPM instead of jumping to a packet-burst rate. Unplug/replug Control
One while a manual loop is selected; confirm mouse controls remain available,
MIDI returns without restarting QLC+, known LEDs restore, Play/Pause works, and
DMX resumes. Repeat once with QLC+ launched first.

## Automated structural checks

Run the V26 release check from the repository root:

```powershell
releases/qlcplus-control-one/v26/Test-V26Package.ps1
```

Run the focused workspace validator after workspace changes:

```powershell
qlcplus/workspace-tools/Test-V26Workspace.ps1 `
  -SourceWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw `
  -CandidateWorkspace releases/qlcplus-control-one/v26/IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw
```

The active GitHub Actions workflow runs package and repository-hygiene checks on
pull requests and `main`. It does not build against private machine-local
toolchains and does not replace physical testing.

Current structural validation covers:

- workspace and profile XML;
- unique Function and Virtual Console widget IDs;
- Chaser/Collection Function references and Scene fixture IDs;
- exactly 128 raw Autoloops, 128 manual owners, ten Autoplay controls, and ten
  Autoplay parents;
- 32 visible read-only strips covering all 128 raw Chasers exactly once;
- one persistent Function `1993` / logical channel `811` mode control;
- all four bank UI channels and one unified Surface feedback line;
- 0.25x/0.5x/1x/2x/4x speed coverage across all raw Autoloops;
- 1/2/4/8/16-measure dwell across all Autoplay parents;
- absolute seek coverage and no visible/polling tracker;
- all five visible dwell controls on every native dwell page;
- the four IR-4 fixtures at addresses 1/11/21/31 in 10-channel mode;
- the four BO-TUBE192 fixtures at 175/215/255/295 in 40-channel mode;
- matching private Priority fixtures on Universe 3;
- absence of personal paths, usernames, serials, tokens, or secrets in the
  published package;
- exact V26 workspace, SoundSwitch, and OS2L hashes; and
- byte-identical preservation of the reviewed V25 Engine.

## Hardware qualification

- Micro: attached at start, attach after launch, active unplug/replug,
  blackout/restore, and two-hour output.
- Control One: MIDI plus DMX 1, DMX 2 independently, both ports with different
  data, and active unplug/replug.
- Combined: VirtualDJ audio, OS2L, Control One MIDI/feedback, Control One DMX
  1/2, and Micro when available for at least two hours.
- Confirm no repeating USB error storm, frozen MIDI, visible DMX flicker, UI
  starvation, or audio disruption.

Disconnect fog, sparks, lasers, pyrotechnics, and other hazardous loads during
general testing. Start with a tester or simple low-risk LED fixture.

## Upgrade rule

The DLLs are not version-independent drop-ins. V26 is pinned to QLC+ commit
`a124abebe0b5ad6077727c561a5a0e1f3730810c`, UI `5.3.0 GIT a124abe`, and its
documented Qt/compiler runtime.

Keep the executable, plug-ins, Qt/FFmpeg/runtime DLLs, `.qxi`, and `.qxw` as one
rollback bundle. Use the release installer only while QLC+ is closed. For a new
official QLC+ build, install side by side and rebuild first. Never solve missing
DLLs or corrupted text by mixing runtime files between QLC+ installations.

## Safe editing rules

- Preserve public Function IDs and logical channels.
- Back up the `.qxw` before editing.
- Use actual workspace fixture IDs when changing Scenes.
- For creative-only work, prove the Virtual Console/control layer is unchanged.
- For UI-only work, prove the Engine and creative Functions are unchanged.
- Keep machine paths, serials, private shows, and credentials out of the
  repository.
- Do not replace a released file or checksum in place. Create a new immutable
  versioned package.
