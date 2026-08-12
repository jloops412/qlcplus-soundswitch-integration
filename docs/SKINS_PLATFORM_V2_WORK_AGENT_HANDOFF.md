# Skins Platform V2 — Work-Agent Handoff

Status: **planning complete; ready for bounded implementation after this planning branch is reviewed/merged**.

Planning branch:

```text
planning/skins-platform-v2-2026-08-12
```

Parent epic: **#63**.

## 1. Mission

Build the registry-governed skins platform defined by:

- `SKINS_PLATFORM_V2_START_HERE.md`;
- `SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`;
- ADR 0006;
- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`;
- `spec/ui/ember-actions-contract-v1.md`;
- `spec/ui/skin-designer-contract-v1.md`.

Do not create a second UI architecture, competing command IDs, an arbitrary script VM, skin-specific lighting semantics, or a designer dependency in lean Perform.

## 2. Current dependency reality

The repository already contains:

- a narrow native `UiCommandId`/`UiCommandFacade` implementation;
- a narrow native `UiStateDefinition`/`LiveCoreUiState` implementation;
- planning-seed JSON command/state registries and schemas;
- declarative skin/package/layout/component contracts and examples;
- issue #31 command/state facade ownership;
- issue #32 `.emberskin` runtime/Safe ownership;
- issue #37 toolkit decision gate;
- issue #39 first custom-control/overlay scope;
- issues #57–#59 Autoloop V2 planning/runtime-state coordination;
- issue #38/core-ready gates that still constrain broad product rollout.

The first implementation task is therefore **reconciliation and generation**, not invention of a new parallel list.

## 3. Work map

| Stable ID | Issue | Suggested branch | Main deliverable | Hard dependencies |
| --- | ---: | --- | --- | --- |
| SKIN2-001 | #64 | `agent/skin2-001-registry` | Canonical registries, codegen, diff, Surface Contract Gate | coordinate #31; preserve current behavior |
| SKIN2-002 | #65 | `agent/skin2-002-ember-actions` | Action schema/IR/compiler/executor/diagnostics/text round-trip | accepted #64/#31 contracts; runtime boundary #32 |
| SKIN2-003 | #66 | `agent/skin2-003-custom-controls` | Binding editor, Learn/feedback, custom pages, overlays | #31/#64, #37, #32, relevant #35/#36; #65 for action editing |
| SKIN2-004 | #67 | `agent/skin2-004-skin-designer` | Full responsive visual designer and packaging | proven #32/#64/#65/#66 seams |
| SKIN2-005 | #68 | `agent/skin2-005-reference-kit` | Forkable SoundSwitch-familiar templates | #30/#34 evidence, shared registries/runtime, Autoloop contracts |
| SKIN2-006 | #70 | `agent/skin2-006-migration` | UI/mapping/action migration IR; VirtualDJ adapter first | #64/#65; #67 import/review surface for full UX |
| SKIN2-007 | #72 | `agent/skin2-007-lifecycle-qualification` | Artifact manager, preflight, trust/provenance, qualification | representative #64–#70 artifacts and #32 installed runtime |

Use the exact prepared branch when an owner/manager supplies one. The names above are defaults only; do not create a conflicting branch if one already exists.

## 4. Recommended execution passes

### Pass A — Registry spine

Primary: **#64**.

May run in parallel only with behavior-preserving #31 work and isolated #65 schema/fixture validation.

Deliver:

1. Reconcile native definitions, planning seeds, schemas, current callbacks/status, and in-flight #31/#59 contracts.
2. Select one canonical registry source and record the decision.
3. Generate native IDs/types/lookups plus JSON/tooling catalogs deterministically.
4. Add command/state/component/capability/value/result metadata completeness.
5. Add deprecation/replacement/version/digest/diff support.
6. Validate Safe/Default/Reference examples, bindings, profiles, and action fixtures.
7. Add the narrow `surface-contract-gate` and direct-bypass ledger guard.
8. Preserve current behavior and exact invocation/state tests.

Do not begin broad visual editor work before this spine is usable.

### Pass B — Ember Actions core

Primary: **#65**.

Can begin schema/normalization/fuzz fixtures before Pass A completes, but runtime resolution must rebase onto accepted generated registries.

Deliver:

1. Refine and validate `ember-action.schema.json`.
2. Implement canonical normalization/hash/dependency manifest.
3. Implement typed graph validation and budgets.
4. Compile immutable Action IR.
5. Execute on bounded UI/control service only.
6. Preserve typed results/diagnostics/feedback.
7. Implement import/export/version/deprecation/relink preview.
8. Add expert parser/formatter only with visual/canonical round-trip equality.
9. Measure latency/memory/jitter and active-DMX neutrality.

Do not add UI sleeps, arbitrary state triggers, plugins, raw devices, or private domain semantics.

### Pass C — First user authoring slice

Primary: **#66**, expanding #39.

Requires #32 runtime/Safe and #37 toolkit evidence for production UI, plus stable generated registries. A bounded non-production model/editor harness may be developed earlier if it remains toolkit-neutral.

Deliver:

1. Binding editor and Command/State/Action Explorer integration.
2. Keyboard/MIDI/HID Learn, transforms, soft takeover, and feedback.
3. Base-declared custom slots.
4. Visual add/arrange/configure for ordinary controls.
5. Named custom pages/pad banks.
6. Overlay autosave/Undo/validate/activate/reset/import/export/relink.
7. Deterministic simulation and explicit safe Test action.
8. Compact/standard/touch qualification and mandatory-control checks.

This pass is the first owner-visible customization milestone.

### Pass D — Full designer and Reference kit

Primary: **#67** and **#68** in parallel after shared contracts stabilize.

File ownership must be separate:

- #67 owns generic designer/source/layout/theme/component/action authoring infrastructure;
- #68 owns Reference package/templates/original assets/evidence/goldens, not generic infrastructure or domain behavior.

Deliver complete responsive authoring and a forkable SoundSwitch-familiar Performance starting point.

### Pass E — Migration and release lifecycle

Primary: **#70** and **#72**.

- #70 first proves the normalized read-only migration IR and VirtualDJ evidence parser/mapping fixtures.
- #72 consumes representative artifacts for side-by-side install/update/preflight/recovery/security/performance qualification.

A public marketplace remains outside this program until separately approved.

## 5. Shared-file reservation rules

Before editing, each agent posts an issue comment with exact files and reserves them according to the project agent model.

### High-conflict files

Coordinate explicitly before touching:

```text
native-core/include/emberlights/ui_command.hpp
native-core/src/ui_command.cpp
native-core/include/emberlights/ui_state.hpp
native-core/src/ui_state.cpp
native-core/src/windows_app.cpp
native-core/CMakeLists.txt
spec/ui/schema/*.json
spec/ui/registry/*
docs/UI_PROGRAM_START_HERE.md
docs/21_UI_IMPLEMENTATION_PROGRAM.md
docs/26_UI_QUALIFICATION_MATRIX.md
```

### Ownership intent

#### #64

Owns canonical registry source, generator, generated artifacts, registry schemas/tests/diff, Explorer catalog generation, and bypass ledger. Coordinates changes to existing `ui_command`/`ui_state` files with #31 and feature owners.

#### #65

Owns action schema/source model/compiler/IR/executor/diagnostics/tests and action fixtures. It does not own command/state naming or raw Runner semantics.

#### #66

Owns overlay/custom-control/binding editor source, overlay schema/runtime integration, and profile-editing UX. It consumes generated catalogs and action APIs.

#### #67

Owns generic designer source project/canvas/hierarchy/constraints/theme/assets/localization/simulation/package build infrastructure.

#### #68

Owns SoundSwitch Reference templates/package/theme/original assets/evidence/deviation/goldens only.

#### #70

Owns migration IR, source adapters/parsers/rule tables/reports/synthetic fixtures. It cannot mutate source or own runtime behavior.

#### #72

Owns artifact lifecycle/preflight/trust/cache/recovery and release qualification fixtures/reports.

## 6. Required first update on every issue

Post before coding:

```text
Bounded deliverable:
Dependency gate:
Files/contracts reserved:
Commands/states/components/capabilities/schema generations expected to change:
Explicit non-goals:
Narrow tests:
Completion evidence:
```

Do not post vague “working on UI” scope.

## 7. #64 first-agent exact startup

1. Read `AGENTS.md` and `docs/00_START_HERE.md`.
2. Read `docs/UI_PROGRAM_START_HERE.md` and `docs/SKINS_PLATFORM_V2_START_HERE.md`.
3. Read issue #64, #31, #59, ADR 0006, the registry policy, command/state contract, registry README/seeds/schemas, current native command/state headers/sources/tests, and current callback inventory.
4. Inspect current branches/issues for in-flight registry changes before selecting files.
5. Post the required issue scope/reservation comment.
6. Build a complete reconciliation table before renaming or generating anything.
7. Preserve the canonical blackout/work-light names and current tested behavior unless an accepted compatibility decision says otherwise.
8. Implement the smallest end-to-end generated slice first: one canonical source -> native definitions -> JSON catalog -> validation test -> clean-generation check.
9. Expand by domain with narrow tests.
10. Leave the current runtime usable after every commit.

### First slice acceptance

- at least current Live core command/state set represented in the canonical source;
- generated native definitions compile;
- generated JSON validates;
- current native facade/state tests still pass;
- duplicate/unknown/stale generation tests pass;
- one compatible-additive and one breaking-diff fixture classify correctly;
- no new direct callback bypass;
- no Runner scheduler JSON parsing/allocation added.

## 8. #65 safe parallel startup

Before #64 is fully merged, #65 may own only isolated files for:

- JSON Schema syntax/fixture validation;
- canonical source normalization design;
- graph cycle/depth/node/size tests;
- parser/formatter round-trip fixtures using mock registry interfaces;
- compiler interfaces that accept an injected immutable registry view.

It must rebase before runtime integration and cannot freeze command/state metadata independently.

## 9. Testing policy

### Per-change narrow tests

- schema and canonicalization fixtures;
- generated-diff cleanliness;
- relevant native command/state/action/compiler unit tests;
- exact binding/component/overlay fixture validation;
- no-allocation/scheduler isolation where touched;
- deterministic hashes/replay.

### Gate tests

- complete first-party artifact cross-reference;
- package/action/parser fuzz/resource abuse;
- previous-registry compatibility/migration fixtures;
- DPI/accessibility/goldens;
- startup/memory/CPU/repaint/subscription/action-dispatch;
- repeated valid/invalid activation/switch/update/recovery;
- VirtualDJ co-load/reference machine;
- active-DMX continuity and scheduler jitter.

Do not run full soak/golden matrices after every cosmetic edit. Do not skip them at the documented gate.

## 10. Stop and escalate conditions

Stop the bounded lane and update the issue when:

- another active agent owns the exact shared file/ID/schema;
- #31/#59 accepted command/state names conflict with the plan;
- implementation would require arbitrary code execution or direct device/file/network access;
- an action requires UI timing to own musical behavior;
- a skin/template would require private Reference-only domain semantics;
- the toolkit cannot satisfy the public contract without leaking toolkit-specific schemas;
- a proposed “compatible” change alters safety, persistence, realtime, target, or result semantics;
- source licensing does not permit fixture/asset redistribution;
- broad work would delay #38/core-ready gates;
- physical hardware support would be claimed without physical evidence.

Document the exact conflict and propose the smallest contract-level resolution. Do not create a parallel workaround.

## 11. Completion evidence

Every work issue leaves:

- exact deliverable and issue/gate;
- branch and commit/PR;
- files/contracts changed;
- commands/states/components/capabilities/schema/generations changed;
- generated-artifact and compatibility-diff result;
- bundled skin/Safe/action/profile/migration impact;
- direct bypasses added/removed/remaining;
- exact tests and results;
- performance/DPI/accessibility/fuzz/DMX evidence where applicable;
- unresolved risks and intentional non-goals;
- next dependency/issue.

## 12. Planning completion statement

The architecture, contracts, ADR, issue decomposition, dependencies, safety boundaries, compatibility governance, and work sequencing are sufficient to begin implementation. The recommended first work agent is **#64 / SKIN2-001**, coordinated with the existing #31 command/state owner. A second isolated agent may begin the safe schema/fixture portion of **#65**.
