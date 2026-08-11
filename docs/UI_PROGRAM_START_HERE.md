# EmberLights Modular UI Program — Start Here

Use this file only for UI/UX, command/state, mapping, persistence, skin, screenshot, toolkit, component, customization, or UI-qualification work.

## Mission

Build a lightweight modular interface platform in which:

- EmberLights Default, SoundSwitch Reference, Safe, and future user skins are presentations over one engine;
- skins, keyboard, MIDI/controller profiles, DJ commands, and future remotes invoke the same typed commands;
- feedback comes from shared state snapshots;
- SoundSwitch migration familiarity is preserved without copying proprietary assets or inherited rigidity;
- VirtualDJ-style customization becomes possible later through safe typed controls/overlays;
- changing DJ software/controller/skin/topology does not rewrite lighting content;
- skin failure or switching never stops Runner/DMX or bypasses safety.

## Current gate

**Issue #38 / `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` is the immediate P0.**

Broad Default/Reference implementation waits for:

- raw SoundSwitch Micro proof and fixture truth;
- visible truthful Connections state;
- deterministic OS2L startup/listener behavior;
- shared Static Look Toggle/Hold/feedback/Autoloop-return behavior.

Planning, SoundSwitch evidence, behavior-preserving command/state extraction, and bounded toolkit spikes may proceed in parallel if they do not interfere.

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
                                            #39 custom controls later
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
- future custom controls/overlay/action-composition plan;
- qualification matrix and issue decomposition.

## Immediate implementation order after the core gate

1. Complete #31 command/state facade without changing lighting behavior.
2. Complete #37 equivalent toolkit measurements and decision.
3. Implement #32 package/runtime/Safe surface.
4. Build #33 Default journeys.
5. Finish #30 Tier A evidence and build #34 Reference parity skin.
6. Close #35 persistence/connection behavior across both skins.
7. Execute #36 qualification.
8. Begin #39 custom controls only after relevant platform evidence is stable.

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
- no broad UI work that delays the active core hardware recovery program.

## Completion evidence

Every UI agent reports:

- issue/gate and bounded deliverable;
- files/contracts owned;
- commands/states/schema/components changed;
- exact tests and results;
- measured performance/DPI/accessibility evidence where relevant;
- risks and legacy bridges;
- docs/issues updated;
- next dependency.
