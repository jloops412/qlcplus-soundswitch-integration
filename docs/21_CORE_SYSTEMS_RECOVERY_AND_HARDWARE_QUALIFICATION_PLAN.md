# EmberLights Core Systems Recovery and Hardware Qualification Plan

Status: **Binding immediate implementation program**  
Date: 2026-08-11  
Primary objective: prove and harden the complete path from operator/DJ input to physical DMX before additional skin or Studio expansion.  
Implementation baseline: preview.314 / source commit `4f6111493377d2b366836c3579ad5514042d177f`, merged to `main` as `01735d18834cb8d2c2ec5bcbecd5ac3420a76860`.

## 1. Immediate directive

The next build program is not a general UI iteration. It is a bounded core-recovery and hardware-qualification program.

The build agent must work in this order:

1. Prove raw SoundSwitch Micro output independently of projects, fixtures, Looks, Autoloops, OS2L, and the main UI.
2. Harden the reusable SoundSwitch Micro session/adapter until raw output is repeatable across restart and reconnect.
3. Prove the exact rendered channel values and then prove one real fixture/profile/address through the normal compiler and Runner.
4. Make Connections settings visible, persistent, truthful, and safely applicable.
5. Make VirtualDJ/OS2L connection deterministic at startup and report the actual listener/client state.
6. Complete Static Look Toggle/Hold behavior and feedback without pausing the underlying Autoloop.
7. Extract the connection/output contracts needed for future adapters and Control One without destabilizing the working slice.
8. Only then resume broad UI/skin work.

No amount of accepted USB writes, passing project validation, moving BPM, or polished UI counts as physical-output qualification.

## 2. User-observed baseline

Joshua's preview.314 test produced the following important facts:

- The project validates with 0 errors and 0 warnings.
- Runner is Running and advancing beat position at approximately 155 BPM.
- Autoloop B1/S1 is active and progressing.
- OS2L has one client connection, receives messages, and reports zero decode errors.
- SoundSwitch Micro is open on project universe 1 using native JLS1 framing.
- WinUSB accepted 1,276 Micro frame writes with zero reported write failures and Windows error 0.
- The last routed universe contained 78 nonzero slots.
- The physical fixture did not respond.
- VirtualDJ did not initiate OS2L direct-IP traffic until an OS2L/DMX pad was invoked, despite `os2l=Yes`.
- The Connections **Save & Apply** action was visually buried/overlapped and reachable only through keyboard tabbing.
- Scheduler jitter p99 and maximum were approximately 31.9 ms, with 266 deadline misses over approximately 32 seconds.

These facts narrow the problem, but they do not identify one root cause. They prove that the high-level show loop, frame queue, selected universe, and host-side WinUSB calls are active. They do not prove the USB device was initialized into its DMX streaming state, that a physical DMX signal was emitted, or that the staged fixture profiles/addresses correspond to the actual rig.

## 3. Critical findings from the current implementation

### 3.1 The JLS1 frame bytes are probably not the first thing to change

The current implementation produces the independently observed JLS1 packet shape:

```text
73 54 52 74                 "sTRt"
01 00                       command 0x0001 / DMX
02 02                       payload length 0x0202 / 514
00 00                       two protocol payload bytes
<512 DMX slots>             channel 1 begins at packet offset 10
```

It also sends these two 12-byte initialization packets:

```text
73 54 52 74 02 00 04 00 00 00 01 00
73 54 52 74 02 00 04 00 01 00 FF FF
```

Those bytes and offsets match the independently published exact replay/driver evidence in `asoronow/laser-controller`. Do not invent another framing candidate without new controlled evidence.

### 3.2 The Windows Micro lifecycle is incomplete compared with the independent working replay

The current `SoundSwitchMicroSender::open()` sends both initialization packets and immediately reports success. It does not currently provide:

- an explicit post-initialization settling delay;
- a controlled blackout warm-up stream;
- a visible initialization-stage state machine;
- a logged active configuration value or current alternate setting;
- an explicit alternate-setting selection;
- a pipe reset before initialization;
- a repeatable standalone physical output test;
- a report distinguishing initialization, warm-up, streaming, and physical qualification.

The independent replay waits after initialization and sends a sequence of blackout frames before active DMX. This is the highest-value transport hypothesis to test without changing the known packet bytes.

### 3.3 `Micro: Open` and accepted writes are not physical readiness

The current status becomes Ready/Open when `CreateFile`, `WinUsb_Initialize`, descriptor/pipe checks, and initialization writes succeed. A successful `WinUsb_WritePipe` means Windows accepted the transfer. It does not prove:

- that the device entered its DMX streaming state;
- that the USB firmware consumed the command as intended;
- that the DMX transceiver is driving the line;
- that the wireless transmitter sees valid DMX;
- that a fixture's address/mode/profile matches the emitted slots.

The UI and diagnostics must stop presenting one ambiguous Ready state for all of those stages.

### 3.4 The converted 71-fixture project is deliberately staged, not decoded

The active project is named `PATCH REVIEW REQUIRED` for a reason. The SoundSwitch V1 converter deliberately creates a safe, non-overlapping staging layout rather than claiming to decode the actual SoundSwitch patch:

- uplights at U1 addresses 1, 11, 21, and 31;
- four 48-channel tube blocks beginning at 41;
- Wash FX HEX at 233;
- IR-4 fixtures at 244 and 254.

Its fixture profiles are semantic provisional profiles with linear channels and zero defaults. They are not physical qualification of the fixture modes, channel ranges, shutter/program channels, or real addresses. A project can therefore have zero schema warnings while still being wrong for the physical fixture.

This is a likely explanation for a dark fixture and must be isolated from the USB transport before either side is changed.

### 3.5 The Connections visibility defect is deterministic

The current Connections page lays out many form rows down the page and separately anchors Refresh and Save & Apply near the bottom of the client area. At common laptop heights and/or Windows DPI scaling, the later form controls and action bar intersect. This is why the action can be hidden while remaining in the tab order.

This is not merely a styling complaint. It prevents the operator from knowing whether settings were saved or activated and is therefore a core operational defect.

### 3.6 The Static Look layer priority is already conceptually correct

The current layer order places `EventMoment` above Autonomous, TrackScript, and ManualAutoloop. The scheduler continues ticking lower Autoloops while a Static Look occupies the higher EventMoment layer. Clearing the Static Look therefore reveals the current lower-layer state rather than restarting the Autoloop.

The missing work is the interaction and ownership contract:

- Toggle versus Hold;
- press/release behavior;
- authoritative active state and feedback;
- avoiding an old release event clearing a newer Static Look;
- consistent behavior across UI, MIDI, Control One, keyboard, and OS2L.

Do not reimplement this as a separate UI-only scene engine.

### 3.7 The VirtualDJ first-command behavior is primarily a client activation issue

The EmberLights listener cannot force VirtualDJ, which is the TCP client, to
initiate a direct-IP connection. EmberLights now advertises `_os2l._tcp`
through Windows DNS-SD, so the primary setup is `os2l=Auto` with
`os2lDirectIp` blank. Where discovery is unavailable, use `os2l=Yes`, the
matching direct endpoint, and this reserved fallback action:

```text
wait 100ms & os2l_button 'EmberLights Keepalive' off
```

`EmberLights Keepalive` is ignored before action routing, so it exercises the
client path without clearing an intentional blackout or changing a Look.
Installed Windows/VirtualDJ discovery, fallback, launch-order, and reconnect
evidence remain required; the listener still needs to move outside Start Show.

## 4. Ranked root-cause hypotheses for no physical light

Investigate in this order. A lower item must not be used to distract from an untested higher item.

| Rank | Hypothesis | Why it remains plausible | Disproof/confirmation method |
| --- | --- | --- | --- |
| 1 | The staged fixture address, DMX mode, or provisional profile does not match the physical fixture | The converter explicitly does not decode the real patch and hard-codes semantic linear profiles | Raw universe/channel test independent of profiles, followed by one exact fixture bench project |
| 2 | The Micro requires initialization settling and/or initial blackout frames before active streaming | Current bytes match independent evidence, but current Windows lifecycle omits the delay/warm-up used by the working replay | Standalone exact-session probe with init delay and controlled warm-up |
| 3 | Windows interface/configuration/alternate-setting state differs from the independent libusb path | Current code verifies endpoint but does not log configuration/alternate setting or explicitly select alt 0 | Descriptor/config report, current-alt query, explicit alt 0, pipe reset, repeated open test |
| 4 | The selected universe/frame routing is not the physical frame Joshua expects | Current count proves nonzero slots, not which slots or values | Frame inspector with a bounded nonzero channel/value list and frame hash |
| 5 | The fixture requires nonzero shutter/program/default channels that provisional profiles leave at zero | Several fixtures use ranges/macros rather than simple linear RGB channels | Exact profile/manual or channel discovery; raw channel chase with safe bounds |
| 6 | DMX electrical/transmitter state is not seeing a valid stream | Joshua has confirmed the equipment works, but the software path still needs physical-layer evidence | Transmitter DMX indicator plus a direct-wired test when practical |
| 7 | Scheduler jitter is preventing all response | Timing quality is poor, but hundreds of writes and continuous frames make total darkness from jitter alone unlikely | Raw fixed-cadence output test independent of the main scheduler |

## 5. Required diagnostic decomposition

Every test and diagnostic must identify the furthest verified boundary:

```text
Operator/DJ event
  -> typed command accepted
  -> show state/layer changed
  -> rendered universe contains expected slots
  -> OutputRouter selected expected universe
  -> adapter initialized
  -> adapter accepted expected packet bytes
  -> physical DMX/transmitter observed
  -> fixture interpreted address/mode/channel values
```

Do not collapse those boundaries into one `Ready` badge.

## 6. Work Package A — standalone raw hardware proof (P0, first)

### 6.1 Deliverable

Create an installed, one-click **EmberLights Hardware Test** tool, either by safely expanding `soundswitch_micro_probe.exe` or by adding `emberlights_hardware_probe.exe`. It must use the same production Micro session implementation as Runner; no second divergent driver is allowed.

The tool must operate without opening or compiling an `.emberlights` project.

### 6.2 Required test modes

1. **Descriptor/session report**
   - target VID/PID and device path;
   - configuration descriptor value;
   - interface and current alternate setting;
   - endpoint address, type, maximum packet size, and relevant pipe policies;
   - every initialization stage and Windows error;
   - application version and commit.

2. **Initialize only**
   - open device;
   - perform the complete deterministic initialization sequence;
   - report whether the adapter indicator changed, without claiming physical DMX.

3. **Blackout stream**
   - send all-zero 522-byte frames at the configured refresh rate;
   - use this as warm-up and as the safe state before/after every active test.

4. **Single channel**
   - operator supplies channel 1–512 and value 0–255;
   - bounded hold duration, preferably 5 seconds by default and no more than 30 seconds without another explicit action;
   - automatically returns to blackout.

5. **Address-range chase**
   - operator supplies first channel and bounded footprint;
   - illuminate/test one channel at a time with clear current-channel display;
   - never run hazard-specific assumptions automatically.

6. **Full raw frame**
   - accepts an explicitly displayed list of channel/value pairs;
   - shows packet prefix, frame hash, nonzero slot count, and exact nonzero slots before sending.

7. **Reconnect test**
   - close cleanly with blackout;
   - reopen through the full initialization sequence;
   - repeat the same raw frame.

8. **Export report**
   - text or JSON report with no private source payloads;
   - includes stages, timings, errors, frame metadata, and operator observations.

### 6.3 Safety rules

- Blackout before initialization tests, between patterns, on timeout, on normal close, and on failure where the handle remains usable.
- Do not automatically test fog, haze, spark, laser, or high-rate strobe semantics.
- Raw values must be explicit and visible to the operator.
- No background process may retain the adapter after the tool exits.
- Refuse active output when SoundSwitch or another process owns the device.
- The test tool must have one unmistakable **Blackout Now** action.

### 6.4 Acceptance gate A

The transport is not advanced beyond this work package until Joshua can report, from the installed build:

- whether initialization changes the Micro indicator;
- whether the transmitter indicates valid DMX;
- whether a known fixture responds to at least one raw channel/value combination;
- whether automatic blackout works;
- whether the same test succeeds after close/reopen and unplug/replug.

If raw output does not work, do not edit fixture profiles or Autoloops as a substitute.

## 7. Work Package B — production SoundSwitch Micro session hardening (P0)

### 7.1 Shared adapter/session contract

Extract a reusable Micro session that both Hardware Test and Runner call. The scheduler must never own USB handles or block on USB operations.

Recommended adapter lifecycle:

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

Recommended session operations:

```text
enumerate()
open(config)
initialize()
send_frame(universe)
send_blackout(repetitions)
close()
status_snapshot()
```

### 7.2 Deterministic open sequence

Implement and record the following sequence:

1. Enumerate the exact interface GUID and verify VID/PID.
2. Open with `FILE_FLAG_OVERLAPPED` as required by WinUSB.
3. Call `WinUsb_Initialize`.
4. Query and record the device/configuration/interface/pipe descriptors.
5. Verify the active configuration describes configuration value 1; fail clearly rather than guessing if it does not.
6. Query and explicitly select alternate setting 0 when appropriate.
7. Reset the bulk OUT pipe before initialization.
8. Keep RAW_IO disabled; let WinUSB split the 522-byte transfer for the 64-byte endpoint.
9. Keep short-packet termination disabled unless new controlled evidence proves it is required.
10. Set a bounded transfer timeout suitable for recovery; report timeout separately.
11. Send the two exact 12-byte initialization packets in order.
12. Wait a controlled 200 ms default settling interval off the scheduler thread.
13. Send a controlled warm-up of 50 blackout frames at 40 Hz by default.
14. Enter Streaming and begin newest-frame-wins output.

The delay and warm-up counts should be internal/session configuration with conservative defaults, not user-facing framing guesses.

### 7.3 Reconnect and shutdown

- Any device/write fault closes the session and enters Recovering.
- Every reconnect repeats the entire open/init/settle/warm-up sequence.
- Reconnect must consume only the newest available universe frame after warm-up.
- Do not replay stale queued looks after recovery.
- Shutdown sends a bounded blackout train before releasing the handle.
- A failed blackout write is reported but must not deadlock shutdown.

### 7.4 Telemetry

Publish at minimum:

- lifecycle state;
- device present/open;
- initialization attempts/successes/failures;
- current stage and last stage error;
- warm-up progress/completion;
- frames attempted/accepted/failed;
- last frame hash;
- bounded list of nonzero slot/value pairs;
- selected project universe;
- reconnect count;
- last successful write age;
- physical qualification state: `untested`, `operator-confirmed`, or `failed` as separately recorded evidence.

Never infer `operator-confirmed` from host-side writes.

### 7.5 Automated tests

- golden init packet bytes;
- golden blackout and nonzero frame bytes;
- channel 1 maps to packet offset 10 and channel 512 to offset 521;
- state-transition tests for every open/init/warm-up/write failure;
- reconnect replays initialization once and discards stale frames;
- shutdown blackout behavior;
- fake transport records exact ordering and timing boundaries;
- installed Hardware Test self-test remains non-outputting by default.

### 7.6 Acceptance gate B

- Ten consecutive initialize/stream/blackout/close cycles on Joshua's Micro.
- Unplug/replug recovery without restarting EmberLights.
- Ten-minute 40 Hz stream with no stale replay and no unbounded queue growth.
- Physical fixture response and blackout confirmed in the standalone tool.

## 8. Work Package C — frame truth and fixture/patch qualification (P0)

### 8.1 Add a frame inspector

Diagnostics must expose the actual last rendered/output frame, not merely the nonzero count.

For each bounded displayed nonzero slot, report:

```text
Universe
DMX channel
Raw value
Patched fixture ID/name, when one owns the slot
Profile/channel mapping
Semantic property or constant/default
Winning layer/source
```

Also report:

- complete frame hash;
- nonzero slot count;
- first/last nonzero channel;
- output adapter and selected universe;
- whether the displayed frame is pre-blackout or post-blackout.

The inspector must be snapshot-based and must not allocate or format strings on the real-time scheduling path.

### 8.2 Make provisional patch status truthful

The current SoundSwitch color-rig conversion must emit visible validation warnings, not zero warnings, until the operator verifies the physical data. Add durable qualification metadata or equivalent evidence for:

- fixture address verified/unverified;
- fixture DMX mode verified/unverified;
- profile/channel ranges verified/unverified;
- output universe verified/unverified.

At minimum, projects produced by the conservative converter must report a warning similar to:

```text
MIGRATED_PATCH_UNVERIFIED: This is a non-overlapping staging patch, not decoded SoundSwitch addressing. Verify every physical address, mode, and profile before relying on output.
```

A warning should not block the raw probe, but it must prevent the UI from implying physical readiness.

### 8.3 Build a one-fixture bench project

Create a minimal project/wizard path containing exactly:

- one selected real fixture;
- one verified profile/mode;
- one verified universe/address;
- Blackout;
- Red, Green, Blue, White/Intensity as supported by that fixture;
- no Autoloop dependency;
- Micro universe 1 enabled;
- all unrelated network outputs disabled by default.

The fixture profile must come from an exact manual, exact QXF/OFL source under the existing quarantine rules, or controlled channel discovery. Do not use the current semantic profile as proof merely because its footprint matches.

### 8.4 Raw-to-semantic comparison

For a known test state:

1. Send raw channel/value pairs through Hardware Test and save the frame report.
2. Trigger the corresponding Static Look or manual property through Runner.
3. Compare the emitted universe byte-for-byte.
4. Explain every difference.

Preview 95 implements the software comparison boundary. The production
renderer attaches fixed per-slot attribution, the normal output thread
publishes the exact pre-global-blackout and routed frames plus per-backend
attempt/accept results, and `RunnerFrameInspector` hashes and compares full
512-byte references with bounded cause-labelled differences.

Preview 96 implements the software evidence join. A read-only parity service
verifies embedded completed Raw Hardware Test attempts, selects the best exact
universe/backend/frame match deterministically, and reports the authored
reference, actual routed frame, prior raw requirement/observation, and current
Micro route acceptance as independent facts. A profile-only change can retain
explicitly historical evidence only while fixture/profile identity,
manufacturer/model/mode, universe, and address remain stable; a repatch or
malformed audit fails closed. It always denies current optical/fixture-side
observation. Acceptance remains incomplete until the installed Windows run
captures a fresh raw attempt, reopened Runner snapshot, and physical response.

This isolates compiler/profile defects from adapter defects.

### 8.5 Acceptance gate C

- Raw test activates the fixture.
- The one-fixture semantic project produces the same expected raw channels.
- Red/blue/white or the fixture's supported equivalents work.
- Blackout works from Hardware Test, Runner, and application shutdown.
- The staged 71-fixture project no longer reports itself as fully verified.

## 9. Work Package D — Connections and output lifecycle (P0)

### 9.1 Immediate layout repair

Without waiting for the future skin system:

- place Refresh and **Save & Apply Connections** in a sticky action bar that is always visible;
- use a responsive two-column layout when width permits;
- provide a vertical scrolling viewport at narrow height/high DPI;
- prevent controls from overlapping at every supported size;
- retain Enter/default-button and Alt+A access;
- make tab order follow the visible reading order;
- display per-section headings for DJ input, network DMX, USB DMX, controller/MIDI, and timing.

### 9.2 Geometry tests

Add Windows UI smoke checks at minimum for:

- 1366×768 at 100%, 125%, 150%, and 200% scaling;
- 1920×1080 at 100% and 150%;
- minimum supported window size.

Assert that:

- Save & Apply is visible, enabled, and within the page client rectangle;
- it does not intersect any field or message control;
- Refresh is visible;
- keyboard activation reaches the same command;
- every editable field remains reachable through visible scrolling rather than hidden coordinates.

### 9.3 Truthful Save & Apply transaction

Implement one command path:

```text
Read controls
  -> parse and validate
  -> atomically save desired project settings
  -> compute connection diff
  -> zero/close only affected old outputs
  -> apply/restart only affected services/adapters where architecture permits
  -> wait for bounded outcome
  -> publish Saved / Applied / Active result per endpoint
```

Required outcomes:

```text
SavedAndApplied
SavedNoRuntimeChange
SavedRestartRequired
SavedApplyFailed
ValidationRejected
SaveFailed
```

If apply fails, the UI must say that settings are saved but not active. It must not silently label the adapter Ready.

### 9.4 ConnectionCoordinator growth path

Begin extracting a `ConnectionCoordinator` that owns desired versus active connection state and adapter lifecycles. Keep the first change bounded; do not rewrite the entire app before physical output is proven.

The eventual boundaries are:

- `DjTransportService` — OS2L and future DJ adapters;
- `ControllerService` — MIDI/HID/Control One input and feedback;
- `OutputRouter` — immutable universe frames to output adapters;
- `ConnectionCoordinator` — persistence, diff/apply, reconnect, and status;
- `Runner` — deterministic show/layer/frame generation only.

### 9.5 Stable device identity

Do not rely indefinitely on Windows enumeration indices for MIDI or output devices. Persist a stable descriptor where available and resolve the current runtime index on each launch. Index can remain a compatibility hint, not the identity.

### 9.6 Acceptance gate D

- Save & Apply is visibly accessible at all tested sizes/DPI.
- A changed Micro setting persists after app restart.
- Active status matches the setting actually in use.
- Unchanged services are not unnecessarily restarted.
- Old output is blacked out before switching.
- A failed new adapter reports SavedApplyFailed and does not masquerade as active.

## 10. Work Package E — VirtualDJ/OS2L deterministic startup (P0/P1)

### 10.1 Immediate reliable setup

Connections must recommend automatic discovery first:

```text
os2l=Auto
os2lDirectIp=<blank>
```

It must also show and copy the safe direct-IP fallback:

```text
os2l=Yes
os2lDirectIp=<displayed listener endpoint>
wait 100ms & os2l_button 'EmberLights Keepalive' off
```

The fallback action is a reserved no-op and must never route to blackout,
Static Look, or Autoloop behavior.

The application should distinguish:

- OS2L disabled;
- listener starting;
- listening/waiting for VirtualDJ;
- client connected but no beat yet;
- receiving beat/transport;
- held/fallback clock;
- fault, including bind errors.

### 10.2 Move listener lifetime out of Show start

The OS2L listener should start when a loaded project enables OS2L, not only when Runner is started. This permits VirtualDJ to connect first and allows Start Show to consume an already healthy normalized clock.

Maintain a bounded event/snapshot boundary between `DjTransportService` and Runner. The transport thread may allocate/manage sockets; the scheduler consumes bounded normalized events without blocking.

### 10.3 Duplex feedback

Add nonblocking OS2L feedback output for commands that have meaningful button state, beginning with:

- blackout;
- work light;
- active Static Look;
- later Autoloop pads where stable names/page mapping exist.

Feedback must be derived from authoritative Runner state, not optimistic button clicks.

### 10.4 Discovery implementation and installed evidence

`_os2l._tcp` Windows DNS-SD advertisement is implemented and is the primary
setup. Keep direct-IP plus the reserved Keepalive action as a fallback. Neither
software implementation counts as installed VirtualDJ evidence: exercise both
paths on Windows, including discovery-service absence and reconnect, without
delaying the raw Micro/fixture proof.

### 10.5 Launch-order matrix

Test all combinations:

1. EmberLights project loaded, then VirtualDJ launched.
2. VirtualDJ launched, then EmberLights project loaded.
3. EmberLights Runner started before VirtualDJ.
4. VirtualDJ restarted while Runner remains active.
5. OS2L toggled off/on through Save & Apply.
6. TCP connection dropped and reconnected.
7. Port already occupied.

### 10.6 Acceptance gate E

- With automatic discovery or the documented safe fallback, OS2L connects
  without manually pressing a performance/DMX pad.
- BPM and beat begin without entering a mode-changing flow.
- Listener survives client restart and reports Waiting versus Connected truthfully.
- Clock fallback/recovery continues to work.

## 11. Work Package F — Static Look Toggle/Hold and priority semantics (P0/P1)

### 11.1 One core implementation

Expose shared typed commands, conceptually:

```text
staticLook.activate(lookId, sourceToken)
staticLook.deactivate(lookId, sourceToken)
staticLook.toggle(lookId, sourceToken)
staticLook.clear()
```

Expose authoritative state:

```text
staticLook.active.id
staticLook.active.name
staticLook.active.sourceToken
staticLook.active.transitioning
staticLook.active.transitionProgress
```

Toggle/Hold is an interaction/binding behavior over the same Static Look engine, not a second content type.

### 11.2 Required semantics

**Toggle**

- Press inactive Look: activate it.
- Release: no action.
- Press same active Look: deactivate it.
- Press a different Look: replace the active Look using its configured fade.

**Hold / momentary**

- Press: activate Look.
- Release: deactivate only if that same source activation still owns the active Look.
- If another Look was activated after the press, the old release must not clear the newer Look.

**Priority**

- Static Look owns `EventMoment` above Autonomous, TrackScript, and ManualAutoloop for properties it assigns.
- Underlying Autoloops continue advancing in the background.
- Clearing/releasing the Static Look reveals the current lower-layer values at their current phase.
- ManualOverride, Emergency/Work Light, Blackout, and Safety retain their higher authority.

### 11.3 Source ownership

Use a monotonically increasing activation/ownership token or equivalent stable source token. Do not infer ownership from only the Look index. This prevents stale release events from UI, MIDI, or Control One from clearing a newer activation.

### 11.4 Feedback

UI/controller LEDs must follow authoritative active state. Feedback is cleared only after the Runner accepts the release/toggle and publishes the new snapshot.

### 11.5 Tests

- toggle on/off;
- hold press/release;
- hold A, activate B, release A — B remains active;
- hold A from two sources — define and test deterministic last-activation ownership;
- replace with fade;
- Autoloop phase advances while Static Look is active;
- clear Static Look reveals advanced Autoloop frame;
- blackout and work light remain higher priority;
- behavior identical through direct Runner command and MIDI mapping.

### 11.6 Acceptance gate F

Joshua can visibly confirm Toggle and Hold modes, active feedback, Static Look priority over Autoloops, and seamless Autoloop return after release.

## 12. Work Package G — timing and real-time hardening (after physical proof)

The preview.314 jitter evidence is not acceptable as a production target, although it is unlikely to explain complete darkness by itself.

After raw and semantic physical output work:

- profile scheduler wake-up and output cadence separately;
- use an appropriate high-resolution waitable timer and/or Windows multimedia scheduling characteristics;
- avoid formatting, logging, device I/O, locks, and allocation on the scheduler path;
- preserve newest-frame-wins behavior on output threads;
- measure scheduler and actual adapter write cadence;
- test under UI resize, Diagnostics refresh, file save, OS2L traffic, MIDI traffic, and adapter reconnect;
- establish a documented p99 target, initially under 2 ms at 40 Hz unless evidence supports another threshold;
- run a ten-minute hardware test before an eight-hour qualification.

Timing work must not become a reason to postpone the standalone raw-output proof.

## 13. Work Package H — Control One path, clearly separated (next core iteration)

Joshua requires both the SoundSwitch Micro and Control One to become dependable. Treat the Control One as two separate products:

1. **Control surface path** — MIDI input, mappings, Toggle/Hold behavior, bank selection, and output feedback. This builds on the accepted device-agnostic MIDI architecture and should receive a bundled, versioned Control One profile.
2. **Physical DMX outputs** — proprietary device/output transport requiring a separate clean-room probe and qualification program. Do not imply that working MIDI proves the onboard DMX outputs.

Immediate post-Micro work:

- persist stable MIDI device identity, not only index;
- capture the complete owned-device MIDI message map;
- build a bundled profile using shared command IDs;
- implement authoritative LED feedback;
- test hot-plug and duplicate-port cases;
- begin a separately documented output-interface probe only after Micro/OutputRouter contracts are stable.

## 14. Architecture target for growth

The recovery must leave a better foundation without forcing a risky rewrite before the hardware gate.

```text
                    Project / app settings
                            |
                    ConnectionCoordinator
                 /            |             \
        DjTransportService ControllerService OutputRouter
                 \            |             /
                    typed commands/events
                            |
                          Runner
          sync -> layers -> renderer -> immutable frames
                            |
                       OutputRouter
          Art-Net | sACN | USB Pro | Micro | future adapters
```

Rules:

- Runner remains deterministic and hardware-agnostic.
- Adapter sessions live on non-scheduler threads.
- OutputRouter receives immutable fixed-size frames.
- Connections owns desired/active state and diff/apply.
- UI, MIDI, Control One, keyboard, and OS2L invoke shared commands.
- Status is immutable/bounded and converges to the latest state.
- Physical qualification evidence is separate from software-open state.

Do not prematurely replace the fixed two-universe V1 frame model, but do not hard-code Micro or Control One into fixture semantics.

## 15. Overnight implementation sequence

The work agent should use this exact order and keep each stage shippable:

### Slice 1 — Raw proof package

- shared Micro session extraction;
- Hardware Test tool;
- init delay and blackout warm-up;
- exact report and auto-blackout;
- golden byte/state tests;
- Windows installer includes the tool.

### Slice 2 — Frame truth and bench project

- bounded frame inspector — implemented in Preview 95 software;
- migration patch warnings;
- one-fixture bench project/wizard path — output-disabled editable project and
  installed guide packaged; native installed execution pending;
- raw-to-semantic comparison report — automatic active-bench comparison
  implemented; raw-attempt/physical evidence binding pending.

### Slice 3 — Blocking operational fixes

- Connections sticky action/scroll layout;
- truthful Save & Apply outcomes;
- VirtualDJ ONINIT copy/help and improved OS2L states;
- Static Look Toggle/Hold commands, ownership, feedback, and tests.

### Slice 4 — Hardening

- adapter reconnect/blackout tests;
- connection diff/apply extraction;
- scheduler cadence profiling and first timing fix;
- installed-app smoke and release evidence.

If time is constrained, finish and package Slice 1 before partially implementing broad UI work.

## 16. Required build-agent deliverables

The next testable build must include:

1. A Windows installer artifact linked to the exact commit.
2. Installed Hardware Test tool with safe raw Micro modes.
3. A visible, non-overlapping Save & Apply Connections action.
4. Frame inspector or exported frame report showing actual channel/value pairs.
5. Visible warning that the migrated 71-fixture patch/profiles are provisional.
6. VirtualDJ startup-action copy/help.
7. Static Look Toggle/Hold core implementation and automated tests, if Slice 3 is reached.
8. Unit/integration/Windows smoke results.
9. A concise `MORNING_HARDWARE_TEST.md` or equivalent checklist.
10. Release notes that distinguish software verification from Joshua's pending physical confirmation.

Do not claim the Micro is supported or physically qualified until Joshua confirms the hardware gates.

## 17. Morning hardware test script for Joshua

The build should make this test possible with minimal navigation:

1. Close the SoundSwitch desktop application and any other program that might own the Micro.
2. Plug the Micro directly into the Windows PC.
3. Connect the known-good DMX transmitter or, when practical, one fixture directly by DMX cable.
4. Put exactly one known fixture in a confirmed DMX mode and address.
5. Open **EmberLights Hardware Test**.
6. Run **Inspect + Initialize** and record whether the Micro indicator changes.
7. Run **Blackout Stream** and confirm the transmitter's DMX indicator/state.
8. Run **Single Channel** or **Address-Range Chase** for the confirmed fixture footprint.
9. Record the first channel/value that produces a physical response.
10. Confirm the tool automatically returns to blackout.
11. Close/reopen and repeat the known working raw frame.
12. Unplug/replug the Micro and repeat once.
13. Open the one-fixture EmberLights bench project and trigger the matching semantic Look.
14. Compare the frame inspector with the successful raw frame.
15. Start VirtualDJ with the provided ONINIT action and confirm BPM/beat arrives without pressing a DMX pad.
16. Test one Static Look in Toggle mode and one in Hold mode over a running Autoloop.
17. Export Hardware Test and EmberLights Diagnostics reports.

## 18. Core-ready acceptance definition

The core is ready for broad UI/UX implementation only when all of the following are true:

- Micro raw output works repeatedly on the owned device.
- Automatic and emergency blackout work.
- Adapter reconnect works without stale-frame replay.
- One exact fixture/profile/address works through Runner and matches raw output.
- Migrated/unverified fixture data is labeled honestly.
- Connections settings persist and their active state is visible and truthful.
- OS2L connects deterministically using the documented startup path and survives client restart.
- Static Looks support Toggle and Hold, override Autoloops, and release back to the advanced Autoloop phase.
- A ten-minute hardware stream passes before longer soak qualification.
- Installer, smoke tests, diagnostics, and release evidence all correspond to the same commit.

## 19. Explicit non-goals and prohibitions for this recovery

- Do not redesign the full default or SoundSwitch Reference skin.
- Do not add decorative UI while raw output remains unproven.
- Do not create more speculative Micro framing modes.
- Do not use accepted WinUSB writes as a physical support claim.
- Do not treat internal project validation as fixture/address qualification.
- Do not hard-code the whole product around Joshua's current 71-fixture rig.
- Do not bypass safety layers to make a test appear successful.
- Do not move USB writes, logging, file I/O, or network I/O onto the scheduler path.
- Do not couple Control One proprietary DMX research to the generic MIDI/control-surface path.
- Do not destructively modify the original SoundSwitch export.

## 20. Evidence references

Repository evidence:

- `native-core/src/soundswitch_micro.cpp`
- `native-core/include/showcore/soundswitch_micro.hpp`
- `native-core/src/runner.cpp`
- `native-core/src/windows_app.cpp`
- `native-core/src/soundswitch_v1.cpp`
- `native-core/src/fixture.cpp`
- `native-core/src/layer_resolver.cpp`
- `native-core/src/look.cpp`
- `docs/18_SOUNDSWITCH_MIGRATION.md`
- `spec/ui/command-state-skin-contract-v0.md`

Independent protocol comparison:

- `https://github.com/asoronow/laser-controller/blob/fc5edcea8a037e35b531938c6d6edc9975d4f021/app/lib/soundswitch-driver.ts`
- `https://github.com/asoronow/laser-controller/blob/fc5edcea8a037e35b531938c6d6edc9975d4f021/scripts/replay-exact.mjs`

The external implementation is evidence for interoperability behavior, not authority to copy code blindly. Keep EmberLights' implementation independently structured, tested, and attributed where any licensed material is reused.
