# Current 2026 SoundSwitch project — Pilot 01 checkpoint

Checkpoint date: 2026-08-18

This note supersedes using older SoundSwitch Shortcut v1/v2 source packages as the user's current migration baseline. Those packages remain useful for decoder comparison, but the uploaded `2026.ssproj.zip` is the active source for this pilot.

## Source identity

- Uploaded source archive: `2026.ssproj.zip`
- Project folder: `2026.ssproj`
- Project ID: `{7CED9022-5F3E-4154-9C40-E5592BE8F145}`
- SoundSwitch marker: `2.10.0.3`
- Archive SHA-256 observed during pilot work: `2c58ed57965cd12a0702252595d4966ef8caef4a3b024e24bc001e245fcfe11c`
- Files in archive: 4,185
- Root `SSAutoLoop*.ssfile` files: 112

The primary database contains 32 Autoloop entries; `SoundSwitchAutoLoopsEx.bin` contains 80 additional entries. The decoded placement arrays resolve 112 DB-listed Autoloop placements across the four source banks:

- Medium: 32
- Colorful: 32
- Slow Dance: 32
- Flashy: 16

## Important ordering correction

Do **not** use raw entry-list order as bank/slot order.

The decoded Medium placement array starts:

1. `SSAutoLoop1.ssfile` — `Red - Smooth Pulse`
2. `SSAutoLoop2.ssfile` — `Blue - Smooth`
3. `SSAutoLoop7.ssfile` — `Sunny - Smooth`
4. `SSAutoLoop6.ssfile` — `80s - Smooth`
5. `SSAutoLoop8.ssfile` — `teal+Pink - Square`
6. `SSAutoLoop3.ssfile` — `Purp+Yelo - Square`
7. `SSAutoLoop5.ssfile` — `Green+blu - Wave`
8. `SSAutoLoop4.ssfile` — `Blue - Pulse`

Earlier conversation notes that treated slots 3/4 as raw entries were wrong.

## Pilot 01 output strategy

Preview 101 format-1 Autoloops remain capped at whole-Static-Look steps, max 32 steps per loop, and project limits of 256 Looks / 32,768 assignments. The pilot therefore intentionally reconstructs only Medium slots 1–4.

Pilot content:

- B1/S1 `Red - Smooth Pulse`
- B1/S2 `Blue - Smooth`
- B1/S3 `Sunny - Smooth`
- B1/S4 `80s - Smooth`

The pilot uses 23 sampled source states per loop:

- 92 helper Static Looks total;
- 32,384 Look assignments;
- 4 Autoloops;
- each Autoloop remains below 32 steps;
- U1 patch remains contiguous 001–360 for the current 4×IR-4 + 4×BO-TUBE192 destination.

## Decoder/translation findings used in Pilot 01

Current destination mapping:

- source uplights 198–201 -> IR-4 #1–4;
- source Tube 1 group/cells 345 / 346–361 -> Tube #1 cells;
- source Tube 2 group/cells 362 / 363–378 -> Tube #2 cells;
- source Tube 3 group/cells 379 / 380–395 -> Tube #3 cells;
- source Tube 4 group/cells 396 / 397–412 -> Tube #4 cells.

For this pilot:

- source A-records are translated as normalized intensity curves;
- source B-records are translated as timed RGB plus direct Amber/White/UV endpoint colors;
- tube-cell intensity is multiplied into RGBWY emitters because the current virtual tube profile has no master;
- IR-4 strobe is force-zeroed in every helper state;
- if a source uplight owns intensity but no local color, the pilot borrows the same-time decoded source color from the corresponding tube group as a deterministic current-rig color bed. This is **DeterministicallyTranslated**, not exact. It avoids black standalone IR output without inventing colors from loop names.

## Validation result

The generated pilot project passed an independent structural audit:

- project header and CRC round-trip;
- profile offsets and property support;
- fixture/profile references;
- no DMX overlaps;
- U1 001–360 contiguous;
- 92 Looks / 32,384 assignments;
- four Autoloops with 23 steps each;
- first step at beat zero;
- ordered/in-range timing;
- all Look references valid;
- Cut/Linear transitions only.

## Next-step discipline

Do not expand to a full 112-loop conversion until this pilot is physically evaluated. If the first four loops are wrong, refine the decoder and translation rules before generating slots 5–8.

Specific physical questions for Pilot 01:

1. Does B1/S1 read as `Red - Smooth Pulse` on the IR-4s/tubes?
2. Does B1/S2 read as `Blue - Smooth`?
3. Does B1/S3 read as `Sunny - Smooth`?
4. Does B1/S4 read as `80s - Smooth`?
5. Are the IR-4s visibly colored and not black when they pulse?
6. Are snap/cut moments musical rather than accidental-looking?
7. Are tube directions/cell order correct once tubes are physically connected?

Document observed mismatches as decoder evidence, not as a request for more hand-authored replacement packs.