# Skin Manifest and Layout Schema — V1 Designer Hardening Review

Status: **binding pre-implementation review** for issues #32, #64, #66, #67, and #72. This review preserves the v0 schemas/examples as compatibility evidence; it does not replace them in place or claim a runtime implementation.

Review date: 2026-08-12.

Reviewed artifacts at main `1281cb2f3ab9d5487027612bb9ef076537517c40`:

```text
spec/ui/schema/skin-manifest.schema.json
  sha 6aa7e8a65e0b4e05b725893f004abadccc6d6ef2

spec/ui/schema/layout.schema.json
  sha db3982b112f81f586b10f32dd950e5f6adbc489b

spec/ui/examples/reference-live-standard.layout.json
  sha 51b4ce2dfbdee2405ca7e01fe94cb25dbda457c9
```

Related authority:

- `spec/ui/emberskin-package-and-safety-limits-v0.md`
- `spec/ui/command-state-skin-contract-v0.md`
- `spec/ui/ember-actions-contract-v1.md`
- `spec/ui/skin-designer-contract-v1.md`
- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
- `docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`

## 1. Result

The current v0 schemas are strong planning fixtures for ordinary container/control trees, package metadata, responsive target variants, command/state references, native components, and bounded custom slots.

They are **not yet sufficient as the complete source/runtime contract for the V1 visual Designer, overlays, Ember Actions, registry compatibility, migration, or untrusted-package activation**.

The implementation should preserve v0 readers/fixtures and introduce an explicit v1 source/migration path rather than silently widening the meaning of existing fields.

Required validation pipeline:

```text
bounded package/source reader
  -> structural JSON Schema validation
  -> registry-aware semantic and cross-artifact validation
  -> layout/binding/action/component expansion and safety limits
  -> mandatory-control/accessibility/responsive qualification
  -> immutable compiled View Graph + Binding Tables + Action IR
  -> hidden candidate instantiation
  -> transactional activation or exact rejection
```

A schema-valid layout or manifest is not activation-safe until every stage passes.

## 2. Required schema/artifact separation

Do not force authoring source, distributable package metadata, and compiled runtime graph into one file model.

Recommended v1 artifact boundaries:

```text
Designer source project
  project manifest
  editable layout/component/theme/action/binding/localization/asset source
  base/fork/overlay relationship
  simulation/history/export metadata

Distributable package
  skin or overlay manifest
  validated source or precompiled artifact inputs permitted by policy
  assets/locales/license/provenance
  dependency and compatibility metadata

Compiled runtime artifacts
  immutable bounded View Graph
  Binding Tables
  Action IR
  compact registry/capability dependency table
  content and compiler generation digests
```

The exact filenames/extensions remain implementation decisions, but runtime meaning cannot depend on hidden Designer-only metadata.

## 3. Layout schema P0 gaps

### 3.1 Untyped predicate strings

Current fields:

```text
visibleWhen: boolean | string
enabledWhen: boolean | string
```

Problems:

- a free-form string does not prove referenced state/capability/context IDs;
- type, operation count, output type, deprecation, privacy, and update cost cannot be validated structurally;
- different runtimes/editors could interpret the same string differently.

V1 requirement:

- use a shared bounded typed expression/predicate representation or a versioned expert text form that compiles to it;
- resolve every state/capability/context reference against the accepted registry generation;
- cap depth, operations, string/list output, and update rate;
- predicate failure uses a declared safe default and structured diagnostic;
- predicates cannot invoke commands/actions or mutate state.

### 3.2 Untyped command arguments and state comparison values

Current fields include:

```text
commandBinding.arguments.additionalProperties: {}
stateBinding.equals: {}
```

V1 requirement:

- use shared typed value/expression schemas;
- validate argument name, type, unit, range, target kind, availability, safety, and interaction compatibility against the Command Registry;
- validate state comparison/format against the State/Value Registry;
- stable project targets retain exact IDs and become unavailable/relinkable when missing;
- no arbitrary object/list depth or size.

### 3.3 One-control binding model cannot express the accepted platform

Current control fields provide one `on` and optional `onRelease`, while the binding schema has a small event enum.

V1 must support registry-driven bindings or references for:

```text
press
release
value/change start/change/end
encoder step
long press
double press
modifier/layer
activate/deactivate under approved policy
registered Ember Action
result handling
feedback outputs
context/parameter references
transform chain
```

Gesture recognition, relative MIDI dialect, and soft takeover remain Binding/controller-profile responsibilities. Layout source should reference bindings/actions rather than embedding a second partial behavior model.

### 3.4 Native-component properties are structurally unbounded

Current `NativeComponent.properties.additionalProperties: {}` and generic slot children do not prove component version, property types, events, state dependencies, capability requirements, or expansion cost.

V1 requirement:

- every component type/version resolves through the Component Registry;
- component property/event/slot schemas validate before graph expansion;
- unknown properties/events fail exactly or use an explicit versioned extension point;
- component instances declare/derive state subscription and performance class;
- optional component fallback behavior is explicit;
- a skin cannot gain additional command/state/device authority through a native component.

### 3.5 Data sources, selection, tabs, and panel state are raw strings

Current fields such as:

```text
dataSource
selectionState
openState
Tabs.selectionState
```

need an explicit source kind:

- registered authoritative State;
- registered bounded collection/context;
- approved surface/view-local State;
- component-owned view state;
- Designer simulation state.

Presentation page selection such as `view.reference.live.page` must not masquerade as Runner/domain state. View-local state requires type, owner, reset/persistence scope, migration, and collision rules.

### 3.6 No first-class repeater/template context

The current schema has virtualized List/Tree/Table and native matrices, but the full Designer needs a bounded generic repeat/template contract for appropriate primitive/composite cases.

V1 requirement:

- registered bounded collection source;
- typed context item schema;
- stable item identity independent of visual index/name;
- virtualized or hard maximum behavior;
- item-template expansion limits;
- context-aware command/action/state bindings;
- empty/loading/degraded/error representation;
- hidden/offscreen subscription policy.

Large product-shaped collections such as Timeline, library, fixture trees, and Autoloop/Static Look matrices may remain native components.

### 3.7 Reusable components and inheritance are absent

The Designer contract requires reusable user/package components and variant/base inheritance.

V1 requirement:

- component definition ID/version/scope;
- typed public properties/events/slots;
- dependency manifest;
- no cycles/recursive expansion;
- expansion depth/node limits;
- instance overrides and base-value diff;
- stable IDs preserved through visual reordering;
- update/migration preview for base/template changes.

### 3.8 Responsive variant inheritance is not represented

Separate layout files and manifest variants exist, but there is no source-level inheritance/override model.

V1 Designer source needs:

- base variant and override chain;
- no inheritance cycle;
- breakpoint/input/DPI target metadata;
- property/node add/remove/override rules;
- mandatory-control inheritance and reachability;
- deterministic resolved layout;
- source diff showing inherited versus overridden values.

Compiled runtime output may contain fully resolved variants.

### 3.9 Grid/constraint editing metadata is incomplete

Current Grid supports column count and cell widths but lacks ordinary Designer operations such as explicit child row/column/span, row definitions, alignment per cell, named guides, and responsive constraint overrides.

V1 source needs enough toolkit-neutral layout semantics to support:

- row/column/span;
- min/preferred/max sizing;
- grow/shrink/wrap;
- alignment/distribution;
- gap/padding/margin;
- dock/anchor within approved containers;
- bounded overlay/artboard placement where explicitly allowed;
- deterministic overflow/fallback.

Do not expose renderer-specific object types, CSS, XAML, Slint, or Win32 handles in the public source.

### 3.10 Style/theming is only a class-name reference

`styleClass` is useful but insufficient for Designer token inspection, state variants, inherited theme values, and compatibility.

V1 requirement:

- semantic token references;
- registered style roles/component states;
- optional allowed per-instance overrides with typed values;
- selected/active/queued/focus/warning/fault/disabled state mapping;
- reduced motion/high contrast alternatives;
- no unbounded CSS or renderer-specific selectors;
- theme-token registry/deprecation/usage validation.

## 4. Manifest schema P0 gaps

### 4.1 `fallbackVariant` currently blocks arbitrary custom IDs

The manifest permits arbitrary variant IDs, but an `allOf` later restricts `fallbackVariant` to:

```text
compact | standard | wide | touch-live | safe
```

This prevents a valid custom variant ID from being its own declared fallback and cannot prove the referenced variant exists.

V1 requirement:

- `fallbackVariant` references one declared variant ID;
- semantic validator proves existence, workspace compatibility, and usable entrypoint;
- third-party packages cannot select the trusted platform Safe surface by pretending it is a package variant.

### 4.2 Trusted Safe is not a user-authored workspace

The manifest currently permits `safe` in package workspaces and variants.

Binding rule:

- the trusted Safe fallback is app-owned and independent of user/package assets;
- a Designer may preview `safe-preview` and validate recovery journeys;
- ordinary third-party/local `.emberskin` packages cannot replace or claim the trusted Safe implementation;
- official built-in packaging may use a reserved trust class/namespace enforced outside ordinary package permissions.

### 4.3 Variant and entrypoint cross-reference is not proven

The schema does not structurally prove:

- unique variant IDs;
- one appropriate entrypoint per declared variant;
- no undeclared entrypoint key;
- fallback entrypoint availability;
- valid minimum/maximum size/scale relationships;
- non-overlapping or deterministic priority resolution.

These require semantic validation and fixtures.

### 4.4 App/schema/registry compatibility is too shallow

Current manifest has minimum/maximum app version but no full version-range contract for:

- manifest/layout/binding/action/overlay schemas;
- Command Registry;
- State Registry;
- Component Registry;
- Capability Registry;
- Value/Unit/Target Registry;
- Theme Token Registry;
- compiler/runtime generation.

V1 package preflight needs explicit ranges/digests and exact deprecation/replacement diagnostics before activation.

### 4.5 Required versus optional dependencies/fallbacks are incomplete

Current fields include `requiredCommands`, `requiredStates`, `requiredComponents`, and `optionalCapabilities`, but no complete model for:

- required capabilities;
- optional command/state/component/action dependency;
- per-dependency version range;
- fallback behavior: hide, disable with reason, substitute component/layout, or reject package;
- action/action-pack and controller-profile dependencies;
- project target requirements;
- per-variant dependency differences.

`requiredComponents` also includes an `optional` flag, which blurs required/optional semantics. V1 should use one consistent requirement structure.

### 4.6 Base skin, fork, and overlay relationship is insufficient

Current provenance has `basedOn` as a single optional string.

V1 needs:

- base skin/package ID and compatible version range;
- overlay versus fork relationship;
- allowed base slots/regions/properties;
- source/base content digest;
- stable element references;
- conflict/migration policy;
- attribution/license/provenance;
- reset behavior;
- side-by-side install/update rules;
- no silent overwrite of bundled/third-party base packages.

Overlay should have its own manifest/schema rather than overloading full-skin semantics invisibly.

### 4.7 Ember Action and binding dependencies are absent

V1 manifest/package needs explicit declarations for:

- embedded/referenced `.emberaction` IDs/version ranges/digests;
- binding schema/version;
- controller profile recommendations versus required package content;
- action/command/state/capability closure;
- unresolved/missing action behavior;
- import/migration provenance.

A skin must not silently install or replace the user’s controller profile.

### 4.8 Content hash contract is undefined

The manifest requires `contentHash` but does not define:

- included/excluded files/fields;
- canonical path/file ordering;
- line endings/Unicode/number normalization;
- whether previews/authoring-only metadata affect runtime source hash;
- asset hash relationship;
- package versus compiled-artifact digest;
- compiler/registry generation cache key;
- mismatch behavior.

The installer/runtime recomputes integrity; it never trusts a supplied hash alone.

### 4.9 Provenance and trust metadata is too shallow

Current source values are `bundled | local | imported`, with optional `createdWith` and `basedOn`.

V1 needs bounded metadata for:

- official/local/forked/imported/migrated source;
- source ID/version/hash;
- migration adapter and rule version;
- exact/translated/approximated/opaque/unsupported/conflicted outcomes where relevant;
- author/license/attribution and redistribution permission;
- package signing/trust tier when later implemented;
- import/install/update time as local metadata outside digest where appropriate;
- no secrets, account data, serials, or absolute private paths in portable output.

`homepage`, asset `source`, and other URI-like provenance fields are metadata only. The runtime never fetches them while loading/painting/performing.

### 4.10 Designer source project is not a runtime manifest concern

History, autosave, source references, simulation scenarios, validation suppressions, editable asset sources, base merge state, and export targets belong to a separate Designer project manifest. Do not bloat the lean runtime skin manifest or make Perform load authoring metadata.

## 5. Reference example audit implications

The current Reference Live example demonstrates the expected layout but includes mixed registry status:

Implemented/shared examples:

```text
project.active.name
transport.bpm
override.activePropertyCount
output.workLight
output.blackout
output.workLight.toggle
output.blackout.toggle
override.releaseAll
```

Seed-planned, not currently native-facade/state implemented:

```text
view.panel.open
workspace.open
connection.dj.status
controller.status
```

Undefined or incorrectly scoped:

```text
output.summary.status
view.reference.live.page
```

Native component types are also planning contracts until the Component Registry/runtime exists.

Therefore:

- examples remain design/contract fixtures;
- package/runtime tests must validate every reference against generated registries;
- planned/unimplemented IDs cannot masquerade as working controls;
- view-local page state must be modeled explicitly;
- Reference visual implementation waits for the required #31/#32/#64/#59 contracts rather than adding private state.

## 6. Recommended v1 schema set

Exact names may change, but responsibilities should be separated:

```text
skin-manifest-v1.schema.json
overlay-manifest-v1.schema.json
designer-project-v1.schema.json
layout-source-v1.schema.json
component-source-v1.schema.json
binding-source-v1.schema.json
shared-expression-v1.schema.json
theme-source-v1.schema.json
localization-manifest-v1.schema.json
asset-manifest-v1.schema.json
package-dependency-v1.schema.json
compiled-view-graph-v1.schema.json or internal equivalent
```

Reuse the accepted Ember Action schema/IR rather than creating layout-specific action macros.

## 7. Compatibility and migration strategy

1. Keep v0 schemas and examples immutable as golden input fixtures.
2. Add explicit v0 → v1 migrators in Studio/package tooling.
3. Retain original source/package read-only and record digest/provenance.
4. Produce exact migration report:

```text
kept
renamed with compatible replacement
translated
approximated
opaque/preserved
unsupported
conflicted
manual review
```

5. Do not auto-activate a migrated package until v1 validation/qualification passes.
6. Preserve stable element/control IDs through migration.
7. Missing targets/dependencies become unavailable/relinkable, never silently redirected.
8. A failed migration/update retains the current valid package and base.
9. Canonical source and generated runtime output have separate version/digest identities.

## 8. Required fixtures

### Positive

- minimal Live skin with trusted platform safety overlay integration;
- Default/Reference compact and standard variants;
- custom variant ID used as declared fallback;
- overlay adding one Static Look pad to a declared slot;
- reusable component with typed properties/events/slots;
- typed command binding and selector feedback;
- Ember Action binding;
- view-local tab/page state;
- bounded repeater context;
- optional capability hidden/disabled/substituted cases;
- base skin compatible update/migration;
- imported package with provenance and local-path redaction.

### Negative

- fallback references missing variant;
- duplicate variant/element/component IDs;
- third-party package attempts to own trusted Safe;
- untyped/oversized predicate, argument, component property, or state value;
- unknown command/state/component/capability/action/token;
- planned but unimplemented command used as active required control;
- component property/event/slot mismatch;
- recursive component expansion;
- inheritance/base/overlay cycle;
- overlay edits undeclared or mandatory region;
- missing mandatory Blackout/Stop/Work Light/Release All/health path;
- inaccessible focus/target/contrast/non-color state;
- variant size/scale relationship invalid;
- entrypoint/variant mismatch;
- package/asset/content hash mismatch;
- external URL/entity/code/device request;
- excessive nodes/depth/subscriptions/assets/decoded pixels;
- invalid v0 migration preserving current valid package.

## 9. Issue ownership

- **#64:** registry version/range/digest/value/expression/component/capability dependency source and generated cross-reference gate.
- **#65:** Ember Action schema/IR and shared typed action expressions; do not duplicate inside layout.
- **#32:** package reader/validator/compiler, immutable View Graph, Safe fallback, activation, limits, runtime component adapters.
- **#66:** binding source/editor, view-local control/page state, custom slots/pages, overlays, MIDI/HID Learn and feedback.
- **#67:** Designer source project, responsive inheritance, reusable components, theme/assets/localization, simulation/history/package authoring.
- **#72:** artifact lifecycle, install/update/preflight/trust/provenance/rollback and release qualification.

Shared schema changes require exact reservations and additive compatibility. Do not let one issue silently redefine another issue’s registry/action/runtime authority.

## 10. Acceptance before V1 schema freeze

- source, package, overlay, Designer, binding/action, and compiled runtime responsibilities are explicit;
- every generic value/expression/property is bounded and typed;
- all registry/component/capability/action dependencies resolve against versions/generations;
- Safe remains trusted and unreplaceable by ordinary packages;
- arbitrary custom variants and valid fallbacks work deterministically;
- reusable components/variant inheritance/overlays are acyclic and bounded;
- view-local state is explicit and cannot collide with domain state;
- v0 inputs migrate with exact reports and preserved originals;
- invalid load/update/migration retains current surface and Runner/DMX;
- required responsive, accessibility, mandatory-control, performance, and abuse fixtures pass;
- compiled runtime artifacts remain bounded, immutable, toolkit-neutral publicly, and lean enough for the approved Perform envelope.
