# Modular UI Implementation Program

Status: binding execution plan for the EmberLights UI/UX program.

## Authority and sequencing

The immediate P0 recovery program in `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` and issue #38 remains first. Broad Default/Reference implementation must not displace raw SoundSwitch Micro proof, fixture/address truth, connection-state correctness, deterministic OS2L startup, or shared Static Look Toggle/Hold behavior.

Planning, SoundSwitch evidence capture, command/state extraction that preserves behavior, and toolkit spikes may proceed in parallel when they do not interfere with #38.

## Related artifacts

### Architecture and contracts

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
- `../spec/ui/command-state-skin-contract-v0.md`
- `../spec/ui/current-win32-command-state-inventory-v0.md`
- `../spec/ui/emberskin-package-and-safety-limits-v0.md`
- `../spec/ui/native-component-contracts-v0.md`
- `../spec/ui/user-customization-and-action-composition-v0.md`
- `../spec/ui/schema/`
- `../spec/ui/examples/`

### Product UX and evidence

- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `22_SOUNDSWITCH_UI_OBSERVATION_LEDGER.md`
- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`
- `26_UI_QUALIFICATION_MATRIX.md`
- `27_CROSS_DJ_CONTROLLER_AND_SKIN_PORTABILITY_PLAN.md`
- `../research/ui/soundswitch/`

### GitHub execution

- Epic #29
- Evidence #30
- Command/state facade #31
- Skin runtime #32
- Default skin #33
- Reference skin #34
- Connection/persistence #35
- Qualification #36
- Toolkit spike #37
- Core recovery #38
- Future custom controls #39

## Mission

Deliver a lightweight, SoundSwitch-familiar but modern and fully modular UI system without destabilizing the project/compiler/Runner/output core.

The end state is a platform in which:

- EmberLights Default, SoundSwitch Reference, Safe, and future user skins are separate validated presentations;
- keyboard, MIDI, controller profiles, DJ commands, skins, and future remotes invoke the same typed commands;
- feedback comes from shared bounded state snapshots;
- users can later customize controls without a second lighting engine;
- Runner remains deterministic, offline, small, and safe;
- migration from the current Win32 shell is incremental and reversible;
- changing DJ software, controller, skin, topology, or output adapter does not rewrite show content.

## Program principles

1. **Core proof before broad polish.** Issue #38 gates broad skin implementation.
2. **Capability before chrome.** Extract commands/state before redesign.
3. **One behavior implementation.** Skins and mappings never reimplement domain logic.
4. **Runner authority.** UI timing and animation cannot become show timing.
5. **Safety survives every surface.** Blackout, Stop, Work Light, Release All, health, and hazard gates remain reachable.
6. **Evidence, not imitation.** Exact SoundSwitch measurements require native evidence.
7. **Strangler migration.** Current Win32 remains a legacy view adapter until each replacement is proven.
8. **Toolkit-neutral public contract.** `.emberskin`, registries, bindings, and native-component contracts do not expose toolkit internals.
9. **No browser tax in Runner.** HTML/CSS/browser embedding is not a default requirement.
10. **Explicit persistence.** App-local, project-authored, and live-transient state never mix silently.
11. **Qualification is implementation.** Mockups/screenshots alone complete nothing.
12. **Token/test efficiency.** Repository contracts and narrow tests prevent repeated rediscovery.

## Dependency graph

```text
Immediate core gate
#38 Core recovery -----------------------------------------> broad skin work allowed

Parallel evidence lane
#30 Native SoundSwitch capture ----------------------------> #34 exact visual freeze

Platform lane
#31 Command/state facade --------┐
mock contract/examples ----------┼-> #37 Toolkit spike -> #32 Skin runtime + Safe
                                 │                           |
                                 └---------------------------┘
                                                             v
                                                   ┌---------+---------┐
                                                   v                   v
                                             #33 Default          #34 Reference
                                                   └---------+---------┘
                                                             v
                                              #35 persistence/connection closeout
                                                             v
                                                   #36 qualification evidence
                                                             v
                                                #39 custom controls later
```

#30, planning, #31 behavior-preserving extraction, and #37 bounded spikes may run beside #38. Final broad skin implementation waits for the core-ready gate.

## Stage gates

### G0 — Planning and evidence contract — complete

Evidence:

- architecture, product journeys, brand, screenshot plan, observation ledger;
- Win32 inventory;
- command/state/binding/manifest/layout schemas;
- package limits and native-component contracts;
- issue/branch/test sequence.

### G-Core — Immediate hardware and operational readiness

Owner: #38.

Required before broad Default/Reference skin work:

- raw Micro response and fixture truth;
- visible/usable Connections with desired/saved/applied/active outcomes;
- deterministic OS2L startup/listener behavior;
- shared Static Look Toggle/Hold/feedback/Autoloop-return semantics;
- installed evidence tied to the exact build.

UI agents consume these service commands/states; they do not change adapter framing or hide unqualified hardware claims.

### G1 — Typed command/state facade

Owner: #31.

Deliverables:

- registry metadata matching machine-readable schemas;
- typed invocation API and explicit outcomes;
- bounded coherent state snapshots/subscriptions;
- current Win32 menus/accelerators/controls routed through facades;
- Command Explorer and State Explorer;
- no new direct callback bypass;
- bypass ledger for temporary exceptions.

Mandatory first slices:

1. blackout/stop/work light/hazard disarm/Release All/BPM/tap;
2. Looks/Autoloops/Track Scripts and active/progress state;
3. OS2L/controller/output health/reconnect;
4. project lifecycle/Undo/validation/migration utilities;
5. authoring CRUD;
6. view/navigation cleanup.

Gate:

- current behavior preserved;
- unknown/invalid/unavailable/safety/queue/reconnect/restart/IO results explicit;
- F8 retains non-droppable blackout;
- no project/lighting semantic change.

### G-T — Toolkit qualification

Owner: #37. Final recommendation reconciles with #31.

Candidates:

- Slint/C++ first;
- WinUI 3/C++ control;
- Win32/Direct2D Safe baseline;
- Qt only if evidence triggers it.

Use the committed Safe, Default Live/Studio, and Reference Live/Studio layout examples and dense synthetic component fixtures.

Gate:

- product-shaped, equivalent measurements;
- Windows DPI/accessibility/input/custom drawing/deployment;
- startup/memory/CPU/repaint/jitter;
- licensing/modules/runtime recorded;
- toolkit does not leak into public skin contracts.

### G2 — Skin runtime and Safe fallback

Owner: #32. Requires G1 and G-T; broad implementation also respects G-Core.

Deliverables:

- `.emberskin` package/manifest/layout/theme/localization/binding support;
- package and resource limits;
- immutable bounded view graph;
- responsive containers and ordinary primitives;
- native-component adapters;
- state subscriptions and typed bindings;
- semantic theme tokens;
- trusted Safe surface;
- transactional load/reload/switch;
- structured diagnostics/cache/provenance;
- package fuzz/resource-abuse suite.

Gate:

- invalid first load reaches Safe;
- invalid reload preserves current skin;
- no code/path/device/network access;
- no safety bypass;
- no Runner/output/content reset;
- hidden panels suspend expensive subscriptions;
- package/component/runtime remains toolkit-neutral publicly.

### G3 — EmberLights Default v0

Owner: #33. Requires G2 and G-Core.

Deliverables:

- Project Hub/startup/recovery routing;
- Studio with Library, canvas, Inspector, waveform, and utility drawers;
- Live Home/Autoloops/Static Looks/Moments/Overrides;
- pinned gig-health/safety strip;
- all existing functions migrated or bounded legacy bridges;
- EmberLights design tokens/assets;
- complete journeys from doc 24.

Gate:

- workflows pass at 1366×768 and 1920×1080/high DPI;
- all visible action/state is registry-backed;
- keyboard/UIA/non-color/reduced-motion behavior;
- Runner-neutral activation;
- measured footprint/repaint.

### G4 — SoundSwitch Reference v0

Owner: #34. Requires G2/G-Core; exact token freeze requires Tier A #30 evidence.

Deliverables:

- familiar Studio and Performance landmarks;
- four-bank/32-slot window over 64×32 catalog;
- selected/active/progress/queued/filter/exclusive/repeat distinctions;
- explicit Static Look RELEASE/SET/FORCE_ZERO;
- performance overrides/content/group intensities;
- permanent health/safety/emergency controls;
- evidence-tagged deviation ledger;
- original EmberLights assets.

Gate:

- parity journeys pass for implemented capabilities;
- identical domain outcomes versus Default/mappings;
- no proprietary SoundSwitch assets;
- compact/standard/wide/high-DPI qualification;
- skin switching active DMX is neutral.

### G5 — Connection and persistence UX

Owner: #35. Can inventory early; closes after skins consume it.

Deliverables:

- definitive setting-scope registry;
- immediate persistence where safe;
- exact reconnect/restart/save/validation outcomes;
- configured-source auto-connect/reconnect;
- actionable DJ/controller/U1/U2/USB/safety drawers;
- last-project reopen/recovery;
- explicit transient-to-project promotion only where safe.

Gate:

- restart/reopen scope matrix;
- controlled disconnect/reconnect without Runner stop;
- no machine-local identity leak;
- no routine Live modal;
- hazard arming fail-closed.

### G6 — Qualification

Owner: #36.

Use `26_UI_QUALIFICATION_MATRIX.md` for:

- registries/bindings/packages/components;
- goldens and resolutions/DPI;
- keyboard/UIA/accessibility;
- persistence and faults;
- cross-surface equivalence;
- skin failure/DMX continuity;
- startup/memory/CPU/repaint/jitter;
- installed and hardware evidence;
- machine-readable release reports.

### G7 — End-user custom controls — later

Owner: #39. Requires stable G1–G6 evidence.

First milestone:

- keyboard/MIDI/HID binding editor;
- designated custom panel slots;
- Button/Toggle/Pad/Fader/Knob/Status/Progress;
- typed command/state selection;
- labels/icons/accents/size/accessibility;
- responsive pages/pad banks;
- immutable base skin + validated overlay;
- export/import/reset/relink.

V0 custom control invokes one registered command. Full layout overlays, Action Sets, visual skin designer, and community distribution follow later.

## Agent ownership and parallelism

### Allowed

- #30 beside #31/#37/#38;
- registry/schema harnesses beside #38 when behavior is preserved;
- #35 scope inventory before visual drawers;
- Default/Reference package authoring in parallel after G2/G-Core when file ownership is separate;
- qualification harness design before final skins.

### Disallowed

- two agents inventing competing command IDs;
- Reference-only or Default-only domain paths;
- cosmetic rewrite of `windows_app.cpp` before facade;
- toolkit selection by preference/mockup;
- skin scripting before Safe/package limits;
- broad skin implementation that displaces #38;
- ordinary queue replacement for blackout/priority safety;
- fake/dead controls for unsupported engine capability.

## Branch and merge protocol

Suggested branches:

```text
ui/evidence-corpus
ui/command-state-facade
ui/toolkit-spike
ui/skin-runtime
ui/default-skin-v0
ui/reference-skin-v0
ui/connection-persistence
ui/qualification
ui/custom-controls
```

Rules:

1. #31 owns command/state names until merged.
2. #37 owns spike code/evidence, not production domain semantics.
3. #32 owns package/runtime paths.
4. Skin agents own package/layout/theme/assets, not runtime behavior.
5. #35 owns services/scopes, not duplicate skin logic.
6. #36 owns qualification baselines and cannot silently bless regressions.
7. Schema/ID changes include compatibility/deprecation or explicit pre-public reset.
8. Rebase dependent branches after contract changes.

## Required completion report

Each agent leaves:

- bounded deliverable/issue/gate;
- files/contracts owned and changed;
- commands/states/schema/components changed;
- exact tests and results;
- measurements/evidence;
- unresolved risks;
- legacy bridges;
- docs/issues updated;
- next dependency.

“Looks good” is not completion evidence.

## Token and compute efficiency

1. Read only the route in `00_START_HERE.md` and `AGENT_BOOTSTRAP_PROMPT.md`.
2. Do not repeat existing SoundSwitch research unless closing a named evidence gap.
3. Persist compact registries/reports rather than pasting generated output into chat.
4. Run narrow tests per change; full suites at merge gates.
5. Run affected goldens during development; full matrix at G6.
6. Run long soak only at qualification/release/hardware gates.
7. Do not create throwaway UI paths outside the facades.
8. Do not ask the owner to retest unchanged mechanics because presentation moved.
9. Keep summaries to decisions, code, evidence, risks, and next dependency.
10. Preserve screenshots as research evidence, not duplicated skin assets.

## Definition of completion

The UI program is complete when the installed app can reopen a project, connect the qualified DJ/controller/output paths, run through either bundled skin, switch/fail/recover skins without output interruption, execute identical commands from UI/keyboard/MIDI, diagnose faults from Live, preserve correct persistence scopes, and stay inside the Runner performance and safety envelope.
