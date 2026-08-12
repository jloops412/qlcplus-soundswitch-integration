# Canonical UI Registry Source — Bounded Decision Spike

Status: **required first design spike for issue #64**. This is a comparison and provisional recommendation, not an accepted replacement for the work agent’s measured decision.

Baseline:

- C++ application/Runner;
- current native command/state arrays in headers;
- current minified JSON seed registries and JSON Schemas;
- future visual Designer, package validator, migration adapters, Explorer catalogs, controller mappings, and Ember Actions require machine-readable metadata;
- Runner/scheduler must not parse general registry source or perform dynamic string resolution;
- generated artifacts should be committed and CI-checked so ordinary builds and releases do not depend on a generator at runtime.

## 1. Required output set

One canonical source must deterministically produce or validate:

```text
native compact IDs/enums/types/lookups
native facade adapters where appropriate
JSON command/state/component/capability/value/result catalogs
JSON Schema references/value schemas
Explorer/designer metadata
compatibility/deprecation/alias maps
registry version/generation/digest
human reference docs
first-party cross-reference manifest
compatibility diff fixtures
```

## 2. Candidates

### A. Canonical schema-validated JSON fragments

Proposed shape:

```text
spec/ui/registry/source/
  registry-set.json
  values/*.json
  results/*.json
  commands/<domain>.json
  states/<domain>.json
  components/<domain>.json
  capabilities/<domain>.json
  interactions/*.json
  theme-tokens/*.json
```

A host-side generator validates and aggregates fragments, then emits committed native and tooling artifacts.

#### Strengths

- directly machine-readable by validator, Designer, migration, compatibility, and documentation tooling;
- reuses existing seed/schema investment;
- supports domain ownership and smaller feature-agent diffs;
- easier cross-reference and semantic-diff tooling;
- canonical serialization/digest can be specified clearly;
- source can remain toolkit- and native-language-neutral;
- no general parser is required in the installed Runner when generated outputs are committed.

#### Risks

- strict JSON is less pleasant for comments and long hand-authored definitions;
- fragment ordering, references, and canonicalization require a disciplined generator;
- duplicate/stale generated artifacts become a CI concern;
- a host generator/toolchain is required for registry changes.

#### Required mitigations

- use explicit `description`/`notes` fields rather than comments;
- one deterministic path/order/ID policy;
- schema validation before generation;
- atomic generator output and clean-diff check;
- generated files have headers stating source/digest/tool version;
- ordinary application build consumes generated native artifacts only;
- release bundles contain only the bounded catalogs needed by each process.

### B. Canonical C++ constexpr/native definitions

Native structures become authoritative and tooling JSON/Schemas/docs are generated from C++ or a parallel metadata macro/DSL.

#### Strengths

- compile-time type checking for native code;
- no separate native generator output required;
- direct compact representation.

#### Risks

- C++ becomes the de facto public authoring/schema language;
- generating rich JSON Schema, Designer pickers, compatibility maps, docs, and migration metadata is more complex;
- native build structure can leak into toolkit-neutral contracts;
- feature agents may add behavior without complete non-native metadata;
- introspecting arbitrary C++ definitions requires macros/codegen conventions or a second source of truth;
- external tooling cannot validate source without compiling project-specific code.

### C. YAML or TOML canonical source

Human-oriented fragments generate JSON and native artifacts.

#### Strengths

- comments and multi-line text are easier;
- potentially cleaner hand-authored diffs.

#### Risks

- adds a new parser/dependency and canonicalization ambiguity;
- YAML implicit typing/anchors/features increase security and deterministic-normalization risk unless heavily restricted;
- JSON Schema/tooling still requires conversion;
- another format adds little user value because the visual Designer and generated Explorer are the intended authoring UX.

### D. Custom registry DSL

A purpose-built text grammar generates all outputs.

#### Strengths

- could optimize ergonomics and enforce domain-specific constraints.

#### Risks

- large parser/tooling/IDE/documentation burden;
- new compatibility surface before the registry itself is stable;
- duplicates schema and Action Script work;
- slows the first vertical slice and increases maintenance.

## 3. Provisional recommendation

Use **Candidate A: schema-validated canonical JSON fragments**, with these boundaries:

1. Fragments under `spec/ui/registry/source/` are authoritative source.
2. A deterministic host-side generator emits committed native compact artifacts and aggregated JSON/tooling catalogs.
3. The application and Runner build from generated native artifacts; they do not parse source fragments.
4. Studio/Designer/validator may use a bounded generated catalog, not the raw full source when a smaller process-specific catalog is sufficient.
5. Existing seed files become migration inputs/baselines, not a second authority.
6. A registry source change is incomplete until clean regeneration, semantic diff, first-party cross-reference, and narrow tests pass.
7. The generator implementation language is chosen separately based on portability and dependency evidence; source format does not force Python at runtime.

This recommendation is based on current repository structure and product requirements. It becomes accepted only after the spike below proves it.

## 4. Required comparison spike

Implement the same small registry slice in at least Candidate A and one native-first control candidate.

Minimum slice:

```text
commands:
  show.start
  output.blackout.set
  staticLook.hold
  autoloop.launch
  group.override.property.set

states:
  runner.state
  output.blackout
  staticLook.active.id
  autoloop.active.progress
  output.micro.status

shared:
  invocation results
  interaction kinds
  realtime/safety/persistence classes
  stable ID and value schemas
```

Each candidate must produce:

- compile-ready native definitions/lookups;
- valid deterministic JSON catalog;
- one generated human reference page;
- one compatible-additive and one breaking semantic diff;
- one first-party binding/action fixture validation;
- stable digest after repeated generation;
- exact stale-generated-output failure.

## 5. Decision criteria

Score each candidate with evidence:

| Criterion | Weight |
| --- | ---: |
| One-source-of-truth enforceability | 20 |
| Rich schema/Designer/migration metadata | 15 |
| Deterministic generation and semantic diff | 15 |
| Native compile/runtime efficiency | 15 |
| Feature-agent editing and review clarity | 10 |
| Windows/Linux CI and local portability | 10 |
| Generator/bootstrap simplicity | 5 |
| Backward/native enum-ID preservation | 5 |
| Security/canonicalization simplicity | 5 |

Automatic fail conditions:

- requires general source parsing on the scheduler;
- produces two manually maintained authorities;
- cannot generate/validate non-native tooling metadata;
- cannot classify semantic compatibility;
- leaks toolkit-specific layout concepts into domain registries;
- requires a browser/runtime scripting engine;
- cannot preserve current tested IDs during migration.

## 6. Generator contract if Candidate A wins

### Inputs

- bounded registry source fragments;
- JSON Schemas;
- prior accepted baseline registry;
- first-party artifact manifests.

### Outputs

Suggested categories, exact names decided by #64:

```text
native-core/include/emberlights/generated/*
spec/ui/registry/generated/*
docs/generated/ui-registry/*
spec/ui/registry/baselines/*
```

Every generated artifact records:

- registry generation;
- canonical source digest;
- generator version;
- source baseline;
- do-not-edit marker where applicable.

### Determinism

- canonical UTF-8 and newline policy;
- sorted paths/IDs/keys where semantic ordering is not declared;
- no timestamps inside digest-bearing output;
- stable numeric/string/Unicode representation;
- atomic temporary output then replace;
- repeated generation produces byte-identical output;
- CI fails any generated diff.

### Runtime split

Generate process-appropriate views:

- compact native scheduler/control IDs and metadata;
- Studio/Designer Explorer catalog;
- package/action/mapping validator catalog;
- owner-facing docs;
- compatibility baseline/diff.

Do not make every process load every field.

## 7. Decision record requirement

Before broad registry migration, #64 records:

```text
selected candidate and score
rejected candidates and evidence
canonical paths and fragment rules
generator implementation/runtime dependencies
generated output ownership
native enum/ID preservation strategy
seed migration/deprecation plan
version/digest/diff policy
ordinary and release gate commands
```

The accepted decision updates the registry README and, if architecture-significant beyond ADR 0006, adds a focused ADR rather than burying the choice in code comments.
