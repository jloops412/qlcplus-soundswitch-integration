# EmberLights Skins Platform V2 — Start Here

Status: **binding planning and continuity entrypoint** for the user-created skin, visual designer, action-composition, import/export, and registry-governance program.

Owner direction captured: **2026-08-12**.

Planning branch:

```text
planning/skins-platform-v2-2026-08-12
```

## Mission

Make EmberLights a genuinely modular control-surface platform in which users can:

- use the bundled EmberLights Default, SoundSwitch Reference, and Safe surfaces;
- create or fork complete skins without modifying the lighting engine;
- customize existing skins through bounded overlays and custom control pages;
- lay out interfaces visually with grids, responsive constraints, reusable components, theme/color tools, icon and asset pickers, and live state simulation;
- bind software controls, keyboard, MIDI/HID/controllers, DJ commands, and future remotes to the same product commands;
- compose richer behavior through a robust, typed, inspectable, bounded action system rather than hand-editing fragile callback code;
- import/export skins, overlays, action packs, and controller profiles with compatibility diagnostics;
- eventually use source-specific migration assistants for VirtualDJ and other products without creating alternate runtime engines.

SoundSwitch remains the primary workflow and migration reference. VirtualDJ is the primary architectural inspiration for user-extensible skins, mappings, custom controls, pad pages, and a shared action vocabulary. EmberLights must preserve the useful flexibility while replacing manual XML/script/file editing with first-class visual tools, stronger typing, deterministic validation, explicit safety, and stable compatibility contracts.

## This program extends the existing UI program

Do **not** create a competing UI architecture. The following remain binding:

- `UI_PROGRAM_START_HERE.md`;
- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`;
- `21_UI_IMPLEMENTATION_PROGRAM.md`;
- `spec/ui/command-state-skin-contract-v0.md`;
- `spec/ui/emberskin-package-and-safety-limits-v0.md`;
- `spec/ui/native-component-contracts-v0.md`;
- `spec/ui/user-customization-and-action-composition-v0.md`;
- issues #29, #31, #32, #33, #34, #36, #37, and #39.

Skins Platform V2 closes the deliberately deferred gap between the current declarative package plan and a complete end-user authoring ecosystem.

## Product vocabulary

| Term | Meaning |
| --- | --- |
| **`.emberskin`** | Validated distributable presentation package: manifest, layouts, theme, bindings, localization, and assets. |
| **`.emberoverlay`** | User-local or project-recommended customization layered over an immutable base skin. |
| **Ember Actions** | User-facing action-composition system over registered commands and observable state. It is not an unrestricted programming language or alternate lighting engine. |
| **Action Graph** | Visual, typed representation of an Ember Action. |
| **Ember Action Script** | Optional expert textual representation that round-trips to the same Action Graph/IR; it never bypasses validation. |
| **Action IR** | Canonical, versioned, bounded, immutable program produced before activation. |
| **Skin Designer** | In-app Studio-side visual authoring environment for overlays and full skins. It is not resident in the lean Perform/Runner path. |
| **Command Registry** | Versioned source of truth for callable product behavior. |
| **State Registry** | Versioned source of truth for observable product facts and update classes. |
| **Component Registry** | Versioned catalog of primitive and native complex components available to skins. |
| **Capability Registry** | Versioned catalog of optional engine, adapter, project, and platform capabilities used for validation and graceful degradation. |

## Non-negotiable continuity invariant

Every agent adding, removing, renaming, or materially changing a user-visible feature must keep the skin platform current in the same change.

At minimum, the change must reconcile:

1. command definitions and typed arguments;
2. observable state definitions and update classes;
3. capability and component exposure;
4. deprecation/replacement metadata;
5. generated native and JSON registry artifacts;
6. bundled skin and Safe-surface impact;
7. controller/binding/action compatibility;
8. schemas, examples, tests, and migration diagnostics;
9. affected backlog, decision, parity, and continuity records.

A feature is not surface-ready merely because a hard-coded control exists. A feature removal is not complete if skins, overlays, actions, mappings, examples, or generated catalogs still reference it.

## One behavior implementation

```text
Skin / keyboard / MIDI / DJ command / remote event
        -> validated Binding
        -> optional compiled Ember Action
        -> registered typed Command
        -> Studio service or Runner command boundary
        -> authoritative domain state change
        -> registered bounded State snapshot
        -> software/controller feedback
```

The domain behavior exists exactly once. Skins and actions select, parameterize, compose, and display behavior; they do not implement lighting semantics, scheduling, persistence, adapter protocols, or safety policy.

## Runtime boundary

The lean Perform/Runner process may load only validated, compiled presentation and action artifacts. It must not contain the full designer, source importers, package editors, arbitrary script interpreters, browser engines, or migration tooling.

- The Skin Designer runs in Studio/edit mode or a separate authoring process.
- Package parsing, asset decoding, action compilation, migration, and validation remain off the DMX scheduler.
- Musical timing remains in Runner/domain commands.
- UI gesture timing may be handled only by bounded binding primitives such as press, release, long press, double press, debounce, encoder mode, and soft takeover.
- Skin switching or failure never stops DMX, recompiles the show, resets active content, replays stale output, or weakens Blackout/Stop/safety authority.

## Required reading by work type

### Every Skins Platform V2 agent

1. `AGENTS.md`
2. `docs/00_START_HERE.md`
3. `docs/UI_PROGRAM_START_HERE.md`
4. `docs/SKINS_PLATFORM_V2_START_HERE.md`
5. `docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`
6. `spec/ui/command-state-skin-contract-v0.md`
7. `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`

### Action system

- `spec/ui/ember-actions-contract-v1.md`
- `spec/ui/schema/ember-action.schema.json`
- command/state schemas and accepted registries
- issue #31 command/state ownership and any successor registry issue

### Designer, overlays, and full skins

- `spec/ui/skin-designer-contract-v1.md`
- `spec/ui/user-customization-and-action-composition-v0.md`
- `.emberskin` package limits and layout schemas
- native component contracts and bundled layout examples

### Migration adapters

- `docs/27_CROSS_DJ_CONTROLLER_AND_SKIN_PORTABILITY_PLAN.md`
- SoundSwitch evidence and Reference-skin documents
- source-specific migration status/provenance contracts
- the exact official source format documentation used by that adapter

### Work-agent execution

- `docs/SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md`
- the assigned GitHub issue and its exact dependencies/file reservations

## Dependency order

```text
#38 core-ready gate -------------------------------------------> broad skin rollout

#31 registry/facade + #37 toolkit
                 -> #32 runtime/Safe
                 -> #33 Default + #34 Reference
                 -> #36 relevant qualification
                 -> #39 overlays/custom controls
                 -> Skins Platform V2 visual designer/actions/import ecosystem

Registry governance, schemas, planning, fixtures, code generation harnesses,
and compatibility tests may proceed earlier when they preserve behavior and
avoid shared-file conflicts.
```

The long-term designer/action architecture is binding now even when implementation is gated. New engine work must not hard-code itself into a presentation path that later has to be extracted.

## Safety and trust model

All third-party skins, overlays, actions, mappings, controller profiles, and migration sources are untrusted data.

They cannot:

- execute native, JavaScript, Lua, WebAssembly, shell, or arbitrary plugin code;
- access arbitrary files, URLs, network sockets, USB, MIDI, cameras, microphones, secrets, or process internals;
- write state directly;
- bypass command availability, priority, safety, validation, or persistence rules;
- replace or intercept emergency blackout delivery;
- use UI sleeps/timers as show timing;
- create cycles, recursion, unbounded loops, or unbounded allocation;
- silently redirect missing project targets;
- silently upgrade deprecated behavior with incompatible semantics.

## Binding artifacts in this planning package

- `SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md` — product and architecture plan;
- `SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md` — implementation sequence, ownership, acceptance, and stop conditions;
- `SKINS_PLATFORM_V2_CONTINUITY_CHECKPOINT.md` — compact state for future agents;
- `../spec/ui/ember-actions-contract-v1.md` — typed action model;
- `../spec/ui/skin-designer-contract-v1.md` — visual authoring contract;
- `../spec/ui/schema/ember-action.schema.json` — provisional machine-readable action schema;
- `../spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md` — mandatory feature/registry drift prevention;
- `adr/0006-registry-governed-skins-and-ember-actions.md` — accepted architecture decision.

## Completion standard

This program is complete only when a non-programmer can visually create or fork a responsive EmberLights skin, bind controls and feedback to discoverable typed capabilities, optionally compose bounded actions, validate all variants and mandatory controls, test against simulated/live-safe state, export the package, import it on another machine, and continue to use it across compatible application updates with explicit diagnostics and migration rather than silent breakage.

A screenshot, mockup, hard-coded custom page, hand-written JSON example, or text-only macro parser is not completion.
