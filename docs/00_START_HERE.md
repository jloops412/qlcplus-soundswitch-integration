# Start Here

## Mission

Create a uniquely owned, general-purpose, gig-ready replacement for SoundSwitch. The product must be usable by DJs and lighting operators with rigs unrelated to Love & Light Entertainment. It must eventually match every relevant SoundSwitch capability, then surpass it with open hardware, safer persistence, portable semantic scripts, SoundSwitch migration, and event-aware workflows.

This is not a generic DMX console and not a Wolfmix clone.

## Current state

- The original `Build Custom Lighting Software` conversation was reviewed in full on 2026-08-08.
- Later research superseded the initial suggestion to fork QLC+ wholesale.
- Joshua confirmed a hybrid QLC+ strategy is acceptable if the result remains uniquely ours.
- The handoff package now exists.
- The compiled/tested native vertical slice and first end-to-end `runner_lab` exist.
- EmberLights now has a versioned/checksummed project model and compiler, a deterministic service Runner, Art-Net, sACN, and initial ENTTEC DMX USB Pro serial output, WinMM MIDI/learn foundations, a bounded Studio-only QLC+ QXF fixture importer, a native Windows Studio/Live shell, and machine-readable production qualification/report foundations. Windows/Linux CI and installer packaging pass on the preceding merged checkpoint; searchable fixture-library distribution, VirtualDJ, MIDI, USB-DMX, broader output-hardware, eight-hour Windows soak, and live-event qualification remain active gates.
- Joshua clarified on 2026-08-08 that his fixtures are later validation inputs, not the product scope, and that full SoundSwitch feature parity is the minimum finished-product bar.
- Preview.314 proved active Runner, OS2L parsing, frame generation, output routing, and host-side SoundSwitch Micro writes, but it did not produce physical fixture response. It also exposed a Connections layout/accessibility defect and VirtualDJ's need for a first OS2L command in the current direct-IP workflow.
- `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` is the binding immediate implementation program while its gates remain open. It requires raw Micro proof independent of projects, reusable adapter/session hardening, frame and fixture truth, visible Connections, deterministic OS2L startup, and Static Look Toggle/Hold before broad UI/skin implementation proceeds.
- `13_SOUNDSWITCH_PARITY_LEDGER.md` is the binding completeness checklist. Milestone exclusions sequence delivery; they do not remove features from the finished product.

### Modular UI planning package

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md` is the binding UI direction: one shared command/state model powers the default UI, bundled SoundSwitch Reference skin, future user skins, keyboard/MIDI/controller mappings, and external control surfaces.
- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md` defines the required screenshot/evidence corpus, screen-state matrix, measurement method, current Win32 UI audit, and preserve/improve/reject analysis.
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md` defines the build-ready SoundSwitch-familiar Studio/Live layouts, responsive behavior, component/state requirements, parity journeys, and golden-screen gates.
- `21_UI_IMPLEMENTATION_PROGRAM.md` defines UI stage gates, dependencies, agent ownership, merge order, risks, and token/test-efficiency rules. Its broad implementation stages remain subordinate to the open core hardware recovery gates.
- `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md` records current SoundSwitch interface findings, evidence confidence, capture gaps, and approved modernization targets.
- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md` keeps the toolkit evidence-gated: Slint first spike, WinUI control comparison, Direct2D Safe baseline, Qt only if triggered by evidence.
- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md` defines the modern EmberLights Default startup, migration, setup, authoring, rehearsal, gig, recovery, responsive, and accessibility experience.
- `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md` defines EmberLights-owned visual language, semantic tokens, component states, content-color separation, safety hierarchy, icon/asset governance, and motion.
- `26_UI_QUALIFICATION_MATRIX.md` defines command/state/binding/package/component, golden-screen, DPI, accessibility, performance, persistence, fault, hardware, and DMX-continuity evidence.
- `../spec/ui/command-state-skin-contract-v0.md` defines the typed command registry, shared state registry, bindings, skin package/runtime, validation, safe fallback, and strangler migration from the existing hard-coded Win32 shell.
- `../spec/ui/current-win32-command-state-inventory-v0.md` maps the present Win32 controls and callbacks into proposed product-semantic commands, states, priority classes, and persistence scopes.
- `../spec/ui/emberskin-package-and-safety-limits-v0.md` defines bounded package/archive/assets/view graph/expressions, activation transaction, Safe fallback, caching, abuse tests, and failure behavior.
- `../spec/ui/native-component-contracts-v0.md` defines toolkit-neutral interfaces for the browser, venue/patch, track hierarchy, timeline, waveform, Autoloops, Static Looks, Inspector, mapping, connections, diagnostics, migration, validation, history, active layers, performance controls, and preview host.
- `../spec/ui/schema/` contains machine-readable schemas for command, state, binding, and skin-manifest definitions.
- GitHub issue #29 is the UI epic; issues #30–#37 are the bounded evidence, facade, toolkit, runtime, skin, connection, and qualification work packages.

## Binding product statement

> A general-purpose, SoundSwitch-first, automation-first DJ/event lighting workstation with a lightweight offline Runner, a capable Studio, open MIDI and DMX choices, first-class migration, and no dependency on one user's fixtures or hardware.

## What is accepted

- Windows is the required launch platform; macOS follows later and cannot block Windows delivery.
- Two DMX universes in V1.
- VirtualDJ/OS2L first; Serato is the second direct DJ-integration priority after VirtualDJ is stable.
- Predictive hold, then live-audio beat fallback, then tap tempo, then a safe unsynchronized look.
- Control One first as MIDI; onboard DMX/OLED/storage are experimental.
- Any MIDI controller can map to actions and parameters.
- Same-PC and separate-PC modes are equal.
- SoundSwitch import is core.
- SoundSwitch workflow outranks Wolfmix ideas.
- Original semantic/layered core; optional QLC+ output bridge, Studio-only QXF compatibility adapter, and selective audited reuse.
- OFL enters through a versioned Studio adapter and quarantine boundary; Runner consumes only our stable compiled profile format.
- Autoloops use a scalable 32-slot bank library; four banks are a controller/UI window, not an engine limit.
- No AI in Runner's live output path.
- UI behavior is exposed through stable command/state contracts; skins and hardware mappings are control surfaces over those contracts, not alternate engines.
- EmberLights ships a modern Default skin and a SoundSwitch Reference skin for migration/parity testing; the Reference uses original EmberLights assets rather than a pixel-for-pixel proprietary copy.
- Native SoundSwitch screenshots are research evidence and parity baselines, not source-code/CSS recovery; exact visual tokens remain evidence-tagged until controlled captures exist.
- No arbitrary skin code, embedded browser requirement, or user script VM may become mandatory in Runner's live path.
- A malformed or failing skin cannot stop Runner/DMX; invalid first load reaches the trusted Safe surface and invalid reload preserves the current skin.
- The production UI toolkit remains unaccepted until the bounded spike measures product-shaped Default, Reference, Studio, Safe, accessibility, deployment, and performance workloads.
- The product name is EmberLights, and full-V1 Windows testing builds are distributed through an installer while qualification continues.
- Host-accepted USB writes are a software boundary, not proof of physical DMX output.
- Conservative migration projects must disclose unverified physical patch/profile data even when internal schema validation passes.
- Static Looks use one core EventMoment layer above Autoloops, with Toggle/Hold implemented as shared command/binding behavior rather than a UI-specific engine.

## What is not accepted yet

- Production implementation language and UI toolkit.
- Exact native support for MyDMX Buddy or SoundSwitch USB interfaces until physical qualification gates pass.
- Exact first-use balance between manual song scripting and AutoScripting.
- Any claim that proprietary Control One DMX or OLED functions are supported.
- Exact SoundSwitch Reference visual dimensions before the controlled native screenshot corpus is measured.
- Freeform end-user skin design or a public marketplace before the command/state facade, Safe runtime, two bundled skins, and qualification suite are stable.

## Core required reading order

All agents read only the smallest binding set needed for their work:

1. This file.
2. `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` while its recovery gates remain open.
3. `01_PRODUCT_REQUIREMENTS.md`.
4. `03_ARCHITECTURE.md`.
5. `04_V1_SCOPE_AND_ACCEPTANCE.md`.
6. `08_DECISIONS_AND_OPEN_QUESTIONS.md`.
7. `09_BUILD_AND_TEST_STANDARDS.md`.
8. The parity/backlog/research file directly relevant to the bounded assignment.

## Additional required reading for UI/UX or skin work

Read the smallest route matching the work package after the core set.

### All UI-platform work

1. `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`.
2. `21_UI_IMPLEMENTATION_PROGRAM.md`.
3. `../spec/ui/command-state-skin-contract-v0.md`.

### Current Win32 / command / state / mapping / persistence work

4. `../spec/ui/current-win32-command-state-inventory-v0.md`.
5. `../spec/ui/schema/command-definition.schema.json`.
6. `../spec/ui/schema/state-definition.schema.json`.
7. `../spec/ui/schema/binding-definition.schema.json`.

### Toolkit / runtime / package / components

4. `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`.
5. `../spec/ui/emberskin-package-and-safety-limits-v0.md`.
6. `../spec/ui/native-component-contracts-v0.md`.
7. `../spec/ui/schema/skin-manifest.schema.json`.

### EmberLights Default

4. `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`.
5. `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`.

### SoundSwitch Reference / evidence

4. `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`.
5. `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`.
6. `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`.
7. `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`.
8. `research/ui/soundswitch/README.md` and the relevant template.

### UI qualification

4. `26_UI_QUALIFICATION_MATRIX.md`.
5. The exact platform/skin/component/package specs under test.

Do not make non-UI agents consume the UI research corpus unless their bounded work touches those contracts. Do not let UI stage work displace the raw hardware and fixture-truth gates.

## Definition of progress

Progress is measured by a working, testable replacement path:

- exact inputs captured;
- deterministic state produced;
- correct DMX emitted;
- failures handled safely;
- footprint and latency measured;
- real hardware and VirtualDJ verified;
- user workflow tested.

During the active core recovery program, progress specifically requires:

- raw Micro output proven without fixture/profile assumptions;
- physical adapter evidence separated from software-open/write evidence;
- one exact fixture/profile/address matched to a successful raw frame;
- truthful desired/saved/applied/active connection state;
- deterministic VirtualDJ startup behavior;
- Static Look Toggle/Hold ownership and Autoloop return tests.

For UI work, progress additionally requires:

- command/state contracts rather than layout-specific callbacks;
- a measured toolkit decision and toolkit-neutral public schema;
- both bundled skins exercising identical domain behavior;
- the Default journeys and Reference parity journeys passing;
- screenshot/golden-state evidence at target resolutions and DPI;
- accessible keyboard/non-color/focus behavior;
- safe fallback and uninterrupted DMX during skin failure/switching;
- correct app/project/live persistence scope;
- measured usability, startup, memory, CPU, repaint, and jitter—not mockups alone.

Lines of code and mock screens are not milestones by themselves.
