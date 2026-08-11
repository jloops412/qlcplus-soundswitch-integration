# Build-Agent Bootstrap Prompt

You are working on an independently owned, SoundSwitch-first DJ/event lighting workstation for Love & Light Entertainment.

## Core pre-read

Before acting, read only the binding core needed for every workstream:

1. `AGENTS.md`
2. `docs/00_START_HERE.md`
3. `docs/01_PRODUCT_REQUIREMENTS.md`
4. `docs/03_ARCHITECTURE.md`
5. `docs/04_V1_SCOPE_AND_ACCEPTANCE.md`
6. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`
7. `docs/09_BUILD_AND_TEST_STANDARDS.md`
8. The backlog/parity/research file directly related to your bounded deliverable

Treat accepted decisions as binding. The product is not a full QLC+ fork, not a Wolfmix clone, and not a generic scene console. It uses an original semantic, sparse, per-property layered core; QLC+ is optional compatibility infrastructure. V1 is Windows/VirtualDJ/OS2L first, supports two universes, treats MIDI as device-agnostic, and keeps AI out of the live path.

## Additional UI/UX pre-read

Only agents touching UI, commands/state, mappings, persistence UX, skins, screenshots, visual qualification, or `native-core/src/windows_app.cpp` additionally read:

1. `docs/18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
2. `docs/21_UI_IMPLEMENTATION_PROGRAM.md`
3. `spec/ui/command-state-skin-contract-v0.md`
4. `spec/ui/current-win32-command-state-inventory-v0.md`
5. `docs/19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md` when collecting/analyzing evidence or building the Reference skin
6. `docs/22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
7. `docs/20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md` when building or qualifying the Reference skin

Do not spend tokens re-reading or re-explaining unrelated research files.

UI work must follow the shared command/state/skin architecture. Do not implement new behavior as layout-specific callbacks when it can be represented as a stable product command and observable state. The default EmberLights UI, bundled SoundSwitch Reference skin, MIDI/keyboard/controller mappings, and future user skins must target the same capability contracts. Preserve Runner determinism and footprint; skins are presentation/binding packages, never alternate lighting engines.

Before changing `native-core/src/windows_app.cpp`, inventory the affected `ControlId` callbacks, visible state, realtime class, persistence scope, command IDs, and state keys. Prefer the specified strangler migration: route existing behavior through command/state facades, preserve tests, then move presentation into the skin path. Do not begin with a visual rewrite that duplicates domain behavior.

For SoundSwitch-reference work, use the forensic evidence tiers and capture matrix. Mark values as `MEASURED`, `ESTIMATED`, `DESIGN_TARGET`, or `BEHAVIORAL`; never present compressed public screenshot measurements as exact SoundSwitch implementation values. Use original EmberLights assets and preserve the explicit deviation ledger.

## Work contract

Start by stating:

- the exact bounded deliverable;
- dependencies and files/contracts owned;
- explicit non-goals;
- acceptance tests;
- smallest relevant test commands.

Do not change architecture or promise hardware support without evidence. Preserve unknown migration data and never operate destructively on the user's only media/project files.

During implementation:

- run narrow tests first;
- avoid repeated full suites for cosmetic changes;
- reserve full matrix, soak, and golden regeneration for documented gates;
- record decisions and evidence in the repository rather than long chat explanations;
- do not add a direct UI callback bypass after command/state extraction begins.

At completion, update tests and affected decision/backlog/spec documents. Report measured outcomes, unresolved risks, explicit legacy bridges, and the next dependency—never only a prose claim that the feature is done.
