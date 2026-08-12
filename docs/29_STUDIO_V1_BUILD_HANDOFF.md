# Studio V1 Foundation — Work-Agent Handoff

Status: execution packet for **issue #46**, subordinate to `28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md` and current direct owner instructions.

This packet is intentionally narrower than the full Studio program. It establishes durable document and migration contracts without colliding with the active Live/output, fixture-profile, or UI-platform lanes.

## 1. Build objective

Implement the first production-quality Studio foundation that:

- wraps the current format-1 `ProjectDocument` in an explicit generation-checked Studio document service;
- preserves existing project load/save, checksum, unknown records, Undo/Redo, and durable history behavior;
- introduces a versioned SoundSwitch source-corpus manifest and migration intermediate representation;
- represents exact, translated, approximate, opaque, unsupported, conflicted, missing, and rejected migration outcomes without inventing source semantics;
- produces deterministic, content-safe reports and synthetic regression fixtures;
- leaves current `convert-v1`, Runner, output, Live controls, and fixture profiles behaviorally unchanged.

This is **foundation work**, not a timeline visual rewrite and not a claim of broader SoundSwitch decoding.

## 2. Branch and issue protocol

After the planning documents are merged or explicitly accepted:

```text
branch: agent/studio-v1-foundation
base: latest main containing docs 28/29 and issue #46 continuity
issue: #46
```

Before editing:

1. Read `AGENTS.md`.
2. Read `docs/00_START_HERE.md`.
3. Read `docs/28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md`.
4. Read this handoff.
5. Read only the relevant current headers/sources listed below.
6. Rebase against current `main` and inspect concurrent changes.
7. Post one concise scope/file-reservation comment on issue #46.

Do not create an alternate architecture, another product ledger, or a replacement planning document.

## 3. Required current-code reading

### Project/document

- `native-core/include/emberlights/project.hpp`
- `native-core/include/emberlights/project_io.hpp`
- `native-core/src/project_io.cpp`
- `native-core/include/emberlights/project_edit_history.hpp`
- `native-core/src/project_edit_history.cpp`
- relevant project/IO/history tests in `native-core/tests/test_main.cpp`

### SoundSwitch migration

- `native-core/include/emberlights/soundswitch_import.hpp`
- `native-core/src/soundswitch_import.cpp`
- `native-core/include/emberlights/soundswitch_v1.hpp`
- `native-core/src/soundswitch_v1.cpp`
- `native-core/src/soundswitch_migrate.cpp`
- `docs/18_SOUNDSWITCH_MIGRATION.md`

### Build/test

- `native-core/CMakeLists.txt`
- `native-core/Makefile`
- `.github/workflows/native-core.yml`

### Shared facade — inspect, do not seize ownership

- `native-core/include/emberlights/ui_command.hpp`
- `native-core/include/emberlights/ui_state.hpp`
- issue #31 and its current branch/PR state

The first slice does not need to edit `windows_app.cpp`, `runner.cpp`, any output adapter, any fixture profile, or skin package.

## 4. File reservation

Reserve these exact new files before implementation:

```text
native-core/include/emberlights/studio_document.hpp
native-core/src/studio_document.cpp
native-core/include/emberlights/soundswitch_migration_ir.hpp
native-core/src/soundswitch_migration_ir.cpp
native-core/tests/test_studio_foundation.cpp
spec/migration/soundswitch-corpus-manifest-v1.schema.json
spec/migration/soundswitch-migration-report-v1.schema.json
```

Shared files that may require small additive edits:

```text
native-core/CMakeLists.txt
native-core/Makefile
native-core/src/soundswitch_migrate.cpp       # only if adding the bounded report command
.github/workflows/native-core.yml             # only to register the new focused test target
README/docs/parity/decision continuity files  # only after behavior exists
```

Do not edit shared files until the reservation comment confirms no conflict. If #31, Live, or fixture work currently owns a shared file, isolate the Studio implementation and defer wiring.

## 5. Required types and contracts

Names may be refined for repository conventions, but semantics may not be weakened.

### 5.1 Studio document service

Provide a Studio-only service around the current project and edit history.

Minimum public concepts:

```cpp
using StudioDocumentGeneration = std::uint64_t;

enum class StudioMutationResult {
    Applied,
    NoChange,
    StaleGeneration,
    ValidationFailed,
    LimitExceeded,
    InvalidCandidate,
    InternalError
};

struct StudioDocumentSnapshot {
    ProjectDocument document;
    StudioDocumentGeneration generation;
    bool dirty;
    bool can_undo;
    bool can_redo;
};

struct StudioMutationOutcome {
    StudioMutationResult result;
    StudioDocumentGeneration generation;
    ProjectValidation validation;
    std::string message;
};
```

Required behavior:

- current project state has one monotonically increasing generation;
- New/Open/Restore boundaries replace the document and clear in-session history;
- a candidate mutation includes the expected generation;
- stale candidates are rejected without mutation;
- candidate validation occurs before mutation;
- a successful changed candidate records exactly one pre-change history entry and increments generation once;
- an identical deterministic serialized candidate returns `NoChange` and records no Undo entry;
- Undo and Redo increment generation and preserve the current existing history semantics;
- the service tracks whether current deterministic serialized content matches the last durable-save baseline;
- save acknowledgement changes the durable baseline only after `save_project_atomic` succeeds;
- the service contains no Runner pointer, output logic, audio access, or UI toolkit type.

Do not replace `ProjectEditHistory` in this slice. Compose it and preserve its 100-entry bound.

### 5.2 Source-corpus manifest

Reuse existing `SoundSwitchInspection` and `SoundSwitchArtifactKind` rather than creating a second file classifier.

Minimum concepts:

```cpp
enum class MigrationSourceRole {
    Required,
    Conditional,
    Optional
};

enum class MigrationSourceAvailability {
    PresentVerified,
    Missing,
    Unreadable,
    RejectedUnsafe
};

struct MigrationSourceArtifact {
    std::string artifact_id;
    std::string relative_path;
    SoundSwitchArtifactKind kind;
    std::uint64_t size;
    std::string sha256;
    MigrationSourceRole role;
    MigrationSourceAvailability availability;
};

struct SoundSwitchCorpusManifest {
    std::uint32_t format_version{1};
    std::string bundle_id;
    std::string source_version;
    std::vector<MigrationSourceArtifact> artifacts;
    std::vector<std::string> missing_dependency_codes;
};
```

Rules:

- artifact IDs are deterministic from source identity; never random per scan;
- portable reports contain bundle-relative paths, not mandatory absolute private paths;
- the manifest records the project-container evidence independently from copied audio and DJ-library evidence;
- absence of audio/library evidence is reported rather than treated as “no scripted tracks”;
- no raw payload bytes appear in JSON reports or tests;
- source root and local discovery paths may remain process-local and must not be required in the portable manifest;
- serializers emit stable field and collection order.

### 5.3 Migration IR/report

Minimum status enum:

```cpp
enum class MigrationItemStatus {
    Exact,
    DeterministicallyTranslated,
    Approximated,
    PreservedOpaque,
    Unsupported,
    Conflicted,
    MissingDependency,
    RejectedUnsafe
};
```

Minimum item evidence:

```cpp
struct MigrationEvidenceRef {
    std::string artifact_id;
    std::uint64_t offset;
    std::uint64_t length;
    bool has_byte_range;
    std::string decoder_id;
    std::string decoder_version;
};

struct MigrationItem {
    std::string item_id;
    std::string item_kind;
    MigrationItemStatus status;
    std::string source_label;
    std::string destination_ref;
    std::string rule_id;
    std::vector<MigrationEvidenceRef> evidence;
    std::vector<std::string> warnings;
    std::vector<std::string> blockers;
};

struct SoundSwitchMigrationReport {
    std::uint32_t format_version{1};
    std::string source_bundle_id;
    std::string source_version;
    std::vector<MigrationItem> items;
    deterministic aggregate counts;
};
```

Rules:

- status is object/field specific, not one project-wide confidence percentage;
- item IDs and report ordering are deterministic;
- `Approximated` cannot be serialized as exact/imported without its warning;
- `PreservedOpaque` requires an artifact identity;
- `MissingDependency` identifies a stable missing-dependency code;
- `Conflicted` has no selected destination until an explicit resolution exists;
- source labels are display-only and cannot establish semantics;
- IR/report has no `ProjectDocument*`, Runner index, raw DMX, or UI callback.

### 5.4 JSON schemas

Create strict draft-2020-12-compatible schemas for the manifest and report.

Require:

- fixed format names/versions;
- bounded string lengths and array sizes matching implementation limits;
- enumerated statuses/roles/availability;
- SHA-256 lowercase/uppercase policy stated and tested;
- `additionalProperties: false` for frozen v1 records;
- no absolute-path requirement;
- no arbitrary payload/blob field;
- deterministic examples generated synthetically.

## 6. Source-completeness evaluation

The first implementation may use a conservative rules table over existing artifact kinds. It must not claim that a source class is absent just because it is outside the selected `.ssproj` root.

At minimum report these stable dependency codes when unresolved:

```text
soundswitch.project_manifest_missing
soundswitch.venue_database_missing
soundswitch.autoloop_database_missing
soundswitch.track_map_unavailable
soundswitch.lighting_files_unavailable
soundswitch.scripted_audio_unavailable
soundswitch.dj_library_identity_unavailable
soundswitch.source_version_unverified
authorized_soundswitch_corpus_unavailable
```

The exact required/conditional role depends on requested migration scope:

- project-only migration can proceed without audio but must label scripted-show coverage incomplete;
- exact scripted-track migration requires compatible lighting-file, audio, and association evidence;
- Control One import/export is never treated as a complete scripted-track backup;
- a current `convert-v1` run retains its existing explicit approximation warnings.

## 7. Optional CLI integration

Only after the library contracts and tests pass, add one bounded command to `emberlights_migrate`:

```text
corpus-manifest <project-root> --report <path>
```

It may inventory the selected project root and report external dependency classes as unavailable. Do not add recursive scanning of arbitrary music libraries in this slice.

Requirements:

- same read-only inspection and byte limits as existing migration tooling;
- atomic report write;
- deterministic output;
- no raw source bytes, absolute path requirement, audio modification, or project conversion;
- help text explicitly says the command evaluates evidence availability, not semantic import completeness.

If CLI wiring would conflict with another branch, leave the serializer/library fully tested and defer the command.

## 8. Synthetic test corpus

Private SoundSwitch projects, audio, and DJ databases do not enter Git. Build synthetic directories/files at test runtime.

Required tests:

### Studio document

- initial generation/snapshot;
- applied mutation increments once and records one Undo entry;
- identical candidate returns `NoChange`;
- stale expected generation is rejected;
- invalid project candidate does not mutate;
- Undo/Redo increment generation and restore exact deterministic serialization;
- New/Open/Restore boundary clears history;
- failed save does not mark clean;
- successful save acknowledgement marks clean;
- mutation after save marks dirty;
- existing unknown record survives service mutation and save/reparse;
- 100-entry edit history remains bounded.

### Source manifest

- deterministic artifact IDs/order/serialization;
- project-only corpus reports external scripted-track dependencies as unavailable rather than absent;
- missing manifest/venue/Autoloop cases emit stable codes;
- oversized, symlink, changing, or unreadable source remains rejected through existing inspection behavior;
- manifest contains no payload bytes or mandatory absolute path;
- schema validation fixture is syntactically valid JSON and representative.

### Migration IR/report

- every status round-trips through serializer/name parser where applicable;
- approximate item requires warning;
- opaque item requires artifact evidence;
- conflict has no silent destination;
- missing dependency carries stable blocker;
- deterministic counts/order/output;
- source labels do not alter IDs or status;
- no report field can contain an arbitrary binary payload.

### Current migration regression

- existing SoundSwitch inspection/comparison/bundle tests remain unchanged and pass;
- current safe V1 converter still emits output-disabled approximation warnings and identical expected project semantics;
- no fixture address/profile, Look, Autoloop, or Runner behavior changes.

## 9. Build-system integration

Prefer a focused executable rather than adding more unrelated cases to `core_tests`:

```text
studio_foundation_tests
```

Add it to CMake, Makefile `all`/`test`, CTest, and CI focused/native suites. Keep warnings fatal.

During development run:

```bash
cmake -S native-core -B native-core/build -DCMAKE_BUILD_TYPE=Release
cmake --build native-core/build --target studio_foundation_tests -j2
ctest --test-dir native-core/build -R studio_foundation_tests --output-on-failure
```

Then affected migration and core tests:

```bash
ctest --test-dir native-core/build -R "studio_foundation_tests|core_tests|migration_help" --output-on-failure
```

At the merge gate only:

```bash
cmake --build native-core/build -j2
ctest --test-dir native-core/build --output-on-failure
make -C native-core test
make -C native-core smoke
```

Run Windows cross-link/installed GUI smoke through the existing release workflow after the source branch is stable. This docs/foundation slice does not require physical DMX, long soak, or fixture retesting because it must not change those mechanics.

## 10. Prohibited drift

Do not:

- edit SoundSwitch Micro, Control One, DMX USB, Art-Net, sACN, OS2L, sync, scheduler, safety, or Runner behavior;
- change existing fixture profiles, addresses, channel maps, qualification state, or the generated pilot project;
- change the project format version merely to add the service wrapper or nonpersisted IR;
- expand `UiCommandId` or `UiInvocationResult` without coordinating with issue #31 ownership;
- build timeline/waveform visuals, choose the production toolkit, or change skins in this slice;
- scan or copy the user's entire music library;
- decode undocumented binary fields by pattern speculation;
- put private source paths, source bytes, audio, serials, or personal library data in Git/tests/reports;
- claim exact migration, Studio parity, or V1 completion from this foundation.

## 11. Completion evidence

The PR must contain:

- issue #46 link and exact scope;
- reserved/changed files;
- documented public contracts and schema versions;
- focused test commands and results;
- full affected-suite result;
- proof current converter/output/runtime behavior is unchanged;
- source-data/privacy statement;
- exact remaining risks;
- next dependency: media/track identity or persisted vNext model—not broad UI polish.

Update continuity additively:

- issue #46 checklist/comment;
- `08_DECISIONS_AND_OPEN_QUESTIONS.md` only for accepted implemented decisions;
- `06_PRIORITIZED_BACKLOG.md` status only after merged evidence;
- `13_SOUNDSWITCH_PARITY_LEDGER.md` only when a listed capability materially advances;
- no completion claim based solely on new types or docs.

## 12. Stop condition

Stop the first work slice when the document service, source manifest, migration IR/report, schemas, deterministic synthetic tests, and build integration are complete and green. Do not consume remaining work-agent budget starting waveform, timeline, or speculative decoders without a fresh bounded handoff.
