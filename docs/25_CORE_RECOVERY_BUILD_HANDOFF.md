# EmberLights Core Recovery Build Handoff

Status: **APPROVED TO EXECUTE — planning complete**  
Date: 2026-08-11  
Repository: `jloops412/EmberLights`  
Immediate baseline under test: Windows preview.314, source commit `4f6111493377d2b366836c3579ad5514042d177f`, merged as `01735d18834cb8d2c2ec5bcbecd5ac3420a76860`  
Implementation base: latest `main`, rebased immediately before work begins  
Primary operator: Joshua Pelling  
Primary objective: deliver the smallest installed Windows build that can decisively prove and then harden the complete path from a raw channel value to physical DMX and one verified fixture, while repairing the blocking connection and interaction mechanics needed for normal testing.

---

## 0. Authorization and handoff contract

The planning phase is complete. The scheduled work agent may begin implementation immediately from the latest `main`.

This handoff is subordinate only to the repository's mission and accepted safety architecture. It is authoritative for the next core build. Broad UI/UX, skin authoring, decorative work, unrelated feature expansion, and speculative migration decoding remain paused until the physical core gates in this document are satisfied.

The implementer must not restart planning from zero. Read the required files, verify the source baseline, then execute the slices in order.

Required reading, in this exact order:

1. `docs/00_START_HERE.md`
2. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`
3. `docs/adr/0005-core-hardware-qualification-and-service-boundaries.md`
4. `docs/24_FIXTURE_LIBRARY_AND_PROFILE_QUALIFICATION_PLAN.md`
5. This file
6. `docs/09_BUILD_AND_TEST_STANDARDS.md`
7. Only the source files and narrower specs referenced by the active slice

The UI/skin program remains valid, but it may not displace or reinterpret this recovery program.

---

## 1. Morning outcome

The next Windows installer should let Joshua answer these questions without guessing:

1. Is the SoundSwitch Micro physically detected?
2. Did the exact JLS1 initialization sequence complete?
3. Did the adapter pass settling and blackout warm-up?
4. Did Windows accept the exact expected packets?
5. Did the transmitter or direct fixture observe valid DMX?
6. Did one explicit raw channel/value produce a physical response?
7. Did the same response survive close/reopen and unplug/replug?
8. Can Runner emit the byte-identical frame through one exact fixture profile and patch?
9. Are desired, saved, applied, and active connection settings visibly identical?
10. Can VirtualDJ begin OS2L traffic without a manual performance-pad press using the documented startup path?
11. Do Static Looks support Toggle and Hold, override a running Autoloop, and release back to the Autoloop's advanced phase?

A build that merely reports `Micro: Ready`, increments accepted writes, or shows a moving BPM is not sufficient.

---

## 2. Ground truth from preview.314

The operator's report proves:

- project schema validation passed;
- Runner advanced frames and beat position;
- an Autoloop was active;
- OS2L accepted one client and decoded messages;
- the Micro device path opened through WinUSB;
- 1,276 522-byte writes were accepted without a host error;
- the selected universe contained 78 nonzero slots;
- no physical fixture response was observed;
- Save & Apply was visually inaccessible and only reachable through tab navigation;
- VirtualDJ direct-IP OS2L did not connect until an OS2L/DMX command was invoked;
- scheduler jitter was poor enough to require later hardening.

This proves the system reached the host USB-write boundary. It does **not** prove device initialization, physical DMX, fixture mode, fixture address, or profile correctness.

The active project is explicitly named `PATCH REVIEW REQUIRED`. The converter staged non-overlapping addresses and provisional linear profiles. Zero schema warnings in preview.314 were misleading because physical qualification metadata did not yet participate in validation.

---

## 3. Source findings that drive the work

### 3.1 Current Micro implementation

`native-core/src/soundswitch_micro.cpp` currently:

- discovers `VID_15E4/PID_0053` through the installed interface GUID;
- opens with `FILE_FLAG_OVERLAPPED`;
- calls `WinUsb_Initialize`;
- verifies interface 0 and bulk OUT pipe `0x01`, 64-byte maximum packet;
- applies a transfer timeout and disables short-packet termination;
- writes the two exact 12-byte JLS1 initialization packets;
- immediately returns open/ready;
- writes a complete 522-byte frame when `send()` is called.

It does not currently:

- expose a lifecycle beyond open/fault;
- query/report the full device/configuration/interface/alternate-setting state;
- explicitly select alternate setting 0;
- reset the output pipe before initialization;
- wait after initialization;
- send a blackout warm-up train;
- distinguish initialized/warmed/streaming/physically-confirmed;
- provide a raw physical output test independent of a project;
- own a deterministic blackout-close sequence.

### 3.2 Current Runner coupling

`RunnerService` currently owns:

- OS2L server lifetime;
- MIDI input/output lifetime;
- output adapters;
- show scheduling;
- Static Look interaction;
- connection settings copied at start.

This is acceptable for the existing vertical slice but not the target architecture. Extraction must be incremental: prove the Micro first, then move lifecycle ownership behind shared services without rewriting the entire application before a physical test is possible.

### 3.3 Current Static Look behavior

The scheduler already ticks lower Autoloops before the `EventMoment` Static Look and resolves layers afterward. This means the desired background-phase behavior is already structurally possible. The missing pieces are a typed Toggle/Hold command contract, source ownership, stale-release protection, and authoritative feedback.

### 3.4 Current OS2L behavior

The TCP server is created inside `RunnerService::run_input()`. It cannot listen before Runner starts. It supports one active client at a time and receives messages, but it has no DNS-SD advertisement and no duplex feedback path. VirtualDJ direct-IP is client-initiated and may remain dormant until a command exercises OS2L.

### 3.5 Current fixture model

The normalized fixture model supports simple coarse/fine linear properties, but it does not yet faithfully represent all ranges, wheels, cells, master cells, shutter functions, macros, safety classes, source licensing, or qualification evidence. Do not block the one-fixture proof on the complete future schema. Use a sidecar or small forward-compatible addition where necessary, then schedule the fuller model migration after the physical gate.

---

## 4. Root-cause decision tree

The implementer must use this tree rather than changing several layers at once.

### Branch A — raw Hardware Test produces no physical response

The fault remains below fixture semantics.

Investigate, in order:

1. exact device/configuration/interface/alternate-setting state;
2. pipe reset and policies;
3. JLS1 initialization ordering;
4. 200 ms settling;
5. 50 blackout frames at 40 Hz;
6. exact active frame bytes and channel offset;
7. native WinUSB versus a bounded reference transport;
8. official SoundSwitch capture comparison on the same Windows machine;
9. physical DMX/transmitter/direct-cable observation.

Do not edit Autoloops or fixture profiles as a substitute.

### Branch B — raw Hardware Test works, but one-fixture Runner project does not

The adapter is proven. Freeze its protocol bytes and lifecycle unless new evidence shows a transport regression.

Investigate:

1. selected universe;
2. physical start address;
3. physical DMX mode/channel count;
4. profile channel offsets/ranges/defaults;
5. master dimmer/shutter/open values;
6. compiler output;
7. layer winner and blackout/safety suppression;
8. raw-versus-semantic frame difference.

### Branch C — raw and semantic frames match, but a fixture is still dark

The current test conditions are not equivalent. Reconfirm:

- the exact fixture and cable/transmitter path;
- the fixture display address and mode;
- the active universe;
- whether a direct-wired test differs from wireless;
- whether a required master/shutter value was omitted;
- whether the comparison frame is pre- or post-blackout.

### Branch D — native WinUSB fails but bounded reference transport works

The JLS1 bytes and fixture path are substantially corroborated. The defect is in Windows session lifecycle or WinUSB configuration/policy. Keep the reference path diagnostic-only until the native cause is understood, unless an accepted decision explicitly adopts it as the supported adapter backend.

### Branch E — both EmberLights transports fail but SoundSwitch works immediately on the same setup

Capture the official application with USBPcap/Wireshark or equivalent owned-machine tooling. Compare configuration/interface operations, control/bulk ordering, timing, transfer lengths, and complete frame bytes. Preserve captures with hashes and redact unrelated device traffic. Do not add another speculative framing selector.

---

## 5. Implementation slices and hard gates

The slices are ordered. Each slice must remain buildable and testable. A later slice may begin in a separate non-conflicting lane, but it cannot be represented as complete before the earlier acceptance gate.

---

# Slice 0 — Baseline, fixtures, and test seams

## 5.0.1 Branch and baseline

Create or use one integration branch based on the newest `main`, for example:

```text
agent/core-recovery-hardware-proof
```

Before modifying code:

- record the base commit;
- run the existing Windows/Linux core test suite;
- run packaging smoke where available;
- record preview.314 behavior as a regression fixture;
- identify concurrent UI-agent changes and avoid overwriting them;
- establish one integration owner for `windows_app.cpp`, `CMakeLists.txt`, installer, and workflow files.

## 5.0.2 Introduce transport seams

The production Micro implementation and Hardware Test must share one session implementation. Do not create a second copy of packet and USB lifecycle code.

Recommended boundaries:

```text
ISoundSwitchMicroTransport
  enumerate/open
  query descriptors
  query/set alternate setting
  reset/abort pipe
  set pipe policy
  write exact bytes
  close

SoundSwitchMicroSession
  inspect
  initialize
  settle
  warm_up_blackout
  stream
  blackout_close
  recover
  snapshot

WinUsbSoundSwitchMicroTransport
FakeSoundSwitchMicroTransport
OptionalDiagnosticReferenceTransport
```

The fake transport records exact calls, byte buffers, errors, and monotonic timing boundaries. Production code must be testable without a device.

## 5.0.3 Baseline acceptance

- Existing tests still pass.
- Fake transport can reproduce preview.314's immediate-init behavior before the new lifecycle is enabled.
- No UI or fixture change is needed to instantiate and test the session.

---

# Slice 1 — Shared SoundSwitch Micro session and installed Hardware Test

This is the highest-priority deliverable. Package it before partially completing broad UI work.

## 5.1.1 Protocol constants

Retain the current exact packets unless controlled evidence contradicts them.

Initialization:

```text
73 54 52 74 02 00 04 00 00 00 01 00
73 54 52 74 02 00 04 00 01 00 FF FF
```

Frame:

```text
73 54 52 74             sTRt
01 00                   DMX command
02 02                   payload length 514
00 00                   protocol payload prefix
<512 DMX slots>         channel 1 at packet offset 10
```

Total length: 522 bytes.

Golden tests must prove:

- channel 1 maps to packet byte 10;
- channel 512 maps to packet byte 521;
- a blackout frame contains no nonzero slots;
- packet length, command, payload length, and init bytes are exact;
- no project or fixture code can alter the packet contract.

## 5.1.2 Lifecycle states

Replace the ambiguous adapter state for Micro diagnostics with a richer bounded lifecycle:

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
Closed
```

Keep the public generic adapter summary if needed, but publish the detailed Micro state separately.

Also publish independent qualification boundaries:

```text
devicePresent
handleOpen
protocolInitialized
settlingComplete
warmupComplete
writeAccepted
physicalDmxObservation = Unknown | Confirmed | Failed
fixtureObservation = Unknown | Confirmed | Failed
```

Only the operator or imported test evidence can set physical observations. Host writes never do.

## 5.1.3 Native Windows open sequence

Implement this sequence off the scheduler thread:

1. Enumerate the exact interface GUID and verify VID/PID.
2. Open the path with `FILE_FLAG_OVERLAPPED`.
3. Call `WinUsb_Initialize`.
4. Query device descriptor and complete configuration descriptor.
5. Record configuration value, interface number, current alternate setting, endpoint type/address/max packet, and relevant policies.
6. Verify the descriptor corresponds to the expected configuration/interface. If configuration value is not the expected value, fail with a precise report rather than guessing.
7. Query and explicitly select alternate setting 0 when supported/appropriate.
8. Ensure no I/O is outstanding, then reset bulk OUT pipe `0x01`.
9. Keep RAW_IO disabled so WinUSB may split 522 bytes over the 64-byte endpoint.
10. Keep short-packet termination disabled unless new evidence requires it.
11. Apply a bounded transfer timeout. Report timeout separately from other write failures.
12. Write START init packet.
13. Write LED init packet.
14. Sleep 200 ms by default on the adapter/session thread.
15. Write 50 blackout frames at 40 Hz.
16. Enter Streaming and consume only the newest available frame.

Do not attempt to invent a Windows `setConfiguration` operation through an undocumented path. Query and report the active configuration first. A diagnostic reference backend may explicitly exercise configuration selection only through a supported library/API in its own bounded test path.

## 5.1.4 Streaming and recovery

- One in-flight write at a time.
- Queue policy remains newest-frame-wins.
- A write fault closes the session and enters Recovering.
- Reconnect repeats inspect/init/settle/warm-up from the beginning.
- After warm-up, consume only the newest frame; never replay a backlog.
- Track last accepted write age and reconnect count.
- On normal close, send at least 20 blackout frames at 40 Hz when the handle remains usable.
- On application stop, send the bounded blackout train before release.
- A failed blackout attempt is reported but may not deadlock shutdown.
- Device ownership conflict must produce a specific actionable error.

## 5.1.5 Installed Hardware Test application

Add an installed executable named clearly, for example:

```text
EmberLights Hardware Test.exe
```

or retain a console tool only if it presents a genuinely simple guided flow. A small native dialog is preferred because Joshua needs to test quickly.

It must operate without loading an `.emberlights` project.

Required actions:

### Inspect Device

Report:

- application version/commit;
- VID/PID and device path;
- device/configuration/interface descriptors;
- current alternate setting;
- endpoint and pipe policies;
- driver/service identity where available;
- ownership conflict;
- every lifecycle stage and Windows error.

### Initialize + Warm Up

- execute the full production session sequence;
- display each completed stage;
- ask the operator whether the Micro indicator changed;
- do not claim DMX because initialization completed.

### Blackout Stream

- stream zeros at 40 Hz;
- allow the operator to confirm the transmitter/fixture DMX indicator;
- remain safe on timeout or close.

### Single Channel

Inputs:

```text
channel: 1..512
value: 0..255
hold duration: default 5 seconds, maximum 30 seconds
```

Before output, display the exact nonzero slot list and frame hash. Automatically return to blackout.

### Bounded Address/Footprint Chase

Inputs:

```text
start address
footprint
safe test value set
per-step duration
```

Never automatically activate fog, haze, spark, laser emission, calibration/reset, or high-rate strobe assumptions. The operator can explicitly test a raw value, but the tool must label that as a raw action and enforce timeout/blackout.

### Explicit Frame

Accept a bounded list of channel/value pairs, show the complete normalized list, hash it, and send for a bounded duration.

### Reconnect Test

- blackout and close;
- reopen through the full sequence;
- repeat the approved raw frame;
- report whether bytes and physical observation match.

### Blackout Now

One unmistakable always-enabled action. It must be keyboard accessible and independent of the currently selected test page.

### Export Report

Export both human-readable text and JSON, containing:

- build/version/commit;
- hardware descriptors;
- lifecycle timestamps;
- exact packet/frame hashes;
- exact nonzero channel/value pairs;
- transfer counts/errors;
- reconnect counts;
- operator Yes/No/Unknown observations;
- automatic-blackout result;
- no credentials or unrelated USB data.

## 5.1.6 Diagnostic contingency backend

Do **not** add a second production driver by default.

If the hardened WinUSB sequence still produces no physical response, add a bounded diagnostic reference backend that uses a supported libusb-compatible path against the same owned device and replays the same init/settle/blackout/active sequence. Keep it clearly labeled `Diagnostic reference — not production qualified`.

Decision:

- reference works, native fails → inspect WinUSB lifecycle/policies/configuration;
- both work → remove or retain reference only as a development diagnostic;
- both fail, SoundSwitch works → capture and compare official traffic;
- both fail, SoundSwitch also fails → physical setup/driver/ownership changed.

## 5.1.7 Slice 1 automated tests

Add tests conceptually equivalent to:

```text
micro_packet_golden_bytes
micro_channel_offsets
micro_session_happy_path_order
micro_session_init_failure_by_stage
micro_session_settle_before_warmup
micro_session_warmup_count_and_cadence_boundary
micro_session_newest_frame_after_recovery
micro_session_blackout_close
micro_session_unplug_replug
hardware_test_self_test_is_output_disabled
hardware_report_schema_and_hashes
```

The fake transport should assert ordering, not sleep in unit tests. Use an injected clock/sleeper.

## 5.1.8 Slice 1 release gate

Package a Windows installer as soon as all software tests pass. Release notes must say physical confirmation remains pending.

The gate is complete only after Joshua confirms at least one of:

- transmitter reports valid DMX during Blackout Stream;
- a direct fixture or transmitter path responds to an explicit raw channel/value;
- a known raw frame works after reopen and unplug/replug.

If physical testing is not yet possible, ship the tool and report honestly that the software slice is ready for operator qualification.

---

# Slice 2 — Frame truth, fixture evidence, and one-fixture semantic proof

## 5.2.1 Last-frame snapshot

Publish a bounded immutable output snapshot outside the real-time scheduler:

```text
generation
sequence
timestamp
selected adapter/universe
pre-blackout frame hash
post-blackout frame hash
nonzero slot count
bounded nonzero channel/value list
```

The scheduler copies fixed-size frame data only. Formatting, hashing, ownership lookup, and report generation occur off the scheduler thread.

## 5.2.2 Frame inspector

For displayed channels, resolve:

```text
universe/channel/value
fixture instance, if patched
physical address and mode
profile revision
profile channel/function
semantic property or raw/default source
winning layer/source where available
profile qualification state
patch qualification state
```

At minimum, the first morning build must expose raw values and fixture ownership. Full per-property layer attribution may follow in the same slice if it can be added without destabilizing the scheduler.

## 5.2.3 Truthful migrated-project validation

Projects created by the conservative SoundSwitch converter must emit visible warnings. Add at least:

```text
MIGRATED_PATCH_UNVERIFIED
MIGRATED_PROFILE_UNVERIFIED
```

The message must explain that non-overlapping staging addresses and provisional profiles are not decoded physical truth.

Do not convert these warnings into blocking errors for Hardware Test. They should block `PhysicalReady` or equivalent readiness claims.

## 5.2.4 Fixture artifact/provenance skeleton

Implement the minimal local content-addressed boundary from `docs/24_FIXTURE_LIBRARY_AND_PROFILE_QUALIFICATION_PLAN.md`:

```text
%LOCALAPPDATA%\EmberLights\FixtureLibrary\objects\<sha256>
%LOCALAPPDATA%\EmberLights\FixtureLibrary\reports\<sha256>.json
%LOCALAPPDATA%\EmberLights\FixtureLibrary\quarantine\
```

For an explicitly selected source file:

- read with a size bound;
- hash before parsing;
- preserve original bytes only with operator intent;
- never modify the source;
- record source kind and license status;
- default unknown license to `UnknownDoNotRedistribute`;
- never commit private/unknown-license files into the repository or installer.

## 5.2.5 SoundSwitch fixture export probe

Add a safe read-only profile probe/import entry point. The supported operator path is:

1. use SoundSwitch Fixture Manager;
2. add the exact Production/Public/Local fixture to its Workspace;
3. compare against the exact manufacturer manual;
4. use `File > Save Fixture as…`;
5. explicitly select the resulting file in EmberLights.

The first implementation may probe and preserve an unknown exported format before it can semantically parse it. It must report container/magic/strings/version/entries safely and quarantine unknown forms. Do not scrape SoundSwitch's cloud, ask for inMusic credentials, or bypass profiles restricted by third-party licensing.

## 5.2.6 One-fixture bench project

Create either a small wizard or a packaged bench project generated from one exact verified profile.

Selection preference:

1. an exact fixture whose complete manual and selected mode are known;
2. a profile exported through SoundSwitch Fixture Manager and manual-matched;
3. a lawful vendor/QXF/OFL profile and manual-matched;
4. controlled raw discovery only as a last resort.

The project contains:

- one fixture;
- universe 1;
- operator-confirmed start address;
- exact mode/channel count;
- Micro universe 1;
- Blackout;
- only safe visible Red/Green/Blue/White/Intensity looks supported by the fixture;
- no OS2L or Autoloop requirement;
- unrelated outputs disabled by default.

## 5.2.7 Raw-to-semantic comparison

Provide an automated comparison action:

```text
successful raw frame
vs.
last Runner output frame
```

Report:

- equality result;
- differing channels;
- raw/semantic values;
- fixture/profile ownership;
- defaults or safety/layer causes;
- pre/post-blackout distinction.

If equality is expected, any unexplained difference fails the bench gate.

## 5.2.8 Slice 2 tests

```text
migrated_project_emits_unverified_warnings
fixture_source_is_content_addressed
fixture_probe_rejects_path_traversal
fixture_probe_rejects_oversize_and_bomb_inputs
unknown_license_is_not_redistributable
one_fixture_project_has_no_unrelated_outputs
raw_semantic_frame_equality
frame_snapshot_does_not_format_on_scheduler
frame_inspector_resolves_patch_ownership
```

## 5.2.9 Slice 2 release gate

- One raw test visibly controls the fixture.
- One exact semantic Look emits the approved frame.
- Blackout works through Hardware Test, Runner, and application shutdown.
- The 71-fixture staging project no longer reports zero physical warnings.

---

# Slice 3 — Connections: visible, persistent, and truthful

This is core operational work, not broad UI polish.

## 5.3.1 Immediate accessibility fix

Without waiting for the future skin runtime:

- make **Save & Apply Connections** visible without scrolling or tabbing into hidden geometry;
- add a fixed/sticky action region or a top-level visible command;
- provide vertical scrolling for all fields;
- prevent overlap at supported window sizes and DPI;
- keep a default/Enter action where safe;
- provide an explicit accelerator, such as Alt+A;
- ensure Blackout remains available while editing connections.

A hidden button still present in the tab order is a failed test.

## 5.3.2 Desired/saved/applied/active state

Introduce or begin extracting a `ConnectionCoordinator` with separate immutable snapshots:

```text
desiredSettings
savedSettings
activeSettings
endpointStatuses
lastApplyResult
```

Required result values:

```text
ValidationRejected
SaveFailed
SavedNoRuntimeChange
SavedAndApplied
SavedRestartRequired
SavedApplyFailed
```

Required transaction:

```text
read controls
-> parse/validate
-> atomically save desired project settings
-> calculate endpoint diff
-> blackout/close only affected old outputs
-> apply only affected services/adapters where supported
-> wait for bounded outcome
-> publish saved and active states separately
```

A saved setting that failed activation must remain visibly saved but not active. Do not roll the UI back silently or label the failed endpoint Ready.

## 5.3.3 Incremental extraction

Target boundaries:

```text
ConnectionCoordinator
DjTransportService
ControllerService
OutputRouter
RunnerService
```

For this build:

- Micro Hardware Test and Runner must already share the session;
- connections may still restart Runner for changes that cannot yet hot-apply safely;
- the result must explicitly say `SavedRestartRequired` rather than hiding that behavior;
- do not undertake a full application rewrite before the physical gate.

## 5.3.4 Geometry and persistence tests

Test:

```text
1366x768 @ 100/125/150/200%
1920x1080 @ 100/150%
minimum supported client size
```

Assert:

- action visible and inside client bounds;
- no control intersection;
- visible scroll path to every field;
- tab order matches visible order;
- accelerator and click invoke the same command;
- settings survive application restart;
- active Micro universe/framing in diagnostics equals the applied project setting.

---

# Slice 4 — VirtualDJ/OS2L deterministic startup and truthful state

## 5.4.1 Do not misdiagnose client laziness as a server failure

VirtualDJ is the TCP client. EmberLights cannot force a direct-IP client to connect before VirtualDJ chooses to initiate OS2L traffic.

Support two deterministic paths:

### Direct-IP path

Keep:

```text
os2l=Yes
os2lDirectIp=127.0.0.1:9996
```

Provide a one-click copyable VirtualDJ ONINIT action:

```text
wait 100ms & os2l_button 'blackout' off
```

This safely exercises the OS2L path without changing a performance state.

### Discovery path

After the listener is reliable independently of Runner, advertise the service through native Windows DNS-SD/mDNS as:

```text
_os2l._tcp
```

Then test VirtualDJ `os2l=auto`. Keep discovery behind explicit status/diagnostics until verified on the owned machine.

## 5.4.2 Extract listener lifetime

Create `DjTransportService` or a bounded `Os2lService` owned independently from show scheduling.

Desired states:

```text
Disabled
Starting
Listening
WaitingForClient
ClientConnectedNoClock
ReceivingClock
HeldFallbackClock
Fault
Stopping
```

The service should start when a loaded project's active connection settings enable OS2L, not only after Start Show.

It publishes normalized bounded events/snapshots to Runner. Socket operations, decoding, discovery, and feedback remain off the scheduler thread.

## 5.4.3 Duplex feedback

Extend the server with a bounded nonblocking send path for authoritative state:

- blackout;
- work light;
- active Static Look;
- later stable Autoloop pad/page state.

Feedback follows Runner state, not optimistic input. If a command is rejected or superseded, the next snapshot corrects the client state.

## 5.4.4 Launch-order tests

1. EmberLights listening, then VirtualDJ launches.
2. VirtualDJ launches, then EmberLights enables OS2L.
3. Runner starts before VirtualDJ.
4. VirtualDJ restarts while Runner remains active.
5. OS2L off/on through Save & Apply.
6. Client drops and reconnects.
7. Port occupied.
8. Direct-IP ONINIT path.
9. DNS-SD `auto` path when implemented.

## 5.4.5 Gate

At minimum, the documented ONINIT path must remove the need to press a DMX performance pad manually. DNS-SD auto-discovery is highly desirable but may not block the Micro/fixture proof if direct IP is deterministic and clearly documented.

---

# Slice 5 — Static Look Toggle/Hold arbiter and feedback

## 5.5.1 Domain contract

Do not implement Toggle/Hold only in a button callback. Add shared typed commands/state usable by UI, MIDI, Control One, keyboard, and OS2L.

Recommended command concepts:

```text
staticLook.press(lookId, interactionMode, sourceId, activationToken)
staticLook.release(lookId, sourceId, activationToken)
staticLook.clear(sourceOrAll)
```

Recommended state:

```text
activeLookId
activeLookName
winningActivationToken
winningSourceId
interactionMode
transitioning
transitionProgress
activeClaims[]   bounded diagnostic view
```

## 5.5.2 Arbiter

Implement a bounded `StaticLookArbiter` above the existing `StaticLookPlayer`.

Each live claim contains:

```text
activationToken
sourceId
lookId
mode = Toggle | Hold
activationOrdinal
```

Rules:

### Toggle

- press inactive Look → create/activate a latched claim;
- release → no action;
- press the same active Toggle claim → remove it;
- press another Look → new claim wins and replaces/fades from the current Look.

### Hold

- press → add a momentary claim;
- release → remove only the exact token/source claim;
- a stale release never clears a newer activation;
- when the winning claim releases, the next-most-recent still-live claim may resume deterministically; otherwise clear EventMoment.

### Priority

- Static Look remains on `EventMoment` above Autonomous, TrackScript, and ManualAutoloop for assigned properties.
- Lower Autoloops keep advancing while hidden.
- Releasing/clearing the winning Static Look reveals the current lower-layer phase.
- ManualOverride, Emergency/Work Light, Blackout, and Safety retain their existing higher authority.

### Project activation

On project/package generation change:

- invalidate claims referencing missing/reordered content;
- never let an old release token affect a new generation;
- publish cleared/corrected feedback.

## 5.5.3 Feedback

UI pressed state, MIDI/Control One LEDs, and OS2L buttons follow authoritative arbiter state.

Required cases:

- Toggle indicates on until toggled off or replaced.
- Hold indicates on only while its claim wins or according to documented multi-claim feedback.
- Rejected/stale release produces no false off state.
- Blackout does not destroy the Static Look claim unless explicitly designed; when blackout clears, the authoritative layer state is restored.

## 5.5.4 Tests

```text
static_look_toggle_on_off
static_look_hold_press_release
static_look_stale_release_ignored
static_look_newer_claim_wins
static_look_previous_live_claim_resumes
static_look_project_generation_invalidates_old_token
static_look_autoloop_phase_advances_under_override
static_look_release_reveals_advanced_autoloop
static_look_blackout_and_worklight_priority
static_look_same_behavior_direct_midi_os2l
static_look_feedback_is_authoritative
```

---

# Slice 6 — Timing, reconnect, packaging, and morning evidence

## 5.6.1 Timing decomposition

Measure separately:

- scheduler wake jitter;
- render duration;
- queue delay;
- adapter write start/finish cadence;
- UI/diagnostics impact.

Hardware Test provides a fixed 40 Hz adapter cadence independent of Runner. This tells us whether preview.314's scheduler jitter matters to physical output.

After physical proof:

- use an appropriate high-resolution waitable timer and/or MMCSS characteristics;
- keep formatting, logging, file I/O, USB, and network I/O off the scheduler;
- preserve newest-frame-wins output;
- target initial scheduler p99 under 2 ms at 40 Hz unless measured evidence supports a different budget;
- exercise resize, diagnostics refresh, project save, OS2L, MIDI, and reconnect under load.

## 5.6.2 Reliability matrix

Test:

- ten init/stream/blackout/close cycles;
- unplug/replug during blackout;
- unplug/replug during active raw frame;
- application close while active;
- adapter ownership conflict;
- transfer timeout;
- malformed/unsupported device descriptors;
- connection setting change while Runner is active;
- VirtualDJ restart;
- one-fixture ten-minute stream;
- no stale frame after recovery.

## 5.6.3 Installer contents

The next installer must contain:

```text
EmberLights.exe
EmberLights Hardware Test.exe
existing qualification/migration tools
fixture source probe, when implemented
one-fixture bench template or generator
MORNING_HARDWARE_TEST.md
release manifest
SHA-256 checksums
```

The installer workflow must:

- build from one exact source commit;
- run unit/core tests;
- install silently into a clean path;
- launch `EmberLights.exe --startup-smoke`;
- run non-outputting Hardware Test self-test;
- confirm all required tools/templates/docs exist;
- generate manifest and hashes only after smoke passes;
- upload the Windows artifact;
- never perform active USB output in CI.

## 5.6.4 Release notes

State separately:

```text
Software verified
Installed-app smoke verified
USB session protocol tests verified
Physical Micro response pending/confirmed by Joshua
Fixture profile pending/confirmed
Reconnect/soak pending/confirmed
```

Do not call the Micro supported or production-ready before the owned-hardware gates pass.

---

## 6. File ownership and parallel work lanes

One integration owner controls conflict-prone files. Subagents may work in isolated lanes, but they must not independently redesign shared contracts.

### Lane A — Micro transport/session and Hardware Test

Primary files:

```text
native-core/include/showcore/soundswitch_micro.hpp
native-core/src/soundswitch_micro.cpp
new transport/session headers and sources
new hardware test source
native-core/tests/* Micro session tests
```

May define the shared session contract. Must not edit broad UI, fixture semantics, or OS2L.

### Lane B — frame truth and fixture evidence

Primary files:

```text
fixture/profile/provenance headers and sources
fixture source probe
project validation
SoundSwitch migration warnings
frame snapshot/inspector support
fixture-specific tests and synthetic corpus
```

Must not change Micro packet/lifecycle code.

### Lane C — typed interaction and transport services

Primary files:

```text
Runner command/state contract
StaticLookArbiter
OS2L/DjTransportService
OS2L feedback/discovery
unit/integration tests
```

Must not directly lay out the Windows Connections page.

### Lane D — Windows shell, Connections, installer, CI

Owned by integration lead only:

```text
native-core/src/windows_app.cpp
native-core/CMakeLists.txt
native-core/Makefile
installer/EmberLights.iss
.github/workflows/native-core.yml
docs/MORNING_HARDWARE_TEST.md or equivalent
```

This lane integrates completed contracts from A/B/C and resolves concurrent UI-agent changes intentionally.

### Merge order

1. Test seams and shared Micro session contract.
2. Hardware Test and session lifecycle.
3. Frame snapshot, migration warnings, fixture provenance/probe.
4. Static Look arbiter and OS2L service contracts.
5. Connections shell integration.
6. Installer/workflow/morning docs.
7. Full rebase on latest `main`, conflict review, complete tests, artifact.

No subagent may overwrite another lane's public contract without updating the governing ADR/spec and coordinating with the integration lead.

---

## 7. Stop conditions and anti-drift rules

Stop and package the current slice rather than drifting when any of these occur:

- Slice 1 is software-complete and requires Joshua's physical result.
- A source/profile file format cannot be parsed without a representative user export.
- A broad schema rewrite would delay raw output proof.
- UI skin/runtime work begins touching the same shell files.
- a protocol change lacks controlled packet evidence;
- a test depends on an unavailable fixture manual or hazardous function.

Explicit prohibitions:

- no speculative A/B/C Micro framing selector;
- no claim based solely on accepted WinUSB writes;
- no cloud scraping or credential capture for fixture profiles;
- no redistribution of unknown-license fixture files;
- no full UI redesign in this branch;
- no USB/network/file I/O on the scheduler path;
- no destructive modification of the original SoundSwitch export or fixture source;
- no hard-coding the product around the current 71-fixture rig;
- no treating Control One MIDI as proof of its onboard DMX outputs;
- no eight-hour soak before a ten-minute physical slice passes.

---

## 8. Morning operator test embedded in the build

The installed docs/tooling should guide Joshua through this exact sequence.

### Preparation

1. Close SoundSwitch and any program that can own the Micro.
2. Plug the Micro directly into the Windows PC.
3. Connect the known-good transmitter or, preferably for isolation, one fixture directly by DMX cable when practical.
4. Use exactly one known fixture.
5. Confirm and record its displayed DMX mode/channel count and start address.
6. Avoid fog, haze, spark, laser emission, and high-rate strobe functions.

### Raw hardware test

1. Open **EmberLights Hardware Test**.
2. Click **Inspect Device**.
3. Export the initial report.
4. Click **Initialize + Warm Up**.
5. Record whether the Micro indicator changes.
6. Start **Blackout Stream**.
7. Record whether the transmitter/fixture indicates valid DMX.
8. Use **Single Channel** or a bounded footprint chase.
9. Record the first channel/value producing a visible safe response.
10. Confirm automatic return to blackout.
11. Run **Reconnect Test** using the successful raw frame.
12. Unplug/replug once and repeat.
13. Export the final report.

### Semantic fixture test

1. Open the one-fixture bench project.
2. Confirm the profile, universe, address, and mode shown by EmberLights match the physical fixture.
3. Trigger the equivalent safe Static Look.
4. Export the frame comparison.
5. Confirm the semantic frame matches the successful raw frame or explain every difference.
6. Confirm application Blackout and close-time blackout.

### Connections and OS2L

1. Open Connections at normal laptop size.
2. Confirm Save & Apply is visibly accessible without tabbing to hidden controls.
3. Save/apply a harmless setting and restart EmberLights.
4. Confirm desired/saved/active state matches.
5. Add or use the provided VirtualDJ ONINIT action.
6. Launch in both orders and confirm BPM/beat without pressing a DMX pad.

### Static Look interaction

1. Run an Autoloop.
2. Activate a Static Look in Toggle mode.
3. Confirm it visibly overrides assigned properties and shows active feedback.
4. Toggle it off and confirm the Autoloop returns at its advanced phase.
5. Activate a Static Look in Hold mode.
6. Release and confirm the same return behavior.
7. Test a stale-release scenario by activating another Look before releasing the first.

---

## 9. Morning result decision table

| Observation | Meaning | Next action |
| --- | --- | --- |
| Device not found | Driver/path/device presence | inspect driver binding, GUID, VID/PID, ownership |
| Open succeeds, init fails | transport/session stage | fix exact reported WinUSB stage |
| Init succeeds, no indicator change | init may not be consumed | compare configuration/alt/reset/timing; diagnostic reference backend |
| Indicator changes, no DMX indication | DMX streaming/electrical boundary | confirm warm-up/continuous frames; direct-cable test; capture official app |
| Raw channel works | Micro transport is proven | freeze protocol and qualify profile/patch |
| Raw works, semantic differs | compiler/profile/layer defect | use frame diff and exact manual |
| Raw and semantic match | fixture/profile path substantially proven | reconnect/soak and broader rig qualification |
| Direct-IP still waits | expected VirtualDJ client activation issue | verify ONINIT command; test DNS-SD auto path |
| Save & Apply hidden | blocking shell regression | fail build; fix geometry before release |
| Static Look stops Autoloop phase | core layer/interaction regression | fail Static Look gate |

---

## 10. Core-ready definition

The core may be handed back to broad UI/UX implementation only when:

- raw Micro output works repeatedly on Joshua's owned device;
- automatic and emergency blackout work;
- reconnect does not replay stale frames;
- one exact fixture/profile/mode/address works through Runner;
- semantic output matches an approved raw frame;
- migrated/unverified profiles and patch data are labeled honestly;
- Connections desired/saved/applied/active state is visible and persistent;
- OS2L starts deterministically through a documented/tested path and survives restart;
- Static Look Toggle and Hold work across the shared command/state contract;
- underlying Autoloops continue advancing under Static Looks;
- a ten-minute hardware stream passes;
- installer, manifest, hashes, smoke tests, and reports all name the same source commit.

This definition does not claim full SoundSwitch parity or full Control One support. It establishes the trustworthy core on which those iterations can proceed.

---

## 11. Next core iterations after this build

After the morning gates are resolved, continue in this order:

1. Fix any observed native Micro lifecycle defect and repeat the raw gate.
2. Qualify the first exact fixture and profile.
3. Expand fixture import/profile schema using the proven provenance model.
4. Qualify additional owned fixture types and then the full rig patch.
5. Stabilize Control One as a bundled MIDI/control-surface profile with authoritative feedback.
6. Begin a separate clean-room qualification program for Control One onboard DMX outputs.
7. Complete timing and longer soak qualification.
8. Resume broad default UI and SoundSwitch Reference skin implementation over the accepted shared commands/state.

---

## 12. Completion statement

This document completes the requested technical planning handoff. The scheduled implementation agent is authorized to begin the core recovery build now, starting with the shared SoundSwitch Micro session and installed raw Hardware Test. The first obligation is a decisive, safe physical-output test—not another layer of optimistic readiness reporting.