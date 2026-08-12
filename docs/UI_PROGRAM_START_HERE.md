# EmberLights Modular UI Program — Start Here

Use this file only for UI/UX, command/state, mapping, persistence, skin, screenshot, toolkit, component, customization, migration, designer, or UI-qualification work.

## Mission

Build a lightweight modular interface platform in which:

- EmberLights Default, SoundSwitch Reference, Safe, and future user skins are presentations over one engine;
- skins, keyboard, MIDI/controller profiles, DJ commands, and future remotes invoke the same typed commands;
- feedback comes from shared state snapshots;
- SoundSwitch migration familiarity is preserved without copying proprietary assets or inherited rigidity;
- VirtualDJ-style customization becomes possible through safe typed controls, overlays, Ember Actions, and a first-class visual Skin Studio;
- users can create, fork, import, export, update, repair, and migrate supported skins without manually editing files;
- changing DJ software/controller/skin/topology does not rewrite lighting content;
- skin failure or switching never stops Runner/DMX or bypasses safety.

## Current gate

**Issue #38 / `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` remains the core hardware authority where work conflicts.**

Broad Default/Reference implementation still requires coordination with:

- raw SoundSwitch Micro and fixture truth;
- visible truthful Connections state;
- deterministic OS2L startup/listener behavior;
- shared Static Look Toggle/Hold/feedback/Autoloop-return behavior;
- lean Perform/Studio separation and resource budgets in #56.

Planning, catalog/schema work, behavior-preserving command/state extraction, bounded action/compiler work, migration-IR work, and toolkit spikes may proceed in isolated lanes if they do not interfere.

## Read by work type

### Foundation — every UI agent

1. `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
2. `21_UI_IMPLEMENTATION_PROGRAM.md`
3. `../spec/ui/command-state-skin-contract-v0.md`
4. `SKIN_PLATFORM_V2_EMBER_ACTIONS_AND_DESIGNER_PLAN.md` for issue #69 and later full customization

### Current Win32 / command / state / mapping

- `../spec/ui/current-win32-command-state-inventory-v0.md`
- `../spec/ui/schema/command-definition.schema.json`
- `../spec/ui/schema/state-definition.schema.json`
- `../spec/ui/schema/binding-definition.schema.json`
- issue #31 for typed facade extraction
- issue #71 for canonical catalogs/code generation

### Ember Actions

- `../spec/ui/ember-actions-v1.md`
- issue #73

Ember Actions is a typed bounded graph shared by skin, keyboard, MIDI/HID/controller, DJ commands, tests, and future external surfaces. It is not arbitrary code or show timing. Musical waits/fades/quantization remain Runner/domain plans.

### Toolkit / runtime / package / components

- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
- `../spec/ui/emberskin-package-and-safety-limits-v0.md`
- `../spec/ui/native-component-contracts-v0.md`
- `../spec/ui/schema/skin-manifest.schema.json`
- `../spec/ui/schema/layout.schema.json`
- `../spec/ui/examples/`
- issues #37 and #32

### Default skin/product UX

- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`
- issue #33

### SoundSwitch evidence/Reference skin

- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
- `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `../research/ui/soundswitch/`
- issue #34

### Cross-DJ/controller/customization/Skin Studio

- `27_CROSS_DJ_CONTROLLER_AND_SKIN_PORTABILITY_PLAN.md`
- `../spec/ui/user-customization-and-action-composition-v0.md`
- `SKIN_PLATFORM_V2_EMBER_ACTIONS_AND_DESIGNER_PLAN.md`
- issue #39 for Level 0/1 bounded customization
- issue #74 for the full visual Skin Studio

### Skin migration and SDK

- `../spec/ui/skin-migration-ir-v1.md`
- issue #75

Foreign adapters are read-only Studio/migration work. They produce provenance-rich IR and normal candidate Ember packages/profiles; foreign scripts never execute and Perform never parses foreign formats.

### Build-agent handoff

- `handoffs/SKIN_PLATFORM_V2_BUILD_AGENT_HANDOFF_2026-08-12.md`

### Qualification

- `26_UI_QUALIFICATION_MATRIX.md`
- issues #36 and #56

## Issues and order

```text
#38 core authority where conflicts exist
#56 lean Perform/Studio budgets ----------------------------------+
                                                                  |
#30 SoundSwitch evidence -------------------------------> #34 exact Reference visual freeze
#31 command/state facade ---+                                       |
                            +-> #71 catalogs/codegen ---------------+
                            |                                       |
                            +-> #37 toolkit -> #32 runtime/Safe -----+
                                      |                              |
                                      +-> #73 Ember Actions ---------+
                                               |                     |
                                               +-> #39 bounded customization
                                               |                     |
                                               +-> #33/#34 bundled skins
                                               |                     |
                                               +-> #74 Skin Studio --+
                                               |                     |
                                               +-> #75 migration/SDK-+
                                                                  |
                                                     #35/#36 qualification

Parent full-platform epic: #69
```

## Current planning artifacts complete

- architecture and one-command/one-state principles;
- screenshot/evidence program and templates;
- SoundSwitch observation/deviation ledger;
- Reference skin specification;
- Default information architecture and user journeys;
- EmberLights design system/brand direction;
- current Win32 command/state inventory;
- command/state/binding/manifest/layout schemas;
- package safety/resource limits;
- native complex-component contracts;
- Safe/Default/Reference Live/Studio layout examples;
- toolkit spike/decision gate;
- cross-DJ/controller/skin portability plan;
- bounded custom controls/overlay/action-composition plan;
- full Skin Studio/package/import/export architecture;
- typed bounded Ember Actions contract;
- standard Skin Migration IR and VirtualDJ-adapter plan;
- build-agent work packets and shared-file coordination;
- qualification matrix and issue decomposition.

## Immediate implementation order

1. Merge/accept the issue #69 planning artifacts.
2. Complete #71's canonical catalog/signature/code-generation foundation without changing domain behavior.
3. Complete #73's direct-command-compatible action schema/compiler foundation.
4. Complete #37/#32's renderer/runtime/Safe and immutable package activation.
5. Complete #39's binding editor and bounded custom controls/overlays.
6. Build #33/#34 bundled packages over the same contracts.
7. Build #74 Skin Studio in staged visual-authoring slices.
8. Build #75 Migration IR/SDK and a narrow VirtualDJ importer only after shared contracts are stable.
9. Close #35/#36/#56 persistence, safety, accessibility, installed-Windows, VirtualDJ co-load, performance, and switching evidence.

## Non-negotiable rules

- no cosmetic rewrite before command/state/catalog facades;
- no UI-specific lighting engine;
- no Reference-only or Default-only behavior;
- no browser required in Runner;
- no arbitrary skin/action/foreign code or direct device/network/filesystem access;
- no UI wait/timer as show timing;
- no exact SoundSwitch or foreign-skin pixel/semantic claim without evidence;
- original EmberLights assets for bundled skins;
- no automatic redistribution of imported proprietary assets;
- no hidden/droppable blackout or safety path;
- no live transient state silently saved;
- no controller/device identity embedded into a skin;
- no toolkit selection by preference;
- no mockup-only completion claim;
- no Skin Studio/migration/compiler burden in lean Perform;
- no broad UI work that delays an active authoritative core gate;
- no shared command/state/schema edits without an issue reservation and active-branch check.

## Completion evidence

Every UI/skin/action/migration agent reports:

- issue/gate and bounded deliverable;
- branch and exact files/contracts owned;
- commands/states/capabilities/components/actions/schema versions changed;
- exact tests and results;
- measured performance/DPI/accessibility/security evidence where relevant;
- active-show/DMX continuity evidence where relevant;
- migration provenance/translation status where relevant;
- risks and legacy bridges;
- docs/issues/decision ledger updated;
- next dependency;
- installed/hardware support evidence distinguished from local/CI proof.