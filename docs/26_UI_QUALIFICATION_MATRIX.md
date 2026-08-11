# Modular UI Qualification Matrix

Status: binding qualification plan for issue #36 and release-gate integration.

Related:

- `21_UI_IMPLEMENTATION_PROGRAM.md`
- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `25_EMBERLIGHTS_UI_DESIGN_SYSTEM_AND_BRAND_DIRECTION.md`
- `../spec/ui/emberskin-package-and-safety-limits-v0.md`
- `../spec/ui/native-component-contracts-v0.md`

## Qualification objective

Prove that the modular UI architecture is functionally equivalent across surfaces, safe during failure, usable at required Windows resolutions/DPI/input methods, accessible, and small enough to coexist with the deterministic Runner.

Mockups and ordinary unit tests are necessary but insufficient. Release evidence must cover behavior, persistence, safety, timing, memory, accessibility, and failure continuity.

## Evidence tiers

### Tier U — unit evidence

Pure command/state/schema/package/component logic without a real window or Runner.

### Tier C — component evidence

One UI component in deterministic harness data across states, sizes, themes, and input paths.

### Tier I — integration evidence

Real command/state facades, UI runtime, bundled skin, persistence services, and representative Runner/service process.

### Tier S — system evidence

Installed Windows build with real windowing, DPI, controller/input/output adapters or qualified simulations, package switching/failure, and production-like Runner load.

### Tier H — hardware/user evidence

Owned DJ controller, MIDI/controller, DMX interface, fixture rig, multiple monitors/DPI, and task-based user rehearsal.

Public/gig-qualified UI claims require the applicable Tier S/H evidence, not only U/C/I.

## Required environments

### CI

- current supported Windows runner;
- Linux core/schema logic where portable;
- deterministic headless package/registry/component tests;
- installer build and installed startup smoke.

### Reference Windows machines

1. **Low-end qualification machine**
   - integrated graphics;
   - modest dual/quad-core CPU;
   - 8 GB RAM target;
   - 1366×768 / 100% display where available.

2. **Reference DJ laptop**
   - owner’s normal VirtualDJ environment;
   - 1920×1080 / 100% or recorded actual scaling;
   - DDJ-REV7 / Control One where relevant;
   - SoundSwitch Micro and other supported outputs.

3. **High-DPI machine/display**
   - 2560×1440 / 150%;
   - 3840×2160 / 200% where available.

Every report records CPU, GPU, RAM, OS/build, display, scale, renderer, framework/runtime, app build, project fixture count, and enabled adapters.

## Command registry tests

| Test | Tier | Required result |
| --- | --- | --- |
| unique stable IDs | U | no duplicates/case collisions |
| schema/metadata completeness | U | scope, parameters, interaction, realtime, persistence, safety, availability, Undo present |
| typed parameter validation | U | invalid/missing/out-of-range rejected exactly |
| deprecation/replacement | U | warning and safe migration behavior |
| unavailable command | U/I | explicit `unavailable`; no state mutation |
| safety rejection | U/I | explicit `safetyRejected`; core policy authoritative |
| queue full/busy | I/S | explicit result; emergency command unaffected |
| invocation acknowledgement | I | accepted/pending/noChange reflected in state/result |
| command explorer listing/search | C/I | correct metadata and binding display |
| surface equivalence | I/S | same command+arguments produce same domain result |

## State registry tests

| Test | Tier | Required result |
| --- | --- | --- |
| unique keys and type stability | U | no collision/type drift |
| update class enforcement | U/I | no subscription exceeds metadata policy |
| snapshot generation/consistency | I | related values are coherent for one generation |
| hidden subscription suspension | C/I | high-rate work drops when hidden |
| stale generation handling | U/C | stale data rejected/identified |
| progress/transport source | I/S | Runner state is authority; UI timer not authority |
| reconnect/fault transitions | I/S | complete state machine visible |
| accessible state text | C/S | status understandable without color |

## Binding tests

- keyboard press/release/repeat;
- MIDI note, CC, pitch, absolute, relative encoder;
- momentary/toggle/latch behavior;
- soft takeover;
- scaling, inversion, curve, range;
- bank/target parameters;
- outbound feedback and resynchronization;
- conflict detection;
- unavailable target after package change;
- binding import/export/version migration;
- user overlay reset;
- emergency binding cannot be silently shadowed.

## Package validator tests

Execute every abuse/failure case in `emberskin-package-and-safety-limits-v0.md`, including:

- path traversal/absolute/UNC/drive/symlink/reparse;
- compression bomb and excessive decoded size;
- file/path/nesting/count limits;
- invalid/case-colliding duplicate files;
- malformed/oversized JSON/UTF-8/localization;
- component cycles/depth/expansion limits;
- excessive widgets/subscriptions/bindings;
- huge/invalid raster and SVG assets;
- unknown/deprecated commands/states/tokens/components;
- incompatible command/widget interactions;
- unsafe command requests;
- hash/schema/app-version mismatch;
- missing required asset/capability/variant;
- cache corruption;
- repeated concurrent reload request;
- package removed/changed during read;
- simulated allocation failure.

Required behavior:

- no crash/hang/path escape/code execution;
- structured exact diagnostic;
- current skin retained or Safe activated according to transaction state;
- Runner/DMX uninterrupted.

## Native component matrix

Every component in `native-component-contracts-v0.md` must test:

- loading/empty/ready/unavailable/degraded/error/read-only/editing;
- minimum/standard/wide sizes;
- 100/125/150/200% DPI;
- Default and Reference theme mappings;
- keyboard/focus/context actions;
- UI Automation roles/names/states/values;
- maximum bounded data;
- virtualization/culling;
- accepted/rejected command feedback;
- stale document/state generations;
- hidden/visible subscription cost;
- content colors under contrast normalization;
- golden screenshots where appropriate;
- memory/CPU/scroll/pan/input latency.

### Component-specific stress sets

| Component | Stress fixture |
| --- | --- |
| Asset Browser | 10,000 rows, search/filter/sort/favorite |
| Fixture Browser | large indexed corpus, quarantine/errors |
| Venue/Patch | 256 fixtures, two universes, overlaps/errors |
| Track Hierarchy | 256 tracks, deep groups, mute/solo |
| Timeline | dense cue/curve set, pan/zoom/playhead |
| Waveform | long cached waveform, zoom/seek/missing audio |
| Autoloop Matrix | 64×32 catalog, active offscreen bank, progress |
| Static Look Editor | many targets/properties/ownership states |
| Mapping Editor | multiple ports, conflicts, learn stream |
| Diagnostics | bounded large event list and active metrics |
| Migration Review | mixed confidence/conflict/unsupported corpus |

## Bundled skin golden states

Generate golden screenshots with deterministic project/state fixtures.

### Dimensions

- 1366×768 / 100%;
- 1920×1080 / 100%;
- 2560×1440 / 150%;
- 3840×2160 / 200%;
- touch-live target where available.

### Default skin states

- Project Hub/continue;
- Studio empty project;
- Studio normal project;
- profile/patch/group selection;
- Static Look ownership edit;
- Autoloop editor;
- Track Script/timeline skeleton;
- Mapping Learn;
- Migration Review;
- Live healthy Home;
- Live Autoloop active/progress;
- Static Look active;
- Moment active;
- override active count;
- DJ hold/fallback/recovery;
- output fault;
- blackout/work light;
- save failed/recovery draft;
- invalid skin fallback.

### Reference skin states

- Studio normal SoundSwitch-familiar hierarchy;
- Master/Group/Fixture selection;
- Autoloop Edit banks;
- Static Look Edit;
- fixture library/patch;
- Performance healthy;
- Autoloops selected/active/progress/exclusive/repeat;
- Static Looks active;
- overrides/faders;
- source selector;
- DJ/output connected/disconnected;
- notification/fault;
- blackout/work light;
- compact/wide variations.

### Golden review rules

- goldens test layout/state, not ownership of SoundSwitch artwork;
- exact Reference comparisons use only approved native evidence regions/measurements;
- anti-aliasing/render-backend differences require bounded tolerance or semantic image masks;
- expected image updates require explicit review and reason;
- never update every golden automatically to make CI pass;
- preserve a textual semantic snapshot alongside the image where possible.

## Responsive and DPI tests

For each required variant:

- no clipped critical control;
- no overlapping text/control;
- focus ring fully visible;
- minimum target size by input mode;
- correct asset/icon scale;
- correct pixel alignment/hairlines;
- window moved between monitors/scales;
- runtime DPI change and resize;
- compact fallback selection;
- focus/page/bank/selection preservation;
- Safe surface operable below normal Studio size.

## Accessibility tests

### Automated/manual inspection

- Windows Accessibility Insights or equivalent UIA inspector;
- Narrator basic traversal and activation;
- keyboard-only execution of critical journeys;
- focus order and restoration;
- accessible name/role/state/value/range;
- status announced without excessive live-region noise;
- non-color state communication;
- contrast checks for ordinary text/control boundaries;
- content-color contrast normalization;
- reduced motion;
- high contrast/Booth High Contrast theme;
- color-vision deficiency simulations;
- no hover/right-click-only critical action.

### Critical keyboard journey

A keyboard-only user must be able to:

- open/reopen/save a project;
- switch Studio/Live;
- inspect health;
- launch/clear a Look and Autoloop;
- Release All;
- activate Work Light;
- invoke/release Blackout;
- open Diagnostics;
- switch to Safe/Default after skin failure;
- exit safely.

## Persistence tests

Matrix every setting/state against restart, project reopen, skin switch, package activation, and machine change.

### App-local expected persistence

- last project/reopen preference;
- skin/variant override;
- window/monitor/panel layout;
- recent paths;
- device/path matching hints;
- reduced-motion/theme/accessibility preferences.

### Project-authored expected persistence

- venue/fixtures/groups/profiles;
- Looks/Autoloops/scripts/mappings as designed;
- logical connections/output routing;
- safety policy limits;
- project-preferred layout only when explicitly authored.

### Live-transient expected non-persistence

- active content;
- overrides;
- blackout/work light unless deliberately required for process lifetime only;
- hazard arming;
- temporary intensities;
- reconnect/backoff state.

Required checks:

- no machine-local serial/path leaks into portable project unintentionally;
- no transient state silently saved;
- exact reconnect/restart indicator;
- save failure leaves dirty/recovery state;
- restore history and recovery draft remain intact across UI migration.

## Connection/fault tests

### OS2L/DJ

- disabled, waiting, connected, stale, hold, fallback, recovery, fault;
- wrong bind/port;
- reconnect while Live;
- same-PC and LAN configurations;
- project switching with compatible/incompatible endpoint change;
- no pad press required for EmberLights-side auto-listening; VirtualDJ-specific configuration is represented accurately.

### MIDI/controller

- no ports;
- recognized port/profile;
- unknown controller;
- hot unplug/replug;
- multiple simultaneous devices;
- feedback output failure;
- Learn cancellation/timeout/conflict;
- mapping target disappears after package change.

### Outputs

- Art-Net/sACN/USB/Micro disabled, starting, waiting, ready, fault, reconnecting;
- one universe fault while the other remains ready;
- stale frame supersession;
- shutdown zero/safe behavior;
- controlled device unplug/replug;
- adapter error details and safe action;
- skin/drawer activity does not alter adapter lifecycle.

## Cross-surface equivalence suite

Representative commands:

```text
show.start/stop
output.blackout.set(true/false)
output.workLight.toggle
override.releaseAll
transport.manualBpm.set
autoloop.launch/clear
autoloop.bankFilter.selectExclusive/enableAll
staticLook.activate/clear
trackScript.start/clear
group override set/release
connection.os2l.reconnect
project.save
```

Invocation sources:

- legacy Win32 adapter while present;
- EmberLights Default;
- SoundSwitch Reference;
- keyboard;
- MIDI test profile;
- direct registry test;
- future external adapter fixture when available.

Compare:

- invocation result;
- resulting domain state;
- shared state snapshot;
- visible/feedback state;
- persistence and Undo effects;
- safety behavior;
- log/diagnostic record.

## Skin-switch and failure continuity

Test while Runner is active and representative frames are being produced:

- Default → Reference → Default;
- Default → Safe;
- valid local skin;
- invalid first load;
- invalid reload;
- missing asset;
- unknown command/state;
- oversized package;
- renderer/component simulated failure;
- repeated rapid switch requests;
- DPI/monitor change during switch;
- project activation concurrent with UI validation, serialized according to policy.

Required:

- no show-package reload;
- no active content reset;
- no DMX stop or stale replay;
- no missed emergency blackout;
- no hazard-policy bypass;
- current view or Safe always available;
- structured exact error.

## Performance tests

### Metrics

- process cold start median/p95;
- time to Safe interactive;
- time to preferred skin interactive;
- resident/private working set;
- peak activation memory;
- idle CPU and UI wakeups;
- active 30/60 Hz progress CPU;
- dense timeline pan/zoom CPU/GPU/frame time;
- diagnostics log scrolling;
- input-to-command-facade latency;
- state-publication-to-feedback latency;
- skin validate/compile/switch times;
- repeated-switch memory stability;
- scheduler jitter/deadline misses during every UI load.

### Scenarios

1. Safe Live idle.
2. Default Live healthy.
3. Reference Live healthy.
4. Autoloop progress at 30 Hz.
5. Multiple health state changes.
6. Diagnostics open.
7. Default Studio normal.
8. Studio dense synthetic project.
9. Timeline continuous pan/zoom.
10. Skin validation/switch/reload/failure.
11. Hidden/minimized UI.
12. 30-minute active repaint.
13. four-to-six-hour representative Live soak.

### Product ceilings

Existing product requirements remain authoritative:

- Runner UI target under 75 MB and ceiling 125 MB;
- cold start target under 2 seconds and ceiling 4 seconds;
- complete two-universe system CPU target under 2%, ceiling 5%, on reference machine;
- MIDI/input and visible-DMX latency ceilings remain unchanged;
- DMX scheduling jitter p99/ceiling remain unchanged.

UI qualification reports its share and total-process effect rather than claiming a separate unlimited budget.

## Test cadence for token/compute efficiency

### Per change

- affected unit tests;
- affected component state/golden at one standard size;
- startup smoke if window/runtime paths changed.

### Per PR

- complete affected component matrix;
- package/schema tests when relevant;
- compact + standard screenshots;
- short representative performance run;
- cross-surface subset.

### Merge gate

- Windows/Linux core tests as applicable;
- installed GUI smoke;
- all affected resolution/DPI goldens;
- complete cross-surface suite;
- package failure continuity;
- performance comparison against baseline.

### Release/qualification gate

- all resolutions/DPI/themes;
- full accessibility/manual inspection;
- fuzz/abuse suite;
- complete performance matrix;
- long Live soak;
- real VirtualDJ/MIDI/DMX hardware journeys;
- machine-readable release evidence.

Do not run full golden matrices or long soaks after unrelated text/icon tweaks unless the relevant acceptance surface changed.

## Machine-readable evidence

Recommended report shape:

```json
{
  "schemaVersion": 1,
  "build": {},
  "machine": {},
  "display": {},
  "renderer": {},
  "skin": {},
  "scenario": "reference-live-active-progress",
  "durationSeconds": 300,
  "metrics": {},
  "assertions": [],
  "screenshots": [],
  "logs": [],
  "result": "pass"
}
```

Reports include source commit, project fixture, package hashes, command/state registry versions, skin schema/version, toolkit/runtime version, and test command.

## Release blockers

Any of the following blocks UI qualification:

- critical operation exists only in one bundled skin;
- domain results differ across surfaces;
- skin failure stops/resets output;
- blackout/emergency path can be dropped or hidden;
- app/project/transient persistence crosses scope silently;
- required compact/high-DPI control is clipped/unreachable;
- accessibility tree/focus prevents critical keyboard use;
- package validator permits path/code/resource abuse;
- current UI exceeds product ceilings without an accepted mitigation;
- exact SoundSwitch visual claim lacks evidence tag/source;
- proprietary SoundSwitch asset is included;
- toolkit license/deployment obligations are unresolved;
- qualification evidence exists only as prose.

## Completion

Issue #36 closes only when:

1. both bundled skins and Safe surface pass applicable matrices;
2. command/state/binding/package/component contracts are versioned;
3. cross-surface equivalence passes;
4. installed Windows performance/accessibility/failure evidence is committed;
5. hardware journeys are recorded or remain explicit release gates;
6. release-gate documents and parity ledger are updated with evidence links.
