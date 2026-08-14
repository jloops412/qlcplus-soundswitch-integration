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

## 11. Additive AL2-003 authoring/content-pack checkpoint — 2026-08-12

A bounded toolkit-neutral part of #60 now exists on the combined local base.
The issue reservation is comment `5267965507`. This checkpoint advances only
source authoring and original pack management; it does not claim the complete
Pass C stop condition.

### Implemented source-authoring boundary

- `AutoloopAuthoringService` edits the accepted
  `AutoloopSourceDocument`; it does not define another timeline, time, color,
  fixture-capability, compiled, or persistence model.
- every snapshot is normalized, validated, generation-stamped, and carries the
  canonical source digest;
- a changed candidate commits atomically, increments generation once, and keeps
  one source-level Undo/Redo step; identical candidates are `NoChange`, and
  stale generations do not mutate source;
- create, rename, deep content duplicate, and explicit delete are transactional;
  deletion reports every dependent placement, shared dependency, and orphaned
  program/launch/provenance record before it can remove referenced placements;
- content identity remains separate from placement identity: the same asset can
  be placed again, while duplicate creates new asset/program/launch/provenance
  identities;
- assign, unassign, move, swap, and deterministic wrapping next-open operations
  use the existing 64 x 32 address contract; occupied destinations refuse the
  edit and require an explicit swap.

This service is a bounded source-edit session, not a replacement for
`StudioDocumentService`. Rich Autoloop project persistence must later make the
Studio document service authoritative for save/history/durable dirty state and
commit the same validated canonical source through a versioned project seam.

### Original EmberLights starter pack

- pack ID `emberlights.starter.autoloops`, version `1` / semantic version
  `1.0.0`, is generated deterministically from original EmberLights rules;
- it contains 128 independently named semantic placements across banks 0..3,
  with 128 stable assets/programs/launch profiles/provenance records;
- programs use the accepted signed 64-bit 960-PPQ musical time and semantic
  Intensity/RGB property blocks derived through the shared `StudioColor` type;
  they contain no raw DMX, fixture assumptions, strobe, vendor defaults, source
  labels, private migrated content, or random input;
- the pack records its canonical source digest, stable management-key prefix,
  original-content provenance, and current distribution-license boundary;
- Populate fills empty addresses only and never overwrites an occupied user
  placement; repeated Populate is idempotent;
- Reset matches exact recognized pack management keys, restores only proven
  pack-owned record IDs, reports the exact changed assets and linked placements,
  never deletes unrelated user records, and commits as one Undoable source
  transaction;
- an unknown management key or unrelated stable-ID collision fails closed
  instead of being treated as pack ownership.

### Evidence at this checkpoint

- focused `autoloop_v2_studio_tests` pass with warnings fatal;
- canonical source serialize/parse/reserialize and deterministic pack digest
  pass;
- generation, stale/no-change, one-step Undo/Redo, invalid-candidate retention,
  duplicate identity, exact delete dependency, placement overwrite refusal,
  swap/move/unassign/next-open, and full 2,048-address capacity tests pass;
- pack Populate/Reset/idempotency/user-preservation/stale-plan tests pass;
- all 128 pack programs compile through `compile_autoloop_programs` on a
  representative synthetic semantic target, repeated compiled digests match,
  and a 127-program compiler limit rejects before package creation;
- full Make warnings-fatal native tests and all build targets pass;
- the current integrated Surface Contract Gate is generation 2 with the same
  accepted 29 commands and 39 states, plus planned non-callable metadata, and
  digest `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`;
- Runner dry-run smoke, WinMM/DMX USB Windows syntax checks, and focused
  ASan+UBSan (`detect_leaks=0`) pass.

CMake is not installed in this Linux workspace, so the additive CMake target
was inspected but not configured here. Windows cross-build/installed startup,
installer packaging, physical fixture behavior, and gig qualification were not
run or claimed.

### Remaining ordered boundaries

- versioned rich-source persistence was not implemented by the authoring-pack
  slice; it is now delivered under the bounded section 14 contract below;
- exact V2 no-output preview was not implemented by the authoring-pack slice;
  it is now delivered under the bounded section 12 contract below and remains
  separate from physical output or fixture qualification;
- linked persisted palette/Position/Attribute realization and capability
  degradation matrices remain coordinated Studio/fixture work;
- deterministic AutoScript, cancellation/work scheduling, proposal preview,
  and commit remain #60 work;
- the SoundSwitch Autoloop delta corpus remains unavailable in this lane, so
  `soundswitch.autoloop_delta_corpus_unavailable` remains the exact blocker;
  no decoder, name heuristic, migration fidelity, or re-import claim was added;
- no Runner/director/output/hardware, fixture profile, UI registry, Windows UI,
  skin, AutoScript, migration IR, or source/compiler implementation file changed.

## 12. Additive AL2-003 exact V2 preview checkpoint — 2026-08-12

Issue #60 reservation comment `5268305894` extends the existing
`StudioPreviewService`; it does not add a second preview engine or make Studio
timing authority.

### Implemented preview boundary

- one atomic load validates the document generation, source generation and
  canonical source digest, compiles the format-1 project candidate, resolves
  V2 target selectors against that candidate venue, and calls the accepted
  `compile_autoloop_programs` compiler;
- the last-good candidate remains active when project compilation, source
  digest validation, target/capability resolution or V2 compilation fails;
- successful snapshots carry document/source generations, canonical source
  digest, immutable compiled-package digest, placement/asset/program identity,
  exact renderer frame digest, current rendered frames and semantic ownership;
- bounded restart, absolute tick seek, nearest-tick beat seek, half-open phase
  seek and tick advance stay within
  `kMaximumStudioAutoloopPreviewTransportTick`; phase, loop tick and completed
  cycles come from the shared 960-PPQ program length;
- the most recent 256 exact rendered frame-digest records are retained with an
  explicit dropped-record count;
- direct Property Block/Curve programs that the venue can realize use the
  production immutable compiler, `AutoloopProgramEvaluator`, layer resolver,
  safety policy and fixture renderer;
- Palette, Position, Attribute, Movement, Effect and legacy references are not
  guessed. Until a coordinated semantic reference resolver supplies canonical
  assignments, the production compiler reports `MissingReference` and the
  candidate fails closed;
- custom transition references retain the compiler's
  `UnsupportedPayload` diagnostic; target/property capability gaps retain
  `MissingCapability`; evaluator rejection clears the V2 layer and reports an
  explicit internal diagnostic instead of reusing an approximate frame;
- format-1 Static Look, Autoloop and draft-palette preview continue through
  their existing APIs and renderer path.

### Structural no-output boundary

`StudioPreviewService` owns only editable/canonical source, immutable compiled
packages, evaluator/layer buffers and `CompiledShow`'s engine. It has no Runner,
output-backend, Art-Net, sACN, USB, Control One, SoundSwitch Micro or device
session dependency and exposes `output_disabled = true` in every snapshot.
Tests deliberately enable valid project connection settings while previewing;
those settings are compiled as document data but no adapter/session is created.

### Focused evidence

- warning-fatal `autoloop_v2_preview_tests` cover deterministic identical
  source/package/frame digests, exact RGB DMX slots and semantic ownership;
- restart, tick/beat/phase seek, advance and exact loop-boundary wrap pass;
- stale document/source generations, reused generations, mismatched source
  digests and out-of-range transport retain the previous exact frame;
- unresolved Palette/Position/Attribute references, custom transitions and a
  missing fixture capability fail closed with their exact compiler diagnostics;
- the trace remains bounded at 256 entries and reports dropped history;
- existing warning-fatal Studio authoring, V2 source/compiler and V2 authoring
  pack suites pass unchanged.
- full warning-fatal Make test/all targets, generation-2 UI and Ember Action
  registry checks plus `surface-contract-gate`, dry-run Runner smoke, WinMM and
  DMX USB Windows syntax checks, and focused ASan+UBSan pass.

The accepted evaluator's zero-allocation contract remains covered by
`autoloop_v2_model_tests`. This Studio service intentionally allocates while
building user-facing fixture/ownership/diagnostic snapshots and SHA-256 trace
strings; it is not a Runner scheduler path and does not claim zero allocation.

### Remaining ordered boundaries

- Windows Studio UI wiring from an authoring draft into the now-authoritative
  rich-source document transaction;
- canonical linked Palette/Position/Attribute/Movement/Effect reference
  resolution and qualified exact/degraded fixture matrices;
- deterministic AutoScript proposal/cancellation/preview/commit;
- SoundSwitch controlled-delta evidence and decoder work, still blocked by
  `soundswitch.autoloop_delta_corpus_unavailable`;
- Windows installed-app, physical fixture, hardware-output, resource/soak and
  gig qualification. No such claim is made by output-disabled preview.

## 13. Additive AL2-002 Runner integration checkpoint — 2026-08-12

Issue #59 reservation comment `5268370538` and evidence comment `5268700941`
record the narrow `CompiledShow`/Runner activation seam. Feature integration
commit `074b4df` contains the locally cherry-picked result.

### Implemented runtime boundary

- `CompiledShow` optionally owns one immutable V2 package compiled from the
  canonical source; format-1-only compilation retains a null package and the
  exact legacy path;
- Runner activates the package with its package generation, publishes bounded
  director/ownership status, routes existing Autoloop/bank commands, advances
  from normalized Runner transport, and hot-swaps through the existing atomic
  activation boundary without adding registry IDs or output-adapter work;
- V2 evaluates into a private fixed-storage `LayerStack`. Autonomous and Manual
  contributions are copied deliberately, while `TrackScript` has one explicit
  mutually exclusive owner: legacy Static Look, legacy Autoloop, or compiled
  V2. Director ticks cannot overwrite an active legacy buffer;
- ownership handoff retires stale V2 scripted content before legacy playback,
  package clear preserves an active legacy script, and stop/fault/incompatible
  activation clears only V2-owned contributions;
- manual Overlay preserves lower progression; Replace suppresses and restores
  the lower automated layer through the director's explicit mode; repeated
  scheduler ticks remain allocation-free.

### Integrated evidence and remaining boundary

- exact legacy `TrackScript` layer-buffer parity is covered across 32 frames
  with V2 active; V1/no-package Runner behavior, V2 package activation/hot
  swap, stale generation, ownership handoff, Once return, Overlay/Replace,
  deterministic replay, and zero-allocation ticks pass;
- the combined branch passes 16 warning-fatal native executables, all Make
  targets, both generation-2 registry gates, smoke, WinMM/DMX USB syntax,
  benchmarks, and fresh focused ASan+UBSan;
- TrackDuration fails closed as `TrackBoundaryUnavailable` until Runner exposes
  a trustworthy track epoch. Non-Static-Look semantic references remain exact
  compile failures until coordinated Palette/Position/Attribute/Movement/
  Effect resolvers exist;
- CMake/installed Windows, physical output, source-decoder fidelity, long soak,
  and gig qualification were not run or claimed.

## 14. Additive AL2-003 rich-source persistence checkpoint — 2026-08-12

Issue #60 reservation comment `5268856525` records this local-only persistence
slice from production-integration base `e48f605`. It makes the existing
`StudioDocumentService` authoritative for rich Autoloop save/history without
changing the typed format-1 `AUTOLOOP`/`STEP` model.

### Persisted record contract

- one `EMBERLIGHTS_AUTOLOOP_SOURCE_RECORD` version-1 record carries the
  accepted canonical `AutoloopSourceDocument` through the existing format-1
  unknown-record channel;
- the bounded envelope records its canonical byte count (maximum 8 MiB),
  source format version, lowercase SHA-256 canonical source digest, and exact
  lowercase-hex canonical source bytes;
- canonical serialization remains owned by `serialize_autoloop_source`; the
  persistence helper does not define another source, timeline, musical-time,
  color, compiler, fixture, or Runner model;
- one recognized record is replaced in place or appended when absent. Every
  unrelated unknown record retains its bytes and relative order, including
  through save/reopen;
- malformed fields/encoding, duplicate recognized records, unsupported record
  or source versions, size mismatch/overflow, digest mismatch, invalid source,
  and parseable-but-noncanonical source all fail closed;
- project parse/load and pre-save verification reject an invalid recognized
  record. Unrecognized future records remain opaque and preserved;
- the format-1 project header/version and typed legacy Autoloop collections are
  unchanged, and the format-1 compatibility adapter produces the same digest
  before and after a rich-source transaction.

### Authoritative Studio transaction

- each `StudioDocumentSnapshot` exposes the validated persisted source plus a
  stamp containing presence, record version, source version, and source digest;
- `apply_autoloop_source` checks both the active document generation and the
  complete prior stamp, then commits the canonical record as one existing
  `ProjectEditHistory` transaction;
- a caller presenting the current numeric generation with an older digest or
  version is rejected as stale, so reused generations cannot bypass source
  identity checks;
- the generic full-document candidate path cannot add, remove, or replace the
  rich source; such changes must use the generation-and-stamp transaction;
- New/Open/Restore boundaries validate the recognized record, establish their
  existing durable baseline behavior, and never turn navigation into an edit;
- existing dirty comparison, atomic save acknowledgement, one-step Undo/Redo,
  and save/reopen behavior now cover rich source without a parallel history or
  persistence authority.

### Evidence and remaining boundary

- focused warnings-fatal `autoloop_v2_persistence_tests` pass deterministic
  record/idempotence, canonical source round-trip, unknown-record preservation,
  atomic save/reopen, dirty acknowledgement, one-step Undo/Redo, stale numeric
  generation plus digest/version mismatch, generic-candidate add/remove/change
  bypass refusal, no-record stamp/generation pairing, legacy format-1 stability,
  malformed/duplicate/version/size/digest rejection, and pre-save
  no-file-on-failure cases;
- full `make -j2 test` passes 17 native executables; `make -j2 all` passes every
  target;
- generation-2 UI and Ember Action registry checks, `surface-contract-gate`,
  dry-run Runner smoke (42 frames, zero decode errors/dropped beats/send
  failures), and WinMM/DMX USB Windows syntax checks pass;
- a fresh focused ASan+UBSan build/run passes with
  `detect_leaks=0:halt_on_error=1`; CMake is not installed in this Linux lane,
  so its additive target was inspected but not configured or claimed.

Rich source persistence/save/reopen/history is therefore no longer an open
AL2-003 boundary. Remaining work is the in-flight deterministic AutoScript
proposal/commit slice, canonical linked Palette/Position/Attribute/Movement/
Effect resolution and qualified exact/degraded capability matrices, installed
Windows application/packaging verification, and controlled-delta migration
evidence. The exact decoder blocker remains
`soundswitch.autoloop_delta_corpus_unavailable`. No Runner/output/device,
registry, fixture-profile truth, skin, migration decoder/IR, AutoScript, Windows
UI, backlog, or parity-ledger file changed in this slice.
## 15. Additive AL2-003 deterministic AutoScript proposal checkpoint — 2026-08-12

Issue #60 reservation comment `5268893342` adds the first versioned,
rule-based AutoScript service on integrated production base `e48f605`. The
service works only from an immutable `AutoloopAuthoringSnapshot` and returns a
reviewable proposal; it does not mutate the active authoring source while it
generates.

### Request, generation and provenance boundary

- requests use signed 64-bit 960-PPQ track, loop and grid ticks and require
  exactly one ordered, non-overlapping musical-section or energy-band
  timeline;
- an explicit seed is mandatory, including when the intended seed is zero;
  eligible semantic role selectors are stable IDs, with an explicit
  diagnostic when the request falls back to the semantic Master target;
- style and complexity are bounded enums. Musical-section energy and
  energy-band ranges use exact per-mille integers rather than a wall-clock,
  audio-analysis or platform-randomness authority;
- generator ID `emberlights.autoscript.rule-based`, version `1`, normalized
  complete request parameters, seed, request digest and deterministic evidence
  status are recorded in ordinary `Generated` provenance;
- generated stable asset/program/launch/provenance/placement IDs derive from
  normalized structural inputs. A different seed retains those identities and
  placement intent while changing only seeded semantic choices and recorded
  provenance/digests. After one proposal is accepted, regeneration over that
  same structural identity fails closed on the existing IDs; replacement must
  use explicit `AutoloopAuthoringService` dependency-aware delete/edit
  transactions before a fresh proposal, never an AutoScript overwrite;
- output is ordinary editable `AutoloopSourceDocument` content: semantic
  Master/RoleSelector targets, Intensity/RGB Property Blocks, 960-PPQ programs,
  launch profiles and placements. It contains no raw DMX, fixture profile,
  vendor preset, private source, strobe/hazard, opaque runtime recipe, model
  call or I/O operation.

### Bounded proposal/commit boundary

- hard service caps are 32 timeline segments, 4 role selectors, 32 generated
  assets, 8,192 generated events, 16,384 complete-candidate events, 16 MiB of
  canonical candidate source, 100,000 work operations and 4,096 four-beat bars
  of musical track time; each request supplies equal or tighter explicit
  content and operation budgets;
- an atomic cancellation token is checked before work, through source scans and
  at every segment/event construction checkpoint, plus immediately before
  final canonicalization and publication. Those final checkpoints consume the
  explicit operation budget as well. Cancellation or budget exhaustion
  discards local work and returns no candidate;
- the immutable proposal carries base generation/source digest, normalized
  request digest, canonical preview-source digest, proposal digest, exact
  generated IDs/addresses/event count, operations used and stable diagnostics;
- the complete candidate is exposed read-only for output-disabled validation
  and preview. The active `AutoloopAuthoringService` remains byte-identical
  until an explicit commit;
- commit rechecks proposal integrity, base generation and base source digest,
  then calls `AutoloopAuthoringService::apply_candidate` once. The accepted
  generation is one source transaction and one Undo restores the exact prior
  canonical source;
- occupied requested addresses, out-of-range contiguous placement, stable-ID
  collision, source capacity, stale generation, invalid input, cancellation,
  content/operation exhaustion and integrity/validation failure all fail
  closed. Generation always appends its own records to the copied base and
  never skips, replaces or deletes unrelated content.

### Evidence and remaining scope

- focused warning-fatal `autoloop_autoscript_tests` pass for exact repeated
  request/source/compiled digests, normalized input ordering, seeded variation
  with stable IDs, semantic-only records, full normalized provenance,
  preview-without-mutation, one-transaction commit/Undo, stale generation,
  occupied placement, catalog capacity, initial and post-commit regeneration
  stable-ID collision, cancellation and hard content/operation/byte budgets,
  including exhaustion at the final publication checkpoint;
- full warning-fatal Make `all` and `test` pass with the additive target;
- generation-2 UI and Ember Action registry checks plus
  `surface-contract-gate` pass at 29 commands, 39 states and digest
  `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`;
- Runner dry-run smoke, WinMM/DMX USB Windows syntax checks and fresh focused
  ASan+UBSan (`detect_leaks=0`) pass.

CMake and a Windows cross-compiler are not installed in this Linux lane, so
the additive CMake target was inspected but not configured and an installed
Windows build was not run. Toolkit scheduling/UI acceptance orchestration,
rich source persistence, canonical Palette/Position/Attribute/Movement/Effect
selection, SoundSwitch controlled-delta decoding, physical hardware, soak and
gig qualification remain separate work. The migration blocker remains exactly
`soundswitch.autoloop_delta_corpus_unavailable`.

No source-model/compiler, Studio document/color/preview, migration, Runner,
output/hardware, fixture/profile, registry/UI, Windows UI, skin, project-format,
backlog or decision-ledger file changed in this slice.

## 16. Additive AL2-003 Studio/AutoScript transaction bridge — 2026-08-12

Issue #60 reservation comment `5269197040` adds one narrow bridge on exact
integrated base `d083226`. It connects the existing deterministic AutoScript
proposal service to the existing authoritative persisted Studio document
transaction; it does not introduce another source, history or persistence
service.

### Snapshot and commit authority

- `propose_studio_autoloop_autoscript` accepts one immutable
  `StudioDocumentSnapshot`. A present rich-source record supplies its validated
  canonical `AutoloopSourceDocument`; an absent record supplies the canonical
  empty rich source and never adapts or mutates format-1 `AUTOLOOP` records;
- the returned immutable handoff captures the exact Studio document
  generation, all four persistence-stamp fields (presence, record version,
  source format version and digest), the effective base-source digest, the
  existing AutoScript proposal and a versioned bridge digest;
- proposal creation remains output-disabled and no-mutation. Existing rich
  source is copied into the complete proposal candidate, so generation appends
  its own semantic assets/programs/launch profiles/provenance/placements and
  never overwrites unrelated persisted content;
- commit first compares the active document generation and complete stamp,
  reconstructs the active effective rich source, and rechecks its canonical
  digest. It then independently recomputes the existing versioned AutoScript
  proposal digest, canonical preview-source digest and bridge digest. The
  independent recomputation is deliberately locked to AutoScript generator
  version 1; any digest-format change must update the owning version and its
  golden vectors in lockstep, while one-sided drift is rejected before the
  document authority is invoked;
- only a ready, unchanged handoff reaches
  `StudioDocumentService::apply_autoloop_source`, exactly once with the same
  document generation and full persistence stamp. That existing authority
  performs the sole project mutation and records one complete document Undo
  transaction;
- failed proposal, occupied placement, malformed/unsupported stamp, mismatched
  source bytes, stale document generation, stale persisted source and failed
  proposal/source/bridge integrity all fail closed before mutation. Undo/Redo
  restores exact prior/later project bytes and successful content survives the
  ordinary atomic save/load and OpenedDocument path.

### Evidence and remaining scope

- focused warning-fatal `autoloop_autoscript_studio_tests` pass for absent
  source first commit with unchanged format-1 Autoloop adapter digest, existing
  persisted-source preservation, same-generation stamp/source validation, stale
  document and source rejection, one document Undo/Redo, atomic save/reopen and
  invalid/occupied proposal no-mutation. Fixed generator-v1 and bridge-v1 digest
  vectors enforce the lockstep/version invariant;
- existing warning-fatal AutoScript, rich-source persistence and Autoloops V2
  Studio suites pass beside the new bridge target;
- full warning-fatal Make `all` and `test` pass, including all V1/V2 Studio,
  preview, runtime and Runner suites;
- generation-2 UI and Ember Action registry checks plus
  `surface-contract-gate` pass at 29 commands, 39 states and digest
  `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`;
- Runner dry-run smoke passes with 42 frames, zero decode errors, dropped beats
  or send failures; WinMM and DMX USB Windows syntax checks pass;
- core and full-performance benches pass; a fresh focused ASan+UBSan build and
  run passes with leak detection disabled; `git diff --check` passes.

CMake and a Windows cross-compiler are not installed in this Linux lane, so
the additive CMake source/test entries were inspected but not configured and
no installed Windows build is claimed. UI scheduling, proposal acceptance and
preview presentation, canonical linked Position/Attribute/Movement/Effect
resolution, SoundSwitch controlled-delta decoding, physical hardware, soak and
gig qualification remain separate work. The exact migration blocker remains
`soundswitch.autoloop_delta_corpus_unavailable`.

No existing Studio document/persistence/AutoScript authority, source schema,
project format, Studio color/preview, migration, Runner/output/hardware,
fixture/profile, registry/UI, Windows UI, skin, backlog or decision-ledger file
changed in this slice.

## 17. Additive AL2-003 Studio Palette resolution checkpoint — 2026-08-12

Issue #60 reservation comment `5269174721` records this local-only slice from
exact integration base `5f79eb5`. It resolves project-owned Studio palette
swatches into the accepted compiled Autoloop reference contract without adding
a color model, source field, persistence record, Runner path, or output owner.

### Stable reference and capability contract

- a V2 Palette event's `reference_id` matches only an exact stable
  `StudioColorSwatch::id` across the current project's palette assets. Display
  names, palette names, fixture/channel names, raw DMX offsets/ranges/defaults,
  and color-wheel slots are never interpreted;
- zero stable-ID matches is `Missing`, more than one match anywhere in the
  project is `Ambiguous`, a complete semantic realization is `Exact`, an
  accepted lossy `realize_studio_color` result is `Degraded`, and a missing
  profile/semantic color capability or partial target realization is
  `Unsupported`;
- each unique `(swatch ID, target kind, target stable ref)` produces at most
  one `AutoloopReferenceBinding`, even when multiple events reuse it. The
  assignment list is deterministic by runtime fixture/property and is bounded
  by the existing compiler target/reference/assignment limits;
- target membership uses the existing Master/Group/Fixture/RoleSelector
  semantics and the compiler's existing intersection capability mask. A mixed
  target whose fixtures require nonuniform properties (for example RGB and
  white-only fixtures under one Master target) fails closed as
  `MissingCapability`; it is not reported as a partially degraded success.
  Authors can use distinct target-scoped references when each target is
  individually realizable;
- a single-capability target may be `Degraded` (for example a white-only target
  receiving RGB intent) only when every fixture has a complete usable semantic
  realization and every resulting property belongs to the target intersection;
- missing, ambiguous, and unsupported Palette candidates return structured
  resolution evidence and compiler-aligned diagnostics before immutable
  compilation. Position, Attribute, Movement, Effect, legacy references and
  custom transitions retain their existing fail-closed compiler behavior.

### Preview and no-output boundary

`StudioPreviewService` supplies the resulting target/reference spans to the
production V2 compiler and evaluator, retains exact/degraded resolution records
in its outcome/snapshot, and renders through the existing output-disabled
production fixture renderer. A rejected newer project/source candidate leaves
the prior document/source generations, source/package digests, rendered frame,
and last-good Palette resolution active. The resolver and preview headers and
implementation have no Runner, output-backend, network/USB adapter, or device
session dependency; valid connection settings in adversarial tests never open
an output path.

### Evidence and remaining boundary

- focused warning-fatal `autoloop_v2_palette_resolution_tests` cover exact RGB,
  degraded white-only, independent target-scoped RGB/white bindings, reused
  reference deduplication, heterogeneous-target refusal, unknown and duplicate
  stable IDs, deterministic duplicate reporting/package/frame digests, display
  name non-inference, raw color-wheel DMX non-inference, exact rendered DMX,
  bounded target-fixture/reference-assignment capacity refusal,
  output-disabled snapshots, and last-good retention;
- existing warning-fatal `autoloop_v2_preview_tests` pass unchanged, including
  Position/Attribute/custom-transition/capability refusal, generation/digest
  guards, bounded transport/trace and deterministic frames;
- full `make -j2 test` passes 18 native executables and `make -j2 all` passes
  every target; both generation-2 registry checks, `surface-contract-gate`,
  dry-run Runner smoke (42 frames, zero decode errors/dropped beats/send
  failures), and WinMM/DMX USB Windows syntax checks pass;
- a fresh focused ASan+UBSan build/run passes with
  `detect_leaks=0:halt_on_error=1`; CMake is not installed in this Linux lane,
  so its additive source/test entries were inspected but not configured or
  claimed.

Canonical Position/Attribute/Movement/Effect resolution, legacy linked-Look
resolution, transition profiles, richer cross-capability authoring UX,
installed Windows verification, physical qualification, and migration decoder
evidence remain separate boundaries. The decoder blocker remains
`soundswitch.autoloop_delta_corpus_unavailable`. No registry, persisted source
or project schema, Runner/output/device, hardware/fixture truth, migration,
AutoScript, skin, broad UI, backlog, or parity-ledger file changed here.

## 18. AutoScript Studio end-to-end Windows journey — 2026-08-12

Issue #60 reservation comment `5272424627` records this stacked slice from the
exact integration tree used by Windows preview `0.1.0-preview.84.1`. The work
turns the accepted proposal, preview, persistence, compiler, Runner, and typed
Live command pieces into one user-visible path without changing the V2 source
schema, project record envelope, scheduler, output ownership, or device code.

### Authoring and preview boundary

- `StudioAutoloopAutoscriptWorkflow` is a toolkit-neutral orchestration layer
  over the existing immutable proposal bridge, `StudioPreviewService`, and
  `StudioDocumentService`. Generate and preview do not mutate the document;
  Commit requires a successful production-compiler preview and delegates the
  exact proposal once through the authoritative persisted-source transaction;
- the Windows **Studio • AutoScript** page accepts bounded musical inputs,
  stable fixture roles, catalog address, and explicit deterministic seed. Its
  review panel exposes proposal/source/compiler/frame digests, exact output-
  disabled state, phase, placement, and per-fixture DMX bytes before Commit;
- project drift after preview disables/refuses Commit. A successful commit is
  one ordinary project Undo transaction, becomes durable only after the normal
  atomic Save, and survives the existing checksum/recovery save and reopen
  path;
- the temporary Win32 direct callback is recorded in the binding bypass ledger
  with issue #60 ownership, focused regression evidence, and removal gate at
  issue #31 G1E. It adds no second engine or UI-owned lighting semantics.

### Production compile and Live path

- `compile_project_with_persisted_autoloops` is the production document entry
  point. An absent rich record preserves exact format-1 behavior; a present
  record compiles its exact canonical V2 source; malformed, unsupported, or
  digest-mismatched persistence fails closed. Windows Validate, Diagnostics,
  Start Show, last-known-good recovery, and running-show atomic activation now
  use that same entry point;
- once a persisted V2 record exists, the Windows Live list shows its placements
  by bank/slot and stable name. Launch still routes through registered
  `autoloop.launch`; the typed facade resolves a persisted placement/content ID
  or address and posts the existing Runner command. Previous, Next, Clear, and
  64-bank filters remain unchanged;
- V2 is authoritative when present. Format-1 records remain byte-for-byte in
  the project for compatibility but the Runner never mixes two Autoloop
  engines in one activation.

### Evidence and remaining boundary

- focused `autoloop_autoscript_workflow_tests` pass the complete
  propose → production preview → phase seek → one-transaction commit → atomic
  save → reopen → persisted production compile → Runner start → typed stable-ID
  Live launch journey, plus discard/no-mutation and malformed persistence
  refusal;
- all 28 warning-fatal Linux CTest targets pass, the 13 packaging-contract
  tests pass, the generation-2 registry check remains unchanged at 29 commands
  and 39 states, and `git diff --check` passes;
- the complete Windows GUI translation unit compiles under the pinned
  llvm-mingw/LLVM 22.1.8 cross-toolchain used for preview 84.1. Installed native
  Windows launch/uninstall and physical output are not claimed by this check.

This page deliberately generates one musical section per gesture. Rich
Position/Attribute/Movement/Effect authoring and resolution, bulk timeline
editing, post-commit V2 detail editing, SoundSwitch controlled-delta decoding,
native Windows/hardware qualification, soak, signing, and production release
remain follow-up work. The migration blocker remains
`soundswitch.autoloop_delta_corpus_unavailable`.

## 19. Additive named fixture-function authoring checkpoint — 2026-08-13

This Preview 93 source slice extends the renderer-neutral fixture-control
catalog into committed Autoloop V2 content. It consumes the existing source,
authoring, persistence, compiler, preview, and fixture-capability contracts; it
does not introduce another event model, profile interpreter, preview engine,
Runner path, or command registry.

### Exact proposal and transaction boundary

- `plan_autoloop_fixture_control` accepts one immutable authoring snapshot,
  project fixture truth, stable program/target/choice IDs, a canonical
  half-open 960-PPQ tick range, and a bounded range position;
- a fixture target emits one semantic target/lane/`PropertyBlock` event, while
  a mixed-profile group emits one exact fixture-scoped set per supported
  member so different profile ranges can retain different normalized values;
- generated stable IDs come from a caller-owned prefix and collisions are
  explicit refusal, never overwrite behavior;
- the complete candidate is normalized and validated before
  `AutoloopAuthoringService` applies it as one generation-checked source
  transaction. Stale generation, missing program/target/choice, empty target,
  invalid time, ownership conflict, identifier collision, fixture-write or
  source capacity, protected choice, and safety-gated choice all leave the
  original generation, digest, and canonical source unchanged;
- no raw DMX address, channel offset, byte, display label, or Windows control
  identity is persisted in the Autoloop event.

### Transitional Windows journey

The Studio AutoScript page can select an existing persisted V2 placement,
fixture/group, named profile function, exact start/end beats, and continuous
range position. It builds the immutable candidate, loads it through
`StudioPreviewService`, seeks the production output-disabled preview, and
shows the rendered frame digest before committing the canonical source through
the existing `StudioDocumentService::apply_autoloop_source` authority. A
preview/compile/generation failure leaves the project unchanged. One accepted
gesture becomes one ordinary project edit and is durable only after normal
Save.

This remains in the already recorded Authoring direct-callback area. It adds
no callable command/state/component/capability registry entry because the
current controls only select arguments for an existing Studio authoring
transaction. Issue #31 G1E remains the removal gate for the Win32 adapter.

### Focused evidence and remaining boundary

- warning-fatal Make and fresh CMake/CTest builds pass
  `autoloop_fixture_controls_tests`;
- regressions cover two deliberately different wheel profiles, exact
  per-fixture semantic values, one atomic apply/Undo, half-open adjacency,
  lane-priority overlap, and fail-closed stale/safety/protected/collision/
  ownership/range/budget/capacity paths;
- `fixture_controller_binding_tests` separately proves the same stable catalog
  can produce persistent bounded MIDI/controller mappings without introducing
  controller-specific fixture semantics;
- `migration_portability_review_tests` separately keeps SoundSwitch artifact
  identity distinct from semantic decoder fidelity and reports WOLFMIX as
  research/evidence unavailable.

Position/Attribute/Movement/Effect reference authoring and realization,
drag/curve tooling, bulk AutoScript orchestration, controller feedback, Ember
Actions/EmberSkin exposure, exact SoundSwitch controlled-delta decoding,
installed Windows, physical fixtures/controllers, soak, and gig qualification
remain open. The exact SoundSwitch blocker remains
`soundswitch.autoloop_delta_corpus_unavailable`; the WOLFMIX blocker is
`wolfmix.controlled_delta_corpus_unavailable`. Neither blocker is an import
claim.
