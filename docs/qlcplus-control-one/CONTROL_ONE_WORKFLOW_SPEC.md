# Control One Workflow Specification

This specification recreates the parts of the SoundSwitch Control One workflow that matter during a live QLC+ show. It distinguishes verified SoundSwitch behavior from deliberate QLC+ adaptations.

## Performance pads

- The 32 physical pads are presented as four columns by eight rows, numbered left-to-right: 1–4, 5–8, through 29–32.
- `Auto Loop` alternates the shared pad surface between Autoloops and Static Looks.
- A normal Bank press selects one of four Autoloop banks.
- An Autoloop pad selects one loop and repeats it until another loop is selected or it is toggled off.
- A Static Look is a latched, exclusive full-look overlay. Pressing another Static Look replaces it; pressing the active look again releases it.
- While a Static Look is active, the underlying Autoloop continues advancing. Releasing the look reveals the current Autoloop state without restarting it.

## Autoplay

Autoplay is independent of manual repeat-one behavior.

- `Auto Bank` advances through all 32 loops in the selected bank.
- `Auto All` advances through Bank 1 from 1–32, then Bank 2, Bank 3, and Bank 4.
- Sequential and Random are separate order policies.
- Dwell selects how many musical measures each chosen Autoloop owns before advancing. Supported values are 1, 2, 4, 8, and 16 measures.
- Dwell can be changed while Autoplay is running. The current loop and bank position remain active; the new duration applies without relaunching the parent sequence.
- Chase speed is a different control. A speed multiplier changes the internal step rate of the current and future Autoloops while remaining locked to the beat source.
- Default state is manual repeat-one, sequential order, eight-measure dwell, and 1x chase speed.

The default QLC+ adaptation uses `Shift + Bank` as a compact scope gesture: start that bank first; repeating the same shifted bank selects all banks. This differs from SoundSwitch's exclusive-bank long-press model and should remain a documented, replaceable policy.

## Overrides

- Color Overrides are exclusive latches.
- A color override writes color-emitter parameters only. Intensity, movement, and other parameters from the running Autoloop continue.
- White, Black, and UV performance buttons are momentary full-output overrides.
- Static Looks are full-look overlays and therefore take precedence over the Autoloop, unlike color-only overrides.

## Transport

Transport has three explicit states: running, paused, and stopped/ready.

- Play/Pause operates on the selected manual loop or active Autoplay owner.
- A transport stop must not merely freeze the last DMX packet while QLC+ timing advances in the background.
- If exact engine pause is unavailable without modifying QLC+ core, the supported fallback is deterministic stop/restart of the selected owner. The UI must label this honestly; it must never resume at a hidden, advanced step.
- Stop All remains an emergency global stop and is not a substitute for ordinary transport.

## Control surface feedback

At minimum, hardware LEDs should show:

- current pad mode;
- selected bank;
- active manual pad or Autoplay scope;
- active Static Look;
- active color override;
- running/paused transport state.

The OLED is not required for this milestone.


