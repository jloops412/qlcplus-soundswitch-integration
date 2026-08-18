# OS2L Reliability Build Agent Handoff — 2026-08-18

## Mission

Fix the real VirtualDJ/OS2L failures reported during owner testing without broadening EmberLights UI scope:

1. `os2l_button 'blackout'` reaches EmberLights but behaves as a momentary control instead of a true stateful toggle.
2. VirtualDJ/OS2L often does not connect until the operator manually presses a DMX/OS2L pad, and the path appears to drop or stop advancing around song stop/load/change.

Issue #89 is the owning issue. `docs/46_OS2L_RELIABILITY_AND_VDJ_CONTROL_CHECKPOINT.md` is the current product/architecture checkpoint. This handoff converts both into an implementation-ready build plan.

## Current branch/context

- Repository: `jloops412/EmberLights`
- Active integration branch: `agent/backyard-party-v2`
- Current PR: #86
- Owning issue: #89
- Coordinate with: #38, #35, #31, #64, #87

Do not start a new feature lane. This is a P0 transport/control reliability slice.

## User-observed facts to preserve

- Plain `os2l_button 'blackout'` does reach EmberLights.
- Explicit `os2l_button 'blackout' on` did not produce the expected persistent blackout in the tested VDJ pad path; this must be captured on the wire before assuming VDJScript syntax or EmberLights parsing is wrong.
- Pressing any DMX/OS2L pad tends to wake the connection, which strongly suggests lazy connection initiation or reconnect discovery behavior rather than a DMX-rendering failure.
- The operator needs the normal path to work without scripting hacks: launch either app first, load/play/stop/change songs, and use Blackout/Looks/Autoloops without manually repairing OS2L.

## Current code shape to understand first

### OS2L TCP server

Files:

- `native-core/include/showcore/os2l_server.hpp`
- `native-core/src/os2l_server.cpp`
- `native-core/src/os2l.cpp`

Current server responsibilities:

- opens an IPv4 listener;
- accepts one client;
- receives bytes;
- decodes JSON events through `Os2lStreamDecoder`;
- exposes connection/disconnect/message/decode stats;
- advertises DNS-SD on Windows.

Known gap: it is effectively receive-only. There is no public bounded send/write/feedback path.

### Runner input ownership

Files:

- `native-core/include/emberlights/runner.hpp`
- `native-core/src/runner.cpp`

Current OS2L adapter sits inside `RunnerService::run_input()` as a local `showcore::Os2lTcpServer os2l;`.

Important consequences:

- The OS2L listener is tied to Runner/input-thread lifetime.
- `run_input()` waits until there is a published activation before opening OS2L.
- On stop, the local OS2L server is closed.
- A real persistent app-level DJ transport service does not yet exist.

### Current inbound semantics

`RunnerService::os2l_callback()` currently:

- sends OS2L beat events into the beat queue;
- treats `EmberLights Keepalive` / `emberlights.keepalive` as a no-op;
- maps `blackout` directly to `service.set_blackout(event.button.on)`;
- maps `worklight` / `white` to `set_work_light(event.button.on)`;
- forwards other button names as target-aware Look/Autoloop button events.

This is why plain `os2l_button 'blackout'` works as a momentary control: VDJ sends an on event on press and likely an off event on release when feedback is absent.

### Existing safety foundation to preserve

The branch already has connection/session epoch work for Static Look ownership:

- OS2L/MIDI owner session tokens exist.
- Stale release/loss from an old connection must not clear a new activation.
- Disconnect may release transport-owned external holds/latches, but must not clear authoritative global blackout.

Do not remove or weaken these protections.

## Non-negotiable architecture

The final shape must be:

```text
VirtualDJ / OS2L
    -> app-owned Os2lService / DjTransportService
    -> canonical command/state boundary
    -> Runner / application state
    -> authoritative registered state
    -> outbound OS2L feedback
```

Rules:

- No second lighting state machine.
- No VDJ-only command semantics that bypass the command/state registry.
- No socket writes on the DMX scheduler.
- No unbounded inbound or outbound queues.
- No stale command replay after reconnect.
- Blackout remains authoritative and non-droppable.
- Beat/sync state and TCP connection state are separate facts.

## Implementation order

### Slice 1 — Blackout outbound feedback, no service extraction yet

Goal: solve the Blackout toggle path with the smallest safe change and prove the send path.

Touch candidates:

- `native-core/include/showcore/os2l_server.hpp`
- `native-core/src/os2l_server.cpp`
- `native-core/src/os2l_capture.cpp` if useful for capture/diagnostics
- `native-core/src/runner.cpp`
- `native-core/include/emberlights/runner.hpp` only if new counters/status fields are needed
- `native-core/tests/test_main.cpp` or a new narrow OS2L test file if the repo structure supports it
- CMake/Makefile only to register tests if needed

Required design:

1. Add a bounded send method to `Os2lTcpServer`, for example:

```cpp
[[nodiscard]] bool send_feedback(std::string_view name, bool on) noexcept;
```

or a lower-level:

```cpp
[[nodiscard]] bool send_raw(std::string_view json) noexcept;
```

plus a small serializer:

```json
{"evt":"feedback","name":"blackout","state":"on"}
```

2. Keep it safe:

- return false if no client is connected;
- increment send failure stats when send fails;
- handle partial sends correctly or explicitly loop within bounded message size;
- on unrecoverable send failure, disconnect the client and return to listener state;
- do not allocate on the scheduler;
- do not block indefinitely.

3. On OS2L client connect, send initial authoritative Blackout feedback.

4. Whenever `blackout_requested_` changes, enqueue/publish Blackout feedback from the input/control side, not from the scheduler.

5. Coalesce duplicate feedback: if Blackout remains on, repeated on messages are not harmful but should not create unbounded traffic.

6. Do **not** change `set_blackout()` semantics so that socket feedback becomes a new source of truth. The source of truth remains EmberLights state.

Acceptance tests for Slice 1:

- Serialize `blackout` feedback exactly.
- Connected client receives initial feedback after connect.
- Setting blackout on emits feedback on.
- Setting blackout off emits feedback off.
- Disconnect and reconnect gets a fresh initial snapshot.
- Broken client/send failure does not crash and returns server to listening/fault-retry path.
- Blackout remains true across client disconnect when it was active.
- Keepalive still changes no output state.

Manual VDJ acceptance after build:

- Use plain `os2l_button 'blackout'`.
- First press: blackout turns on and stays on after release.
- Second press: blackout turns off.
- Pressing EmberLights UI Blackout changes VDJ's observed button state.

Stop after this slice if time/credits are tight. It directly attacks the first user-visible bug.

### Slice 2 — Installed raw OS2L capture and diagnostics

Goal: resolve the unexplained `os2l_button 'blackout' on` behavior and distinguish TCP disconnect from transport beat loss.

Touch candidates:

- `native-core/src/os2l_capture.cpp`
- installer/package tooling if the capture utility is not currently shipped
- Diagnostics view/status model only if a narrow non-invasive display can be added

Required captures:

- `os2l_button 'blackout'`
- `os2l_button 'blackout' on`
- `os2l_button 'blackout' off`
- `os2l_button 'EmberLights Keepalive' off`
- song load / play / stop / replace transitions

Record:

- raw JSON;
- timestamps;
- connection connect/disconnect events;
- whether VDJ sends release events;
- whether song stop closes TCP or merely stops beat messages.

Do not infer from documentation when the installed capture disagrees.

### Slice 3 — App-owned OS2L service extraction

Goal: fix the startup/reconnect/song-change issue correctly.

This is larger than Slice 1. Do not attempt it until Slice 1 tests are green unless specifically assigned.

Target outcome:

```text
Application/connection layer owns OS2L listener and DNS-SD advertisement.
Runner consumes normalized transport events and commands.
Stopping Runner does not necessarily close the listener.
Song transport does not own the listener.
```

New or extracted boundary candidates:

- `native-core/include/emberlights/os2l_service.hpp`
- `native-core/src/os2l_service.cpp`
- or `DjTransportService` if broader MIDI/DJ transport ownership is being handled

Responsibilities:

- persistent listener lifecycle;
- DNS-SD registration/re-registration;
- client accept/reaccept;
- session epoch assignment;
- inbound event queueing;
- outbound feedback queueing/coalescing;
- diagnostics counters;
- no direct Runner scheduling or DMX output ownership.

Runner integration:

- Runner receives beat events if active;
- Runner receives commands if active and available;
- app service remains listening even if Runner is stopped, where safety allows;
- queued stale events from old sessions are discarded.

Acceptance matrix:

- EmberLights first -> VDJ second connects without DMX pad.
- VDJ first -> EmberLights second connects without DMX pad where VDJ discovery permits.
- EmberLights restart while VDJ open.
- VDJ restart while EmberLights open.
- stop/play same loaded track repeatedly.
- load A -> play -> stop -> load B -> play.
- if VDJ closes TCP, EmberLights remains listening/advertised and reconnects without Runner restart.
- if VDJ only stops beat messages, diagnostics say TCP ready but sync waiting/held.

### Slice 4 — Generic `os2l_cmd` command bridge

This is P1. Do not start before reliability/feedback is proven.

Goal: map VirtualDJ numeric `os2l_cmd id param` into canonical registered EmberLights commands.

Initial candidates:

- release all overrides;
- manual BPM / tap where semantics fit;
- autoloop next/previous/bank select;
- group/fixture property set/release with explicit configured target/property mapping;
- color/intensity/strobe/movement faders through existing property vocabulary.

Rules:

- versioned mapping table;
- exact target/property validation;
- reject unknown IDs;
- reject hazards unless explicitly armed/allowed;
- no raw DMX shortcuts;
- no direct Runner bypass;
- all outcomes visible in diagnostics.

## Test strategy

### Native loopback tests

Create a fake/local client against `Os2lTcpServer` where possible.

Minimum assertions:

- accepts a client;
- parses inbound button and beat events;
- sends outbound feedback bytes;
- handles disconnected client;
- handles send after disconnect gracefully;
- reconnect creates new session;
- feedback snapshot sent on new connection;
- no stale release crosses connection session.

### Runner/control tests

- Blackout set from OS2L emits feedback and updates state.
- Blackout set from UI/command facade emits OS2L feedback if client connected.
- Disconnect does not clear Blackout.
- Keepalive does not change Blackout, Work Light, Static Look, Autoloop, or overrides.
- Named Static Look behavior remains target-aware after OS2L changes.

### Installed Windows + VirtualDJ manual tests

Add to a checklist/report, even if not automated immediately:

1. `os2l=Yes`, `os2lDirectIp` blank, EmberLights first.
2. `os2l=Yes`, `os2lDirectIp` blank, VDJ first.
3. Direct-IP fallback with Keepalive.
4. `os2l_button 'blackout'` true toggle.
5. Song stop/play same track.
6. Song replace/load next track.
7. VDJ restart.
8. EmberLights restart.
9. Port conflict.
10. Eight-hour soak later.

## Diagnostics required by #89

Expose, at minimum internally/tests and eventually UI:

```text
OS2L listener: closed | listening | fault
Discovery: unavailable | starting | advertised | fault
Client: disconnected | connected
Transport sync: waiting | synced | hold | unavailable
Last client connect/disconnect time
Connection/session epoch
Messages / decode errors / dropped actions
Last raw received event, bounded/sanitized
Last feedback sent
Feedback sends / send failures
Last socket/discovery error
Configured endpoint vs actual bound endpoint
```

Do not conflate `ClientConnected` with transport sync. Do not call beat loss a socket disconnect.

## Files to avoid broadening

Avoid large edits to:

- `native-core/src/windows_app.cpp` unless needed for a narrow diagnostic/status label;
- Slint lab files;
- fixture library/profile code;
- Static Look authoring UI;
- Autoloop V2 authoring/runtime beyond preserving existing Look/Autoloop behavior;
- installer/release files except to include a capture tool or test evidence.

## Suggested first PR summary

> Implement bounded OS2L feedback for authoritative Blackout state. Add server-side send support, feedback serialization, connection snapshot, state-change feedback, and loopback tests. Keep OS2L receive semantics and Runner scheduling unchanged. This makes VirtualDJ `os2l_button 'blackout'` eligible to behave as a stateful toggle once VirtualDJ receives feedback.

## Suggested second PR summary

> Extract OS2L listener/advertisement into an application-owned transport service so discovery/reconnect survives Runner stop/start and song transport changes. Preserve session epochs, keep stale releases bounded, and add installed VirtualDJ launch-order/reconnect evidence.

## Stop conditions

Stop and report instead of continuing if:

- sending feedback requires socket writes from the scheduler;
- a design would clear Blackout on OS2L disconnect;
- fixing reconnect requires replacing broad Runner lifecycle in the same patch as feedback;
- tests cannot distinguish TCP connection state from beat/sync state;
- VDJ installed behavior contradicts assumptions and no raw capture exists.

## Temporary operator workaround remains non-architectural

Until Slice 3 lands, `ONINIT` or `ONSONGLOAD` keepalive mappings may remain documented as compatibility fallbacks only:

```vdjscript
wait 500ms & os2l_button 'EmberLights Keepalive' off
```

They must never be treated as the accepted product behavior.
