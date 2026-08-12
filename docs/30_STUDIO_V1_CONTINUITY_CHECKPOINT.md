# Studio V1 Continuity Checkpoint

Checkpoint date: 2026-08-11

Repository baseline reviewed: `main` at `5f192e153a96a6c504674f23592887b6484d5835`

Planning branch: `planning/studio-v1-2026-08-11`

Primary issue: #46 — Studio V1 authoring, SoundSwitch source compatibility, migration, and track timeline

Binding documents:

- `28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md`
- `29_STUDIO_V1_BUILD_HANDOFF.md`
- existing UI program, parity ledger, migration guide, architecture, and fixture plans

## 1. Current product truth

### Merged foundation

EmberLights already has:

- deterministic two-universe Runner and layered semantic rendering;
- Static Look, Autoloop, basic TrackScript, MIDI, OS2L, output, safety, validation, compile, activation, history, and diagnostics foundations;
- checksummed format-1 projects with unknown-record retention;
- external content-identified audio records and strict verified relinking;
- read-only SoundSwitch inspection, comparison, source bundling, and a narrow source-qualified `convert-v1` path;
- the complete modular UI/skin plan and first command/state facade slice.

### Current migration boundary

The generated 2026 V1 project is useful but deliberately approximate:

- its patch is a safe staged layout, not a decoded physical SoundSwitch patch;
- its 32 Autoloop names are retained, but native patterns were rebuilt from names;
- opaque movers, effects, purchased track shows, and scripted-lightshow payloads remain in the preserved source;
- output remains disabled until physical patch/profile review.

No agent may describe that conversion as complete SoundSwitch project or song-file migration.

### Current Studio boundary

The existing TrackScript is a manually triggered beat-relative list of Look/Autoloop actions. It is not yet the detailed SoundSwitch-grade timeline model required for Master/Group/Fixture curves, colors, Positions, Attributes, movement shapes, strobe blocks, waveform, beatgrid, phrases, or exact track association.

## 2. Source availability checkpoint

A File Library search performed during this planning pass found the planning ledger and generated EmberLights pilot project/report, but did not expose a directly usable raw `.ssproj` payload tree, copied scripted audio corpus, or DJ-library database.

This does not mean the user lacks those files. It means a work agent must not assume they are mounted or present in Git.

Required unavailable-source result:

```text
authorized_soundswitch_corpus_unavailable
```

Private SoundSwitch/audio/library source remains outside Git. Synthetic fixtures are used in CI.

## 3. Accepted Studio planning decisions

These decisions are accepted for issue #46 planning and should be promoted into the main decision ledger when the planning branch is merged/approved.

### ST-D001 — three separate representations

SoundSwitch/audio/library evidence, the editable Studio document, and the immutable compiled Runner package remain separate versioned representations. Decoders never write Runner structures, and the compiler never parses source artifacts.

### ST-D002 — `.ssproj` is not presumed complete for scripted tracks

Project, lighting-file, copied-audio, metadata-tag, TrackMap, and DJ-library evidence are inventoried independently. Missing external evidence is reported explicitly rather than interpreted as “no scripted work.”

### ST-D003 — object-level migration status

Every decoded candidate uses one of: Exact, DeterministicallyTranslated, Approximated, PreservedOpaque, Unsupported, Conflicted, MissingDependency, or RejectedUnsafe. There is no misleading project-wide confidence percentage.

### ST-D004 — no name/path-only semantics

Names may aid display and candidate review. Names, filenames, and paths cannot establish exact cue data, fixture patch/mode, or automatic track association.

### ST-D005 — strict track identity baseline

Content digest plus size remains the strict baseline. SoundSwitch UID, DJ-library ID, fingerprint, duration, and metadata are separate evidence. Filename/path alone never auto-binds a lightshow.

### ST-D006 — integer authoring time

The vNext authoring model uses signed 64-bit absolute microseconds plus signed 64-bit musical ticks at 960 PPQ, connected through a versioned tempo/beatgrid map. Current float-beat cues receive a deterministic compatibility adapter.

### ST-D007 — semantic timeline scope

Detailed scripts contain Master, Group, and Fixture tracks with semantic typed events. They do not persist raw DMX channel blocks. Fixture replacement/repatching preserves stable identity and reports capability conflicts.

### ST-D008 — linked Position and Attribute assets

Position and Attribute Cues are stable reusable assets. Intentional edits show dependency impact and update linked scripts/Autoloops in one Undoable transaction, preserving the useful SoundSwitch behavior without silent deletion.

### ST-D009 — output-disabled reviewed import

Migration produces a reviewed document transaction with output disabled. Compile success alone cannot activate an imported patch; profile/mode/address/safety and operator activation gates remain.

### ST-D010 — first work slice is foundation only

The next work-agent slice ends after the Studio document service, source-corpus manifest, migration IR/report, schemas, deterministic synthetic tests, and build integration. It does not begin waveform, timeline UI, speculative decoding, or Live changes.

## 4. Studio backlog and dependency order

| Order | Work package | Depends on | Completion evidence |
| ---: | --- | --- | --- |
| 0 | STUDIO-000 continuity/contracts | current repo review | docs 28–30, issue #46, AGENTS routing |
| 1 | STUDIO-001 document service | format-1 project/history | generation/Undo/save/unknown-record tests |
| 2 | STUDIO-002 source manifest + migration IR | existing inspector/bundle | schemas, deterministic synthetic reports, no claim drift |
| 3 | STUDIO-003 media/track identity | audio asset foundation | cancellable read-only index, ranked resolver, conflict tests |
| 4 | STUDIO-004 persisted vNext authoring model | accepted compatibility plan | Venue/Rig, assets, beatgrid, tracks/events, old cue adapter |
| 5 | STUDIO-005 compiler/runtime event contract | Live-lane agreement | deterministic lowering, fixed capacities, replay tests |
| 6 | STUDIO-006 waveform/beatgrid worker/components | media identity + UI platform | disposable cache, native views, no Runner audio access |
| 7 | STUDIO-007 project-content decoders | controlled delta evidence + fixture IDs | exact/translated import and reviewed output-disabled commit |
| 8 | STUDIO-008 scripted-track decoders | lighting/audio/library corpus + timeline | verified association and exact/opaque/conflict results |
| 9 | STUDIO-009 Default/Reference Studio journeys | UI runtime + components/services | identical commands/outcomes across skins |
| 10 | STUDIO-010 production qualification | all prior gates | installed Windows evidence, performance/fault/round-trip/parity updates |

## 5. Cross-lane coordination

### Live/output lane

May continue independently. Studio must not edit runtime/output files in the first slice. Any future expanded compiled-event ABI is jointly reserved and reviewed before implementation.

### Fixture lane

May continue independently. Studio stores stable profile/revision/capability evidence and conflict state, but does not establish channel truth or physical qualification.

### UI command/state lane

Issue #31 owns registry naming and facade expansion. Studio foundation may define internal service results but must not seize `UiCommandId`, `UiInvocationResult`, or shared registry files without coordination.

### Skin/toolkit lane

Existing Default/Reference/native-component specifications remain binding. Foundation services come first; a cosmetic Win32 rewrite is not a Studio milestone.

## 6. Merge and build readiness

Planning artifacts created on the branch:

- full Studio architecture/migration plan;
- exact first-slice build packet;
- AGENTS routing for Studio work;
- issue #46 with complete work-package sequence.

After the planning PR is reviewed/merged—or Joshua explicitly directs work from the planning branch—the next agent can begin `agent/studio-v1-foundation` using doc 29.

## 7. Claim boundaries

At this checkpoint:

- Studio foundation planning: **complete**;
- implementation: **not started**;
- exact broad SoundSwitch semantic decoder: **not implemented**;
- exact scripted-song migration: **not implemented**;
- raw private corpus availability to an agent: **unconfirmed**;
- current pilot conversion: **validated approximate/output-disabled**;
- UI skins: **planned, not broadly implemented**;
- Live/output/fixture physical qualification: owned by their active lanes.

Do not report Studio V1, SoundSwitch migration, or parity as complete until issue #46 and the parity ledger evidence say so.

## 8. Next action

Switch this planning lane to a work agent and execute only the bounded foundation packet in doc 29. Preserve issue #46 as the additive source of progress, decisions, file ownership, and handoff—not a new replacement plan per pass.
