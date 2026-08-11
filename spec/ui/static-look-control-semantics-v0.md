# Static Look Control Semantics v0

Status: binding behavior specification for issue #38 and the later command/state facade. The implementation may use existing native types/names internally, but every software/MIDI/controller surface must observe the same semantics.

Related:

- `../../docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`
- `../../docs/01_PRODUCT_REQUIREMENTS.md`
- `../../docs/03_ARCHITECTURE.md`
- `command-state-skin-contract-v0.md`
- `registry/README.md`

## Objective

Static Looks are sparse EventMoment-layer content above Autoloops and Track Scripts. A Look can be activated from Live UI, MIDI, Control One, keyboard, future remote control, or a validated Moment. Toggle and Hold are input/control behaviors over one authoritative activation model—not separate playback engines.

Requirements:

- identical result from every surface;
- explicit ownership so one release does not clear another source incorrectly;
- `RELEASE`, `SET`, and `FORCE_ZERO` property semantics remain intact;
- background Autoloop/Track layers continue while covered;
- clear/release returns naturally to the still-running lower layer;
- active feedback is Runner-owned and available to all surfaces;
- package activation, disconnect, queue pressure, and faults resolve predictably;
- blackout/safety always outrank Static Looks.

## Layer placement

Static Look playback uses the existing EventMoment layer:

```text
Idle
Default autonomous
Track Script
Manual Autoloop
EventMoment / Static Look
Manual fixture/group override
Emergency
Safety policy
```

No UI-specific scene layer is added.

## Core model

### Look definition

A compiled Static Look contains:

```text
stable Look ID
name/category/color metadata
crossfade/release fade
sparse target/property assignments
assignment state: RELEASE | SET(value) | FORCE_ZERO
```

### Activation record

The Runner owns one current Static Look activation record in v0:

```text
lookId
activationGeneration
ownerKind
ownerId
behavior
activatedAt
fadeIn
releaseFade
status
```

Provisional enums:

```text
ownerKind: ui | keyboard | midi | controller | moment | external | test
behavior: latch | hold | explicit
status: activating | active | releasing | none
```

`ownerId` is a stable invocation/binding/control instance identity supplied by the trusted binding/command layer, not arbitrary untrusted skin text.

### Why ownership is required

Without ownership:

- releasing one held MIDI pad could clear a Look activated from software;
- duplicate Note On/Off or reconnect events could release the wrong activation;
- a skin reload could leave or clear state inconsistently;
- two controls targeting the same Look could disagree about feedback.

The Runner therefore validates release against the current activation generation/owner policy.

## Commands

The accepted implementation may expose equivalent names, but the semantic command set must cover these operations.

### `staticLook.activate`

Arguments:

```text
lookId
behavior: latch | hold | explicit
owner context supplied by invocation service
optional fade override within authored/safety limits
```

Behavior:

- validate active package and Look existence;
- allocate/increment activation generation;
- replace the current EventMoment Look atomically;
- begin authored/approved crossfade;
- preserve lower layers running underneath;
- publish authoritative activation state;
- return explicit accepted/unavailable/notFound/safetyRejected/queueFull result.

For `hold`, release authorization belongs to the owner/generation.

### `staticLook.toggle`

Arguments:

```text
lookId
owner context
optional fade override
```

Atomic scheduler behavior:

- if the current active Look is the same `lookId` and is owned by the same logical control or is a latch activation eligible for global toggle, begin release;
- otherwise activate/replace the requested Look as latch behavior;
- no UI read-then-write toggle race;
- return the resulting activation generation/state.

A surface must not implement toggle by reading `staticLook.active.id` and separately choosing Activate/Clear on the UI thread.

### `staticLook.hold.begin`

Arguments:

```text
lookId
owner context
optional fade override
```

Behavior:

- activate requested Look with `hold` behavior;
- bind the activation generation to the owner;
- duplicate begin from the same owner is idempotent/noChange where possible;
- begin from another owner replaces according to the normal single-active-Look rule and publishes a new generation.

### `staticLook.hold.end`

Arguments:

```text
owner context
optional expected lookId/activationGeneration for diagnostics and stale-event rejection
```

Behavior:

- release only when the current activation is hold-owned by the same owner and, when supplied, generation matches;
- stale release from a prior activation returns noChange/stale rather than clearing a newer Look;
- release fade restores lower layers naturally;
- a lost release event is handled by controller disconnect/timeout policy below.

### `staticLook.clear`

Arguments:

```text
optional expected lookId
authority: userClear | projectStop | packageChange | faultRecovery
optional fade override
```

Behavior:

- deliberate targetless user Clear may clear the current Static Look regardless of ordinary surface owner;
- safety/emergency/package-stop policies may clear with their own approved fade/immediate behavior;
- Clear never clears Autoloops, Track Scripts, manual fixture overrides, blackout, or safety state;
- returns noChange when no Look is active.

## Binding behaviors

### Toggle button/pad

```text
press -> staticLook.toggle(lookId)
release -> no command
feedback -> active when staticLook.active.id == lookId
```

### Hold button/pad

```text
press -> staticLook.hold.begin(lookId, owner context)
release -> staticLook.hold.end(owner context, generation)
feedback -> held-active only when owner/generation matches
```

### Explicit activate button

```text
press -> staticLook.activate(lookId, explicit/latch)
release -> no command
separate Clear control -> staticLook.clear
```

### Moment

A Moment invokes the same activation command with `ownerKind=moment`. It does not manipulate the EventMoment layer directly.

## Feedback state

Minimum authoritative state:

```text
staticLook.active.id
staticLook.active.name
staticLook.active.generation
staticLook.active.ownerKind
staticLook.active.ownerIdHash or ownerFeedbackKey
staticLook.active.behavior
staticLook.active.status
staticLook.active.transitionProgress optional
staticLook.active.activatedAt optional
```

Privacy rule: ordinary UI/controller feedback receives a non-sensitive stable feedback key, not personal device serial/path data.

### Surface feedback

- every pad/button targeting the active Look can show active content state;
- only the owner control shows `held by this control` state;
- another control targeting the same active Look shows active but not owner-held;
- selected/editing remains separate from active;
- releasing/transition state is distinct from active/selected;
- software and controller LEDs update from shared state, not local press state.

## Multiple-control scenarios

### Same Look, two toggle controls

Both display active from shared state. Either eligible toggle can clear the latched Look through the atomic toggle command. This is intentional user-global behavior for latch controls.

### Same Look, two hold controls

- A presses: A owns generation 10.
- B presses: B activates generation 11 and owns it.
- A releases stale generation 10: noChange; generation 11 remains.
- B releases generation 11: release begins.

### Hold Look replaced by toggle Look

The new toggle activation receives a new generation. The old hold release is stale and cannot clear it.

### UI reload/skin switch

The skin/control instance disappears, but the Runner activation does not change merely because presentation changed.

Policy:

- latch/explicit activation remains;
- hold activation must have an owner-liveness policy. The trusted UI binding service releases its hold before destroying the owning control where possible; a bounded owner lease/cleanup handles abnormal disappearance;
- switching skins never clears a latch Look automatically.

### Controller disconnect

For active hold owned by the disconnected controller:

- binding/controller service publishes owner loss;
- Runner/binding authority releases the hold using the expected generation;
- authored anti-snap release fade applies;
- latch activations remain unless the controller profile or safety policy explicitly defines disconnect-clear and that behavior is visible/qualified.

### MIDI Note Off lost

A hold binding may use:

- device disconnect owner cleanup;
- bounded maximum-hold lease only when explicitly configured;
- All Notes Off handling where verified;
- manual Release/Clear fallback.

Do not silently turn every Hold into a timed effect.

## Package activation and project changes

### Compatible package activation

If the same stable Look ID exists and the compiled assignments remain valid, policy may preserve the activation through generation-stamped package activation only when the compiler/Runner contract explicitly supports it.

### Missing or changed Look

- candidate package activation does not reference stale UI vectors;
- current activation is released/cleared according to package activation policy;
- state publishes exact reason;
- stale hold releases from the old package cannot affect the new package generation.

### Project stop/Runner stop

- active Static Look clears;
- owner records clear;
- feedback resets;
- outputs follow normal stop/blackout/adapter policy.

## Crossfade and lower-layer return

During activation/release:

- EventMoment values crossfade according to authored/approved policy;
- `RELEASE` never claims the property;
- `SET(value)` transitions into/out of ownership;
- `FORCE_ZERO` transitions toward explicit zero and then releases on clear;
- lower Autoloop/Track state continues advancing while covered;
- on release, the renderer resolves the current lower-layer value, not the value from activation time;
- no snap unless configured/safety-required;
- UI progress is status only and not transition authority.

## Safety interaction

- blackout/emergency remains above Static Look and non-droppable;
- intensity/strobe/movement/hazard caps apply after layer resolution;
- a Look cannot arm fog/haze/laser/spark merely by containing property assignments unless the core hazardous-effect design explicitly permits an already armed path;
- unauthorized hazardous assignments remain blocked/reported;
- Hold/Toggle cannot bypass safety by choosing a different surface.

## Queue and acknowledgement behavior

- Toggle must be one atomic command, not two ordinary queue operations;
- Hold begin/end carry owner/generation context;
- priority class is ordinary bounded live command unless safety policy elevates Clear during stop/fault;
- queue full returns explicit failure; local UI/MIDI feedback does not claim activation without authoritative state;
- controller LEDs may show pending only after acceptedPending result, then reconcile to state;
- duplicate events are idempotent where possible.

## Persistence

- Look definitions are project-authored;
- active Look, owner, behavior, generation, and transition are live-transient;
- active Look is not restored automatically on application restart;
- project may store preferred button behavior/mapping, not active ownership;
- hazard arming remains separately fail-closed.

## Command/state registry guidance

Proposed commands:

```text
staticLook.activate
staticLook.toggle
staticLook.hold.begin
staticLook.hold.end
staticLook.clear
```

Proposed states:

```text
staticLook.active.id
staticLook.active.generation
staticLook.active.ownerKind
staticLook.active.ownerFeedbackKey
staticLook.active.behavior
staticLook.active.status
staticLook.active.transitionProgress
```

Issue #31 promotes final names into the generated registry after reconciling #38 implementation evidence. `staticLook.activate` and `staticLook.clear` in the planning seed remain valid minimum operations; Toggle/Hold must not be implemented only in a skin/mapping script.

## Required tests

### Core tests

- activate and clear;
- toggle on/off atomically;
- hold begin/end;
- duplicate begin/end;
- stale generation release;
- two owners same Look;
- two owners different Looks;
- toggle replacing hold and stale release;
- clear regardless of ordinary owner;
- EventMoment precedence over Autoloop;
- lower Autoloop continues and resumes current phase;
- RELEASE/SET/FORCE_ZERO transition behavior;
- package activation compatible/missing Look;
- Runner stop/reset;
- queue full/invalid target/safety rejection;
- blackout/safety precedence.

### MIDI/controller tests

- Note On/Off Hold;
- Toggle behavior;
- duplicate Note On;
- lost/stale Note Off;
- disconnect owner cleanup;
- feedback resync after reconnect;
- multiple devices/controls targeting same Look.

### UI tests

- software Toggle and Hold use the same commands;
- selected versus active versus owner-held;
- active feedback in Default and Reference;
- switch skins while Look active;
- invalid skin fallback while Look active;
- Release All Overrides does not clear Static Look;
- Clear Look does not clear manual overrides/Autoloop/Track.

### Hardware qualification

With an exact one-fixture bench project:

1. Autoloop visibly runs.
2. Toggle Look activates over it.
3. Toggle again releases and Autoloop returns at current phase.
4. Hold press activates; release returns.
5. Software and MIDI/controller produce identical output/feedback.
6. Controller disconnect during Hold releases safely.
7. Blackout during active Look works immediately and release restores expected resolved state.

## Acceptance

1. Toggle/Hold are core command semantics, not UI-only logic.
2. EventMoment remains the one Static Look layer.
3. Ownership/generation prevents stale or foreign release.
4. Lower layers continue and return naturally.
5. Every surface receives authoritative feedback.
6. Safety/emergency remains authoritative.
7. Live activation state is never silently persisted.
8. Package/skin/controller changes behave predictably.
9. Core, MIDI, UI, and physical bench tests pass.
10. Final command/state names are published once through issue #31.
