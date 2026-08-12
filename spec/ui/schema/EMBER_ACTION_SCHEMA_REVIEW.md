# Ember Action Schema v1 — Pre-Implementation Hardening Review

Status: **required implementation review for SKIN2-002 / issue #65**.

Reviewed artifact:

- `spec/ui/schema/ember-action.schema.json`
- `spec/ui/ember-actions-contract-v1.md`
- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`

Review date: 2026-08-12.

## 1. Result

The provisional schema is structurally sufficient to communicate the intended action model and issue decomposition. It is **not yet sufficient as the sole untrusted-input validator**.

Implementation must use four layers:

```text
bounded file/package reader
  -> JSON Schema structural validation
  -> registry-aware semantic/action-graph validation
  -> immutable Action IR compiler and executor budgets
```

No one should interpret a schema-valid file as activation-safe until all four layers pass.

## 2. Required P0 hardening before runtime activation

### 2.1 Bound generic JSON values

The current provisional schema intentionally leaves several values unconstrained with `{}` or broad objects:

- parameter `default`;
- surface-state `default`;
- expression `literal`;
- `Switch.case.match`;
- transform `map` values;
- source `sourceMap`;
- any future free-form provenance/diagnostic extension.

Before implementation, replace these with bounded definitions or validate them immediately after schema parsing:

- maximum string length;
- maximum object properties;
- maximum list items;
- maximum nesting depth;
- allowed scalar/object/list forms;
- schema-reference requirement for structured object/list values;
- deterministic key ordering for canonicalization;
- no non-finite numbers;
- no path/URL/device interpretation merely because a value is a string.

### 2.2 Platform limits are authoritative

The top-level `limits` object may describe or request a lower action budget. It must never raise platform limits.

Compiler rule:

```text
effectiveLimit = min(platformLimit, packageLimit, actionDeclaredLimit)
```

Missing declared limits use platform/package defaults. An action cannot self-authorize additional nodes, calls, state reads, source bytes, expression operations, stack, queue pressure, or execution duration.

### 2.3 Source-map bounds and privacy

`source.sourceMap` must be bounded and structurally defined before imported expert text is accepted.

Required minimum:

- bounded entry count;
- bounded source path/virtual URI length;
- source ranges use non-negative offsets with `end >= start`;
- no absolute local path required in portable artifacts;
- no embedded source file contents in ordinary source-map entries;
- no arbitrary nested metadata;
- local owner diagnostics and portable export are separate.

### 2.4 Canonical hash contract

`contentHash` is presently optional because canonicalization remains an implementation deliverable. Before installation/export, define:

- fields included/excluded from hash;
- normalization of object keys, numbers, Unicode, line endings, and optional expert source;
- whether authoring-only comments/source maps affect runtime digest;
- action dependency digest behavior;
- registry/compiler generation included in compiled-cache key but not necessarily source hash;
- mismatch behavior;
- deterministic test vectors.

The runtime must not trust a supplied hash without recomputing it.

### 2.5 Semantic reference closure

JSON Schema cannot prove that:

- every entry-point reference exists;
- every child/branch/`next` reference exists;
- no node is unreachable unintentionally;
- graph and action dependencies are acyclic;
- declared requirements exactly cover actual dependencies;
- `resultAs`, locals, node outputs, and surface-state references are defined before use;
- state reads and command/action IDs resolve against the accepted registry generation.

The semantic validator must build and validate the complete graph before IR generation.

### 2.6 Type-dependent value rules

`valueType` fields require semantic validation beyond the current structural schema.

Examples:

- `minimum <= maximum`;
- integer types reject fractional bounds/defaults;
- enum requires bounded unique `values` or an approved schema reference;
- stable ID requires `targetKind`;
- object requires approved `schemaRef`;
- list requires `itemType` and effective `maxItems`;
- string requires an effective maximum length;
- duration declares whether it is input-gesture duration or a domain timing type;
- units are valid for the selected value kind;
- defaults/literals match the declared type, unit, range, enum, target kind, and schema.

### 2.7 Expression operator arity and result typing

The schema bounds argument count but does not encode exact arity or operand/result types.

The semantic validator/compiler must enforce, for example:

- `not`, `isNull`, and `isAvailable`: exactly one argument;
- comparisons: exactly two compatible arguments;
- `conditional`: exactly condition/true/false arguments with compatible branch types;
- arithmetic: numeric operands with compatible units;
- `divide`: explicit zero handling;
- `contains`: approved bounded collection/string semantics;
- `format`: registry-approved bounded templates only;
- every expression has one inferred output type and bounded output size.

### 2.8 Transform-specific requirements

The generic transform object currently permits fields that may be absent or irrelevant for a selected kind. The validator must apply discriminated rules:

- `clamp` requires valid minimum/maximum;
- `scale` requires non-degenerate source range and target range;
- `deadZone` requires a bounded threshold/range;
- `curve` requires a registered bounded curve ID;
- `quantize` requires a positive finite step;
- `enumMap` requires a bounded typed map with complete/default behavior;
- `relativeEncoder` requires a supported dialect and bounds;
- `unitConvert` requires registered source/target units and an approved conversion.

Free-form unit strings must be replaced or resolved against the canonical Value/Unit Registry.

### 2.9 Result and feedback typing

`Return.result`, `OnResult.result`, and feedback expressions require explicit inferred or declared types.

Implementation must ensure:

- `OnResult` receives the registered invocation-result type;
- `StartedAsync` operation data is represented through an approved result schema;
- feedback names map to declared supported output types/limits;
- labels/icons/colors cannot conceal mandatory faults or safety state;
- controller feedback respects device-profile output capability and range;
- diagnostic payloads are bounded and privacy-safe.

### 2.10 Execution budget and cancellation

In addition to static graph limits, IR execution needs runtime guards:

- maximum instructions per invocation;
- maximum command submissions per invocation and per source/time window;
- maximum expression operations and stack depth;
- maximum transient memory;
- cancellation token for utility/Studio work;
- bounded diagnostic trace;
- no executor work on the DMX scheduler;
- explicit QueueFull/Cancelled/InternalError behavior;
- no automatic retry loop for rejected or hazardous commands.

## 3. Important P1 design clarifications

### 3.1 `onActivate` / `onDeactivate`

These entry points can create surprising side effects when a page, skin, mapping layer, or artifact activates. Before enabling command invocation from them:

- define exact lifecycle owner;
- define whether activation during app startup/import/preview/switch may run actions;
- default to view-local initialization only;
- prohibit output-critical or hazardous command invocation without a separately approved policy;
- ensure invalid/failed activation cannot partially execute actions.

### 3.2 Refreshed state boundaries

`ReadState.refreshBoundary` needs a precise policy:

- which executor classes may refresh;
- whether the refresh is coherent across a snapshot group;
- maximum refreshes per invocation;
- no polling/waiting;
- update-rate constraints;
- deterministic tests.

Default remains one coherent snapshot at action start.

### 3.3 `Parallel`

`Parallel` means bounded independent submission, not threads, simultaneity, ordering, or atomicity.

Registry metadata must declare whether each command is parallel-compatible. Priority/emergency, Studio transaction, blocking, and mutually conflicting Runner operations are rejected from generic parallel groups.

### 3.4 Surface-local state

Surface-local state must have:

- stable owner scope;
- deterministic reset and migration;
- strict count/size/type limits;
- no collision with authoritative state IDs;
- app-local persistence only through an approved service;
- no use as hidden lighting state or safety authority.

### 3.5 Expert source retention

Optional expert text is authoring source, not runtime authority.

Define:

- whether normalized graph is stored beside text;
- mismatch resolution if text and graph differ;
- parser/formatter version;
- source-map relationship;
- whether formatting-only edits change source hash;
- exact visual → text → visual round-trip fixtures.

## 4. Required negative fixture set

Add fixtures for:

1. unknown top-level property;
2. missing required field;
3. invalid stable ID and SemVer;
4. oversized strings/localization/source/action;
5. huge or deeply nested literal/default/map/source-map;
6. NaN/Infinity-equivalent parser edge cases;
7. invalid value-type combinations;
8. default/literal type, range, unit, enum, target, and schema mismatch;
9. missing node reference;
10. duplicate/unreachable node;
11. graph cycle;
12. action dependency cycle;
13. call/branch/dependency depth overflow;
14. command/state/capability/action missing or deprecated incompatibly;
15. invalid expression arity/type/unit;
16. divide-by-zero and non-finite transform result;
17. invalid transform fields/ranges/dialect/curve/unit conversion;
18. undeclared local/node output/surface-state read;
19. result-type mismatch;
20. mixed incompatible realtime classes;
21. priority/emergency command inside Sequence/Parallel/OnResult misuse;
22. Studio transaction containing non-transactional operation;
23. utility async bound as ordinary Live action;
24. command flood within graph and repeated input window;
25. `onActivate` output side effect rejected by default policy;
26. absolute path/private identity in portable source map/provenance;
27. content-hash mismatch;
28. unsupported future schema/registry range;
29. malicious archive/path/XML/source import feeding an action;
30. invalid update preserving current valid action.

## 5. Required positive/golden fixtures

1. one-command Static Look trigger;
2. ownership-safe Static Look hold/release;
3. Autoloop launch with domain-owned repeat/quantize/return arguments;
4. group-intensity absolute control with binding soft takeover;
5. result-aware reconnect action;
6. typed conditional based on one coherent state snapshot;
7. reusable nested action within call-depth limits;
8. Studio-only transactional action;
9. async utility start with operation ID and progress states;
10. visual graph and expert text producing identical canonical graph/IR/digest;
11. compatible registry-additive update;
12. deprecated compatible replacement with exact migration report;
13. missing target becoming unavailable/relinkable;
14. action import/export with machine identity redacted;
15. same action invoked through software, keyboard, MIDI/controller, and direct test with equivalent command/results.

## 6. Implementation acceptance

SKIN2-002 cannot claim the action schema/runtime complete until:

- structural and semantic validators are separate and both tested;
- the P0 findings above are resolved or explicitly bounded by accepted implementation rules;
- canonicalization and hashes have golden vectors;
- graph/reference/type/unit/safety/realtime/persistence/transaction closure is proven;
- malicious and oversized sources fail closed;
- visual/text/canonical round-trip is deterministic;
- action execution is bounded, cancellable where appropriate, and outside the DMX scheduler;
- invalid update/install retains the current valid action/surface;
- active DMX continuity and scheduler budgets pass under representative action load.
