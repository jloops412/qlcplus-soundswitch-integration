# EmberLights Modular UI Program — Start Here

Use this file only for UI/UX, command/state, mapping, persistence, skin, screenshot, toolkit, component, customization, or UI-qualification work.

## Mission

Build a lightweight modular interface platform in which:

- EmberLights Default, SoundSwitch Reference, Safe, and future user skins are presentations over one engine;
- skins, keyboard, MIDI/controller profiles, DJ commands, and future remotes invoke the same typed commands;
- feedback comes from shared state snapshots;
- SoundSwitch migration familiarity is preserved without copying proprietary assets or inherited rigidity;
- VirtualDJ-style customization becomes possible through safe typed controls, overlays, a visual Skin Designer, and bounded Ember Actions;
- changing DJ software/controller/skin/topology does not rewrite lighting content;
- skin failure or switching never stops Runner/DMX or bypasses safety.

## Current gate

**UI course correction (2026-08-14):** the current Win32 shell is now a frozen transitional/Safe bridge, not EmberLights Default. Previews 98–100 proved reusable authoring and fixture-control models but did not prove acceptable product UI. New product presentation begins with the renderer-neutral Fixtures + Static Looks acceptance slice in `44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md`; raw DMX and the legacy Fixture Control Inspector are Advanced diagnostics, not the ordinary workflow.

**Replacement-shell implementation checkpoint (2026-08-14):** `45_SLINT_FIXTURES_LOOKS_LAB_CHECKPOINT.md` records the first real issue #37 surface. Slint 1.17.1 is pinned for this lab only. The markup and C++ adapter are source-green, the renderer-neutral model is native-test-green, and the lab invokes existing Static Look authoring functions without giving the renderer output authority. Offline simulation is the default; an explicit project plus process-level arm can exercise the registered bounded physical-preview service with visible cap/countdown/fault state and terminal blackout. Windows/DPI/accessibility/performance/deployment and physical-fixture evidence, a WinUI comparison, the Safe baseline, persistence completion, and toolkit acceptance remain open.

**Issue #38 / `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` is the immediate P0.**

Broad Default/Reference implementation waits for:

- raw SoundSwitch Micro proof and fixture truth;
- visible truthful Connections state;
- deterministic OS2L startup/listener behavior;
- shared Static Look Toggle/Hold/feedback/Autoloop-return behavior.

Planning, SoundSwitch evidence, behavior-preserving command/state extraction, registry/code-generation contracts, and bounded toolkit/compiler spikes may proceed in parallel if they do not interfere.

## Read by work type

### Foundation — every UI agent

1. `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
2. `21_UI_IMPLEMENTATION_PROGRAM.md`
3. `../spec/ui/command-state-skin-contract-v0.md`

### Current Win32 / command / state / mapping

- `../spec/ui/current-win32-command-state-inventory-v0.md`
- `../spec/ui/schema/command-definition.schema.json`
- `../spec/ui/schema/state-definition.schema.json`
- `../spec/ui/schema/binding-definition.schema.json`

### Toolkit / runtime / package / components

- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
- `../spec/ui/emberskin-package-and-safety-limits-v0.md`
- `../spec/ui/native-component-contracts-v0.md`
- `../spec/ui/schema/skin-manifest.schema.json`
- `../spec/ui/schema/layout.schema.json`
- `../spec/ui/examples/`

### Default skin/product UX

- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`

### SoundSwitch evidence/Reference skin

- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
- `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `../research/ui/soundswitch/`

### Cross-DJ/controller/customization

- `27_CROSS_DJ_CONTROLLER_AND_SKIN_PORTABILITY_PLAN.md`
- `../spec/ui/user-customization-and-action-composition-v0.md`

### Skins Platform V2 — visual designer, actions, import/export, compatibility

1. `SKINS_PLATFORM_V2_START_HERE.md`
2. `SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`
3. `../spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
4. the smallest matching contract:
   - `../spec/ui/ember-actions-contract-v1.md` and `../spec/ui/schema/ember-action.schema.json`;
   - `../spec/ui/skin-designer-contract-v1.md`;
   - `SKINS_PLATFORM_V2_WORK_AGENT_HANDOFF.md` for assigned implementation.

The registry policy applies to every user-visible feature change, even when the assigned agent is not primarily a UI agent.

### Qualification

- `26_UI_QUALIFICATION_MATRIX.md`

## Issues and order

```text
#38 Core recovery gate

#30 SoundSwitch evidence -------------------------------> #34 exact Reference visual freeze
#31 Command/state facade ---┐
                           ├-> #37 Toolkit spike -> #32 Skin runtime + Safe
                           └-------------------------------┘
                                                   |
                                             #33 Default / #34 Reference
                                                   |
                                              #35 persistence/connection
                                                   |
                                                 #36 qualification
                                                   |
                                      #39 / #66 custom controls and overlays
                                                   |
                             #67 full designer / #68 Reference kit / #70 migration
                                                   |
                                              #72 release qualification

#63 is the Skins Platform V2 epic.
#64 registry governance/codegen begins only in coordination with #31.
#65 Ember Actions may build schema/compiler fixtures early but cannot bypass #31/#32.
```

## Current planning artifacts complete

- architecture and principles;
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
- custom controls/overlay/action-composition foundation;
- Skins Platform V2 visual designer, Ember Actions, migration, registry lifecycle, ADR, issue graph, and work-agent handoff;
- qualification matrix and issue decomposition.

## Current implementation checkpoint

`45_SLINT_FIXTURES_LOOKS_LAB_CHECKPOINT.md` is the active implementation checkpoint under the binding direction in `44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md`. It preserves the registry/domain foundations, keeps the legacy Win32 freeze intact, and implements the first product-shaped Fixtures + Static Looks lab without claiming toolkit or product acceptance. `36_UI_RESOURCE_ADOPTION_AND_DEFAULT_2_1_CHECKPOINT.md` remains historical evidence for preview 88's bounded bridge and toolkit/resource audit.

This checkpoint adds a renderer adapter and lab-only callbacks over existing domain behavior. It does not accept a production toolkit, `.emberskin` runtime, Safe surface, product installer, or Reference skin.

## Immediate implementation order after the core gate

1. Keep the D-091 Win32 freeze green and complete/reconcile #31 command/state facade and #64 registry authority without changing lighting behavior.
2. Complete #37 equivalent toolkit measurements using the real Fixtures + Static Looks slice—not placeholder counters or another form—and record the decision.
3. Implement #32 package/runtime/Safe surface.
4. Build #33 Default journeys.
5. Finish #30 Tier A evidence and build #34 Reference parity skin.
6. Close #35 persistence/connection behavior across both skins.
7. Execute #36 relevant qualification.
8. Execute #39/#66 custom bindings, controls, overlays, and pages.
9. Build #65 Action IR/compiler/executor against accepted registries; integrate into #66 only through the canonical contract.
10. Build #67 full designer, #68 forkable Reference kit, #70 migration seam, and #72 lifecycle/release gates in dependency order.

Registry schemas, generators, compatibility fixtures, migration IR planning, and action-schema validation may proceed earlier when they preserve behavior, reserve shared files, and avoid broad UI rollout.

## Non-negotiable rules

- no cosmetic rewrite before command/state facades;
- no UI-specific lighting engine;
- no Reference-only or Default-only behavior;
- no browser required in Runner;
- no arbitrary skin code or direct device/network/filesystem access;
- no exact SoundSwitch pixel claim without Tier A evidence;
- original EmberLights assets only;
- no hidden/droppable blackout or safety path;
- no live transient state silently saved;
- no toolkit selection by preference;
- no mockup-only completion claim;
- no broad UI work that delays the active core hardware recovery program;
- no user-visible feature added/removed/renamed/changed without registry, generated-artifact, bundled-skin, action/mapping, schema, test, deprecation, and migration reconciliation;
- no Ember Action feature that cannot compile to the bounded canonical graph/IR;
- no UI sleep/timer used as musical/show timing;
- no full designer/importer dependency in lean Perform.

## Completion evidence

Every UI or user-visible feature agent reports:

- issue/gate and bounded deliverable;
- files/contracts owned;
- commands/states/components/capabilities/schema/generation changed;
- bundled skin/Safe/action/profile/migration impact;
- direct callback bypasses added/removed;
- exact tests and results;
- compatibility diff and generated-artifact cleanliness;
- measured performance/DPI/accessibility evidence where relevant;
- risks and legacy bridges;
- docs/issues updated;
- next dependency.
