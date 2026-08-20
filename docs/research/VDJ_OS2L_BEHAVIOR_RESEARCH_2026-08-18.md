# VirtualDJ / OS2L behavior research — 2026-08-18

## Context

Joshua's real VirtualDJ + EmberLights testing exposed two user-visible failures:

1. `os2l_button 'blackout'` reaches EmberLights but behaves like a momentary flash instead of a true toggle.
2. OS2L appears to require a manual DMX/OS2L pad press to connect or reconnect on first launch and after some transport/song transitions.

This research note supports issue #89 and `docs/46_OS2L_RELIABILITY_AND_VDJ_CONTROL_CHECKPOINT.md`. It records external OS2L/VirtualDJ observations so implementers do not rely on guesses or duplicate research.

## Primary protocol findings

### OS2L message surface

The public OS2L protocol defines the inbound message kinds EmberLights already parses:

```json
{"evt":"beat","change":true,"pos":12345,"bpm":128.0}
{"evt":"btn","name":"blackout","state":"on"}
{"evt":"cmd","id":1,"param":0.5}
```

The protocol also defines optional outbound feedback from the DMX application:

```json
{"evt":"feedback","name":"blackout","state":"on"}
{"evt":"feedback","name":"blackout","state":"off"}
```

Protocol implication: stateful VirtualDJ buttons require EmberLights to send authoritative feedback. Without feedback, VDJ has no reliable external state source.

Source: https://os2l.org/

## VirtualDJ behavior findings

### `os2l_button` becomes stateful only when feedback exists

External reports and the OS2L protocol agree on the model:

- without feedback, VDJ button actions behave like press/release or `while_pressed`;
- with feedback, VDJ can store the last remote state and send the opposite state on the next press;
- release should not send an extra off event in the feedback-backed toggle case.

Implementation implication: EmberLights should make plain VDJ mappings like this correct:

```vdjscript
os2l_button 'blackout'
```

Do not require Joshua to maintain shadow VDJ variables such as `$EL_BLACKOUT` as the accepted solution.

Useful corroborating references:

- https://forum.thelightingcontroller.com/viewtopic.php?t=6559
- https://forum.thelightingcontroller.com/viewtopic.php?f=94&t=6559

### `os2l=Auto`, `os2l=Yes`, and `os2lDirectIp`

Reports from VirtualDJ support/community history consistently show that OS2L connection timing differs by configuration:

- `os2l=Auto` may defer connection until OS2L is needed.
- `os2l=Yes` is frequently recommended when the user wants OS2L always active.
- `os2lDirectIp` can trigger lazy connection behavior where the first OS2L command wakes or initiates the connection.
- Some historical direct-IP cases also mention startup delay or awkward connection timing.

Implementation implication: EmberLights' normal setup helper should prefer discovery-based configuration when installed testing confirms it:

```text
os2l = Yes
os2lDirectIp = [blank]
```

Direct IP should remain an explicit fallback, not the ideal same-machine path, unless VirtualDJ capture proves otherwise for current builds.

Useful references:

- https://virtualdj.com/forums/237658/VirtualDJ_Technical_Support/VirtualDJ_only_sends_OS2L_data_after_manually_using_an_os2l_script_once.html
- https://www.virtualdj.com/forums/260254/VirtualDJ_Technical_Support/Bug_report%3A_Slow_startup_time_when_using_os2lDirectIP.html
- https://www.virtualdj.com/forums/260810/Wishes_and_new_features/Setting_os2l_config_option_to__yes__should_connect_os2l__as_it_does_when_setting_to__no_.html
- https://www.lightjams.com/os2l.html

### Current-build risk: explicit `on`/`off` must be captured, not assumed

Joshua observed that:

```vdjscript
os2l_button 'blackout'
```

reaches EmberLights, while:

```vdjscript
os2l_button 'blackout' on
```

did not turn blackout on in his tested mapping path.

External reports are mixed enough that the build agent must not assume the docs describe the installed behavior exactly. Some third-party reports suggest recent VirtualDJ versions may have changed or broken some `os2l_button` state behavior in specific workflows.

Implementation implication: before finalizing mapping recommendations, capture real wire traffic from Joshua's installed VDJ for:

```vdjscript
os2l_button 'blackout'
os2l_button 'blackout' on
os2l_button 'blackout' off
os2l_button 'EmberLights Keepalive' off
```

Record exact JSON, event order, connection state, and release behavior. Treat forum reports as hypothesis only until captured.

Reference to treat as non-authoritative but relevant risk signal:

- https://www.lumikit.com.br/forum/viewtopic.php?t=41

## Other OS2L host patterns worth borrowing

### QLC+

QLC+ OS2L documentation says its OS2L plugin listens to `beat`, `cmd`, and `btn` events and maps them through input profiles.

Implementation implication: EmberLights' future `os2l_cmd` bridge should be profile/mapping driven and canonical-command-backed, not hard-coded raw DMX.

References:

- https://docs.qlcplus.org/v5/plugins/os2l
- https://docs.qlcplus.org/v4_de/plugins/os2l

### ENTTEC ELM

ENTTEC's ELM documentation exposes an OS2L monitor that shows arriving OS2L messages, including page/name/state for `btn` and id/param for `cmd`, while excluding beat spam from the main monitor.

Implementation implication: EmberLights Diagnostics should include a bounded OS2L monitor for the most recent sanitized non-beat input and last feedback sent. Beat messages should remain summarized/counted rather than spamming the UI.

Reference:

- https://support.enttec.com/user-manuals/elm

## Recommended product decisions from research

### Decision 1 — feedback is mandatory for stateful controls

For Blackout, Work Light, Static Looks, and any boolean/latching surface, EmberLights must send authoritative feedback. VDJ variables are acceptable only as temporary operator workarounds.

### Decision 2 — connection state and sync state are separate

Stopping or changing a song may change transport sync, beat source, BPM, or predictive hold status. It must not be treated as an OS2L socket/listener failure unless the socket actually disconnects.

Diagnostics must separately show:

```text
listener state
discovery state
client TCP state
transport beat/sync state
last raw non-beat input
last feedback output
```

### Decision 3 — app-owned OS2L service before broad VDJ features

Do not broaden VDJ pad/fader features until the OS2L service is persistent and bidirectional. Otherwise every new mapping will inherit flaky connection and stale-state behavior.

### Decision 4 — direct-IP fallback must be honest

`EmberLights Keepalive` remains a no-op compatibility fallback. It must not become the product's accepted connection model. The normal goal is auto-discovery/auto-reconnect without manual DMX-pad wake-up.

## Implementation priorities confirmed by research

1. Add outbound feedback serialization/writes to the OS2L transport.
2. Prove Blackout as the first stateful VDJ toggle using authoritative `output.blackout` feedback.
3. Add an OS2L capture/monitor path before making assumptions about current VDJ's explicit `on`/`off` behavior.
4. Move OS2L listener/discovery out of Runner Start Show lifetime into an application/connection-owned service.
5. Then build the generic `os2l_cmd` mapping surface over the canonical command registry.

## Non-goals

- Do not build a VDJ-specific lighting state machine.
- Do not make VDJ variables the accepted product solution.
- Do not expose raw DMX integer commands as the first external command bridge.
- Do not treat forum reports as conclusive without installed capture.
- Do not put socket writes or DNS-SD work on the DMX scheduler.
