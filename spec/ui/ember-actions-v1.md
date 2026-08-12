# Ember Actions v1 — Typed Action, Binding, and Composition Contract

Status: **proposed normative contract** for the skin-platform implementation program. It extends `command-state-skin-contract-v0.md`, `binding-definition.schema.json`, and `user-customization-and-action-composition-v0.md` without changing current Runner semantics.

## 1. Purpose

Ember Actions is EmberLights' answer to the useful role served by VDJScript: one action vocabulary that can be used by software controls, custom pages, keyboard, MIDI/HID/controller profiles, DJ commands, tests, and future remote surfaces.

It is intentionally not a general-purpose scripting language.

The system must be:

- visually authorable;
- strongly typed;
- deterministic;
- bounded in memory and execution;
- inspectable and testable;
- shared by every control surface;
- compatible with a lean Perform process;
- unable to bypass domain validation or safety;
- capable of optional expert text authoring without creating different semantics.

## 2. Canonical model

The canonical representation is a versioned typed action graph/AST.

```text
Visual Action Editor ----┐
                         |
JSON package source -----+--> canonical Ember Action Graph --> compiled instructions
                         |
Expert text projection --┘
```

Rules:

1. The graph is authoritative, not the text syntax.
2. Visual and text forms must round-trip without semantic loss before expert text editing is released.
3. Perform executes only the validated compiled representation.
4. Runtime never evaluates arbitrary source strings.
5. A direct command binding is syntactic sugar for a one-node `Invoke` action.
6. Feedback/state styling is separate from action execution.

## 3. Terminology

### Command

A stable registered product/domain operation such as `staticLook.toggle`, `autoloop.launch`, `override.releaseAll`, or `view.panel.toggle`.

### State

A registered observable value published by the app, document services, connection services, or Runner.

### Capability

A registered fact describing whether a platform, adapter, component, command variant, or data source is supported and currently usable.

### Binding

Maps one source event—skin, keyboard, MIDI, HID, DJ command, external adapter, or test—to one command or Ember Action.

### Action program

A named bounded graph composed only of approved nodes, registered commands, typed values, state/capability predicates, and reusable action references.

### Surface action

An immediate control-plane action invoked by a UI, keyboard, MIDI, HID, or DJ event. It may request domain work but is not a lighting timing engine.

### Domain plan

A precompiled Runner-owned object for musical quantization, fades, transitions, timed holds, cue sequences, Moments, Autoloops, or Track Scripts. A skin invokes a domain plan through a registered command; it does not simulate the plan with waits.

## 4. Execution classes

Every command declares one execution class. The action compiler derives the program's legal execution class from its nodes.

| Class | Meaning | Action rules |
| --- | --- | --- |
| `viewLocal` | UI navigation/presentation only | May compose with other view-local actions |
| `studioMutation` | Authoring document mutation | May use an explicit Studio transaction and Undo contract |
| `runnerCommand` | Bounded command posted to Runner-owned state | Immediate request; no UI timing authority |
| `runnerPriority` | Independent emergency/priority route | Direct one-step binding only; excluded from general graphs |
| `domainPlanRequest` | Request to launch a precompiled Moment/Autoloop/TrackScript/transition | Timing remains domain-owned |
| `utilityAsync` | File/import/analysis/validation work | Not a routine Perform pad action; returns operation ID |
| `blockingForbiddenLive` | Operation disallowed while Perform is active | Compiler/editor exposes exact restriction |

An action cannot claim atomicity across execution classes. When users need a single atomic behavior, the domain must expose one registered command that owns it.

## 5. Type system

### 5.1 Primitive types

```text
bool
int32
uint32
number
string (bounded)
enum<T>
optional<T>
```

### 5.2 Semantic scalar types

```text
normalized       0.0 ... 1.0
percentage       0 ... 100
bpm              bounded by command schema
beats
bars
seconds          presentation/domain argument only; never UI sleep
milliseconds     bounded configuration only
colorRgb
colorRgba
universe
channel
bankIndex
slotIndex
```

### 5.3 Stable reference types

```text
ProjectId
VenueId
FixtureId
GroupId
LookId
AutoloopId
AutoloopAddress
TrackScriptId
MomentId
PaletteId
PositionId
PropertyId
AdapterId
ControllerProfileId
PanelId
ComponentId
ActionId
```

Stable references are not interchangeable with arbitrary strings. The editor uses typed pickers and preserves missing references for repair.

### 5.4 Value sources

A typed argument may come from:

- a validated literal;
- the normalized input event;
- the current component/data context;
- a registered state snapshot;
- a declared capability;
- a compile-time stable target reference;
- a previous command result within an explicit result branch.

No argument may read arbitrary memory, project JSON paths, filesystem paths, environment variables, device handles, or renderer internals.

### 5.5 No implicit coercion

- IDs do not coerce to strings or to other ID types.
- numbers do not silently become booleans.
- enum values require exact registered members.
- normalized values clamp only when the binding explicitly declares a clamp transform.
- missing optionals require an explicit default or branch.
- incompatible command signature changes require migration, not runtime guessing.

## 6. Input event model

A binding normalizes source-specific input into one of:

```text
press
release
change(value)
delta(value)
doublePress
longPress
repeatTick
focus
blur
```

Input context may include, when applicable:

```text
value             normalized absolute value
rawValue          bounded source value for diagnostics only
velocity
pressure
delta
direction
isPress
isRelease
sourceKind
modifier/layer
controlId
page/context IDs
```

### 6.1 Gesture policy

A control declares one policy:

- `eagerPress`: press fires immediately; a later long/double event may also fire if configured;
- `deferredPress`: ordinary press waits for the bounded gesture window;
- `gestureOnly`: no ordinary press action.

`deferredPress` is prohibited for emergency/priority actions and discouraged for gig-critical one-touch controls.

### 6.2 Repeat

There is no action-language loop. A platform input recognizer may emit bounded `repeatTick` events only when:

- the command/action is marked repeat-compatible;
- rate and maximum duration meet platform limits;
- emission stops on release, focus loss, disconnect, workspace change, or fault;
- emergency and hazardous commands are excluded.

## 7. Program definition

Conceptual source form:

```json
{
  "schemaVersion": 1,
  "id": "user.wedding.firstDance",
  "version": "1.0.0",
  "labelKey": "action.firstDance",
  "allowedEvents": ["press"],
  "workspace": ["live"],
  "requiredCommands": ["staticLook.toggle"],
  "requiredStates": ["runner.state", "staticLook.active.id"],
  "requiredCapabilities": [],
  "root": {
    "type": "Invoke",
    "command": "staticLook.toggle",
    "arguments": {
      "lookId": {
        "kind": "target",
        "targetType": "LookId",
        "id": "look:first-dance"
      }
    }
  }
}
```

Required program metadata:

- stable ID and version;
- label/description localization keys;
- supported workspace/surface/event types;
- required commands/state/capabilities;
- persistence scope and provenance;
- root node;
- language/schema version;
- optional test scenarios.

## 8. Node catalog

### 8.1 `Invoke`

Invokes exactly one registered command with typed arguments.

```json
{
  "type": "Invoke",
  "command": "autoloop.launch",
  "arguments": {
    "bank": {"kind": "context", "path": "bank"},
    "slot": {"kind": "context", "path": "slot"}
  }
}
```

Compile checks:

- command exists and is supported;
- event/control interaction is compatible;
- parameter names, types, ranges, and targets match;
- command is legal in the workspace/execution class;
- safety and capability requirements remain core-owned;
- deprecated commands have an explicit compatible rewrite or fail with a migration diagnostic.

### 8.2 `Sequence`

Executes immediate child actions in declared order.

```json
{
  "type": "Sequence",
  "onFailure": "stop",
  "children": [ ... ]
}
```

Policies:

- `stop` — stop on the first non-success result;
- `continue` — continue only where each command is independently safe and the editor displays non-atomic semantics;
- `branch` — use an explicit result handler.

A sequence is not atomic unless it invokes one domain command that provides atomic behavior. The compiler and editor must display this distinction.

### 8.3 `Branch`

Chooses one of two child actions from a typed, side-effect-free predicate.

```json
{
  "type": "Branch",
  "condition": {
    "op": "eq",
    "left": {"kind": "state", "key": "runner.state"},
    "right": {"kind": "literal", "value": "running"}
  },
  "then": { ... },
  "else": { ... }
}
```

Predicates observe a coherent invocation snapshot. They cannot mutate state or invoke commands.

### 8.4 `Switch`

Selects one child by an enum, bounded integer, optional, or command-result value. All cases are type-checked; a default is required when the input is not exhaustive.

### 8.5 `OnResult`

Branches on the typed result of one command/action.

Result groups:

```text
success        Accepted | NoChange
unavailable    Unavailable | Unsupported | NotFound
invalid        InvalidArguments | ValidationFailed
pressure       QueueFull
safety         SafetyRejected
fault          InternalError
async          StartedAsync(operationId)
```

Exact individual results remain addressable.

There is no implicit retry of `QueueFull` in Live. Retrying stale performance commands can be unsafe; coalescing or retry belongs to the registered domain command/adapter policy.

### 8.6 `Call`

Calls another action by stable `ActionId` with typed parameters.

Rules:

- call graph must be acyclic;
- maximum call depth is bounded;
- arguments are pass-by-value immutable data;
- called program execution class must be compatible;
- package compiler expands or resolves calls before activation;
- missing calls are compatibility errors, not late string lookup.

### 8.7 `Parallel`

Deferred until the direct/sequence/branch model is qualified.

When enabled, it may contain only commands explicitly marked independently parallel-safe. It does not imply cross-command atomicity. Conflicting target/property writes, Studio mutations, priority commands, and utility operations are rejected.

### 8.8 `Return`

Returns a typed action result from a reusable action. It does not return arbitrary object graphs.

### 8.9 No general assignment node in v1

V1 does not expose mutable user variables. Authors use immutable input/context/state values, typed parameters, and explicit command/state. Bounded typed local values may be added later only after deterministic need and lifetime rules are proven.

## 9. Expression language

Expressions are pure and bounded.

Allowed initial operations:

- `and`, `or`, `not`;
- equality/inequality;
- numeric comparisons;
- null/existence checks;
- enum membership;
- bounded arithmetic for argument/presentation transforms;
- clamp, scale, invert, dead zone, round, percent, and unit conversion;
- bounded static map lookup;
- capability checks;
- conditional value selection;
- safe bounded string formatting for labels/diagnostics only.

Prohibited:

- loops and recursion;
- function definition;
- dynamic evaluation;
- reflection or arbitrary property paths;
- regular expressions in v1;
- random values;
- wall-clock/time-of-day as show authority;
- filesystem, network, registry, process, environment, device, or clipboard access;
- command invocation from expressions;
- mutation of state or project content;
- unbounded strings/collections/allocation.

Compiled expressions declare maximum operation count and stack depth. Evaluation failure yields a typed safe default or explicit action failure and a structured diagnostic.

## 10. Binding transforms

The binding layer remains the correct place for source value normalization:

- absolute input range;
- output range;
- clamp;
- inversion;
- dead zone;
- linear/log/exponential/S-curve or bounded control points;
- relative encoder dialect;
- step size and acceleration policy;
- rate limiting;
- soft takeover/pickup;
- enum/value mapping;
- press/release/toggle/momentary/latch semantics;
- modifier/layer selection.

An Action receives normalized typed input. It should not contain vendor-specific MIDI message parsing.

## 11. Command invocation results

The current explicit result model remains the foundation:

```text
Accepted
NoChange
Unavailable
InvalidArguments
NotFound
ValidationFailed
QueueFull
SafetyRejected
Unsupported
InternalError
StartedAsync(operationId)   future metadata-compatible extension
```

Requirements:

- no result is silently ignored by the platform;
- default control feedback is generated consistently;
- `SafetyRejected`, `QueueFull`, and `InternalError` are visible in diagnostics;
- the action trace identifies the exact node and command;
- user-facing surfaces may show concise feedback without leaking sensitive details;
- skin authors cannot redefine a failure as success.

## 12. Timing and scheduling boundary

### 12.1 Prohibited model

The following are not legal surface-action nodes:

```text
wait
sleep
delay loop
repeat until
setInterval
busy wait
poll state until
random timer
```

### 12.2 Correct model

Use typed domain commands and plans:

- launch on next beat/bar;
- fade over N beats;
- hold/release for a bounded duration;
- repeat until track end;
- transition between Looks;
- run a Moment sequence;
- play an Autoloop;
- execute a Track Script.

The registered command accepts quantization/transition parameters or targets a compiled domain object. Runner owns the clock, scheduling, priority, cancellation, generation, and status.

### 12.3 Immediate action composition

A surface action may issue multiple immediate requests, but it cannot use elapsed UI time to create lighting choreography. The editor should recommend creating a reusable Moment/domain plan whenever timing or atomicity is required.

## 13. Safety rules

### 13.1 Priority actions

Commands classified `runnerPriority`—including the independent blackout path—are not legal inside a multi-node action graph.

They require:

- a direct approved binding;
- one-step activation;
- no conditional visibility that removes all access;
- no deferred/double/long gesture dependency;
- no overlay that intercepts the platform shortcut;
- authoritative feedback from safety state.

### 13.2 Stop, Work Light, and Release All

These may use normal registered commands where accepted architecture permits, but mandatory Live reachability remains compiler/runtime enforced. The platform may inject a trusted Safety Dock when the skin does not satisfy the contract.

### 13.3 Hazard commands

- core safety gates remain authoritative;
- hazard arming cannot persist through launch/restart when policy requires re-arming;
- an action cannot synthesize approval, bypass interlocks, or hide the armed state;
- ambiguous gestures and automatic action calls cannot arm a hazard without explicit approved policy;
- repeated hazard invocation is rejected.

### 13.4 Utility and blocking commands

File import/export, migration, package compilation, device tests, and blocking utilities cannot be assigned to an ordinary Live performance pad unless the command metadata explicitly defines a safe asynchronous Live contract.

## 14. Feedback is not optimistic domain state

A control may display local interaction states such as hovered, pressed, focused, or invocation-pending. Domain states such as active Look, active Autoloop, progress, blackout, connection health, or armed hazard come only from registered authoritative state.

The runtime must not assume that invoking a toggle means the domain toggled successfully.

Recommended flow:

```text
input -> invoke -> explicit result -> authoritative state snapshot -> visual/hardware feedback
```

## 15. Reusable action libraries

Actions may live in:

- a bundled application catalog;
- a complete skin package;
- an overlay;
- a controller profile where device-specific composition is appropriate;
- a project-authored semantic Moment/domain plan.

Resolution precedence must be explicit and collision-free. Public/bundled IDs cannot be shadowed silently. User actions use owner/package namespaces.

A skin action cannot directly depend on machine-local controller identity. It may expose semantic roles that a controller profile binds independently.

## 16. Persistence and portability

Each action records:

- stable ID/version;
- schema/language version;
- owning package/overlay/profile;
- provenance and content hash;
- required command/state/capability/component signatures;
- stable target references;
- persistence scope;
- optional source location for diagnostics;
- test scenarios.

Export redacts machine paths, device serials, endpoints, credentials, and private state unless the owner explicitly chooses a separate diagnostic export.

Missing targets after project/import changes remain represented as missing references. They are not redirected to a similarly named object.

## 17. Compiler pipeline

```text
parse source
  -> schema validation
  -> ID/reference resolution
  -> type inference/checking
  -> command/state/capability signature validation
  -> execution-class analysis
  -> safety-policy analysis
  -> call-graph cycle/depth analysis
  -> worst-case step/expression/subscription analysis
  -> constant folding and direct-command desugaring
  -> compile immutable bounded instructions
  -> emit compatibility lock + source map + tests
```

The compiler emits exact diagnostics with:

```text
package/action ID and version
source file + JSON pointer/node ID
command/state/capability/component ID
expected and actual type/signature
execution class and workspace
safety rule
limit and actual value
suggested compatible replacement where known
fallback/activation decision
correlation ID
```

## 18. Runtime executor

### 18.1 Isolation

- executor runs outside the DMX scheduler;
- it holds no scheduler locks;
- it posts only through approved typed boundaries;
- no filesystem/network/device operation occurs during ordinary execution;
- instruction and stack memory are bounded/preallocated where practical;
- invocation has a strict maximum step count;
- state reads use one coherent snapshot per evaluation phase;
- cancellation occurs on surface destruction, package generation change, workspace change, device disconnect, or app shutdown as defined by action metadata.

### 18.2 Transaction semantics

Studio mutations may request a documented document transaction that either commits once or rolls back. Live Runner commands do not acquire fake cross-command transactions. Atomic Live behavior requires one domain command/plan.

### 18.3 Generation safety

Every invocation is associated with:

- active skin/overlay generation;
- command registry generation;
- active project/show-package generation where targets depend on it;
- controller profile generation where applicable.

Stale target bindings reject or re-resolve only through approved stable-ID rules; they never execute against a new object by numeric coincidence.

## 19. Provisional limits

These values preserve the existing bounded Action Set direction and may be tightened by benchmarks.

### Per action program

| Limit | v1 |
| --- | ---: |
| graph nodes | 32 |
| branch/switch depth | 8 |
| call depth | 8 |
| referenced state keys | 16 |
| referenced capabilities | 16 |
| parameters | 16 |
| expression operations | 128 per expression |
| expression stack | 32 |
| serialized source | 16 KiB |
| maximum immediate command attempts | 32 |

### Per skin/overlay

| Limit | v1 |
| action programs | 512 |
| total action nodes | 8192 |
| action source bytes | 2 MiB |
| concurrent surface invocations | bounded by platform; provisional 256 |
| repeat events per control | max 20 Hz, platform-gated |

The compiler computes worst-case paths before activation. Limit failure rejects the candidate package or action; it never truncates behavior silently.

## 20. Trace and debugging

Skin Studio and development Diagnostics provide a bounded Action Trace:

```text
timestamp/order
source surface/control/event
skin/overlay/profile/action generation
node path
resolved command and redacted arguments
availability/preflight result
invocation result
associated state changes by generation, not causal guess
execution duration
fallback/branch selected
```

Rules:

- fixed-size ring buffer;
- no unbounded payloads;
- personal paths, device serials, credentials, track metadata, and project-private text are redacted by default;
- export is explicit;
- trace is not required for command execution;
- disabled/hidden diagnostics reduce formatting/update work.

## 21. Expert text projection — working name `EmberScript`

Expert text is optional and must not precede the canonical graph/compiler.

Required characteristics:

- explicit action, event, command, parameter, and reference syntax;
- exact types and no implicit coercion;
- autocomplete from the live catalogs;
- formatting, go-to-definition, diagnostics, and visual-graph synchronization;
- no imports from arbitrary paths;
- no loops, recursion, user functions, dynamic evaluation, device/network/filesystem access, or runtime source execution;
- stable source maps back to graph node IDs;
- lossless text -> graph -> text round-trip.

Illustrative—not frozen—syntax:

```text
action user.wedding.firstDance on press in live {
  when state.runner.state == running {
    invoke staticLook.toggle(
      lookId: @look("look:first-dance")
    )
    on unavailable {
      invoke view.notice.show(
        messageKey: "action.firstDance.unavailable"
      )
    }
  }
}
```

The editor stores canonical graph data. Text source may be retained for user readability only when it produces the identical graph and signature.

## 22. Examples

### 22.1 Contextual Autoloop pad

```json
{
  "type": "Invoke",
  "command": "autoloop.launch",
  "arguments": {
    "bank": {"kind": "context", "path": "bank"},
    "slot": {"kind": "context", "path": "slot"}
  }
}
```

Feedback is defined separately:

```json
{
  "active": {
    "op": "and",
    "items": [
      {"op": "eq", "left": {"state": "autoloop.active.bank"}, "right": {"context": "bank"}},
      {"op": "eq", "left": {"state": "autoloop.active.slot"}, "right": {"context": "slot"}}
    ]
  },
  "progress": {
    "when": "active",
    "state": "autoloop.active.progress"
  }
}
```

### 22.2 Target-aware Static Look hold

Press and release are separate bindings to registered Runner-owned semantics:

```json
{
  "press": {
    "command": "staticLook.hold",
    "arguments": {"lookId": {"kind": "target", "targetType": "LookId", "id": "look:first-dance"}, "pressed": true}
  },
  "release": {
    "command": "staticLook.hold",
    "arguments": {"lookId": {"kind": "target", "targetType": "LookId", "id": "look:first-dance"}, "pressed": false}
  }
}
```

The skin does not implement ownership or late-release protection; Runner does.

### 22.3 Group intensity fader

The binding normalizes MIDI/UI input and invokes one group command:

```json
{
  "command": "group.intensity.set",
  "arguments": {
    "groupId": {"kind": "target", "targetType": "GroupId", "id": "group:dance-floor"},
    "value": {"kind": "input", "path": "value"}
  }
}
```

Soft takeover belongs to the controller binding transform. Feedback reads the authoritative group intensity/override state.

### 22.4 Capability fallback

```json
{
  "type": "Branch",
  "condition": {"op": "capability", "id": "transport.exactPlayhead"},
  "then": {
    "type": "Invoke",
    "command": "trackScript.activate",
    "arguments": {"trackScriptId": {"kind": "context", "path": "trackScriptId"}}
  },
  "else": {
    "type": "Invoke",
    "command": "view.notice.show",
    "arguments": {"messageKey": "trackScript.exactTransportRequired"}
  }
}
```

The action does not pretend BPM-only transport provides exact script playback.

### 22.5 Timed wedding Moment — correct abstraction

Do not write:

```text
activate look
wait 4 seconds
launch autoloop
wait 16 beats
release look
```

Create/target a project-authored `Moment` or other compiled domain plan and invoke:

```json
{
  "type": "Invoke",
  "command": "moment.activate",
  "arguments": {
    "momentId": {"kind": "target", "targetType": "MomentId", "id": "moment:grand-entrance"}
  }
}
```

## 23. Compatibility and migration

### 23.1 Direct command bindings

Existing bindings with `command` and typed arguments compile to a generated one-node action. No user migration is required merely to introduce the Action Graph.

### 23.2 Command changes

- additive metadata remains compatible;
- compatible aliases resolve during compilation;
- deprecated command migration uses declared replacement/signature adapters;
- incompatible changes block activation and produce exact repair UI;
- no alias resolution occurs on the scheduler hot path.

### 23.3 Foreign scripts

Migration adapters parse foreign syntax into a source-specific AST, then map supported constructs to Ember Actions. Unknown constructs remain opaque with source location and status. They are never injected into an executable runtime.

## 24. Required tooling

- Action Explorer and searchable catalog;
- visual simple-command editor;
- visual graph editor;
- generated target/parameter editors;
- action validator/compiler CLI;
- formatter/inspector;
- deterministic action test runner;
- Action Trace viewer;
- compatibility/diff report;
- fuzz corpus and parser/type-checker harness;
- cross-surface equivalence harness;
- package-local action scenarios;
- optional expert text editor after exact round-trip proof.

## 25. Acceptance

Ember Actions v1 is implemented when:

1. A direct command binding and its generated one-node action produce identical domain results.
2. Skin, keyboard, MIDI/HID, DJ command, and direct test surfaces can invoke representative actions through one compiler/executor boundary.
3. Typed parameters, targets, state, capabilities, transforms, and result branches validate deterministically.
4. Cycles, excessive depth/nodes/expressions, prohibited execution classes, unsafe priority composition, and arbitrary I/O are rejected before activation.
5. No action uses UI timing as show timing.
6. Missing targets/capabilities/commands remain visible and repairable without silent substitution.
7. Invalid action updates preserve the current active skin/action generation.
8. Blackout and other priority paths remain independent and reachable.
9. Action execution and trace remain inside the Perform CPU/memory/jitter envelope.
10. Visual and source forms compile to the same canonical graph; expert text is not released until round-trip goldens pass.
11. Version/signature migration and provenance are machine-readable.
12. Fuzz, property, safety, cross-surface, installed-Windows, and long-run tests pass.