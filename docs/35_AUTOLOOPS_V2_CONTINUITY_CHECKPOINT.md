# Autoloops V2 Continuity Checkpoint

Checkpoint date: 2026-08-12  
Repository baseline reviewed: `main` at `f166a582b24972c6762022fd956ba868d2aae1cd`  
Planning branch: `planning/autoloops-v2-2026-08-12`  
Primary epic: #57

Binding planning package:

- `docs/33_AUTOLOOPS_V2_PARITY_MIGRATION_AND_AUTOMATION_PLAN.md`;
- `docs/34_AUTOLOOPS_V2_WORK_AGENT_HANDOFF.md`;
- this continuity checkpoint;
- issues #57–#61;
- existing architecture, Studio, fixture, command/state, migration and parity documents.

## 1. Planning result

The Autoloops V2 implementation program is ready for work agents in three bounded passes:

| Order | Issue | Branch | Scope |
| ---: | --- | --- | --- |
| 1 | #58 / AL2-001 | `agent/autoloops-v2-model` | semantic source model, format-1 compatibility, compact compiled program/evaluator |
| 2 | #59 / AL2-002 | `agent/autoloops-v2-runtime` | Runner director, SoundSwitch performance behavior, status and resource evidence |
| 3 | #60 / AL2-003 | `agent/autoloops-v2-studio-migration` | authoring services, original defaults, deterministic AutoScript, SoundSwitch evidence import |
| Later | #61 / AL2-004 | split when promoted | WOLFMIX source adapter and AI authoring provider as separate later packets |

This ordering is intentional. #58 establishes one model/compiled ABI. #59 consumes it without changing Studio/source decoding. #60 consumes the model/runtime and shared Studio color/preview contracts. #61 cannot redirect or delay SoundSwitch parity.

## 2. Current product truth

### Implemented foundation

EmberLights currently has:

- 64 banks × 32 slots with a four-bank pageable window;
- stable Autoloop bank/slot addressing;
- move, swap, duplicate and next-open placement operations;
- direct, next, previous and bank-filter commands;
- Once, Infinite and TrackDuration repeat foundations;
- progress/completed-cycle Runner status;
- separate Autonomous, TrackScript and ManualAutoloop layers;
- higher-priority Static Look/EventMoment, ManualOverride, Emergency and Safety layers;
- deterministic compile/package activation and no-allocation live foundations;
- basic no-output Autoloop preview through the compiled engine;
- SoundSwitch source inventory/migration IR and an approximate output-disabled converter.

### Current limitation

The authored Autoloop remains a maximum 32-step sequence of whole Static Looks using float beat positions and Cut/Linear transitions. It cannot natively express the required semantic target lanes, property curves, reusable colors/Positions/Attributes, movement/effects, portable generator input or exact vendor migration evidence.

### Current migration boundary

The 2026 pilot converter retains 32 Autoloop names and constructs useful native patterns from names. Those patterns are approximations. The authorized backup’s Venue/Autoloop source identity is not proven to match the generated Ember project used by the pilot. No current artifact establishes exact broad SoundSwitch Autoloop timeline decoding.

## 3. Source-evidence checkpoint

Known source evidence includes an authorized read-only SoundSwitch application-data backup and earlier user workflows/projects, including a known 59-Autoloop project. Private source remains outside Git.

At this checkpoint:

- source inventory/identity mechanisms exist;
- the backup and current generated Ember project are not a matching controlled baseline;
- a complete versioned one-change Autoloop delta corpus is not confirmed;
- exact source fields for rich Autoloop timelines are therefore not yet qualified;
- synthetic CI fixtures can support all native model/runtime/authoring work immediately.

If #60 cannot access matching authorized delta evidence, it must report:

```text
soundswitch.autoloop_delta_corpus_unavailable
```

It must preserve opaque source and continue native authoring/default/generator work. It may not infer timeline semantics from names, paths, slot counts or UI observation.

No source bytes, private labels, client/audio data, machine paths or device credentials enter Git.

## 4. Accepted Autoloops V2 planning decisions

These decisions are accepted for #57 planning. Promote them additively into the main decision ledger after checking active ownership and after the planning PR is accepted.

### AL-D001 — content and placement are separate

An Autoloop’s stable authored content identity is not its bank/slot. Placement can move or repeat without breaking content references. “Duplicate content” and “place existing content again” are separate explicit actions.

### AL-D002 — reuse Studio integer musical time

Autoloops V2 uses signed 64-bit musical ticks at 960 PPQ through the shared Studio contract. Float-beat format-1 content receives a deterministic compatibility adapter; no second time system is created.

### AL-D003 — preserve the legacy whole-Look path

Current format-1 Autoloops remain readable and retain byte-equivalent behavior. The whole-Static-Look sequence remains an explicit compatibility specialization/fast path until equivalence, performance and installed evidence justify any removal.

### AL-D004 — compact used-content compiled arenas

Runner retains the fixed 64×32 placement lookup but rich events/curves/targets use bounded contiguous arenas sized for used content during compile/load. EmberLights does not reserve a worst-case rich program inside every possible slot.

### AL-D005 — one Runner director, three sessions

One deterministic director owns Autonomous, Scripted and Manual Autoloop sessions on their existing layers. The semantic evaluator evaluates one program; the layer resolver preserves cross-feature priority.

### AL-D006 — Overlay and Replace are different modes

Manual one-shot Overlay starts immediately, owns its authored properties and naturally returns to still-running lower layers. Replace/Override is an explicit separate mode with a defined suppression/ownership mask. Neither is inferred from the other.

### AL-D007 — exclusive bank is pending state

For autonomous playback, selecting an exclusive bank while another loop runs becomes pending and activates at the natural boundary. Direct launch remains intentional, immediate and unfiltered.

### AL-D008 — TrackDuration requires trustworthy track boundaries

TrackDuration consumes a normalized track epoch/end event. When the active integration lacks that evidence, Runner reports degraded capability rather than guessing from BPM, filename or UI state.

### AL-D009 — deterministic AutoScript precedes AI

The first AutoScript Autoloop generator is a versioned rule-based Studio worker using normalized inputs and explicit seed. It emits ordinary editable source and reproducible provenance.

### AL-D010 — SoundSwitch decoding is field-evidence gated

A source field is imported as exact/translated only after source-version-specific controlled deltas and synthetic regression coverage. Current name-derived patterns remain approximated. Unsupported bytes remain opaque and identity-linked.

### AL-D011 — original default content only

EmberLights ships an independently authored versioned starter pack, not copied SoundSwitch/WOLFMIX defaults or proprietary presets. Populate fills empties. Reset is previewable, Undoable and limited to recognized pack-managed placements unless broader replacement is explicit.

### AL-D012 — fixture capabilities resolve at compile time

Portable role/category/group/fixture target intent resolves against one qualified immutable venue/profile snapshot. Missing/changed capabilities produce exact diagnostics; no Autoloop code manufactures channel semantics.

### AL-D013 — no broad adapter ABI before proof

Implement SoundSwitch as a native Studio adapter first. Extract a small reusable adapter lifecycle only after SoundSwitch demonstrates real requirements; validate it with WOLFMIX later. Do not add a plugin VM prematurely.

### AL-D014 — AI is an optional Studio provider

A future AI provider reads bounded semantic summaries, proposes normal source, validates/compiles/previews, returns a diff and commits once only after human acceptance. It cannot write files directly, emit DMX or enter Runner.

### AL-D015 — claims remain evidence-tiered

Model, compile, UI, installed behavior, physical output, gig qualification and parity completion are separate evidence tiers. Planning or a starter pack cannot close parity rows alone.

## 5. SoundSwitch parity target frozen for this program

The program must cover:

- 128-loop/four-bank familiar workflow while preserving 64×32 capacity;
- version-aware older 32-loop layout handling;
- reorder/move/duplicate/populate/reset;
- beat-synchronized unscripted fallback cycling;
- selected/queued/active/progress/bank feedback;
- pending exclusive-bank boundary;
- immediate manual loop over scripted content and natural return;
- explicit scripted-track override mode;
- Once/Infinite/TrackDuration lifecycle;
- semantic Master/Group/Fixture or equivalent targeting;
- linked colors/palettes, Positions and Attributes;
- movement/effect generation and curves;
- deterministic editable AutoScript using fixture categories/capabilities, colors, cues, movement and seed;
- evidence-gated import and opaque retention;
- Control One/MIDI/shared UI bindings through existing command/state contracts;
- lean deterministic Runner behavior.

UI resemblance is not sufficient. Each row requires behavior and evidence.

## 6. Cross-lane coordination at checkpoint

### PR #53 — Studio color/preview

At planning time, PR #53 is the only open PR and owns:

- rich Studio color/palette contracts;
- Studio no-output preview services;
- Studio authoring tests;
- shared CMake/Make integration; and
- additive decisions/checkpoint updates.

#58 and #60 must inspect its final state. They must not duplicate `StudioColor`, palette realization or `StudioPreviewService`.

### Issue #46 — Studio

#58 coordinates persistence/time/assets. #60 coordinates document transactions, preview and migration IR. Neither creates another Studio document service, source corpus, status taxonomy or timeline foundation.

### Issue #52 — fixture/static look

Autoloops consumes qualified profile/capability truth and shared color/Static Look dependencies. It does not change profile mappings or claim physical fixture qualification.

### Issue #56 — lean Perform

#59 adds dense-Autoloop resource snapshots and must retain zero-allocation/live isolation. #56 measures/preserves Autoloops but does not redesign them.

### Issue #31 — command/state

#59 coordinates explicit mode/queued/pending/status additions. Skins and controller surfaces remain consumers of shared behavior.

## 7. File-ownership summary

### #58 owns new

```text
native-core/include/emberlights/autoloop_source.hpp
native-core/src/autoloop_source.cpp
native-core/include/showcore/autoloop_program.hpp
native-core/src/autoloop_program.cpp
native-core/tests/test_autoloop_v2_model.cpp
spec/autoloops/autoloop-source-v1.md
```

### #59 owns new

```text
native-core/include/showcore/autoloop_director.hpp
native-core/src/autoloop_director.cpp
native-core/tests/test_autoloop_v2_runtime.cpp
```

### #60 owns new

```text
native-core/include/emberlights/autoloop_authoring.hpp
native-core/src/autoloop_authoring.cpp
native-core/include/emberlights/autoloop_content_pack.hpp
native-core/src/autoloop_content_pack.cpp
native-core/include/emberlights/autoloop_autoscript.hpp
native-core/src/autoloop_autoscript.cpp
native-core/include/emberlights/soundswitch_autoloop_adapter.hpp
native-core/src/soundswitch_autoloop_adapter.cpp
native-core/tests/test_autoloop_v2_studio.cpp
native-core/tests/test_soundswitch_autoloop_adapter.cpp
spec/autoloops/emberlights-default-pack-v1.json
research/migration/soundswitch-autoloops/README.md
```

Shared project/compiler/Runner/Studio/registry/build files require an issue reservation and current-main rebase. Exact lists and prohibited files are in doc 34 and each issue.

## 8. Qualification checkpoints

### After #58

- format-1/unknown-record compatibility;
- stable asset-placement model;
- integer timing;
- legacy frame equivalence;
- deterministic compact compile/digest;
- no-allocation evaluator;
- no live behavior change.

### After #59

- autonomous selection/cycling;
- pending bank transition;
- manual Overlay/Replace/repeat/track behavior;
- Static Look/override continuation;
- Runner-owned selected/queued/active state;
- deterministic replay;
- Windows/VirtualDJ resource snapshots;
- no output protocol change.

### After #60

- generation-checked authoring and dependency impact;
- original 128+ starter pack;
- reproducible deterministic AutoScript;
- exact no-output preview;
- source-version/evidence matrix;
- controlled-delta import or explicit blocker;
- opaque retention and idempotent re-import;
- installed packaging of content/services;
- parity ledger updated only to proven tiers.

### Before gig/parity claim

- installed Windows Runtime/Studio;
- VirtualDJ same/separate-PC behavior;
- trustworthy TrackDuration boundary;
- physical Control One feedback where claimed;
- representative physical fixture color/movement/attribute comparison;
- low-end/reference CPU/RSS/jitter;
- 8-hour soak and recovery/fault scenarios;
- migration supported-version/gap documentation;
- no data-loss blocker.

## 9. Claim boundary at this checkpoint

- Autoloops V2 planning: **complete**;
- GitHub epic and bounded work packages: **created**;
- implementation branch #58: **not started**;
- semantic V2 source/compiled model: **not implemented**;
- complete autonomous runtime director: **not implemented**;
- SoundSwitch performance parity: **not qualified**;
- original EmberLights 128-loop pack: **not implemented**;
- deterministic AutoScript Autoloops: **not implemented**;
- exact SoundSwitch Autoloop timeline decoder: **not implemented**;
- WOLFMIX file migration: **deferred/not implemented**;
- AI authoring provider: **deferred/not implemented**;
- current pilot Autoloops: **validated approximate/output-disabled source conversion**.

## 10. Next action

After the planning PR is accepted or merged, assign one work agent to issue #58 on:

```text
agent/autoloops-v2-model
```

That agent executes only Pass A from doc 34. It must not begin Runner selection or Studio/migration work. After #58 merges, assign #59; after #59 and PR #53 merge, assign #60.

Use #57 and this checkpoint additively for continuity. Do not replace the plan with a new one on each pass.
