# V30 Performance Recovery

## Purpose

V30 is a corrective release built from the reviewed V27 Full Rig workspace. It
does not attempt the broader Autoloop-quality redesign. It restores the live
performance contracts that must be dependable before creative review resumes:

1. chase speed changes every raw Autoloop immediately and consistently;
2. pressing a pad during Auto Bank or Auto All starts that exact loop;
3. sequential playback continues with the next numbered loop;
4. randomized playback starts on the requested loop and continues through a
   non-repeating shuffled cycle;
5. Priority Looks retain output ownership and Control One feedback;
6. normal color pads latch, while Shift + color is momentary;
7. every color override affects the complete physical and private fixture rig.

V30 is generated only from
`IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw`, SHA-256
`a4f7559930e93485f3ea2815a0b44ca8e40acdfcc43fb3b0430e863c64b2dc4b`.
The V27 source is not edited in place.

## Corrected runtime contracts

### Autoloop speed

The 128 raw Autoloops previously used `PerStep` timing. QLC+ SpeedDial changes
Function-level FadeIn and Duration values, while a PerStep Chaser executes the
timing stored on each Step. The control could therefore move and light up
without changing the timing that was actually running.

V30 makes the raw Autoloops use `Common` FadeIn, FadeOut, and Duration timing.
It then regenerates the hidden speed-control matrix from the final V27 values:

- 52 Duration groups;
- 42 FadeIn groups;
- one Duration controller and one FadeIn controller for every raw Autoloop;
- no speed controller writes FadeOut;
- Control One speed presets remain `0.25x`, `0.5x`, `1x`, `2x`, and `4x` on
  logical channels 470–474;
- the default remains `1x`.

One hundred nineteen Autoloops already had uniform per-step timing, so their
running timing is preserved exactly. Nine had variable per-step timing and are
normalized to their reviewed top-level timing in V30:

`573, 576, 632, 633, 635, 636, 645, 657, 658`

That is an explicit corrective tradeoff. Those nine loops remain listed for the
later creative Autoloop pass, where variation can be reintroduced through a
speed-compatible design instead of silently disabling the live speed control.

### Selected-loop start during autoplay

While Auto Bank or Auto All is running, an unshifted performance pad now seeks
the active parent Chaser rather than stopping autoplay and starting an isolated
manual loop.

For sequential order:

```text
6 running, 7 pending -> press 3 -> 3 runs, then 4, 5, 6, 7...
```

For randomized order, V30 replaces QLC+'s opaque runtime `Random` lookup with a
stable non-repeating randomized cycle. The plug-in translates the requested
logical loop to its exact position in that cycle. The selected loop therefore
runs first, and the shuffled sequence continues from that point.

QLC+ Cue List Steps mode suppresses an unchanged input value. V30 sends a
same-step nudge immediately before the target seek value, so pressing the same
pad again after the sequence has advanced reliably restarts that loop without
briefly selecting a neighboring loop.

The selected-loop position is also restored after changing Sequential/Random
order and after resuming autoplay with Play/Pause.

### Priority Looks

V30 repairs Priority ownership in the native SoundSwitch plug-in:

- Priority control feedback on logical channels 600–631 now updates both the
  full-frame ownership state and the Control One pad LED;
- selecting a new Priority Look clears any stale private frame before the new
  frame arrives;
- the base Autoloop remains visible during that handoff rather than flashing the
  previous Priority Look;
- releasing the last active Priority Look clears the private frame and reveals
  the continuously advancing base show immediately.

Universe 3 remains a private Priority buffer on
`soundswitch:priority-layer`. It must not be patched to physical DMX, Art-Net,
sACN, or another hardware output.

### Color latch and Shift-hold

The nine normal color pads keep exclusive press-on/press-off latch behavior on
logical channels 36–44.

Shift + those same nine color pads now drives independent momentary LTP Flash
Scenes on logical channels 164–172. Releasing Shift + color removes only the
held layer, revealing any latched color that was already underneath it.

Every latch Scene and every hold Scene contains FixtureVals for:

- physical fixture IDs 0–10 on Universe 1; and
- private Priority mirror IDs 100–110 on Universe 3.

This includes all four IR-4 fixtures, the Wash FX Hex, all four BO-TUBE192
fixtures, and both Focus Spot Two fixtures in both output layers.

The nine Focus A/B position shortcuts formerly occupied Shift + color. They now
use Shift + performance pads 1–9 on logical channels 128–136. Their Function
IDs and decoded pan/tilt values remain unchanged.

## Stable logical mappings

| Control | Logical channels | V30 behavior |
|---|---:|---|
| Performance pads in Autoloop mode | 0–31 | Manual loop, or exact seek while autoplay runs |
| Performance pads in Priority mode | 600–631 | Full-frame Priority Look toggle |
| Shift + performance pads 1–9 | 128–136 | Focus A/B position Scenes 1–9 |
| Color pads | 36–44 | Exclusive color latch |
| Shift + color pads | 164–172 | Momentary full-rig color hold |
| Speed presets | 470–474 | 0.25x through 4x raw-Autoloop timing |
| Sequential/Random state | 485–486 | Select parent order and preserve selected start |
| Autoplay seek | 632 | Internal exact-step Cue List control |

## Regression protection

`Build-V30PerformanceRecovery.py` is deterministic and refuses any source whose
SHA-256 differs from the reviewed V27 workspace.

`Test-V30Workspace.py` verifies:

- all 22 physical/private fixture instances and addresses are unchanged;
- all 128 raw Autoloops are Common-timed;
- every raw Autoloop has exactly one Duration and one FadeIn speed controller;
- all 320 bank/all, sequential/randomized pad selections start the exact
  requested loop;
- repeated-seek nudges remain inside the same Cue List step;
- the five randomized parents are complete non-repeating permutations;
- all latch and hold colors cover fixture IDs 0–10 and 100–110;
- Shift + color and Shift + position channels are exclusive and non-conflicting;
- the private Priority output and unified Control One feedback patch remain
  intact; and
- unrelated Functions, widgets, fixtures, addresses, and routing remain
  unchanged from V27.

The plug-in CI adds focused tests for randomized seek inversion, repeated seek,
Priority ownership, stale-frame rejection, multiple-owner release, base-frame
restore, and plug-in loading without hardware.

## Qualification boundary

The deterministic workspace tests and Windows plug-in CI are software evidence,
not physical-rig qualification. Before V30 is treated as show-ready, load the
exact V30 workspace and matched `soundswitch.dll` in the pinned QLC+ Windows
host and observe:

1. all five speed presets while representative loops from every bank run;
2. exact selected-loop start in Sequential and Randomized modes for Auto Bank
   and Auto All;
3. repeated selection of the same pad after playback advances;
4. all 32 Priority Looks, look-to-look changes, LED state, and clean release;
5. every latched color and every Shift-held color with and without Priority;
6. every fixture class, including Wash and both Focus fixtures; and
7. Control One reconnect, LED restore, intended DMX outputs, and combined soak.

V27 remains the immediate full-rig rollback. V26 remains the protected
Autoplay Clarity generation source behind V27.
