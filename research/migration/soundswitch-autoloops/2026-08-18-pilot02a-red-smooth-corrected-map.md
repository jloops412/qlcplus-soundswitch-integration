# 2026 Pilot 02A — Red - Smooth Pulse corrected-map probe

Checkpoint date: 2026-08-18

## Purpose

This probe is intentionally small: one Autoloop only, `Medium / S1 / Red - Smooth Pulse`, from the uploaded current `2026.ssproj.zip` source. It exists to isolate the SoundSwitch decoder and target-map correction before expanding to more loops.

## Source identity

- Source archive: `2026.ssproj.zip`
- Archive SHA-256: `2c58ed57965cd12a0702252595d4966ef8caef4a3b024e24bc001e245fcfe11c`
- Source project folder: `2026.ssproj`
- SoundSwitch marker: `2.10.0.3`
- Source loop: `2026.ssproj/SSAutoLoop1.ssfile`
- Loop SHA-256: `ca82522401feee11cf8ea8710da8f8328463648da25997c7f77303dc5392f73d`

## Current target map used

This probe uses the corrected current `2026.ssproj/SoundSwitchVenues.bin` target map:

- uplights: source `198-201` -> destination IR-4 #1-4;
- Tube 1 cells: source `90-105` -> destination Tube 1 cells 1-16;
- Tube 2 cells: source `107-122` -> destination Tube 2 cells 1-16;
- Tube 3 cells: source `124-139` -> destination Tube 3 cells 1-16;
- Tube 4 cells: source `141-156` -> destination Tube 4 cells 1-16.

It does **not** use the old `345+` tube IDs from the failed Pilot 01.

## Translation choice

The source uplight IDs `198-201` in `SSAutoLoop1.ssfile` carry normalized intensity A-records but have `B=0` local color records. For physical testing on the user's IR-4 row, the probe uses a clearly labeled controlled fallback:

- source A-record intensity -> IR-4 master;
- red emitter = `1.0`;
- green/blue/white/amber/uv = `0`;
- strobe = `FORCE_ZERO`.

This is **not** claimed as exact SoundSwitch color decoding. It is a controlled probe to answer one question first: does the current-map source intensity shape and IR fixture ordering now resemble the original `Red - Smooth Pulse` behavior?

Tube cells use only current-map source IDs and decoded RGBWY segment/intensity data. Unsupported UV is dropped rather than remapped.

## V1 representation

Preview 101 format-1 Autoloops are capped at 32 whole-Static-Look steps. Pilot 02A therefore samples the first 19,200 ms / 32 source beats at one state per beat:

- 32 helper Static Looks;
- 1 Autoloop;
- 32 steps;
- 11,264 assignments;
- project CRC32 `58B46CD1`.

## Acceptance / next decision

Physical comparison should focus only on B1/S1:

1. If IR-4 pulse timing and row order now feel close, the next task is decoding/resolving the missing color context and then expanding to `Blue - Smooth`, `Sunny - Smooth`, and `80s - Smooth`.
2. If timing/order is still wrong, stop expanding and revisit SoundSwitch timing/loop-length interpretation before any more project files are generated.
3. Do not treat structural validation as migration fidelity evidence.
