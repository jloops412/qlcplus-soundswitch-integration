# Decisions and Open Questions

Last updated: 2026-08-10.

## Accepted decisions

| ID | Decision | Rationale |
| --- | --- | --- |
| D-001 | SoundSwitch is the primary workflow/product reference. | The goal is a replacement; Wolfmix familiarity is not assumed. |
| D-002 | Wolfmix is secondary inspiration only. | Useful tactile ideas may strengthen live control but cannot redirect core scope. |
| D-003 | V1 exposes exactly two DMX universes. | Matches Joshua's need and narrows validation; post-V1 expansion remains possible. |
| D-004 | Windows and VirtualDJ are first. | Matches the actual DJ workflow. |
| D-005 | OS2L is primary timing/control transport. | Avoids unnecessary audio analysis when DJ state is available. |
| D-006 | Timing falls back through prediction, audio, tap, then safe static behavior. | Prevents one packet loss from creating a visual jump or blackout. |
| D-007 | Studio and Runner are separate operational modes. | Sophisticated authoring should not burden live performance. |
| D-008 | Runner is deterministic, offline, and has no AI model in its output path. | Gig safety and reproducibility. |
| D-009 | MIDI is first-class and device-agnostic. | Any MIDI controller should control actions/parameters; Control One gets a bundled profile. |
| D-010 | Same-PC and separate-PC arrangements are equal requirements. | Supports lean laptops and flexible rigs without two code paths. |
| D-011 | SoundSwitch migration is core and loss-preserving. | Existing work materially lowers the switching cost. |
| D-012 | Control One MIDI and proprietary DMX/OLED/storage are separate tracks. | Official support confirms generic MIDI but not third-party DMX parity. |
| D-013 | Build an original semantic, per-property layered domain core. | QLC+/flat DMX models do not fit portable song/event intent. |
| D-014 | Use QLC+ as optional compatibility bridge/reference and selectively reuse only audited components. | Gains hardware reach without forcing Qt/general-console overhead into Runner. |
| D-015 | Open Fixture Library is the preferred upstream fixture source. | Open documented JSON and MIT-licensed project. |
| D-016 | Normal performance target includes low-end PCs. | Most DJs have stronger machines, but the product should not require one. |
| D-017 | Primary controller workflow is Pioneer DDJ-REV7 plus Control One. | Confirmed by Joshua. |
| D-018 | Static Looks and Autoloops are the first authoring/playback priorities; custom track scripting is not a V1 migration gate. | Joshua uses Static Looks for dinner and special moments and Autoloops throughout dancing; the value arrives before full song-script parity. |
| D-019 | The core workflow is event-agnostic. | Ceremony, reception, party, or another event can use the same looks, loops, moments, and live controls; event templates are optional organization. |
| D-020 | Resume behavior is defined per trigger class, not by one global switch. | Static Looks latch with a configurable crossfade, one-shot Autoloops finish their musical length, and momentary FX release on button-up with a short anti-snap fade. |
| D-021 | The product is general-purpose; Joshua's rig is a qualification matrix, not product scope. | Any user must be able to supply unrelated fixtures, controllers, venues, and event workflows. |
| D-022 | Full relevant SoundSwitch functional parity is the minimum finished-product bar. | Milestones may stage delivery, but omitted scripting, automation, integration, fixture, or performance features remain required work. |
| D-023 | A feature-by-feature parity ledger is a binding release artifact. | “All SoundSwitch features” must be auditable with evidence and acceptance tests rather than treated as a vague aspiration. |
| D-024 | Windows is the required launch platform; macOS follows later and is not a launch gate. | Joshua prioritized Windows while welcoming portability as a bonus. |
| D-025 | Serato is the second direct DJ-integration priority after VirtualDJ. | Joshua explicitly selected Serato for the next ecosystem while VirtualDJ remains first. |
| D-026 | OFL ingestion uses a versioned Studio adapter, stable native profile contract, provenance, and quarantine; Runner never parses OFL source JSON. | OFL documents that its native format can change incompatibly and recommends plugin-based transformation. This also preserves deterministic gig startup. |
| D-027 | Four Autoloop banks are a pageable control-surface window, not a product limit; V1 packages support 64 banks of 32 slots (2,048 loops). | Control One exposes four banks at once, but the general-purpose app must not inherit that hardware constraint. Fixed compiled capacity preserves deterministic, allocation-free Runner behavior. |
| D-028 | WinMM is the first Windows MIDI adapter, isolated behind logical device/port contracts with one bounded queue per input; MIDI feedback never runs on the DMX scheduler. | Windows is the launch platform and WinMM provides the required desktop MIDI device lifecycle. Isolation preserves later backend replacement, while per-port queues and off-thread feedback protect deterministic output. |
| D-029 | The product name is EmberLights. | Joshua selected the name for the application and repository; project files use the `.emberlights` extension. |
| D-030 | The first distributed Windows build is a coherent full-V1 testing build with an installer, not a hard-coded lab-only alpha. | Joshua asked to keep building the whole application and improve it through installed versions and iterations. Hardware qualification and parity evidence still determine gig-ready/public-release claims. |
| D-031 | The first native USB-DMX output is the published ENTTEC DMX USB Pro serial protocol, with one independently reconnecting COM device mapped per universe. | It gives Windows users a lawful, dependency-light native adapter while preserving the two-universe architecture. Slow writes stay off the scheduler and stale queued frames are superseded instead of replayed late. DMX USB Pro Mk2 dual-port behavior and third-party compatible devices remain separate hardware qualifications. |
| D-032 | QLC+ Fixture Definition (`.qxf`) is a versioned Studio-only compatibility input, not a Runner or canonical project format. | QXF provides an efficient on-ramp from the QLC+ ecosystem and OFL's QLC+ export without importing Qt/general-console runtime code. The bounded adapter records provenance, reports approximations, quarantines switching aliases, and compiles only validated native profiles; Runner remains deterministic and QLC+-free. |
| D-033 | Release claims are evidence-tiered: testing preview, gig-qualified V1, public beta, then parity-complete general public 1.0. | Joshua asked for a true production release while continuing quickly. Explicit gates prevent an installer or version label from being mistaken for hardware qualification or the accepted full-SoundSwitch-parity finish line. |
| D-034 | Live show-package replacement uses generation-stamped atomic handoff; compiled semantic/safety changes may activate without stopping, while connection or project-identity changes require an explicit restart. | Scheduler, input, and output must all acknowledge the new immutable package before the old package is retired. Invalid candidates never replace the running show, and stale commands/frames cannot cross generations. |
| D-035 | SoundSwitch migration begins with a read-only, SHA-256 inventoried source bundle and controlled export comparison; semantic decoding remains sample-corpus gated. | Official guidance separates project data, lightshow data, and copied audio. The comparison records paths, hashes, and changed byte ranges without exporting source bytes, so one known change at a time can produce reproducible decoder evidence. Independently observed binary layouts are useful evidence but are insufficient for a lossless production decoder across versions. Unknown payloads must remain byte-identical and third-party unlicensed decoder code is not incorporated. |
| D-036 | The first native scripted-track slice is a manually triggered, beat-relative semantic cue list in the existing `TrackScript` layer. | It provides portable authored playback immediately without pretending OS2L exposes enough track identity/transport detail for automatic association. Projects persist optional non-path audio keys; compilation resolves cue targets into fixed arrays before activation; rewind/seek clears and replays state deterministically. A scripted Static Look and scripted Autoloop are deliberately mutually exclusive handoffs within that one layer, while manual performance controls remain higher-priority overlays. MIDI can start or clear a named script through the same compiled target contract. Direct VirtualDJ association, waveform/beatgrid editing, and complete transport semantics remain explicit parity work. |
| D-037 | Normal project saves retain a bounded, verified local restore-point history beside the project; internal active-runner snapshots do not. | A single backup handles immediate corruption, while multiple restore points protect authored-show mistakes without putting filesystem work on the Runner. Each history copy is checksum-validated before activation, pruning retains the newest 20 entries, and restore verifies the selected entry belongs to that project's history before atomically replacing the primary file. The pre-restore primary version becomes a new restore point. |
| D-038 | Studio keeps a bounded in-session Undo/Redo document stack that is separate from persisted restore points and never enters the Runner. | Authoring needs fast reversible exploration before a save, while durable history protects saved revisions. The stack stores up to 100 complete project states, clears at new/open/restore boundaries, and preserves the saved-state indicator when users return to the last saved document. It records only model mutations, not navigation or live-performance commands. |
| D-039 | Autoloop placement is a stable 64×32 address, with explicit safe move/swap operations rather than silent replacement. | Bank/slot edits must not accidentally overwrite another performance loop. Core placement helpers reject invalid or occupied targets, swap only after the user selects that explicit action, and can move a loop to the next open address across the full grid. These operations are Studio-only, preserve IDs/references, and participate in Undo/Redo; the Runner receives the changed compiled address only after normal save/activation. |
| D-040 | Live Autoloop bank filtering is transient Runner state with a pageable four-bank control window. | A performer can enable any banks, select one exclusive bank, or return to all 64 without changing the authored project. The scheduler owns the mask, exposes it in status, preserves it through compatible live package activation, and uses it only for Previous/Next selection; direct named launches remain intentional and unfiltered. |
| D-041 | Active Autoloop progress is a bounded Runner status snapshot, not UI-derived timing. | The scheduler chooses the highest-priority active Autoloop (manual, scripted, then autonomous) and atomically publishes its stable address, repeat mode, clamped 0–100% progress, and completed-cycle count. Live and Diagnostics consume that snapshot only; neither infer timing from UI intervals or modify playback. |
| D-042 | Live fixture overrides are transient bounded Runner commands with explicit property/all release. | A performer can apply a normalized property value to one active-package fixture or release that property, while Release All clears only the ManualOverride layer. Runner tracks the active property count for feedback; safety gates still apply, and overrides are cleared on stop or package activation instead of being written into authoring data. |
| D-043 | A group-level Live Override is one Runner command with an immutable fixture mask, not a UI loop of independent commands. | A partial multi-fixture override is unsafe and hard to reason about under queue pressure. Studio resolves the active project’s named group to stable fixture indices before posting. Runner validates the complete mask against the active package, applies every member only if all are valid, and uses the same bounded ManualOverride/safety path as individual fixture control. |
| D-044 | Release All Manual Overrides is also a targetless, MIDI-bindable Runner action. | Performers need a one-touch, hardware-independent way to return from temporary fixture or group control to authored playback. The mapping has no project target, posts through the existing bounded MIDI-to-scheduler route, clears only the transient ManualOverride layer when active, and retains the same safety and status semantics as the Live button. Appending the persisted enum preserves every earlier project mapping value. |
| D-045 | MIDI group-property mappings compile named group membership into the immutable active package. | Hardware group controls must not depend on Studio vectors or post one command per fixture at show time. The compiler resolves up to 256 named groups to fixed fixture-ID arrays, deduplicates legacy duplicate membership, and rejects a missing or empty group target. The scheduler applies each valid member through the same ManualOverride/safety path as a fixture mapping; the appended persisted action preserves earlier mapping values. |
| D-046 | MIDI bank pads select one exclusive Autoloop bank through Runner-owned transient state. | Bank navigation belongs on a performance controller as well as the Live window. A targetless bank index is persisted in the mapping, validated against the 64-bank catalog, and applies only on an active event; scheduler selection updates the existing status mask without changing authored project data. The action is appended to preserve prior mapping values. |
| D-047 | MIDI can enable/disable one Autoloop bank or restore all banks using the existing mapping behavior semantics. | Momentary controls enable while held, Toggle controls alternate a bank’s enabled state, and a one-shot All Banks action restores the unrestricted set. All changes remain transient scheduler-owned mask state and validate their bank indices at project load/compile time; appended enum values preserve earlier mapping values. |
| D-048 | Group blackout is a MIDI action that force-zeros visual emitter properties, rather than relying on a zero-value dimmer approximation. | A named group must black out reliably even when fixture profiles use additive, subtractive, or multi-emitter color channels. The compiled group action owns the twelve normalized optical properties (Intensity plus eleven emitters) as literal zero in ManualOverride, then releases exactly those properties on action release. It never moves pan/tilt, changes beam-selection attributes, or affects fixtures outside the group; the appended action value preserves prior mappings. |
| D-049 | Native audio associations are external, content-identified Studio records; they are not copied into show projects or required by Runner. | A track script can bind one optional audio asset by stable ID. Each asset records a SHA-256 digest, byte size, filename, and replaceable local path hint. Add/verify/relink and bounded music-folder recovery are read-only; a path changes only when size and digest match, preventing same-name substitutions. Runner continues to consume already compiled semantic cues and never opens, hashes, or allocates for media on the scheduling path. Legacy migration keys remain intact. |

## Superseded recommendations

| Earlier idea | Current status |
| --- | --- |
| Fork/incorporate the full QLC+ application. | Superseded by D-013/D-014. |
| Four always-running processes. | Superseded by lean logical modules: one Runner, Studio only while authoring, on-demand audio worker, optional adapter process. |
| More than two universes as an early superiority requirement. | Deferred post-V1 by D-003. |
| Control One as the sole controller model. | Superseded by D-009. |
| SoundSwitch migration probably unavailable. | Superseded by D-011; exact recoverability still requires samples. |

## Current user hardware facts

- DJ controller: Pioneer DDJ-REV7.
- Lighting controller: SoundSwitch Control One.
- Possible additional adapters: ADJ MyDMX Buddy and SoundSwitch simple USB-DMX interface; exact model/VID/PID unverified.
- Reported fixtures include Both Lighting IR-4 uplights, Both Lighting 360 LED Tubes, and CHAUVET DJ Wash FX Hex units; quantities and active DMX modes are pending.
- Joshua can later provide `.ssproj` and copied scripted audio samples.

## Workflow facts confirmed 2026-08-08

- Static Looks dominate dinner and special/formal moments.
- Autoloops may be used for grand entrances and first dances and, together with custom scripts, throughout dancing.
- Some events may run almost entirely on Autoloops and custom scripted songs.
- Static Look and Autoloop migration/authoring matter more for the first usable build than custom track-script migration.
- The product must not encode wedding-reception stages into its core state model.
- SoundSwitch 2.9 documents immediate manual Autoloop launch, completion for the loop's duration, then return to the scripted track. Static Looks are latched sparse scene overlays; public documentation does not specify a beat/bar-quantized release transition.

## Questions for Joshua

### Next hardware/reliability round

1. Exact DJ laptop model, CPU, RAM, GPU, and Windows version?
2. Which lower-end Windows PC can we use as the minimum/reference test machine?
3. Do you have a wired Ethernet adapter/switch available for the separate-lighting-computer setup?
4. Are you comfortable temporarily running QLC+ as a bridge if one of your USB dongles lacks a native driver?

### Workflow round

5. Which SoundSwitch screens/actions do you rely on most during a gig?
6. Which live actions must be one-touch and impossible to bury in menus?
7. After an A/B hardware test, should any trigger differ from the provisional return model in D-020?

### First qualification round

8. How many IR-4s, 360 Tubes, and Wash FX Hex fixtures make up the normal rig, and which other models belong in its first profile pack?
9. Which DMX mode/channel count do you normally use on each fixture?
10. Which effect fixtures require hard safety locks—fog/haze, laser, cold sparks, confetti, others?
11. Which unrelated fixture/controller combination should join Joshua's rig as the second qualification matrix?

### Product round

12. Should the EmberLights performance UI visually echo familiar SoundSwitch concepts or deliberately feel new while retaining workflow?
13. What is the oldest Windows release we should intentionally support: Windows 10, Windows 11, or evidence-based support for both?
14. For Serato, which workflow must qualify first: exact scripted-track transport, automatic beat-synced Autoloops, or both together?

## Questions that do not block the native core

All questions above affect hardware validation, defaults, or UI. None changes the accepted semantic/layer/adapter boundaries, so core work continues while answers arrive.
