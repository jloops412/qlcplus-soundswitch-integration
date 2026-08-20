# 2026 Pilot 01 failure and source-map correction — 2026-08-18

## Status

The generated file `01_medium_slots_01-04_SOURCE_PILOT.emberlights` should **not** be treated as a valid current-project SoundSwitch reconstruction.

Physical feedback: user reported it does not match the original SoundSwitch Autoloops at all.

The project structurally validates, but the migration logic was wrong. This is exactly why schema validation is not sufficient for SoundSwitch migration fidelity.

## Root cause found after physical feedback

The Pilot 01 decoder reused target IDs from an older SoundSwitch Shortcut lineage for the BO-TUBE192 row:

- wrong / old mapping used in Pilot 01:
  - Tube 1 group/cells: `345 / 346-361`
  - Tube 2 group/cells: `362 / 363-378`
  - Tube 3 group/cells: `379 / 380-395`
  - Tube 4 group/cells: `396 / 397-412`

Those IDs do appear in the current `2026.ssproj` files, but they are **not** the current 2026 venue's four BO-TUBE192 fixtures at DMX 41/121/201/281. Using them caused visibly wrong color beds. The user's runtime snapshot for `Red - Smooth Pulse` showed RGB values that match this wrong source-family contamination rather than the intended current tube row.

## Current 2026 venue mapping evidence

Parsing `2026.ssproj/SoundSwitchVenues.bin` around the current rig records gives the actual destination source IDs:

- `4 Uplights`
  - group `197`
  - children `198-201`
  - profile text: `6x18W RGBWA UV 6in1 Uplight (BO-S601)`

- `Bo-Tube 1`
  - group `89`
  - cells `90-105`

- `BO-Tube 2`
  - group `106`
  - cells `107-122`

- `BO-Tube 3`
  - group `123`
  - cells `124-139`

- `BO-Tube 4`
  - group `140`
  - cells `141-156`

The BO-TUBE192 row is therefore not at `345/362/379/396` in the current 2026 project.

## Additional lesson

The first four Medium slots still appear to be correctly identified by the decoded placement array:

1. `SSAutoLoop1.ssfile` — `Red - Smooth Pulse`
2. `SSAutoLoop2.ssfile` — `Blue - Smooth`
3. `SSAutoLoop7.ssfile` — `Sunny - Smooth`
4. `SSAutoLoop6.ssfile` — `80s - Smooth`

But the translation was invalid because fixture target mapping was wrong.

## Next corrective approach

Do not generate the next file until the following is complete:

1. Produce a source-map verification report from `2026.ssproj/SoundSwitchVenues.bin`, not from older projects.
2. Re-decode only Medium slot 1 first (`Red - Smooth Pulse`) using:
   - uplight group/fixtures `197-201`;
   - tube groups/cells `89-156`;
   - no fallback to unrelated source IDs.
3. If the uplights have intensity but no local color records, do **not** borrow colors from unrelated target IDs. Treat that as an explicit `MissingColorSource` / `NeedsColorContext` blocker or use only a clearly labeled controlled test fallback.
4. Generate a one-Autoloop probe/pilot before continuing with slots 2-4.
5. Ask the user to compare that single result against the real SoundSwitch behavior before expanding.

## Product/importer implications

- Target IDs must be resolved from the exact source project's Venue database per source identity. They are not stable across Shortcut versions or user projects.
- Migration reports must include the source ID map used for every fixture group/cell.
- If a target's color lane is missing, importer output must mark the gap; it must not invent or borrow color unless the rule is source-proven and reported.
- Source-fidelity acceptance requires physical/user comparison, not just project validation.
