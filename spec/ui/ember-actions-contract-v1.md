# Ember Actions Contract v1

Status: **provisional binding contract for implementation planning**. It extends `command-state-skin-contract-v0.md` and `user-customization-and-action-composition-v0.md` without permitting arbitrary skin code.

## 1. Purpose

Ember Actions are reusable, typed, bounded compositions of registered EmberLights commands, state predicates, input transforms, result branches, and feedback outputs.

They provide one shared customization vocabulary for:

- skin controls;
- custom buttons and pad pages;
- keyboard shortcuts;
- MIDI/HID/controller mappings;
- DJ-command adapters;
- future authenticated remote surfaces;
- reusable user action packs.

An Ember Action is never a replacement for:

- lighting-domain behavior;
- the layer resolver;
- Studio document services;
- project compilation;
- Runner musical timing;
- adapter I/O;
- safety gates;
- persistence authority.

## 2. Architectural flow

```text
Surface event/value/context
  -> validated Binding
  -> compiled Ember Action entry point
  -> bounded Action executor
  -> registered UiCommandFacade/Studio command invocation
  -> authoritative domain result
  -> registered State snapshot/result feedback
```

The action executor runs on an approved UI/control service. It never runs on the DMX scheduler and never writes output frames directly.

## 3. Canonical artifact

Provisional extension:

```text
.emberaction
```

One file may contain one action. A deterministic action-pack container may contain multiple actions and shared localization/metadata later.

Conceptual canonical form:

```json
{
  "schemaVersion": 1,
  "id": "com.example.action.first-dance-hold",
  "version": "1.0.0",
  "label": "First Dance Hold",
  "description": "Holds a selected Static Look while the control is pressed.",
  "author": "Example",
  "provenance": {
    "kind": "local",
    "sourceId": null,
    "sourceHash": null
  },
  "compatibility": {
    "minimumAppVersion": "0.1.0",
    "commandRegistry": ">=1 <2",
    "stateRegistry": ">=1 <2",
    "capabilityRegistry": ">=1 <2"
  },
  "parameters": {
    "lookId": {
      "type": "stableId",
      "targetKind": "staticLook",
      "required": true
    }
  },
  "requires": {
    "commands": ["staticLook.activate", "staticLook.clear"],
    "states": ["staticLook.active.id"],
    "capabilities": ["content.staticLooks"]
  },
  "entryPoints": {
    "onPress": "node.activate",
    "onRelease": "node.clear"
  },
  "nodes": {},
  "feedback": {},
  "limits": {},
  "contentHash": "sha256:..."
}
```

The exact schema is `schema/ember-action.schema.json`. Canonical serialization order and hash rules are implementation deliverables.

## 4. Identity and versioning

### Action IDs

Use reverse-domain or owner-scoped stable IDs:

```text
com.emberlights.action.release-all
com.example.wedding.first-dance
local.<generated-uuid>
```

Display names are not identifiers.

### Action version

Action content uses semantic versions. An installed update with the same ID may coexist by version until the owner chooses migration/replacement policy.

### Registry compatibility

Actions declare the registry generations they were compiled against and exact required IDs. Validation distinguishes:

- compatible unchanged;
- compatible additive;
- deprecated with safe replacement;
- missing optional capability;
- incompatible argument/state/component contract;
- removed without safe replacement;
- unsupported future schema.

## 5. Type system

Every parameter, local value, state read, command argument, result, and feedback output is typed.

Initial value kinds:

| Type | Rules |
| --- | --- |
| `boolean` | `true` or `false`. |
| `integer` | Signed/unsigned metadata, explicit min/max. |
| `number` | Finite only, explicit min/max and optional unit. NaN/Infinity rejected. |
| `enum` | Values from an approved schema or registry definition. |
| `string` | UTF-8, bounded length, no implicit path/URL meaning. |
| `stableId` | Stable project/app object ID with declared `targetKind`. |
| `semanticRole` | Registry-defined role resolved only where the command supports it. |
| `color` | Linear/sRGB representation declared by schema; presentation color is not a safety state. |
| `duration` | Bounded UI/control duration only; musical duration uses domain command types. |
| `object` | Approved schema reference only. |
| `list` | Bounded items of an approved type/schema. |
| `result` | Registered invocation-result enum plus optional operation/diagnostic data. |
| `void` | No value. |

No implicit string-to-ID, string-to-number, enum-to-integer, or display-label-to-command conversion occurs.

### Units

Numeric metadata may use:

```text
normalized
percent
bpm
beats
bars
seconds
milliseconds
degrees
dmx8
dmx16
```

A transform must declare source and target units when they differ. Lighting/music timing units can only reach commands that explicitly accept them.

## 6. Context sources

An action may read only declared context supplied by the binding/component:

- surface input value;
- press/release/gesture metadata;
- current item in a bounded component repeater;
- approved page/bank/slot context;
- selected stable project object;
- declared modifier/layer state;
- action parameters;
- registered state snapshot values.

It cannot inspect arbitrary application objects, memory, widget internals, filesystem paths, device handles, adapter packets, or DMX buffers.

## 7. Entry points

V1 supports:

| Entry point | Typical source |
| --- | --- |
| `onPress` | Button/pad/key/MIDI press. |
| `onRelease` | Button/pad/key/MIDI release. |
| `onValue` | Fader/knob/absolute input. |
| `onEncoderStep` | Relative encoder step. |
| `onLongPress` | Binding-recognized long press. |
| `onDoublePress` | Binding-recognized double press. |
| `onActivate` | Surface/page/action activation where explicitly safe. |
| `onDeactivate` | Surface/page/action deactivation where explicitly safe. |

The binding engine owns debounce, long-press, double-press, encoder dialect, soft takeover, and press/release recognition. The action does not block while waiting for another event.

V1 does not allow arbitrary registered-state changes to fire actions. State subscriptions drive feedback. A later event-trigger subsystem requires separate cycle/rate/ownership qualification.

## 8. Node model

All nodes have:

- stable node ID unique within the action;
- node kind;
- typed inputs;
- typed outputs;
- deterministic child/branch order;
- optional source-location metadata;
- optional owner-facing label/comment;
- no hidden side effects.

### 8.1 `InvokeCommand`

Invokes one registered command.

Required validation:

- command exists and is supported;
- action/surface scope is permitted;
- arguments exist, types/ranges/units match, and required targets resolve;
- interaction and realtime class are compatible;
- capability/availability predicate is satisfiable;
- safety gate remains core-owned;
- persistence/Undo behavior is not contradicted;
- deprecated command handling is explicit.

Output: typed invocation result.

### 8.2 `Sequence`

Executes child nodes in listed order.

Policy options:

```text
continue
stopOnRejected
stopOnError
stopOnAnyNonAccepted
```

Default is `stopOnError`; safety rejection is surfaced and never swallowed.

### 8.3 `Parallel`

Submits a bounded set of independently safe child operations.

Rules:

- maximum child count is enforced;
- child commands must declare parallel compatibility;
- no ordering or atomicity is implied;
- emergency/priority commands are excluded unless a specific single-step contract permits them;
- mixed Studio transaction and Runner command children are rejected;
- result aggregation is deterministic.

### 8.4 `If`

Evaluates one approved side-effect-free predicate and chooses `then` or optional `else`.

### 8.5 `Switch`

Matches an enum/integer/string value against bounded constant cases and optional default. Dynamic pattern matching and regular expressions are excluded in V1.

### 8.6 `ReadState`

Reads one registered state value from the coherent snapshot available at action start or an explicitly allowed refreshed snapshot boundary.

The default is snapshot-at-start. Actions cannot busy-poll or request state faster than its update class.

### 8.7 `Let`

Defines an immutable invocation-local typed value from a literal, parameter, context value, state value, transform, or prior node output.

### 8.8 `MapValue`

Applies one validated transform chain:

- clamp;
- scale/range map;
- invert;
- dead zone;
- linear or registered bounded response curve;
- quantize/round;
- enum/value map;
- relative encoder conversion;
- unit conversion approved by metadata.

The transform is deterministic and finite.

### 8.9 `InvokeAction`

Invokes another installed/embedded compatible action with typed arguments.

Rules:

- dependency resolves at compile time;
- action dependency graph is acyclic;
- call depth and referenced-action count are bounded;
- required command/state/capability sets are flattened for validation;
- embedded and external dependency version policy is explicit;
- imported action cannot silently bind to a different action with the same display name.

### 8.10 `OnResult`

Branches on a prior command/action result.

Allowed cases include:

```text
Accepted
NoChange
Unavailable
InvalidArguments
MissingTarget
ValidationFailed
QueueFull
SafetyRejected
Unsupported
Cancelled
StartedAsync
InternalError
```

A branch may improve user feedback or choose a safe fallback command. It cannot reinterpret SafetyRejected as success or bypass the same safety gate through an alternate raw path.

### 8.11 `Return`

Ends the current entry point with a typed result and optional bounded feedback values.

## 9. Predicates and expressions

Predicates are declarative, side-effect-free, and compiled.

Allowed operations:

- boolean `and`, `or`, `not`;
- equality/inequality;
- numeric comparison;
- null/availability checks;
- enum matching;
- bounded arithmetic;
- clamp/map/round;
- bounded string equality/selection/formatting;
- conditional selection;
- bounded static-map lookup;
- approved collection membership for bounded collections.

Prohibited:

- command invocation from an expression;
- assignment/mutation;
- loops or recursion;
- random values;
- current time as lighting authority;
- filesystem/network/device access;
- reflection/dynamic symbol lookup;
- regex in V1;
- user-defined functions outside validated reusable actions;
- unbounded string/list construction.

Each compiled expression has operation, stack, and output-size budgets.

## 10. Local and presentation state

### Invocation local

`Let` values exist only for one event invocation and are immutable.

### Surface local

A binding/action may declare small typed surface-local state for:

- selected custom page;
- modifier/layer selection;
- local display mode;
- local alternate bank view;
- soft-takeover pickup state;
- temporary editor preview state.

Rules:

- it is never authoritative domain state;
- it cannot shadow registered safety/output/content facts;
- scope and reset behavior are explicit;
- persistence, if allowed, is app-local and schema-bound;
- size/count are strictly limited;
- controller and software surfaces use shared named state only when intentionally configured.

There is no unrestricted global-variable namespace.

## 11. Timing and scheduling

### Input timing

The binding engine may use bounded timers for:

- debounce;
- long press;
- double press;
- hold/release recognition;
- encoder acceleration where approved;
- soft takeover.

This timing interprets input only.

### Show timing

The following require registered domain commands/compiled content:

- next beat/bar/phrase scheduling;
- musical-duration holds;
- fades/transitions;
- repeats;
- timed release;
- return to Track Script/Autoloop at a musical boundary;
- multi-step show sequences;
- event automation.

An action passes typed timing policy to the domain and observes returned state/progress. It does not sleep, spin, schedule DMX frames, or retain a UI timer as authority.

## 12. Realtime classes and mixed operations

The compiler derives the strongest class from all reachable nodes:

```text
viewLocal
studioMutation
runnerCommand
runnerPriority
utilityAsync
blockingForbiddenLive
```

Rules:

- `blockingForbiddenLive` actions cannot bind to normal Live controls;
- a `utilityAsync` operation returns `StartedAsync(operationId)` and runs in an approved worker/service;
- Studio mutations can opt into one transaction only when all commands are transaction-compatible;
- Runner commands remain independently authoritative unless a dedicated registered aggregate command exists;
- priority/emergency commands preserve their direct delivery semantics and cannot be buried in a general macro;
- an action with incompatible class mixing is rejected with exact node diagnostics.

## 13. Transactions and rollback

### Studio

An action may declare one Studio transaction boundary.

Requirements:

- every mutation is undoable or explicitly compatible with the transaction;
- validation completes before commit;
- failure rolls back the document transaction;
- file/import/export utilities are not falsely presented as transactional unless the service provides it;
- action diagnostics identify the failed command/node.

### Runner/Live

No general rollback is promised after accepted Runner commands. When the product needs atomic multi-property or musical behavior, implement a domain command that validates and schedules that behavior as one operation.

An `OnResult` branch may submit explicit safe compensating commands, but the UI must not claim atomicity.

## 14. Asynchronous operations

V1 action graphs may start registered async utilities, but do not suspend an ordinary Live action waiting indefinitely.

Async contract includes:

- operation ID;
- operation state/progress registered states;
- cancellation command where supported;
- completion/failure diagnostics;
- bounded owner notification;
- no continuation on the DMX scheduler;
- no implicit replay after restart.

A later continuation node requires explicit timeout/cancellation/restart semantics and separate qualification.

## 15. Feedback outputs

An action may publish named computed feedback values for its bound surface:

```text
active
selected
queued
available
warning
fault
progress
value
primaryLabel
secondaryLabel
iconToken
accentToken
controllerColor
```

Feedback values derive only from registered state, surface-local state, parameters, and bounded expressions. They cannot conceal mandatory faults or replace safety state with decorative color only.

Bindings/components may use action feedback and direct state subscriptions together. All dependencies are inspectable and count against subscription/expression budgets.

## 16. Safety restrictions

Actions cannot:

- write `output.blackout` or any state directly;
- intercept or disable F8 emergency Blackout;
- bypass command safety gates or availability;
- persist hazard arming;
- retry rejected hazardous commands automatically;
- call raw adapter/device APIs;
- generate DMX/property output outside registered commands;
- conceal SafetyRejected, QueueFull, or critical fault results;
- invoke a deprecated emergency command through an alias without explicit compatibility approval;
- use loops/recursion/repeated event triggers to flood a command queue.

The compiler performs mandatory-control reachability and emergency-command-use checks at package activation.

## 17. Resource limits

Provisional per-action limits:

| Resource | V1 planning limit |
| --- | ---: |
| normalized action bytes | 32 KiB |
| nodes | 64 |
| graph branch depth | 8 |
| referenced actions | 8 |
| action call depth | 8 |
| state reads | 16 |
| immutable locals | 16 |
| parameters | 16 |
| feedback outputs | 16 |
| parallel children | 8 |
| command invocations per entry point | 32 |
| expression source per expression | 2 KiB |
| compiled expression operations | implementation-bounded |
| compiled stack depth | implementation-bounded |

Package/overlay totals are separately bounded. Limits may be reduced after measurement. Increases require the existing resource-limit review process.

## 18. Validation and compilation

Compilation pipeline:

```text
Parse bounded source
  -> schema validate
  -> canonicalize IDs/order/types
  -> resolve action dependencies
  -> resolve command/state/capability definitions
  -> type and unit check
  -> realtime/safety/persistence/transaction check
  -> detect cycles and resource-limit violations
  -> compile predicates/transforms
  -> flatten required dependency manifest
  -> produce immutable Action IR
  -> compute deterministic digest
  -> execute fixture/simulation smoke tests
  -> cache by source + registry + compiler versions
```

Activation is transactional. Invalid new actions are unavailable; invalid updates retain the currently active valid version where policy permits.

## 19. Action IR

The runtime IR must be:

- immutable;
- allocation-bounded;
- source-language independent;
- stable enough for cached artifacts but regenerated when compiler/registry versions change;
- inspectable for diagnostics and required-capability summaries;
- free of raw command/state name lookups in the hot dispatch path where generated IDs are available;
- free of source parsing, dynamic code generation, and reflection at runtime;
- executed outside the DMX scheduler.

The serialized IR format is not automatically a public compatibility promise. Source/canonical action schema remains authoritative unless a future portable bytecode version is explicitly standardized.

## 20. Expert text representation

The optional Ember Action Script grammar must satisfy:

- one-to-one representation of canonical graph semantics;
- no features unavailable in the visual graph/IR;
- explicit command/action IDs or registry-resolved symbols;
- named typed arguments;
- deterministic formatter;
- source-range diagnostics;
- registry-aware completion and documentation;
- visual-to-text-to-visual round-trip tests;
- no runtime `eval`;
- no dynamic imports;
- no hidden variable scope;
- no ambiguous precedence left undocumented.

Text source may be included for author convenience, but normalized canonical data and digest determine runtime meaning.

## 21. Examples

### 21.1 Static Look hold

```text
onPress:
  InvokeCommand staticLook.activate(lookId = parameter.lookId,
                                    ownership = "hold",
                                    owner = binding.id)

onRelease:
  InvokeCommand staticLook.clear(owner = binding.id)

feedback.active:
  state.staticLook.active.id == parameter.lookId
```

Exact ownership arguments remain subordinate to issue #31/#38 accepted commands.

### 21.2 One-shot Autoloop over a scripted track

```text
onPress:
  InvokeCommand autoloop.launch(
    address = context.autoloopAddress,
    mode = "oneShotOverlay",
    quantize = "nextBeat")

feedback.active:
  state.autoloop.active.id == context.id
feedback.progress:
  state.autoloop.active.id == context.id
    ? state.autoloop.active.progress
    : 0
```

Runner owns quantization, duration, and return behavior.

### 21.3 Group intensity fader

```text
onValue:
  MapValue input.value from [0,1] to [0,1], clamp
  InvokeCommand group.override.property.set(
    groupId = parameter.groupId,
    property = "intensity",
    value = mappedValue)

onDeactivate:
  InvokeCommand group.override.property.release(
    groupId = parameter.groupId,
    property = "intensity",
    owner = binding.id)
```

Soft takeover remains a binding/profile concern.

### 21.4 Safe result feedback

```text
onPress:
  result = InvokeCommand connection.output.reconnect(outputId = parameter.outputId)
  OnResult result {
    Accepted | StartedAsync -> Return Accepted
    Unavailable             -> Return Unavailable("Output is disabled")
    SafetyRejected          -> Return SafetyRejected
    *                       -> Return result
  }
```

## 22. Import/export

Imported actions are untrusted.

Import preview reports:

- identity/version/provenance/hash;
- required registries/capabilities/targets;
- resolved/deprecated/replaced/missing IDs;
- exact type incompatibilities;
- action dependencies;
- safety/realtime classification;
- resource usage;
- source migration status;
- conflicts with installed action IDs/versions.

Install options include side-by-side version, local copy/fork, or explicit compatible update. Current valid actions are not overwritten silently.

Export discloses local/project scope, stable target dependencies, bundled assets/localization, licenses, and machine-local data. Paths/serials/secrets/device identity are redacted by default.

## 23. Required tests

- schema positive/negative fixtures;
- deterministic canonicalization and hash;
- every node type and type/unit edge case;
- command/state/capability resolution;
- unknown/deprecated/replaced/removed IDs;
- missing and relinked project targets;
- graph/action dependency cycles;
- depth/node/size/instruction/stack/queue limits;
- expression failure safe defaults;
- Studio transaction commit/rollback;
- Runner command ordering and explicit non-atomic behavior;
- priority/emergency restrictions;
- async start/cancel/progress/failure;
- visual/text/canonical round-trip;
- malformed/fuzzed/untrusted input;
- repeated activation/update/failure;
- action dispatch latency and memory;
- scheduler jitter under dense UI/action load;
- software/keyboard/MIDI/controller equivalence;
- DMX continuity while editing, compiling, installing, activating, and failing actions.

## 24. V1 acceptance

1. One canonical action produces identical registered command invocations from every permitted surface.
2. Visual graph and expert text round-trip to identical canonical IR/digest.
3. Arguments, state reads, capabilities, targets, realtime classes, persistence, transactions, and safety are validated before activation.
4. No action executes arbitrary code or accesses devices/files/network directly.
5. No action owns musical/show timing or runs on the DMX scheduler.
6. Cycles, recursion, flooding, oversized graphs, and incompatible mixed operations are rejected.
7. Invocation results and feedback are typed, visible, and consistent.
8. Registry deprecations and compatible replacements produce explicit migration results.
9. Invalid updates preserve the current valid action/surface where possible.
10. Performance and DMX continuity remain inside the approved release envelope.
