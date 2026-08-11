# UI/UX and Modular Skin Architecture

Status: binding product direction for future UI work. The exact rendering toolkit remains evidence-gated, but the command/state/skin separation in this document must be preserved.

## Objective

EmberLights must feel immediately understandable to an experienced SoundSwitch user while fixing SoundSwitch's dated, mode-fragmented, difficult-to-customize workflow. The default experience should remain lightweight and gig-safe, but the UI must not be hard-wired to one layout, DJ platform, controller, event company, or monitor size.

The central rule is:

> **One command model, many control surfaces.**

The default EmberLights UI, the SoundSwitch-reference skin, future user skins, MIDI controllers, keyboard shortcuts, OS2L/custom DJ commands, touch surfaces, and future remote controls must invoke the same stable command registry and observe the same state registry. A skin is a view/binding package over product capabilities; it is never a second lighting engine.

This borrows the useful architectural idea behind VirtualDJ's skins, custom buttons, pad pages, and controller mappings: visual controls are separable from the actions they invoke. EmberLights should go further by making the contract typed, versioned, deterministic, and shared by software and hardware surfaces.

## Research summary: what to retain from SoundSwitch

SoundSwitch remains the primary workflow reference. Its current model has several good concepts worth preserving:

- **Studio/Edit versus Performance separation.** Authoring can be dense; live operation should be simple and large-target.
- **Venue/rig reuse.** A venue tab or equivalent provides a useful mental model for reusable fixture layouts.
- **Audio-editor timeline metaphor.** Master, group, and fixture control tracks align naturally with a waveform/beatgrid.
- **Static Looks.** Scene-like live looks are an excellent primitive for weddings and event moments.
- **Autoloops.** Beatgrid-synchronized reusable lighting clips are central to unscripted DJ operation.
- **Bank/pad operation.** SoundSwitch 2.9's four banks of 32 Autoloops, progress feedback, direct triggering, duplication, and repeat policies are useful live concepts even though EmberLights' engine already exceeds the storage limit.
- **Global performance overrides.** Movement, strobe, color, intensity, scripted-track intensity, Autoloop intensity, and group intensity are good gig-time controls.
- **Visible connection health.** SoundSwitch 2.9's red/green DJ-software connection status, hardware indicator, and notifications are the right direction.
- **Control One mirroring.** Software state and controller feedback should tell the same story.

Sources reviewed include SoundSwitch's current What is SoundSwitch, Getting Started in Edit Mode, Performance Mode Preferences, Static Looks, Autoloops, Control Tracks, Saving Projects and Light Shows, 2.9 UI/status changes, 2.9 Autoloop changes, and 2.10 release information, plus current UI screenshots and user reports.

## Problems EmberLights should deliberately fix

### 1. Mode fragmentation

SoundSwitch exposes major concepts as separate modes/views and sometimes makes the user reason about different rules in each context. Newer users frequently report that something configured or understood in Edit Mode behaves differently or becomes difficult to locate in Performance Mode.

**EmberLights response:** keep authoring and live operation operationally separate, but use the same objects, names, IDs, commands, colors, bank locations, state indicators, and inspectors across both. Switching workspace changes the presentation, not the underlying semantics.

### 2. Hidden state and weak discoverability

A control should never require prior knowledge that it exists in another tab or preference screen. Critical gig state must not be hidden behind modal dialogs.

**EmberLights response:** persistent status strip, command search, contextual inspector, and right-click/long-press control introspection. Every user-visible control can expose `What is this?`, `Command`, `Current binding`, and `Learn MIDI` where applicable.

### 3. Confusing persistence

SoundSwitch project/lightshow saving and reopening can be unintuitive because project, venue, and currently loaded audio are distinct concepts. EmberLights already has stronger versioned project persistence; the UI must make the model obvious.

**EmberLights response:**

- authored project changes use clear Saved/Unsaved state, Undo/Redo, autosaved recovery draft, explicit durable Save, and restore points;
- preferences and connection settings save immediately unless a setting explicitly requires restart;
- live transient overrides never silently become authored content;
- any transient control can offer an explicit `Set as default`/`Write to project` action where that is safe;
- the app remembers the last opened project as an application preference and can reopen it automatically;
- project-scoped connection/output configuration persists with the project, while machine-specific device identity/reconnect hints remain local and are clearly labeled.

There should be no generic Apply/Save button on a settings page if changing a field already commits it. If a restart or reconnect is required, show that exact pending action next to the changed field.

### 4. Connection setup feels separate from performance

Connection state must be actionable, not merely displayed.

**EmberLights response:** configured DJ inputs and outputs auto-connect/reconnect by default. The status strip exposes source, transport health, BPM, DMX output health, controller state, and safety state. Clicking a problem opens the narrow diagnostic for that connection rather than a generic settings maze.

### 5. Fixed layouts and controller assumptions

SoundSwitch is designed around its own UI and Control One layout. EmberLights must support users moving from different DJ software, controllers, and lighting workflows.

**EmberLights response:** layout and binding are data. Hardware surfaces and skins target stable commands rather than internal UI callbacks.

### 6. Density without hierarchy

SoundSwitch's Edit Mode can become visually dense, while Performance Mode has large controls but limited personalization.

**EmberLights response:** progressive disclosure. Keep primary performance controls always visible; place advanced adjustments in contextual trays/inspectors. Studio defaults to a familiar library + timeline + inspector hierarchy, with detachable/resizable panels and workspace presets.

## Product UX model

EmberLights has two primary workspaces, not a collection of unrelated application modes:

### Studio

Author, organize, import, migrate, preview, map, validate, and compile.

Primary areas:

1. **Global status/title strip**
2. **Project/Venue navigation**
3. **Library/Assets dock**
4. **Timeline/Editor canvas**
5. **Inspector/Fixture dock**
6. **Transport/Waveform area**
7. **Optional utility panels**: Mapping, Connections, Diagnostics, Preview, Migration report

### Live

Operate one immutable validated show package with large, fast, low-risk controls.

Primary areas:

1. **Gig status strip**
2. **Primary performance surface**
3. **Autoloops / Static Looks / Moments / Overrides as switchable panels or pad pages**
4. **Compact group/intensity controls**
5. **Emergency controls**
6. **Non-modal diagnostics drawer**

Live does not expose authoring complexity by default. If the combined Windows app hosts both workspaces, entering Studio is still an explicit authoring transition and must not destabilize the Runner.

## Command registry

No new clickable behavior should be implemented as a one-off UI callback when it can be represented as a product command.

Each command definition should include, at minimum:

- stable namespaced ID;
- human name and description;
- parameter schema and range;
- scope: Studio, Live, or Both;
- interaction type: trigger, momentary, toggle, latch, absolute value, relative value, selection;
- whether it is real-time safe;
- whether it is MIDI-bindable;
- whether it is keyboard-bindable;
- whether it may be invoked from an external adapter;
- optional safety requirements;
- optional feedback/state key;
- deprecation/version metadata.

Example namespaces, not a frozen API:

```text
app.*
project.*
workspace.*
connection.*
transport.*
output.*
safety.*
autoloop.*
staticLook.*
trackScript.*
group.*
fixture.*
override.*
palette.*
position.*
timeline.*
editor.*
undo.*
view.*
```

Representative commands:

```text
workspace.open("live")
project.save
output.blackout.set(true)
safety.workLight.toggle
autoloop.launch(bankId, slotId)
autoloop.next
autoloop.bank.select(bankId)
autoloop.repeat.set("infinite")
staticLook.activate(lookId)
staticLook.clear
override.releaseAll
group.intensity.set(groupId, 0.72)
override.movementRate.set(0.4)
connection.os2l.reconnect
view.panel.toggle("diagnostics")
```

A future macro/action language may compose commands, but the first implementation should prefer a small typed command graph over unrestricted embedded JavaScript/Lua in Runner. Determinism and safety outrank cleverness during a gig.

## State registry

Controls must read shared state rather than infer engine behavior from UI timers.

Representative state keys:

```text
app.workspace
project.id
project.name
project.saveState
project.activeVenue
connection.dj.kind
connection.dj.status
connection.dj.detail
transport.bpm
transport.phase
transport.syncState
transport.activeDeck
output.universe[1].status
output.universe[2].status
controller.count
autoloop.active.id
autoloop.active.progress
autoloop.visibleBanks[]
staticLook.active.id
trackScript.active.id
override.activeCount
safety.blackout
safety.hazardArmed
runner.health
runner.jitterP99
```

Runner state remains scheduler-owned and is exposed through bounded snapshots already consistent with existing architectural decisions. UI refresh rate must never become timing authority.

## Binding engine

A binding connects an input event to a command. The same binding semantics should serve:

- skin controls;
- keyboard shortcuts;
- MIDI/HID profiles;
- Control One;
- OS2L/custom DJ commands;
- future remote surfaces.

Bindings may specify press/release, alternate/shift action, value transform, range, inversion, relative encoder mode, soft takeover, feedback, and visibility/enabled predicates.

A user should eventually be able to right-click a button, pad, fader, or knob and choose **Edit Binding** or **Learn MIDI** without editing the skin package manually.

## Skin system

### Principle

Skins define **presentation and bindings**, not domain behavior.

A skin package must be versioned and portable. Proposed packaging:

```text
MySkin.emberskin
  manifest.json
  theme.json
  layouts/
    live.json
    studio.json
    compact-live.json
  bindings/
    default.json
  assets/
    ...
```

The exact filenames may evolve, but the separation should remain.

### Manifest

The manifest should declare:

- skin ID, name, author, version;
- minimum/maximum supported skin-schema version;
- supported workspaces;
- intended screen classes/aspect ranges;
- optional required capabilities;
- asset hashes;
- fallback layout.

### Declarative widgets

Initial skin primitives should cover ordinary controls without requiring code:

- Button
- Toggle
- Pad
- Knob
- Slider/Fader
- Meter/Progress
- Text/Value
- Icon/Status
- Badge
- Tabs/SegmentedControl
- Grid
- Stack/Row/Column
- SplitPane
- Panel/Drawer
- Spacer/Separator

Complex native widgets are reusable components placed by skins rather than recreated by skin authors:

- Timeline
- Waveform/Beatgrid
- Fixture/Group List
- Fixture Inspector
- Music/Asset Browser
- Autoloop Matrix
- Static Look Matrix
- Mapping Editor
- Connection Panel
- Diagnostics Panel
- Venue/Patch View
- Preview/Visualizer

This distinction keeps skins powerful without making the skin engine responsible for rebuilding a DAW timeline from primitives.

### Layout and responsive behavior

Do not repeat SoundSwitch's legacy fixed-resolution assumptions. Skins should support:

- minimum/maximum sizes;
- relative/flex sizing;
- dock/split layouts;
- screen-density scaling;
- `compact`, `standard`, `wide`, and optional `touch` variants;
- high-DPI-safe vector/icon assets where practical;
- a fallback when an optional panel does not fit.

### Theme tokens

Use semantic tokens rather than hard-coded colors/sizes throughout every widget:

```text
surface.canvas
surface.panel
surface.raised
text.primary
text.secondary
status.ok
status.warn
status.error
action.primary
action.danger
pad.active
pad.queued
focus.ring
space.1 ... space.n
radius.small ... radius.large
font.body / font.control / font.numeric
```

A theme can change appearance without changing layout or actions.

### Skin safety

Runner should parse, validate, and compile a skin into an immutable/bounded view graph before using it live. Invalid widgets, unknown commands, impossible bindings, oversize assets, and schema mismatches fail closed to the bundled safe skin.

No skin is allowed to:

- perform arbitrary filesystem/network/USB operations;
- execute arbitrary native code;
- bypass safety policies;
- become the timing source for lighting playback;
- directly mutate compiled show-package memory;
- block the DMX scheduler.

Skin reload is a UI operation and must never stop DMX output. A failed live reload leaves the current skin active.

## Default EmberLights UX

The default skin should be more modern than SoundSwitch but keep the mental landmarks a migrating user expects.

### Global status strip

Always visible in Live; normally visible in Studio.

Left to right suggestion:

- EmberLights / active project
- active venue/rig
- DJ source + connection state
- BPM + sync-health state
- controller count/state
- Universe 1 / Universe 2 output state
- safety/hazard state
- notifications
- Settings/Connections affordance
- BLACKOUT as a visually isolated destructive control

Status uses color plus text/icon; never color alone.

### Studio default layout

**Left dock: Library**

- Music
- Autoloops
- Static Looks
- Palettes
- Positions/Attributes
- Effects
- Fixtures/Profiles

**Center: Editor**

- venue tabs/workspaces near the top;
- master/group/fixture track stack;
- semantic cue blocks and curves;
- waveform + beatgrid aligned to the editor;
- tool strip for select/draw/range/snap/zoom;
- transport controls close to the waveform.

**Right dock: Inspector**

Contextual, never a permanent wall of unrelated properties. Selecting a fixture, group, cue, Autoloop, look, mapping, or timeline block changes the inspector.

**Bottom/optional drawers**

Connections, Mapping, Diagnostics, Migration Report, Preview.

### Live default layout

The user should be able to run most gigs without opening a settings dialog.

Recommended default composition:

- top status strip;
- large primary control row: master intensity, movement rate, movement size, strobe rate/cap, color override, clear/release;
- central pad surface with pages: Autoloops, Static Looks, Moments, Overrides;
- page/bank selectors adjacent to the pads;
- active item/progress visible on the pad itself and in a concise Now Playing area;
- group faders in a collapsible side/bottom strip;
- scripted-track and Autoloop intensity controls visible when relevant;
- work light and blackout permanently reachable;
- diagnostics slide-out rather than a full mode switch.

## The bundled SoundSwitch Reference skin

EmberLights must ship a **SoundSwitch Reference** skin for migration, testing, parity QA, and familiar operation. It is a functional compatibility layout, not a pixel-for-pixel copy of proprietary artwork. Use EmberLights-owned icons, typography, assets, and branding.

Purpose:

1. give existing SoundSwitch users familiar muscle memory;
2. make parity gaps obvious during side-by-side testing;
3. prove the skin architecture can express a materially different layout without engine changes;
4. provide a stable UI target while the default EmberLights skin evolves.

### SoundSwitch Reference — Live

Approximate SoundSwitch information architecture:

- header/status row with DJ connection, hardware/output connection, BPM, and project/venue;
- visible workspace tabs/segments for `Performance`, `Autoloops`, `Static Looks`, `Edit/Studio`, and `Connections/Diagnostics` equivalents;
- Performance page with large Movement Rate, Movement Size, and Strobe controls;
- recognizable color/movement/strobe override blocks;
- Autoloop intensity, scripted-track intensity, and group intensity faders;
- Autoloop bank selector + 32-slot matrix for the selected bank, with active/progress feedback;
- Static Looks matrix;
- explicit play/clear/repeat/override controls where the engine supports them;
- blackout and work light always reachable even when the reference layout did not historically emphasize them.

### SoundSwitch Reference — Studio/Edit

Approximate SoundSwitch editing landmarks:

- project/venue tabs below the main toolbar;
- music/library tree on the left;
- central multitrack timeline;
- Master, Group, and Fixture tracks;
- cue/effect palette categories adjacent to the editor;
- waveform/beatgrid aligned at the bottom;
- fixture list/inspector on the right;
- Autoloop and Static Look editing views that use the same bank/slot IDs as Live.

### Reference-skin parity rule

When a SoundSwitch workflow is implemented in EmberLights, parity QA should be able to execute the equivalent flow using the SoundSwitch Reference skin. A feature is not considered UI-complete if it only exists in a hidden developer screen or only in the default skin.

## Customization UX roadmap

### Phase A — architecture first

- stable command registry;
- stable state registry;
- skin schema and validator;
- bundled Default and SoundSwitch Reference skins;
- keyboard and MIDI binding editor targets the same commands;
- safe fallback skin.

### Phase B — user-editable controls

- right-click/long-press `Edit Control`;
- choose a command from categorized searchable action list;
- set label/icon/color/interaction behavior;
- add Custom Buttons, Pads, Faders, and Knobs to designated user panels;
- export/import mapping and skin packages.

This should feel analogous to VirtualDJ's custom-button/pad workflow: a user can discover commands by category, see descriptions, and assign behavior without learning source code.

### Phase C — visual skin designer

- unlock layout/design mode;
- drag/rescale/reorder components;
- add/remove panels;
- create responsive variants;
- duplicate/fork bundled skins;
- theme-token editor;
- validation before activation.

### Phase D — community ecosystem

- signed/hashed packages;
- compatibility/version metadata;
- preview screenshots;
- optional community sharing/marketplace later, without making cloud access required to run a gig.

## Cross-DJ and cross-hardware portability

A DJ-platform adapter publishes normalized transport/mixer state; it does not own UI. A controller profile binds physical controls to the command registry; it does not own lighting logic. A skin presents those commands/states; it does not know whether data came from VirtualDJ, Serato, Engine, MIDI Clock, or audio fallback.

This enables the desired migration model:

```text
DJ software adapter  -> normalized state -> Runner
Hardware mapping     -> command registry -> Runner
Skin/UI control      -> command registry -> Runner
Runner state         -> state registry   -> skin + hardware feedback
```

Changing DJ software, controller, or skin should therefore be an adapter/profile change, not a show rewrite.

## Persistence rules for UI and connections

Use three clearly separated persistence scopes:

### Application-local

- last opened project;
- window size/monitor;
- chosen skin and layout variant;
- local device reconnect hints;
- recent files;
- machine-specific paths.

### Project-authored

- venue/rig definitions;
- logical connection/output configuration that belongs to the show;
- controller mapping/profile choices where portable;
- Autoloops, Looks, scripts, groups, safety policy;
- optional preferred skin/workspace only if explicitly chosen as project behavior.

### Live-transient

- active Autoloop/Look;
- current bank filter;
- manual overrides;
- temporary intensity changes;
- diagnostics drawer state;
- currently armed hazard state where safety requires re-arming.

Live-transient state must not silently become project content.

## Performance and footprint requirements

The modular UI cannot undermine Runner targets in `01_PRODUCT_REQUIREMENTS.md`.

- no embedded browser runtime in Runner;
- no user script VM required for basic skins;
- no filesystem/network activity on paint or input callbacks;
- no UI allocation or lock dependency on the DMX scheduler;
- bounded state snapshots from Runner to UI;
- UI can drop visual refresh frames without dropping lighting commands;
- idle/hidden panels reduce update work;
- asset sizes and widget counts are validated;
- animation is ornamental and throttled/disabled under load;
- skin parse/compile happens before activation;
- representative Default and SoundSwitch Reference skins must be included in footprint/repaint benchmarks before toolkit acceptance.

The rendering toolkit remains undecided. Any candidate must prove both skins at target resolutions and high DPI without violating Runner memory/CPU ceilings. The domain-facing skin schema must remain toolkit-agnostic so a toolkit change does not invalidate user skins or mappings.

## Input, accessibility, and gig ergonomics

- mouse, keyboard, touch, MIDI, and controller operation are first-class;
- focus/keyboard navigation for ordinary controls;
- high-contrast state cues and labels, not color-only status;
- minimum touch targets in touch layouts;
- no hover-only critical action;
- destructive commands require spatial isolation and/or deliberate gesture;
- no blocking modal dialog over Live for routine connection failure;
- `Esc`/Back semantics are consistent;
- every critical live control must have a stable command ID suitable for hardware binding.

## Command discoverability

Add a **Command Explorer / Command Palette** early. It is both UX and development infrastructure.

Search result should show:

- action name;
- namespace/category;
- description;
- parameter schema;
- current keyboard/MIDI bindings;
- whether the action is available in the current workspace/state;
- Execute/Test where safe;
- `Add to Custom Panel` later.

This prevents capabilities from becoming hidden and gives build agents a direct way to verify that new engine actions are properly surfaced.

## Acceptance tests for the UI foundation

The first modular-UI milestone is accepted only when all of the following are true:

1. Default and SoundSwitch Reference skins are loaded from separate declarative packages.
2. Switching between those skins does not reload/recompile the show package or stop DMX output.
3. At least ten representative live actions are invoked through the command registry from both skins and MIDI/keyboard mappings.
4. UI controls read active/progress/connection/health data from the shared state registry rather than duplicating timing logic.
5. An invalid skin safely falls back without stopping Runner.
6. Unknown/deprecated command IDs produce clear validation errors.
7. Window scaling works at 100%, 125%, 150%, and 200% DPI on Windows for both bundled skins.
8. Compact 1366×768, standard 1920×1080, and wide/high-resolution layouts remain operable without clipped gig-critical controls.
9. Runner CPU/memory/repaint benchmark includes both skins and stays inside the established product ceilings.
10. OS2L/DJ and DMX connection status is visible and actionable from Live without navigating to a separate settings mode.
11. Connection settings demonstrate immediate persistence, auto-reconnect, and clear scope.
12. Last project can reopen automatically without requiring the user to browse for it every launch.
13. Blackout remains reachable and non-droppable regardless of skin/page.
14. A skin cannot invoke an unsafe/hazardous action without the same safety gate used by hardware and native controls.

## Implementation order for build agents

Do not start by painting a prettier hard-coded UI.

1. Inventory every existing Studio/Live UI callback and map it to a stable command/state contract.
2. Implement command and state registries around existing Runner/Studio actions without changing lighting semantics.
3. Route keyboard and MIDI mappings through the same command model where compatible.
4. Define the versioned skin package/schema and validator.
5. Implement a small reusable widget set plus complex native components.
6. Rebuild the existing minimal UI as `EmberLights Default v0` using the skin path.
7. Build `SoundSwitch Reference v0` from the same components/commands.
8. Add Command Explorer and control introspection.
9. Add persistence scope labels and auto-connect/reconnect UX.
10. Benchmark both skins before selecting/finalizing the production rendering toolkit.
11. Only then add end-user layout editing and custom-control creation.

## Non-goals for the first skin milestone

- pixel-perfect cloning of SoundSwitch graphics;
- arbitrary third-party executable plugins;
- CSS/HTML compatibility;
- unrestricted code inside skin files;
- cloud skin marketplace;
- every Studio panel being rearrangeable on day one;
- making Runner load authoring-only libraries to satisfy a skin.

## VirtualDJ inspiration: what to copy conceptually, not literally

VirtualDJ demonstrates the value of separating interface elements, custom buttons/pads, controller mappings, and a shared action vocabulary. Its skin SDK exposes visual elements and containers; custom buttons and pad pages can target VDJScript actions; controller mapping files target the same action language.

EmberLights should preserve that architectural freedom while improving safety for a live lighting engine:

- typed/versioned commands instead of unbounded text everywhere;
- the same actions for software and hardware;
- declarative conditional state/visibility;
- reusable complex widgets;
- schema validation and safe fallback;
- deterministic Runner with no mandatory script interpreter.

## Binding principle for future agents

Before adding a new UI feature, ask:

1. What stable product command does this invoke?
2. What stable state does it observe?
3. Could the same capability be bound to MIDI/keyboard/another skin?
4. Does it belong to Studio authoring, Live transient control, or both?
5. What persistence scope owns it?
6. Can the SoundSwitch Reference skin expose the equivalent workflow?
7. Does the implementation preserve Runner footprint and deterministic output?

If those questions are not answered, the feature is not ready to be hard-coded into the interface.
