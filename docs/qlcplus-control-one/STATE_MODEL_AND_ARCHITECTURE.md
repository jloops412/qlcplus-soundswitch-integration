# State Model and Architecture

## Why the original prototype drifted

The first prototype stored a shadow copy of playback state inside the MIDI translator while QLC+ independently owned the real Functions. Hardware presses updated both, but on-screen buttons, OS2L commands, Stop All, same-pad toggles, and reconnects could update QLC+ without updating the shadow copy. That caused incorrect labels, wrong Bank/All decisions, and transport commands aimed at functions that were no longer running.

The other major coupling was Pan/Speed changing Autoplay dwell. In Control One terminology, Speed belongs to effect/chase timing; Autoplay dwell belongs to selection policy. Combining them made it impossible to change chase speed without changing when the next loop would be selected.

## Authoritative model

Treat these dimensions independently:

| Dimension | Values |
|---|---|
| Transport | stopped/ready, running, paused |
| Playback owner | manual repeat-one, Auto Bank, Auto All |
| Selected bank | 1, 2, 3, 4 |
| Order | sequential, random |
| Dwell | 1, 2, 4, 8, 16 measures |
| Chase speed | 0.25x, 0.5x, 1x, 2x, 4x |
| Pad role | Autoloops, Static Looks |
| Full-look overlay | none or one Static Look |
| Parameter overlay | none or one color override |

QLC+ Function running state is authoritative. The MIDI translator may retain selection preferences needed after reconnect, but it must reconcile them from QLC+ input feedback rather than assuming a synthetic press succeeded.

## Function ownership

The 128 creative Autoloops remain ordinary QLC+ Chasers. They are not duplicated for each dwell or order option.

Autoplay requires ten stable parent owners:

- Bank 1–4 Sequential;
- Bank 1–4 Random;
- All Banks Sequential;
- All Banks Random.

One live dwell control updates the common parent-step duration on all ten owners. This changes the active owner's dwell in place and avoids fifty duplicated parents.

Raw Autoloop speed is controlled separately. Each raw Chaser uses common timing within that Chaser. Hidden native QLC+ Speed Dials group functions by their original duration and fade values, so a global multiplier can scale every Autoloop without destroying the relative programming or introducing a custom playback engine.

## Priority and handoff

Two native QLC+ ownership lanes prevent a child Chaser from cancelling its own parent:

```text
manual control Collection -> manual handoff Scene -> raw Autoloop
automatic control Collection -> auto handoff Scene -> parent Autoplay Chaser -> raw Autoloop
```

Solo Frames monitor the handoff Scenes, not the raw child Chasers. Static Looks use Flash Override plus Force LTP and define the complete patched look. Color Overrides use the same priority mechanism but contain only color-emitter channels.

## Reconnect rules

- USB DMX lines retain stable UIDs and enumeration order for the life of the plug-in instance.
- The Control One MIDI input remains logically patched while the device is absent and retries periodically.
- Reconnect preserves selected bank, pad role, order, dwell, speed, and latch preferences.
- Output and input handle recovery must not require restarting QLC+.
- QLC+ feedback should drive Control One LEDs through the device's MIDI output endpoint; feedback is presentation, never playback authority.

## Scope discipline

The hardware output plug-in must not contain fixture addresses or show-specific channel scaling in a reusable release. Rig-specific intensity groups belong in the QLC+ workspace or in explicit user configuration. Hardware transport, MIDI translation, reconnect handling, and feedback are reusable; fixture programming is not.


