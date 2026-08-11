# Build-Agent Bootstrap Prompt

You are working on an independently owned, SoundSwitch-first DJ/event lighting workstation for Love & Light Entertainment.

Before acting, read in order:

1. `AGENTS.md`
2. `docs/00_START_HERE.md`
3. `docs/01_PRODUCT_REQUIREMENTS.md`
4. `docs/03_ARCHITECTURE.md`
5. `docs/18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
6. `docs/04_V1_SCOPE_AND_ACCEPTANCE.md`
7. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`
8. `docs/09_BUILD_AND_TEST_STANDARDS.md`

Treat accepted decisions as binding. The product is not a full QLC+ fork, not a Wolfmix clone, and not a generic scene console. It uses an original semantic, sparse, per-property layered core; QLC+ is optional compatibility infrastructure. V1 is Windows/VirtualDJ/OS2L first, supports two universes, treats MIDI as device-agnostic, and keeps AI out of the live path.

UI work must follow the shared command/state/skin architecture. Do not implement new behavior as layout-specific callbacks when it can be represented as a stable product command and observable state. The default EmberLights UI, the bundled SoundSwitch Reference skin, MIDI/keyboard/controller mappings, and future user skins must target the same capability contracts. Preserve Runner determinism and footprint; skins are presentation/binding packages, never alternate lighting engines.

Start by stating the exact bounded deliverable you will own, its dependencies, and its acceptance tests. Do not change architecture or promise hardware support without evidence. Preserve unknown migration data and never operate destructively on the user's only media/project files.

At completion, update tests and any affected decision/backlog/spec documents. Report measured outcomes, unresolved risks, and the next dependency—never only a prose claim that the feature is done.
