# UI Registry Lifecycle and Compatibility Policy

Status: **binding cross-agent policy** for every user-visible EmberLights feature.

This policy exists so skins, overlays, Ember Actions, keyboard/MIDI/controller profiles, bundled surfaces, tests, documentation, and migration tools remain current when application behavior changes.

## 1. Core invariant

No user-visible behavior may exist only as a hard-coded UI callback, widget-local state mutation, controller-specific handler, or skin-specific implementation.

Every supported surface routes through shared versioned contracts:

```text
Binding/Event
  -> registered Command or validated Ember Action
  -> authoritative service/domain behavior
  -> registered State/result feedback
```

Skins are presentation and binding packages. Ember Actions are bounded compositions. Neither is an alternate engine.

## 2. Registry set

The platform maintains at least the following registries:

| Registry | Authority |
| --- | --- |
| **Command** | Callable product actions, arguments, results, safety, realtime class, persistence, Undo, and availability. |
| **State** | Observable authoritative facts, value schema, update class, snapshot group, privacy, and lifecycle. |
| **Component** | Primitive/native component contracts, properties, events, slots, states, capabilities, performance class, and version. |
| **Capability** | Optional feature/adapter/project/platform availability and dependency relationships. |
| **Interaction** | Supported surface event/gesture/value behaviors and compatibility rules. |
| **Theme Token** | Semantic visual roles and required state/safety meaning. |
| **Value/Schema** | Reusable argument, state object, target, enum, unit, and collection schemas. |
| **Invocation Result** | Stable command/action outcome vocabulary. |

One skin or controller profile cannot publish a private competing registry.

## 3. Canonical source of truth

The accepted implementation chooses one canonical machine-readable or generated-native source. It must be capable of producing deterministic artifacts.

Required generated outputs:

- C++ IDs/enums/types/lookup tables or equivalent native definitions;
- runtime compact lookup metadata appropriate to each process;
- JSON registry snapshots for package/action/mapping validation;
- JSON Schemas and referenced value schemas;
- Command/State/Component/Capability Explorer catalogs;
- designer property editors, pickers, ranges, units, and documentation;
- compatibility/deprecation/alias maps;
- Markdown or generated reference documentation;
- cross-reference manifests for bundled skins, overlays, actions, and profiles;
- deterministic registry digest/version metadata;
- test fixtures and example stubs where appropriate.

The DMX scheduler never parses full JSON registries or performs dynamic string resolution on the hot path.

## 4. Registry metadata minimums

### Command

Every command definition includes:

- stable ID;
- registry generation introduced;
- human label/description/localization keys;
- scopes/surfaces permitted;
- interaction kinds supported;
- realtime class;
- typed arguments, units, ranges, schemas, target kinds, and defaults;
- availability/capability predicate;
- safety class and core gate;
- invocation result contract;
- authoritative feedback states;
- persistence scope;
- Undo/transaction behavior;
- parallel/action-composition compatibility;
- privacy/diagnostic considerations;
- deprecation/replacement/removal metadata.

### State

Every state definition includes:

- stable key;
- registry generation introduced;
- value type/schema/enum/unit;
- authority domain and lifecycle scope;
- update class and maximum publication rate;
- coherent snapshot group;
- bounded collection/string/object limits;
- sensitive/privacy flag and redaction policy;
- owner-facing description/localization where applicable;
- deprecation/replacement/removal metadata.

### Component

Every component definition includes:

- stable ID and contract version;
- primitive or native-complex classification;
- supported workspaces/variants/input modes;
- properties/events/slots and types;
- commands/actions it may invoke through bindings;
- state/context it exposes or consumes;
- required/optional capabilities;
- accessibility semantics;
- performance/update class and virtualization rules;
- failure/fallback behavior;
- deprecation/replacement/removal metadata.

### Capability

Every capability includes:

- stable ID;
- category and description;
- app/platform/project/adapter origin;
- required dependencies and conflicts;
- whether absence hides, disables, degrades, or rejects a feature;
- relevant commands/states/components;
- version/qualification status;
- experimental/unverified/qualified metadata where applicable.

## 5. Stable ID rules

- IDs describe product semantics, not coordinates, Win32 control numbers, display labels, controller model names, or list indices.
- IDs are case-sensitive canonical strings with one documented normalization policy.
- An ID is never silently reused for different semantics.
- Display/localization text may change without changing IDs.
- Stable project targets use IDs or approved semantic roles, never visual position.
- Array/list indices are not public identities.
- Vendor names appear only when the command/state/capability genuinely manages that adapter/source.
- Explicit `set/start/stop/activate/release` forms are preferred where idempotence matters; `toggle` is additive for human surfaces, not the only automation contract.
- Emergency/priority behavior is metadata and implementation authority, not inferred only from the name.

## 6. Registry generations and schema versions

Track independently:

- application version;
- registry-set generation;
- command registry major/minor generation;
- state registry major/minor generation;
- component registry major/minor generation;
- capability registry major/minor generation;
- skin/layout/binding/action/profile schema versions;
- native component contract versions;
- compiler/generator version.

### Compatible minor change

Examples:

- add optional command/state/capability/component;
- add optional metadata field with default;
- add enum value only when consumers declare unknown-value behavior;
- add optional command argument with compatible default;
- clarify descriptions without changing semantics.

### Breaking major change

Examples:

- remove or reuse an ID;
- change argument/state type or unit incompatibly;
- change command ownership/result/safety/persistence/realtime semantics;
- make optional behavior mandatory without fallback;
- change component property/event/slot meaning incompatibly;
- change target identity semantics;
- reduce a supported range in a way that invalidates existing artifacts without migration.

Breaking changes require an accepted decision, migration tooling or explicit pre-public reset, compatibility fixtures, and owner-visible release notes.

## 7. Deprecation and replacement

A deprecated definition records:

```text
introducedGeneration
deprecatedGeneration
replacementId or null
replacementCompatibility
argument/state conversion rule
warning severity
plannedRemovalGeneration or undecided
migration notes
```

Automatic replacement is allowed only when:

- semantics are equivalent for the artifact's usage;
- argument and state conversions are deterministic and lossless or explicitly approved;
- safety, realtime, persistence, Undo, and result behavior do not weaken or change silently;
- the converted artifact validates completely;
- the migration report records the replacement.

Otherwise the artifact remains unavailable/conflicted and requires owner review.

Aliases are temporary compatibility entries, not duplicate canonical definitions.

## 8. Required feature-change matrix

Every PR/commit that adds, removes, renames, or materially changes user-visible behavior must reconcile this matrix.

| Change area | Required action |
| --- | --- |
| Domain behavior | Add/change registered command; preserve one implementation. |
| Observable result/status | Add/change authoritative registered state or invocation result. |
| Availability | Add/change capability and predicate. |
| UI exposure | Use registered primitive/native component property/event and shared binding. |
| Keyboard/MIDI/controller | Reuse same command/action and state feedback; update profile tests. |
| Persistence | Declare app/project/controller/live-transient scope and restart behavior. |
| Studio edit | Declare Undo/transaction/validation behavior. |
| Runner/Live | Declare realtime/queue/priority/timing behavior. |
| Safety | Declare safety class/gate and mandatory-control impact. |
| Deprecation/removal | Add replacement/migration or approved breaking decision. |
| Bundled surfaces | Update Default/Reference/Safe or document intentional non-exposure. |
| Designer | Ensure Explorer/pickers/components/templates generate correctly. |
| Actions | Validate affected action fixtures and compatibility maps. |
| Migration | Update source-to-canonical mapping/report classification where affected. |
| Tests/docs/backlog | Update narrow tests, examples, decision/issue/continuity records. |

A code review description must not merely say “no UI changes” when the change alters a command, state, capability, component contract, persistence, safety, or action compatibility.

## 9. Direct-callback bypass policy

Temporary hard-coded UI/controller callbacks are allowed only as explicit strangler bridges.

Each bypass ledger entry includes:

- source file/function/control/handler;
- behavior and authoritative service called;
- missing command/state/capability/component contract;
- reason it cannot use the facade yet;
- owner issue;
- safety/realtime/persistence risk;
- test coverage;
- planned removal gate;
- creation and last-reviewed dates.

Rules:

- no new bypass without ledger entry and issue;
- no Reference-only or Default-only domain behavior;
- bypass count must trend downward after facade work starts;
- emergency paths may retain special delivery but still publish registry metadata and tests;
- CI checks known callback/handler locations against the ledger where practical;
- expired/unowned bypasses fail the platform gate.

## 10. Surface Contract Gate

The project should add a required CI/test aggregate named conceptually:

```text
surface-contract-gate
```

It runs narrow deterministic checks suitable for ordinary feature PRs and broader compatibility fixtures at release gates.

### Required ordinary checks

1. Canonical registry schema validation.
2. Deterministic code generation and clean generated diff.
3. Unique IDs and canonical naming rules.
4. Command/state/component/capability cross-reference integrity.
5. Type/unit/range/schema/picker integrity.
6. Deprecation/replacement graph validity and no cycles.
7. Bundled skin/layout/binding/action/profile references resolve.
8. Safe mandatory command/state references resolve.
9. Example artifacts validate.
10. No unexpected direct-callback bypass additions.
11. Registry version/digest changes match semantic impact.
12. Generated documentation/Explorer catalog is current.

### Release/major checks

- current plus supported prior registry fixtures;
- artifact migration preview/golden reports;
- Default/Reference/Safe full validation;
- controller-profile compatibility matrix;
- action visual/text/canonical round-trip;
- package/action fuzz and abuse suites;
- full DPI/accessibility/performance/DMX-continuity matrix;
- installed cache invalidation and last-known-good behavior.

## 11. Compatibility diff artifact

Every registry-changing build generates a machine-readable and human summary:

```text
added
changedCompatible
deprecated
replacementAvailable
changedBreaking
removed
unchanged
affectedBundledArtifacts
affectedFixtureArtifacts
affectedExternalArtifactCount when scanned
migrationAvailable
manualActionRequired
```

The diff compares against:

- previous main/release registry;
- current supported compatibility baseline(s);
- optional owner-selected installed artifact catalog during local upgrade preview.

Do not infer compatibility from version numbers alone; compare semantic metadata and referenced schemas.

## 12. Registry coverage

Coverage is not “every command appears in every skin.” It means every supported behavior is inspectable and every required product journey has a reachable surface.

Track:

- implemented user-callable behaviors with command metadata;
- implemented observable facts with state metadata;
- accepted components/capabilities;
- current hard-coded bypasses;
- Default/Reference/Safe exposure by journey;
- keyboard/MIDI/controller profile exposure where required;
- test invocation and feedback coverage;
- unexposed commands with explicit rationale;
- fake/dead controls prohibited.

A feature may be intentionally unavailable from a surface, but that decision must be explicit and capability-aware.

## 13. Generated Explorer catalogs

Command/State/Component/Capability Explorer data is generated from registries and includes:

- label/description and stable ID;
- category/search tags;
- supported workspaces/surfaces/interactions;
- arguments/value schema/units/ranges/defaults/pickers;
- availability/capability requirements;
- realtime/safety/persistence/Undo/transaction metadata;
- invocation results and feedback states;
- component properties/events/slots;
- usage examples/templates;
- current bindings/references where available;
- introduced/deprecated/replacement/version information;
- Add to custom panel / Add to action where compatible.

No separate handwritten list may become more authoritative than the registry.

## 14. Bundled artifact policy

The following are first-party compatibility fixtures, not disposable examples:

- Safe surface;
- EmberLights Default;
- SoundSwitch Reference;
- committed example layouts/bindings;
- bundled controller profiles;
- built-in Ember Actions/templates;
- registry fixture snapshots;
- representative overlay/custom-page fixtures.

A registry change must validate them or update them in the same bounded change. Updating goldens is not acceptable without reviewing semantic differences.

## 15. External artifact scan

The installed application should eventually offer an upgrade preflight that scans installed:

- `.emberskin`;
- `.emberoverlay`;
- `.emberaction` and action packs;
- controller profiles;
- designer-source projects where indexed;
- project recommendation manifests.

It reports compatibility before activation/update and preserves the current valid version on failure.

The repository CI uses synthetic/public fixtures only, never private user files.

## 16. Migration adapter maintenance

Source adapters map source-specific concepts to canonical IDs through versioned mapping tables.

When canonical behavior changes, the same PR/issue must consider:

- source mapping exactness;
- migration classification;
- preserved unknown fields;
- relink/conflict UX;
- deterministic re-import;
- report wording and confidence;
- source adapter fixture versions.

Do not silently upgrade `approximated` to `exact` without evidence.

## 17. Ownership and coordination

- Issue #31 or its accepted successor owns canonical command/state names during registry foundation work.
- The skin-runtime owner owns package compilation/activation, not domain semantics.
- Skin agents own layouts/themes/assets/bindings, not private commands.
- Action-system agents own schema/compiler/executor, not raw engine behavior.
- Feature agents own registry reconciliation for behavior they add/change.
- Qualification agents own baselines and may not silently bless drift.
- Migration agents own source evidence/mappings, not vendor-specific runtime engines.

Agents reserve exact shared registry/schema/generated files before editing and rebase dependent work after accepted registry changes.

## 18. Emergency/hotfix process

A critical hardware/safety/output hotfix may land before a complete new UI surface, but it still must:

- preserve the independent emergency path;
- update or reserve canonical command/state/capability metadata in the same hotfix where practical;
- add a temporary explicit bypass entry only when unavoidable;
- create a bounded follow-up issue before completion;
- avoid publishing incompatible IDs casually;
- update Safe/diagnostic state when the user must know the behavior;
- pass narrow safety and compatibility tests.

The hotfix exception cannot become a permanent untracked alternative path.

## 19. Completion report requirement

Every user-visible feature agent reports:

- commands added/changed/deprecated/removed;
- states added/changed/deprecated/removed;
- capabilities/components/theme tokens affected;
- registry/schema/generator version impact;
- bundled skin/Safe/profile/action/migration impact;
- direct bypasses added/removed;
- exact tests and generated-diff result;
- compatibility classification;
- migration/deprecation plan;
- remaining risks and next dependency.

“UI updated” or “feature works” is insufficient.

## 20. Policy acceptance

1. One canonical registry source generates native and tooling artifacts deterministically.
2. Every user-visible feature change reconciles the full Surface Contract matrix.
3. CI fails stale generated artifacts, broken references, unapproved breaking changes, and unexpected bypasses.
4. Stable IDs are not reused; deprecations and replacements are explicit.
5. Default, Reference, Safe, examples, actions, profiles, and compatibility fixtures remain valid or are intentionally migrated.
6. Designer/Explorer catalogs always reflect the accepted registry.
7. External artifacts receive preflight and exact compatibility diagnostics.
8. Migration adapters remain evidence-classified and source-preserving.
9. Runtime hot paths do not parse general registries or source artifacts.
10. Future agents can add/remove/modify capabilities without silently breaking the skins platform.
