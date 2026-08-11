# SoundSwitch UI Observation Ledger

Status: current research findings feeding the SoundSwitch Reference skin. Exact visual values remain provisional until the Tier A native screenshot corpus is complete.

Related:

- `19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- issue #30

## Version baseline

As of the current research pass:

- SoundSwitch 2.10 is the latest major public release found in official support material;
- SoundSwitch 2.10 primarily introduced the inMusic licensing/account migration plus performance improvements and bug fixes;
- SoundSwitch 2.10.2 is specifically documented for macOS 26 compatibility;
- the latest official public documentation describing a substantial desktop UI change remains SoundSwitch 2.9, which introduced source selection, clearer DJ/software connection status, hardware status, notifications, and expanded Autoloop UI behavior.

This does not prove that every 2.10.x pixel is unchanged. Native capture metadata must record the exact installed build.

## Evidence reviewed

### Official primary sources

- What is SoundSwitch / Edit and Performance model
- Getting Started in Edit Mode
- Control Tracks
- Static Looks
- Autoloops and 2.9 Autoloop improvements
- Performance Mode preferences and troubleshooting
- 2.9 UI/status changes
- BPM Detection
- VirtualDJ and Serato troubleshooting
- SoundSwitch 2.10 overview
- current minimum system requirements

### Secondary visual evidence

- community screenshots showing the full Edit Mode timeline at desktop resolutions;
- historical screenshots used only to identify recurring structures or generate capture questions.

Secondary evidence does not define exact current styling.

## Overall information architecture

### Edit Mode

Observable hierarchy:

1. operating-system menu/application chrome;
2. compact SoundSwitch toolbar;
3. venue/project tabs;
4. left-side music library and effect/palette sources;
5. central time-based control-track canvas;
6. Master, Group, and Fixture track hierarchy;
7. color/intensity/movement/attribute events aligned to a musical grid;
8. waveform and beatgrid near the bottom;
9. fixture/group track controls and contextual editing behavior;
10. separate preferences/dialog workflows for library, connection, performance, and hardware configuration.

### Performance Mode

Observable hierarchy:

1. source/status/header area;
2. top page tabs such as Performance, Autoloops, Static Looks, Edit Mode, External Mixer, and Standalone in documented screenshots;
3. high-level rate/size/strobe controls;
4. override grids for movement/strobe/color or related attributes;
5. large vertical intensity controls for Autoloops, scripted tracks, and groups;
6. dedicated Autoloop and Static Look pages;
7. status indicators for DJ software and hardware;
8. notification/help affordances.

## Visual observations

All items below are `ESTIMATED` or `BEHAVIORAL` unless explicitly marked otherwise.

### Application surfaces

- `ESTIMATED`: dark neutral charcoal surfaces rather than pure black;
- `ESTIMATED`: low-radius or square utility controls;
- `ESTIMATED`: fine separators and grid lines carry much of the hierarchy;
- `ESTIMATED`: muted gray secondary text with brighter active labels;
- `BEHAVIORAL`: vivid cue/event colors communicate content categories and lighting values;
- `BEHAVIORAL`: blue selection/highlight is used for selected Autoloop bank state in official 2.9 documentation;
- `BEHAVIORAL`: red and green are used for disconnected/connected status, accompanied by text;
- `BEHAVIORAL`: Autoloop active progress receives a dedicated visual indicator.

### Density

- Edit Mode is intentionally dense and desktop-first;
- Performance Mode enlarges primary controls but still places many secondary controls in a single fixed surface;
- preferences are dense tabbed forms with radio buttons and checkboxes rather than progressive/contextual configuration;
- the full timeline benefits from horizontal space and would not translate safely to a small fixed-width layout.

### Typography

- compact labels and numeric readouts dominate;
- long explanatory copy is mostly absent from primary workspaces;
- hierarchy is frequently communicated by placement, line separators, and control size rather than large typography differences;
- EmberLights should preserve density but improve title/body/control/numeric hierarchy and focus legibility.

### Performance controls

- large circular/dial controls provide immediate percent feedback for rate/size/strobe parameters;
- override targets are rectangular, vividly colored, and grouped by attribute family;
- tall intensity faders create an immediately readable three-or-more-channel performance mix surface;
- a fixed layout makes the key controls easy to learn but limits adaptation to different event workflows and controllers.

### Edit timeline

- cue blocks, ramps, vertical event lines, and position markers are visually layered over a dark grid;
- Master/Group/Fixture tracks encode precedence and scope spatially;
- waveform and beatgrid align lighting data to music;
- a large number of small controls compete for attention;
- track hierarchy and semantic scope are more valuable to preserve than the exact pixel styling.

## Important screenshot-analysis caution

Official troubleshooting screenshots often contain red outlines, callout boxes, crops, or other article annotations. Those marks are evidence of what the documentation wants the user to notice; they are not automatically SoundSwitch theme tokens or application controls.

Every captured/collected image must distinguish:

```text
application pixels
operating-system chrome
support-article annotation
video-player overlay
screen-capture scaling/compression
```

## Behavioral findings to preserve

### Studio/Edit

- venue tabs provide reusable rig context;
- Master Track content is designed to remain portable across venues;
- Fixture and Group tracks override broader scope;
- fixtures can be grouped, collapsed, muted, and soloed;
- music library selection loads a track and waveform;
- fixture library uses manufacturer/model/mode discovery and drag/drop;
- Positions and Attributes are reusable named semantic cues;
- Autoloops are edited in a bank/slot matrix;
- Static Looks provide per-fixture granular control.

### Live/Performance

- Autoloops synchronize to DJ beatgrid or fallback timing source;
- one manually triggered Autoloop can temporarily run over a scripted track and return naturally;
- repeat modes include infinite and track-duration behavior;
- exclusive bank filtering and return-to-all are supported;
- active Autoloop progress is visible;
- Control One feedback mirrors software state;
- connection status and hardware status are important gig-time information;
- multiple input-source types exist and may require deliberate selection;
- movement, strobe, color, and intensity overrides provide live intervention.

## Friction and modernization targets

### 1. Selected versus active ambiguity

A pad or bank may be selected for editing/navigation without being the content currently affecting output.

EmberLights requirement:

- selection border;
- active fill/progress;
- queued/pending indicator;
- disabled/filter state;
- repeat/exclusive badge;
- all states readable without relying only on color.

### 2. Settings are detached from the fault

A user may need to navigate to preferences to understand why Performance Mode is not behaving as expected.

EmberLights requirement:

- pinned status;
- click status to open the narrow owner-specific drawer;
- exact error and safe next action;
- Retry/Reconnect/Test where appropriate;
- no blocking modal over Live.

### 3. Save semantics are split

Official troubleshooting instructs users to save both lightshow and project information, which is evidence that the persistence model is not self-evident.

EmberLights requirement:

- one clear authored-project save state;
- explicit external asset/association status;
- atomic verified save and history;
- recovery draft;
- no silent persistence of live overrides.

### 4. Fixed performance surface

SoundSwitch provides learnable muscle memory, but every user inherits the same arrangement.

EmberLights requirement:

- bundled familiar reference layout;
- modern default layout;
- same commands/state underneath;
- later user buttons/pads/faders/knobs;
- responsive variants and controller-specific mappings.

### 5. Dense preferences

The Performance Mode preference screen combines mixer mode, decks, faders, crossfader behavior, loop-auto-strobe, smoothing, external mixer, Autoloop policy, and MIDI sync.

EmberLights requirement:

- organize settings by outcome and owner;
- show current activation status;
- disclose advanced options progressively;
- searchable Command/Settings Explorer;
- preserve expert access without making the common path a wall of checkboxes.

### 6. Static Look ownership is easy to misunderstand

SoundSwitch's included/excluded fixture behavior maps to meaningful layer ownership, but inclusion controls can obscure the distinction.

EmberLights requirement:

```text
RELEASE       lower layers continue
SET(value)    this Look owns the property
FORCE_ZERO    this Look explicitly turns it off
```

This must be visible in the editor and Inspector.

## Component inventory derived from evidence

### Shared chrome

- Project/Venue tabs
- Workspace tabs
- DJ source/status badge
- hardware/output badge
- notification center
- BPM/sync indicator
- settings/account affordance
- show state and saved state

### Studio components

- Music/asset browser
- fixture/profile browser
- effect/palette browser
- venue tabs
- track hierarchy/tree
- track header
- timeline ruler/grid
- cue block
- curve/ramp editor
- waveform
- beatgrid/phrase markers
- position marker
- attribute marker
- fixture/group Inspector
- Autoloop matrix/editor
- Static Look editor
- patch/DMX chart
- transport controls

### Live components

- source selector
- health strip
- rate/size/strobe knob
- color/movement/strobe override group
- master/group/content intensity fader
- Autoloop bank selector
- Autoloop pad matrix
- active progress overlay
- repeat/exclusive badge
- Static Look matrix
- active-content summary
- Release All
- Work Light
- Blackout
- diagnostics drawer

## Preserve / Improve / Reject ledger

| SoundSwitch pattern | Decision | EmberLights treatment |
| --- | --- | --- |
| Studio/Edit versus Performance | Preserve | Two workspaces sharing objects/commands/state |
| Venue tabs | Preserve | Stable reusable venues/rigs with clearer active context |
| Master/Group/Fixture hierarchy | Preserve | Native semantic timeline hierarchy |
| Timeline + waveform + beatgrid | Preserve | Modern native complex component |
| Autoloop bank/slot matrix | Preserve | Four-bank reference window over 64×32 engine |
| Active progress | Preserve | Runner-owned state, pad and Now Playing feedback |
| Static Looks | Preserve | Explicit property ownership states |
| Performance overrides | Preserve | Modular controls and user layouts later |
| Red/green connection status | Improve | Semantic badge, text, icon, details and action |
| Fixed Performance layout | Improve | Bundled reference plus responsive/custom surfaces |
| Dense preference tabs | Improve | Outcome-based drawers/settings with advanced disclosure |
| Separate project/lightshow save mental model | Reject | Unified versioned project save and clear asset state |
| Hidden developer-only parity controls | Reject | Reference skin must expose implemented parity |
| Hardware UI defining engine capacity | Reject | Controller is a window, not a storage limit |
| Modal routine fault recovery | Reject | Non-modal actionable Live diagnostics |
| Pixel-for-pixel proprietary artwork | Reject | Original EmberLights assets and branding |

## Provisional design targets

These are EmberLights `DESIGN_TARGET` values, not SoundSwitch measurements:

- compact Studio minimum: 960×640, with some complex editing requiring larger workspace;
- SoundSwitch's documented minimum application resolution is 1280×720, so EmberLights must at least remain fully operable at 1366×768 and should provide a Safe/Live fallback below the Studio minimum;
- standard reference target: 1920×1080;
- high-DPI qualification: 125%, 150%, 200%;
- ordinary touch target in touch-live: approximately 44 device-independent pixels minimum;
- ordinary compact desktop controls may remain denser when keyboard/mouse accessible;
- status uses icon + text + semantic color;
- vivid cue colors are content data, not hard-coded widget theme colors.

## Exact native capture gaps

The following remain required before final visual-token approval:

1. current SoundSwitch exact version/build screenshot;
2. Windows 1366×768 at 100%;
3. Windows 1920×1080 at 100%;
4. high-DPI capture with recorded app client size;
5. Edit normal, selected cue, Master/Group/Fixture expanded/collapsed;
6. Autoloop Edit banks and context menu;
7. Autoloop Live selected/active/progress/exclusive/repeat;
8. Static Look Edit and Live;
9. source selection;
10. DJ connected, disconnected, reconnecting, and information popover;
11. single and multiple hardware indicators;
12. notification states;
13. preferences tabs;
14. fixture library and DMX chart;
15. keyboard/focus/disabled/hover/pressed states where visible.

## Current planning conclusion

The SoundSwitch Reference skin should reproduce the **mental map and performance muscle memory**, not the rigidity:

```text
SoundSwitch familiarity
+ EmberLights semantic core
+ VirtualDJ-style surface flexibility
+ stronger persistence/diagnostics/safety
+ responsive and accessible presentation
= migration UI that can grow beyond one vendor workflow
```
