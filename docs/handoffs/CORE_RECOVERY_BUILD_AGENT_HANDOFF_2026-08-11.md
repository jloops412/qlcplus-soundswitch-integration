# EmberLights Core Recovery Build-Agent Handoff

Status: **Planning complete — implementation may begin**  
Date: 2026-08-11  
User: Joshua  
Physical-test baseline: EmberLights `0.1.0-preview.314`, source `4f6111493377d2b366836c3579ad5514042d177f`, merged as `01735d18834cb8d2c2ec5bcbecd5ac3420a76860`  
Implementation base: current `main`; preserve all concurrent planning work, but this handoff governs the next core implementation slice.

## 1. Mission for this build

Produce one Windows installer that lets Joshua determine, with minimal navigation and no fixture-library guesswork, exactly where the current dark-output failure occurs:

```text
EmberLights command/state
  -> rendered 512-slot universe
  -> selected output route
  -> SoundSwitch Micro session initialized
  -> exact USB frame accepted
  -> physical DMX observed
  -> exact fixture mode/address interprets the slots
```

The build must then prove one complete raw-to-semantic physical path, repair the blocking Connections and OS2L startup behavior, and complete Static Look Toggle/Hold core semantics.

This is not a broad UI/skin iteration. It is not a fixture-marketplace iteration. It is the core recovery build that must be ready for Joshua's morning hardware test.

## 2. Authority and conflict resolution

Read in this order:

1. `AGENTS.md`
2. `docs/00_START_HERE.md`
3. this handoff
4. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`
5. `docs/23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md`
6. `docs/24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md`
7. only the code/architecture/test documents directly needed for the bounded change

`docs/24_FIXTURE_LIBRARY_AND_PROFILE_QUALIFICATION_PLAN.md` is useful supporting research created concurrently. It does not supersede the active order above. Where it differs on the first owned bench fixture, use the newer manufacturer evidence in `24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md`: **Both Lighting IR-4 in 6-channel mode at address 1 is the first fixture test.**

Broad UI plans, issue #29, and UI issues #30–#37 remain subordinate until the core-ready gates pass.

## 3. Facts already established

From Joshua's preview.314 diagnostics:

- project schema validation passed;
- Runner and Autoloop timing advanced;
- OS2L accepted a VirtualDJ client and decoded messages;
- the Micro WinUSB handle opened;
- 1,276 frame writes were accepted with zero host-side write failures;
- the selected universe contained 78 nonzero slots;
- no physical fixture responded;
- Save & Apply was hidden/overlapped and reachable only by tabbing;
- VirtualDJ did not open its direct-IP OS2L path until a DMX/OS2L button was invoked;
- scheduler p99/max jitter reached approximately 31.9 ms with many >5 ms deadline misses.

These facts do **not** prove physical DMX. Accepted WinUSB writes only prove the Windows transfer boundary accepted bytes.

The current 71-fixture migration is deliberately a non-overlapping staging patch. Its addresses and channel semantics are not a decoded physical rig, even though it validates internally.

## 4. Ranked failure hypotheses

Investigate in this exact order:

1. **Fixture mode/address/profile mismatch.** The staging patch is provisional. A required master dimmer, shutter-open, or mode-specific channel may remain zero.
2. **Micro session lifecycle incomplete.** The exact JLS1 packet bytes are plausible, but the current session omits the settling/warm-up behavior used by independent working evidence.
3. **WinUSB interface state.** Configuration/alternate setting/pipe reset/policies are insufficiently explicit or observable.
4. **Wrong actual rendered channels.** A nonzero count does not identify the target slot/value or winning source.
5. **Physical wireless/electrical path.** The transmitter is known good, but the software test still needs a direct physical-boundary observation.
6. **Scheduler cadence.** Poor timing must be fixed, but it is unlikely to explain total darkness by itself when hundreds of writes are accepted.

Do not switch between these hypotheses informally. The Hardware Test and frame comparison must identify the furthest verified boundary.

## 5. Implementation order

### Slice A — Shared SoundSwitch Micro session and raw Hardware Test (P0, first)

Extract one production `SoundSwitchMicroSession` used by both Runner and the installed Hardware Test. Do not create a second divergent driver.

Required lifecycle:

```text
Disabled
Detecting
Opening
Inspecting
Initializing
Settling
WarmingUp
Streaming
Recovering
Fault
Closing
```

Required deterministic open sequence:

1. enumerate exact interface and verify `VID_15E4/PID_0053`;
2. open with the WinUSB-required overlapped flags;
3. initialize WinUSB;
4. query and record device/configuration/interface/pipe descriptors;
5. verify the active configuration rather than assuming it;
6. query and explicitly select alternate setting 0 where appropriate;
7. reset the bulk OUT pipe;
8. keep RAW_IO disabled so WinUSB segments 522-byte writes for the 64-byte endpoint;
9. use a bounded transfer timeout;
10. send the two exact 12-byte JLS1 initialization messages in order;
11. wait a conservative 200 ms default settling interval off the scheduler thread;
12. send 50 blackout frames at 40 Hz by default;
13. enter Streaming and publish only the newest available universe frame.

Every reconnect repeats the full sequence. Every close/failure path attempts a bounded blackout train without deadlocking.

The installed Hardware Test must work without an `.emberlights` project and provide:

- Inspect / descriptor report;
- Initialize Only;
- Blackout Stream;
- Single Channel with explicit 1–512 channel, 0–255 value, and bounded duration;
- bounded Address-Range Chase;
- explicit channel/value full-frame test;
- Reconnect Test;
- Blackout Now;
- Export Report.

Active tests must start and finish at blackout. Do not automatically test fog, haze, laser, spark, reset, calibration, or high-rate strobe channels.

### Slice B — Frame truth and source attribution (P0)

Add a snapshot-based frame inspector/export outside the real-time allocation path. For each bounded nonzero slot, report:

```text
universe
DMX channel
raw value
fixture ID/name, if patched
profile/mode/channel mapping
semantic property or constant/default
winning layer/source
output adapter and selected universe
frame hash
```

Expose pre-blackout versus post-blackout state. The raw Hardware Test report and Runner report must use a comparable frame representation and hash.

Change the migration validation result. The current project must emit at least:

```text
MIGRATED_PATCH_UNVERIFIED
```

It must never report zero warnings while physical addresses, modes, and profiles remain unverified.

### Slice C — One exact IR-4 bench project (P0)

Use one Both Lighting IR-4 only.

Physical test assumptions must be explicit and operator-confirmed in the morning:

```text
Fixture model: BOIR4 / IR-4
Mode: 6-channel
Universe: 1
Address: 1
Master: Off
```

Official 6-channel map:

```text
CH1 Red
CH2 Green
CH3 Blue
CH4 White
CH5 Amber
CH6 Purple/UV documentation ambiguity
```

Generate an installed bench project:

```text
Joshua IR-4 Raw-to-Runner Bench
1 fixture
Micro universe 1
all unrelated outputs disabled by default
Static Looks: Blackout, Red, Green, Blue, White, Amber
no OS2L or Autoloop dependency for initial proof
```

Required raw test order:

```text
all zero
CH1=255 red
CH1=0, CH2=255 green
CH2=0, CH3=255 blue
CH3=0, CH4=255 white
CH4=0, CH5=255 amber
all zero
```

Do not automatically test CH6 until the manual's Purple/UV wording is recorded.

Required interpretation:

- raw CH1 red fails → stay in transport/session/electrical diagnosis; do not edit Autoloops or broad profiles;
- raw CH1 red works → transport and physical universe path are proven for that state;
- raw works but Runner does not → compare frames and fix profile/patch/compiler/layer/output routing;
- Runner frame matches the successful raw frame but behavior differs → inspect session ownership/reinitialization/cadence rather than changing semantics blindly.

After six-channel qualification, add a separate 10-channel profile test where CH1 is the master dimmer and CH2–CH7 are colors. Never reuse the six-channel qualification for the ten-channel mode.

### Slice D — Fixture evidence and immutable qualification (P0/P1)

Implement the minimum evidence model without bloating Runner:

- deterministic native-profile SHA-256;
- source kind/revision/content hash/importer version;
- manual reference/hash;
- conversion warnings;
- profile qualification state;
- per-fixture universe/address/mode qualification;
- raw report hash;
- Runner frame hash;
- operator-confirmed physical result.

Preferred append-only records:

```text
PROFILE_EVIDENCE
PROFILE_QUALIFICATION
FIXTURE_QUALIFICATION
```

Missing evidence means unverified, never implicitly qualified. Changing a behavior-affecting profile, universe, address, or physical mode invalidates the relevant qualification.

Use the existing Studio-only QXF importer as the immediate open fixture-library bridge. Preserve and hash manufacturer-hosted Both Lighting `.ssl2` files when the build environment can retrieve them, but do not commit or redistribute their raw contents unless licensing permits. Probe their format read-only; do not bypass protection.

Do not use the OFL `Chauvet DJ WashFX` profile as a Wash FX Hex personality—the documented footprints differ. Tubes and Wash FX Hex follow only after the simple IR-4 path works.

### Slice E — Connections persistence and lifecycle (P0)

Repair the existing operational defect without waiting for the new skin system:

- sticky always-visible **Save & Apply Connections** action bar;
- vertical scrolling at low height/high DPI;
- no overlapping controls;
- visible Refresh;
- reading-order tab sequence;
- Enter/default and Alt+A command access;
- geometry smoke tests at 1366×768 and 1920×1080 across relevant DPI scales.

Implement one truthful transaction:

```text
read controls
  -> parse/validate
  -> atomically save desired project settings
  -> compute desired-vs-active diff
  -> blackout/close only affected old outputs
  -> apply/restart only affected services
  -> publish bounded outcome per endpoint
```

Outcomes:

```text
SavedAndApplied
SavedNoRuntimeChange
SavedRestartRequired
SavedApplyFailed
ValidationRejected
SaveFailed
```

A setting can be Saved but not Active. Never label that state Ready.

Begin a bounded `ConnectionCoordinator` extraction with replaceable `DjTransportService`, `ControllerService`, and `OutputRouter` boundaries. Do not rewrite the app before Slice A works.

### Slice F — Deterministic VirtualDJ/OS2L startup (P0)

Use standards-based discovery first:

```text
os2l=Auto
os2lDirectIp=<blank>
```

When Windows DNS-SD discovery is unavailable, expose the direct-IP fallback
with `os2l=Yes`, the displayed listener endpoint, and this copyable ONINIT
action:

```text
wait 100ms & os2l_button 'EmberLights Keepalive' off
```

`EmberLights Keepalive` is a reserved no-op intercepted before performance
action routing. It activates VirtualDJ's direct-IP path without clearing an
intentional blackout, changing a Look, or requiring a performance pad.

Move OS2L listener lifetime out of Start Show. When a loaded project enables OS2L, the listener should be available before Runner starts.

Expose truthful states:

```text
disabled
listener starting
listening / waiting for VirtualDJ
client connected / no beat yet
receiving beat/transport
held/fallback clock
fault with bind/error detail
```

Test discovery and fallback across all launch orders and VirtualDJ restart.
DNS-SD advertisement exists, but installed Windows/VirtualDJ evidence and
listener lifetime outside Start Show remain open gates.

### Slice G — Static Look Toggle/Hold semantics (P0/P1)

Implement once in core commands/state, not separately in UI and MIDI.

Conceptual commands:

```text
staticLook.activate(lookId, sourceToken)
staticLook.deactivate(lookId, sourceToken)
staticLook.toggle(lookId, sourceToken)
staticLook.clear()
```

Required behavior:

- Static Look owns the existing higher-priority `EventMoment` layer;
- Toggle press latches/unlatches the selected look;
- Hold press activates and release clears only the activation owned by that press/source token;
- an old release event cannot clear a newer activation;
- authoritative active state drives UI/MIDI/Control One feedback;
- the lower Autoloop continues advancing while hidden;
- release reveals the Autoloop at its current phase, not from the beginning;
- blackout and safety layers retain their proper higher priority.

Test UI, keyboard, MIDI, future Control One binding, duplicate release, rapid replacement, project activation, stop, and connection restart boundaries.

### Slice H — Scheduler timing and hardening (P1 after raw physical proof)

Profile the observed ~31.9 ms jitter before guessing. Measure scheduler wakeup, render, queue publish, output consumer, UI/status/log contention, and timer resolution separately.

Targets for the next build:

- fixed intended 40 Hz cadence;
- no unbounded queues;
- newest-frame-wins after stalls/reconnect;
- no logging/file/network/USB work on the scheduling path;
- bounded status snapshots;
- materially lower p99 jitter and deadline misses;
- ten-minute hardware stream before a later eight-hour gate.

Do not optimize away evidence or safety handling.

## 6. Control One path — required architecture, separate qualification

Joshua ultimately requires both the SoundSwitch Micro and Control One to work reliably.

Official SoundSwitch guidance establishes two different boundaries:

- Control One can be used as a MIDI controller with other software;
- SoundSwitch says its proprietary DMX/OLED/full feedback behavior is designed for SoundSwitch/Engine Lighting and is not supported as another DMX application's generic interface.

Therefore:

### Immediate Control One scope

- preserve generic WinMM MIDI input/output support;
- resolve Control One by stable device identity rather than enumeration index;
- build a bundled mapping profile over the shared command registry;
- map Static Looks, Autoloop banks/slots, blackout, intensity, color/position overrides, tap/cue, and release actions as supported by observed MIDI messages;
- add a safe MIDI capture/report path for Joshua's device;
- derive feedback from authoritative state, never optimistic button clicks;
- keep missing OLED/RGB feedback non-fatal and truthfully reported.

### Proprietary Control One DMX track

Treat the onboard two-universe DMX outputs as a separate output adapter and hardware-research program:

1. capture exact Windows USB identities/interfaces/descriptors for both USB ports;
2. distinguish MIDI/control, storage, DMX, and firmware interfaces;
3. inspect lawful public/installed-driver evidence and controlled behavior;
4. create a descriptor-only probe first;
5. use clean-room bounded output tests only after a credible application contract is established;
6. implement `ControlOneDmxAdapter` behind the same OutputRouter/session/qualification contract as Micro;
7. qualify each universe, blackout, reconnect, dual-USB ownership, and stale-frame handling physically;
8. never claim DMX/OLED support from MIDI functionality alone.

Do not let the proprietary DMX investigation block tomorrow's Micro/IR-4 build. Do not hard-code fixture semantics or Runner around either device.

## 7. Automated and installed-build gates

Required unit/integration tests:

- exact JLS1 init/frame bytes and channel offsets;
- every Micro lifecycle transition/failure;
- settle/warm-up ordering;
- reconnect discards stale frames;
- blackout on close/failure;
- Hardware Test self-test performs no output by default;
- frame inspector attribution and deterministic hash;
- profile hash/evidence round-trip and invalidation;
- raw-to-Runner frame comparison;
- migrated-patch warning;
- Save & Apply geometry, validation, persistence, diff, and failure outcomes;
- OS2L launch-order/reconnect matrix;
- Static Look Toggle/Hold ownership and Autoloop phase continuation;
- installed GUI startup and installed Hardware Test self-test.

Required artifact contents:

```text
EmberLights.exe
EmberLights Hardware Test
SoundSwitch Micro Probe
Joshua IR-4 Raw-to-Runner Bench project
MORNING_HARDWARE_TEST.md
MORNING_FIXTURE_TEST.md
release manifest
SHA-256 checksums
```

The installer, reports, version string, commit, and GitHub artifact must all identify the same source commit.

## 8. Morning test UX

Joshua's first test should require no deep menus:

1. close SoundSwitch and any process that might own the Micro;
2. set one IR-4 to 6-channel mode, universe 1/address 1, Master Off;
3. open **EmberLights Hardware Test**;
4. Inspect + Initialize;
5. Blackout Stream and observe the transmitter/fixture DMX indicator;
6. Single Channel: CH1=255 for five seconds;
7. confirm red or report no response;
8. verify automatic blackout;
9. repeat close/reopen and, if practical, unplug/replug;
10. open the installed one-fixture bench project;
11. trigger Red and compare/export its frame against the successful raw frame;
12. test visible Save & Apply persistence;
13. start VirtualDJ with the provided ONINIT action and confirm beat without pressing a DMX pad;
14. test one Toggle Static Look and one Hold Static Look over a running Autoloop;
15. export Hardware Test and Diagnostics reports.

## 9. Stop conditions

- If raw IR-4 CH1 red fails, do not spend time adjusting Autoloops, broad fixture profiles, or skin code.
- If the Micro cannot guarantee blackout, do not proceed to active fixture discovery.
- If source mode/footprint differs from the exact manual, quarantine it.
- If a proprietary file is protected or its redistribution rights are unknown, preserve only lawful local evidence/hashes; do not bypass or package it.
- If Save & Apply remains hidden at any supported geometry, the build is not ready for Joshua.
- If VirtualDJ still requires a manual performance pad after the documented startup action, report the exact connection state and keep investigating the client activation path.
- If Static Look release restarts the Autoloop, the semantic gate has failed.
- Do not mark Micro or Control One DMX `Supported` from software-open, accepted-write, MIDI, or profile-library evidence.

## 10. Definition of implementation success

The scheduled agent's first deliverable is successful when:

- a tested installer is available from GitHub;
- the raw Hardware Test is safe and independently usable;
- Joshua can make one binary observation: IR-4 raw CH1 red works or does not;
- every software boundary up to that observation is visible in the report;
- a successful raw frame can be reproduced byte-for-byte through the one-fixture Runner project;
- the provisional migration reports visible warnings;
- Connections Save & Apply is accessible and truthful;
- VirtualDJ can activate OS2L through the documented startup action without pressing a DMX pad;
- Static Looks support Toggle and Hold and release to the advancing Autoloop;
- the architecture remains ready for other fixtures, adapters, generic MIDI, and later clean-room Control One DMX work;
- no broad UI/skin or catalog work displaced these gates.

No additional planning decision is required before implementation begins.
