# EmberLights Studio V1 — Authoring, Migration, and Source Compatibility Program

Status: **binding planning authority for issue #46**. This plan is additive to the existing architecture, SoundSwitch parity ledger, migration workflow, fixture-qualification program, and modular UI/skin program.

Plan baseline: `main` commit `5f192e153a96a6c504674f23592887b6484d5835`, reviewed 2026-08-11.

## 1. Mission

Deliver the production authoring side of EmberLights so a user can:

1. open, create, recover, and safely evolve a project;
2. preserve and inspect every supplied SoundSwitch artifact without mutation;
3. import everything that is defensibly decoded and retain everything else for later decoder improvements;
4. resolve fixture/profile, patch, media, and track-identity conflicts explicitly;
5. author Static Looks, Autoloops, reusable semantic assets, and detailed song timelines;
6. rehearse against waveform, beatgrid, transport, and the qualified fixture model;
7. validate and compile one deterministic immutable show package;
8. activate that package through the existing generation-stamped Runner boundary;
9. use EmberLights Default or SoundSwitch Reference without changing domain behavior; and
10. reopen the project later without losing unknown source evidence, IDs, associations, or history.

The Live/output agent remains authoritative for runtime synchronization, playback, layer ownership, safety, output, and health. The fixture agent remains authoritative for manufacturer/model/mode/channel/capability truth. Studio consumes those contracts; it does not create competing implementations.

## 2. Binding existing work

Do not rediscover or replace these artifacts:

- `03_ARCHITECTURE.md` — sophisticated Studio; small deterministic Runner;
- `04_V1_SCOPE_AND_ACCEPTANCE.md` — installed V1 and qualification gates;
- `06_PRIORITIZED_BACKLOG.md` — additive product backlog;
- `08_DECISIONS_AND_OPEN_QUESTIONS.md` — accepted architecture decisions;
- `09_BUILD_AND_TEST_STANDARDS.md` — correctness, persistence, fault, performance, provenance, and release evidence;
- `13_SOUNDSWITCH_PARITY_LEDGER.md` — binding completeness checklist;
- `18_SOUNDSWITCH_MIGRATION.md` — current read-only inventory, bundle, delta comparison, and conservative converter;
- `UI_PROGRAM_START_HERE.md` and linked specifications — command/state, skins, native components, Default, Reference, Safe, and qualification;
- `23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md` and related fixture plans — fixture source and qualification ownership.

Official SoundSwitch behavior used by this plan is documented in its current support guides for Saving Projects and Light Shows, moving a project between computers, Scripted Tracks, Edit Mode, Control Tracks, Beatgrids, Phrase Editing, Position Cues, Attribute Cues, Movement Shape Effects, Strobe Effects, Static Looks, Autoscripting, and Engine Lighting export.

## 3. Reconciled current baseline

### 3.1 Implemented and retained

- checksummed transactional `.emberlights` project persistence;
- bounded local restore history and in-session Undo/Redo;
- unknown project-record preservation;
- stable fixture, group, Look, Autoloop, TrackScript, and MIDI identities;
- external audio records with SHA-256, byte size, filename, and replaceable local path hint;
- manually triggered beat-relative TrackScript cues compiled into fixed Runner arrays;
- deterministic project compilation and generation-stamped activation foundation;
- read-only SoundSwitch artifact inventory, SHA-256 reporting, controlled export comparison, and verified source-bundle creation;
- an output-disabled SoundSwitch 2.10.x color-rig converter that preserves source hashes and labels its staged patch and rebuilt content as approximations;
- a complete modular UI/skin architecture with native contracts for asset browser, patch view, timeline, waveform/beatgrid, Autoloop and Static Look editors, inspector, migration review, validation, history, connections, and diagnostics;
- a first typed command/result/state facade over the existing Win32 shell.

### 3.2 Deliberately incomplete

The current source document and TrackScript model do not yet express:

- first-class reusable Venue/Rig objects;
- waveform, beatgrid, tempo-map, phrase, or editable timing evidence;
- Master, Group, and Fixture timeline lanes;
- cue ranges, automation curves/nodes, transitions, easing, and property ownership over time;
- reusable Position, Attribute, Movement, Effect, palette, and preset assets with dependency tracking;
- ranked multi-source track identity and automatic association evidence;
- versioned SoundSwitch semantic decoder output independent of the final project model;
- object-level migration status, provenance, conflict resolution, and later decoder upgrades;
- exact scripted-lightshow import from packaged lighting files, TrackMap/metadata, copied audio, and DJ-library identity data;
- production authoring services behind the planned Studio components.

The current `convert-v1` output remains a useful pilot project and regression fixture. It must not become the general importer by accumulating more filename- or name-based guesses.

## 4. Non-negotiable invariants

1. **Source is immutable.** SoundSwitch projects, databases, lighting files, audio, and DJ-library data are opened read-only. EmberLights does not alter a tag, path, timestamp, database row, or payload byte.
2. **Preservation precedes translation.** Unknown or unsupported payloads remain byte-identical in a verified source bundle and linked by digest from migration records.
3. **No false fidelity.** A retained name, guessed patch, rebuilt pattern, or inferred association is never labeled exact.
4. **Imported output is disabled.** Imported patch/output cannot become live until profile, mode, universe, address, safety, validation, and activation gates pass.
5. **Studio is not timing authority.** Studio may preview and request transport actions; Runner remains the scheduler and source of live playback state.
6. **Runner remains media/importer-free.** It never scans libraries, opens audio, decodes waveforms, parses SoundSwitch artifacts, or executes skin logic.
7. **Fixture truth has one owner.** Studio references stable qualified profile IDs/revisions and capability sets from the fixture lane. It reports mismatches; it never silently rewrites a profile.
8. **One behavior, many surfaces.** Default, Reference, Safe, keyboard, MIDI, and future surfaces invoke the same commands and read the same state/document views.
9. **Every model edit is transactional.** A gesture may use a local draft; commit produces one typed Studio command and one Undo transaction.
10. **Compatibility is explicit.** Existing format-1 projects remain readable. New semantics require an intentional versioned extension or format bump and a tested adapter.
11. **No proprietary copying.** Interoperability with user-owned data and observable behavior is allowed; proprietary source, assets, fixture library, and trade dress are not copied.
12. **Claims follow evidence.** Source-decoded, compiled, installed, hardware-tested, gig-qualified, and parity-complete remain separate statuses.

## 5. Ownership and integration boundaries

### Studio owns

- editable document and authoring transactions;
- external asset/media index and verified relinking;
- waveform, beatgrid, tempo-map, and phrase caches/models;
- semantic timeline and reusable authoring assets;
- SoundSwitch source inventory, decoders, migration intermediate representation, report, and review actions;
- validation, preview inputs, compiler request, and candidate activation request;
- document view models consumed by native components and skins;
- Studio command metadata, results, Undo behavior, and project-authored state.

### Live/output owns

- DJ/transport normalization and runtime clock truth;
- scripted, Autoloop, Look, manual, emergency, and safety layer behavior;
- seek/loop/reverse runtime semantics;
- bounded runtime commands/state snapshots;
- scheduler, output adapters, reconnect, blackout, and health;
- atomic package handoff implementation and last-known-good runtime behavior.

### Fixture lane owns

- manufacturer/model/mode/channel-map truth;
- stable profile revision and provenance;
- semantic capability mapping and physical qualification;
- profile quarantine and replacement-compatibility evidence;
- fixture-library ingestion rules.

### UI platform owns

- toolkit-neutral command/state/binding schemas;
- `.emberskin` runtime, Safe fallback, component adapters, themes, layout, accessibility, DPI, and UI performance;
- Default and SoundSwitch Reference presentation packages.

### Shared contracts

```text
QualifiedProfileSnapshot
  stableProfileId + sourceRevision + capabilityRevision + evidenceState

StudioDocumentSnapshot
  projectGeneration + immutable authoring view + validationGeneration

CompiledCandidate
  projectId + sourceGeneration + packageSchema + packageDigest + diagnostics

ActivationRequest / ActivationResult
  candidateGeneration + accepted/rejected/pending + exact reason + activeGeneration

RunnerStateSnapshot
  active package/transport/content/output/safety state; read-only to Studio UI
```

Shared headers, compiler paths, command registries, project serialization, tests, and CMake files require reservation before editing. New Studio modules should be isolated first so concurrent Live and fixture work can merge cleanly.

## 6. Three-representation architecture

Source evidence, editable authoring data, and live-optimized runtime data must remain separate.

```text
Read-only SoundSwitch / audio / DJ-library sources
                         |
                         v
1. Source Evidence Bundle + Migration IR
   exact bytes, hashes, relative paths, version evidence,
   candidate decoded objects, status/conflicts, unknown retention
                         |
                         v
2. Canonical Studio Document
   stable semantic objects, media identities, timelines, dependencies,
   provenance links, Undo/history, human-inspectable persistence
                         |
                         v
3. Immutable Compiled Show Package
   only validated used profiles/patch/groups/events/mappings/safety;
   fixed capacities; no source paths, media decoding, or importer state
```

A decoder never writes directly into Runner structures. A UI component never writes project memory directly. The compiler never reads SoundSwitch payloads or audio files.

## 7. SoundSwitch source-corpus contract

A `.ssproj` by itself is not assumed to contain every scripted show. SoundSwitch documents separate Project and Save Lightshow operations, metadata links in scripted audio, optional lighting-file inclusion during export, DJ-library dependencies, and audio transfer requirements.

### 7.1 Corpus classes

When available and authorized, inventory independently:

| Class | Examples | Purpose |
| --- | --- | --- |
| Project container | `.ssproj` manifest or extracted project root | version/export identity and project scope |
| Venue/project data | venue databases, fixtures, groups, patch, Looks, Positions, Attributes | reusable project content |
| Autoloop data | active/extended Autoloop databases and script payloads | banks, names, timing, semantic content |
| Track mapping | TrackMap databases and identifier tables | source lighting-file/audio association |
| Lighting files | `.ssfile`, recordable data, packaged lighting payloads | manual/auto scripted timelines |
| Audio | authorized copies of scripted files | embedded SoundSwitch ID, content identity, metadata, duration, waveform |
| DJ-library identity | VirtualDJ database, Serato/Engine library metadata where authorized | beatgrid, track IDs, crate/playlist and path reconciliation |
| Export context | SoundSwitch version, OS, export choices, selected venues, include-lighting-files flag | decoder version and completeness |
| Controlled deltas | before/after exports with one documented edit | reproducible semantic field discovery |
| UI evidence | native screenshots/state notes with private data scrubbed | Reference-skin behavior and parity tests |

### 7.2 Required manifest fields

Each source item records at minimum:

```text
sourceBundleId
artifactId
relativePathWithinBundle
artifactKind
byteSize
sha256
sourceProductVersion
capture/export timestamp when known
readResult
completeness role: required | conditional | optional
decoder adapter/version
privacy/licensing note
```

No absolute private path is required for a portable source bundle. Local discovery paths remain app-local hints.

### 7.3 Missing-source behavior

The current ChatGPT File Library search did not expose the raw `.ssproj` payload tree, copied scripted audio, or DJ-library database; it exposed the generated EmberLights project/report and planning ledger. Build agents must not infer that private source bytes are in Git or available in their environment.

When the authorized corpus is absent, report exactly:

```text
authorized_soundswitch_corpus_unavailable
```

Continue synthetic fixture/tests and contract work; do not invent decoded counts or semantics.

## 8. Migration status and evidence model

Do not collapse migration quality into one percentage. Every candidate object and field carries one status:

- **Exact** — directly decoded and regression-proven for the source version;
- **DeterministicallyTranslated** — source meaning is known, but normalized into an EmberLights semantic equivalent;
- **Approximated** — useful substitute with a documented difference;
- **PreservedOpaque** — retained byte-identically but not interpreted;
- **Unsupported** — known source concept not yet representable;
- **Conflicted** — multiple valid targets or incompatible evidence require a decision;
- **MissingDependency** — required project/audio/library/profile evidence is absent;
- **RejectedUnsafe** — would violate validation, safety, license, or bounded-resource rules.

Each result also records:

```text
sourceArtifactId + byte range or record identity when known
decoder adapter/version
destination object/field
rule ID
warnings and blockers
human resolution and actor/time when applicable
source and destination digests/generations
```

A migration can commit exact and approved translated items while preserving blocked items. It cannot silently discard them.

## 9. Canonical Studio document vNext

### 9.1 Compatibility strategy

- Keep the format-1 reader permanently covered by golden fixtures.
- Preserve current unknown records byte-for-byte through load/save where possible.
- Introduce new records additively until semantics require a version bump.
- When format 2 is introduced, provide explicit `v1 -> v2` conversion, deterministic serialization, checksum tests, restore-history compatibility, and a written downgrade boundary.
- Never reinterpret an existing enum value or record field.

### 9.2 Project structure

```text
Project
  metadata and compatibility
  Venue/Rig definitions
  qualified profile snapshots/references
  patched fixture instances and groups
  reusable semantic assets
  audio/media identities
  track identities and lightshows
  Static Looks and Autoloops
  controller mappings
  connection and safety policy
  source bundles, migration runs, and provenance links
  unknown/future records
```

### 9.3 Venue/Rig model

A Venue/Rig has a stable ID and owns patch-specific data:

```text
venueId, name, notes/categories
fixture instances: stable ID, profile revision, U/address, roles, optional physical metadata
groups and logical targets
venue-specific Position/Attribute realization where required
validation and qualification status
```

Content portability is semantic. Repatching does not change fixture identity. Replacing a fixture/profile produces a capability compatibility report and preserves authored content unless the user explicitly resolves an incompatibility.

### 9.4 Reusable semantic assets

First-class project assets:

- ColorPalette / swatches;
- PositionCue with per-target pan/tilt values;
- AttributeCue for gobo, prism, focus, zoom, iris, frost, rotation, color wheel, and custom qualified attributes;
- MovementPreset: circle, scans, ovals, figure-eights, triple-eights, square, and later extensible generators;
- EffectPreset / transition profile;
- Static Look;
- Autoloop;
- authoring/style preset.

Position and Attribute references are linked by stable ID so an intentional asset edit can update every dependent script/Autoloop, matching the useful SoundSwitch workflow. Before commit, Studio shows the affected-object count; the edit is one Undo transaction. Future frozen/snapshot references may be added without changing the default linked contract.

## 10. Audio, library, and track identity

Path and filename are hints, never identity.

### 10.1 Audio identity

```text
audioAssetId
sha256 + byteSize
container/codec/duration when analyzed
fileName and localPathHint
embedded SoundSwitch UID/tag values, retained separately
optional acoustic fingerprint/version
metadata snapshot: title/artist/remix and provenance
availability/verification state
```

Audio remains external by default. Studio never modifies it. A relink succeeds automatically only when the configured identity policy passes; content digest plus byte size is the strict baseline.

### 10.2 DJ-library identity

Keep source-specific IDs as evidence, not canonical IDs:

```text
VirtualDJ: database identity/path/beatgrid metadata
Serato: crate/library identity and track metadata
Engine: database/playlist/drive identity
SoundSwitch: metadata/lightshow UID and TrackMap identity
```

### 10.3 Ranked resolver

Candidate association order:

1. exact content digest and size;
2. exact embedded SoundSwitch UID plus compatible content evidence;
3. exact DJ-library stable ID plus compatible content evidence;
4. approved fingerprint plus duration tolerance;
5. metadata/duration candidate requiring human confirmation.

Filename/path alone never auto-binds a scripted show. Every resolved association records the evidence and whether it was automatic or approved.

### 10.4 Indexing

- indexing, hashing, metadata extraction, and folder recovery run off the UI and Runner threads;
- every job is cancellable and generation-stamped;
- partial/stale results cannot overwrite a newer index generation;
- bounded concurrency and byte limits protect low-end systems;
- external library databases are read-only.

## 11. Waveform, beatgrid, tempo, and phrase model

### 11.1 App-local cache

Waveforms are not copied into the project or Runner package. Cache key:

```text
audio sha256 + byteSize + decoderVersion + cacheSchema
```

Store bounded multiresolution peaks, channel summary, duration, and analysis diagnostics. Corrupt/stale cache is disposable and regenerable.

### 11.2 Musical time

Use integer time in the authoring model:

```text
AbsoluteTimeUs: int64 microseconds
MusicalTick: int64 at fixed 960 PPQ for V1
```

The beatgrid/tempo map provides deterministic conversion between them. Existing floating beat-relative TrackScripts import through a tested quantization adapter and remain semantically stable within documented tolerance.

### 11.3 Beatgrid and phrase data

```text
Beatgrid
  origin/downbeat
  one or more tempo segments
  bar/time-signature metadata
  source/provenance and confidence
  user corrections and revision

PhraseMap
  start/end ticks
  type, name, source, confidence
  split/resize/rename history through normal Undo
```

Imported VirtualDJ/Serato beatgrids remain evidence-tagged. User edits create EmberLights-owned timing data; they never modify the DJ library or audio.

## 12. Semantic timeline model

### 12.1 Tracks and scope

```text
Master track
Group tracks -> stable group ID
Fixture tracks -> stable fixture ID
```

Within the TrackScript layer, fixture-specific ownership overrides compatible group ownership, and group ownership overrides Master for the same fixture/property/time. Exact overlap/tie behavior must be deterministic, validation-visible, and regression-tested before public schema freeze.

### 12.2 Event envelope

Every event has:

```text
eventId
trackId
startTick + durationTicks
target/scope
source/provenance link
lock/mute/validation metadata
typed payload
```

Typed payloads initially include:

- `PropertyCurve`: intensity, color components, white/UV, pan/tilt, strobe, and qualified custom properties;
- `PaletteBlock` / color transition;
- `PositionReference`;
- `AttributeReference`;
- `MovementEffect`: shape, phase/spread, start/end rate and size, base Position reference;
- `StrobeBlock`: start/end rate and safety-capped intent;
- `LookTrigger` / `AutoloopTrigger` only where the accepted layer contract allows it;
- future extensible effect events through versioned typed payloads.

Curves use bounded ordered points with explicit interpolation. Events reference semantic targets and assets, never raw DMX channels.

### 12.3 Editing transactions

- pointer drag/resize/node edits use a component-local draft;
- commit sends one typed command with expected document generation;
- validation occurs before mutation;
- one command creates one coalesced Undo entry;
- cancellation changes nothing;
- conflicting stale-generation commits return an explicit result and reload/rebase path.

### 12.4 Legacy TrackScript compatibility

Existing `TriggerLook`, `ClearLook`, `TriggerAutoloop`, and `ClearAutoloop` cues remain readable and compilable. A migration adapter can represent them as vNext timeline events without deleting the original semantics. Conversion is deterministic and covered by golden old-project fixtures.

## 13. Compiler and Runner boundary

Compilation proceeds from an immutable Studio snapshot:

1. validate project and source generation;
2. resolve venue, fixture profiles, groups, roles, and capabilities;
3. resolve linked assets and timeline dependencies;
4. lower musical events to bounded compiled structures;
5. reject unsupported/unsafe/unresolved items with exact diagnostics;
6. serialize deterministically and compute package digest;
7. return `CompiledCandidate` without changing the active Runner;
8. request generation-stamped activation through the existing service;
9. keep the prior package active on rejection or incomplete acknowledgement.

The compiled package includes no audio paths, waveform, SoundSwitch payloads, migration UI state, or source-library database. Preview and Runner use the same compiler output; preview may substitute a no-DMX sink but cannot use a second lighting engine.

Runtime event execution, seek/loop/reverse semantics, and any expanded fixed-capacity TrackScript engine are coordinated with the Live lane through an explicit compiled-event contract. Studio does not modify the scheduler privately.

## 14. SoundSwitch decoder architecture

### 14.1 Versioned adapters

```text
SoundSwitchSourceReader
  -> version/artifact-specific decoder adapters
  -> Migration IR
  -> validation/reconciliation
  -> one reviewed Studio transaction
```

Adapters are selected by recorded source evidence, not by “latest.” Unknown versions remain inspectable and preserved.

### 14.2 Migration IR

The IR contains candidate objects with source identity and status:

```text
IRProject / IRVenue
IRProfileReference / IRFixture / IRGroup
IRPalette / IRPosition / IRAttribute
IRStaticLook / IRAutoloop
IRTrackIdentity / IRLightshow / IRTimelineEvent
IROpaqueArtifact
IRConflict / IRMissingDependency
```

It is deterministic, serializable for tests, and independent of the final project format. A future improved decoder can rerun against the preserved bundle and produce a new migration proposal without reacquiring source files.

### 14.3 No production name inference

Names may assist display or candidate matching. They cannot establish cue type, color values, timing, fixture address, mode, or exact semantics. The current semantic V1 template remains explicitly classified as an approximation path.

### 14.4 Import stages

1. **Acquire/inventory:** verify source bundle, limits, hashes, version, completeness.
2. **Decode:** produce IR and exact diagnostics; no project mutation.
3. **Reconcile profiles:** map source fixture references to qualified profile snapshots; unresolved items remain conflicts.
4. **Review content:** venue, groups, Looks, Autoloops, Positions, Attributes, tracks, and opaque items.
5. **Resolve media:** scan authorized audio/library evidence and rank track associations.
6. **Propose destination:** create new project, merge into project, or create new Venue/Rig; preview ID collisions and replacements.
7. **Commit once:** one validated document transaction with output disabled.
8. **Rehearse/compare:** preview, frame inspection, optional shadow comparison, and explicit activation.

## 15. Studio services and UI integration

Build services before cosmetic replacement:

- `StudioDocumentService` — generation, commands, Undo/Redo, snapshots, validation;
- `AssetIndexService` — external audio/library discovery and verified relinking;
- `AudioAnalysisService` — waveform/beatgrid/phrase cache jobs;
- `MigrationService` — inventory, decoder IR, reconciliation, reports, reviewed commit;
- `CompilationService` — cancellable immutable snapshot compilation and activation request;
- `PreviewService` — candidate package against visual/no-DMX or qualified output policy;
- document view-model providers for Library, Timeline, Waveform, Inspector, Migration, Validation, and History.

The existing Win32 pages remain a strangler adapter until commands and services are proven. Default and Reference place the same native components and bind the same commands/state; no feature is complete when it only exists in one skin or a developer CLI.

## 16. Dependency-ordered implementation program

### STUDIO-000 — continuity and contracts

- commit this plan and the build handoff;
- create issue #46 and cross-lane ownership rules;
- define source-corpus manifest, migration status enum, IR skeleton, and evidence-safe synthetic fixtures;
- add no runtime or visual change.

### STUDIO-001 — project/document compatibility foundation

- add `StudioDocumentService` around current `ProjectDocument`;
- generation-checked typed mutations and snapshot views;
- old-project golden corpus, unknown-record retention, save/history/Undo compatibility;
- reserve format-2 change until actual new persisted semantics land.

### STUDIO-002 — source evidence and migration IR

- versioned source manifest and completeness evaluation;
- deterministic IR schema/model and report;
- route current inspector/bundle/comparison and `convert-v1` evidence through shared source identities without changing its claims;
- synthetic controlled-delta tests.

### STUDIO-003 — media and track identity

- extend audio identity/provenance without weakening digest checks;
- cancellable index, metadata-tag reader, read-only DJ-library adapters, ranked resolver, and conflict UI model;
- no automatic filename-only association.

### STUDIO-004 — semantic authoring model

- Venue/Rig, palettes, Positions, Attributes, Movement/Effect assets;
- integer beatgrid/tempo/phrase structures;
- Master/Group/Fixture timeline and typed events;
- deterministic persistence and old TrackScript adapter.

### STUDIO-005 — compiler/runtime event contract

- agree compiled-event ABI/capacity with Live lane;
- deterministic lowering and diagnostics;
- preview and activation use identical candidates;
- seek/loop/reverse replay corpus owned jointly at the boundary.

### STUDIO-006 — waveform/beatgrid worker and native components

- bounded cache and analysis worker;
- timeline/waveform synchronized document views;
- native component controllers using shared commands;
- no audio access from Runner or skin packages.

### STUDIO-007 — project-content SoundSwitch decoders

- exact version-qualified venue/group/patch/Look/Autoloop/Position/Attribute decoding only where controlled evidence supports it;
- fixture reconciliation through qualified profile IDs;
- import report and output-disabled reviewed commit.

### STUDIO-008 — scripted-track migration

- lighting-file, TrackMap, metadata-tag, audio, and DJ-library adapters;
- exact/translated/opaque/conflict classification;
- timeline reconstruction and verified track association;
- preserve unmatched scripts and audio evidence.

### STUDIO-009 — complete Default and Reference Studio journeys

- Project Hub, Library, Venue/Patch, Static Look, Autoloop, Timeline, Waveform, Inspector, Migration, Validation, Preview, History;
- identical domain results across skins and legacy bridge;
- accessibility, compact/standard/wide behavior, and UI performance.

### STUDIO-010 — production qualification

- deterministic repeated import and round-trip;
- old/new project compatibility and restore recovery;
- large library/cue/track performance;
- seek/loop/reverse/pitch/cue-jump replay;
- fixture replacement/repatch without cue loss;
- malformed/oversized/corrupt source and cache fault matrix;
- Windows installed GUI smoke and machine-readable evidence;
- parity ledger updated only with proven rows.

## 17. First build slice after agent switch

The first work-agent lane should implement **STUDIO-000 + the bounded foundation of STUDIO-001/002**, not attempt the full timeline UI.

Required deliverables:

1. `StudioDocumentService` skeleton with immutable snapshots and generation-checked transaction results around current format-1 data;
2. migration source-manifest and IR/status types isolated in new files;
3. deterministic synthetic source fixtures and tests for exact/translated/approximate/opaque/conflict/missing classifications;
4. old-project open/save/unknown-record/Undo/history regression tests;
5. current SoundSwitch inventory/bundle/converter claims unchanged;
6. a CLI or developer report proving source completeness without exposing payload bytes;
7. additive command/state registry seeds for migration/document progress only after #31 ownership is reconciled;
8. issue #46 progress comment, file reservations, and next-slice handoff.

This creates the stable foundation on which waveform, detailed timeline, exact decoders, and both Studio skins can be built without repeated schema rewrites.

## 18. Qualification and acceptance matrix

### Compatibility

- every committed format-1 golden opens and compiles;
- save/reopen preserves known semantics and unknown records;
- Undo/Redo and durable restore points survive the service extraction;
- no old enum is renumbered or reinterpreted.

### Source preservation

- source bytes unchanged before/after every operation;
- bundle manifests and copied payload hashes match;
- symlinks, changing files, over-limit artifacts, and corrupt data fail closed;
- unavailable source reports are explicit.

### Migration determinism

- identical bundle + decoder versions + decisions produce identical IR, report, project, and package digests;
- a decoder upgrade creates a new run linked to the same source bundle and never overwrites prior evidence;
- exact and approximate results are visually and machine-readably distinct.

### Track/media identity

- moved files relink only under the approved identity policy;
- duplicate names and near-duration tracks never silently steal scripts;
- SoundSwitch UID, DJ-library ID, digest, fingerprint, metadata, and human decisions remain separately inspectable;
- missing audio does not corrupt the compiled semantic show.

### Authoring

- Master/Group/Fixture precedence is deterministic;
- Position/Attribute edits show dependency impact and update linked references in one transaction;
- drag/resize/node edits commit once and cancel cleanly;
- timeline and waveform UI drops do not affect playback timing.

### Compiler/Runner boundary

- compile never mutates the active package;
- invalid candidates cannot activate;
- active output survives compile, UI failure, skin switch, and rejected activation;
- package contains no source paths, source payloads, waveform, or importer state.

### UI equivalence

- representative commands return identical results through legacy Win32, Default, Reference, direct tests, keyboard, and MIDI where applicable;
- selection, active output, save state, validation, migration status, and progress are not inferred from UI timers;
- all critical operations have keyboard and accessible alternatives.

## 19. Token- and test-efficient agent protocol

1. Read `AGENTS.md`, `00_START_HERE.md`, this plan, `29_STUDIO_V1_BUILD_HANDOFF.md`, issue #46, and only the linked contract needed for the assigned slice.
2. Post a concise scope and reserve exact files before editing.
3. Do not repeat broad SoundSwitch research unless closing a named evidence gap.
4. Use synthetic fixtures in Git; private projects/audio/library databases remain outside Git.
5. Run narrow unit/serialization tests per edit; full native suite, Windows cross-link, installer smoke, and long tests only at merge/release gates.
6. Never add a second domain path inside a skin or component.
7. Keep progress reports to changed code, tests/evidence, blockers, and next dependency.
8. Update issue #46 and continuity artifacts additively; do not replace history with a fresh plan.

## 20. Ready-to-build gate

Planning is ready to hand to a work agent when this document, the build handoff, issue #46, and the continuity addendum are committed on the planning branch and reviewed against current `main`. The work agent begins with the first build slice above and rebases before touching shared files.
