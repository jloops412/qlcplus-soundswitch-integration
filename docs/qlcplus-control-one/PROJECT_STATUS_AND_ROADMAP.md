# Project Status and Roadmap

## Where the project started

The original question was whether to keep building EmberLights, run a separate bridge, or reuse QLC+. The selected architecture is QLC+ plus one focused native plug-in. That keeps one lighting application on the DJ laptop while preserving already-owned SoundSwitch Micro and Control One hardware.

## What now exists

- A working SoundSwitch Hardware QLC+ plug-in for Micro and Control One DMX.
- A Control One MIDI translator/input, QLC+ profile, reconnect loop, and LED feedback.
- A complete V20 workspace for four IR-4 fixtures and four 40-channel BO-TUBE192 tubes.
- Four banks of 32 Autoloops, manual repeat-one ownership, Bank/All Autoplay, sequential/random order, live dwell, independent chase speed, and pad seek.
- A two-mode 4×8 pad surface that mirrors the physical Control One layout.
- Full-frame Priority Looks that can be Scenes or Chasers and release back to the advancing Autoloop.
- Color-only overrides, global/group intensity, OS2L timing, clickable bank controls, and active-loop/status UI.
- Reusable plug-in source, tests, input profile, portable workspace, release hashes, and maintenance documentation in this repository.

## Current confidence

The baseline behavior through V19 was physically exercised and the user confirmed the key workflow: all four pad banks, override colors, page switching, Priority Look takeover/release, and underlying Autoloop continuation. Micro DMX plus Control One DMX 1 and DMX 2 have each produced live fixture output. OS2L and the Control One surface have worked.

V20 is the same functional base with UI-state additions; it passed structural validation but awaits the owner's next hands-on regression. Both Control One ports operating simultaneously, long hot-plug/soak behavior, and a pinned QLC+ build package remain unqualified.

## Owner-confirmed priority order — 2026-08-23

The current Autoloops feel good enough for this milestone. Do not spend primary engineering time polishing loops or Scenes yet.

1. Make Control One MIDI/LED and VirtualDJ OS2L reconnect automatically and reliably.
2. Complete the important Control One controls and SoundSwitch-familiar workflow parity.
3. Make every essential show operation available and understandable from the Virtual Console as a mouse fallback.
4. Pin the plug-in to an exact QLC+ build and create a repeatable install/rollback package.
5. Build purposeful event-ready Priority Looks.
6. Later, meticulously grade and improve every Autoloop and Scene.

## Next session: do these first

Keep this short and physical. Do not redesign anything until these pass.

1. Load V20 and reselect the desired Micro or Control One output if using the portable file.
2. Confirm the clickable Bank bar, 4×8 pad layout, active-loop highlight, now-playing status, and 1/2/4/8/16 dwell controls.
3. Run one manual pad in each bank; same-pad off and cross-pad replacement.
4. Run Auto Bank and Auto All, sequential and random; seek with a pad and change dwell while running.
5. Apply/release a still and moving Priority Look over Autoplay.
6. Confirm color overrides plus Global, Group 1, and Group 3 intensity.
7. Unplug/replug Control One once while QLC+ stays open and confirm MIDI/LED recovery.

If all seven pass, mark V20 as the known-good creative baseline before changing loops or Looks.

## Required engineering before community release

1. Rebuild the plug-in against one exact supported QLC+ release or pinned commit; publish the full compatibility tuple.
2. Qualify both Control One DMX ports simultaneously with MIDI/feedback active. Each port already works independently.
3. Complete the two-hour combined DJ workload soak and repeated hot-plug tests.
4. Move hard-coded IR-4/tube intensity addresses out of the reusable plug-in into workspace/configuration.
5. Add deterministic automated workspace checks and plug-in CI/smoke builds.
6. Create a clean install/rollback package and a short first-plug guide for another DJ.
7. Decide whether to maintain a minimal QLC+ fork or prepare an upstreamable plug-in contribution.

## Creative work after V20 is frozen

- Grade every Autoloop bank for smoothness, musical phrasing, intensity, fixture separation, and beat coherence.
- Improve Slow Dance fades and eliminate abrupt jumps.
- Replace color-only Priority Looks with purposeful event looks: announcements, introductions, first dance, parent dances, cake, open dancing, and finale.
- Use moving Chasers where a Look should have gentle motion while still remaining full-frame authority.
- Organize colors and button labels so the Virtual Console communicates the physical pad result at a glance.
- Add useful position/movement overrides for future movers without compromising current fixtures.
- Import additional fixture definitions from the historical SoundSwitch project only after the live rig is stable.

## Optional backlog

- VirtualDJ pad pages and stronger automatic OS2L wake/reconnect after QLC+ restarts.
- More complete Back/Link/Select/Hue/Smoke/Strobe/position workflows.
- Pan and Tilt override behavior while preserving Speed as the Autoloop multiplier.
- More detailed LED feedback and, much later, OLED investigation.
- Multi-device identity and a portable per-rig configuration format.
- A future SoundSwitch project importer for fixture inventory and reusable look metadata.
- Custom Control One firmware is explicitly deferred; it is unnecessary for the current goal.

## Scope guardrails

- One runtime lighting application: QLC+.
- No standalone EmberLights revival, bridge daemon, or custom lighting engine.
- Keep custom code only where QLC+ cannot cleanly express the hardware/workflow.
- Freeze a known-good workspace before creative passes.
- Treat gig qualification and community distribution as separate milestones.
