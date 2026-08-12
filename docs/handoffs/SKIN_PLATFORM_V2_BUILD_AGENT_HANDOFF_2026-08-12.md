# Skin Platform v2 — Build-Agent Handoff

Date: 2026-08-12  
Planning branch: `planning/skin-platform-v2`  
Planning baseline: `main@f166a582b24972c6762022fd956ba868d2aae1cd`  
Parent epic: **#69**

## 1. Mission

Build EmberLights' user-authorable skin platform without creating alternate domain behavior, burdening lean Perform, delaying active hardware/core work, or breaking the current Live/Studio lanes.

Users must eventually be able to build/fork/import/export complete skins and overlays visually, bind controls to shared actions/state/MIDI, and migrate supported concepts from other applications. SoundSwitch is the primary workflow reference; VirtualDJ is the primary skin/action-system inspiration.

## 2. Binding reading order

1. `docs/00_START_HERE.md`
2. `AGENTS.md`
3. `docs/UI_PROGRAM_START_HERE.md`
4. `docs/SKIN_PLATFORM_V2_EMBER_ACTIONS_AND_DESIGNER_PLAN.md`
5. `spec/ui/command-state-skin-contract-v0.md`
6. `spec/ui/emberskin-package-and-safety-limits-v0.md`
7. `spec/ui/ember-actions-v1.md`
8. `spec/ui/skin-migration-ir-v1.md`
9. `spec/ui/user-customization-and-action-composition-v0.md`
10. the assigned issue and every dependency/current reservation comment

Later accepted decisions and current direct instructions remain authoritative.

## 3. Current implementation facts

- The typed `UiCommandFacade` and explicit invocation results are already merged.
- Current Live commands cover show/safety/BPM, Static Looks, Autoloops, Track Scripts, bank filters, and fixture/group overrides.
- `LiveCoreUiState`/`LiveViewModel` expose representative Runner, connection, output, active-content, progress, override, and safety state.
- `.emberskin`, layout, binding, controller-profile, package-limit, native-component, Default/Reference/Safe example contracts exist.
- The first runtime and bundled skins are not complete.
- UI toolkit qualification #37 remains part of the renderer decision.
- Core hardware/fixture/connection/OS2L/Static Look gates and active Studio/Autoloop work remain higher authority where shared code conflicts.
- Lean Perform/Studio isolation and VirtualDJ co-load budgets are tracked by #56.

## 4. Issue map

```text
#69 Skin-platform epic
 |
 +-- #71 SKIN-001 catalogs/codegen ------------------+
 |                                                    |
 +-- #73 SKIN-002 Ember Actions <---------------------+
 |                                                    |
 +-- #32 package/runtime/Safe + #37 toolkit ----------+
 |                                                    |
 +-- #39 bounded controls/overlays -------------------+
 |                                                    |
 +-- #74 SKIN-003 Skin Studio ------------------------+
 |                                                    |
 +-- #33 Default + #34 SoundSwitch Reference --------+
 |                                                    |
 +-- #75 SKIN-004 migration IR/VirtualDJ/SDK --------+
 |                                                    |
 +-- #35 persistence + #36 qualification + #56 perf -+
```

## 5. Recommended delivery passes

### Pass 1 — contracts and first executable foundation

Run as two coordinated lanes after the planning PR is merged.

- **Lane A: #71** — canonical registries/catalogs and generation.
- **Lane B: #73-A/B preparation** — action schemas, canonical graph, direct-binding desugaring, parser/type-checker fixtures using the agreed catalog shapes.

Lane B may create isolated files/tests while Lane A owns shared command/state source changes. Lane B must not invent a competing registry.

Desired result: registry metadata and an action compiler can validate representative one-command actions without selecting a production renderer.

### Pass 2 — runtime and bounded customization

- **#73-C/D/E** — executor boundary, events/results, trace/tooling.
- **#32** — validated source -> immutable compiled skin artifact, Safe fallback, transactional activation/cache.
- **#39 Level 0/1** — binding editor, MIDI/HID Learn, custom slots/pages/controls, overlays/import/export/reset.

Desired result: Default/Safe conformance packages can load and a user can add/bind one safe control without touching JSON.

### Pass 3 — bundled skins and Skin Studio

- **#33/#34** — Default and SoundSwitch Reference over identical behavior.
- **#74 A–F** — visual document, hierarchy/grid, theme/assets, action/feedback/input, deterministic preview.
- coordinate with active Studio/Autoloop/domain-component owners; do not reimplement their editors/logic.

Desired result: a non-developer can customize the Reference skin, add Static Look/Autoloop/group controls, preview variants, and activate an overlay safely.

### Pass 4 — lifecycle, migration, SDK, and hard qualification

- **#74 G/H** — fork/update/three-way merge/package UX/SDK surfaces.
- **#75** — source bundle, Migration IR, materializer, narrow VirtualDJ prototype, CLI.
- **#35/#36/#56** — persistence, security, accessibility, DPI, installed Windows, VirtualDJ co-load, memory/CPU/repaint/jitter, repeated switches/failures.

Desired result: portable deterministic packages and an honest fixed-canvas VirtualDJ candidate with explicit unsupported/manual/rights findings.

## 6. Work packet A — #71 catalogs/codegen

Suggested branch after planning merge: `agent/skin-catalog-codegen`

### Own first

Prefer new isolated paths such as:

```text
spec/ui/catalog/
tools/ui_catalog/
native-core/include/emberlights/generated/ui_*.hpp
native-core/src/generated/ui_*.cpp
native-core/tests/test_ui_catalog*.cpp
```

Any edit to existing `ui_command.*`, `ui_state.*`, schemas, `CMakeLists.txt`, or examples requires a posted reservation and reconciliation with active lanes.

### First bounded milestone

1. Choose/document canonical source and generated/verified outputs.
2. Preserve every current public command ID and persisted enum value.
3. Fully describe the current Live command/state slice.
4. Generate deterministic signatures and JSON.
5. Add duplicate/drift/signature tests.
6. Provide catalog reader usable by tests/Skin Studio without renderer coupling.

### Stop conditions

- domain behavior would need changing;
- active branch owns the same shared file and cannot reconcile;
- registry generation would require runtime JSON parsing in the scheduler;
- metadata cannot represent an existing command honestly.

Record the gap rather than inventing semantics.

## 7. Work packet B — #73 Ember Actions

Suggested branch: `agent/ember-actions-v1`

### Own first

```text
spec/ui/schema/action-*.schema.json
native-core/include/emberlights/ui_action*.hpp
native-core/src/ui_action*.cpp
native-core/tests/test_ui_action*.cpp
tools/skin_cli/ or equivalent isolated tooling
spec/ui/examples/actions/
```

### First bounded milestone

- schemas/models for program, nodes, typed values, predicates, source maps;
- direct command binding -> one-node `Invoke` compilation;
- type checking against synthetic then real #71 catalogs;
- immutable compiled instruction representation;
- deterministic serialization/hash;
- cycle/depth/node/expression/safety-class rejection;
- no runtime executor or advanced graph until compiler tests are stable.

### Second milestone

- executor outside scheduler;
- generation/target safety;
- `Sequence`, `Branch`, `Switch`, `OnResult`, `Call`, `Return`;
- normalized events/gesture policy;
- bounded redacted trace;
- skin/keyboard/MIDI/DJ/direct equivalence tests.

### Prohibited

- general `wait`, sleep, timer, loop, recursion, dynamic eval, imports, arbitrary code;
- direct device/network/filesystem access;
- priority blackout inside a multi-node graph;
- UI-derived beat/progress/timing;
- fake cross-command atomicity;
- foreign script execution.

## 8. Work packet C — #32 runtime/Safe integration

Suggested branch: `agent/emberskin-runtime-v0` unless #32 already has an assigned branch.

### Required integration contract

- consumes #71 catalogs and #73 compiled actions;
- public package/layout/action schemas remain toolkit-neutral;
- compiled artifact is immutable, bounded, content/signature locked;
- first-load failure -> trusted Safe; failed update -> current skin remains;
- activation never recompiles the show or stops/resets Runner/content;
- mandatory safety/health reachability is validated or platform-injected;
- editor/source/migration services are not resident in lean Perform.

Do not begin a full visual designer in this packet.

## 9. Work packet D — #39 bounded customization

Suggested branch: `agent/skin-custom-controls-v1`

### First milestone only

- keyboard/MIDI/HID binding editor over #71/#73;
- simple Button/Toggle/Pad/Fader/Knob/Status/Progress/Label/Separator controls;
- declared base-skin custom slots;
- visual generated command/target editor;
- state feedback and hardware feedback;
- custom pages/pad banks;
- overlays, reset, export/import/relink/conflict preview;
- responsive approved grids;
- mandatory-control protection.

Do not implement arbitrary layout surgery or full expert text here.

## 10. Work packet E — #74 Skin Studio

Suggested branch: `agent/skin-studio-v1`

### First milestone

- source document/lifecycle/Undo/recovery;
- package tree and hierarchy;
- component palette from #71;
- grid/constraint artboards and inspector;
- semantic theme/state/icon/asset editor;
- simple action and feedback editors;
- deterministic mock-state/DPI/input preview;
- validate/export/import/fork/overlay flows.

### Integration rule

Waveform, timeline, Autoloop, Static Look, fixture, mapping, connections, and diagnostics remain versioned native/domain components. Skin Studio composes their public contracts and slots; it does not copy their behavior into generic widgets.

### Process rule

The visual editor and migration services must be absent/dormant in lean Perform. Do not meet a Studio feature by embedding a browser or mandatory script VM in Runner.

## 11. Work packet F — #33/#34 bundled skins

Suggested branches remain owned by those issues.

### Reference skin requirements

- familiar Performance/Autoloops/Static Looks/Live Tools landmarks;
- four-bank/32-pad visible window over 64×32 catalog;
- selected/active/queued/progress/repeat/filter/exclusive distinctions;
- rate/size/strobe/color/content/group controls using real commands/state;
- permanent health/safety/active-content visibility;
- original EmberLights assets and evidence-tagged deviations;
- custom slots/pages so users can extend rather than fork immediately.

### Conformance rule

Any capability required only by one bundled skin must still enter the shared catalog/runtime. No Reference-only behavior path.

## 12. Work packet G — #75 migration/SDK

Suggested branch: `agent/skin-migration-ir`

### First milestone

- source manifest and immutable adapter host;
- versioned Migration IR models/schemas;
- deterministic synthetic adapter and reports;
- materializer into a simple valid candidate package;
- no VirtualDJ-specific parser required until shared IR tests pass.

### VirtualDJ milestone

- narrow observed ZIP/XML/image corpus;
- panels/groups/pages/geometry/source rectangles/primitives;
- source-specific VDJScript AST, never execution;
- documented simple action/query mappings;
- fixed-canvas candidate first;
- unsupported timers/repeat/plugins/dynamic behavior explicit;
- rights confirmation before exporting imported assets;
- controller profile candidate separate from skin.

### SDK/CLI

One shared implementation supports validate/format/inspect/test/pack/unpack/diff/golden/inventory/parse/plan/materialize/migrate. Studio is a client, not a second implementation.

## 13. Work packet H — qualification

Tracked through #36/#56 and parent #69.

Required evidence includes:

- registry/action schema and fuzz tests;
- safety and cross-surface equivalence;
- package/path/archive/asset/resource abuse;
- 1366×768 through 4K and 100–200% DPI;
- keyboard/focus/UI Automation/Narrator/non-color/contrast/target size;
- deterministic goldens/mock scenarios;
- 100 valid/invalid switches/reloads;
- active-show/content/DMX continuity;
- installed Windows smoke;
- VirtualDJ co-load;
- cold start, first frame, RSS/private bytes, CPU/repaint, latency, scheduler jitter;
- low-end and reference machines;
- migration source immutability, provenance, deterministic IR/output, and no silent translations.

No Linux-only or framework-demo measurement is a Windows/VirtualDJ support claim.

## 14. Shared-file reservation protocol

Before editing a shared file, post on the assigned issue:

```text
Branch:
Issue/packet:
Exact paths:
Why shared edit is unavoidable:
Current command/state/schema IDs affected:
Active branches checked:
Expected integration point:
Expected release date: omit; work is current-turn/agent execution only
```

The final line above means agents must not promise asynchronous delivery; report completed evidence or an exact blocker.

High-conflict paths:

```text
native-core/include/emberlights/ui_command.hpp
native-core/src/ui_command.cpp
native-core/include/emberlights/ui_state.hpp
native-core/src/ui_state.cpp
native-core/include/emberlights/live_view_model.hpp
native-core/src/live_view_model.cpp
native-core/src/windows_app.cpp
native-core/CMakeLists.txt
spec/ui/schema/*
spec/ui/examples/*
docs/08_DECISIONS_AND_OPEN_QUESTIONS.md
installer/release files
```

## 15. Required agent issue updates

At start:

- concise scope and non-goals;
- exact file reservations;
- dependency SHA and active-branch check;
- planned test/evidence set.

At each checkpoint:

- implemented contract/behavior;
- exact commands/state/components/schema versions changed;
- test commands/results;
- measurements where required;
- remaining legacy bridges and risks;
- next dependency.

At completion:

- PR/commit;
- acceptance checklist;
- machine-readable evidence paths;
- installed/hardware evidence status distinguished from local/CI tests;
- issues/docs/decision ledger updated;
- no unsupported support/parity claim.

## 16. First assignment recommendation

The most efficient first builder assignment is **#71 on a fresh branch from the merged planning checkpoint**. It unlocks every later visual/action tool and can proceed as a behavior-preserving metadata/codegen slice without waiting for the full renderer.

A second agent may begin **#73 schema/model/test scaffolding** in isolated files after reading #71's chosen catalog shape. Do not run #74 or #75 implementation ahead of those contracts except for isolated mockups/test fixtures that cannot become production behavior.

## 17. Completion definition

The work program is ready for broad implementation only after:

- planning artifacts and issue topology are merged;
- #71 publishes a stable catalog/signature strategy;
- #73 proves direct-command equivalence and bounded compilation;
- #32 proves Safe fallback and transactional artifact activation;
- #37/#56 establish a viable renderer/process envelope;
- shared-file ownership is posted.

Thereafter Skin Studio, bundled skins, customization, and migration can advance in short coordinated passes without sacrificing the core mission.