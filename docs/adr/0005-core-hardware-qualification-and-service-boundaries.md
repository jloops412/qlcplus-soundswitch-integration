# ADR 0005: Core Hardware Qualification and Service Boundaries

- Status: Accepted
- Date: 2026-08-11
- Governing plan: `../21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`

## Context

Preview.314 demonstrated active Runner timing, OS2L messages, rendered nonzero frames, selected-universe routing, an open SoundSwitch Micro WinUSB handle, and accepted 522-byte writes. The owned fixture still did not respond. The active SoundSwitch migration project also uses a deliberately staged, non-overlapping patch and provisional semantic profiles rather than decoded physical addresses/modes.

The same test exposed three additional core defects: VirtualDJ direct-IP OS2L remains dormant until a first OS2L command is invoked, Connections Save & Apply is hidden by fixed Win32 layout geometry at practical sizes/DPI, and Static Looks lack an explicit cross-surface Toggle/Hold ownership contract.

## Decisions

### 1. Qualification boundaries are explicit

The product distinguishes these facts:

1. device discovered;
2. handle opened;
3. protocol initialized;
4. warm-up completed;
5. host accepted frame writes;
6. physical DMX/transmitter observed;
7. fixture response confirmed;
8. reconnect/blackout/soak qualified.

No earlier boundary implies a later one. `Ready` may not collapse all of them.

### 2. Raw transport is proven before fixture semantics

A safe installed Hardware Test sends bounded raw universe/channel values through the production adapter session without compiling a project. One exact fixture/profile/address is then compared byte-for-byte against the successful raw frame.

This prevents transport changes from being mixed with speculative fixture/profile changes.

### 3. The Micro uses one shared session implementation

Hardware Test and Runner use the same SoundSwitch Micro session. The session owns enumeration, WinUSB inspection, exact JLS1 initialization, settling, blackout warm-up, streaming, recovery, and blackout close. USB work remains off the scheduler.

No new framing variant is added without controlled evidence contradicting the current exact packet contract.

### 4. Connection lifecycles leave Runner

The long-term boundaries are:

- `DjTransportService` for OS2L and later DJ adapters;
- `ControllerService` for MIDI/HID/Control One control surfaces;
- `OutputRouter` for immutable universe frames and output adapter sessions;
- `ConnectionCoordinator` for desired/saved/applied/active state, diff/apply, reconnect, and status;
- `Runner` for deterministic sync, layers, rendering, and bounded commands/state.

Extraction is incremental and may not delay raw hardware proof.

### 5. Connection state is truthful and persistent

Save & Apply means parse/validate, atomically persist desired project settings, compute a connection diff, safely zero/close affected outputs, apply affected services/adapters, and publish one explicit result. Saved-but-failed settings remain distinguishable from active settings.

The blocking Win32 Connections action must remain visibly accessible at supported window sizes and DPI while the future skin runtime is built.

### 6. OS2L startup is a transport concern

The OS2L listener should exist independently of Show start when enabled. EmberLights supplies the direct-IP startup action `wait 100ms & os2l_button 'blackout' off` so VirtualDJ initiates the client connection without a manual performance-pad press. DNS-SD discovery is a follow-up, not a Micro blocker.

### 7. Static Look Toggle/Hold uses one shared engine

Static Looks continue to occupy the existing `EventMoment` layer above Autoloops. Toggle and Hold are shared command/binding behaviors with activation ownership tokens. Lower Autoloops continue advancing and reappear at their current phase when the Static Look releases. UI, MIDI, Control One, keyboard, and OS2L do not implement alternate scene engines.

### 8. Control One control and DMX are separate qualifications

Control One MIDI input/feedback is a device-agnostic controller-profile path. Proprietary onboard DMX/OLED/storage requires separate clean-room adapters and evidence. Working MIDI never implies working Control One DMX.

## Consequences

- Diagnostics become more detailed but substantially more trustworthy.
- A staged migration project can validate internally while still carrying physical-verification warnings.
- The next installer must prioritize Hardware Test, frame truth, Connections, OS2L startup, and Static Look interaction over broad skin work.
- Output adapters become easier to add and test without modifying fixture semantics or Runner.
- Physical support claims require Joshua's owned-hardware evidence, not CI alone.
