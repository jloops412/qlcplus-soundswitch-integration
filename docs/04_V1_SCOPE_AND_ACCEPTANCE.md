# V1 Scope and Acceptance

## First gig-qualified outcome

The first gig-qualified build is successful when Joshua can perform a complete live event offline with VirtualDJ, the DDJ-REV7, MIDI lighting control, and a representative rig, using no SoundSwitch function during normal operation and keeping SoundSwitch only as an emergency fallback during initial pilots. This is the first qualification environment, not a Love & Light-specific product definition. The same package contracts must accept unrelated fixtures, groups, controllers, and event types.

V1 exposes at most two DMX universes.

The first usable workflow is intentionally weighted toward dependable Static Looks and rich Autoloop banks. Full custom scripting, AutoScripting, remaining integrations, and other ledger items arrive in later parity milestones, but are mandatory before a build is called SoundSwitch-parity-complete or general public 1.0.

EmberLights is delivered as installable Windows testing builds throughout V1 development. “Full V1” means one coherent installed application—project creation, fixture profiles and patch, Static Looks, Autoloops, MIDI Learn, VirtualDJ/OS2L timing, live controls, two-universe network output, validation, recovery, and diagnostics—not a collection of laboratory executables. A build may still be marked testing-only until the hardware, soak, and event gates below pass.

V1 show packages may contain up to 64 Autoloop banks of 32 slots each. Four-bank layouts are pageable control-surface views, including the planned Control One profile, and are not an authoring or runtime capacity limit.

## Milestone 0 — Native vertical slice

### Included

- Flat OS2L `beat`, `btn`, and `cmd` parsing.
- Normalized clock and predictive-hold state machine.
- Device-agnostic MIDI mapping contract and soft takeover.
- Sparse per-property layer resolver.
- Semantic RGB/intensity/pan/tilt/strobe/fog fixture rendering.
- Patch validation and two fixed universe frames.
- ArtDMX packet encoding and unicast output contract.
- One beat-driven semantic Autoloop.
- QLC+ bridge contract using Art-Net.
- Unit tests and native benchmark harness.

### Gate

- Every layer and packet test passes.
- Render tick performs no heap allocation after initialization.
- One million simulated ticks complete without state divergence or crash.
- The spike provides measured time/tick and resident-memory evidence.

## Milestone 1 — Hardware/DJ laboratory build

### Included

- Real OS2L TCP listener and VirtualDJ discovery/direct-IP configuration.
- Same-PC and separate-PC test runs.
- WinMM or equivalent Windows MIDI input/output adapter.
- Captured Control One map and first profile.
- Art-Net output verified against a receiver/node.
- QLC+ loopback bridge verified and benchmarked.
- USB inventory for Control One, MyDMX Buddy, and SoundSwitch USB interface.
- Minimal status console: clock source, BPM, beat, active show/layer, outputs, MIDI devices, warnings, blackout.

### Gate

- VirtualDJ beat/BPM remains synchronized for at least two hours.
- Network interruption enters predictive hold and recovers without a visible phase snap.
- MIDI-to-engine p99 stays under 30 ms.
- Output disconnect/reconnect does not require project reload.
- Same show package produces equivalent frames in same-PC and separate-PC modes.

## Milestone 2 — Shadow-gig build

### Included

- Fixture/venue source format and compiler.
- Groups, roles, palettes, positions, Static Looks, and Autoloop banks.
- Per-trigger transition policy: latched/crossfaded Static Looks, musically complete one-shot Autoloops, and momentary FX anti-snap release.
- MIDI Learn and persisted mappings.
- Emergency/photographer/work-light safety states.
- Atomic package load and last-known-good recovery.
- Basic live-audio BPM fallback plus manual tap tempo.
- Diagnostics/event log.
- Initial SoundSwitch project inspector and migration report.

### Gate

- Eight-hour soak test passes with two universes at production frame rate.
- Controlled OS2L, MIDI, audio, output, and UI fault injection passes.
- Corrupt fixture, track, or import artifact is quarantined without preventing Runner startup.
- Runner remains inside release ceilings on the low-end reference PC.
- At least three full recorded-event rehearsals complete while SoundSwitch runs in parallel for comparison.

## Milestone 3 — First live pilot

### Preconditions

- A complete venue/rig package has been preflighted.
- Emergency backup path has been rehearsed.
- Output hardware, network isolation, and power plan have been tested at the venue or an equivalent rig.
- No proprietary Control One feature is required for core operation.
- Joshua explicitly chooses a low-risk event for the pilot.

### Success criteria

- Four-to-six hours continuous operation.
- No missed blackout/emergency/manual commands.
- No unsafe fog/strobe behavior.
- No output stall visible to guests.
- Recovery from at least one planned controller or network reconnection.
- Post-event logs are complete and reviewable.

## Explicit non-gates

The following are not required before the first safe internal pilot:

- polished waveform editor;
- full song-script parity;
- proprietary Control One DMX or OLED;
- AI AutoScripting;
- public storefront, licensing, or code-signed broad distribution (an internal Windows installer is required);
- post-V1 universe expansion;
- Wolfmix-style standalone hardware.

These are non-gates only for the first safe internal pilot. Any corresponding SoundSwitch capability remains governed by `13_SOUNDSWITCH_PARITY_LEDGER.md`.
