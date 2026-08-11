# Modular UI Implementation Program

Status: binding execution plan for the EmberLights UI/UX program.

Related artifacts:

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
- `../spec/ui/command-state-skin-contract-v0.md`
- `../spec/ui/current-win32-command-state-inventory-v0.md`
- GitHub epic #29 and child issues #30–#36

## Mission

Deliver a lightweight, SoundSwitch-familiar but modern and fully modular UI system without destabilizing the already-working project/compiler/Runner/output core.

The end state is not one redesigned window. It is a UI platform in which:

- EmberLights Default and SoundSwitch Reference are separate validated skin packages;
- keyboard, MIDI, Control One, DJ-software commands, future remotes, and skins invoke the same typed commands;
- all feedback comes from shared bounded state snapshots;
- a user can later customize controls and layouts without creating a second lighting engine;
- the Runner remains deterministic, offline, small, and safe;
- migration from the current Win32 shell is incremental and reversible.

## Current baseline

The current native Windows shell is a valuable functional reference and migration host. It already exposes project lifecycle, Undo/Redo, profiles, patching, groups, Static Looks, Autoloops, Track Scripts, MIDI Learn, connections, safety, diagnostics, SoundSwitch migration tools, Runner controls, last-project reopening, and installed-GUI smoke testing.

Its UI implementation is intentionally temporary:

- fixed `Page` and `ControlId` enums;
- direct Win32 control construction;
- control-ID-specific callback dispatch;
- fixed navigation/status dimensions;
- explicit pixel positioning in `layout_page`;
- page-specific refresh functions;
- ambiguous Apply actions for Connections and Safety.

The program preserves functionality while replacing that coupling.

## Program principles

1. **Capability before chrome.** Extract commands/state before visual redesign.
2. **One behavior implementation.** Skins and mappings never reimplement domain logic.
3. **Runner authority.** UI timing, animation, and skin state cannot become show timing.
4. **Safety survives every skin.** Blackout, stop, work light, override release, hazard gates, and output health remain reachable.
5. **Evidence, not imitation.** Exact SoundSwitch measurements require native evidence; workflows may be specified from official documentation sooner.
6. **Strangler migration.** The current Win32 shell remains a legacy adapter until each capability is proven through the new facade/runtime.
7. **No toolkit lock-in at the public contract.** The skin schema, commands, states, and native-component interfaces remain toolkit-neutral.
8. **No browser tax in Runner.** HTML/CSS/browser embedding is not a default requirement.
9. **Explicit persistence.** App-local, project-authored, and live-transient state are never silently mixed.
10. **Qualification is part of implementation.** Screenshots alone do not complete a UI milestone.

## Dependency graph

```text
Evidence lane
#30 Native SoundSwitch capture and measurements
          └───────────────┐
                          v
Platform lane        #34 visual freeze
#31 Command/state facades
          |
          v
#32 Skin runtime + Safe fallback
          |
      ┌───┴──────────────┐
      v                  v
#33 Default v0      #34 Reference v0
      └──────┬───────────┘
             v
#35 Connection/persistence completion
             v
#36 UI qualification and release evidence
```

#30 runs in parallel with #31. It blocks exact SoundSwitch visual-token freeze, not command/state extraction or runtime design.

## Stage gates

### G0 — Planning and evidence contract

Required before implementation:

- binding architecture and reference-skin specification committed;
- evidence confidence tiers defined;
- current Win32 surface inventory started;
- epic and bounded child issues created;
- ownership and merge order explicit.

Exit evidence: documents 18–22, UI contract, issue #29.

### G1 — Typed command/state facade

Owner: issue #31.

Deliverables:

- generated or statically defined command metadata;
- typed invocation API and explicit result/rejection model;
- state metadata and subscription/snapshot API;
- current menus, accelerators, and representative Win32 controls routed through the facade;
- Command Explorer and State Explorer developer views;
- complete list of temporary direct-callback bypasses.

Mandatory first command set:

- show start/stop;
- blackout;
- work light;
- manual BPM and tap;
- Static Look activate/clear;
- Autoloop launch/previous/next/clear;
- Track Script start/clear;
- group/fixture override and Release All;
- bank page/filter/exclusive controls;
- safety arming/disarming;
- OS2L reconnect;
- project open/save/validate.

Gate tests:

- current behavior preserved;
- unknown command and invalid parameter rejection;
- unavailable/safety-rejected/queue-full outcomes visible;
- F8 still reaches the non-droppable blackout path;
- no project schema or lighting-semantic changes.

### G2 — Skin runtime and Safe fallback

Owner: issue #32. Requires G1.

Deliverables:

- versioned `.emberskin` manifest/package;
- strict package validator and limits;
- immutable bounded view graph;
- responsive layout containers;
- ordinary control primitives;
- adapters for native complex components;
- semantic theme tokens;
- compact/standard/wide/touch-live variant selection;
- state subscriptions and typed command bindings;
- Safe fallback skin.

Gate tests:

- invalid first load falls back to Safe;
- invalid reload leaves current skin active;
- skin switch does not stop Runner, output, active content, or project state;
- unknown commands/state keys fail with exact diagnostics;
- no arbitrary code/filesystem/network/USB access from skins;
- hidden panels do not continue expensive repaint/subscription work.

### G3 — EmberLights Default v0

Owner: issue #33. Requires G2.

Deliverables:

- modern Studio and Live workspaces;
- contextual Inspector;
- Library/Assets dock;
- utility drawers;
- pinned Live health/safety strip;
- migration of every currently supported functional surface or explicit bounded legacy bridge;
- Command Explorer entry points;
- clear project save/recovery state;
- original EmberLights theme and assets.

Gate tests:

- currently supported workflows function without using the legacy page list, except tracked bridges;
- every visible action/state is registry-backed;
- 1366×768 and 1920×1080 usable;
- Runner-neutral skin load/reload;
- footprint and repaint evidence recorded.

### G4 — SoundSwitch Reference v0

Owner: issue #34. Requires G2 and enough G1 coverage. Exact visual freeze requires minimum Tier A evidence from #30.

Deliverables:

- SoundSwitch-familiar Studio hierarchy;
- SoundSwitch-familiar Live/Performance hierarchy;
- four-bank visible Autoloop window over the 64×32 engine;
- selected/active/progress/repeat/exclusive states;
- Static Look `RELEASE`/`SET`/`FORCE_ZERO` authoring clarity;
- primary performance overrides and intensity controls;
- permanent health, Work Light, Blackout, active content, safety, and override release;
- approved Preserve/Improve/Reject deviation ledger.

Gate tests:

- documented parity journeys pass for implemented engine capabilities;
- Default and Reference produce identical domain results for the cross-surface suite;
- no SoundSwitch-proprietary assets;
- compact/standard/high-DPI variants qualify;
- switching skins during active DMX is neutral.

### G5 — Connection and persistence UX

Owner: issue #35. May start during G2/G3 but closes after both skins can consume it.

Deliverables:

- definitive setting-scope registry;
- immediate persistence where safe;
- exact reconnect/restart/validation state when not immediate;
- configured-source auto-connect/reconnect;
- actionable DJ, MIDI, Universe 1, Universe 2, USB-DMX, and safety status;
- narrow diagnostic drawers;
- last-project reopening preference/recovery;
- explicit transient-to-project promotion only where safe.

Gate tests:

- restart/reopen scope matrix;
- controlled disconnect/reconnect without Runner stop;
- machine-specific identifiers do not leak into portable project data;
- routine connection failure creates no blocking Live modal;
- hazardous arming remains fail-closed.

### G6 — End-user custom controls

Not part of the first skin-runtime gate, but the architecture must not block it.

First customization milestone:

- right-click/long-press `Edit Control`;
- searchable command picker;
- label/icon/accent/interaction configuration;
- Custom Button, Pad, Fader, Knob, and small user panel;
- shared keyboard/MIDI binding editor;
- import/export of user overlay and mapping packages;
- reset to bundled defaults.

Visual freeform layout editing follows only after the two bundled skins and qualification suite are stable.

### G7 — Qualification

Owner: issue #36. Requires G3–G5.

Required evidence:

- golden screens for both skins at target resolution/DPI combinations;
- keyboard/focus/touch/reduced-motion/non-color status tests;
- cold start, memory, idle CPU, active repaint, diagnostics-open, and skin-switch benchmarks;
- malformed/fuzzed package tests;
- unknown/deprecated command/state tests;
- cross-surface domain equivalence;
- DMX continuity during switch/reload/failure/fallback;
- machine-readable report integrated into the release gate.

## Agent ownership and parallelism

### Allowed parallel work

- #30 evidence capture can run beside #31 facade extraction.
- Native complex-component interface design can be planned during #31 but implemented only against stable command/state contracts.
- #35 persistence-scope inventory can begin before the skins, while visual drawers wait for G2.
- Default and Reference layout package authoring may proceed in parallel after G2 if they do not edit the same runtime files.

### Disallowed parallel work

- Two agents must not independently invent command IDs for the same behavior.
- No agent may build a separate Reference-only event path.
- No cosmetic rewrite of `windows_app.cpp` before the facade exists.
- No third-party skin scripting engine before Safe fallback and package limits are qualified.
- No final toolkit decision based only on mockups or developer preference.
- No replacing the non-droppable blackout path with ordinary UI dispatch.

## Branch and merge protocol

Recommended branches:

```text
ui/evidence-corpus
ui/command-state-facade
ui/skin-runtime
ui/default-skin-v0
ui/reference-skin-v0
ui/connection-persistence
ui/qualification
```

Rules:

1. #31 owns domain-facing command/state names until merged.
2. #32 owns skin package/runtime paths after G1 contracts stabilize.
3. Skin agents own bundled package/layout/theme/assets paths, not runtime semantics.
4. Connection/persistence work owns settings services and state exposure, not duplicate skin logic.
5. Qualification owns golden/evidence tooling and may not silently update expected results to hide regressions.
6. Merge in dependency order; rebase skin branches after runtime contract changes.
7. Every schema or command rename includes compatibility/deprecation handling or an explicit pre-public reset decision.

## Required agent completion report

Each work agent must leave:

- bounded deliverable completed;
- files changed;
- commands/states/schema added or changed;
- tests added and exact commands run;
- measured results where relevant;
- unresolved risks;
- explicit legacy bridges remaining;
- documentation/backlog/issue updates;
- next dependency.

A prose statement that the UI “looks good” is not completion evidence.

## Token and compute efficiency rules

These rules are binding for UI agents because continuity and test discipline matter more than repeated explanation.

1. Read the binding files once, then cite/update repository artifacts instead of restating the project history in every turn.
2. Do not repeat SoundSwitch web research already captured in documents 19, 20, and 22 unless a specific evidence gap is being closed.
3. Commit compact machine-readable registries and test evidence; do not paste large generated tables into chat.
4. During development, run the smallest relevant unit/component tests first.
5. Run full Windows/Linux suites at merge gates, not after every cosmetic edit.
6. Run eight-hour soak only at qualification/release gates, never as routine UI iteration.
7. Golden-screen generation runs only for affected states/resolutions during development; full matrix runs at G7.
8. Do not create throwaway mock implementations that bypass the command/state layer.
9. Do not ask the owner to retest unchanged mechanics because visual code moved.
10. Summaries should report decisions, code, evidence, blockers, and next action—no generic tutorials unless requested.
11. Persist source observations, measurements, and decisions in the repo so later agents do not spend tokens rediscovering them.
12. Treat external screenshots as research evidence, not application assets; avoid duplicating image binaries unnecessarily.

## UI toolkit decision gate

The production UI toolkit remains undecided until a candidate proves:

- both bundled skins;
- native complex components;
- Windows 100/125/150/200% DPI;
- keyboard/focus/accessibility requirements;
- cold start and resident-memory targets;
- idle and active repaint targets;
- no dependency on DMX scheduler locks or allocation;
- installer/startup-smoke compatibility;
- portable public skin schema.

Candidate evaluation should be a bounded spike, not a broad framework survey. The winning candidate is the smallest option that satisfies the required component and accessibility behavior with acceptable maintainability.

## Risk register

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Command registry mirrors current control IDs rather than product semantics | Future skins remain coupled to legacy UI | Stable namespaced commands, typed parameters, review against domain jobs |
| Skin runtime becomes a second app framework | Footprint and maintenance explosion | Small primitives, native complex components, strict package limits |
| Reference skin becomes a pixel clone | Legal/brand risk and inherited UX defects | Original assets, deviation ledger, workflow rather than artwork compatibility |
| Visual work outruns engine/state support | Fake or dead controls | Availability predicates and parity ledger linkage |
| Skin switching disrupts output | Gig safety failure | Immutable activation, current-skin retention, DMX continuity tests |
| Apply/save ambiguity survives redesign | User loses trust/data | setting-scope registry, immediate persistence, exact pending state |
| Multiple agents rename commands independently | Merge churn and broken skins | #31 registry ownership and generated validation |
| High-frequency state subscriptions waste CPU | Runner target regression | update classes, visibility-aware subscriptions, repaint benchmark |
| Third-party package is malformed or hostile | Crash/resource abuse | bounds, schema validation, no code execution, Safe fallback |
| Evidence work blocks architecture unnecessarily | Schedule delay | evidence and platform lanes run in parallel |

## Immediate next work order

1. Complete the current Win32 command/state inventory in `spec/ui/current-win32-command-state-inventory-v0.md`.
2. Start #31 by wrapping the existing non-droppable and priority Live actions first.
3. Begin #30 native SoundSwitch captures using the committed templates.
4. Finalize package limits and Safe skin minimum surface during #32 design.
5. Do not start final Default/Reference visual implementation before the shared runtime exists.
6. Keep core DMX/Micro/OS2L stabilization work independent; UI agents consume its status/commands without changing adapter protocols.

## Definition of program completion

The program is complete when a user can install EmberLights, reopen a project, connect VirtualDJ and supported output hardware, run a show through either bundled skin, switch skins while output remains active, execute the same functions from UI/keyboard/MIDI, diagnose failures from Live, and safely recover from an invalid skin—within the established Runner performance envelope.
