# EmberLights Autoloops V2 — Parity, Migration, and Automation Plan

Status: **binding planning authority for issue #57** once this planning PR is accepted or merged.

Plan date: 2026-08-12  
Repository baseline: `main` at `f166a582b24972c6762022fd956ba868d2aae1cd`  
Planning branch: `planning/autoloops-v2-2026-08-12`

## 1. Mission

Build Autoloops into a production-grade EmberLights subsystem that:

1. reaches relevant SoundSwitch Autoloop authoring and performance parity;
2. preserves every existing EmberLights format-1 project and current live behavior;
3. replaces the whole-Static-Look-only authoring ceiling with portable semantic musical content;
4. migrates SoundSwitch Autoloops only to the fidelity proven by source-version evidence;
5. retains unsupported and unknown source data for later decoder upgrades;
6. gives DJs a useful original 128-loop starter library without copying vendor content;
7. provides deterministic rule-based AutoScript generation before optional AI assistance;
8. leaves one clear source-adapter seam for WOLFMIX and other products after SoundSwitch;
9. allows future AI agents to propose, validate, preview, and commit ordinary editable Autoloops; and
10. keeps Runner deterministic, bounded, offline, and allocation-free after package activation.

SoundSwitch is the first migration and parity target. WOLFMIX is secondary design evidence and a later import target. Neither product becomes a second engine inside EmberLights.

## 2. Binding context and ownership

This plan extends rather than replaces:

- `AGENTS.md`;
- `docs/00_START_HERE.md`;
- `docs/03_ARCHITECTURE.md`;
- `docs/06_PRIORITIZED_BACKLOG.md`;
- `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`;
- `docs/09_BUILD_AND_TEST_STANDARDS.md`;
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`;
- `docs/18_SOUNDSWITCH_MIGRATION.md`;
- `docs/28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md`;
- `docs/30_STUDIO_V1_CONTINUITY_CHECKPOINT.md`;
- `docs/32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md`;
- the modular command/state/skin program under `docs/UI_PROGRAM_START_HERE.md`;
- issue #46 for Studio/document/migration ownership;
- issue #52 for fixture/profile truth and Static Looks;
- issue #56 for lean Perform/Runner process and resource budgets; and
- issue #31 for shared command/state registry ownership.

### 2.1 Autoloops lane owns

- canonical editable Autoloop assets, placements, launch profiles, and selection metadata;
- compatibility adaptation from current format-1 Autoloops;
- deterministic lowering into immutable compiled Autoloop programs;
- bounded Autoloop evaluation and runtime selection/lifecycle behavior;
- Autoloop-specific Studio transactions and view models;
- original EmberLights Autoloop content packs;
- deterministic Autoloop AutoScript generation;
- SoundSwitch Autoloop evidence decoding and reviewed import through Studio’s migration IR;
- later source-adapter and AI-authoring-provider contracts after the SoundSwitch seam is proven.

### 2.2 Other lanes remain authoritative

**Studio / #46** owns document generations, save/history/Undo, shared 960-PPQ timing, reusable semantic assets, source corpus, migration status/evidence, preview and candidate activation requests.

**Live/output / #56** owns transport normalization, scheduler, atomic package activation, output, reconnect, safety, process isolation, resource budgets and last-known-good behavior.

**Fixture / #52** owns manufacturer/model/mode/channel/capability truth and physical qualification. Autoloops consumes qualified capability snapshots; it does not invent channel meaning.

**UI platform / #31 and skin issues** owns shared command/state naming, component bindings, skin runtime and presentation. Autoloops supplies behavior and toolkit-neutral state, not a parallel UI engine.

## 3. Reconciled current implementation truth

EmberLights already has a strong Autoloop foundation:

- a deterministic `64 × 32` address space: 64 banks, 32 slots, 2,048 possible placements;
- a four-bank pageable control window rather than a four-bank product ceiling;
- direct launch, clear, next, previous, bank enable/all/exclusive controls;
- safe move, swap, duplicate and next-open placement operations;
- Once, Infinite and TrackDuration repeat enums;
- active address, repeat, progress and completed-cycle Runner status;
- separate Autonomous, TrackScript and ManualAutoloop layers;
- higher EventMoment/Static Look, ManualOverride, Emergency and Safety layers;
- deterministic compile and generation-stamped activation;
- current Autoloop preview through the production compiler/renderer; and
- MIDI/controller foundations for banks and direct launch.

The current content representation is deliberately narrow:

```text
AutoloopDefinition
  id, name, bank, slot
  float length_beats
  repeat
  up to 32 steps

AutoloopStepDefinition
  float at_beat
  Static Look ID
  Cut | Linear
```

At runtime each step applies or blends an entire compiled Static Look. This is a useful compatibility path, but it cannot express SoundSwitch-grade Master/Group/Fixture tracks, property ranges, curves, palette intent, Position or Attribute references, movement shapes, fixture-role generation or rich migration evidence.

The present `convert-v1` workflow retains 32 SoundSwitch Autoloop names but creates native patterns from name heuristics. It is explicitly `Approximated`, output-disabled and useful as a pilot/regression artifact. It is not an exact Autoloop decoder and must not accumulate more guessed semantics.

## 4. SoundSwitch parity contract

Official SoundSwitch behavior establishes the minimum product expectations below. Observable behavior may be implemented independently; proprietary source, assets, defaults and trade dress are not copied.

| Capability | SoundSwitch reference behavior | EmberLights V2 contract |
| --- | --- | --- |
| Capacity | 128 loops in four banks of 32 | Familiar four-bank/128-loop starter window; native capacity remains 64×32 |
| Older 32-loop upgrade | Existing 32 loops distributed as eight per bank | Version-aware compatibility/import transform when source ordering is proven; never guess source version |
| Edit organization | Rearrange/move between banks, duplicate, populate empty slots | Stable-ID move/swap/reorder/duplicate; populate only empties; explicit copy-vs-second-placement action |
| Defaults | New default library; reset can overwrite existing loops | Original EmberLights pack; previewable Undoable reset limited to recognized pack-managed placements unless user explicitly chooses replacement |
| Fallback | Beatgrid-synced clips cycle on unscripted tracks | Runner-owned autonomous session and deterministic eligible-loop selection |
| Common lengths | 8/16/32/64/128-bar musical clips; AutoScript UI commonly offers 16–128 | Parity presets plus custom integer-tick length; no float-time drift |
| Performance feedback | Active/progress and bank indicators | Selected, queued/pending and active identity; progress, phase, cycles, source/reason and active/pending bank state |
| Exclusive bank | Takes effect after current loop finishes, unless a loop in that bank is manually launched | Pending exclusive bank applied on natural autonomous boundary; direct launch remains immediate |
| Manual launch | Can launch immediately over scripted track, play its duration and return | Explicit manual Overlay mode on ManualAutoloop layer; natural one-shot return to still-running lower layer |
| Override | Separate “Override Scripted Tracks” behavior | Explicit Replace mode, never inferred from Overlay launch |
| Repeat | Infinite and TrackDuration; new track deactivates TrackDuration and selects another loop | Once/Infinite/TrackDuration with normalized track-boundary contract and deterministic fallback |
| AutoScript | Presets, fixture categories, colors, Positions, Attributes, optional movement and Random modes | Versioned deterministic generator using equivalent semantic inputs, editable output and recorded seed/provenance |
| Positions | Reusable pan/tilt Positions placed in scripts/Autoloops | Linked stable Position assets with dependency impact and capability validation |
| Attributes | Reusable gobo/prism/focus/zoom/etc.; edits update linked uses | Linked stable Attribute assets; exact dependency graph and one-transaction updates |
| Movement | Circle, scans, ovals, figure/triple eights, square; start/end rate and size; Position integration | Versioned semantic movement generators with bounded parameters and deterministic evaluation |
| Manual tempo | Standalone tempo/phase/cue and MIDI launch | Existing manual BPM/tap foundation; explicit future quantized launch/phase contract |

Parity does not require reproducing unsafe behavior. EmberLights may improve reset/recovery, conflict review, portability, safety, capacity and diagnostics while preserving the recognizable workflow and output semantics.

## 5. Non-negotiable invariants

1. **One semantic engine.** SoundSwitch, WOLFMIX and future products translate into EmberLights. Runner never switches to a vendor mode.
2. **Source, document and compiled package remain separate.** Vendor evidence never writes Runner structures directly.
3. **Bank/slot is placement, not identity.** Moving a loop never changes its stable content ID or dependent references.
4. **Format-1 remains readable.** Existing projects and unknown records remain covered by golden fixtures.
5. **Legacy output remains exact.** The current whole-Look pattern is retained as a compatibility specialization until frame equivalence is proven.
6. **Musical time is integer.** V2 uses the accepted signed 64-bit, 960-PPQ Studio time contract; no second timing system.
7. **Content is semantic.** Canonical source events refer to properties, targets and reusable assets—not raw DMX slots.
8. **Fixture truth has one owner.** Capability resolution uses qualified profiles and reports degradation/conflict explicitly.
9. **No false migration fidelity.** Names, paths, slot counts or visual similarity do not establish cue semantics.
10. **Unknown source survives.** Unsupported payload remains byte-identical in the verified source bundle and linked by digest/evidence.
11. **Runner is bounded and offline.** No parser, filesystem access, UI callback, analysis worker, arbitrary script, model or network call on the live path.
12. **No scheduler allocation.** Package load/activation may allocate bounded immutable storage; ticks may not.
13. **One behavior, many surfaces.** Default, SoundSwitch Reference, Safe, MIDI, keyboard and future surfaces use the same commands/state.
14. **Every edit is transactional.** A committed authoring gesture is generation-checked, validated and one Undo entry.
15. **Claims follow evidence.** Implemented, compiled, installed, hardware-tested, gig-qualified and parity-complete remain distinct.

## 6. Five-part canonical Autoloop model

### 6.1 `AutoloopAsset`

Stable identity and authored content, independent of where it appears in the performance grid.

```text
assetId
name, description, tags, style, energy, categories
programId
launchProfileId
provenanceId
revision/compatibility metadata
```

An asset can be placed more than once. “Duplicate content” creates a new stable asset and deep-copies or intentionally links reusable dependencies. “Place again” creates another placement referencing the same asset. The UI must make this distinction explicit.

### 6.2 `AutoloopPlacement`

Performance organization only:

```text
placementId
bank (0..63)
slot (0..31)
assetId
display override metadata when intentionally supported
content-pack management identity
```

The fixed `64 × 32` placement table remains ideal for deterministic lookup, MIDI mapping and controller paging. Empty placement is valid. Silent overwrite is not.

### 6.3 `AutoloopProgram`

A bounded musical program with stable target lanes and typed semantic events.

```text
programId
lengthTicks
optional musical metadata: bars/time-signature/grid/phrase markers
ordered lanes
ordered typed events
```

#### Target scopes

- Master/all eligible fixtures;
- stable Group ID;
- stable Fixture ID;
- semantic role/category selector;
- future cell/beam selector through a versioned selector payload.

Role/category selectors are resolved against one immutable venue/profile snapshot at compile time. The compiler records exactly which fixtures were selected and rejects or warns when a later venue revision changes the result.

#### Event families

- `LegacyLookSequenceEvent` or equivalent compatibility reference;
- property Set, Release or ForceZero block;
- bounded property curve with ordered points and explicit interpolation;
- palette/color block using shared `StudioColor`/palette assets;
- Position reference;
- Attribute reference;
- movement generator block;
- beam/effect generator or sequencer block;
- transition/easing profile reference;
- future typed event through a versioned, bounded extension—not an arbitrary script.

Each ranged event uses `[startTick, endTick)` and explicit end ownership. Source ordering is deterministic. Overlapping ownership on the same target/property is either resolved by an explicit intra-program lane priority/mix rule or rejected; vector ordering never becomes accidental precedence.

### 6.4 `AutoloopLaunchProfile`

Playback policy is separate from content:

```text
repeat: Once | Infinite | TrackDuration
launch: Immediate | NextBeat | NextBar | NextPhrase
phase origin/alignment
mode: Overlay | Replace
return boundary/fade
track-boundary requirement
```

SoundSwitch-compatible direct launch defaults to Immediate. Quantized modes are additive for standalone/controller workflows. A profile cannot weaken Emergency/Safety behavior.

### 6.5 `AutoloopProvenance`

```text
origin: native | contentPack | generated | migrated
contentPack/generator/adapter ID + version
generation parameters + seed when applicable
source bundle/artifact/object keys
migration status/rules/evidence refs
human resolution/acceptance metadata
```

Provenance affects migration and regeneration workflows, never playback. Runner receives no private source paths or opaque vendor payloads.

## 7. Reusable semantic assets

Autoloops reuse the Studio asset system rather than embedding venue-fragile values everywhere:

- color palettes/swatches;
- Positions;
- Attributes;
- movement presets;
- effect/transition presets;
- Static Looks where intentionally referenced;
- style/generator presets.

Linked Position and Attribute edits show affected Autoloops/scripts and commit as one Undoable transaction. A future “freeze/snapshot dependency” mode may be added without changing linked-by-default behavior.

A reusable asset can carry portability requirements and venue-specific realization. Example: a Position can be globally named but contain per-venue fixture values. Missing realization is a conflict, not a guessed pan/tilt.

## 8. Compatibility and persistence

### 8.1 Format-1 adapter

Every current `AutoloopDefinition` becomes a deterministic V2 compatibility program:

- stable asset/program/placement IDs derived from existing stable loop ID;
- `length_beats` converted to ticks through one documented rounding rule;
- each step becomes a legacy whole-Look event at the equivalent tick;
- Cut/Linear preserved exactly;
- repeat preserved;
- bank/slot preserved;
- unknown records preserved untouched.

Load, compile, save and reopen of an unchanged format-1 project must not silently rewrite it merely because V2 exists.

### 8.2 Rich persistence

Do not squeeze rich events into the old `STEP` record or reinterpret existing enum values. Coordinate with #46 on one explicit versioned project extension or format-2 transition that includes:

- deterministic field/record ordering;
- stable IDs;
- bounds matching implementation;
- checksum and atomic save/history behavior;
- `v1 -> vNext` adapter;
- golden old/new fixtures;
- unknown/future record retention; and
- written downgrade/export limitations.

### 8.3 SoundSwitch 32-to-128 layout

Official SoundSwitch 2.9 upgrades older 32-loop libraries by distributing eight loops into each of four banks. EmberLights may offer that deterministic transform only when the source adapter proves the source is a compatible older layout and ordering. It must remain a named import/compatibility action, not a universal assumption.

## 9. Compiler and immutable package

### 9.1 Lowering stages

```text
immutable Studio snapshot
  -> validate IDs/time/order/dependencies
  -> resolve target selectors against qualified venue/profile snapshot
  -> realize colors/attributes/capabilities
  -> lower reusable references and generators
  -> build placement index + compact program arenas
  -> compute deterministic diagnostics and digest
  -> candidate activation through existing generation-stamped boundary
```

### 9.2 Compact arenas

Do not reserve worst-case rich-event storage inside every one of 2,048 placement slots. Use:

- fixed `64 × 32` placement/index table;
- a bounded program-header array for used unique assets/programs;
- contiguous event, curve-point, target-span and reference arenas sized during compile;
- per-program offsets/counts;
- exact upper bounds selected from content-pack and stress evidence;
- fail-before-activation on overflow;
- immutable storage after candidate construction.

Sharing one asset across placements stores one program. A true duplicate stores a second program only when content differs.

### 9.3 Evaluator

A compiled evaluator receives only:

```text
compiled program
phase/tick
active fixture/profile-independent semantic target spans already resolved
LayerBuffer destination
```

It performs deterministic bounded arithmetic and no allocation. Versioned movement/effect evaluators use stable mathematical definitions, clamped parameters and test vectors. They do not invoke a general expression language.

### 9.4 Legacy fast path

Retain the present `AutoloopPattern`/Static Look behavior as a specialization or compatibility evaluator until:

- boundary and interpolation frames match byte-for-byte;
- Once/Infinite/TrackDuration lifecycle matches;
- rewind/wrap/negative-time behavior is defined and matched;
- current performance/no-allocation tests pass; and
- installed Windows evidence shows no regression.

Removal is optional; correctness and compatibility matter more than architectural tidiness.

## 10. Runner director and layer behavior

### 10.1 Sessions

One Runner-owned director manages three explicit sessions:

| Session | Layer | Purpose |
| --- | --- | --- |
| Autonomous | `Autonomous` | automatic fallback for unscripted/eligible transport |
| Scripted | `TrackScript` | Autoloop event invoked by a track timeline |
| Manual | `ManualAutoloop` | direct performer launch |

`EventMoment` Static Looks, `ManualOverride`, `Emergency` and `Safety` remain higher-priority layers. Covering a loop does not stop or restart its clock. On release, the underlying loop is revealed at its current phase, matching the user’s required continuation behavior.

### 10.2 Autonomous selection

The selection engine consumes populated compatible placements, active bank mask, pending exclusive bank, policy and deterministic seed.

Initial supported policies should include:

- Sequential;
- DeterministicShuffle with bounded no-repeat window;
- Weighted/style/energy-ready interface for later generation and richer DJ context.

The exact default SoundSwitch ordering is not assumed from documentation. The SoundSwitch Reference behavior must be based on observable tests; otherwise EmberLights documents its deterministic selection difference while matching bank/boundary behavior.

### 10.3 Bank state

Keep authored bank metadata separate from transient performance filtering.

```text
activeBankMask
pendingBankMask
activeExclusiveBank? 
pendingExclusiveBank?
```

An exclusive-bank request during an autonomous loop becomes pending and activates at its natural boundary. Manual launch from that bank is immediate and may make the pending selection visible sooner, matching documented SoundSwitch behavior. Direct launch outside the filter remains intentional and valid.

### 10.4 Overlay versus Replace

These are separate typed modes.

**Overlay** applies the manual program on `ManualAutoloop`; it owns only its compiled properties. Lower scripted/autonomous layers continue running. Once returns naturally.

**Replace** intentionally suppresses the relevant lower automated content for its lifetime while still allowing ManualOverride, Emergency and Safety. Exact all-property suppression/ownership needs A/B observation and an explicit compiled mask; it must not be approximated by clearing arbitrary layers or resetting the track script. Underlying musical clocks should continue unless evidence and product decision explicitly choose pause semantics.

### 10.5 Repeat and track boundary

- Once: one program length, then return.
- Infinite: cycle until cleared/replaced.
- TrackDuration: cycle until a normalized track-epoch/end event, then deactivate and allow autonomous selection.

Exact TrackDuration parity requires a trustworthy normalized track boundary from the active DJ integration. If OS2L does not expose enough identity/transport detail, Runner must publish `trackBoundaryUnavailable` rather than pretend a BPM change or filename is a new track. Direct VirtualDJ and later Serato adapters can advance the normalized track epoch.

### 10.6 Status

Runner, not UI, publishes:

- selected asset/placement;
- queued/pending asset/placement and reason;
- active asset/placement;
- source: autonomous/scripted/manual;
- mode: overlay/replace;
- repeat;
- phase/progress/completed cycles;
- active and pending bank state;
- selection policy/reason;
- track-boundary capability;
- package generation and last transition/result.

Status is bounded, atomic and rate-classified through the shared UI contract.

## 11. Studio authoring contract

### 11.1 Transactions

Autoloop authoring operations use `StudioDocumentService` expected-generation checks. One gesture may use a temporary draft; commit is one typed transaction and one Undo entry.

Required operations:

- create/edit/rename/delete with dependency report;
- duplicate asset;
- place existing asset;
- move/swap/reorder/next-open;
- assign/unassign placement;
- edit bank metadata/tags;
- bulk replace target/palette/Position/Attribute/style;
- populate empty placements from a pack;
- reset recognized pack-managed placements with impact preview;
- import/re-import proposal review and one output-disabled commit.

### 11.2 Editor services

Toolkit-neutral services expose:

- library and bank/slot view models;
- program lanes/events and inspector drafts;
- snap/grid and parity length presets;
- reusable asset pickers;
- capability/degradation report;
- dependency impact;
- compile diagnostics;
- exact no-output preview; and
- content-pack/import/generation provenance.

The visual editor can later resemble the SoundSwitch workflow through the Reference skin, but its behavior remains shared with Default, keyboard, MIDI and future surfaces.

## 12. Original EmberLights content pack

Ship an independently authored, versioned starter pack with at least 128 useful placements in the first four-bank window. It must not copy SoundSwitch names, patterns, presets, fixture library or visual assets.

Suggested bank concepts are product-level, not hard-coded engine modes:

1. subtle, ambient, slow and formal-event-safe;
2. colorful, flowing and medium-energy;
3. rhythmic, contrast, build and drop;
4. high-energy, beam/mover-aware and conservative-strobe content.

Each source asset is semantic and fixture-independent where possible. Compilation produces exact/degraded/unsupported diagnostics for the current venue. Partial rigs remain useful: a color-only rig still receives color/intensity behavior; mover-only events do not invalidate unrelated color lanes.

Pack contract:

```text
packId + semantic version
content digest
asset/placement management keys
required engine/model versions
source provenance/license
populate/reset policy
representative compile evidence
```

Populate fills only empties. Reset defaults must be Undoable and preview its replacements. The safer EmberLights behavior intentionally improves on a destructive reset workflow.

## 13. Deterministic AutoScript

AutoScript is a Studio generator, not a runtime effect chooser.

### 13.1 Inputs

- program length, grid and phrase shape;
- style, energy, complexity and variation;
- eligible fixture roles/categories/groups;
- selected palettes/swatches;
- selected Positions and Attributes and their order;
- movement/effect choices and bounds;
- transition/ownership rules;
- qualified fixture capabilities;
- project safety policy;
- generator ID/version and seed.

### 13.2 Output

The generator emits an ordinary editable `AutoloopAsset` and `AutoloopProgram`, plus provenance and diagnostics. No opaque recipe is required for playback.

### 13.3 Reproducibility

```text
same normalized inputs + generator version + seed
  => same source serialization + compiled digest
```

Random presets use an explicit seed. Changing the seed changes only documented choices. Generation is cancellable and off UI/Runner threads. Preview and validation occur before one accepted transaction.

The generator must never emit unarmed hazards. Strobe generation is conservative and safety-clamped. Unsupported capabilities produce substitution or omission only through an explicit rule recorded in diagnostics.

## 14. SoundSwitch Autoloop migration

### 14.1 Present evidence boundary

The current authorized SoundSwitch backup and current generated EmberLights pilot are not proven to share the same Venue/Autoloop source hashes. Treat them as separate source evidence. The existing 32 names may be retained as user labels, but their native patterns remain approximations until a matching baseline and controlled deltas establish the source fields.

### 14.2 Adapter pipeline

```text
read-only source probe and version gate
  -> bounded inventory / source-corpus manifest
  -> source-version-specific decode
  -> object/field migration IR + evidence
  -> fixture/group/asset reconciliation
  -> deterministic destination proposal
  -> human review/conflict resolution
  -> one validated output-disabled Studio transaction
```

Reuse existing migration statuses:

- Exact;
- DeterministicallyTranslated;
- Approximated;
- PreservedOpaque;
- Unsupported;
- Conflicted;
- MissingDependency;
- RejectedUnsafe.

Each imported field/object records adapter ID/version, source artifact and record/byte-range identity when known, rule ID and destination ID. Display names never become object identity.

### 14.3 Controlled-delta program

For each supported SoundSwitch source version, collect authorized baseline/delta pairs that change one property at a time:

- add/delete/rename;
- bank/slot/move/duplicate;
- length/grid;
- Master/Group/Fixture target;
- one color/palette event;
- one Position reference;
- one Attribute reference;
- one movement shape and one parameter;
- transition/fade;
- repeat/override setting when persisted;
- older 32-loop, known 59-loop and current 128-loop layouts when available.

Record source version, export options, exact artifact hashes and changed ranges. A field is production-decoded only after repeatable deltas and synthetic regression fixtures exist. If required evidence is unavailable, use stable blocker `soundswitch.autoloop_delta_corpus_unavailable`, retain opaque source and continue native authoring work.

### 14.4 Idempotent re-import

Canonical source key:

```text
adapterId + adapterVersion + sourceBundleId + artifactId + sourceObjectKey
```

A later adapter can propose upgrades from opaque/approximate to translated/exact. It must show the diff and never silently overwrite human edits. Conflict resolution records whether the user kept native, accepted source, duplicated or merged.

### 14.5 Current converter

Do not mutate `convert-v1` into the production decoder. Keep its explicit approximation warnings and output-disabled project. A later reviewed path may supersede its Autoloops while retaining the original report/source bundle and comparison evidence.

## 15. Cross-source interoperability

Do not build a broad plugin ABI before two real adapters prove the requirements. First implement the SoundSwitch adapter in ordinary Studio code. Then extract a small lifecycle:

```text
Descriptor -> Probe -> Inventory -> Decode -> Reconcile -> Plan -> Commit -> Upgrade
```

Every adapter is bounded, read-only, version-gated and evidence-bearing.

### 15.1 WOLFMIX seam

Official WOLFMIX material describes project/fixture backup through WTOOLS/USB and separate Color FX, Move FX, Beam FX, static controls, presets and sequencers. These are useful semantic compatibility targets:

- groups and fixture order/linking -> target selectors and phase/order;
- Color FX -> palette/color effect blocks;
- Move FX -> movement generators;
- Beam FX/sequencer -> beam/effect events;
- static colors/positions/gobos -> shared assets/Looks;
- presets/intelligent presets -> content-pack/generator provenance.

This is a mapping hypothesis, not a decoded file claim. WOLFMIX source migration begins only with authorized exports and controlled deltas. Unsupported source remains opaque.

## 16. AI authoring-provider seam

Future AI assistance uses typed Studio tools:

```text
read bounded project/capability/asset summaries
  -> propose normal Autoloop draft
  -> validate
  -> compile
  -> preview/simulate
  -> return diff, diagnostics and provenance
  -> human accept/reject/edit
  -> commit once through StudioDocumentService
```

The provider never writes project files directly, sends DMX, controls a live show or bypasses safety. Accepted content is ordinary editable source and works offline without the provider.

Persist provider/version/model identifier, normalized request/style constraints, project generation/capability snapshot, tool versions, optional seed, diagnostics and human acceptance. Private device credentials, arbitrary file access and live output are outside the provider contract.

The deterministic built-in generator remains the default offline capability even after AI integration exists.

## 17. Resource and safety budgets

Autoloops V2 must satisfy issue #56 and existing release gates:

- zero scheduler allocations after activation;
- no source parsing, filesystem, network, model, UI formatting or slow device work on scheduler;
- compact used-content arenas under headless/Runner memory ceilings;
- dense semantic content CPU and p99 jitter within Windows/VirtualDJ budgets;
- bounded commands/status and no repaint-driven timing;
- safe overflow rejection before activation;
- exact last-known-good behavior on invalid candidates;
- generation-safe commands/frames through package activation;
- no hazard activation outside existing arm/safety policy;
- source/import workers cancellable and isolated from Perform.

Large source capacity does not mean all 2,048 placements must contain worst-case dense programs simultaneously. Product limits must be explicit, measured and gracefully diagnosed.

## 18. Test and qualification matrix

### 18.1 Compatibility

- every current format-1 golden project;
- unknown-record preservation;
- v1-to-V2 adapter deterministic serialization;
- legacy frame equivalence at every transition boundary and wrap;
- placement move/swap/duplicate identity behavior.

### 18.2 Semantic evaluator

- every event family and interpolation;
- overlapping ownership diagnostics;
- target selector resolution and venue revision drift;
- color realization exact/degraded/unsupported matrices;
- Position/Attribute dependency updates;
- movement/effect mathematical golden vectors;
- rewind, seek, reverse and looped phase inputs;
- capacity and malformed-record rejection;
- zero allocation.

### 18.3 Runtime director

- autonomous selection and cycling;
- enabled/all/exclusive banks and pending boundary;
- direct launch outside filter;
- manual Once/Infinite/TrackDuration over autonomous and scripted content;
- Overlay vs Replace;
- Static Look toggle/hold continuation;
- ManualOverride/Emergency/Safety interaction;
- track boundary available/unavailable;
- package hot activation, moved/deleted placement and stale command;
- beat loss/prediction/recovery and manual tempo;
- deterministic replay digest.

### 18.4 Studio/content/generator

- every operation generation/Undo/save/reopen;
- dependency impact and deletion protection;
- pack populate/reset/idempotency;
- representative fixture-capability matrices;
- generator seed/version reproducibility;
- cancellation/stale generation/no mutation;
- exact no-output preview;
- 2,048-placement browsing and bounded compile/validation.

### 18.5 Migration

- read-only source and source-byte immutability;
- source-version gates and corruption/fuzz cases;
- one-change delta fixtures;
- exact status/evidence serialization;
- opaque retention;
- conflicts/missing dependencies;
- idempotent re-import and later decoder upgrade;
- mismatch between backup and project baseline;
- no private source bytes/paths in Git or portable reports.

### 18.6 Installed qualification

- Windows Release build and installer startup;
- VirtualDJ same-PC co-load and separate-PC mode;
- OS2L/manual tempo/Control One command and feedback paths;
- reference and low-end machine CPU/RSS/jitter;
- 8-hour dense Autoloop soak;
- adapter reconnect, sleep/resume, project activation and Studio crash/restart;
- physical fixture comparison for representative color, mover and beam content;
- rehearsal before gig-qualified claim.

## 19. Work-package sequence

| Order | Issue | Branch | Outcome |
| ---: | --- | --- | --- |
| 0 | #57 | `planning/autoloops-v2-2026-08-12` | accepted plan, backlog and boundaries |
| 1 | #58 / AL2-001 | `agent/autoloops-v2-model` | source model, v1 compatibility, compact compiled program/evaluator |
| 2 | #59 / AL2-002 | `agent/autoloops-v2-runtime` | Runner director, SoundSwitch performance semantics and status |
| 3 | #60 / AL2-003 | `agent/autoloops-v2-studio-migration` | Studio services, original pack, deterministic AutoScript, SoundSwitch evidence import |
| Later | #61 / AL2-004 | split when promoted | WOLFMIX adapter research/import and AI provider as separate passes |

#58 and #59 should remain sequential because the runtime must consume a stable compiled contract. #60 may begin its toolkit-neutral authoring services after #58 and PR #53, but final compile/preview/qualification must reconcile with #59. #61 remains backlog until the SoundSwitch adapter proves the seam.

## 20. Cross-agent collision rules

- Rebase each pass onto current `main` immediately before shared-file work.
- Post issue scope and exact file reservation before editing.
- Prefer new modules first; touch shared compiler/Runner/registry/build files only for integration.
- PR #53 currently owns Studio color/preview modules and shared build files; do not duplicate those contracts.
- #58 does not touch Runner/output/UI shell.
- #59 does not touch source decoders/Studio authoring/fixture profiles.
- #60 does not touch Runner/output/shared command registry without a separate coordinated reservation.
- A conflicting lane keeps its file; the Autoloop pass isolates code and defers wiring rather than overwriting.
- Continuity updates are additive in #57 and the parity ledger after behavior exists.

## 21. Claim boundaries

At plan time:

- scalable placement/catalog foundation: implemented;
- basic whole-Look Autoloop playback: implemented;
- Static Look continuation over underlying Autoloops: architectural foundation implemented, installed parity still to qualify;
- rich semantic Autoloop document/program: not implemented;
- complete autonomous director: not implemented;
- full SoundSwitch performance parity: not qualified;
- deterministic Autoloop AutoScript: not implemented;
- original 128-loop EmberLights pack: not implemented;
- exact SoundSwitch Autoloop timeline migration: not implemented;
- WOLFMIX file migration: not implemented;
- AI authoring provider: not implemented;
- current 32-loop converter content: approximate/output-disabled.

No planning document, issue, UI matrix or generated content alone closes parity rows. Evidence must be merged, installed and qualified at the tier claimed.

## 22. Primary official behavior sources

SoundSwitch:

- [Autoloop Improvements in SoundSwitch 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9)
- [What’s New in SoundSwitch 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858481-soundswitch-what-s-new-in-soundswitch-2-9-)
- [Autoloops Explained](https://support.soundswitch.com/en/support/solutions/articles/69000847100-soundswitch-autoloops-explained)
- [How to Autoscript Autoloops](https://support.soundswitch.com/en/support/solutions/articles/69000844240-soundswitch-how-to-autoscript-autoloops)
- [Pan and Tilt Control with Position Cues](https://support.soundswitch.com/en/support/solutions/articles/69000847598-soundswitch-pan-and-tilt-control-with-position-cues-)
- [Controlling Gobo, Prism & more with Attribute Cues](https://support.soundswitch.com/en/support/solutions/articles/69000828942-controlling-gobo-prism-more-with-attribute-cues)
- [Movement Shape Effects](https://support.soundswitch.com/en/support/solutions/articles/69000847396-soundswitch-movement-shape-effects)
- [Using SoundSwitch in Standalone Mode](https://support.soundswitch.com/en/support/solutions/articles/69000847411-soundswitch-using-soundswitch-in-standalone-mode)

WOLFMIX:

- [WOLFMIX W1 product/effects overview](https://www.wolfmix.com/wolfmix-w1)
- [WOLFMIX downloads, WTOOLS and reference manual](https://www.wolfmix.com/download)

These sources establish observable workflow targets. Private source-format semantics still require authorized controlled-delta evidence.
