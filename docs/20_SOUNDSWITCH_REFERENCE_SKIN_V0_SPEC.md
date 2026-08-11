# SoundSwitch Reference Skin v0 Specification

Status: implementation target for migration UX and parity QA.

This document specifies the bundled **SoundSwitch Reference** skin. It is subordinate to the EmberLights domain/runtime architecture and uses the shared command/state contracts in `../spec/ui/command-state-skin-contract-v0.md`.

## Objective

Provide an EmberLights-owned interface that is immediately legible to an experienced SoundSwitch user and precise enough for side-by-side parity testing.

The reference skin must:

- preserve SoundSwitch's useful Studio/Edit and Performance landmarks;
- expose implemented parity workflows without hidden developer screens;
- use original EmberLights code, icons, typography, assets, and branding;
- improve safety, persistence clarity, connection diagnostics, responsiveness, and accessibility;
- prove that the same engine can support a materially different UI package;
- remain lightweight enough for the Runner performance budget.

It must not:

- copy SoundSwitch artwork pixel-for-pixel;
- imply affiliation or endorsement;
- define lighting behavior outside the command/state layer;
- execute arbitrary code;
- hide EmberLights safety controls to achieve visual similarity.

## Skin identity

Provisional manifest identity:

```text
id: com.emberlights.skin.soundswitch-reference
name: SoundSwitch Reference
version: 0.1.0
schema: 0
workspaces: studio, live
variants: compact, standard, wide, touch-live
fallback: com.emberlights.skin.safe
```

## Workspace model

The skin exposes two primary workspaces:

- **Studio** — the SoundSwitch Edit Mode mental model, modernized.
- **Live** — the SoundSwitch Performance Mode mental model, modernized.

Connections, Mapping, Diagnostics, Migration, and Preferences are panels/drawers within those workspaces, not separate engines or timing modes.

## Global invariants

These remain true in every page and responsive variant:

1. Blackout is always reachable from Live.
2. Work Light is always reachable from Live.
3. DJ/sync, DMX output, controller, project, and safety state remain visible or one deliberate action away.
4. Opening a panel does not interrupt DMX.
5. Skin switching does not recompile or stop the show.
6. All controls invoke stable commands.
7. All feedback reads shared state.
8. State is communicated by more than color alone.
9. Live transient changes never silently mutate authored content.
10. Hazardous output remains gated by the core safety policy.

## Responsive classes

These are EmberLights design targets, not claimed SoundSwitch dimensions.

| Class | Client width | Client height | Use |
| --- | ---: | ---: | --- |
| `compact` | 960–1439 | 640+ | Small laptop / 1366×768 |
| `standard` | 1440–2199 | 800+ | 1080p primary target |
| `wide` | 2200+ | 900+ | 1440p/4K, multi-panel view |
| `touch-live` | 1024+ | 700+ | Large targets, simplified Live controls |

Below 960×640, load the bundled safe compact Live surface and require Studio to use a larger window.

## Visual design direction

### Familiar characteristics to preserve

- neutral charcoal application surfaces;
- compact Studio chrome;
- timeline-first authoring hierarchy;
- square/low-radius utility controls;
- bright cue colors over a dark grid;
- rectangular Autoloop and Static Look targets;
- dense but orderly track headers;
- selected-bank and active-loop emphasis;
- restrained branding during technical work.

### Deliberate EmberLights improvements

- stronger text hierarchy;
- semantic status tokens;
- higher-contrast focus/selection rings;
- clearer active versus selected versus queued states;
- consistent spacing scale;
- scalable vector icons;
- explicit saved/unsaved state;
- persistent gig-health strip;
- contextual inspector;
- no color-only status;
- non-modal diagnostics;
- responsive panel collapse rather than clipping.

## Provisional theme tokens

All values are `DESIGN_TARGET` until Tier A native screenshot measurements are completed.

### Color roles

```text
surface.app             #15181B
surface.chrome          #1B1F23
surface.panel           #20252A
surface.panelRaised     #272D33
surface.canvas          #171A1D
surface.control         #2A3036
surface.controlHover    #343B42
surface.controlPressed  #181C20
border.subtle           #343A40
border.strong           #4B535C
text.primary            #E4E8EB
text.secondary          #A8B0B7
text.muted              #747D85
accent.reference        #2C92D4
status.ok               #3FBF72
status.warn             #E1A63A
status.error            #D84B4B
status.info             #4E9FD6
action.danger           #B52929
action.dangerActive     #E34040
focus.ring              #78BFFF
selection.fill          rgba(44,146,212,0.22)
selection.border        #4AA6E4
```

Cue/pad colors are content data and must be contrast-adjusted against the selected theme.

### Spacing and geometry

```text
space.1   4
space.2   8
space.3   12
space.4   16
space.5   24
space.6   32
radius.utility  2
radius.panel    4
radius.touch    8
border.thin     1
border.active   2
```

### Typography

Use a bundled/system-available UI font chosen for Windows clarity and licensing safety. Do not depend on SoundSwitch's font.

```text
font.body          13/18 standard, 12/16 compact
font.control       12/16
font.caption       11/14
font.title         18/24
font.numeric       tabular figures, 13/18
font.timeline      11/14
```

## Global frame

## Application menu row

Studio only by default; Live may collapse it behind the application menu.

Target height:

- compact: 22
- standard/wide: 24

Menus:

- File
- Edit
- Show
- Insert
- View
- Connections
- Mapping
- Help

Menu actions remain commands and are searchable in Command Explorer.

## Primary toolbar

Studio target height: 38 standard, 34 compact.

Suggested groups:

1. back/forward or context navigation;
2. Undo / Redo;
3. Save state;
4. Select / Draw / Range tools;
5. Snap / Grid;
6. Zoom;
7. Link/association;
8. Validate / Compile / Activate;
9. workspace switch Studio ↔ Live;
10. compact connection/safety indicators.

No toolbar icon may be the sole access point for an undocumented operation.

## Gig/status strip

Visible in Live and Studio. In Live it occupies the top of the client area and remains pinned.

Target height:

- compact: 40
- standard/wide: 44
- touch-live: 54

Left-to-right regions:

1. project + saved state;
2. active venue/rig;
3. DJ source + connection;
4. BPM + sync state;
5. controller status;
6. Universe 1 and Universe 2 status;
7. active content summary;
8. override count;
9. safety/hazard status;
10. notifications;
11. Work Light;
12. BLACKOUT.

### Required status bindings

| Region | Command | State |
| --- | --- | --- |
| Project | `project.openRecent`, `project.save` | `project.name`, `project.saveState` |
| Venue | `venue.select` | `venue.active.id`, `venue.active.name` |
| DJ | `connection.dj.openPanel`, `connection.dj.reconnect` | `connection.dj.kind`, `connection.dj.status`, `connection.dj.detail` |
| BPM | `transport.tap`, `transport.manualBpm.set` | `transport.bpm`, `transport.syncState`, `transport.confidence` |
| Controllers | `connection.controllers.openPanel` | `controller.connectedCount`, `controller.expectedCount`, `controller.status` |
| Outputs | `connection.output.openPanel` | `output.universe[1].status`, `output.universe[2].status` |
| Content | `view.panel.open("nowPlaying")` | active script/look/loop IDs and names |
| Overrides | `override.releaseAll` | `override.activeCount` |
| Safety | `view.panel.open("safety")` | arming/cap/fault states |
| Work Light | `safety.workLight.toggle` | `safety.workLight` |
| Blackout | `output.blackout.toggle` | `safety.blackout` |

## Studio specification

## Standard layout

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ Menu                                                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│ Toolbar                                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ Status / Project / Venue / DJ / Output / Safety                             │
├─────────────────────────────────────────────────────────────────────────────┤
│ Venue Tabs                                                                  │
├───────────────┬───────────────────────────────────────────┬─────────────────┤
│ Library       │ Timeline + Track Headers                  │ Inspector       │
│               │                                           │                 │
│               │                                           │                 │
│               │                                           │                 │
├───────────────┴───────────────────────────────────────────┴─────────────────┤
│ Transport + Waveform + Beatgrid                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│ Optional drawer: Connections / Mapping / Diagnostics / Migration / Preview  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Design-target proportions at 1920×1080

| Region | Target |
| --- | ---: |
| Menu | 24 px |
| Toolbar | 38 px |
| Status strip | 44 px |
| Venue tabs | 30 px |
| Library | 232 px default; 180 min; 360 max |
| Inspector | 292 px default; 240 min; 440 max |
| Track-header column | 184 px default; 148 min; 260 max |
| Waveform/transport | 124 px default; 92 min; 220 max |
| Utility drawer | 240 px default when open |
| Timeline | remaining flexible area |

Panel sizes are persisted as app-local workspace preferences, not authored show data.

## Compact Studio

At compact width:

- library and inspector cannot both remain permanently open;
- show Library as the default left dock;
- Inspector becomes a right overlay/drawer or replaces Library after selection;
- track-header column may reduce labels and move secondary actions into a context menu;
- waveform height reduces but does not disappear;
- status strip groups controller/output state into compact badges;
- blackout remains visible only if Runner is active; otherwise it remains in the status overflow with F8 still functional.

## Wide Studio

At wide width:

- Library and Inspector remain visible;
- optional Fixture/Group overview or Preview can occupy a fourth dock;
- timeline can display expanded property lanes without horizontal crowding;
- utility drawer may become a persistent lower or right panel;
- no new domain capability is exclusive to wide mode.

## Venue tab strip

Purpose: preserve SoundSwitch's venue-tab familiarity while representing stable EmberLights venue/rig objects.

Components:

- venue tabs;
- add venue;
- duplicate venue;
- context menu;
- dirty/incomplete/error indicators;
- active-output assignment badge;
- overflow list.

Bindings:

```text
venue.select(id)
venue.create
venue.duplicate(id)
venue.rename(id, name)
venue.delete.request(id)
venue.validate(id)
```

States:

```text
venue.list[]
venue.active.id
venue[item].validation
venue[item].dirty
venue[item].outputAssignment
```

Deleting a venue or fixture must never silently delete shared content; destructive impact is previewed and protected by Undo/history.

## Library panel

Header:

- Library title;
- search;
- source/filter menu;
- favorites/recent toggle;
- collapse.

Default sections:

- Music
- Track Scripts
- Autoloops
- Static Looks
- Colors/Palettes
- Positions
- Attributes
- Effects
- Fixtures/Profiles
- Groups
- Venues
- Imports

Item row states:

- normal;
- selected;
- active/loaded;
- favorite;
- imported;
- missing dependency;
- approximation warning;
- incompatible;
- drag source.

Double-click, Enter, drag, and context-menu behavior must be documented per item type. No critical action is available only through double-click.

## Timeline editor

The timeline is a complex native component positioned by the skin.

Required subregions:

- time ruler;
- track-header column;
- track canvas;
- vertical grid;
- playhead;
- selection range;
- cue blocks;
- automation curves/nodes;
- collapsed/expanded property lanes;
- horizontal/vertical scrollbars;
- zoom/snap overlay;
- empty/load/error overlay.

### Track hierarchy

```text
Master
Groups
  Group A
    optional property lanes
Fixtures
  Fixture 1
  Fixture 2
```

The reference skin may visually resemble SoundSwitch's flat stacked tracks, but hierarchy must be explicit through indentation, disclosure controls, track-type icon, and label.

### Track header row

Primary controls:

- disclosure;
- track-type icon;
- name;
- Mute;
- Solo;
- target/scope badge;
- validation/warning badge;
- context menu.

Secondary controls appear in Inspector or expanded lane to avoid excessive row clutter.

### Timeline commands

Representative minimum set:

```text
timeline.play
timeline.pause
timeline.stop
timeline.seek(time)
timeline.zoom.in
timeline.zoom.out
timeline.zoom.fit
timeline.snap.set(mode)
timeline.selection.set(range)
timeline.selection.clear
timeline.cue.create(type, target, range)
timeline.cue.delete(selection)
timeline.cue.duplicate(selection)
timeline.cue.move(selection, delta)
timeline.cue.resize(selection, edge, delta)
timeline.node.create
timeline.node.move
timeline.node.delete
track.mute.toggle(id)
track.solo.toggle(id)
track.collapse.toggle(id)
```

### Timeline state

```text
timeline.duration
timeline.playhead
timeline.playing
timeline.zoom
timeline.scroll
timeline.snap
timeline.selection
track.list[]
track[item].mute
track[item].solo
track[item].expanded
editor.tool
editor.validation
```

## Waveform and transport

Persistent lower strip containing:

- play/pause;
- stop;
- loop preview;
- time/beat position;
- BPM;
- waveform;
- beatgrid/downbeat markers;
- zoom overview;
- track identity/association status.

Waveform rendering is a native component. The skin controls placement and surrounding actions, not audio decoding.

## Inspector

The Inspector is contextual and uses consistent sections:

1. identity/name;
2. target/scope;
3. primary properties;
4. timing/transition;
5. ownership/layer behavior;
6. bindings;
7. validation/provenance;
8. advanced section.

Supported contexts in v0:

- project;
- venue;
- fixture profile;
- patched fixture;
- group;
- track;
- cue block;
- automation node;
- Autoloop;
- Static Look;
- palette;
- position/attribute cue;
- MIDI mapping;
- connection;
- migration result.

No selection displays a short useful project/connection summary rather than a blank panel.

## Autoloop Studio page/panel

The reference skin provides the SoundSwitch-familiar bank/matrix view.

### Standard arrangement

```text
┌──────────────────────────────────────────────────────────────────┐
│ Bank window: [1] [2] [3] [4]       Page 1/16      Search/Filter │
├───────────────────────────────────────────────┬──────────────────┤
│ 32-slot matrix, 4 columns × 8 rows            │ Autoloop Inspector│
│                                               │                  │
└───────────────────────────────────────────────┴──────────────────┘
```

The exact column count may adapt by width, but slot order remains stable and visible.

### Bank header state

- selected: white/bright outline + selected label;
- contains active loop: pulsing or animated blue indicator, throttled;
- exclusive: red badge/outline plus text/icon;
- disabled/filter-excluded: muted with explicit disabled icon;
- empty: count shown as `0/32`;
- active progress: thin progress strip independent of selected state.

### Slot state

- empty slot shows index and `Add` on hover/focus;
- populated slot shows name, length, repeat default, optional color tag;
- selected is not the same as active;
- active shows progress and play icon;
- warning shows a non-color badge;
- imported approximation shows provenance badge;
- drag preview preserves source address until commit;
- occupied drop requires explicit Move/Swap/Cancel choice.

### Commands

```text
autoloop.select(id)
autoloop.create(bank, slot)
autoloop.duplicate(id)
autoloop.move(id, bank, slot)
autoloop.swap(id, targetId)
autoloop.delete.request(id)
autoloop.populateEmpty(windowOrBank)
autoloop.resetDefaults.request(scope)
autoloop.preview.start(id)
autoloop.preview.stop
autoloop.window.previous
autoloop.window.next
autoloop.bank.select(bank)
```

## Static Look Studio page/panel

### Standard arrangement

- Look matrix/list on the left or center;
- contextual fixture/group ownership table;
- preview controls;
- Inspector for selected assignment;
- live pad/binding section.

### Ownership control

Each target/property uses a visible three-state control:

```text
Release to lower layer
Set value
Force zero/off
```

Do not represent this only as an inclusion checkbox. The reference skin may show a compatibility label such as `Included`, but the underlying state and tooltip must be explicit.

## Live specification

## Standard Live layout

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ Pinned gig/status strip                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ Page tabs: Performance | Autoloops | Static Looks | Moments | Overrides    │
├───────────────────────────────┬─────────────────────────────────────────────┤
│ Primary control surface       │ Now Playing / Banks / Page tools           │
│ or 32-pad matrix              │                                             │
│                               │                                             │
├───────────────────────────────┴─────────────────────────────────────────────┤
│ Group intensity strip / optional side faders                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Pinned safety controls

BLACKOUT:

- isolated at the far right/top or a consistent edge;
- danger styling;
- label changes to `RELEASE BLACKOUT` when active;
- active state cannot be hidden by a page;
- F8 remains a default keyboard binding;
- command always passes through core safety/output authority.

WORK LIGHT:

- adjacent but visually distinct from blackout;
- clear active state;
- not styled as destructive.

## Live page tabs

Reference labels:

- Performance
- Autoloops
- Static Looks
- Moments
- Overrides

Studio switch is present in application/workspace navigation but visually separated from gig page tabs to prevent accidental authoring transitions.

Connections and Diagnostics open drawers from status indicators rather than occupying a primary performance tab.

## Performance page

### Primary modules

1. Master Intensity
2. Movement Rate
3. Movement Size
4. Strobe Rate / Strobe Enable
5. Color Override
6. White / UV quick override where supported
7. Autoloop Intensity
8. Scripted Track Intensity
9. Group Intensity strip
10. Release All Overrides
11. active-content summary

### Control behavior

- knobs/faders show value numerically;
- double-click or dedicated reset returns to authored/lower-layer behavior, not a hidden arbitrary default;
- touch variant uses faders or large radial controls with explicit reset;
- strobe obeys safety cap and displays cap/blocked state;
- movement and color controls own only their mapped semantic properties;
- `Release All Overrides` clears the transient ManualOverride layer only.

### Commands and states

| Control | Command | State |
| --- | --- | --- |
| Master Intensity | `override.masterIntensity.set`, `.release` | `override.masterIntensity`, ownership state |
| Movement Rate | `override.movementRate.set`, `.release` | value/ownership |
| Movement Size | `override.movementSize.set`, `.release` | value/ownership |
| Strobe | `override.strobeRate.set`, `.release` | value, allowed, cap |
| Color | `override.color.set`, `.release` | selected color, ownership |
| White/UV | semantic override commands | value/availability |
| Autoloop Intensity | `autoloop.intensity.set`, `.release` | value/ownership |
| Script Intensity | `trackScript.intensity.set`, `.release` | value/ownership |
| Group fader | `group.intensity.set`, `.release` | group value/ownership |
| Release All | `override.releaseAll` | `override.activeCount` |

## Autoloops Live page

### Standard arrangement

- four-bank window selector at top or side;
- 32-slot matrix;
- `Previous`, `Next`, `Play All Banks`;
- repeat selector: One Shot / Infinite / Track;
- override-script state;
- active item and progress;
- page navigation for 64-bank library;
- concise explanation of exclusive-bank long press/context action.

### Active-state hierarchy

A slot may be:

1. selected for inspection;
2. currently active;
3. queued/pending boundary;
4. default candidate for automatic selection;
5. disabled by bank filter.

These states must not use the same styling.

### Reference behavior

- clicking a populated slot launches immediately according to Runner behavior;
- one-shot runs its musical length and returns naturally;
- Infinite repeats until disabled/cleared;
- Track repeats until track transition/stop according to engine policy;
- long-press or explicit bank menu sets exclusive bank;
- `Play All Banks` restores unrestricted selection;
- active progress is read from Runner status, not calculated by the skin;
- software and controller feedback use the same state keys.

## Static Looks Live page

- pageable/searchable rectangular pad matrix;
- active Look is clearly latched;
- clicking another Look crossfades according to authored/default policy;
- Clear releases to the lower layer;
- hazardous Looks display required safety state before activation;
- group/category filters are visible and resettable;
- user can open `Edit Binding` without leaving Live, but authored Look editing remains Studio work.

## Moments page

Moments are optional organization over existing commands and content, not a new engine layer.

Examples:

- Entrance
- First Dance
- Toasts
- Open Dance
- Last Dance
- Send-Off

A Moment tile may launch a Static Look, Autoloop, Track Script, macro/sequence, or open a control page. The tile must reveal its underlying action in control introspection.

## Overrides page

This page provides granular manual control beyond the primary Performance modules.

Required hierarchy:

- Target: All / Group / Fixture
- Property category: Intensity / Color / Position / Movement / Beam / Strobe / White/UV / Custom
- value control;
- Apply/Own property;
- Release property;
- Release target;
- Release All;
- active override summary.

Applying a group override must remain one validated Runner command using an immutable target mask, consistent with existing architecture.

## Now Playing panel

Visible as a compact summary in Live and expandable as a drawer.

Display:

- DJ track identity when trustworthy;
- sync state;
- active Track Script;
- active Autoloop + bank/slot + repeat + progress + cycles;
- active Static Look;
- active Moment;
- manual override count;
- degraded/fallback source.

It does not infer timing from paint intervals.

## Connection drawers

## DJ source drawer

Fields/actions:

- source kind;
- endpoint/bind address where relevant;
- current status;
- last successful event time;
- last error;
- Reconnect;
- Test/Capture;
- setup help;
- Auto-connect toggle;
- scope label: machine or project.

## Output drawer

Per universe:

- configured adapter;
- destination/device;
- Ready/Reconnecting/Fault/Disabled;
- frame counters;
- last error;
- Test output where safe;
- Reconnect;
- zero-output/stop semantics;
- project versus machine identity fields.

## Controller drawer

- expected mappings/profiles;
- connected inputs/outputs;
- feedback state;
- conflicts;
- Learn MIDI;
- reconnect/refresh;
- open Mapping panel.

## Notifications

Priority:

- safety/output fault;
- DJ timing degradation;
- controller issue;
- project save/validation;
- migration warning;
- update/informational.

Critical live faults remain visible until acknowledged/resolved. Informational notices may auto-dismiss.

## Error, empty, and degraded states

The reference skin must include designed states for:

- no project;
- project loading;
- recovered project;
- project validation failed;
- no venue;
- no fixtures;
- no audio loaded;
- no Autoloops;
- no Static Looks;
- no DJ connection;
- predictive hold;
- audio fallback;
- manual BPM;
- safe unsynchronized mode;
- output reconnecting;
- output fault;
- missing controller;
- missing media asset;
- imported approximation;
- skin validation fallback.

Each state identifies the owner and next safe action. Avoid generic `Something went wrong` messages.

## Input and accessibility

- ordinary controls are keyboard focusable;
- focus order follows visible hierarchy;
- Enter/Space activation is consistent;
- Escape closes drawers/menus without clearing live state;
- no critical action depends on hover;
- context menus are also available through keyboard/menu buttons;
- status uses icon/text/shape plus color;
- compact text maintains readable contrast;
- touch-live targets should normally be at least 44×44 logical pixels;
- blackout and hazardous controls require deliberate placement/gesture without adding delay to emergency blackout;
- motion/progress animations can be reduced or disabled;
- numeric values use tabular figures where available.

## Control introspection

Right-click/long-press a skin control opens a standard introspection menu:

- What is this?
- Current command
- Current value/state
- Keyboard bindings
- MIDI/controller bindings
- Learn MIDI
- Edit Binding, when permitted
- Add to Custom Panel, later phase
- Open Command Explorer

This menu is supplied by the platform, not individually implemented by each skin.

## Golden parity journeys

The SoundSwitch Reference skin is accepted when these journeys can be executed without hidden developer screens.

### Journey 1 — Start a safe gig

1. open last project;
2. confirm venue;
3. confirm DJ source;
4. confirm both universes;
5. start Runner;
6. trigger and release Work Light;
7. verify blackout from UI and F8;
8. return to normal output.

### Journey 2 — Unscripted track / Autoloop

1. DJ track begins without script;
2. Autoloop fallback becomes active;
3. active bank and slot are visible;
4. progress updates from Runner;
5. next loop can be triggered;
6. exclusive bank can be set;
7. Play All Banks restores all banks.

### Journey 3 — Autoloop over scripted track

1. scripted track active;
2. launch one-shot Autoloop;
3. show loop progress;
4. loop finishes full musical length;
5. scripted layer resumes naturally;
6. repeat modes behave distinctly.

### Journey 4 — Static Look event moment

1. dancing content active;
2. activate First Dance Look;
3. excluded properties continue from lower layer;
4. explicit-zero properties remain off;
5. clear Look;
6. lower layer resumes with configured crossfade.

### Journey 5 — Manual override

1. set group intensity;
2. set color override;
3. verify override count;
4. release one property;
5. Release All;
6. automated playback continues underneath.

### Journey 6 — Connection fault

1. disconnect DJ source or output in a controlled test;
2. status changes immediately;
3. Live remains operable;
4. clicking status opens owner-specific diagnostics;
5. auto-reconnect is visible;
6. recovery is confirmed without restarting the show.

### Journey 7 — Studio authoring

1. load track;
2. select venue;
3. expand Master/Group/Fixture hierarchy;
4. create/select a cue;
5. edit in Inspector;
6. Undo/Redo;
7. save project;
8. validate/compile/activate;
9. switch to Live without changing object identity.

### Journey 8 — Autoloop organization

1. navigate four-bank window;
2. select a loop;
3. duplicate;
4. move to empty slot;
5. attempt occupied move and choose Swap/Cancel;
6. Undo;
7. populate empty with impact warning/history;
8. verify same IDs/locations in Live.

## Golden screenshot states

At minimum, generate golden screenshots for each bundled skin at:

- 1366×768, 100%;
- 1920×1080, 100%;
- 2560×1440, 150%;
- 3840×2160, 200%.

Reference states:

- Studio empty;
- Studio normal timeline;
- Studio selected cue;
- Studio Autoloop editor;
- Studio Static Look editor;
- Live Performance normal;
- Live Autoloop active at ~50%;
- Live exclusive bank;
- Live Static Look active;
- Live overrides active;
- DJ disconnected;
- Universe fault;
- blackout active;
- compact drawer open;
- invalid skin fallback.

Golden tests detect unintended layout change; they do not replace behavior/accessibility tests.

## Performance requirements

The skin must satisfy the existing Runner ceilings.

Specific UI requirements:

- no browser runtime required;
- immutable validated view graph during Live;
- bounded widget and asset counts;
- state subscriptions are explicit;
- hidden panels do not refresh unnecessarily;
- animation is throttled and optional;
- no UI lock is held by the DMX scheduler;
- commands use bounded queues or direct safe APIs according to command classification;
- dropping a visual frame never drops a lighting command;
- failed reload preserves the running skin;
- fallback skin is compiled into the application or otherwise guaranteed available.

## Definition of done for Reference v0

- [ ] loads as an external/bundled declarative skin package;
- [ ] Studio and Live layouts meet this specification at compact and standard sizes;
- [ ] every visible action maps to a registered command;
- [ ] every visible feedback item maps to registered state;
- [ ] all golden parity journeys pass for implemented engine features;
- [ ] invalid package falls back safely;
- [ ] switching from Default to Reference does not stop DMX or recompile show data;
- [ ] blackout remains authoritative and accessible;
- [ ] connection states are actionable;
- [ ] persistence scope is visible;
- [ ] high-DPI and keyboard tests pass;
- [ ] representative repaint/CPU/memory benchmarks remain within product ceilings;
- [ ] deviations from SoundSwitch are recorded and intentional;
- [ ] original EmberLights branding/assets are used.
