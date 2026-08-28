# Project Status and Roadmap

## Mission and selected architecture

The selected architecture is QLC+ plus one focused native hardware/workflow plug-in.

At show time:

```text
VirtualDJ -- direct OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

QLC+ owns fixtures, Scenes, Chasers, beat timing, Autoplay order, dwell, the Virtual Console, persistence, and routing. The SoundSwitch plug-in exists only for proprietary SoundSwitch USB transport, Control One translation/feedback/reconnect, full-frame Priority Look selection, and the current temporary rig-specific intensity scaling. V26 also carries one focused QLC+ OS2L plug-in correction; the stock core executable remains unchanged.

## What exists in V26

- Native SoundSwitch Micro and Control One DMX output with no bridge or second lighting program.
- Control One MIDI translation, named `.qxi` profile, LED state feedback, and reconnect recovery.
- A complete workspace for four 10-channel IR-4 fixtures and four 40-channel BO-TUBE192 tubes.
- Four banks of 32 native QLC+ Autoloops: Medium, Colorful, Slow Dance, and Flashy.
- Manual repeat-one, Auto Bank, Auto All, sequential/random order, live 1/2/4/8/16-measure dwell, independent 0.25x–4x chase speed, and pad seek.
- A shared 4×8 Autoloop/Priority Look surface matching the physical Control One orientation.
- Full-frame Priority Looks that may be still Scenes or moving Chasers and release back to the advancing Autoloop.
- Sparse color overrides, Global/group intensity, VirtualDJ OS2L timing, clickable essential controls, and a mouse fallback.
- A full-width four-bank native running-state strip below every pad, following manual playback, Auto Bank, Auto All, and seek without joining playback ownership.
- One persistent mouse switch between Autoloops and Priority Looks.
- One unified Surface feedback route for mouse commands and Priority Look ownership, with positive-edge command handling.
- A 1600×900 dark Live Console with no visible/polling tracker service.
- Always-visible 1M/2M/4M/8M/16M dwell choices backed by QLC+'s native beat-counted SpeedDial.
- Stable direct OS2L BPM derived from the sender's reported BPM instead of packet bursts.
- A pinned two-plug-in install/rollback package, complete hashes, deterministic release validator, and reproducible workspace builders.

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

### V24 — Runtime Feedback alpha candidate

V24 is generated from V23 and keeps all 2,090 lighting Functions unchanged.

- Removes V23's duplicate Priority feedback declaration; QLC+ retains one Surface feedback destination on Universe 2.
- Routes Priority Look ownership channels `600–631` through that unified Surface line while preserving the private Priority output buffer.
- Handles empty mouse-command Scenes on their positive edge only, preventing double mode/bank/dwell/order/transport/speed dispatch.
- Keeps the Surface command line open without attached hardware, so the mouse workflow does not depend on Control One's MIDI-output handle.
- Replaces the hidden large monitor layer with 32 visible read-only frames containing four bank indicators per pad.
- Preserves every fixture, Autoloop, Priority Look, public Function ID, logical channel, and creative Function from V23.

V24 passes protocol, plug-in smoke, package, XML/reference, mapping, fixture, speed/dwell coverage, and isolated runtime checks. The isolated run confirmed Bank selection, both mode directions, speed selection, latched Start Bank/All, parent progression, and current-loop feedback. It now needs the short fixture observation.

### V25 — Lean Feedback reviewed handoff

V25 preserves the complete V24 Engine and creative show while clipping the previously enlarged Autoplay tracker frame to `1×1`. It keeps ten native Cue Lists only for absolute seek and retains the 128 read-only raw-Chaser monitors. V25 is the reviewed source for V26, not the public release target.

### V26 — Autoplay Clarity alpha candidate

V26 preserves the V25 Engine byte-for-byte and changes only Virtual Console presentation:

- expands each four-bank running-state rail to a full-width strip below its pad;
- keeps all five dwell values visible on each native multipage dwell state;
- labels musical dwell as 1M/2M/4M/8M/16M and 4/8/16/32/64 beats; and
- explicitly uses QLC+'s stock amber Monitoring state instead of a custom UI executable.

V26 also packages the unchanged V24 SoundSwitch plug-in and one focused build-matched `os2l.dll` fix. The latter reads reported BPM, emits a stable native QLC+ beat clock, and stops on disconnect/source silence. No tracker process, bridge, or QLC+ core fork is introduced.

## Current physical evidence

The user has confirmed the essential live behavior on preceding workspaces: all four pad banks, color overrides, pad-page switching, Priority Look takeover/release, underlying Autoloop continuation, OS2L, and Control One operation.

- Micro DMX produced live fixture output.
- Control One DMX 1 produced live fixture output.
- Control One DMX 2 produced live fixture output independently.
- Control One MIDI and core LEDs worked.
- The four-bank manual/Autoplay ownership logic worked after the later-bank input correction.

This evidence does not yet prove V26 gig qualification, simultaneous Control One ports, repeated hot-plug recovery, or the combined two-hour DJ/audio/OS2L/MIDI/LED/DMX workload.

## Immediate next session

Do not redesign the control architecture until this short pass is complete.

1. Close QLC+, run the V26 package test/installer, and open `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
2. Click the persistent Autoloop/Priority Looks switch twice and confirm both directions.
3. Run one manual pad in each bank. Confirm latch, same-pad off, replacement, and the full-width native amber strip.
4. Run Auto Bank and Auto All. Confirm the strip advances at every loop and through bank changes.
5. Seek with pads and change dwell while Autoplay remains running.
6. Apply/release one still and one moving Priority Look. Confirm sole authority and seamless return.
7. Confirm color override plus Global, IR-4, and tube group intensity.
8. Confirm the selected Micro or Control One output reaches fixtures.
9. Play a known-BPM VirtualDJ track and confirm QLC+ remains near that tempo instead of jumping to a packet-burst rate.
10. Unplug/replug Control One once while QLC+ remains open and observe MIDI/LED recovery.

If these pass, promote V26 to the local alpha baseline. Retain V21 and the immediately preceding V25 workspace locally, plus the published rollback releases and installer-created DLL backup.

## Engineering priorities after promotion

1. Complete repeated Control One hot-plug and LED restoration qualification.
2. Qualify Control One DMX 1 and DMX 2 simultaneously with MIDI/feedback and VirtualDJ active.
3. Complete a two-hour combined DJ workload soak.
4. Move hard-coded IR-4/tube intensity ranges out of reusable plug-in code and into configuration or native workspace logic.
5. Re-check whether a later official QLC+ release incorporates equivalent OS2L timing before carrying the focused patch forward.
6. Decide whether to prepare the narrow OS2L correction as an upstream contribution.
7. Create a fixture-neutral starter workspace/configuration guide for other DJs.

## Creative priorities

Current loops are sufficient for the alpha milestone; preserve V26 before new creative passes.

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
- No second lighting runtime, bridge daemon, replacement firmware, or custom lighting engine.
- Keep custom code only where QLC+ cannot cleanly express the hardware/workflow.
- Preserve public Function IDs and logical channels.
- Freeze a known-good release before creative or UI passes.
- Treat structural validation, physical output, and gig qualification as separate claims.
