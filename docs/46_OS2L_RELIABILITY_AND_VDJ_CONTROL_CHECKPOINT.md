# OS2L Reliability and VirtualDJ Control Checkpoint

**Date:** 2026-08-18  
**Primary tracker:** #89  
**Coordinates with:** #38, #40, #35, #31, #64, #59, #63, #66, #87  
**Integration context:** PR #86 / `agent/backyard-party-v2` until convergence Gate A resolves the current branch truth.

## Why this checkpoint exists

Real owner-rig testing with VirtualDJ exposed two related product blockers that must be solved as one transport/control-surface boundary rather than with permanent VDJScript workarounds.

### Observed connection failure

- EmberLights and VirtualDJ frequently do not establish OS2L automatically in either launch order.
- Pressing an OS2L/DMX pad wakes the path immediately.
- Stopping, replacing, loading, or playing songs can leave the OS2L path apparently disconnected or unusable until another manual pad press.
- The product currently does not make it sufficiently obvious whether the TCP client actually disconnected or whether beat/sync traffic merely stopped.

### Observed stateful-button failure

- `os2l_button 'blackout'` reaches EmberLights.
- It behaves as a momentary press/release rather than a reliable stateful toggle.
- `os2l_button 'blackout' on` did not produce the expected persistent blackout in the tested mapping path.
- EmberLights' current OS2L server is effectively receive-only and does not publish authoritative button feedback to VirtualDJ.

The explicit `on` behavior conflict must be settled with real wire capture. Do not infer the user's installed VirtualDJ packet behavior from documentation alone.

---

## Binding architecture

OS2L is an **application-owned DJ transport service**, not a Runner-owned, song-owned, skin-owned, or VDJ-only lighting engine.

```text
VirtualDJ
   |
   | DNS-SD / TCP / OS2L
   v
Application-owned Os2lService / DjTransportService
   |
   +--> inbound beat/transport normalization
   +--> inbound button/command adapter
   +--> outbound authoritative feedback
   +--> connection/discovery/session diagnostics
   |
   v
Canonical EmberLights command/state facade
   |
   v
Runner / authoritative live state
```

### Non-negotiable consequences

1. Listener and DNS-SD advertisement lifetime are independent of song play/stop/load state.
2. Listener and discovery lifetime are independent of Start Show / Runner lifetime where safety permits.
3. TCP connection state and transport sync state are separate facts.
4. UI, MIDI/controllers, skins, VirtualDJ, and future external surfaces invoke the same canonical commands and observe the same authoritative states.
5. No VDJ variable becomes authoritative lighting state.
6. No VDJ-only domain command model is introduced.
7. No socket/network/discovery/feedback work runs on the DMX scheduler.
8. Existing connection/session epochs remain the stale-release and stale-owner boundary.

---

## P0 implementation order

### 1. Persistent service ownership

Move/finish OS2L listener and DNS-SD ownership outside the Runner/song transport lifecycle.

Required behaviors:

- listener remains available while configured even when no song is playing;
- advertisement remains active or re-registers after recoverable faults;
- a client can disconnect and reconnect without Runner restart or project reload;
- either application may launch first;
- restarting either application does not require a lighting pad to mutate state before the connection returns;
- no reconnect replays stale actions or stale frames.

### 2. Truthful transport diagnostics

Expose independent state for:

```text
listener
DNS-SD discovery
TCP client
transport/sync
connection/session epoch
last connect/disconnect
messages
raw last inbound event (sanitized and bounded)
decode errors
dropped inbound actions
last outbound feedback
feedback sends/failures
socket/discovery errors
configured endpoint
actual bound endpoint
```

Do not present beat loss as a socket disconnect. Do not present an open socket as proof of healthy beat sync.

### 3. Raw installed VirtualDJ capture

Use/package the existing `os2l_capture` path and retain sanitized evidence for:

```vdjscript
os2l_button 'blackout'
os2l_button 'blackout' on
os2l_button 'blackout' off
os2l_button 'EmberLights Keepalive' off
```

Capture exact JSON, event ordering, timestamps, automatic button release, connection establishment, and song-load/play/stop transitions.

The specific question to settle is whether VirtualDJ actually closes the TCP connection on song transitions or only stops/resets transport messages.

### 4. Bounded outbound OS2L feedback

Add same-connection outbound writes without placing socket I/O on Runner's scheduler.

First acceptance target is Blackout:

```json
{"evt":"feedback","name":"blackout","state":"on"}
{"evt":"feedback","name":"blackout","state":"off"}
```

Rules:

- send the current authoritative blackout state after a client connects;
- publish feedback whenever authoritative blackout changes from any supported surface;
- connection loss never silently clears authoritative blackout;
- slow/broken clients cannot create unbounded memory/queue pressure;
- send failure returns transport safely to listening/recovery behavior.

After Blackout is proven, extend the same authoritative feedback model to Work Light and stateful named content where OS2L semantics fit.

### 5. Static Look and Autoloop feedback

Static Look feedback must reflect authoritative selection, not button ownership guesses.

For example, switching Look A to Look B should make the external surface observe A off and B on coherently.

Autoloop boolean feedback may be used only where the active/clear concept is semantically correct. Progress remains canonical continuous state rather than being misrepresented as a button.

---

## VirtualDJ setup policy to qualify

The current product helper must be retested against installed VirtualDJ behavior.

Candidate normal same-machine configuration to qualify:

```text
os2l = Yes
os2lDirectIp = blank
```

The goal is DNS-SD discovery without requiring a lighting action to initiate a direct-IP connection.

Direct-IP remains a compatibility fallback when discovery is unavailable.

The existing reserved no-op remains valid as a fallback activation action:

```vdjscript
os2l_button 'EmberLights Keepalive' off
```

`ONINIT` and optionally `ONSONGLOAD` mappings may be offered as compatibility workarounds while qualification is open, but they are **not** the target architecture and must never mutate lighting state.

---

## P1 external command bridge after transport proof

Once connection and feedback reliability are proven, VirtualDJ should become another canonical EmberLights control surface.

### Keep named compatibility actions

Named Static Look and Autoloop actions remain useful operator-friendly shortcuts, but their behavior should route through the canonical command/state boundary.

### Add safe continuous/numeric control

Map inbound OS2L `cmd` (`id`, `param`) through a versioned external-mapping table to approved canonical commands.

Targets include, where semantics and safety allow:

- fixture/group property set/release;
- intensity and profile-backed color properties;
- movement/position/rate controls;
- strobe and other property controls under existing caps/safety gates;
- Autoloop navigation/bank controls;
- Release All;
- tap/manual BPM;
- other registry commands whose interaction model fits OS2L.

Do not map raw DMX as the public external command vocabulary. Use the stable fixture/property/target model and existing safety validation.

Unknown command IDs, invalid parameters, missing/stale targets, unsupported properties, hazardous actions, and queue pressure must fail explicitly and diagnostically.

---

## Required installed-Windows / VirtualDJ acceptance journey

1. Configure the integration once.
2. Launch EmberLights first, then VirtualDJ: automatic connection.
3. Launch VirtualDJ first, then EmberLights: automatic connection.
4. Load song A, play, stop, play again: no manual OS2L wake-up.
5. Replace/load song B: no manual OS2L wake-up.
6. Restart VirtualDJ while EmberLights stays open: safe automatic recovery.
7. Restart EmberLights while VirtualDJ stays open: safe automatic recovery.
8. Trigger plain `os2l_button 'blackout'`: first press latches Blackout on; button release does not clear it; second press clears it.
9. Change Blackout from EmberLights: VirtualDJ observes the updated state.
10. Trigger Static Looks and Autoloops and confirm feedback remains authoritative across replacement/clear.
11. Direct-IP + inert keepalive fallback remains available without mutating live state.
12. Repeated transport changes and reconnects are included in the DJ-workstation soak test.

The normal user journey must not include **"press a DMX pad to make OS2L connect again."**

---

## Stop / claim rules

- Do not call the problem solved because a keepalive workaround masks it.
- Do not infer explicit `on`/`off` packet behavior without the installed capture.
- Do not clear global Blackout because a VDJ socket disconnected.
- Do not bind socket lifecycle to Runner lifecycle.
- Do not create a duplicate VDJ lighting-state model.
- Do not put OS2L socket writes, discovery, parsing, formatting, or diagnostics work on the DMX scheduler.
- Do not broaden hazardous external actions merely because a numeric OS2L command can represent them.

## Source-of-truth tracker

Implementation scope, checkboxes, and completion evidence live in **GitHub issue #89**. This document exists so continuation/build agents encounter the architectural decision and user-tested failure in the repository even when they are not reading the originating conversation.