# UI Registry Reconciliation Baseline — 2026-08-12

Status: **evidence baseline for issue #64 / SKIN2-001**. This document inventories current implementation and planning drift; it does not independently rename accepted commands/states or implement the generator.

Baseline main commit:

```text
1281cb2f3ab9d5487027612bb9ef076537517c40
```

Compared sources:

- `native-core/include/emberlights/ui_command.hpp`
- `native-core/include/emberlights/ui_state.hpp`
- `spec/ui/registry/command-registry-seed-v0.json`
- `spec/ui/registry/state-registry-seed-v0.json`
- `spec/ui/command-state-skin-contract-v0.md`
- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
- `spec/ui/ember-actions-contract-v1.md`
- issue #31 command/state ownership
- issue #59 Autoloop V2 runtime/status ownership

## 1. Baseline findings

| Surface | Native implementation | Seed registry | Exact shared IDs | Native-only IDs | Seed-only IDs |
| --- | ---: | ---: | ---: | ---: | ---: |
| Commands | 29 | 35 | 26 | 3 | 9 |
| States | 39 | 40 | 22 | 17 | 18 |

The counts intentionally distinguish an exact string-ID match from a potential semantic relationship. Similar labels do not establish aliases.

The seed files still identify source commit `75c9766...`; they are not generated from current native code and cannot be treated as the sole current authority.

## 2. Command reconciliation

### 2.1 Exact shared native + seed commands

```text
show.start
show.stop
show.toggleRunning
output.blackout.set
output.blackout.toggle
output.workLight.set
output.workLight.toggle
override.releaseAll
transport.manualBpm.set
transport.tap
safety.hazard.setArmed
safety.hazard.disarmAll
staticLook.activate
staticLook.toggle
staticLook.hold
staticLook.clear
autoloop.launch
autoloop.clear
autoloop.next
autoloop.previous
autoloop.bankFilter.enableAll
autoloop.bankFilter.selectExclusive
autoloop.bankFilter.setEnabled
trackScript.start
trackScript.clear
group.override.property.set
```

These IDs already bridge current native behavior and planning metadata. Preserve them unless #31 accepts an explicit compatibility migration.

### 2.2 Native-only commands missing from the seed

```text
fixture.override.property.set
fixture.override.property.release
group.override.property.release
```

Required #64 action: add them to the canonical source with the exact current semantics, argument contracts, availability, safety/realtime class, and feedback. Do not remove working native commands to match the stale seed.

### 2.3 Seed-only planned commands without native facade implementation

```text
workspace.open
view.panel.open
view.panel.toggle
view.skin.select
view.commandExplorer.open
project.open
project.save
project.validate
connection.os2l.reconnect
```

Required #64 action: represent implementation status explicitly. These may remain valid planned/public targets only after their actual authoritative service and invocation/result contract exists. A generated runtime catalog must not imply that a command is callable merely because it appears in a planning seed.

Recommended metadata distinction:

```text
planned | implemented | bridged | deprecated | removed
```

The exact vocabulary belongs to #64, but callable availability and implementation evidence cannot remain implicit.

## 3. Command metadata drift requiring decisions

### 3.1 Invocation results

Current native result vocabulary:

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
```

The Ember Actions contract additionally expects or names:

```text
MissingTarget
Cancelled
StartedAsync(operationId)
```

High-priority #31/#64 decision:

- determine whether native `NotFound` is a compatibility/internal result, a public alias for `MissingTarget`, or a distinct result;
- add async/cancellation results only with real service contracts;
- generate one authoritative result registry and adapters;
- never silently reinterpret a safety or queue result.

### 3.2 Realtime classes

Seed values include:

```text
viewLocal
blockingUtility
studioMutation
priorityLive
emergency
runnerCommand
```

The accepted V2 contract uses:

```text
viewLocal
studioMutation
runnerCommand
runnerPriority
utilityAsync
blockingForbiddenLive
```

Do not preserve two public vocabularies. Likely semantic relationships such as `priorityLive -> runnerPriority` or `emergency -> runnerPriority + emergency safety metadata` require an explicit accepted mapping. `blockingUtility` must be split according to whether work is async or prohibited in Live.

### 3.3 Interaction types

Native currently supports:

```text
Trigger
Toggle
Absolute
Momentary
Selection
```

Planning schemas also need relative controls, transactions, async utilities, and richer binding gestures. Keep command interaction semantics separate from input-gesture recognition. Long/double press, relative encoder dialect, and soft takeover belong to bindings; Studio transaction support belongs to command metadata/service behavior.

### 3.4 Typed arguments

The native `UiCommandInvocation` is a compact transitional union-like structure. The public registry needs per-command typed argument schemas and generated adapters while preserving the bounded scheduler-facing transport. Do not expose unused generic fields as a public API.

## 4. State reconciliation

### 4.1 Exact shared native + seed state IDs

```text
runner.state
runner.health
transport.bpm
transport.syncState
connection.os2l.status
output.blackout
output.workLight
safety.hazard.fog.armed
safety.hazard.haze.armed
safety.hazard.laser.armed
safety.hazard.spark.armed
staticLook.active.id
autoloop.active.id
autoloop.active.bank
autoloop.active.slot
autoloop.active.progress
autoloop.bankFilter.mask
trackScript.active.id
trackScript.elapsedBeat
override.activePropertyCount
output.controlOne.status
output.controlOne.experimental
```

### 4.2 Native-only state IDs missing from the seed

```text
project.active.id
project.active.name
runner.generation
runner.frames
transport.beatPosition
transport.clockSource
connection.os2l.discoveryStatus
controller.input.status
controller.output.status
output.artnet.status
output.sacn.status
output.dmxUsbPro[0].status
output.dmxUsbPro[1].status
output.micro.status
autoloop.active.repeat
autoloop.active.completedCycles
trackScript.consumedCueCount
```

Required #64 action: add current implemented state to the canonical source, retaining exact authority and update behavior. Do not drop working observability to conform to the stale seed.

### 4.3 Seed-only planned state IDs without an exact native ID

```text
app.workspace
app.skin.id
app.skin.status
project.name
project.lifecycle
project.saveState
project.lastVerifiedSave
project.activeVenue.name
project.activePackage.available
project.validation.summary
runner.jitter.p99Ms
connection.dj.status
connection.os2l.configured
connection.os2l.lastError
controller.status
output.universe[0].status
output.universe[1].status
safety.summary
```

These are not automatically aliases for current native state.

Important distinctions:

- `project.active.name` and `project.name` may represent active Runner package context versus editable project-document identity.
- backend-specific output health and logical universe health are related but distinct views.
- controller input/output health and an aggregate controller status can coexist as source and derived state.
- `runner.jitter.p99Ms` belongs to the performance/qualification contract and requires a real bounded publisher.
- application/project lifecycle states require #31/Studio/application authority, not inferred values in a skin.

## 5. State update-class drift

Native compact classes:

```text
Event
Health
Realtime
```

Seed/tooling classes include:

```text
onChange
health
transport
progress
```

The public registry should preserve semantic update class and maximum publication rate, while generated native/runtime metadata may collapse those into a smaller hot-path enum. Avoid treating the compact native enum as the entire public contract.

Required decisions:

- canonical update-class vocabulary;
- maximum publish Hz;
- coherent snapshot group;
- visibility/hidden-panel throttling policy;
- privacy/redaction;
- bounded collection/string/object schemas.

## 6. Autoloops V2 ownership reservation

Issue #59 owns the exact runtime behavior and status names for:

```text
selected placement/content
queued or pending placement/content and reason
active placement/content
active source: autonomous | scripted | manual
mode: overlay | replace
repeat
phase/progress/completed cycles
active bank mask
pending bank mask and exclusive bank
selection policy and reason
last transition/result/error
```

#64 must support these metadata shapes and reserve coordination, but **must not freeze competing IDs before #59 posts/merges its accepted additions**.

Existing accepted IDs remain valid unless deliberately migrated:

```text
autoloop.active.id
autoloop.active.bank
autoloop.active.slot
autoloop.active.progress
autoloop.active.repeat
autoloop.active.completedCycles
autoloop.bankFilter.mask
```

Likely new semantic families need selected/queued/source/mode/pending/policy/result state, but exact names and value schemas are #59 + #31 decisions.

## 7. Missing registry families

Current native and seed work does not yet provide complete canonical sources for:

- Component Registry;
- Capability Registry;
- Interaction/Event Registry;
- Theme Token Registry;
- reusable Value/Unit/Target Schema Registry;
- Invocation Result Registry;
- direct-callback bypass manifest as machine-readable generated input;
- first-party artifact dependency/cross-reference manifest.

The first #64 vertical slice may begin with command/state/result/value definitions, but the source format must be extensible to the full accepted registry set without creating separate competing generators.

## 8. Canonical-source requirements

The source choice remains an implementation decision, but the first candidate must prove:

1. one definition generates native IDs/lookups and JSON/tooling artifacts;
2. deterministic canonical ordering and digest;
3. expressive typed values, units, ranges, targets, results, capabilities, safety, realtime, persistence, Undo, and deprecation;
4. implemented/planned/bridged evidence state;
5. no general JSON parsing/string resolution in the scheduler;
6. existing IDs and enum stability can be preserved during the strangler migration;
7. generated diff classifies additive, deprecated, compatible replacement, and breaking changes;
8. future component/capability/action/designer metadata can consume the same value schemas.

Do not choose a format merely because it is easiest to hand-edit. Evaluate generator simplicity, schema validation, native compile-time use, deterministic diffs, and feature-agent maintenance.

## 9. First implementation decisions required from #64

Before broad generation, record these decisions in the issue/ADR or generated-source README:

1. canonical source format and why;
2. generated-file ownership and whether generated artifacts are committed;
3. public result vocabulary and `NotFound`/`MissingTarget` policy;
4. public realtime-class mapping;
5. public update-class/rate/snapshot model;
6. implemented/planned/bridged status model;
7. stable enum/native ID preservation strategy;
8. seed migration/deprecation strategy;
9. Autoloops V2 coordination rule;
10. registry version/digest/diff baseline.

## 10. First-slice acceptance evidence

- reconciliation machine-readable baseline validates;
- canonical source includes every current native command/state exactly once;
- planned-only entries cannot masquerade as callable/observable implementation;
- generated native command/state IDs preserve current behavior and tests;
- generated JSON/tooling includes complete metadata for the first slice;
- current native-only items are no longer missing from the catalog;
- seed-only items are explicitly classified;
- result/realtime/update-class decisions are recorded;
- Autoloops V2 unmerged semantics remain owner-reserved, not guessed;
- clean regeneration and compatibility-diff tests pass;
- no new direct callback bypass or scheduler parsing/allocation is introduced.
