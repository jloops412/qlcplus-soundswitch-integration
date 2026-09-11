# Control One Workflow Specification

This specification recreates the live parts of the SoundSwitch Control One workflow that matter in QLC+. Some gestures are deliberate QLC+ adaptations rather than exact SoundSwitch internals.

## Pad surface

- The 32 physical pads are displayed as four columns by eight rows: 1–4, 5–8, through 29–32.
- `Auto Loop` toggles the shared pad surface between Autoloops and Priority Looks. Shift is no longer required for this mode change.
- A normal Bank press selects one of four Autoloop banks: Medium, Colorful, Slow Dance, and Flashy.
- One Autoloop press latches that loop. MIDI Note Off must not release it.
- Pressing the same Autoloop again stops it; pressing another pad replaces it.
- While Autoplay is running, pressing a pad seeks the active Bank/All parent to that loop without stopping Autoplay.

## Priority Looks

- Priority Looks are exclusive latches, not momentary pads.
- A Look can be either a still Scene or a moving Chaser, such as a first-dance look.
- A Look is sole full-frame DMX authority while active.
- The underlying Autoloop or Autoplay owner keeps advancing invisibly.
- Pressing the active Look again releases it and reveals the current underlying loop state without a restart.

This differs intentionally from color overrides. A color override replaces only color-emitter parameters, allowing intensity and movement from the current loop to continue.

## Autoplay

Autoplay is independent from manual repeat-one playback.

- Auto Bank sequential: loop 1 through 32 in the selected bank.
- Auto Bank randomized: a non-repeating shuffled cycle within the selected bank. Pressing a pad starts that exact loop, then the cycle continues from there.
- Auto All sequential: Bank 1 loops 1–32, then Bank 2, Bank 3, and Bank 4.
- Auto All randomized: a non-repeating shuffled cycle across all 128 loops. Pressing a pad starts that exact bank/loop, then the cycle continues from there.
- Dwell choices are 1, 2, 4, 8, and 16 measures.
- Dwell can change while Autoplay is running; it must not relaunch or reset the parent.
- Chase speed is separate: 0.25x, 0.5x, 1x, 2x, and 4x, still referenced to the beat source.
- Default is manual repeat-one, sequential order, eight-measure dwell, and 1x speed.

The current compact gesture uses shifted Bank presses to start a Bank scope and the shared scope control to move between Bank and All. The Virtual Console also exposes clickable Bank, order, dwell, and start controls so the show remains usable without hardware.

The Live page provides the essential mouse fallback: Play/Pause and Order are clickable, the pad header has an explicit Autoloop/Priority Looks switch, and the chase-speed readout advances through 0.25x/0.5x/1x/2x/4x when clicked. The intensity panel header arrow cycles Global, Groups 1–4, and Scripted while its visible slider controls the selected target.

V31 retains the read-only four-bank live strip below every pad. Each segment observes one raw Chaser instead of starting an owner, so it follows a manual latch, Auto Bank, Auto All, and Autoplay seek without changing playback. Banks 1–4 run left-to-right; QLC+'s native amber Monitoring state marks the active Chaser. There is no visible or polling tracker process.

The mouse `AUTOLOOPS ⇄ PRIORITY LOOKS` control is one persistent Button outside the mode-paged frame. It retains public Function `1993` and logical channel `811`; do not recreate separate page-specific copies.

V31 carries Bank, mode, dwell, transport, order, speed, and Priority ownership over one unified QLC+ Surface feedback patch. Empty command Scenes are positive-edge actions; their trailing zero must never dispatch a second command.

## Overrides and performance buttons

- Red, Orange, Yellow, Green, Cyan, Blue, Purple, Pink, and Full Color are exclusive color-only latches on the normal color pads.
- Shift + any of those nine color pads is a momentary full-rig hold. Releasing it reveals the latched color or current show underneath.
- Every color latch and hold addresses the eleven physical fixtures and the eleven private Priority mirrors.
- Shift + performance pads 1–9 selects the nine decoded Focus A/B position Scenes.
- White, Black, and UV retain hold and toggle forms.
- Movement, Strobe, Hue, Smoke, Pan, and Tilt are mapped or reserved but are not all fully programmed for the current full rig.

## Intensity and groups

- The touch strip stores independent levels for Global, Group 1, Group 2, Group 3, Group 4, and Scripted.
- Autoloop Intensity selects Global; Shift + Autoloop Intensity selects Scripted.
- Group 1 is the four IR-4 fixtures.
- Group 2 is the Wash FX Hex direct zone emitters.
- Group 3 is the four BO-TUBE192 fixtures.
- Group 4 is the two Focus Spot Two main and UV dimmer channels. Released programming keeps Focus UV off.
- Double-press Group 1 selects Group 2; double-press Group 3 selects Group 4.
- Scripted remembers state but is reserved and must not alter fixture output.
- Effective output is Global multiplied by the selected fixture-group level.

## V30 speed and seek guarantees

- The Pan/Speed encoder changes every raw Autoloop through 0.25x, 0.5x, 1x, 2x, and 4x presets.
- All raw Autoloops use Function-level Common timing, matching the timing layer changed by QLC+ SpeedDial.
- During Auto Bank or Auto All, a performance pad seeks the active parent rather than replacing it with a manual owner.
- Repeated presses of the same pad remain reliable after the parent has advanced.
- Changing Sequential/Randomized order or resuming autoplay restores the last selected starting loop.

## Transport and feedback

- Play/Pause operates on the selected manual or automatic playback owner even when no song is actively playing.
- Stop All remains the emergency global stop.
- Hardware LEDs should reflect mode, bank, active loop/scope, Priority Look, color override, transport state, and selected intensity layer.
- Control One MIDI is expected to reconnect without restarting QLC+; QLC+ Function state remains playback authority.
- On reconnect, transient held-button state is cleared while bank, mode, playback owner, dwell, speed, order, latches, and intensity target are retained and their known LEDs are restored.
- OLED and custom firmware are deferred.

## V31 input parity and release contract

Use the Live console page for performance controls. V31 mouse Autoloop pads
use the same selection command as hardware: while Autoplay runs they seek its
existing parent; otherwise they latch or replace the manual owner. The 128
original owner Collections and public IDs remain intact.

Normal mouse color buttons share the hardware exclusive latch behavior.
Shift-held colors remain a separate momentary layer. Releasing Shift before
the held key still releases the original held channel when that key is released.
MIDI disconnection releases transient holds while retaining established latches.

Focus position shortcuts use native ForceLTP Scene Flash controlled by exclusive
latches. They are reachable on Live and on the Position Bench and cover both
physical and private Focus fixtures. White/Black/UV performance Scenes also
cover both layers. Their original physical DMX values are unchanged.

Transport controls act on the running owner even if the operator browses a
different bank. Mouse-started Auto All can pause/resume. Known Priority and
color LED state comes from QLC feedback and is restored after reconnect.

MOVE and STROBE remain inherited ordinary Chasers. They do not yet have a
proven force-override contract over arbitrary running content. Their dedicated
redesign and physical observation are explicit requirements of pass two.

V31 raw-Chaser monitors also provide one-way current-loop LED feedback. In
Autoloop mode the hardware pads show the actual running pad number; bank
buttons retain the browsed bank. Priority mode shows the active Look. Releasing
a known momentary hold after navigating away returns to Live before the
release is sent, because QLC dispatches inputs only to its selected page.
