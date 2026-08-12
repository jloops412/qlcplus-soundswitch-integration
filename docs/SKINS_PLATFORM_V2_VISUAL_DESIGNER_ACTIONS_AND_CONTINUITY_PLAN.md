# Skins Platform V2 — Visual Designer, Ember Actions, Import/Export, and Continuity Plan

Status: **binding architecture and product plan**.

Owner direction: EmberLights must support user-created/imported/exported skins, a powerful visual skin-building experience, and a robust VirtualDJ-inspired action system that remains current as the application evolves.

This document extends the accepted modular UI program. It does not replace the command/state facade, `.emberskin` runtime, Safe fallback, bundled Default/Reference skins, or existing package limits.

## 1. Executive decision

EmberLights will implement a registry-governed control-surface platform with five separable artifacts:

1. **Skin package** — responsive presentation, theme, localization, assets, component configuration, and bindings.
2. **Overlay** — user customization layered over an immutable base skin.
3. **Ember Action** — bounded typed composition of registered commands and state predicates.
4. **Controller profile** — MIDI/HID/keyboard input and feedback mapping independent of skin and project content.
5. **Designer source project** — editable Studio-side authoring source that compiles to one or more distributable artifacts.

The public action feature is called **Ember Actions**. The optional expert textual representation is called **Ember Action Script**. Both the visual Action Graph and text representation compile to the same canonical Action IR. Neither can execute arbitrary code, access devices directly, own show timing, or bypass the command/state/safety architecture.

The command, state, component, capability, theme-token, and interaction registries are the long-lived platform ABI. They are machine-readable, versioned, generated, diffed, tested, and part of every user-visible feature change.

## 2. Why this direction

### 2.1 Useful VirtualDJ model

VirtualDJ demonstrates several valuable product ideas:

- skins are replaceable packages rather than one fixed interface;
- the same action vocabulary is reused by skins, keyboard shortcuts, controller mappings, custom buttons, and pad pages;
- users can create multiple mappings and interfaces for different workflows;
- simple actions can be composed into macros;
- controller learn and action discovery reduce some mapping friction.

The official VirtualDJ documentation also shows the limitations EmberLights should avoid:

- skin authoring remains primarily ZIP/XML/image editing;
- users often manage files and action strings manually;
- action semantics can be terse and weakly typed;
- hidden variables and ad-hoc macros can become difficult to inspect, migrate, or validate;
- advanced controller feedback may require separate definition files;
- visual layout, mapping, and script authoring are not one coherent editor.

### 2.2 Useful SoundSwitch model

SoundSwitch remains the primary workflow reference for EmberLights V1. Current official behavior relevant to the Reference skin includes:

- Performance Mode provides Autoloop control synchronized to transport/Beatgrid;
- four Autoloop banks with 32 slots each form a familiar 128-loop performance window;
- active Autoloops expose progress feedback;
- manually triggered Autoloops can play over scripted tracks and return naturally;
- Infinite and Track Duration repeat modes are visible performance concepts;
- Static Looks are useful event scenes and can be MIDI-mapped as direct live triggers;
- Performance status and connection visibility matter in high-pressure use.

EmberLights should reproduce the familiar mental model where it is effective, improve weak areas, and expose the same behavior through a modular package that users can fork or customize.

## 3. Current repository truth

The project already has the correct foundation:

- issue #29 defines a modular UI/control-surface platform;
- issue #31 owns the typed command/state facade;
- issue #32 owns the `.emberskin` runtime, validator, immutable view graph, and Safe fallback;
- issues #33 and #34 own bundled Default and SoundSwitch Reference skins;
- issue #36 owns UI/package/switch/accessibility/performance qualification;
- issue #39 defines the first custom controls and overlay milestone;
- `spec/ui/command-state-skin-contract-v0.md` defines one behavior path;
- `spec/ui/user-customization-and-action-composition-v0.md` defines customization levels and a bounded future Action Set;
- `spec/ui/emberskin-package-and-safety-limits-v0.md` treats skin packages as untrusted data;
- native `UiCommandId`, `UiCommandFacade`, `UiStateDefinition`, and `LiveCoreUiState` now exist, but remain a narrow implementation slice rather than the final generated registry system.

Skins Platform V2 fills the remaining product and governance gaps:

- a complete visual authoring experience;
- an expert text representation that round-trips safely;
- a canonical typed action IR and executor;
- import/export and source-adapter architecture;
- registry lifecycle and compatibility automation;
- mandatory cross-agent maintenance rules;
- distribution and migration diagnostics.

## 4. Product experiences

### 4.1 Quick Customize

A normal operator can customize a bundled or third-party skin without understanding package files:

1. Enter Customize mode from an approved surface.
2. Select an existing customizable region or create a custom page.
3. Drag in a Button, Toggle, Pad, Fader, Knob, Label, Status, Progress, Meter, Separator, or approved native component.
4. Search commands, states, actions, content targets, or reusable templates.
5. Configure typed arguments through appropriate pickers and ranges.
6. Choose label, icon, semantic accent, size, span, focus order, and accessible name.
7. Optionally use keyboard/MIDI/HID Learn and feedback Learn.
8. Preview Ready, Active, Selected, Queued, Unavailable, Fault, and Safety-Rejected states.
9. Validate compact, standard, wide, high-DPI, and touch variants.
10. Save as an app-local overlay or explicitly associate a recommended overlay with a project.

The base skin remains immutable and resettable.

### 4.2 Full Skin Designer

An advanced user can create or fork a complete `.emberskin` through an integrated visual environment:

- hierarchy/tree and visual canvas views;
- responsive Row, Column, Grid, WrapGrid, Dock, Stack, Overlay, SplitPane, Drawer, Tabs, and Scroll containers;
- grid/snap/guides/alignment/distribution and constraint editing;
- breakpoint, size, DPI, touch, and input-mode previews;
- reusable components and template instances;
- native component configuration rather than rebuilding complex editors from primitives;
- semantic theme-token editor with color, spacing, typography, elevation, shape, focus, warning, status, content, and safety roles;
- color chooser, palette tools, gradients where supported, contrast checks, and content-color separation;
- bundled icon browser plus validated licensed SVG/image import;
- localization editor and string-key coverage;
- command/state/capability/component explorers;
- Action Graph editor and expert text view;
- keyboard/MIDI/controller binding and feedback editor;
- simulated-state scenarios and safe live preview;
- focus, accessibility, target-size, mandatory-control, performance-budget, and package-limit diagnostics;
- Undo/Redo, autosave/recovery, source history, diff, and merge against base-skin updates;
- deterministic validation, packaging, preview generation, import, and export.

### 4.3 SoundSwitch-familiar Performance template

The SoundSwitch Reference skin and designer templates must provide a familiar live workspace without freezing EmberLights to one layout:

- permanent project, DJ/sync/BPM, output-universe, controller, safety, Work Light, Release All, Stop, and Blackout visibility;
- Performance/Overrides page;
- Autoloops page with a familiar four-bank/32-pad visible window over EmberLights' larger 64-bank × 32-slot library;
- clear selected, queued, active, progress, completed-cycle, repeat, overlay/replace, enabled-bank, pending-exclusive-bank, and fault states;
- Static Looks page with Toggle, Hold, release/ownership feedback, folders/tags/search, and event-safe organization;
- Moments/quick-actions page for event-specific user controls without adding engine-specific event modes;
- configurable group/master/script/Autoloop intensity and approved movement/color/strobe/position controls;
- compact, standard, wide, and touch-live variants;
- original EmberLights artwork and explicit evidence/deviation records.

Users may fork this package, add pages, replace visual tokens, change layouts, or build a different workflow while preserving command semantics.

## 5. System architecture

```text
Editable designer source
  -> schema validation
  -> base skin / overlay / action / profile source artifacts
  -> command + state + component + capability resolution
  -> compatibility and safety validation
  -> deterministic compilers
       - layout/theme -> immutable View Graph
       - actions -> immutable Action IR
       - mappings -> immutable Binding Tables
  -> hidden candidate instantiation and smoke checks
  -> transactional activation
  -> event -> binding -> optional Action IR -> typed command facade
  -> authoritative Studio service or Runner boundary
  -> registered state snapshot -> UI/controller feedback
```

### 5.1 Separation of concerns

| Concern | Authority |
| --- | --- |
| Lighting semantics and layer ownership | Core/domain services and Runner |
| Musical timing, quantization, repeat, transitions | Runner/domain commands |
| Project mutation and Undo/Redo | Studio document services |
| Adapter I/O | DJ/controller/output adapter services |
| User event interpretation | Binding engine |
| Small action composition | Ember Action executor |
| Layout, style, localization, visibility | Skin/view runtime |
| Editing, import, package construction | Studio-side Skin Designer |
| Safety, priority, validation, persistence | Core registries and services |

A skin or action never writes a fixture channel, accesses a MIDI port, edits project storage directly, or schedules DMX frames.

### 5.2 Runtime process boundary

The lean Perform process loads compiled artifacts only:

```text
validated View Graph
validated Action IR
validated Binding Tables
bounded state subscriptions
native component adapters
```

It does not load:

- the visual designer;
- source-format importers;
- XML/ZIP migration analyzers beyond the bounded runtime package reader;
- an arbitrary language runtime;
- a browser engine;
- source control/history services;
- image editors or asset-generation tools;
- AI providers.

Designer crash/exit/restart must not affect Runner or current DMX output.

## 6. Registry-governed platform ABI

### 6.1 Registries

The platform publishes versioned machine-readable definitions for:

- commands;
- states;
- components;
- capabilities;
- theme tokens;
- interaction/event types;
- argument/value types;
- invocation results;
- action node kinds;
- persistence scopes;
- safety classes;
- realtime/update classes.

### 6.2 One source of truth

Accepted definitions must originate from one canonical source that can generate:

- native C++ IDs/types/lookup tables;
- JSON registry snapshots for validation and tooling;
- JSON Schemas or schema references;
- Command/State/Component/Capability Explorer data;
- designer property controls and pickers;
- compatibility/deprecation maps;
- human-readable reference documentation;
- test vectors and cross-reference manifests.

Generated artifacts are deterministic and checked in CI. The DMX scheduler never parses the full registry JSON.

### 6.3 Required feature-change reconciliation

Every user-visible feature PR must answer:

- Which command creates/changes the behavior?
- Which state proves the authoritative result?
- Which capability describes availability?
- Which primitive/native component can expose it?
- Which persistence and Undo scope applies?
- Which safety and realtime class applies?
- Which bundled skins or Safe surface need exposure or explicit non-exposure?
- Which mappings/actions/examples/tests need updates?
- Is this additive, compatible, deprecated, replaced, or breaking?

CI rejects registry drift, stale generated artifacts, unknown references, and unapproved breaking changes.

## 7. Ember Actions

### 7.1 Purpose

Ember Actions provide the useful composition role of VDJScript while improving safety, discoverability, typing, diagnostics, portability, and migration.

A typical user does not type code. The Action Graph editor creates the canonical action. Expert users may edit a textual representation with completion, type checking, documentation, and lossless round-trip.

### 7.2 Canonical model

An action declares:

- stable action ID, version, label, description, author/provenance;
- compatible action-schema and registry ranges;
- input parameters with types, ranges, defaults, and semantic target kinds;
- one or more supported surface entry points;
- required commands, states, capabilities, and content targets;
- bounded graph nodes and edges;
- feedback outputs;
- persistence and export scope;
- resource budget and validation digest.

The canonical stored form is a normalized graph/AST, not raw source text.

### 7.3 V1 node set

| Node | Meaning |
| --- | --- |
| `InvokeCommand` | Invoke one registered command with typed arguments. |
| `Sequence` | Execute children in deterministic order. |
| `Parallel` | Submit independently safe commands as a bounded group; never implies atomicity unless the domain exposes it. |
| `If` | Branch on an approved side-effect-free predicate. |
| `Switch` | Select one bounded branch from an enum/value. |
| `MapValue` | Clamp/scale/invert/curve/map an input value. |
| `Let` | Define an immutable typed value for one invocation. |
| `ReadState` | Snapshot an approved registered state for predicates/arguments. |
| `InvokeAction` | Reuse another validated action; dependency graph must remain acyclic and bounded. |
| `Return` | End with an explicit result/feedback value. |
| `OnResult` | Branch on typed command result such as Accepted, Unavailable, SafetyRejected, or QueueFull. |

No arbitrary loop, recursion, dynamic function creation, reflection, eval, filesystem/network/device operation, or shell/native extension is allowed.

### 7.4 Entry points

Initial surface events:

```text
onPress
onRelease
onValue
onLongPress
onDoublePress
onEncoderStep
onActivate
onDeactivate
```

Gesture recognition belongs to the bounded binding engine. Action execution does not sleep while waiting to determine a gesture.

State changes drive feedback expressions, not arbitrary command-trigger loops in V1. Future event automation requires a separately qualified trigger policy with rate limits, cycle detection, and explicit ownership.

### 7.5 Values and context

Supported values are explicit and bounded:

- boolean;
- signed/unsigned integer with range;
- finite number with range/unit;
- enum;
- bounded UTF-8 string;
- color/palette token;
- stable project/content ID with target kind;
- semantic role or capability selector where the command supports it;
- bounded list/object defined by an approved schema;
- surface input value and contextual item values.

There are no untyped global variables. V1 allows:

- immutable invocation-local values;
- declared bounded surface-local state for pages/modifiers where presentation ownership is appropriate;
- registry-backed authoritative state reads;
- explicit app-local/project-authored persistence only through registered commands/services.

Missing content targets become unavailable and relinkable. They are never redirected by list position or name guess.

### 7.6 Timing

Ember Actions do not use UI sleeps or repeated timers as show timing.

Requests such as:

- launch on next beat/bar/phrase;
- run for N beats;
- repeat until track end;
- transition over a musical interval;
- return at the natural Autoloop boundary;
- schedule a moment sequence;

must be represented by typed domain command arguments or a compiled project/Runner asset. The action requests the operation; Runner owns timing and publishes progress/state.

Binding-level long-press, double-press, debounce, and soft-takeover timers are bounded input interpretation only.

### 7.7 Execution classes

The compiler classifies each action by the strongest referenced operation:

- `viewLocal`;
- `studioMutation`;
- `runnerCommand`;
- `runnerPriority`;
- `utilityAsync`;
- `blockingForbiddenLive`.

An action cannot hide a blocking utility inside an ordinary performance pad. Mixed-class actions are rejected or require explicit approved boundaries.

Studio mutations may execute within one declared Studio transaction when all referenced commands support it. Runner commands are not claimed atomic merely because they appear in one action. A new domain-level atomic operation requires a registered domain command.

### 7.8 Results and diagnostics

Every invocation produces a typed result with node-level diagnostics. At minimum:

```text
Accepted
NoChange
Unavailable
InvalidArguments
MissingTarget
ValidationFailed
QueueFull
SafetyRejected
Unsupported
Cancelled
StartedAsync(operationId)
InternalError
```

User surfaces can map results to consistent non-modal feedback, controller LEDs, labels, badges, and diagnostics. Safety rejection and queue pressure are never silent.

### 7.9 Compilation and budgets

Source artifacts compile before activation into immutable bounded IR.

Provisional V1 limits:

- 64 nodes per action;
- 8 branch depth;
- 8 referenced actions and 8 call depth;
- 16 registered state reads;
- 16 immutable local values;
- 8 parallel children per node;
- 32 command invocations per surface event;
- 32 KiB normalized action definition;
- acyclic dependency graph;
- fixed instruction/stack budget;
- no allocation or source parsing on the DMX scheduler.

Limits are measured and may be tightened. Raising them follows the package limit-change policy.

### 7.10 Expert text representation

Ember Action Script is optional and secondary to the canonical IR.

Requirements:

- parser and formatter round-trip without semantic loss;
- registry-aware completion and inline documentation;
- explicit types and named arguments;
- no implicit command lookup by display label;
- no hidden global variable namespace;
- no text eval at runtime;
- exact diagnostics with source ranges;
- formatting is deterministic;
- import/export stores normalized IR plus optional source for author convenience;
- compiler output digest must match equivalent visual graph output.

A conceptual example:

```text
action wedding.firstDance(look: StaticLookId) {
  require runner.state == running

  on press {
    staticLook.activate(lookId: look)
  }

  on release {
    staticLook.clear(owner: thisBinding)
  }

  feedback active = staticLook.active.id == look
}
```

The final grammar is subordinate to the typed contract and cannot introduce behavior the graph/IR cannot represent.

## 8. Skin Designer contract

### 8.1 Authoring levels

The same designer shell supports progressive complexity:

- **Level 0 — Bindings:** keyboard/MIDI/HID/feedback editor.
- **Level 1 — Custom Controls:** approved slots and pages in an overlay.
- **Level 2 — Layout Overlay:** move/resize/show/hide approved regions and define responsive variants.
- **Level 3 — Full Skin:** create/fork complete packages, themes, components, actions, localization, and assets.

Users should not have to abandon their Level 1 work to advance to Level 3. Overlay source can be promoted/forked into a full designer project with preserved IDs and provenance.

### 8.2 Canvas and layout

The designer edits the toolkit-neutral layout contract, not toolkit-specific controls.

Required tools:

- canvas plus hierarchy/tree;
- drag/drop and keyboard editing;
- grid, snap, guides, rulers, alignment, distribution, equal-size, and spacing tools;
- responsive constraints, min/max/preferred sizing, spans, wrapping, docking, and overflow behavior;
- breakpoint/variant inheritance and explicit overrides;
- zoom, pan, selection, multi-select, grouping, reusable components, and symbols/templates;
- focus order and tab-navigation editing;
- visibility and availability predicates;
- data/context repeaters backed only by bounded approved state collections;
- live component outline and state-subscription inspection;
- diff between variants and base-skin versions.

Absolute positioning may be supported inside a bounded artboard/overlay component, but ordinary application layout remains responsive and constraint-driven.

### 8.3 Component library

The palette exposes:

- ordinary primitives from the `.emberskin` contract;
- trusted native complex components through stable properties/events/state interfaces;
- reusable authored components;
- product templates such as health strip, Autoloop matrix, Static Look grid, Override panel, connection drawer, inspector, project header, and custom pad page.

The designer must not encourage users to manually recreate a 2,048-slot Autoloop catalog or timeline from thousands of primitive controls when a virtualized native component exists.

### 8.4 Theme, color, icons, and assets

Theme editing is semantic first:

- backgrounds/surfaces;
- primary/secondary/accent;
- content categories;
- selected/active/queued/focus;
- success/info/warning/danger/fault;
- blackout/work-light/safety/hazard;
- text and muted text;
- borders/dividers/elevation;
- spacing, shape, target size, and typography scale.

The UI provides color pickers, swatches, palette generation, contrast checks, state previews, and token usage search. Content colors remain separate from application status/safety colors.

V1 uses approved bundled/system typography and bounded static SVG/raster assets. Custom assets require license/provenance metadata and package validation. External URLs and executable formats remain prohibited.

### 8.5 Binding and feedback editor

For each control, the designer can:

- select an interaction compatible with the command;
- choose a command or existing Ember Action;
- configure typed arguments and stable targets;
- bind context values and input transforms;
- configure press/release/value/long/double/modifier behavior;
- select registered state feedback for active, selected, queued, progress, value, availability, warning, and fault;
- map controller LED/color/text feedback through the same state expressions;
- inspect conflicting software/keyboard/controller bindings;
- launch MIDI/HID Learn and feedback Learn;
- preview invocation results and error states.

### 8.6 Simulation and live-safe preview

The designer includes deterministic scenarios:

- project unloaded/loaded/invalid;
- Runner stopped/starting/running/fault;
- DJ disconnected/connecting/connected/holding/fallback;
- outputs healthy/degraded/fault;
- Autoloop selected/queued/active/progress/repeat/overlay/replace;
- Static Look active/held/released/missing;
- overrides active/released;
- blackout/work light/hazards;
- content empty/large/missing/relinked;
- command Accepted/Unavailable/SafetyRejected/QueueFull.

Simulation never invokes live commands. A separate explicit Test action may invoke the real command through normal safety and availability gates. Hazardous tests require the ordinary arm/interlock/confirmation path.

### 8.7 Save, recovery, and activation

Designer edits operate on source documents with:

- deterministic IDs;
- Undo/Redo;
- autosave and recovered draft state;
- source history and compare;
- transactional compile/validate;
- preview candidate distinct from active package;
- explicit Save, Validate, Package, Activate, Export, and Publish-later operations;
- failed activation retaining the current skin;
- reset returning exactly to the immutable base package.

## 9. Import, export, and migration

### 9.1 Common adapter pipeline

```text
Read-only source artifact
  -> source/version detector
  -> source-specific evidence parser
  -> normalized migration IR
  -> command/state/component/action mapping
  -> exact/translated/approximated/opaque/unsupported/conflicted report
  -> user review and target relink
  -> validated Ember source project or package proposal
  -> transactional install/activation
```

Source artifacts are never executed and never mutated.

### 9.2 VirtualDJ adapter

A future VirtualDJ adapter may ingest user-authorized:

- skin ZIP/XML/image packages;
- keyboard mappings;
- controller definition/mapping XML;
- custom buttons and pad-page action definitions where exportable.

It may:

- import compatible visual structure into a draft artboard/layout;
- preserve source dimensions, IDs, coordinates, assets, and action strings as evidence;
- map known VDJScript verbs to registered Ember commands/actions through explicit versioned rules;
- translate compatible MIDI/HID controls into controller-profile proposals;
- preserve unknown expressions and source data as opaque attachments;
- report unsupported DJ/audio concepts rather than inventing lighting semantics.

It must not:

- execute VDJScript;
- claim exact behavior from a display label or image;
- redistribute artwork the user is not licensed to redistribute;
- silently map unrelated deck/audio functions to lighting commands;
- turn vendor-specific names into core engine concepts.

### 9.3 SoundSwitch workflow migration

SoundSwitch does not expose a general user skin package equivalent. EmberLights therefore provides:

- a bundled original SoundSwitch Reference skin;
- Reference-derived designer templates and custom slots;
- migration of user project/content data through the existing evidence-first source adapters;
- optional import of user-owned screenshots only as locked visual reference layers in the designer, never as distributable skin artwork by default;
- explicit Preserve/Improve/Reject decisions and evidence labels.

### 9.4 Other sources

WOLFMIX, QLC+, Lightkey, ShowXpress, Daslight, and other future sources use separate read-only adapters into the same migration IR and canonical Ember artifacts. They never create source-specific runtime engines.

### 9.5 Export bundles

Export UI distinguishes:

- full `.emberskin`;
- `.emberoverlay`;
- `.emberaction` or action pack;
- controller profile;
- designer source project;
- optional project recommendation manifest;
- diagnostic compatibility report.

It discloses included scopes, dependencies, assets/licenses, local identity, project targets, and machine-specific bindings. Paths, serials, secrets, and local device identity are redacted by default.

## 10. Compatibility and lifecycle

### 10.1 Version dimensions

Artifacts declare compatible ranges for:

- application version;
- skin schema;
- layout schema;
- binding schema;
- action schema;
- command registry generation;
- state registry generation;
- component registry generation;
- capability registry generation;
- required native-component contract versions.

### 10.2 Stable IDs and deprecation

- IDs are never silently reused for different semantics.
- Additive optional fields and capabilities remain compatible.
- Deprecation declares `since`, `deprecatedSince`, replacement, type-conversion rule, warning level, and planned removal generation.
- Automatic replacement is allowed only when argument/result/state semantics are provably compatible.
- Breaking changes require an explicit registry-major decision and migration tooling.
- Bundled skins and examples must be clean against the current registry before release.
- Previous supported registry fixtures remain in compatibility tests.

### 10.3 Base-skin updates and overlays

Overlay update flow reports:

- preserved controls/regions;
- moved/renamed base IDs with compatible aliases;
- changed properties or slot contracts;
- missing commands/states/components/targets;
- conflicts requiring choice;
- optional new features;
- unsupported/deprecated actions;
- exact fallback if migration is rejected.

An overlay can always be disabled without changing the base skin or project content.

## 11. Safety, security, and fault containment

All existing `.emberskin` package restrictions remain binding and extend to actions/designer projects.

Additional rules:

- mandatory Live controls and health visibility are checked after base + overlay + action resolution;
- F8 emergency Blackout is app/core owned and cannot be shadowed;
- priority commands retain their independent delivery path;
- hazardous commands expose metadata and remain subject to domain safety gates;
- action graphs cannot persist hazard arming;
- imported artifacts are quarantined until full validation;
- invalid first load reaches Safe; invalid reload keeps the current surface;
- optional component failure is isolated; mandatory Live component failure prepares Safe before swap;
- source parsers and image decoders run outside the scheduler and in bounded workers where practical;
- designer preview does not hold output/device handles;
- all failures produce structured correlation IDs and actionable diagnostics.

## 12. Performance and resource model

### Perform

- compiled package/action/binding artifacts only;
- visibility-aware state subscriptions;
- no source parsing or designer services;
- no mandatory browser or general script VM;
- bounded action execution on a control/UI service, never the DMX scheduler;
- fixed queues and explicit QueueFull results;
- current Runner CPU/memory/jitter ceilings remain authoritative.

### Studio/Designer

- richer memory budget but still bounded;
- source indexing, asset processing, validation, migration, and preview on cancellable workers;
- expensive hidden panels stopped/throttled;
- large components virtualized;
- compile and activation candidates generation-stamped;
- designer closure/crash leaves active Runner unchanged.

Qualification measures parse/compile/activation memory, action dispatch latency, subscription/repaint cost, dense designer behavior, repeated import/switch, and scheduler jitter beside realistic output load.

## 13. Persistence and scope

### App-local

- installed skins and overlays;
- personal theme/accessibility preferences;
- keyboard mappings;
- machine/controller matching hints;
- designer recent files and workspace layout;
- local trust/provenance decisions.

### Controller profile

- reusable input/output control identities, layers, transforms, and feedback mappings.

### Project-authored or recommended

- optional preferred skin/overlay/profile IDs;
- project content targets and reusable event pages/actions where intentionally authored;
- semantic-role bindings intended to travel with the project.

A project recommendation cannot force-install or activate an untrusted package without owner review.

### Live transient

- current page, focus, selection, modifier/layer, soft-takeover pickup, active content, and current overrides.

Live transient state is not silently persisted into projects or exported profiles.

## 14. Accessibility and usability

The designer and runtime enforce:

- accessible names and descriptions;
- keyboard-only authoring and operation alternatives;
- focus-order preview and no traps;
- minimum target sizes by variant/input mode;
- text and non-text contrast;
- non-color selected/active/queued/fault feedback;
- reduced motion;
- high contrast and DPI behavior;
- icon-only warnings;
- mandatory emergency-control reachability;
- localized strings and truncation/overflow checks;
- touch-live qualification.

Generated controls receive sensible accessibility defaults from command/state metadata, but authors may refine them.

## 15. Qualification matrix

### Registry and generation

- uniqueness and stable IDs;
- schema and generated-native parity;
- deterministic generation;
- required metadata completeness;
- deprecation/replacement validation;
- no unknown references in bundled artifacts;
- registry diff and compatibility report fixtures.

### Action system

- parser/formatter/visual round-trip;
- graph cycle/depth/node/instruction limits;
- typed argument and state validation;
- interaction and realtime-class compatibility;
- deterministic command order;
- result/error branches;
- missing target and capability degradation;
- no timing authority leak;
- no device/file/network access;
- UI/control-thread budget and queue pressure;
- action dependency migration across registry versions.

### Designer

- overlay and full-skin journeys;
- Undo/Redo/autosave/recovery;
- responsive variants and inheritance;
- theme/token/icon/asset/localization flows;
- keyboard/MIDI Learn and feedback;
- state simulation and explicit live-safe test;
- base-skin update/merge/conflict;
- export/import on another machine;
- malformed/oversized/source-parser abuse;
- accessibility and performance.

### Runtime

- Default/Reference/Safe equivalence;
- active DMX skin switch/reload/failure;
- repeated rapid switch and cache corruption;
- hidden subscription throttling;
- mandatory controls and F8;
- low-end Windows and VirtualDJ co-load;
- installed package/provenance behavior.

### Migration

- source immutability and hashes;
- version-specific parser fixtures;
- exact/translated/approximated/opaque/unsupported/conflicted classifications;
- unknown source retention;
- deterministic re-import and no duplication;
- asset/license disclosure;
- redaction and cross-machine install.

## 16. Work packages

### SKIN2-000 — Continuity, decisions, backlog, and contracts

- create this planning package;
- add registry maintenance rules to agent entrypoints;
- create the GitHub epic and work issues;
- reconcile existing #29/#39 rather than superseding them;
- no production code.

### SKIN2-001 — Registry authority, code generation, and drift gates

- canonical command/state/component/capability source;
- generated C++ and JSON artifacts;
- schema/docs/explorer generation;
- registry diff, compatibility, deprecation, and coverage CI;
- feature-PR checklist and direct-bypass enforcement;
- reconcile current `UiCommandId`/`UiStateDefinition` and issue #31 ownership.

### SKIN2-002 — Ember Action IR, compiler, executor, and diagnostics

- action schema and canonical normalization;
- typed validation and registry resolution;
- graph compiler and immutable IR;
- bounded control-thread executor;
- result/diagnostic/feedback model;
- action import/export and expert parser/formatter;
- no general designer beyond a developer graph fixture initially.

### SKIN2-003 — Binding editor, Custom Controls, overlays, and pad pages

- execute and expand issue #39 Level 0/1;
- declared custom slots;
- visual control/property/binding editor;
- MIDI/HID/keyboard Learn and feedback;
- overlay compile/validate/activate/reset/import/export;
- action selection and simple Action Graph editing after SKIN2-002;
- compact/standard/touch variants.

### SKIN2-004 — Full responsive Skin Designer

- complete canvas/tree/constraints/components/theme/assets/localization;
- fork/create full `.emberskin` packages;
- source history, base-skin diff/merge, templates, simulation, packaging;
- Studio process isolation and performance qualification.

### SKIN2-005 — Reference templates and advanced performance authoring

- SoundSwitch-familiar Performance/Autoloop/Static Look/Override/Moment templates;
- user-forkable original assets and semantic tokens;
- full current Autoloop V2 state contract and live tools;
- parity/deviation evidence and golden scenarios;
- coordinate rather than duplicate issue #34.

### SKIN2-006 — Cross-source skin/mapping migration adapters

- normalized UI/mapping/action migration IR;
- VirtualDJ skin/mapping/custom-control importer first where source evidence permits;
- source-preserving reports and relink UX;
- later source adapters only after the first reusable seam is proven.

### SKIN2-007 — Distribution, compatibility qualification, and release gates

- installed package manager and side-by-side versions;
- trust/provenance/license reports;
- compatibility scanning and migration preview;
- full security/fuzz/DPI/accessibility/performance/DMX-continuity matrix;
- community catalog/marketplace remains a separately approved product decision.

## 17. Acceptance for the overall program

- A user can create a responsive custom Live surface without editing files.
- A user can fork the SoundSwitch Reference skin and modify layout, controls, colors, icons, pages, and mappings.
- A control can bind to one command or a validated Ember Action with typed arguments and feedback.
- The same command/action works consistently from software, keyboard, MIDI/controller, and approved external surfaces.
- Expert text and visual Action Graph round-trip to identical canonical IR and digest.
- Musical timing remains domain-owned and action execution cannot degrade DMX scheduling.
- Full skin, overlay, action, and controller-profile import/export work with explicit scope and redaction.
- Registry updates are generated and CI-enforced; stale skins/actions/mappings are detected before merge/release.
- Compatible app updates preserve working artifacts; incompatible changes produce exact migration diagnostics.
- Invalid or malicious artifacts cannot execute code, access devices/files/network, bypass safety, or displace a working Live surface.
- Safe, Default, and Reference remain available and functionally equivalent at the domain boundary.
- Source adapters preserve evidence and never claim unsupported vendor behavior.

## 18. Explicit non-goals

- a general-purpose programming language;
- plugins or arbitrary code inside skins;
- direct DMX/channel scripting from skin actions;
- UI timers as cue/show scheduling;
- a browser/Electron dependency in Perform;
- copying proprietary SoundSwitch/VirtualDJ artwork or source code;
- automatic semantic conversion of every third-party DJ skin control;
- forcing one fixed SoundSwitch-like interface on all users;
- public marketplace before signing, licensing, moderation, update, and support policy exists;
- broad implementation that displaces the active hardware/core-ready gates.

## 19. Decisions left intentionally open for measured implementation

- final toolkit selected by issue #37 evidence;
- exact canonical registry source format and generator technology;
- final Ember Action Script grammar;
- final designer-source project extension/container;
- whether advanced source parsers run in a separate sandboxed process;
- signing/trust UX before public distribution;
- exact action and designer resource limits after representative benchmarks;
- how long deprecated registry generations remain supported before a major migration.

These choices cannot weaken the architecture, safety, compatibility, or maintenance invariants above.
