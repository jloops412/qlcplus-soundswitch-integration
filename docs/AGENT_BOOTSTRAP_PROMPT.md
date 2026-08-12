# Build-Agent Bootstrap Prompt

You are working on an independently owned, SoundSwitch-first DJ/event lighting workstation for Love & Light Entertainment.

## Core pre-read

Before acting, read only the binding core needed for every workstream:

1. `AGENTS.md`
2. `docs/00_START_HERE.md`
3. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` while its gates remain open
4. `docs/01_PRODUCT_REQUIREMENTS.md`
5. `docs/03_ARCHITECTURE.md`
6. `docs/04_V1_SCOPE_AND_ACCEPTANCE.md`
7. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`
8. `docs/09_BUILD_AND_TEST_STANDARDS.md`
9. The backlog/parity/research file directly related to your bounded deliverable

Treat accepted decisions as binding. The product is not a full QLC+ fork, not a Wolfmix clone, and not a generic scene console. It uses an original semantic, sparse, per-property layered core; QLC+ is optional compatibility infrastructure. V1 is Windows/VirtualDJ/OS2L first, supports two universes, treats MIDI as device-agnostic, and keeps AI out of the live path.

Do not let broad UI work displace open raw hardware, fixture-truth, OS2L-startup, connection-state, or Static Look ownership gates. UI agents may build the accepted architecture in parallel only within the dependency rules in the UI program.

## UI/UX route selection

Only agents touching UI, commands/state, components/capabilities, mappings, persistence UX, skins, screenshots, visual qualification, user-visible behavior, or `native-core/src/windows_app.cpp` additionally read the smallest matching route.

### UI foundation — always

1. `docs/18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
2. `docs/21_UI_IMPLEMENTATION_PROGRAM.md`
3. `spec/ui/command-state-skin-contract-v0.md`

### Win32 / command / state / binding / persistence

4. `spec/ui/current-win32-command-state-inventory-v0.md`
5. `spec/ui/schema/command-definition.schema.json`
6. `spec/ui/schema/state-definition.schema.json`
7. `spec/ui/schema/binding-definition.schema.json`

### Toolkit spike / skin runtime / package / components

4. `docs/23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
5. `spec/ui/emberskin-package-and-safety-limits-v0.md`
6. `spec/ui/native-component-contracts-v0.md`
7. `spec/ui/schema/skin-manifest.schema.json`

### EmberLights Default skin

4. `docs/24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
5. `docs/25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`
6. `spec/ui/native-component-contracts-v0.md`

### SoundSwitch evidence / Reference skin

4. `docs/19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
5. `docs/22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
6. `docs/20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
7. `docs/25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`
8. `research/ui/soundswitch/README.md` and the relevant manifest/analysis/deviation template

### Skins Platform V2 / custom controls / actions / designer / migration

4. `docs/SKINS_PLATFORM_V2_START_HERE.md`
5. `docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`
6. `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
7. The smallest matching contract:
   - `spec/ui/user-customization-and-action-composition-v0.md` for #39/#66 overlays/custom pages;
   - `spec/ui/ember-actions-contract-v1.md` and `spec/ui/schema/ember-action.schema.json` for #65;
   - `spec/ui/skin-designer-contract-v1.md` for #66/#67;
   - SoundSwitch evidence/Reference docs for #68;
   - `docs/27_CROSS_DJ_CONTROLLER_AND_SKIN_PORTABILITY_PLAN.md` for #70.
8. `docs/SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md` when assigned one of the #63 work packages.

The registry lifecycle policy is mandatory for any feature that changes callable behavior, observable state, availability, components, persistence, safety, or mappings—even when the assigned work is not described as UI work.

### UI qualification

4. `docs/26_UI_QUALIFICATION_MATRIX.md`
5. The exact platform, package, component, skin, action, persistence, compatibility, and migration specs under test

Do not spend tokens re-reading or re-explaining unrelated research files.

## Binding UI rules

UI work must follow the shared command/state/skin architecture. Do not implement new behavior as layout-specific callbacks when it can be represented as a stable product command and observable state. EmberLights Default, SoundSwitch Reference, MIDI/keyboard/controller mappings, future user skins, Ember Actions, and remotes must target the same capability contracts. Skins are presentation/binding packages, never alternate lighting engines.

Before changing `native-core/src/windows_app.cpp`, inventory the affected `ControlId` callbacks, visible state, realtime class, persistence scope, command IDs, state keys, capabilities, components, and current bypass-ledger entry. Follow the strangler migration: route current behavior through command/state facades, preserve tests, then move presentation into the skin path. Do not begin with a visual rewrite that duplicates domain behavior.

Every user-visible feature addition, removal, rename, or semantic change must reconcile the canonical registries, generated native/JSON artifacts, schemas, Default/Reference/Safe impact, actions/mappings/profiles, compatibility/deprecation metadata, tests/examples, and migration rules. A hard-coded control is not a complete platform feature, and deleting code is not complete while artifacts still reference it.

The production toolkit is not preselected. Issue #37 measures product-shaped Slint, WinUI control, and Direct2D Safe workloads. Do not commit #32/#67 to a framework before that evidence is recorded and reconciled with #31/#64.

A skin package, overlay, action, mapping, profile, designer project, or migration source is untrusted data. Enforce package/action/parser safety limits, no arbitrary code execution, no direct device/network/filesystem/state access, bounded expressions/subscriptions/assets/graphs, transactional activation, and trusted Safe fallback. Invalid reload preserves the current view; invalid first load reaches Safe; Runner/DMX stay active.

Ember Actions are typed bounded graphs over registered commands/state. Do not implement JavaScript/Lua/WASM/native plugins, hidden global variables, runtime `eval`, arbitrary state-trigger loops, or UI sleeps/timers as show timing. The optional expert text form must round-trip to the same canonical Action IR and cannot add semantics unavailable to the graph.

For SoundSwitch-reference work, use forensic evidence tiers and the capture matrix. Mark every claim `MEASURED`, `ESTIMATED`, `DESIGN_TARGET`, or `BEHAVIORAL`; never present compressed public screenshot measurements as exact SoundSwitch implementation values. Use original EmberLights assets and maintain the explicit Preserve/Improve/Reject ledger.

## Work contract

Start by stating:

- the exact bounded deliverable;
- dependency gate and issue number;
- files/contracts owned and exact shared files reserved;
- explicit non-goals;
- commands/states/components/capabilities/schema generations expected to change;
- acceptance tests;
- smallest relevant test commands;
- expected completion evidence.

Do not change architecture or promise hardware support without evidence. Preserve unknown migration data and never operate destructively on the user's only media/project files.

During implementation:

- run narrow tests first;
- avoid repeated full suites for cosmetic changes;
- reserve full DPI/golden/fuzz/soak matrices for documented gates;
- record decisions and machine-readable evidence in the repository rather than long chat explanations;
- do not add a direct UI callback bypass after command/state extraction begins without an explicit ledger entry and owner issue;
- do not create Reference-only or Default-only domain behavior;
- do not create a private action/macro/registry list inside one skin or controller editor;
- do not silently update goldens or benchmark baselines to hide regressions;
- do not add dead/fake controls for parity features the engine does not support;
- run the registry/generated-artifact/cross-reference compatibility checks for every user-visible semantic change.

At completion, update tests and affected decisions/backlog/specs/issues. Report:

- deliverable and files changed;
- commands/states/components/capabilities/schema/generations added, changed, deprecated, or removed;
- bundled skin/Safe/action/profile/migration impact;
- direct callback bypasses added/removed/remaining;
- exact tests run and results;
- generated-artifact cleanliness and compatibility-diff classification;
- measured startup/memory/CPU/repaint/action-dispatch/DPI/accessibility results when applicable;
- unresolved risks;
- explicit legacy bridges;
- next dependency.

Never report only that the UI “looks good” or that a feature is “done.”
