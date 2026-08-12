# EmberLights Skin Platform v2 — Ember Actions, Skin Studio, and Migration-Ready UI

Status: **binding planning proposal for the skin-platform lane**. This document extends—rather than replaces—the accepted modular UI program in `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`, `21_UI_IMPLEMENTATION_PROGRAM.md`, and `spec/ui/command-state-skin-contract-v0.md`.

Planning baseline: `main@f166a582b24972c6762022fd956ba868d2aae1cd` on 2026-08-12.

## 1. Executive decision

EmberLights will have a genuine user-authorable skin platform, not a set of hard-coded themes.

The platform takes the most valuable idea from VirtualDJ—**one shared action vocabulary used by skins, custom controls, pad pages, keyboard mappings, and controller mappings**—and replaces its manual bitmap/XML/script workflow with:

- a visual grid/constraint editor;
- reusable primitive and domain components;
- semantic theme tokens and state styling;
- searchable typed commands and observable state;
- integrated keyboard/MIDI/HID Learn and feedback;
- a bounded visual action graph;
- an optional expert text view over the same graph;
- deterministic validation, preview, packaging, import, export, and migration reports;
- a compiled, immutable Perform artifact that remains independent of Studio and the DMX scheduler.

The public product concepts are:

- **Skin Studio** — the visual authoring environment;
- **Ember Actions** — the typed action/binding system;
- **`.emberskin`** — a complete portable skin package;
- **`.emberoverlay`** — non-destructive user customization layered over a base skin;
- **controller profiles** — independent hardware mappings that use the same actions and state but do not live inside a skin.

A skin is presentation, layout, input binding, and feedback. It is never an alternate lighting engine, timing engine, fixture engine, project format, or safety authority.

## 2. Product outcome

A user must eventually be able to:

1. Start from EmberLights Default, SoundSwitch Reference, a blank template, or an imported/forked package.
2. Add, remove, resize, align, group, dock, and reorder controls without editing files.
3. Choose colors, typography roles, spacing, borders, icons, and state appearances.
4. Add prebuilt lighting components such as Autoloop matrices, Static Look matrices, override racks, group faders, connection status, waveform/timeline, libraries, inspectors, and diagnostics.
5. Bind a control to a command through generated parameter editors rather than memorizing syntax.
6. Define active, queued, disabled, fault, progress, warning, and availability feedback from shared state.
7. Learn keyboard/MIDI/HID input and configure output feedback from the same control editor.
8. Create reusable custom pages, pad banks, toolbars, panels, and component templates.
9. Preview multiple resolutions, DPI scales, input modes, capabilities, connection failures, and live-content states.
10. Validate, package, import, export, update, fork, reset, and compare skins safely.
11. Switch skins during an active show without stopping output, recompiling the show, resetting active content, or changing controller profiles.
12. Inspect and repair compatibility when commands, state, components, projects, or base skins evolve.
13. Migrate supported concepts from other software through explicit adapters and a normalized migration report.
14. Use optional expert syntax without creating a second behavior model or requiring a general-purpose script VM in Perform.

## 3. Non-negotiable boundaries

### 3.1 One command model

Every user-facing action routes through the typed command facade. No skin callback, native component, controller profile, keyboard shortcut, DJ command, imported binding, or future remote surface may implement separate domain behavior.

### 3.2 One state model

Controls observe registered state snapshots. They do not infer playback progress, connection health, active content, safety, or timing from local UI timers.

### 3.3 Runner remains authoritative and lean

- The DMX scheduler never parses skins, runs a user script VM, paints UI, loads assets, or performs migration.
- Perform may dispatch prevalidated actions and render a compiled view, but Studio authoring services remain absent or dormant.
- Musical waits, fades, quantization, and timed playback are domain/Runner features, not UI-thread delays.
- Skin loading, validation, editing, and asset decoding occur away from the scheduler.

### 3.4 Safety is platform-owned

A skin may place and style approved safety controls, but it cannot replace their command implementation, hide all emergency paths, intercept the independent blackout route, persist hazard arming, or suppress fault state.

### 3.5 Untrusted package model

All imported and locally authored packages are untrusted data. They cannot execute arbitrary JavaScript, Lua, WebAssembly, native code, shell commands, DLLs, shaders, filesystem calls, network requests, device enumeration, or direct MIDI/USB operations.

### 3.6 Original assets and lawful migration

SoundSwitch Reference recreates familiar information architecture with EmberLights-owned assets. A migration adapter may inspect user-supplied source files and preserve provenance, but EmberLights does not redistribute proprietary source artwork, personalities, scripts, or packages.

## 4. Relationship to current work

This program builds on concrete repository foundations already present:

- `UiCommandFacade` and explicit invocation results;
- registered Live commands for show, blackout, Work Light, hazards, Static Looks, Autoloops, Track Scripts, bank filtering, and fixture/group overrides;
- `LiveCoreUiState` and `LiveViewModel` snapshots;
- schemas for commands, state, bindings, manifests, layouts, controller profiles, and examples;
- package safety/resource limits;
- Default, Reference, and Safe example layouts;
- accepted Studio/Runner separation;
- issue #32 for the runtime and Safe fallback;
- issues #33 and #34 for bundled Default and Reference skins;
- issue #39 for the first bounded customization milestone;
- issue #56 for the lean Perform process and resource budgets.

The new work fills four gaps:

1. a complete machine-readable component/command/state design catalog;
2. a robust typed action language shared by every surface;
3. a production-grade visual Skin Studio and package lifecycle;
4. a standard migration IR and adapter SDK.

## 5. Reference products: preserve, improve, reject

### 5.1 VirtualDJ

Preserve conceptually:

- separate skin presentation and action behavior;
- shared actions across software controls, pad pages, keyboard, and hardware;
- user-created custom buttons and pages;
- skin forking and package portability;
- conditional visibility and state-driven appearance;
- broad controller mapping freedom.

Improve deliberately:

- use typed command parameters and generated pickers;
- make visual authoring the primary path;
- use responsive layout rather than bitmap coordinates by default;
- provide one integrated hierarchy, grid, theme, action, feedback, MIDI, preview, and package tool;
- generate exact compatibility diagnostics;
- preserve immutable base skins and store user edits as overlays;
- provide package provenance, hashes, deterministic builds, and tests;
- separate domain timing from UI timers;
- use native complex components instead of asking authors to rebuild a timeline or matrix from pixels.

Reject:

- unrestricted text as the only authoring interface;
- implicit value typing and ambiguous coercion;
- arbitrary loops, recursion, dynamic evaluation, and uncontrolled repeat/wait behavior;
- manual ZIP/XML/bitmap management as the normal workflow;
- fixed-resolution layout as the platform default;
- silent partial translation of foreign scripts;
- controller/device access from a skin.

### 5.2 SoundSwitch

Preserve:

- clear Studio versus Perform mental models;
- familiar Performance, Autoloops, Static Looks, and Live-control landmarks;
- bank/pad operation and active-progress feedback;
- global performance overrides and group intensity controls;
- MIDI Learn and controller feedback;
- immediately visible DJ, hardware/output, BPM, and connection health.

Improve:

- unlimited product capacity behind pageable control-surface windows;
- user-configurable pages and panels;
- active content remains visible even when its page is not selected;
- exact selected/active/queued/progress/repeat/filter/exclusive distinctions;
- actionable diagnostics without leaving Perform;
- responsive layouts and touch variants;
- explicit persistence scopes;
- command/state introspection on every control;
- user-created layouts that may remain SoundSwitch-familiar or diverge completely.

### 5.3 QLC+, TouchOSC, and Companion

Use as secondary UX references for:

- edit versus operate modes;
- drag/drop widget palettes;
- hierarchy/layers, grids, snapping, alignment, and rulers;
- live synchronized preview;
- state-driven feedback and conditional styling;
- importing/exporting selected pages or control sets;
- multiple actions with explicit order and failure behavior.

Do not inherit a general-purpose runtime scripting environment or permit action cycles that can consume unbounded CPU.

## 6. Platform architecture

```text
Engine/domain capabilities
        |
        v
Generated Command Registry + State Registry + Capability Registry
        |
        +-------------------------+
        |                         |
        v                         v
Component Catalog            Ember Actions
(properties/events/slots)    (typed, bounded action graph)
        |                         |
        +------------+------------+
                     v
              .emberskin source
       layout + theme + actions + bindings
                     |
                     v
       validate -> type-check -> resolve -> compile
                     |
                     v
     immutable Compiled Skin Artifact + compatibility lock
                     |
          +----------+-----------+
          |                      |
          v                      v
   Perform renderer         Keyboard/MIDI/HID/DJ bindings
          |                      |
          +----------+-----------+
                     v
              Typed Command Facade
                     |
                     v
                 Runner/domain
                     |
                     v
             Shared State Snapshots
```

### 6.1 Registry and catalog layer

The machine-readable design catalog contains:

- command definitions and typed parameter schemas;
- command categories, availability, result classes, safety, persistence, Undo, and feedback metadata;
- state definitions, value types, update classes, privacy, and formatting metadata;
- capabilities and reasons for absence;
- component definitions, versions, properties, events, slots, state/data dependencies, and designer controls;
- theme-token definitions and required state roles;
- bundled icon metadata;
- target picker metadata for Looks, Autoloops, groups, fixtures, properties, adapters, panels, and other stable IDs.

There must be one canonical source of truth that generates or verifies both C++ types and JSON tooling metadata. Hand-maintained duplicate registries are not accepted.

### 6.2 Skin source layer

A source skin remains toolkit-neutral. It uses semantic components and layout constraints, not Slint, WinUI, Qt, Win32, CSS, or renderer-specific object names.

### 6.3 Compiler layer

The compiler:

1. reads through a bounded package interface;
2. validates paths, archive limits, hashes, schemas, assets, licenses, and provenance;
3. resolves localization and theme tokens;
4. type-checks commands, state, actions, components, and data contexts;
5. checks capability fallbacks and required variants;
6. resolves stable target references where project-specific preview is requested;
7. compiles predicates, bindings, and action graphs into bounded immutable instructions;
8. builds state-subscription and focus/navigation graphs;
9. enforces mandatory health/safety reachability;
10. runs layout, accessibility, and package-local scenario tests;
11. emits a deterministic compatibility lock and compiled artifact;
12. activates only after a hidden candidate view passes smoke checks.

### 6.4 Perform layer

Perform consumes only the validated artifact and approved runtime catalogs. It does not ship the full visual editor or migration adapters in the lean path.

A source skin may be compiled during installation or in Studio, then cached by:

```text
package content hash
+ skin schema version
+ action language version
+ command/state/component signature hashes
+ renderer compiler version
```

Cache failure triggers bounded recompilation or Safe fallback; it never stops current output.

## 7. Package model

Proposed canonical source package:

```text
MySkin.emberskin
  manifest.json
  compatibility.lock.json
  styles/
    theme.json
  layouts/
    live.compact.json
    live.standard.json
    live.wide.json
    live.touch-live.json
    studio.standard.json
  components/
    reusable/*.json
  actions/
    actions.json
  bindings/
    skin-bindings.json
  assets/
    icons/*.svg
    images/*.{png,webp}
  locales/
    en-US.json
  previews/
    preview-manifest.json
  tests/
    scenarios.json
```

### 7.1 `.emberskin`

A complete portable skin. It may contain Project Hub, Studio, Live, Safe-compatible, secondary-surface, or future remote layouts, subject to declared capabilities and variants.

### 7.2 `.emberoverlay`

A separate package layered over an immutable base skin. It may contain:

- added controls/pages/components in declared slots;
- approved layout changes;
- theme overrides;
- action definitions;
- binding changes;
- compatibility metadata and the exact base version/hash used.

Reset removes the overlay and returns exactly to the base package.

### 7.3 Controller profiles

Device messages, matchers, layers, soft takeover, and hardware feedback remain in reusable controller profiles. Skin Studio may edit them in context, but a skin never opens a device or owns device identity.

### 7.4 Package fragments

The first release should avoid a proliferation of public extensions. Selected pages, component templates, themes, and action sets export as a scoped overlay or as content copied into a full skin. A community bundle format may be added only after lifecycle and dependency rules are proven.

### 7.5 No external dependencies in the first public schema

A v1 skin is self-contained apart from registered platform components/commands/state and its declared base skin for an overlay. Third-party component-package dependencies are deferred until signing, resolution, versioning, offline caching, and support policies exist.

## 8. Responsive layout and surfaces

### 8.1 Default layout model

Use responsive containers and constraints:

- Row, Column, Grid, WrapGrid, Stack;
- Dock, SplitPane, Panel, Drawer, Scroll, Overlay;
- minimum/maximum/content/fill sizing;
- flex grow/shrink;
- named grid tracks, spans, gaps, alignment, and distribution;
- compact, standard, wide, touch-live, and user-defined bounded variants;
- DPI-aware sizing and semantic density tokens.

### 8.2 Visual grid

Skin Studio presents an approachable grid editor without reducing the runtime to one fixed grid. Authors can:

- select 4/8/12/16/24-column design grids;
- snap to tracks, edges, centers, baselines, and spacing tokens;
- use auto-layout rows/columns inside grid cells;
- nest grids and components;
- convert free placements into constraints;
- preview overflow and fallback behavior.

### 8.3 Legacy fixed-canvas bridge

Migration may require a bounded `LegacyCanvas`/`AbsoluteLayer` container:

- only in explicitly marked legacy variants;
- finite logical dimensions and node count;
- scale-to-fit/letterbox behavior;
- no external assets or code;
- mandatory platform safety overlay remains outside it;
- designer offers a guided conversion to responsive constraints.

New skins should not default to this model, but it prevents a VirtualDJ-style importer from making every fixed-coordinate source unusable.

### 8.4 Multiple surfaces

The public contract should not assume one window forever. Manifest entry points need stable `surfaceId` metadata so future skins can define:

- main Studio/Perform window;
- secondary monitor surface;
- controller mirror;
- kiosk/touch surface;
- authenticated remote surface later.

V1 may render only the main surface, but schema and IDs must not block expansion.

## 9. Component system

### 9.1 Primitive controls

- Button, Toggle, Pad;
- Fader, Knob, XY control;
- Meter, Progress, ValueField;
- Label, Icon, StatusBadge;
- Tabs, SegmentedControl, MenuButton;
- List, Tree, Table, search/filter;
- Spacer, Separator, frame/panel.

### 9.2 Domain components

- GigStatusStrip;
- SafetyDock;
- DJConnectionStatus;
- OutputHealthStrip;
- NowPlaying/ActiveContent;
- AutoloopMatrix and bank/page navigator;
- StaticLookMatrix;
- MomentPage;
- PerformanceOverrideRack;
- GroupFaderBank;
- Color/attribute override components;
- TimelineEditor;
- WaveformBeatgrid;
- LibraryBrowser;
- Inspector;
- Fixture/Profile/Patch components;
- MappingEditor;
- ConnectionsPanel;
- DiagnosticsPanel;
- PreviewVisualizer;
- MigrationReport.

Domain components expose stable properties, events, slots, commands, state, and data contexts. A skin controls composition and appearance but does not recreate internal domain logic.

### 9.3 Component versions

Every public component has:

- stable type ID;
- semantic version or integer contract version;
- typed properties/events/slots;
- required/optional commands, state, capabilities, and data sources;
- performance/update class;
- accessibility contract;
- designer metadata;
- migration/deprecation metadata.

## 10. Skin Studio UX

### 10.1 Modes

- **Design** — layout, hierarchy, styles, components, assets;
- **Actions** — command bindings, action graphs, targets, gestures;
- **Feedback** — state-driven appearance, text, icons, values, progress;
- **Inputs** — keyboard/MIDI/HID/controller Learn and conflict resolution;
- **Preview** — deterministic simulation or connected dry-run;
- **Package** — validation, compatibility, provenance, tests, import/export.

A clear Edit/Operate boundary prevents accidental live command invocation while arranging controls.

### 10.2 Workspace

Recommended default:

- left: component/library palette and package assets;
- center: one or more responsive artboards;
- right: contextual inspector;
- bottom/optional: hierarchy, actions, diagnostics, test trace, and compatibility drawers;
- top: variant/device/DPI/state scenario selectors, Undo/Redo, preview, validate, and publish.

### 10.3 Inspector sections

For every selected element:

1. Identity and reusable-component role;
2. Layout and responsive constraints;
3. Appearance and semantic tokens;
4. Content, labels, icons, localization;
5. Action/event bindings;
6. Feedback/state rules;
7. Input/MIDI/HID bindings;
8. Visibility, availability, and capabilities;
9. Accessibility and focus;
10. Compatibility, performance, and provenance.

### 10.4 Theme editor

Theme editing is token-first:

```text
surface.*
text.*
border.*
action.*
status.*
selection.*
focus.*
pad.*
timeline.*
waveform.*
spacing.*
radius.*
font.*
size.*
motion.*
```

The UI supports color pickers, numeric controls, token aliases, gradients where approved, state matrices, contrast checks, and live previews. Raw per-widget overrides remain possible but are flagged when they undermine consistency.

Required visual states include:

- normal, hover, pressed, focused, disabled;
- selected, active, queued, holding;
- progress and completed;
- healthy, connecting, fallback, warning, fault;
- blackout and hazard states.

### 10.5 Icon and asset picker

- searchable bundled vector icon catalog;
- domain categories and recommended command icons;
- original/licensed SVG, PNG, and WebP import;
- dimension, decode, SVG-complexity, and package-budget validation;
- license/source/provenance fields;
- no remote runtime assets;
- no custom executable fonts, shaders, or animation formats in v1.

### 10.6 Command and State Explorer

The editor generates controls from registry metadata:

- category and search;
- name, description, stable ID, version;
- workspace and surface scope;
- compatible interaction/control types;
- typed parameters and target pickers;
- current availability and missing-capability reason;
- safety, persistence, Undo, and real-time class;
- feedback state and suggested visual states;
- current skin/keyboard/MIDI/controller bindings;
- test/trace where safe.

### 10.7 Feedback editor

Feedback is declarative and independent of action invocation. Authors can map state/predicates to:

- active/selected/queued/disabled/warning/error roles;
- text and secondary text;
- icon and badge;
- progress/meter value;
- visibility and enablement;
- accessible state announcement;
- MIDI LED/ring/display feedback through the controller-profile layer.

### 10.8 MIDI/controller integration

From a selected control, the user may choose **Learn MIDI/HID**. Skin Studio then:

1. selects or creates a controller profile;
2. captures the logical input;
3. chooses press/release/value/relative behavior;
4. configures transform, dead zone, inversion, pickup, and soft takeover;
5. configures hardware feedback from registered state;
6. detects conflicts and layer/modifier interactions;
7. tests without embedding device-specific access in the skin.

### 10.9 Preview and simulation

Built-in scenarios include:

- no project, loading, saved/unsaved, validation fault;
- Runner stopped/starting/running/fault;
- DJ disconnected/connecting/exact/holding/audio fallback/manual;
- 60/120/180 BPM and active phase/progress;
- output healthy/reconnecting/fault for U1/U2;
- controller absent/present/multiple;
- no active content, active Static Look, Autoloop queued/active/progress/repeat, Track Script active;
- overrides active;
- blackout, Work Light, hazard armed/fault;
- missing command/state/component/target/capability;
- compact through 4K at 100/125/150/200% DPI;
- mouse/keyboard, touch, and controller-first input.

Connected preview never turns the design canvas into timing authority. Live command tests are explicit, gated, traced, and use the same safety rules.

### 10.10 Undo, autosave, fork, and compare

- full bounded Undo/Redo for source-document edits;
- autosaved recovery drafts separate from explicit package versions;
- **Fork** creates a new ID and provenance chain;
- **Customize** creates an overlay where possible;
- **Reset** removes the overlay;
- visual and semantic diff by stable component/action/token ID;
- base-update three-way merge with exact conflicts.

## 11. Ember Actions summary

The detailed contract is in `spec/ui/ember-actions-v1.md`.

Core principles:

- canonical typed graph/AST, not free-form text;
- visual editor, package JSON, and optional expert syntax compile to the same graph;
- registered commands and state only;
- explicit parameter and result types;
- bounded sequence/branch/fallback/call composition;
- no cycles, recursion, arbitrary code, direct I/O, or unbounded timers;
- direct safety controls remain direct platform bindings;
- musical timing compiles to domain/Runner plans, never UI `wait` loops;
- deterministic trace and compatibility diagnostics;
- direct-command bindings remain a compatible shorthand.

## 12. SoundSwitch Reference and Perform templates

### 12.1 Bundled SoundSwitch Reference

The Reference skin must make a migrating operator immediately productive while proving the platform can express a materially different UI.

Reference Perform should expose:

- familiar Performance/Autoloops/Static Looks/Live Tools pages;
- project/venue, DJ source, BPM/sync, controller, U1/U2, and safety status;
- movement rate/size, strobe, color, content intensity, group intensity, and Release controls;
- a four-bank × 32-slot visible window over the full 64 × 32 EmberLights catalog;
- selected, active, queued, progress, repeat, enabled-filter, and exclusive-bank feedback;
- Static Look Toggle/Hold semantics and clear release state;
- permanent blackout, Work Light, Release All, and diagnostics access;
- configurable custom pages without changing the engine.

Reference Studio should retain familiar library, venue, timeline, waveform, track hierarchy, Autoloop, Static Look, fixture, and inspector landmarks.

### 12.2 Improved EmberLights templates

Bundled templates should include:

- SoundSwitch Familiar;
- EmberLights Default;
- Compact Laptop Perform;
- Touch/Kiosk Perform;
- Controller-First Perform;
- Studio Wide/Dual-Monitor;
- Safe/Recovery.

Templates are normal first-party packages over the same contracts. Users may fork or overlay them.

## 13. Migration architecture

### 13.1 Standard Skin Migration IR

Every importer produces the same intermediate representation before EmberLights package generation:

```text
source identity/version/hash/provenance/license
source surfaces/canvas/resolution/input assumptions
regions/containers/elements/hierarchy
visual states/styles/assets/text/localization
foreign actions/expressions/feedback
controller mappings and logical roles where present
responsive assumptions
unsupported/opaque payloads
```

Each item carries a translation status:

```text
Exact
Translated
Approximated
ManualReview
OpaquePreserved
Unsupported
RejectedUnsafe
Conflict
```

The importer never silently substitutes behavior.

### 13.2 Adapter pipeline

```text
read-only source bundle
  -> hash and inventory
  -> source-specific parser
  -> Skin Migration IR
  -> semantic matcher
  -> generated candidate .emberskin/.emberoverlay/controller profile
  -> compatibility and licensing report
  -> visual/action review
  -> validate and install side-by-side
```

Adapters run only in Studio/migration workers. Perform never parses foreign formats.

### 13.3 VirtualDJ adapter

A later VirtualDJ adapter may inspect user-supplied skin ZIP/XML/image packages and supported VDJScript definitions.

It should:

- preserve source bytes and hashes;
- inventory XML elements, panels, groups, decks, positions, source rectangles, visual states, and assets;
- translate known primitives to Ember components;
- offer `Preserve fixed canvas` or `Modernize responsively` layout modes;
- map a documented, tested subset of VDJScript actions and queries to Ember commands/state/actions;
- preserve unsupported text as opaque evidence and identify exact manual work;
- separate controller mappings from skin layout;
- require the user to confirm rights to imported assets;
- never publish or redistribute imported proprietary artwork automatically.

Timer/repeat/script constructs are not translated into UI loops. They map only when an equivalent bounded Ember Action or domain plan exists.

### 13.4 SoundSwitch migration

SoundSwitch does not need a foreign skin import to deliver familiarity. EmberLights builds an evidence-backed Reference skin and separately migrates supported show content, fixtures, Looks, Autoloops, and mappings. The migration report can recommend Reference or another template based on the source workflow.

### 13.5 Future adapters

The same IR can support selected concepts from QLC+ Virtual Console, TouchOSC, Companion, other lighting applications, and hardware-surface templates. An adapter exists only after format, licensing, provenance, and semantic evidence are documented.

## 14. Compatibility and versioning

Version independently:

- `.emberskin` package schema;
- layout schema;
- Ember Actions language/IR;
- command registry;
- state registry;
- capability registry;
- component catalog;
- theme-token catalog;
- controller-profile schema;
- migration IR and adapter version.

Rules:

1. Public IDs are stable and never repurposed.
2. Incompatible parameter or state-type changes require a new ID/version.
3. Deprecation includes replacement and deterministic migration metadata where possible.
4. `compatibility.lock.json` records exact signatures used by the compiled package.
5. Import/install shows compatible, upgraded, degraded, and blocked items before activation.
6. Missing project targets become unavailable and relinkable, never silently redirected.
7. Overlays reference stable base component/slot IDs and retain the base version/hash for three-way migration.
8. A base-skin update never overwrites the user’s old working package or overlay.
9. Bundled skins are conformance fixtures for every supported registry/schema version.

## 15. Persistence and ownership scopes

### App-local

- selected skin and variant;
- monitor/window geometry;
- personal overlay selection;
- local asset/editing paths;
- designer preferences and recent packages;
- local device resolution hints.

### Skin package

- layouts, components, theme, actions, skin-control bindings, localization, assets, tests, provenance.

### Overlay

- user layout/theme/action/control differences from a base skin.

### Controller profile

- device matching, layers, input messages, transforms, soft takeover, hardware feedback.

### Project-authored

- show content and optional recommended skin/profile IDs or semantic roles when deliberately authored.

### Live-transient

- current page, focus, modifier/layer, pickup state, active content, overrides, and hazards.

Live-transient state never becomes authored content merely because a skin displayed or changed it.

## 16. Safety, security, and trust

### Mandatory reachability

Every Live variant must provide—or accept platform injection of—reachable:

- Blackout;
- Stop;
- Work Light;
- Release All Overrides;
- Runner/output/DJ/controller/safety health;
- Diagnostics and Safe fallback.

Emergency controls cannot depend on hover, an ambiguous double/long gesture, an optional page, a user action sequence, or a missing package asset.

### Trust tiers

- bundled first-party;
- locally authored;
- imported third-party;
- future signed community package.

Trust changes provenance and UI messaging, not sandbox permissions. Even signed packages remain declarative and bounded.

### Future community distribution

A marketplace/catalog is deferred until there is a documented policy for:

- signing and publisher identity;
- malware/resource-abuse scanning;
- asset and trademark licensing;
- compatibility and support windows;
- moderation, reporting, takedown, and ownership disputes;
- offline package availability and rollback;
- privacy and telemetry.

## 17. Performance architecture

- Skin source compilation and migration are Studio work.
- Perform loads a compiled graph and prevalidated assets.
- Action dispatch occurs outside the scheduler and posts bounded typed commands.
- High-frequency state is coalesced and visibility-aware.
- Hidden pages suspend or throttle expensive subscriptions.
- Native components virtualize large lists and timelines.
- Optional animations are throttled or disabled under load/reduced-motion policy.
- No filesystem/network work occurs in paint or input callbacks.
- Active skin switch prepares the complete candidate before atomic swap.
- Old resources retire only after renderer acknowledgement.
- The current skin remains active on failed reload.

Qualification must measure beside VirtualDJ and representative output traffic, not only in an isolated framework demo.

## 18. Qualification matrix

### Contract

- unique IDs and signature stability;
- command/state/component/action type checking;
- deprecation migration;
- deterministic compilation and hashes;
- malformed/unknown/oversized input diagnostics.

### Action system

- event, transform, branch, sequence, fallback, and result behavior;
- no cycles/recursion/unbounded execution;
- safety-command restrictions;
- no UI timing authority;
- identical outcomes across skin, keyboard, MIDI, DJ command, and direct tests;
- trace redaction and bounded buffers.

### Package and security

- ZIP slip, decompression bombs, path collisions, invalid UTF-8, huge assets, SVG abuse, excessive nodes/depth/subscriptions;
- no executable/device/network/filesystem access;
- hash/cache corruption and package deletion during validation;
- invalid first load -> Safe; invalid update -> retain current.

### Designer

- Undo/Redo/recovery;
- grid/constraints/hierarchy/component editing;
- theme and state matrix;
- typed action/feedback/input editors;
- accessibility and focus tooling;
- multi-variant preview;
- import/export/fork/overlay/reset/update conflict journeys.

### Visual and accessibility

- 1366×768, 1920×1080, 2560×1440, 4K;
- 100/125/150/200% DPI;
- mouse, keyboard, touch, controller-first;
- focus traversal, UI Automation/Narrator, names/roles, non-color state, contrast, target sizes, reduced motion;
- golden screenshots with deterministic mock state.

### Runtime

- cold start, first frame, idle/active memory, CPU/repaint;
- state-to-feedback and input-to-command latency;
- 100 valid/invalid switches and reloads;
- output continuity, active-content continuity, no show recompile;
- scheduler jitter under validation, switching, dense UI, diagnostics, and VirtualDJ co-load;
- low-end/reference Windows machines and installed builds.

### Migration

- source-byte immutability and provenance;
- deterministic IR and reports;
- exact/translated/approximate/manual/opaque/unsupported status;
- no silent action mapping;
- fixed-canvas and responsive conversion;
- rights/license confirmation;
- cross-version corpus and adapter regression fixtures.

## 19. Delivery sequence

### Wave 0 — planning and contracts

- approve this plan and `spec/ui/ember-actions-v1.md`;
- establish issue/branch/file ownership;
- define machine-readable catalog additions and migration IR;
- do not modify renderer or shared runtime behavior in the planning branch.

### Wave 1 — platform foundation

Depends on the active core gates and coordinates with #31, #32, #37, and #56.

- finish command/state metadata and canonical-source/codegen strategy;
- publish component/capability/theme/icon catalogs;
- implement direct-command Ember Actions compatibility and compiler skeleton;
- implement `.emberskin` validator/compiler, compiled artifact, Safe surface, and atomic activation;
- keep Default/Reference example packages as conformance fixtures.

### Wave 2 — bundled skins and bounded customization

Coordinates with #33, #34, #35, #36, and #39.

- deliver Default and SoundSwitch Reference through the runtime;
- implement binding editor and MIDI/HID Learn;
- implement custom slots, controls, pages, theme overrides, overlays, reset, and import/export;
- implement Command/State Explorer and deterministic preview scenarios;
- qualify cross-surface equivalence and safety.

### Wave 3 — Skin Studio v1

- full visual hierarchy/grid/constraint editor;
- component and asset library;
- token/theme/state editor;
- simple and graph action editors;
- feedback and input editors;
- responsive/DPI/accessibility preview;
- package validation, fork, compare, update/merge, publish;
- expert text view only after graph round-trip is exact.

### Wave 4 — advanced Ember Actions and migration

- bounded reusable action graphs and domain plan integration;
- action recorder where semantics are unambiguous;
- Skin Migration IR tooling;
- VirtualDJ importer prototype and corpus;
- guided fixed-canvas-to-responsive modernization;
- SDK/CLI for validate, format, inspect, test, pack, unpack, diff, migrate, and golden screenshots.

### Wave 5 — ecosystem

- package signing and publisher identity;
- optional community catalog/distribution;
- additional adapters and component templates;
- agent-assisted offline authoring through the same typed catalogs and compiler;
- no AI/model dependency in Perform.

## 20. Workstream ownership and conflict rules

### Registry/catalog owner

Owns canonical command/state/capability/component/theme/icon metadata and generation. No other lane invents public IDs independently.

### Ember Actions owner

Owns action schemas, parser, type checker, compiler, executor boundary, traces, and tests. Does not alter domain command semantics.

### Runtime owner (#32)

Owns package reader, validation, immutable view graph, compiled artifact, renderer adapter, Safe fallback, activation, cache, and package diagnostics.

### Skin Studio owner

Owns authoring document/controller, visual editor, previews, package UX, overlay/fork/update workflows, and designer-only services. Does not place editor services in lean Perform.

### Bundled skin owners (#33/#34)

Own package layouts/themes/assets/tests only. They request missing catalog capability through the registry owner and never add private domain callbacks.

### Migration owner

Owns read-only source adapters, source manifests, Migration IR, translation reports, generated candidates, and corpora. Does not add foreign parsing to Perform.

### Qualification owner (#36/#56)

Owns conformance fixtures, goldens, performance/accessibility/security/fault evidence, and installed-system gates. It does not silently waive failures.

### Shared-file rule

Before editing these areas, agents post exact file reservations on the coordinating issue:

```text
native-core/include/emberlights/ui_command.hpp
native-core/src/ui_command.cpp
native-core/include/emberlights/ui_state.hpp
native-core/src/ui_state.cpp
spec/ui/schema/*
spec/ui/examples/*
native-core/CMakeLists.txt
native-core/src/windows_app.cpp
installer/release configuration
```

Prefer new isolated files and generated catalogs over concurrent edits to monolithic shared files.

## 21. Definition of platform completion

The skin platform is complete when a non-developer can visually fork or customize a bundled skin, add and bind controls, configure state feedback and MIDI, validate/export/import the result, activate it during a running show, recover safely from an invalid package, and move the package to another machine without changing show content or leaking machine-local identity.

The advanced platform is complete when a supported foreign skin can enter through the migration IR with an exact report, and an expert can use optional text syntax that round-trips losslessly with the visual Ember Actions graph—while Perform remains bounded, deterministic, offline, and free of arbitrary user code.