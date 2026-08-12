# Skins Platform V2 — Continuity Checkpoint

- **Checkpoint date:** 2026-08-12
- **Planning status:** Complete; ready for bounded work-agent implementation after review/merge.
- **Planning branch:** `planning/skins-platform-v2-2026-08-12`
- **Parent epic:** #63
- **Primary first implementation issue:** #64 / SKIN2-001
- **Safe isolated parallel issue:** #65 / SKIN2-002 schema, canonicalization, and fixture work only until #64/#31 contracts stabilize

## Owner directive preserved

EmberLights must provide a first-class skins platform—not a fixed SoundSwitch clone.

Users must eventually be able to:

- create, fork, import, export, install, update, and share compatible skins;
- visually design interfaces with responsive grids/constraints, reusable components, color/theme tools, icon/asset pickers, localization, state previews, and validation;
- bind controls to product behavior, keyboard, MIDI/HID/controllers, DJ commands, and future approved remotes through one shared model;
- begin from a SoundSwitch-familiar Performance/Autoloop/Static Look workflow and modify it freely;
- use a VirtualDJ-inspired action vocabulary without being forced to hand-author fragile script strings and package files;
- migrate compatible source artifacts from other products through evidence-first adapters over time.

Most importantly, the skin/action platform must stay current whenever agents add, remove, rename, or change application features.

## Accepted architecture

```text
surface event/value/context
  -> validated Binding
  -> optional compiled Ember Action
  -> registered typed Command
  -> Studio service or Runner command boundary
  -> authoritative domain state/result
  -> registered State snapshot
  -> software/controller feedback
```

### Artifact model

- `.emberskin` — complete validated presentation/binding package.
- `.emberoverlay` — customization over an immutable base skin.
- `.emberaction` — reusable typed bounded action graph.
- controller profile — hardware/keyboard input and feedback mapping independent of skin/project content.
- designer source project — editable Studio-side authoring source that compiles into distributable artifacts.

### Ember Actions

- Visual Action Graph is the primary authoring form.
- Optional expert Ember Action Script must round-trip to the same canonical Action IR.
- No JavaScript, Lua, WebAssembly, native plugins, shell execution, runtime `eval`, hidden globals, direct device/file/network access, or direct state writes.
- No UI sleep/timer becomes musical/show timing.
- Runner/domain commands own beat/bar/phrase scheduling, repeats, fades, transitions, ownership, and return behavior.
- Action execution occurs on an approved bounded UI/control service, never the DMX scheduler.

### Runtime boundary

Lean Perform loads only validated compiled:

- immutable View Graph;
- Action IR;
- Binding Tables;
- native component adapters;
- bounded state subscriptions.

The visual designer, source importers, asset tools, migration workers, and source parsers remain outside the lean Runner path. Skin/action/import/designer failure cannot stop DMX or weaken Safe, Blackout, Stop, Work Light, Release All, health, or safety authority.

## Permanent anti-drift rule

Every user-visible feature change must reconcile, in the same bounded work:

1. command definitions and typed arguments;
2. authoritative state definitions and update classes;
3. component/capability exposure;
4. persistence, Undo/transaction, realtime, priority, and safety metadata;
5. deterministic generated native and JSON registry artifacts;
6. Default, SoundSwitch Reference, and Safe impact;
7. keyboard/MIDI/controller bindings and feedback;
8. Ember Actions, examples, templates, schemas, and tests;
9. deprecation/replacement/compatibility metadata;
10. source-migration mappings and diagnostics where affected;
11. backlog, issue, decision, and continuity records.

A hard-coded UI callback is not a complete platform feature. Removing code is not complete while skins, overlays, actions, mappings, profiles, examples, or generated catalogs still reference it.

## Binding repository artifacts

### Entry, architecture, and decision

- `docs/SKINS_PLATFORM_V2_START_HERE.md`
- `docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`
- `docs/adr/0006-registry-governed-skins-and-ember-actions.md`

### Contracts

- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
- `spec/ui/ember-actions-contract-v1.md`
- `spec/ui/schema/ember-action.schema.json`
- `spec/ui/skin-designer-contract-v1.md`
- existing command/state/skin/package/component/customization contracts under `spec/ui/`

### Execution

- `docs/SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md`
- this checkpoint
- issue #63 and child issues #64, #65, #66, #67, #68, #70, and #72

### Mandatory agent routes updated

- `AGENTS.md`
- `docs/00_START_HERE.md`
- `docs/UI_PROGRAM_START_HERE.md`
- `docs/AGENT_BOOTSTRAP_PROMPT.md`

## Issue map

| Issue | Stable work ID | Purpose |
| ---: | --- | --- |
| #63 | SKIN2 epic | Overall visual designer, actions, import/export, and continuity program |
| #64 | SKIN2-001 | Canonical registries, deterministic generation, compatibility diff, and Surface Contract Gate |
| #65 | SKIN2-002 | Ember Action schema, IR, compiler, executor, diagnostics, and expert round-trip |
| #66 | SKIN2-003 | Binding editor, custom controls/pages, overlays, MIDI/HID Learn and feedback; expands #39 |
| #67 | SKIN2-004 | Full responsive visual Skin Designer and package authoring |
| #68 | SKIN2-005 | Forkable SoundSwitch-familiar Performance/Autoloop/Static Look Reference kit; coordinates #34 |
| #70 | SKIN2-006 | Canonical migration IR and evidence-first VirtualDJ skin/mapping/action adapter first |
| #72 | SKIN2-007 | Artifact lifecycle, compatibility preflight, trust/provenance, recovery, and release qualification |

## Dependency order

```text
#38 core-ready ------------------------------------------------> broad rollout
#31 command/state + #64 registry spine
#37 toolkit -------------------------------> #32 runtime/Safe
                                             -> #33 Default + #34 Reference
                                             -> #35 persistence + #36 qualification
                                             -> #39/#66 custom controls and overlays
#65 Action core ----------------------------> #66 action editing
#66 proven editor/runtime seam ------------> #67 full designer
#30/#34 evidence + shared runtime ----------> #68 Reference kit
#64/#65 + #67 import/review ----------------> #70 migration
representative #64–#70 artifacts ----------> #72 lifecycle/release qualification
```

Registry schemas/generators/compatibility fixtures, action-schema/compiler fixtures, migration-IR planning, and read-only evidence parsing may proceed earlier when isolated, behavior-preserving, and coordinated. Broad cosmetic/product rollout remains gated.

## First work-agent handoff

### Start #64 first

The first agent must reconcile rather than replace:

- current native `UiCommandId`, `UiCommandDefinition`, `UiCommandFacade`;
- current `UiStateDefinition`, `LiveCoreUiState`;
- issue #31 in-flight ownership;
- registry seeds and JSON schemas;
- current Win32 callbacks/state and bypass inventory;
- Autoloop state work under #59;
- Safe/Default/Reference examples and mappings.

First vertical slice:

```text
one canonical source
  -> generated native definitions
  -> generated JSON catalog
  -> schema/cross-reference tests
  -> deterministic clean-generation check
  -> compatible/breaking diff fixtures
```

Preserve behavior. Do not parse full registry JSON on the scheduler.

### Safe #65 parallel slice

Before #64 stabilizes, a second agent may work only on isolated:

- action JSON Schema validation;
- canonical normalization/hash fixtures;
- graph cycle/depth/node/size validation;
- parser/formatter round-trip fixtures against an injected mock immutable registry;
- compiler interfaces that do not freeze command/state semantics.

Rebase before runtime integration.

## Work-agent rules

- Read the assigned issue and `SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md`.
- Post exact deliverable, dependency gate, file reservations, expected registry/schema changes, non-goals, narrow tests, and completion evidence before coding.
- Reserve high-conflict command/state/schema/runtime files.
- Do not invent competing IDs, private macros, skin-specific domain paths, or toolkit-specific public schemas.
- Keep current app/Runner usable after each commit.
- Leave exact tests, generated-diff result, compatibility classification, remaining bypasses, evidence, and next dependency.

## Planning completion

The product model, action model, visual authoring contract, safety/runtime boundary, registry lifecycle, compatibility policy, migration seam, accessibility/performance requirements, issue decomposition, dependency graph, file ownership, test strategy, and implementation handoff are sufficiently specified for work agents.

No production feature implementation is claimed by this planning checkpoint.
