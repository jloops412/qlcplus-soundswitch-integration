# Project Status and Roadmap

## Mission and selected architecture

The original decision was whether to keep building standalone EmberLights, run a bridge, or reuse QLC+. The selected architecture is QLC+ plus one focused native hardware/workflow plug-in.

At show time:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

QLC+ owns fixtures, Scenes, Chasers, beat timing, Autoplay, the Virtual Console, persistence, and routing. The plug-in exists only for proprietary SoundSwitch USB transport, Control One translation/feedback/reconnect, full-frame Priority Look selection, and the current temporary rig-specific intensity scaling.

## What exists in V23

- Native SoundSwitch Micro and Control One DMX output with no bridge or second lighting program.
- Control One MIDI translation, named `.qxi` profile, LED state feedback, and reconnect recovery.
- A complete workspace for four 10-channel IR-4 fixtures and four 40-channel BO-TUBE192 tubes.
- Four banks of 32 native QLC+ Autoloops: Medium, Colorful, Slow Dance, and Flashy.
- Manual repeat-one, Auto Bank, Auto All, sequential/random order, live 1/2/4/8/16-measure dwell, independent 0.25x–4x chase speed, and pad seek.
- A shared 4×8 Autoloop/Priority Look surface matching the physical Control One orientation.
- Full-frame Priority Looks that may be still Scenes or moving Chasers and release back to the advancing Autoloop.
- Sparse color overrides, Global/group intensity, VirtualDJ OS2L timing, clickable essential controls, and a mouse fallback.
- A four-bank live rail beside every pad, following manual playback, Auto Bank, Auto All, and seek without overlapping bank monitors.
- One persistent mouse switch between Autoloops and Priority Looks.
- A 1600×900 dark Live Console with an enlarged native current-loop tracker.
- A pinned install/rollback package, complete hashes, deterministic release validator, and reproducible V22 merge tool.

## Release lineage

### V20 — protected creative rollback

V20 remains the protected pre-reliability creative baseline. Do not rewrite or delete it.

### V21 — reliability rollback

V21 introduced the pinned QLC+ 5.3.0 build tuple, Control One stale-handle recovery, LED retry/restore, direct local VirtualDJ OS2L keepalive, complete essential mouse controls, and the self-testing install/rollback package. Its plug-in binary is reused unchanged by V22.

### V22 — unified creative rollback

V22 resolves the split between V21 and the later All Banks Variety Pro creative workspace:

- V21 is the host for all fixtures, I/O, Control One/UI behavior, ownership, dwell, speed, Priority Looks, and reliability work.
- Exactly 22 raw Colorful/Flashy Chasers are replaced by the Variety Pro versions.
- Their 176 Scene steps are imported; 17 colliding donor IDs are remapped to private IDs.
- Existing V21 manual owners, Autoplay parents, Priority Looks, fixtures, I/O, public IDs, logical channels, and plug-in binary remain unchanged.
- A disabled 128-Chaser monitor layer supplies native active-pad outlines without joining the playback owner SoloFrame.

The named Variety Pro workspace—not its autosave—is the donor. The autosave contains 32 unrelated save-state XML changes and is deliberately excluded.

V22 passes structural/package validation. The merged loops and advancing outline need the short owner test before V22 becomes the local production baseline.

### V23 — Live Console alpha candidate

V23 is generated from V22 and changes only the Virtual Console:

- all 2,090 lighting Functions, fixtures, I/O, public IDs, logical channels, Priority Looks, and creative content remain unchanged;
- the 128 raw-Chaser monitors become four non-overlapping bank indicators per physical pad, so an inactive later-bank widget cannot hide the current Chaser;
- the duplicate page-specific mode Button is removed, leaving one persistent Function `1993` / channel `811` control;
- the native ten-variant CueList tracker is enlarged; and
- the Live page is reorganized as a 1600×900 dark performance surface.

V22 is now the unified creative rollback. V23 passes structural/package validation and awaits the short owner UI observation.

## Current physical evidence

The user has confirmed the essential live behavior on preceding workspaces: all four pad banks, color overrides, pad-page switching, Priority Look takeover/release, underlying Autoloop continuation, OS2L, and Control One operation.

- Micro DMX produced live fixture output.
- Control One DMX 1 produced live fixture output.
- Control One DMX 2 produced live fixture output independently.
- Control One MIDI and core LEDs worked.
- The four-bank manual/Autoplay ownership logic worked after the later-bank input correction.

This evidence does not yet prove V22 gig qualification, simultaneous Control One ports, repeated hot-plug recovery, or the combined two-hour DJ/audio/OS2L/MIDI/LED/DMX workload.

## Immediate next session

Do not redesign the control architecture until this short pass is complete.

1. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`; no plug-in reinstall is needed when V21/V22 is already installed.
2. Click the persistent Autoloop/Priority Looks switch twice and confirm both directions.
3. Run one manual pad in each bank. Confirm latch, same-pad off, replacement, and the amber live rail.
4. Run Auto Bank and Auto All. Confirm the live rail and selected tracker row advance at every loop and through bank changes.
5. Seek with pads and change dwell while Autoplay remains running.
6. Apply/release one still and one moving Priority Look. Confirm sole authority and seamless return.
7. Confirm color override plus Global, IR-4, and tube group intensity.
8. Confirm the selected Micro or Control One output reaches fixtures.
9. Unplug/replug Control One once while QLC+ remains open and observe MIDI/LED recovery.

If these pass, promote V23 to the local alpha baseline. Archive older experimental files into a dated legacy folder while retaining V20, V21, V22, the V23 release archive, and the installer-created DLL backup.

## Engineering priorities after promotion

1. Complete repeated Control One hot-plug and LED restoration qualification.
2. Qualify Control One DMX 1 and DMX 2 simultaneously with MIDI/feedback and VirtualDJ active.
3. Complete a two-hour combined DJ workload soak.
4. Move hard-coded IR-4/tube intensity ranges out of reusable plug-in code and into configuration or native workspace logic.
5. Decide whether to maintain the minimal QLC+ plug-in privately or prepare an upstreamable contribution.
6. Create a fixture-neutral starter workspace/configuration guide for other DJs.

## Creative priorities

Current loops are sufficient for the alpha milestone; preserve V23 before new creative passes.

- Build purposeful event Priority Looks: announcements, introductions, first dance, parent dances, cake, open dancing, and finale.
- Meticulously grade every loop for musical phrasing, fixture separation, color balance, and usable intensity.
- Smooth any remaining abrupt Slow Dance transitions.
- Improve labels/colors only when the physical Control One mapping stays obvious.
- Add position/movement overrides for future movers without repurposing Pan/Tilt or destabilizing the current rig.
- Import historical fixture inventory only after the live system is stable.

## Optional backlog

- More complete Back/Link/Select/Hue/Smoke/Strobe/position roles where they provide real show value.
- More detailed LED feedback and, much later, OLED investigation.
- Multi-device identity and a portable per-rig configuration format.
- A SoundSwitch project importer for fixture inventory and reusable metadata.
- Additional VirtualDJ pad pages.

Custom Control One firmware is deferred. It is not needed for the current goal.

## Scope guardrails

- One runtime lighting application: QLC+.
- No standalone EmberLights revival, bridge daemon, replacement firmware, or custom lighting engine.
- Keep custom code only where QLC+ cannot cleanly express the hardware/workflow.
- Preserve public Function IDs and logical channels.
- Freeze a known-good release before creative or UI passes.
- Treat structural validation, physical output, and gig qualification as separate claims.
