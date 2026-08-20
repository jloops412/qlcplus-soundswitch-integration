# EmberLights UI Layout Examples

Status: planning fixtures for issue #37 toolkit spikes, issue #32 runtime/schema validation, and bundled-skin implementation. They are not finished visual assets or accepted skin packages.

Examples:

- `safe-live.layout.json`
- `default-live-standard.layout.json`
- `default-studio-standard.layout.json`
- `default-studio-fixtures-looks-standard.layout.json` — binding first product-shaped replacement-shell slice from D-091
- `reference-live-standard.layout.json`
- `reference-studio-standard.layout.json`

## Purpose

The examples force every toolkit/runtime candidate to exercise the same product-shaped information architecture instead of comparing unrelated framework demos.

They demonstrate:

- shared command/state bindings;
- Safe/Default/Reference separation;
- responsive containers;
- ordinary controls;
- toolkit-neutral native complex components;
- pinned Live health/safety;
- Autoloop/Static Look/performance controls;
- Studio library/timeline/waveform/Inspector/patch/mapping/migration structure;
- custom slots without arbitrary code;
- no domain behavior inside a skin.

## Authority

The examples are subordinate to:

- `../schema/layout.schema.json`;
- `../schema/skin-manifest.schema.json`;
- `../command-state-skin-contract-v0.md`;
- `../native-component-contracts-v0.md`;
- `../emberskin-package-and-safety-limits-v0.md`;
- `../ui-course-correction-v1.json`;
- `../../docs/44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md`;
- `../../docs/24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`;
- `../../docs/20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`.

Command/state names are planning references to the seeds in `../registry/`. Issue #31 owns final registry reconciliation. Issue #38 may refine Static Look Toggle/Hold and core-recovery service semantics first.

## Validation sequence

1. Validate JSON syntax.
2. Validate against `layout.schema.json`.
3. Resolve command/state/component references against the accepted/generated registries.
4. Validate package limits, focus graph, mandatory-control reachability, variant constraints, accessibility names, and safety restrictions.
5. Compile into the immutable view graph.
6. Render in the toolkit/component harness.
7. Test accepted/rejected commands and state updates.
8. Measure layout, DPI, memory, CPU, repaint, input, and scheduler effect.

Schema validity alone does not prove a usable or safe Live surface.

## Known planning placeholders

- localization keys are not yet backed by a complete locale catalog;
- some command/state IDs are planning-seed IDs pending #31;
- native component properties require final per-component schemas;
- exact SoundSwitch Reference dimensions/tokens wait for #30 Tier A evidence;
- precise compact/wide/touch variants will be separate files or generated derivatives;
- Studio timeline/waveform capabilities must reflect actual engine milestones rather than fake controls;
- visual token values live in theme/design-system artifacts, not these layouts.

## Implementation rule

Do not translate these JSON files directly into hard-coded toolkit pages and stop there. The objective is a reusable parser/validator/view graph/component adapter path that can load both bundled skins and future validated overlays.

## Safe example rule

`safe-live.layout.json` is a semantic planning example. The production Safe surface may be compiled/trusted application resources or a tiny hard-coded renderer, but it must preserve the same minimum information/commands and remain independent of optional skin package failure.

## Reference example rule

The Reference examples preserve familiar SoundSwitch landmarks, not proprietary pixels. Original EmberLights assets, evidence-tagged measurements, and the deviation ledger remain mandatory.
