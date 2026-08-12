# ADR 0007 — Canonical UI Registry Source and Generated Native ABI

- **Status:** Accepted for the SKIN2-001 first registry spine
- **Date:** 2026-08-12
- **Issue:** #64
- **Supersedes as authority:** command/state planning seeds (retained as migration evidence)

## Context

The current product has 29 native `UiCommandId` definitions and 39 native Live
state definitions, plus older JSON planning seeds. Future validation, Explorer,
Designer, action, mapping, and migration tooling require richer machine-readable
metadata, while Runner must compile compact native definitions and must not parse
general registry source.

The required spike compared schema-governed JSON fragments with a native-first
C++ control and scored the other documented candidates. Each score is a weighted
percentage using the criteria in
`spec/ui/registry/CANONICAL_REGISTRY_SOURCE_SPIKE.md`.

| Candidate | One source | Rich tooling metadata | Determinism/diff | Native efficiency | Editing/review | Portability | Bootstrap | ID preservation | Canonicalization/security | Weighted score |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| JSON fragments | 5/5 | 5/5 | 5/5 | 5/5 | 4/5 | 5/5 | 4/5 | 5/5 | 5/5 | **97/100** |
| Native-first C++ | 3/5 | 2/5 | 3/5 | 5/5 | 3/5 | 4/5 | 4/5 | 5/5 | 5/5 | **70/100** |
| Restricted YAML/TOML | 4/5 | 4/5 | 3/5 | 5/5 | 5/5 | 3/5 | 2/5 | 5/5 | 2/5 | **77/100** |
| Custom DSL | 4/5 | 3/5 | 3/5 | 5/5 | 2/5 | 2/5 | 1/5 | 5/5 | 3/5 | **66/100** |

### Measured evidence

- JSON fragments generated a byte-stable native header, aggregate catalog,
  bounded cross-reference report, and human reference. Repeated generation
  produced the same SHA-256 digest and `check` detected a deliberately stale
  output.
- The generated native header compiles with C++20 warnings-as-errors and the
  existing facade/state test remains green.
- `ui_registry_native_control_spike.cpp` compiled and emitted deterministic JSON
  for the same five-command/five-state representative slice. It also demonstrated
  the native-first cost: C++ needs an additional handwritten extractor and still
  lacks the rich non-native metadata unless C++ becomes a custom metadata DSL.
- The standard-library-only Python generator runs on the available Windows/Linux
  build host model and is absent from the installed Runner dependency graph.
- Additive, deprecated exact-replacement, and breaking fixtures produced exact
  compatibility classifications.

No automatic-fail condition occurred. Neither candidate parsed source on the
scheduler, changed toolkit contracts, required a browser/script runtime in the
product, or changed existing native IDs.

## Decision

1. Canonical registry definitions are schema-governed JSON fragments under
   `spec/ui/registry/source/`.
2. `native-core/tools/generate_ui_registry.py` is the deterministic host-side
   generator. It uses only the Python standard library and never ships as a
   Runner runtime dependency.
3. Generated native definitions live under
   `native-core/include/emberlights/generated/`; generated tooling catalogs and
   reports live under `spec/ui/registry/generated/`; generated human reference
   lives under `docs/generated/ui-registry/`.
4. Generated artifacts are committed. `make -C native-core
   surface-contract-gate` fails stale output and validates the first spine.
5. Existing native enum ordinals are explicit canonical fields and are checked
   against exact 0–28 command and 0–38 state baselines. The generated header
   replaces only handwritten declarations; `UiCommandFacade`, `LiveCoreUiState`,
   Runner delivery, and domain behavior remain unchanged.
6. The current ten native invocation results retain exact names/ordinals.
   `missingTarget`, `cancelled`, and `startedAsync` are explicit `reserved`
   catalog entries only. They do not silently alias `NotFound` and do not become
   native outcomes until #31/#64 has an authoritative service contract.
7. Public realtime classes use `viewLocal`, `studioMutation`, `runnerCommand`,
   `runnerPriority`, `utilityAsync`, and `blockingForbiddenLive`. Generated native
   views may remain smaller. State definitions separately record canonical update
   class, publication ceiling, snapshot group, and compact native update class.
8. `implemented`, `bridged`, `planned`, and `deprecated` are the lifecycle
   vocabulary. This first integrated generation includes only the 29/39 current
   native definitions; older seed-only entries do not masquerade as callable or
   observable behavior.
9. `command-registry-seed-v0.json` and `state-registry-seed-v0.json` remain
   read-only reconciliation/migration evidence. They are not generated output or
   a second accepted authority.
10. Issue #59 owns exact Autoloops V2 selected/queued/source/mode/policy/result
    state names. Its semantic slots are recorded as owner reservations and are not
    published by this slice.
11. Registry generation and source digest are separate from application/schema
    versions. Compatibility is determined by semantic diff, not version alone.
    The first generated catalog is frozen at
    `spec/ui/registry/baselines/ui-registry-v1.json`.

## Compatibility rules in the first gate

- A new definition is `compatibleAdditive`.
- Label/description/status-only edits that preserve semantic metadata are
  `changedCompatible`.
- Explicit deprecation with a valid replacement is
  `compatibleWithDeprecations` and exposes replacement availability.
- Removal or changes to interaction, arguments/value type, realtime, safety,
  persistence, authority, or other semantic metadata are `breaking` and require
  manual action.
- Duplicate IDs/ordinals, replacement cycles, unknown feedback/reference IDs,
  unbounded high-rate states, or stale generated files fail the gate.

## Intentional first-slice limits

- Component, capability, theme-token, reusable-value, Safe/Default/Reference
  package, and full controller-profile catalogs remain follow-on expansion of the
  same source/generator. No empty implementation claim is made for them here.
- Cross-reference validation is bounded to the representative native registry
  fixture. Existing bundled layout examples and the inactive Control One
  capture-required profile contain planned #31/#32/#59 references; this slice
  reports that deferral rather than publishing fake commands to make them pass.
- No CI workflow is changed in this lane; the reusable Make gate is ready for the
  owning integration lane.
