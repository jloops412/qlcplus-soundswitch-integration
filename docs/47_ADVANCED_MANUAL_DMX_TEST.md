# Advanced Manual DMX Test

The installed **Advanced Manual DMX Test** is a fixture-agnostic diagnostic for
discovering an unknown or newly acquired fixture's literal DMX behavior. It is
not the normal fixture, Static Look, or Autoloop authoring vocabulary, and it
does not modify an EmberLights project or prove a fixture profile correct.

The first installed adapter is SoundSwitch Micro universe 1 or 2. The tool uses
the same production `SoundSwitchMicroSession` as Runner and the evidence-bound
Hardware Test; it does not contain a second USB driver.

## Safe setup

1. Close SoundSwitch and EmberLights so only one process can own the Micro.
2. Disconnect fog, haze, spark, laser, motion, strobe-sensitive, and unrelated
   fixtures. Connect only the fixture or isolated bench you intend to inspect.
3. Verify the fixture's physical mode, address, universe, terminator, and power.
4. Open **Start → EmberLights → Advanced Manual DMX Test**.
5. Choose Micro universe 1 or 2 and a per-preset hold of 1–30 seconds.
6. Review the displayed adapter, universe, hold, ten-minute session limit, and
   plan SHA-256. Type the exact `ARM RAW DMX ...` line. A mismatch opens no
   output device.

Arming opens the selected adapter and sends blackout before accepting a manual
value. Every replacement preset is preceded by blackout. A nonzero preset is
automatically replaced by blackout when its short hold expires; the whole
session closes after ten minutes. `QUIT`, Escape, Ctrl+C, device loss, write
failure, timeout, and normal destruction all enter the terminal blackout/close
path.

## Commands

| Command | Effect |
| --- | --- |
| `SET 1 64` | Merge channel 1 at value 64 into the currently held preset. Value 0 clears that channel. |
| `APPLY 1=255 2=128 7=20` | Replace the entire preset with the listed channel/value pairs. |
| `CLEAR 7` | Remove channel 7 and reapply the remaining preset, or blackout if none remain. |
| `BLACKOUT NOW` | Immediately send repeated all-zero frames while keeping the bounded session armed. |
| `SHOW` | Display the exact held values, frame SHA-256, hold/session time remaining, and frame counters. |
| `QUIT` | Send terminal blackout and close the adapter. |

Channels are one-based (`1–512`), values are bytes (`0–255`), and one preset is
limited to 64 explicitly listed channels. `APPLY` is deterministic and rejects
duplicate channels rather than guessing which value should win.

## Evidence boundary

The console shows host-side accepted/attempted frame counts and the exact
nonzero frame projection. Only a responding isolated fixture proves physical
behavior. Record the observed channel/range behavior against the manufacturer's
DMX chart, then create or edit a Local fixture profile through the normal
profile workflow. Use **EmberLights Hardware Test** for the separate sealed,
one-fixture qualification/audit workflow.

This preview remains unsigned and not physically qualified. Keep an independent
blackout/controller path available throughout testing.
