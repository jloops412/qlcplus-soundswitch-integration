# SoundSwitch UI Forensics and Screenshot Capture Plan

Status: required research and evidence plan for the modular UI program.

Related binding documents:

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `../spec/ui/command-state-skin-contract-v0.md`

## Purpose

This document turns “make EmberLights feel like SoundSwitch” into a reproducible research process. It defines what evidence to collect, what can and cannot be inferred from screenshots, the exact screens and states to capture, and how those observations become components, tokens, interaction requirements, and parity tests.

The goal is not to recover or imitate SoundSwitch source code. Screenshots do not reveal literal CSS, toolkit classes, event handlers, or proprietary assets. They do reveal the observable interface contract: information architecture, hierarchy, density, spatial relationships, state communication, control geometry, workflows, and interaction affordances.

## Research boundary

### We may derive

- layout regions and relative proportions;
- recurring component types;
- panel hierarchy and navigation model;
- spacing rhythm, density, typography hierarchy, and approximate color roles;
- visible interaction states;
- workflow sequence and discoverability problems;
- responsive behavior when multiple resolutions are compared;
- the minimum component set required by the reference skin;
- parity journeys and golden-screen test states.

### We must not claim to derive

- SoundSwitch's literal CSS or source code;
- private component names or internal architecture;
- exact proprietary fonts, icons, or assets unless publicly licensed;
- hidden behavior not evidenced by documentation, live observation, or controlled testing;
- exact pixel values from scaled/compressed screenshots without marking them as estimates.

### Reference-skin rule

The SoundSwitch Reference skin is a workflow-compatible, EmberLights-owned implementation. It should preserve familiar structure and muscle memory while using original code, icons, typography, assets, safety behavior, and branding. It is not intended to be a trademark-confusing or pixel-for-pixel clone.

## Evidence confidence tiers

| Tier | Evidence | Use |
| --- | --- | --- |
| A | Native screenshots captured from the currently installed SoundSwitch version at a recorded resolution/DPI | Measurements, golden comparisons, state inventory |
| B | Official SoundSwitch support screenshots/GIFs and product imagery | Workflow/state confirmation, component discovery |
| C | Official tutorial videos and release documentation | Interaction sequence, transient states, change history |
| D | Community screenshots/videos with visible version/context | Edge cases, real-world density, historical comparison |
| E | Memory or anecdotal description | Question generation only; never the sole basis for an exact requirement |

Tier A is required before freezing exact visual tokens for the SoundSwitch Reference skin. Tiers B–D are sufficient to design the architecture and initial layout specification.

## Sources already reviewed

The current research pass reviewed the following official areas:

- What is SoundSwitch / Edit and Performance mental model
- Getting Started in Edit Mode
- Control Tracks: Master, Group, and Fixture
- Autoloop improvements in 2.9
- UI and status-indicator changes in 2.9
- Performance Mode Preferences
- Static Looks
- Autoscripting Autoloops
- Saving Projects and Light Shows
- SoundSwitch 2.10 overview and release direction

Primary links:

- https://support.soundswitch.com/en/support/solutions/articles/69000847410-soundswitch-what-is-soundswitch-
- https://support.soundswitch.com/en/support/solutions/articles/69000847416-soundswitch-getting-started-in-edit-mode
- https://support.soundswitch.com/en/support/solutions/articles/69000847597-control-tracks-in-soundswitch
- https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9
- https://support.soundswitch.com/en/support/solutions/articles/69000858488-soundswitch-ui-and-status-indicator-changes-in-soundswitch-2-9
- https://support.soundswitch.com/en/support/solutions/articles/69000847088-soundswitch-performance-mode-preferences-explained
- https://support.soundswitch.com/en/support/solutions/articles/69000863339-soundswitch-static-looks-explained
- https://support.soundswitch.com/en/support/solutions/articles/69000844240-soundswitch-how-to-autoscript-autoloops
- https://support.soundswitch.com/en/support/solutions/articles/69000853039-soundswitch-saving-projects-and-light-shows
- https://support.soundswitch.com/en/support/solutions/articles/69000869542-soundswitch-what-s-new-in-soundswitch-2-10-

## Existing EmberLights UI audit

The current Windows application is a valuable functional shell, but it is exactly the kind of hard-coded UI structure the modular program must replace incrementally.

`native-core/src/windows_app.cpp` currently contains:

- a single large Win32 application/view implementation;
- a fixed `Page` enum for Live, Overrides, Profiles, Patch, Groups, Looks, Autoloops, Tracks, MIDI, Connections, Safety, and Diagnostics;
- a large `ControlId` enum coupling visible controls to event handling;
- direct `add_button`, `add_edit`, `add_label`, and related native-control creation;
- a fixed 176-pixel navigation rail and 26-pixel status bar;
- a `layout_page` switch with explicit pixel coordinates;
- direct command dispatch from Win32 IDs to Runner/Studio methods;
- explicit Apply actions on Connections and Safety pages;
- useful existing last-project persistence in the Windows registry;
- useful existing Runner status snapshots and 250 ms status refresh.

### What to preserve

- current project/compiler/Runner behavior;
- non-droppable blackout and safety authority;
- existing project history, validation, migration, mapping, and diagnostic operations;
- the bounded status snapshots already supplied by Runner;
- installed-GUI smoke testing;
- current last-project persistence;
- keyboard accelerators such as F8 blackout until the command registry replaces them cleanly.

### What to change

- move product behavior out of control-ID-specific callbacks;
- replace page-specific state inference with shared state keys;
- replace the fixed layout switch with declarative layout/component descriptions;
- keep the current Win32 shell temporarily as a legacy view adapter during migration;
- eliminate ambiguous Apply behavior when a preference can persist immediately;
- separate app-local, project-authored, and live-transient persistence;
- preserve a tiny hard-coded emergency/fallback UI independent of third-party skins.

## SoundSwitch observable information architecture

### Global application chrome

Observed recurring structure:

1. native menu/application menu;
2. compact icon toolbar;
3. application/brand title;
4. venue/project tabs below the toolbar in Edit Mode;
5. status indicators and mode controls in the upper region;
6. dense central work area;
7. mode-specific panels rather than one universal dashboard.

### Edit Mode

The core mental model is DAW-like:

- music/library tree on the left;
- effects/cue categories near the left or lower-left;
- Master, Group, and Fixture control tracks in the center;
- fixture/group headers and track controls aligned to the tracks;
- a time ruler and vertical beat/grid lines;
- colored cue blocks, white intensity curves, movement/effect shapes, and selection regions;
- waveform and beatgrid aligned along the bottom;
- venue tabs under the toolbar;
- fixture controls/fixture list toward the right;
- selected-track/cue operations through toolbars, dialogs, context menus, or direct manipulation.

### Performance Mode

The core mental model is control-surface-like:

- source selection and connection state;
- performance tabs/pages;
- large primary overrides;
- Autoloop and Static Look selection surfaces;
- intensity and group controls;
- active-state/progress feedback;
- direct controller mirroring;
- hardware and DJ-source health visible without opening the editor.

### Preferences

SoundSwitch exposes operational behavior such as:

- source/input type;
- crossfader behavior;
- deck configuration;
- upfader-only intensity behavior;
- playhead smoothing;
- loop auto-strobe;
- external mixer behavior;
- Autoloop repeat/override behavior.

The concepts are valid, but EmberLights should surface high-impact current state in Live and use contextual configuration rather than forcing the user to remember which preference tab owns a behavior.

## Screen-by-screen forensic findings

## 1. Edit shell

### Preserve

- left-library / central-timeline / right-fixture-inspector mental map;
- venue tabs close to the timeline;
- compact tool strip;
- waveform tied spatially to the cue timeline;
- visible hierarchy from Master to Group to Fixture tracks.

### Improve

- stronger visual separation between navigation, authoring tools, and selected-object properties;
- searchable asset library with filters and favorites;
- contextual inspector instead of scattered modal dialogs;
- explicit save state and object scope;
- dockable/collapsible panels and remembered workspace variants;
- consistent selection, focus, hover, disabled, muted, soloed, overridden, and warning states;
- clearer difference between semantic cue blocks and raw intensity/automation curves.

### Reject

- fixed-resolution assumptions;
- making every tool permanently visible;
- hiding important fixture/persistence behavior inside double-click-only dialogs;
- allowing a visual selection to silently imply an unsafe scope.

## 2. Timeline and control tracks

### Required component model

- `TimelineEditor`
- `TimelineRuler`
- `TrackHeaderColumn`
- `MasterTrack`
- `GroupTrack`
- `FixtureTrack`
- `PropertyLane`
- `CueBlock`
- `AutomationCurve`
- `SelectionRange`
- `Playhead`
- `BeatGrid`
- `Waveform`
- `TrackMuteSoloControls`
- `ZoomSnapTransportStrip`

### Key behavior

- Master intent remains portable across venues;
- Group and Fixture tracks override lower-resolution intent without deleting it;
- track collapse/expand must not alter playback;
- selection scope must always be visible;
- cue blocks and automation curves need strong hit targets at high DPI;
- playhead, beatgrid, and cue timing must share one coordinate transform;
- UI refresh is observational; scheduler timing remains authoritative.

## 3. Library and asset browser

SoundSwitch combines DJ-library navigation and lighting-authoring assets in a dense left column. EmberLights should preserve the single-place discoverability while separating asset types clearly.

Required top-level asset domains:

- Music / DJ libraries
- Track Scripts
- Autoloops
- Static Looks
- Palettes
- Positions
- Attributes
- Effects
- Fixtures / Profiles
- Groups
- Venues / Rigs
- Imports / Migration report

Required common affordances:

- search;
- type filter;
- favorites;
- recent;
- source/provenance;
- drag target preview;
- context actions;
- keyboard navigation;
- empty/loading/error states.

## 4. Autoloop editor

SoundSwitch 2.9 establishes the important live model:

- four visible banks;
- 32 loops in the selected bank;
- selected-bank indicator;
- active-loop progress indication;
- exclusive-bank state;
- drag/reorder and cross-bank movement;
- duplication;
- populate-empty behavior;
- direct triggering over scripted tracks;
- one-shot natural return;
- infinite and track-duration repeats.

EmberLights already exceeds the storage model with 64 banks × 32 slots. The reference skin therefore uses a pageable four-bank window while the domain model retains stable 64×32 addresses.

Required component states:

- empty;
- populated idle;
- selected;
- active;
- active progress;
- queued/pending boundary, if supported;
- disabled by filter;
- bank selected;
- bank contains active loop;
- bank exclusive;
- drag source;
- valid drop target;
- invalid/occupied drop target;
- missing dependency;
- imported/approximated warning.

## 5. Static Looks

SoundSwitch's fixture-inclusion behavior is important and must be made clearer in EmberLights:

- included + value set means the look owns the property;
- excluded means lower playback continues;
- included + explicit zero means the look deliberately turns the property off.

This maps directly to EmberLights `SET`, `RELEASE`, and `FORCE_ZERO` semantics and should be visible in the authoring UI rather than hidden behind a checkbox interpretation.

Required editor treatment:

- fixture/group rows;
- three-state ownership indicator;
- intensity/color/position/attribute controls;
- included scope summary;
- hazardous-effect warnings and arming separation;
- preview without mutating the running project;
- live pad assignment and MIDI binding.

## 6. Performance shell

SoundSwitch correctly prioritizes direct control, but EmberLights should provide a stronger persistent hierarchy.

Always visible in the reference Live shell:

- project and venue;
- DJ source and connection health;
- BPM and sync-health state;
- DMX output health by universe;
- controller count/health;
- current active script/Autoloop/Static Look summary;
- manual override count;
- safety/hazard state;
- work light;
- blackout.

The user should not have to switch away from Live to determine why lights are not moving.

## 7. Connection and notification states

SoundSwitch 2.9's red/green DJ state, hardware indicator, source picker, and notifications are useful. EmberLights should improve them by making every degraded state actionable.

Example status interaction:

- click `VirtualDJ — Not Connected` → narrow connection drawer with endpoint, last error, retry, capture/test action, and relevant setup help;
- click `Universe 1 — Reconnecting` → output-specific diagnostics, not the entire Settings application;
- click `MIDI — 1/2 Connected` → device list with reconnect/conflict information;
- click notification → exact owner and suggested action;
- no routine connection failure opens a blocking modal over Live.

## 8. Settings and persistence

SoundSwitch documentation distinguishes project data from per-track lightshows and requires separate save behavior. EmberLights' native model is already safer; its UI must communicate that advantage.

Required visible concepts:

- `Saved`, `Unsaved`, `Saving`, `Save failed`, `Recovered draft`;
- current project path and last verified save;
- app-local versus project scope labels;
- immediate persistence for ordinary preferences;
- exact `Reconnect required` or `Restart required` indicators;
- explicit `Write live value to project` for safe transient-to-authored promotion;
- no generic Apply button where each field can commit safely;
- destructive reset/populate operations show impact and create Undo/history recovery.

## Preserve / improve / reject matrix

| SoundSwitch pattern | Decision | EmberLights treatment |
| --- | --- | --- |
| Edit vs Performance separation | Preserve | Studio and Live workspaces over one object/command model |
| Venue tabs | Preserve and deepen | Reusable rig/venue tabs, searchable templates, stable IDs |
| Master/Group/Fixture tracks | Preserve | Semantic precedence, contextual inspector, portable intent |
| Waveform + beatgrid timeline | Preserve | One timing transform, better zoom/snap/selection |
| Autoloop bank/pad model | Preserve and exceed | 64×32 engine; four-bank pageable reference window |
| Static Looks | Preserve and clarify | Visible SET/RELEASE/FORCE_ZERO ownership |
| Performance overrides | Preserve and modularize | Command-driven custom surfaces and hardware bindings |
| Connection indicators | Preserve and make actionable | Drill directly into owner-specific diagnostics |
| Control One mirroring | Preserve concept | Shared command/state feedback for any controller |
| Separate Save Project / Save Lightshow confusion | Replace | One coherent native project model with explicit asset association/history |
| Fixed layout | Replace | Declarative responsive skins |
| Hidden context menus/double-click-only configuration | Reduce | Inspector, discoverable context actions, Command Explorer |
| Color-only status | Replace | Text/icon/shape plus color |
| Modal operational troubleshooting | Replace | Non-modal Live drawers |
| Hardware-specific capacity limits | Reject | Controller windows over hardware-independent engine capacity |

## Native screenshot capture protocol

Official screenshots are useful, but exact reference-skin work requires controlled native captures from the currently installed SoundSwitch version.

### Capture environment metadata

Record for every session:

```text
soundswitchVersion
operatingSystem
screenResolution
windowClientSize
windowsScalePercent
fontScaling
projectName
venueName
inputSource
connectedHardware
captureDate
captureOperator
notes
```

### Required resolutions

Minimum baseline:

1. 1366×768 at 100% Windows scaling
2. 1920×1080 at 100%
3. 2560×1440 at 125% or 150%
4. 3840×2160 at 150% or 200%

Also capture maximized and a deliberately resized non-maximized window where SoundSwitch permits it.

### Capture rules

- use PNG, not JPEG;
- capture the complete application window;
- do not crop out title/menu/status areas;
- keep the same sample project and fixture names across captures;
- avoid personal file paths or account information where possible;
- document whether a state is hover, pressed, selected, active, disabled, disconnected, or error;
- for animation/progress, capture beginning, midpoint, and end where possible;
- preserve original files read-only and store derivatives separately;
- do not commit commercial/licensed SoundSwitch assets outside the private evidence area.

### Filename convention

```text
SS-<version>_<workspace>_<screen>_<state>_<resolution>_<scale>_<sequence>.png
```

Example:

```text
SS-2.10.3_Performance_Autoloops_ActiveProgress_1920x1080_100_01.png
```

### Capture matrix: Edit Mode

- [ ] empty/new project
- [ ] normal project, no track loaded
- [ ] track loaded with waveform/beatgrid
- [ ] Master track with color and intensity cues
- [ ] Group track expanded
- [ ] Group track collapsed
- [ ] Fixture track selected
- [ ] Mute/Solo states
- [ ] timeline range selected
- [ ] cue block selected
- [ ] automation node selected
- [ ] zoomed out full track
- [ ] zoomed in beat-level
- [ ] venue tab selected
- [ ] multiple venue tabs
- [ ] add/rename venue context menu
- [ ] fixture library open
- [ ] fixture property/address dialog
- [ ] DMX chart universe 1
- [ ] DMX chart universe 2
- [ ] Position Cue authoring
- [ ] Attribute Cue authoring
- [ ] Static Look editor
- [ ] Static Look fixture-inclusion menu
- [ ] Autoloop bank 1
- [ ] Autoloop bank 2
- [ ] Autoloop empty slots
- [ ] Autoloop context menu
- [ ] drag/reorder state
- [ ] cross-bank move state
- [ ] duplicate action/result
- [ ] populate-empty confirmation/result
- [ ] Autoscript dialog
- [ ] Preferences: libraries
- [ ] Preferences: Performance Mode
- [ ] Save Project / Save Lightshow menus
- [ ] validation/error dialog

### Capture matrix: Performance Mode

- [ ] source selection screen
- [ ] DJ software disconnected
- [ ] DJ software connected
- [ ] BPM Detection source
- [ ] MIDI source
- [ ] Ableton Link source
- [ ] no DMX hardware
- [ ] one hardware device
- [ ] multiple hardware devices
- [ ] notification closed/open
- [ ] main Performance page
- [ ] movement-rate override idle/active
- [ ] movement-size override idle/active
- [ ] strobe idle/active
- [ ] color override idle/active
- [ ] group intensity states
- [ ] scripted-track intensity state
- [ ] Autoloop intensity state
- [ ] Autoloop bank selected
- [ ] Autoloop active at 0%, ~50%, and ~95%
- [ ] active Autoloop in another bank
- [ ] exclusive bank
- [ ] Play All Banks
- [ ] one-shot over scripted track
- [ ] infinite repeat
- [ ] track-duration repeat
- [ ] Static Looks page
- [ ] Static Look active
- [ ] Static Look cleared
- [ ] override scripted tracks enabled/disabled
- [ ] external mixer mode
- [ ] blackout/work-light equivalent where available
- [ ] connection failure during active playback

### Optional Control One paired capture

For every software state that produces Control One LED feedback, capture:

- software screen;
- Control One top-down photo;
- MIDI message log if available;
- state transition used to reach it.

This becomes the controller-feedback parity corpus.

## Measurement and annotation procedure

For each Tier A screenshot:

1. record full image and client dimensions;
2. mark major rectangles: menu, toolbar, status, library, track headers, timeline, inspector, waveform;
3. calculate each rectangle as both pixels and percentage of client area;
4. sample recurring gaps and control heights;
5. identify repeating spacing increments rather than copying every isolated value;
6. sample semantic color roles, not every antialiased pixel;
7. list all visible states and transitions;
8. identify controls whose meaning depends on hover, right-click, long-press, or external hardware;
9. annotate pain points and proposed EmberLights deviation;
10. map each visible control to a proposed command and state key.

### Confidence tags

Every derived value should use one tag:

- `MEASURED` — native Tier A screenshot with known client size;
- `ESTIMATED` — compressed/scaled screenshot;
- `DESIGN_TARGET` — EmberLights-selected value, not claimed as SoundSwitch's;
- `BEHAVIORAL` — observed/documented interaction independent of pixels.

## Preliminary visual observations

These are estimates from public imagery and must not be treated as final measured tokens:

- predominantly neutral charcoal surfaces with low-radius or square controls;
- thin separators and dense control rows;
- compact application/menu/toolbar chrome;
- relatively narrow library and fixture rails compared with the timeline;
- timeline dominates Edit Mode;
- waveform receives a persistent lower strip;
- colored cue blocks and traces are primary semantic accents;
- Performance Mode uses flatter, larger rectangular targets and stronger state color;
- typography is utilitarian and compact rather than brand-heavy;
- selected/active state often relies heavily on color, which EmberLights should supplement with text/icon/shape.

## Research outputs

The forensic work is complete enough for implementation when the repository contains:

- this evidence/capture plan;
- a private native screenshot corpus with metadata;
- annotated screen maps;
- component inventory;
- target design tokens with confidence tags;
- the SoundSwitch Reference skin v0 specification;
- command/state mappings for every reference-skin control;
- golden-state test list;
- explicit deviation ledger explaining every intentional improvement.

## Exit criteria before visual freeze

Do not freeze the reference skin's exact visual dimensions until:

1. native screenshots exist at 1366×768 and 1920×1080;
2. Edit, Performance, Autoloop, Static Look, connection, and settings states are represented;
3. every major region has measured bounds;
4. every critical control has an interaction/state description;
5. every critical control has a command/state mapping;
6. the layout remains usable at Windows 100%, 125%, 150%, and 200% scaling;
7. the design-deviation ledger is approved;
8. the UI toolkit benchmark can render both bundled skins within Runner ceilings.

## Immediate next evidence request

The public evidence is sufficient to architect the system. The highest-value additional input is a controlled native screenshot set from Joshua's installed SoundSwitch version using the capture matrix above. That set should be gathered before final visual polish, but it does not block command/state/schema implementation.
