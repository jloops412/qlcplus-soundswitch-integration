# Autoloops V2 — Work-Agent Handoff

Status: execution packet for issue #57 and work packages #58–#60. Issue #61 remains deferred backlog.

Authority:

- `docs/33_AUTOLOOPS_V2_PARITY_MIGRATION_AND_AUTOMATION_PLAN.md`;
- issue #57;
- work-package issues #58, #59 and #60;
- existing architecture, Studio, fixture, command/state, migration and release contracts.

This packet is designed for **three bounded implementation passes**. Do not turn the work into one large cross-cutting branch, and do not restart architecture planning inside a work pass.

## 1. Merge order

```text
planning/autoloops-v2-2026-08-12
  -> accepted/merged

agent/autoloops-v2-model          #58 / AL2-001
  -> merged

agent/autoloops-v2-runtime        #59 / AL2-002
  -> merged

agent/autoloops-v2-studio-migration  #60 / AL2-003
  -> merged in bounded subcommits/PR if needed
```

PR #53 currently owns rich Studio color/preview modules and several shared build files. Before #58 or #60 begins, inspect whether #53 has merged. Never duplicate its final `StudioColor`, palette, realization or `StudioPreviewService` contracts.

#60 may prototype isolated toolkit-neutral authoring modules after #58 and #53, but the lowest-risk program is sequential. Its final compile/preview qualification must rebase onto #59.

Issue #61 must not start until #57 explicitly promotes it. When promoted, split WOLFMIX and AI into separate issues/branches.

## 2. Universal preflight for every pass

1. Read `AGENTS.md`.
2. Read `docs/00_START_HERE.md`.
3. Read `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`.
4. Read docs 33–35.
5. Read the assigned issue in full.
6. Read the relevant accepted Studio, fixture, command/state and runtime documents named by that issue.
7. Fetch current `main`, open PRs and agent branches; do not trust the 2026-08-12 planning baseline after work begins.
8. Create/use only the prescribed branch from current `main` after its dependency is merged.
9. Post one concise scope statement and exact file reservation on the assigned issue before editing.
10. Inspect shared-file ownership and active diffs. If another lane owns a shared file, isolate the new module and defer wiring.
11. Use synthetic fixtures in Git. Private SoundSwitch/WOLFMIX projects, audio, paths, serials and source payloads remain outside Git.
12. Keep claims scoped to implemented and tested behavior.

Every agent should run the smallest focused suite during development and the full affected suite only at the merge gate. Do not burn work time repeatedly running unrelated packaging or physical tests.

## 3. Pass A — #58 / AL2-001

Branch:

```text
agent/autoloops-v2-model
```

### 3.1 Objective

Land the canonical Autoloop V2 source model, deterministic format-1 adapter, bounded compiled representation and semantic evaluator foundation **without changing existing live selection or UI behavior**.

### 3.2 Required reading

```text
native-core/include/emberlights/project.hpp
native-core/include/emberlights/project_io.hpp
native-core/src/project_io.cpp
native-core/include/emberlights/compiler.hpp
native-core/src/compiler.cpp
native-core/include/showcore/autoloop.hpp
native-core/src/autoloop.cpp
native-core/include/showcore/look.hpp
native-core/src/look.cpp
native-core/include/showcore/types.hpp
native-core/include/emberlights/studio_document.hpp
native-core/include/emberlights/soundswitch_migration_ir.hpp
native-core/tests/test_main.cpp
native-core/tests/test_studio_foundation.cpp
```

Also inspect PR #53 final public color/preview headers if merged. Pass A should reference shared asset IDs/interfaces but should not need to edit its Studio implementation files.

### 3.3 Exact new-file reservation

Reserve these names unless current repository convention requires a narrowly justified rename:

```text
native-core/include/emberlights/autoloop_source.hpp
native-core/src/autoloop_source.cpp
native-core/include/showcore/autoloop_program.hpp
native-core/src/autoloop_program.cpp
native-core/tests/test_autoloop_v2_model.cpp
spec/autoloops/autoloop-source-v1.md
```

Potential additive integration files—reserve individually before editing:

```text
native-core/include/emberlights/project.hpp
native-core/include/emberlights/project_io.hpp
native-core/src/project_io.cpp
native-core/include/emberlights/compiler.hpp
native-core/src/compiler.cpp
native-core/CMakeLists.txt
native-core/Makefile
.github/workflows/native-core.yml
```

Do not edit:

```text
native-core/include/emberlights/runner.hpp
native-core/src/runner.cpp
native-core/src/windows_app.cpp
native-core/include/emberlights/ui_command.hpp
native-core/include/emberlights/ui_state.hpp
any output adapter or fixture profile
PR #53 Studio color/preview implementation files
```

### 3.4 Required public concepts

Names may follow repository convention, but the separation is binding.

```cpp
using MusicalTick = std::int64_t;
inline constexpr MusicalTick kMusicalTicksPerQuarter = 960;

struct AutoloopAssetDefinition;
struct AutoloopPlacementDefinition;
struct AutoloopProgramDefinition;
struct AutoloopLaunchProfileDefinition;
struct AutoloopProvenanceDefinition;
```

Do not create a second time constant if #46 has already landed the shared one. Reuse it.

Minimum target model:

```cpp
enum class AutoloopTargetKind {
    Master,
    Group,
    Fixture,
    RoleSelector,
    // Future typed selector extension; no arbitrary expression.
};
```

Minimum event model:

```cpp
enum class AutoloopEventKind {
    LegacyLook,
    PropertyBlock,
    PropertyCurve,
    Palette,
    Position,
    Attribute,
    Movement,
    Effect,
};
```

Each event has integer start/end ticks, stable target/reference IDs, explicit property ownership and versioned bounded payload. Overlap behavior must be explicit. Vector order is not a mix rule.

### 3.5 Format-1 compatibility adapter

For every existing `AutoloopDefinition`:

- derive stable V2 asset/program/placement identities from existing ID;
- preserve name, bank, slot and repeat;
- convert float beats to integer ticks using one documented deterministic rule;
- preserve Cut/Linear semantics;
- preserve whole-Static-Look behavior;
- leave the unchanged file in its existing readable form unless an explicit migration/save action is taken;
- retain unknown records.

The compatibility adapter must be pure and deterministic. Do not mutate current projects on open merely to “upgrade” them.

### 3.6 Compiled representation

Required shape:

```text
fixed 64×32 placement lookup
used-program headers
compact target spans
compact event arena
compact curve-point arena
compact resolved-reference/generator data
```

The compiled package may allocate during construction/load. It becomes immutable before activation. Evaluation performs no allocation.

Do not allocate maximum rich storage per placement. Sharing one asset across placements shares one compiled program. Choose explicit aggregate capacities from measured stress fixtures and reject overflow before activation.

The compiler resolves:

- stable IDs;
- reusable references;
- target selectors against the immutable qualified venue snapshot;
- supported properties/capabilities;
- color realization where the accepted architecture places it;
- event ordering/ownership;
- generator payload versions;
- diagnostics and deterministic digest.

No private paths, vendor records, migration decoder objects or mutable project pointers enter `showcore`.

### 3.7 Legacy equivalence suite

Create golden traces for:

- Cut at exact boundary;
- Linear midpoint and endpoint;
- multiple same/adjacent boundaries;
- wraparound and multiple cycles;
- Once completion and clear;
- Infinite cycle count;
- TrackDuration active/stop behavior as currently implemented;
- negative/rewound beat input;
- interruption by higher layers;
- every current compiled pilot Autoloop or a representative deterministic digest corpus.

Compare resolved semantic properties and final DMX frame bytes where fixture/profile compilation is involved.

### 3.8 Focused build target

Prefer one focused executable:

```text
autoloop_v2_model_tests
```

Development loop:

```bash
cmake -S native-core -B native-core/build -DCMAKE_BUILD_TYPE=Release
cmake --build native-core/build --target autoloop_v2_model_tests -j2
ctest --test-dir native-core/build -R autoloop_v2_model_tests --output-on-failure
```

Then affected suites:

```bash
ctest --test-dir native-core/build \
  -R "autoloop_v2_model_tests|core_tests|studio_foundation_tests|static_look_authoring_tests" \
  --output-on-failure
```

At merge gate:

```bash
cmake --build native-core/build -j2
ctest --test-dir native-core/build --output-on-failure
make -C native-core test
make -C native-core smoke
```

Run warnings-as-errors and sanitizer jobs through existing CI. No physical DMX test is required because Pass A must not alter live wiring/output.

### 3.9 Pass A completion evidence

The PR and issue comment must include:

- exact changed/reserved files;
- model/persistence/compiled version numbers;
- format-1 and unknown-record proof;
- frame-equivalence results;
- deterministic serialization/digest result;
- capacity and memory rationale;
- no-allocation test result;
- full affected suite result;
- explicit statement that Runner selection/UI/output behavior is unchanged;
- remaining dependency: #59 runtime director.

### 3.10 Pass A stop condition

Stop when the source model, compatibility adapter, compiled arena/evaluator, specifications and tests pass. Do not begin runtime selection, UI, content packs, AutoScript or decoder research.

## 4. Pass B — #59 / AL2-002

Branch:

```text
agent/autoloops-v2-runtime
```

Base: latest `main` after #58 merges.

### 4.1 Objective

Build one deterministic Runner-owned Autoloop director and complete SoundSwitch performance semantics without changing Studio persistence, vendor decoders, fixture truth or output adapters.

### 4.2 Required reading

```text
all public contracts from #58
native-core/include/emberlights/runner.hpp
native-core/src/runner.cpp
native-core/include/emberlights/ui_command.hpp
native-core/src/ui_command.cpp
native-core/include/emberlights/ui_state.hpp
native-core/src/ui_state.cpp
native-core/include/showcore/layer_resolver.hpp
native-core/src/layer_resolver.cpp
native-core/include/showcore/sync_manager.hpp
native-core/src/sync_manager.cpp
native-core/include/showcore/midi.hpp
native-core/src/midi.cpp
native-core/tests/test_main.cpp
native-core/tests/test_live_ui.cpp
```

Read issue #56’s resource/process contract and the accepted Static Look continuation tests. Inspect current #31 ownership before shared registry edits.

### 4.3 Exact new-file reservation

```text
native-core/include/showcore/autoloop_director.hpp
native-core/src/autoloop_director.cpp
native-core/tests/test_autoloop_v2_runtime.cpp
```

Potential additive shared integration files:

```text
native-core/include/emberlights/runner.hpp
native-core/src/runner.cpp
native-core/include/emberlights/ui_command.hpp
native-core/src/ui_command.cpp
native-core/include/emberlights/ui_state.hpp
native-core/src/ui_state.cpp
native-core/include/showcore/midi.hpp
native-core/src/midi.cpp
native-core/CMakeLists.txt
native-core/Makefile
.github/workflows/native-core.yml
```

Do not edit:

```text
project persistence/source-model files except a narrow compile ABI correction coordinated with #58
Studio authoring/preview implementation
SoundSwitch migration decoders
fixture profiles
output adapters/protocols
skins
```

Avoid `windows_app.cpp` until director/facade/state tests are complete. A separate narrow wiring commit may be added only after checking current shell ownership.

### 4.4 Director state machine

Use explicit sessions:

```text
AutonomousSession -> LayerId::Autonomous
ScriptedSession   -> LayerId::TrackScript
ManualSession     -> LayerId::ManualAutoloop
```

The director owns lifecycle and selection. The evaluator from #58 owns one program’s semantic output. Existing layer resolver owns cross-feature priority.

Required state includes:

```text
selected
queued/pending
active
source/reason
Overlay | Replace
repeat
phase/progress/cycles
active bank mask/exclusive bank
pending bank mask/exclusive bank
track-boundary capability/epoch
package generation
last command/transition result
```

### 4.5 Required behavior

#### Autonomous fallback

- start only when normalized transport says fallback is appropriate;
- select from populated, valid placements in enabled banks;
- deterministic policy and seed;
- cycle on natural program boundaries;
- queue exclusive-bank change until the current autonomous program ends;
- direct launch bypasses navigation/filter restrictions;
- no eligible loop yields explicit safe/idle state, not stale output.

#### Manual Overlay

- immediate launch by default;
- owns only the manual program’s properties on ManualAutoloop layer;
- underlying scripted/autonomous sessions continue their clocks;
- Once completes one musical program and reveals lower layers at current phase;
- Infinite/TrackDuration follow explicit lifecycle;
- clear affects only the manual session’s layer.

#### Replace

- separate typed command/profile from Overlay;
- suppresses defined lower automated ownership for its lifetime;
- never suppresses ManualOverride, Emergency or Safety;
- does not destructively clear underlying script state;
- exact property mask/resume semantics documented and A/B-observed before a SoundSwitch-exact claim.

#### Static Looks and overrides

A latched/toggled/held Static Look continues to cover active Autoloops through EventMoment. Releasing it reveals the current loop phase. Fixture/group ManualOverride behaves similarly for only owned properties. Release All clears ManualOverride, not Autoloops.

#### Repeat/track boundary

TrackDuration consumes a normalized track epoch/end event. If the active integration cannot provide one, expose degraded capability and do not guess from filename/BPM. Exact VirtualDJ/Serato track-boundary adapters may be separate integration work, but the director contract and tests belong here.

#### Transport

Define deterministic outcomes for play/pause, beat loss/prediction/hold/recovery, tap/manual BPM, seek/rewind/reverse/loop when available, and package hot activation. Do not add source-specific transport parsing to the director.

### 4.6 Command/state integration

Reuse current typed commands where sufficient. Add only semantic gaps such as explicit mode or launch quantization through #31 coordination.

UI state is observational. No UI timer computes progress, queued state or bank transition. MIDI/controller mappings resolve stable active-package targets before posting bounded commands.

### 4.7 Deterministic replay suite

Minimum scenarios:

```text
autonomous no-script cycle
sequential/deterministic shuffle
bank mask and pending exclusive boundary
manual direct launch outside mask
Overlay Once over autonomous
Overlay Once over scripted
Overlay Infinite and clear
TrackDuration with/without track epoch
Replace lifecycle
Static Look toggle/hold and release
fixture/group override and Release All
blackout/work light/emergency/safety
stale generation/invalid placement/queue full
package activation with retained/moved/deleted program
beat loss and recovery
manual BPM/tap
```

Each scenario records command/transport inputs, status snapshots, resolved semantic frames and deterministic digest.

### 4.8 Focused build target

```text
autoloop_v2_runtime_tests
```

Development:

```bash
cmake --build native-core/build --target autoloop_v2_runtime_tests -j2
ctest --test-dir native-core/build -R autoloop_v2_runtime_tests --output-on-failure
```

Affected suites:

```bash
ctest --test-dir native-core/build \
  -R "autoloop_v2_model_tests|autoloop_v2_runtime_tests|core_tests|live_ui_tests|static_look_authoring_tests" \
  --output-on-failure
```

Merge gate: full native suites, Make tests/smoke, sanitizer, Windows Release cross-build and installed startup smoke. Add #56 machine-readable dense-Autoloop performance snapshot. Physical DMX is not needed to prove selection semantics, but no support/gig claim is made without later installed/hardware qualification.

### 4.9 Pass B completion evidence

- state-machine diagram/contract;
- command/state additions and compatibility statement;
- deterministic replay matrix result;
- Static Look continuation proof;
- track-boundary supported/degraded behavior;
- zero-allocation result;
- CPU/RSS/jitter comparison to pre-pass baseline;
- full affected/Windows smoke result;
- unchanged output adapter/protocol statement;
- remaining dependency: #60 authoring/content/migration.

### 4.10 Pass B stop condition

Stop after director, Runner integration, command/state facade, deterministic replay and resource/fault evidence. Do not start Studio editors, default content, AutoScript, source decoding or skins.

## 5. Pass C — #60 / AL2-003

Branch:

```text
agent/autoloops-v2-studio-migration
```

Base: latest `main` after #58, #59 and PR #53 merge.

### 5.1 Objective

Deliver toolkit-neutral Autoloop authoring services, original EmberLights content, deterministic AutoScript, exact no-output preview and the defensible SoundSwitch Autoloop decoder/import path.

This may be one branch with several reviewable commits or split into two PRs under the same issue if source-decoder evidence would make the PR unmanageably broad:

```text
C1 authoring + content pack + deterministic generator
C2 SoundSwitch Autoloop evidence adapter + reviewed import
```

Do not split into competing architectures or alternate project models.

### 5.2 Required reading

```text
public contracts and tests from #58 and #59
native-core/include/emberlights/studio_document.hpp
native-core/src/studio_document.cpp
native-core/include/emberlights/studio_color.hpp
native-core/src/studio_color.cpp
native-core/include/emberlights/studio_preview.hpp
native-core/src/studio_preview.cpp
native-core/include/emberlights/soundswitch_import.hpp
native-core/src/soundswitch_import.cpp
native-core/include/emberlights/soundswitch_migration_ir.hpp
native-core/src/soundswitch_migration_ir.cpp
native-core/include/emberlights/soundswitch_v1.hpp
native-core/src/soundswitch_v1.cpp
native-core/tests/test_studio_foundation.cpp
native-core/tests/test_studio_authoring.cpp
existing migration schemas and research README files
```

Read issue #46’s current progress/comments and do not duplicate later Studio vNext assets or time contracts that have landed since planning.

### 5.3 Exact new-file reservation

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

Potential additive shared integration files:

```text
native-core/include/emberlights/studio_document.hpp
native-core/src/studio_document.cpp
native-core/include/emberlights/studio_preview.hpp
native-core/src/studio_preview.cpp
native-core/include/emberlights/soundswitch_migration_ir.hpp
native-core/src/soundswitch_migration_ir.cpp
native-core/include/emberlights/soundswitch_import.hpp
native-core/src/soundswitch_import.cpp
native-core/CMakeLists.txt
native-core/Makefile
.github/workflows/native-core.yml
installer/package manifest only after the content pack passes
```

Do not edit:

```text
runner.*
output adapters/protocols
fixture profile truth
shared command/state registries
skins or broad windows_app.cpp
current convert-v1 semantics/warnings except additive adapter entrypoint with regression proof
```

### 5.4 Authoring service results

All operations return typed results including expected/current generation, validation, affected dependencies, warnings and stable IDs.

Required transactions:

- create/edit/rename/delete;
- duplicate asset;
- place existing asset;
- assign/unassign placement;
- move/swap/reorder/next-open;
- edit metadata/bank organization;
- bulk target/palette/Position/Attribute/style changes;
- populate empty from pack;
- preview/reset pack-managed placements;
- generation proposal preview/commit;
- migration proposal review/resolve/commit;
- idempotent re-import update/keep-native/duplicate/merge outcomes.

No operation silently overwrites occupied placement or destroys dependent assets. Cancellation/stale generation leaves document unchanged.

### 5.5 Content pack

Create independently authored semantic source for at least 128 starter placements. Do not derive it from SoundSwitch default content or private migrated user loops.

Content pack tests must compile against representative synthetic rigs:

- RGB dimmer fixtures;
- RGBW and RGBWAUV fixtures;
- dimmerless additive fixtures with virtual-intensity realization;
- movers with pan/tilt/Position support;
- fixtures with gobo/prism/focus/zoom Attribute support;
- color-only partial rig;
- mixed unsupported/degraded capabilities;
- empty/invalid group targets;
- two-universe maximum-size representative rig.

Pack populate fills empties only. Reset shows the exact replacement/deletion plan and affects only recognized pack-managed placements unless user explicitly selects broader replacement. One reset is one Undo transaction.

### 5.6 Deterministic AutoScript

Implement a versioned generator with normalized inputs and explicit seed. Output ordinary editable source.

Golden tests:

- same version/input/seed = same source and compiled digest;
- parameter ordering does not change normalized output;
- different seed changes documented choices only;
- missing capability gives recorded substitution/omission;
- linked Position/Attribute dependencies remain intact;
- hazards never emitted unarmed;
- conservative strobe safety;
- cancellation/stale generation no mutation;
- preview/compile errors block commit;
- accepted proposal one Undo entry.

Generation runs off Runner and UI threads. A large generation request is bounded and cancellable.

### 5.7 SoundSwitch adapter

Use existing read-only source inspection, manifest and migration statuses. Do not reimplement hashing, path safety, source classification or report confidence.

Required adapter stages:

```text
version/completeness probe
controlled-delta decode
item/field evidence
candidate normalization
profile/group/asset reconciliation
reviewed destination proposal
one output-disabled commit
```

The adapter must fail closed on unsupported source versions. Exact/translated fields require source-version-specific evidence and synthetic fixtures. Unsupported bytes stay in the verified source bundle.

The current authorized backup and generated Ember project mismatch remains a blocker to treating them as one baseline. If a matching controlled-delta corpus is unavailable, report:

```text
soundswitch.autoloop_delta_corpus_unavailable
```

Continue native authoring/pack/generator work and preserve source; do not fill the gap with name heuristics.

Private source may be analyzed read-only in an authorized workspace. Git contains only synthetic fixtures and content-safe hashes/range summaries. No user labels or source paths need to enter tests.

### 5.8 Exact no-output preview

Extend `StudioPreviewService` to evaluate V2 compiled programs through the production compiler/resolver/renderer. Preview opens no Art-Net, sACN, USB, Control One or SoundSwitch Micro adapter.

Snapshot includes:

- document/package generation;
- program/placement identity;
- phase/tick/progress;
- resolved fixture/property ownership;
- exact/degraded/unsupported warnings;
- rendered DMX frame/digest;
- optional source/generation provenance summary safe for UI.

### 5.9 Focused build targets

```text
autoloop_v2_studio_tests
soundswitch_autoloop_adapter_tests
```

Development:

```bash
cmake --build native-core/build \
  --target autoloop_v2_studio_tests soundswitch_autoloop_adapter_tests -j2
ctest --test-dir native-core/build \
  -R "autoloop_v2_studio_tests|soundswitch_autoloop_adapter_tests" \
  --output-on-failure
```

Affected suites:

```bash
ctest --test-dir native-core/build \
  -R "autoloop_v2_model_tests|autoloop_v2_runtime_tests|autoloop_v2_studio_tests|soundswitch_autoloop_adapter_tests|studio_foundation_tests|studio_authoring_tests|static_look_authoring_tests|core_tests" \
  --output-on-failure
```

Merge gate: full native suites, Make tests/smoke, sanitizer, Windows Release build/startup smoke and deterministic packaging of the default pack. Private corpus tests are local evidence only and must be reproducible through content-safe manifests/delta reports without entering Git.

### 5.10 Pass C completion evidence

- typed authoring operations and transaction tests;
- pack ID/version/digest/license/provenance;
- pack compile/degradation matrix;
- generator ID/version/seed reproducibility;
- no-output preview exact-frame evidence;
- source-adapter supported version/field matrix;
- controlled-delta fixtures/report or explicit blocker;
- source immutability and opaque-retention proof;
- re-import/idempotency/conflict proof;
- full affected/Windows packaging result;
- explicit unimplemented migration fields and no parity overclaim.

### 5.11 Pass C stop condition

Stop when authoring services, starter pack, deterministic AutoScript, preview and evidence-supported SoundSwitch import are complete. Do not begin WOLFMIX or AI implementation.

## 6. Prohibited drift across all passes

Do not:

- copy SoundSwitch or WOLFMIX defaults, source, fixture database, visual assets or trade dress;
- use names, filenames, paths or slot counts as proof of timeline semantics;
- place private source/audio/library data in Git;
- replace the existing migration IR/status system;
- create a second Studio document service, musical time type, color model, preview engine or fixture capability registry;
- make Runner parse source files or open media;
- add Lua/JavaScript/expression VM to Runner;
- put AI/model calls in live playback;
- allocate during scheduler ticks;
- edit output protocols or fixture profiles as collateral work;
- silently overwrite placement/content during import or reset;
- treat build success as physical/gig/parity qualification;
- rewrite continuity documents wholesale after each pass.

## 7. Continuity protocol

After each merged pass:

1. update its issue checklist/comment with exact evidence;
2. add one dated section to `docs/35_AUTOLOOPS_V2_CONTINUITY_CHECKPOINT.md`;
3. update `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` only for accepted implemented decisions and only after checking active ownership;
4. update `docs/06_PRIORITIZED_BACKLOG.md` status additively;
5. advance exact rows in `docs/13_SOUNDSWITCH_PARITY_LEDGER.md` only to the evidence tier achieved;
6. update the product ledger with measured behavior, not aspirations;
7. leave #57 as the aggregate source of truth;
8. identify the next issue/branch and shared-file risks.

Do not close #57 until all required SoundSwitch Autoloop parity rows are evidenced and qualification gates pass. #61 may remain open after #57 if explicitly categorized post-V1 interoperability rather than SoundSwitch parity.

## 8. Final V1 Autoloop release gate

Before describing Autoloops as SoundSwitch-parity/gig-qualified:

- all #57 SoundSwitch acceptance items implemented;
- format-1 and migrated-project recovery proven;
- installed Windows Runtime/Studio behavior proven;
- VirtualDJ same-PC and separate-PC timing proven;
- Control One mappings/feedback physically qualified where claimed;
- representative fixture color/movement/attribute output physically compared;
- track-boundary capability exact for TrackDuration claim;
- dense library/runtime CPU, RSS and jitter within #56 limits;
- 8-hour soak and fault/reconnect/sleep/resume completed;
- source adapter supported-version matrix and opaque gaps documented;
- no open blocker on reset/import data loss;
- parity ledger updated with evidence links.

Planning completeness is not release completeness.
