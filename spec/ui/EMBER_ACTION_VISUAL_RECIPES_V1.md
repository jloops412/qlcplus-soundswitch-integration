# Ember Actions — Visual Recipe Catalog v1

Status: **authoring and compiler UX contract for issues #65 and #66**. Recipes are guided visual patterns that compile to validated Bindings, Ember Action graphs, registered Commands, and registered State feedback. They are not hidden native callbacks or a second action language.

## 1. Purpose

Most users should never have to construct a graph node-by-node or type Ember Action Script for ordinary controls.

The Designer and Custom Control editor should offer named recipes such as:

```text
Trigger
Toggle
Hold while pressed
Select one
Absolute fader
Relative encoder
Shift action
Long-press alternate
Result-aware action
Launch on next beat/bar
State-driven LED/progress
```

Each recipe:

- declares which layer owns each behavior;
- exposes only compatible registered commands/states/capabilities;
- generates canonical source/Action IR deterministically;
- remains fully editable in the visual graph after creation;
- has a human explanation and preview scenarios;
- cannot introduce behavior unavailable in the canonical contracts.

## 2. Responsibility boundary

| Concern | Authority |
| --- | --- |
| Raw press/release/value/encoder input | Input provider and Binding engine |
| Long press, double press, debounce | Bounded Binding gesture recognizer |
| MIDI relative dialect and high-resolution decode | Controller definition/profile |
| Soft takeover/pickup | Binding/controller profile |
| Typed input transform | Binding transform or `MapValue` Action node |
| Multiple immediate commands and result branches | Ember Action |
| Stable project target resolution | Registry-aware binding/action compiler |
| Authoritative active/selected/progress/fault state | Registered State |
| Toggle/hold ownership and stale-release protection | Registered domain Command/Runner |
| Beat/bar/phrase launch, repeat, fade, duration, return | Registered domain Command/compiled show content |
| Studio mutation transaction and Undo | Studio command/service |
| Hardware LED/ring/text feedback | Binding feedback from registered State/Action feedback |
| Safety/arming/priority/emergency authority | Core domain command and independent platform path |

A recipe must not move a concern into the wrong layer merely because it is easier to demonstrate in the editor.

## 3. Recipe metadata

Every built-in recipe declares:

```text
recipe ID/version
human label/description/category
compatible component events
compatible command interactions and realtime classes
required command/state/capability metadata
parameters and pickers
binding nodes produced
action nodes produced
feedback produced
persistence/reset scope
safety restrictions
simulation fixtures
accessibility default
migration/version behavior
```

Recipe output is canonical ordinary source. There is no private runtime `recipe` instruction after compilation unless the accepted source schema explicitly standardizes one as authoring metadata only.

## 4. Core recipes

### 4.1 Trigger one command

**Use for:** Start, Stop, Release All, launch one Autoloop, open a panel, run one safe utility.

```text
onPress -> InvokeCommand(command, typed arguments)
feedback -> registered result/state
```

Rules:

- component event and command interaction must be compatible;
- utility/Studio/Live restrictions are visible;
- repeated press behavior comes from the command, not guessed local state;
- result feedback includes Accepted, NoChange, Unavailable, MissingTarget/NotFound policy, QueueFull, SafetyRejected, and error.

### 4.2 Explicit boolean set

**Use for:** Blackout set/release, Work Light set/release, a registered setting with explicit boolean authority.

```text
onPress/onValue -> InvokeCommand *.set(active = value)
feedback.active -> registered boolean State
```

Rules:

- preferred for automation and deterministic mappings;
- emergency/priority commands retain their independent delivery path;
- the recipe cannot replace or shadow F8 Blackout;
- action composition cannot retry a rejected hazardous command.

### 4.3 Human toggle

**Use for:** Human-facing toggle controls when a registered toggle command is accepted.

```text
onPress -> InvokeCommand *.toggle(...)
feedback.active -> registered authoritative State
```

Rules:

- do not implement `not(localValue)` as domain truth;
- automated/external deterministic callers should prefer explicit set commands when available;
- unavailable or stale state cannot be guessed;
- state feedback converges even when the invocation is rejected or another surface changes it.

### 4.4 Momentary hold with ownership-safe release

**Use for:** Static Look Hold, temporary override, momentary Work Light or approved performance control.

```text
onPress   -> InvokeCommand hold/set(owner = binding.id, active = true)
onRelease -> InvokeCommand hold/release(owner = binding.id, active = false)
feedback  -> registered active owner/content State
```

Rules:

- the domain command owns stale-release protection;
- a delayed release from an old binding cannot clear a newer owner;
- skin/action source does not maintain an authoritative active flag;
- binding deactivation/unplug handling uses the registered release policy;
- hazardous holds require normal safety gates.

Current proof family:

```text
staticLook.hold
staticLook.active.id
```

Exact ownership arguments remain coordinated with #31/current native semantics.

### 4.5 Latch/select one item

**Use for:** select an exclusive Autoloop bank, choose a page mode, select a project object.

```text
onPress -> InvokeCommand selection command(target/context ID)
feedback.selected -> authoritative State or surface-local page State
```

Rules:

- domain selection uses stable IDs, never visual index unless the command explicitly defines a bounded bank/slot address;
- presentation-only page selection may be surface-local;
- changing page does not imply changing active content;
- radio-group exclusivity is domain- or surface-local authority, not repeated toggle calls.

### 4.6 Absolute fader/knob with soft takeover

**Use for:** group intensity, master intensity, approved override values.

```text
provider value
  -> binding range/resolution/dead-zone/soft-takeover
  -> optional MapValue transform
  -> InvokeCommand *.set(value)
feedback.value -> registered authoritative State
onDeactivate/release -> registered release command when ownership requires it
```

Rules:

- MIDI resolution and pickup remain in the controller binding;
- action receives normalized typed value after pickup;
- no paint/UI refresh rate becomes command cadence authority;
- rate limiting/coalescing is bounded and declared;
- feedback uses actual state, not last submitted value.

### 4.7 Relative encoder

**Use for:** step bank/page, adjust intensity/rate/size, nudge values.

```text
raw MIDI/HID dialect
  -> controller definition/profile normalization
  -> onEncoderStep(delta)
  -> MapValue/Step or registered relative command
```

Rules:

- source dialect is not embedded in the Ember Action;
- acceleration is a bounded binding policy;
- absolute domain limits/clamping are declared by command metadata;
- selected target and unit are typed.

### 4.8 Shift/modifier alternate action

**Use for:** one control performs an alternate function while Shift is held.

```text
binding modifier/layer State
  -> choose primary or alternate entry point/action
```

Rules:

- modifier recognition and release are Binding/controller-profile concerns;
- modifier state is bounded surface/controller-local, not project lighting state;
- both branches validate independently;
- an alternate cannot hide a hazardous command behind an ambiguous gesture;
- software and controller UI display the active layer where practical.

### 4.9 Long-press alternate

**Use for:** explicit secondary operation such as opening details or selecting an advanced mode.

```text
binding recognizes long press
  -> onLongPress entry point
ordinary press remains immediate unless the accepted gesture policy explicitly delays it
```

Rules:

- do not wait inside an Action graph;
- critical ordinary press behavior is not delayed by default;
- destructive/hazardous alternates require explicit review and safety gate;
- always provide a discoverable software path for important functionality.

### 4.10 Double-press alternate

**Use for:** bounded optional shortcut where accidental activation is low risk.

Rules mirror long press:

- Binding recognizes the gesture;
- Action graph receives `onDoublePress`;
- no Action timer/sleep;
- do not assign emergency/hazardous behavior without explicit approved policy;
- single-press behavior and timing remain predictable.

### 4.11 Contextual repeater item

**Use for:** Autoloop pad, Static Look tile, fixture/group row, asset list item.

```text
bounded registered collection/native component context
  -> stable context item ID/address/properties
  -> parameterized binding/action
```

Rules:

- one template, not thousands of authored duplicate controls;
- context schema is registered and bounded;
- stable identity is not visual index/name;
- missing/replaced content becomes unavailable/relinkable;
- large lists use approved virtualized native components;
- active/selected/queued/progress feedback uses registered State/context.

### 4.12 Result-aware action

**Use for:** reconnect, validation, async utility start, safe fallback choice.

```text
result = InvokeCommand(...)
OnResult result {
  Accepted/NoChange/StartedAsync -> Return result
  Unavailable/MissingTarget      -> user-visible bounded feedback
  QueueFull/SafetyRejected       -> preserve and surface exact result
  default                        -> diagnostic result
}
```

Rules:

- SafetyRejected cannot be rewritten as success;
- QueueFull is not silently retried in a loop;
- fallback command must pass its own availability/safety validation;
- async operations expose a real operation ID/progress/cancel contract.

### 4.13 Immediate bounded sequence

**Use for:** small composition of independently valid commands where ordering is meaningful and no musical delay is required.

```text
Sequence [command A, command B, ...]
```

Rules:

- maximum nodes/commands apply;
- policy for non-Accepted results is explicit;
- no atomicity is implied for Runner commands;
- Studio transaction is allowed only when every command/service supports the declared transaction;
- a new domain atomic operation should become one registered command rather than a fragile compensating macro.

### 4.14 Bounded independent parallel submission

**Use for:** rare groups of commands explicitly marked parallel-compatible.

Rules:

- means independent bounded submission, not threads, simultaneity, or atomicity;
- priority/emergency commands are excluded unless specifically approved;
- mixed Studio/Runner/blocking classes are rejected;
- result aggregation is deterministic;
- do not use to conceal ordering requirements.

### 4.15 Domain-scheduled launch/transition

**Use for:** next beat/bar/phrase launch, run for N beats, one-shot natural return, Track Duration, fade/transition.

```text
onPress -> InvokeCommand domain.operation(
  quantize = nextBeat/nextBar/...,
  repeat = once/infinite/trackDuration,
  mode = overlay/replace,
  transition = typed policy)
feedback -> queued/active/progress/repeat/source/result State
```

Rules:

- Action graph contains no `wait`, sleep, beat loop, or UI timer;
- Runner/domain owns schedule, phase, completion, cancellation, ownership, and return;
- if the required semantic command does not exist, the recipe is unavailable and creates a backlog/registry requirement rather than simulating it locally;
- exact names/states for Autoloops V2 are coordinated with #59.

### 4.16 Capability-aware optional control

**Use for:** show a control only when the current project/adapter/module supports it.

```text
required capability metadata
availability feedback
hide | disable-with-reason | substitute approved component
```

Rules:

- absence does not produce a fake/dead control;
- required capability failure blocks activation when the skin cannot operate safely;
- optional capability behavior is declared;
- reason is visible in Explorer/Designer/diagnostics;
- a skin cannot manufacture capability state.

### 4.17 Page navigation and local display mode

**Use for:** Autoloops/Static Looks/Moments/Overrides page, custom pad page, local view filter.

Rules:

- presentation navigation is view/surface-local unless a registered shared command is intentionally required;
- current page is not active content state;
- active content remains visible globally when its page is hidden;
- page selection may persist app-locally through approved workspace service;
- changing page never rewrites project content.

### 4.18 State-driven LED/ring/text/progress feedback

**Use for:** software state styles and hardware output.

```text
registered State + bounded expression
  -> active/selected/queued/progress/value/warning/fault
  -> component style/text or controller output mapping
```

Rules:

- authoritative state, never last-command guess;
- update rate respects State metadata and device capacity;
- hardware range/color/text capabilities come from controller profile;
- status/safety meaning is not encoded by color alone;
- hidden surfaces throttle/unsubscribe high-frequency feedback;
- missing feedback capability degrades explicitly.

## 5. Safety-specific recipes

### 5.1 Emergency Blackout control

The Designer may place a platform-approved Blackout component/binding, but:

- F8 and core priority delivery remain independent;
- mandatory reachability validation applies to every Live variant/overlay;
- the recipe cannot wrap Blackout inside arbitrary sequence/parallel/retry behavior;
- active Blackout state remains globally visible and cannot be concealed;
- imported packages cannot replace the implementation.

### 5.2 Hazard arm/disarm

- uses registered hazard command and safety gate;
- arming is explicit and does not silently persist;
- no automatic retry or activation lifecycle side effect;
- disarm remains broadly reachable;
- simulation and real-output Test are visually distinct.

### 5.3 Confirmation

A confirmation dialog/gesture is presentation policy, not the safety gate itself. A confirmed action still passes the core command safety/availability checks. Routine Live operations should avoid blocking modals; hazardous operations use approved interlocks.

## 6. Recipe anti-patterns

The Designer/compiler must diagnose or reject:

```text
UI timer repeatedly invoking lighting commands
local boolean pretending to be active domain state
toggle implemented by negating stale UI feedback
hard-coded fixture/channel/DMX writes
MIDI message parsing inside an Action
visual index/name used as stable content target
sleep/wait for beat or fade
onActivate starting output-critical content by default
state change triggering an unbounded action loop
SafetyRejected converted to Accepted
QueueFull hidden or retried indefinitely
multiple Runner commands described as atomic
hidden proprietary action string inside a component
one skin defining a private command unavailable elsewhere
```

## 7. Recipe output and migration

Built-in recipes have stable IDs and versions. An authored control stores canonical Binding/Action source plus optional recipe provenance:

```text
createdFromRecipeId
createdFromRecipeVersion
recipeParametersAtCreation
```

Runtime meaning comes from canonical source/IR, not the recipe ID. Users can edit generated graphs freely. A recipe update does not silently rewrite existing controls; the Designer may offer a diff/migration preview when safe.

Imported source actions map to recipes only when semantics are proven. Otherwise they map to explicit graph nodes, remain opaque, or are unsupported.

## 8. Golden recipe journeys

1. Static Look Toggle with authoritative active state.
2. Static Look Hold/release where stale release cannot clear a newer Look.
3. Autoloop pad using stable bank/slot/content context with selected/queued/active/progress/repeat feedback.
4. Group-intensity fader with MIDI Learn, soft takeover, release ownership, and LED/value feedback.
5. Exclusive-bank selection plus All Banks restore.
6. Release All from software and controller with identical result/state.
7. Output reconnect with StartedAsync/progress/fault feedback.
8. Shifted pad primary/alternate action with visible layer state.
9. Long-press details action that does not delay ordinary trigger.
10. Domain-scheduled one-shot Autoloop overlay and natural return without Action timers.
11. Optional capability control disabled with exact reason.
12. Page navigation that preserves globally visible active content.
13. Malformed/missing target recipe output rejected without displacing current surface.
14. Same recipe-derived action from software, keyboard, MIDI/controller, and direct test produces equivalent registered command/results.

## 9. V1 acceptance

- Golden-path customization uses recipes without script/JSON editing.
- Every recipe compiles to ordinary canonical Binding/Action source and deterministic IR.
- Binding, Action, domain timing, state feedback, persistence, safety, and hardware concerns remain correctly separated.
- Recipes cannot produce arbitrary code, direct device/output access, or UI-owned show timing.
- Missing command/state/capability requirements produce actionable registry/backlog diagnostics.
- Visual graph and expert text can represent every generated Action semantic exactly.
- Recipe migration is explicit and never silently rewrites working user artifacts.
- Software and controller journeys remain equivalent and active DMX remains uninterrupted during editing/activation/failure.
