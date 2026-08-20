# Native Complex Component Contracts v0

Status: architecture contract for issue #32 and bundled-skin issues #33/#34.

Related:

- `command-state-skin-contract-v0.md`
- `emberskin-package-and-safety-limits-v0.md`
- `../../docs/24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `../../docs/20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`

## Purpose

A declarative skin should be able to place, size, configure, and theme sophisticated EmberLights components without rebuilding their internal algorithms from generic buttons and rectangles.

Native complex components own presentation-specific interaction for one bounded domain job. They still:

- invoke the shared command registry;
- read the shared state/document view models;
- never implement lighting/render behavior;
- never become show timing authority;
- never access adapters/devices directly;
- expose a toolkit-neutral contract to skins;
- preserve keyboard, accessibility, DPI, and Safe fallback requirements.

## Component boundary

```text
Skin package
  -> NativeComponent node + validated properties/slots
  -> toolkit adapter
  -> component controller/view model
  -> command/state/document facades
```

The skin controls location, size, variant, theme tokens, visible tool slots, and approved presentation options. The native implementation controls virtualization, hit testing, drawing, editing gestures, accessibility tree, and command construction.

## Common component contract

Every native component declares:

```text
componentType
componentVersion
supportedWorkspaces
minimumSize
preferredSize
supportedVariants
requiredCommands
requiredStates
optionalCapabilities
inputModel
selectionModel
focusModel
accessibilityModel
persistenceScope
performanceClass
fallbackPresentation
```

### Common properties

```text
id
visible
enabled
variant
density
themeScope
selectionSource
readOnly
showToolbar
showStatus
emptyStateKey
errorStateKey
helpTopic
```

### Common events

Component events are either:

- typed command invocations;
- app-local selection/view events;
- bounded request events such as `openContextMenu` or `requestFileDialog` routed through approved application services.

Components do not expose arbitrary callbacks to skin code.

### Common states

```text
loading
ready
empty
unavailable
degraded
error
readOnly
editing
```

The component provides accessible names/descriptions for these states and an approved fallback placeholder.

## 1. Music and Asset Browser

Component type:

```text
ember.assetBrowser
```

### Jobs

- search and browse Music, Autoloops, Static Looks, Track Scripts, palettes, Positions, Attributes, Effects, fixtures/profiles, groups, and imported assets;
- favorite, sort, filter, duplicate, and drag/reference assets;
- show provenance, availability, missing/external status, and validation.

### Inputs

```text
assetKinds[]
query
filters
sort
selectedAssetId
project/document snapshot
external asset index snapshot
```

### Commands

```text
asset.search.set
asset.favorite.set
asset.open
asset.duplicate
asset.delete.request
asset.revealInProject
view.selection.asset.set
```

Kind-specific actions are resolved through command metadata/context menus rather than hard-coded browser forks.

### Performance

- virtualize at least 10,000 synthetic rows in toolkit spike;
- index/search work off the UI and Runner scheduler;
- incremental results with cancellation/generation ID;
- hidden browser suspends thumbnails and nonessential updates.

### Accessibility

Tree/list roles, item name/type/status, selection and expanded state, keyboard incremental search, context actions.

## 2. Fixture/Profile Browser

Component type:

```text
ember.fixtureProfileBrowser
```

### Jobs

- search manufacturer/model/mode/local/imported profiles;
- inspect provenance/validation/quarantine;
- drag or add profile to venue/patch;
- create/duplicate/import/edit local profiles.

### Required commands

```text
profile.search.set
profile.create
profile.duplicate
profile.importQxf
profile.openEditor
fixture.createFromProfile
```

### States

```text
fixtureLibrary.indexStatus
fixtureLibrary.resultCount
fixtureLibrary.quarantineCount
profile.selected.*
```

## 2A. Fixture Control Surface

Component type:

```text
ember.fixtureControlSurface
```

### Jobs

- project every ordinary direct attribute and named profile function supported
  by the selected fixture/group; never maintain a renderer-side property
  allowlist;
- provide stable Intensity, Color, Position, Beam, Image, Effect, Atmosphere,
  and Advanced-only Custom navigation plus bounded cross-field search;
- choose an appropriate control from catalog metadata: level/rate/position,
  color mixer, Pan/Tilt XY, slot/range choice, or safety-gated trigger;
- show target coverage, availability, mixed values, safety restriction, and
  explicit `RELEASE`, `SET`, or `FORCE_ZERO` ownership;
- retain the catalog's stable parameter and choice IDs across Default,
  Reference, user skins, Static Looks, Autoloops, Live, mappings, Actions, and
  migration.

Continuous named ranges submit the stable choice ID plus normalized range
position so exact per-profile values are retained. Selector choices submit the
stable choice ID. Direct controls submit the semantic property/value. Raw DMX,
profile channel order, and protected ranges are diagnostics/profile-authoring
evidence and are not an alternate ordinary-control API.

Renderers may specialize Color and Pan/Tilt visually, but specialization must
not make any other surfaced binding unreachable. Closing Advanced removes
Custom controls from the ordinary projection.

Before rendering, bindings are grouped by stable semantic parameter ID. One
property family keeps its direct value controls, continuous named ranges,
exact selector choices, ownership, coverage, availability, mixed state, and
safety together. A skin may restyle or reorder whole families but may not
scatter one property's controls into unrelated page regions or replace the
shared grouping rule. Color-emitter mixers and Pan/Tilt XY are the intentional
multi-property composite families.

## 3. Venue and Patch View

Component type:

```text
ember.venuePatchView
```

### Jobs

- display fixtures with stable identity, profile, universe/address, footprint, roles, groups, and optional physical position;
- reveal overlaps/range/profile errors;
- support selection, repatch, replacement, grouping, and safe output testing;
- provide a universe/DMX chart and optional spatial view.

### Variants

```text
table
universeChart
spatial
split
```

### Commands

```text
fixture.create
fixture.update
fixture.repatch
fixture.replaceProfile
fixture.delete.request
fixture.test.request
group.createFromSelection
view.selection.fixture.set
venue.select
```

### Safety

Fixture test is a registered safety-aware command with explicit target, bounded intensity/duration, and output availability. The component cannot write raw DMX.

### Performance

Support the V1 two universes and at least 256 fixture rows without unbounded per-frame work. Chart geometry updates only when patch/document generation changes.

## 4. Track Hierarchy

Component type:

```text
ember.trackHierarchy
```

### Jobs

- present Master, Group, and Fixture tracks;
- expand/collapse groups;
- selection, mute, solo, lock, visibility, height;
- align exactly with timeline rows;
- show scope, target, validation, and override state.

### Inputs

Immutable track/document generation, selection, view-local expansion/height model, playback/active state.

### Commands

```text
track.select
track.mute.set
track.solo.set
track.lock.set
track.create
track.delete.request
track.reorder
track.height.set
```

Track height/expansion are app-local unless explicitly saved as project-authoring metadata.

### Performance

Virtualize 256 synthetic tracks in spike; scrolling must remain synchronized with timeline without duplicate timing logic.

## 5. Timeline Editor

Component type:

```text
ember.timeline
```

### Jobs

- display musical/time ruler, tracks, cue blocks, curves, events, Positions, Attributes, playhead, loop range, selection, and snapping;
- create/edit/move/resize/duplicate/delete semantic cues;
- pan/zoom and align with waveform/beatgrid;
- expose Master/Group/Fixture precedence clearly;
- rehearse against shared transport state.

### Inputs

```text
project track/cue immutable view model
beatgrid/phrase view
transport/playhead snapshot
selection/view state
snap/zoom/range state
validation annotations
```

### Commands

```text
timeline.play
timeline.pause
timeline.stop
timeline.seek
timeline.zoom.set
timeline.snap.set
timeline.selection.set
timeline.cue.create
timeline.cue.update
timeline.cue.delete.request
timeline.cue.duplicate
track.*
```

### Editing transaction

Pointer/keyboard edits operate on a component-local draft. On commit, one typed Studio command records one coherent Undo transaction. Cancel restores the prior model. Continuous drag does not serialize/save on every pointer event unless the document-edit system explicitly supports coalescing.

### Timing authority

- playhead display reads transport state;
- UI may interpolate presentation between snapshots but cannot schedule output;
- seek/play commands request domain behavior and wait for acknowledgement/state;
- dropped UI frames do not alter cue execution.

### Rendering

- viewport-based cue/curve draw list;
- background grid cached by zoom/viewport/theme;
- offscreen tracks/cues culled;
- content colors contrast-normalized;
- selection/active/error overlays separate from cue color;
- no unbounded allocation during steady pan/playback.

### Accessibility

Expose tracks and selected/nearby cues as a structured virtualized tree/list with time/beat, type, target, value, duration, and commands. Full pixel canvas need not expose every offscreen cue simultaneously.

## 6. Waveform and Beatgrid

Component type:

```text
ember.waveformBeatgrid
```

### Jobs

- display cached waveform peaks;
- beatgrid/bar/phrase markers;
- playhead and loop range;
- zoom/pan synchronized with timeline;
- support beatgrid validation/editing when the engine exists;
- show missing/changed/relink audio state.

### Inputs

External content-identified audio asset metadata, bounded waveform cache, beatgrid model, transport, viewport.

### Commands

```text
audioAsset.add.dialog
audioAsset.relink.dialog
audioAsset.verify
beatgrid.anchor.set
beatgrid.bpm.set
beatgrid.marker.move
phrase.marker.set
timeline.seek
```

### Safety/performance

Runner never opens/decodes audio because a skin placed this component. Studio worker/cache owns decoding. Missing media yields an explicit placeholder while compiled semantic playback remains independent.

## 7. Autoloop Matrix

Component type:

```text
ember.autoloopMatrix
```

### Jobs

- display up to 32 slots for one selected bank or a configured multi-bank matrix;
- show stable bank/slot, name, content color, selected, active, progress, queued, repeat, filter, exclusive, empty, and validation state;
- launch, clear, select, duplicate, move, swap, copy, populate, and inspect according to workspace/read-only mode;
- preserve the full 64×32 catalog while supporting a four-bank reference window.

### Properties

```text
mode: studio | live
bankWindowSize
slotsPerBank
showEmptySlots
showProgress
showSlotNumbers
allowDrag
allowContextMenu
```

### Commands

Live:

```text
autoloop.launch
autoloop.clear
autoloop.previous
autoloop.next
autoloop.bankFilter.*
```

Studio:

```text
autoloop.create
autoloop.duplicate
autoloop.move
autoloop.swap
autoloop.copyToSlot
autoloop.moveToNextEmpty
autoloop.delete.request
autoloop.openEditor
```

### State

Active/progress comes directly from Runner state by stable address/ID. Navigating/selecting does not overwrite active state.

### Accessibility

Grid role with bank/slot, name, empty/selected/active/progress/repeat/filter labels and keyboard navigation. Active pad remains announced when outside the visible bank via Now Playing/status state.

## 8. Autoloop Editor

Component type:

```text
ember.autoloopEditor
```

### Jobs

- edit name/category/color/address/length/repeat/transition;
- author semantic steps/effects/targets;
- preview against clock;
- validate fixture portability and unsupported capabilities;
- move/swap without silent overwrite.

The detailed step/effect submodel remains domain-defined. The component must not hard-code one fixture brand/channel layout.

## 9. Static Look Matrix

Component type:

```text
ember.staticLookMatrix
```

### Jobs

- display/activate/clear Looks in Live;
- display selection, active, transition, category, content color, validation;
- support search/filter/favorites and controller binding hints;
- make active Look visible even outside filtered page.

### Commands

```text
staticLook.activate
staticLook.clear
staticLook.openEditor
staticLook.create
staticLook.duplicate
staticLook.delete.request
```

## 10. Static Look Editor

Component type:

```text
ember.staticLookEditor
```

### Jobs

- select fixture/group/property targets;
- display and edit ownership:
  - `RELEASE`;
  - `SET(value)`;
  - `FORCE_ZERO`;
- edit fade/transition;
- preview safely;
- explain lower-layer behavior and safety caps.

### Presentation requirement

Ownership is a first-class visible column/control, not inferred from inclusion or zero value. Values disabled by `RELEASE`/`FORCE_ZERO` remain understandable and accessible.

### Commands

```text
staticLook.assignment.setOwnership
staticLook.assignment.setValue
staticLook.fade.set
staticLook.preview.start
staticLook.preview.stop
staticLook.update
```

### Preview control contract

The reusable Studio preview surface projects `staticLook.preview.state`,
`mode`, `error`, `remainingMs`, `outputCap`, `selectedFixtureCount`,
`updateCount`, and `frameDigest`. It presents offline simulation separately
from explicitly armed physical output and always exposes a single stop action.
The renderer may invoke only the registered commands; the asynchronous Studio
service and bounded production-Runner lease own compilation, output, timeout,
hazard rejection, and terminal blackout. Preview start/stop are deliberately
unavailable to MIDI, keyboard, and Ember Actions.

## 11. Contextual Inspector

Component type:

```text
ember.inspector
```

### Jobs

- render editor fields/actions from selection type and registered property metadata;
- show validation, dependencies, provenance, scope, persistence, pending reconnect/restart, and command introspection;
- avoid one hard-coded giant property form.

### Input

Selection descriptor and inspector-schema registry supplied by domain/UI adapters.

### Skin control

Skin may configure section order, default expansion, density, header/actions slots, but cannot invent domain fields or write state directly.

### Editing

Field changes route through typed commands or a declared editor-draft transaction. The Inspector shows explicit save/commit semantics when staging is real.

## 12. Mapping Editor

Component type:

```text
ember.mappingEditor
```

### Jobs

- choose device/profile/input event;
- MIDI Learn;
- search command registry;
- configure typed parameters/target;
- behavior, range, transform, inversion, relative mode, soft takeover;
- feedback state/LED mapping;
- conflict detection and testing.

### Commands/states

Uses binding service and registry metadata. It must not maintain a separate action enum.

### Performance/safety

MIDI callbacks remain in the existing bounded adapter path. The editor receives captured events through a safe UI snapshot/queue and cannot run on the MIDI callback or DMX scheduler thread.

## 13. Connection Panel

Component type:

```text
ember.connectionPanel
```

### Variants

```text
summary
dj
controller
outputs
full
```

### Jobs

- display configured/actual status, endpoint/device, last event/error, retry/backoff, counters, scope, and required reconnect/restart;
- edit supported connection settings through typed commands;
- run safe test/capture where supported;
- never block Live with routine failure.

### Rule

Adapter implementation and lifecycle remain service-owned. Component only invokes connection/output commands and reads structured state.

## 14. Diagnostics Panel

Component type:

```text
ember.diagnostics
```

### Jobs

- structured health summary;
- event log with severity/owner/state/timestamp;
- timing/frame/adapter counters;
- active content and override state;
- validation and package/skin diagnostics;
- copy/export report;
- safe Retry/Validate commands.

### Performance

- bounded in-memory event view;
- virtualized log;
- low-frequency update for hidden panel;
- no reformatting of the entire report every 250 ms;
- text export generated on demand off scheduler.

## 15. Migration Review

Component type:

```text
ember.migrationReview
```

### Jobs

- show source inventory/hashes;
- imported/translated/approximated/preserved/unsupported/conflicted items;
- filter by type/confidence/severity;
- resolve fixture/profile/address/association conflicts;
- preserve original source references;
- enable output only after explicit validation.

The component never parses proprietary data itself; it consumes the migration service report/model.

## 16. Validation Panel

Component type:

```text
ember.validation
```

### Jobs

- group errors/warnings/info by object/type/severity;
- navigate to affected object/component;
- run validation;
- show blocking activation state;
- support copy/export.

Validation results have stable IDs/generation so stale results cannot be mistaken for current document state.

## 17. History and Recovery

Component type:

```text
ember.historyRecovery
```

### Jobs

- current save state;
- Undo/Redo stack summaries;
- durable restore points;
- recovery draft comparison;
- verified timestamps/checksum status;
- preview/restore with explicit confirmation.

File operations route through project services. The component never overwrites directly.

## 18. Now Playing / Active Layers

Component type:

```text
ember.activeLayers
```

### Jobs

- show active Track Script, Autoloop, Static Look, Moment, manual override count, blackout/work light, and fallback state;
- explain which layer owns a property at a high level;
- show progress/elapsed/cycles;
- provide Clear/Release commands when available.

This is status/command presentation over Runner-owned snapshots; it does not resolve layers itself.

## 19. Performance Parameter Controls

Component types:

```text
ember.performanceKnobGroup
ember.performanceFaderGroup
ember.overridePalette
```

### Jobs

- render common Live parameter sets from command/state metadata;
- support master/group/content/attribute controls;
- soft takeover/hardware difference indicator;
- reset/release/default behavior;
- configurable grouping through skin properties.

These can be implemented as composite native components or validated reusable skin components depending toolkit evidence. Their behavior remains registry-backed.

## 20. Preview / Visualizer Host

Component type:

```text
ember.previewHost
```

### V0 boundary

- can host a future native 2D/3D visualizer or external-frame preview;
- does not belong in Runner's mandatory Live path;
- can be absent without invalidating a skin if declared optional;
- receives semantic/frame snapshots through a bounded preview service;
- never becomes output authority.

## Component versioning

- component type + version forms the public skin contract;
- additive optional properties do not break older skins;
- required semantic change increments component version;
- runtime may provide compatibility adapters for at least one public schema generation;
- unsupported component version rejects the candidate package or uses declared optional fallback;
- toolkit-specific implementation version is internal and not written into skins.

## Native component styling

Components consume semantic tokens and approved content colors. Skins may configure documented style slots such as:

```text
header
emptyState
toolbar
status
selection
rowDensity
showLabels
showGrid
```

They may not override safety/error/focus below the design-system floors.

## Focus and accessibility

Every component provides:

- one entry focus target;
- predictable internal traversal;
- escape/back behavior;
- accessible role/name/state/value;
- virtualized accessible children where needed;
- keyboard alternative for pointer drag/edit;
- command/context menu access without right-click only;
- focus restoration after dialog/reload/variant switch.

## Component test harness

Each component ships with a deterministic harness covering:

- empty/loading/ready/unavailable/degraded/error;
- compact/standard/wide/high-DPI;
- keyboard/focus/accessibility tree;
- normal and maximum bounded data;
- invalid/stale generation data;
- command accepted/rejected/queue-full/safety-rejected feedback;
- theme/content-color contrast;
- hidden/visible subscription behavior;
- golden screenshots where visual stability matters;
- memory/repaint/scroll/pan benchmarks appropriate to performance class.

## Implementation priority

### Runtime/Safe first

1. Connection summary
2. Diagnostics summary
3. Active Layers
4. Autoloop Matrix Live
5. Static Look Matrix Live
6. Performance controls

### Default/Reference authoring foundation

7. Asset Browser
8. Inspector
9. Venue/Patch View
10. Autoloop Editor
11. Static Look Editor
12. Mapping Editor
13. Validation/History/Migration

### Timeline foundation

14. Track Hierarchy
15. Timeline
16. Waveform/Beatgrid
17. Preview host later

This order allows a real modular Live skin and current CRUD migration before waiting for the full DAW-grade timeline.

## Acceptance

1. Both bundled skins place the same native components with different layouts/theme options.
2. No native component calls adapters or lighting renderer directly.
3. Commands/state/document view models are the only product-facing boundary.
4. Components remain toolkit-neutral at the skin contract.
5. Large-data components virtualize/cull and meet spike/qualification budgets.
6. Selection and active output remain distinct.
7. Every component defines empty/fault/accessibility behavior.
8. Safe fallback relies only on trusted minimum components.
9. Component failure cannot stop Runner/DMX.
10. Versioning and fallback behavior are tested.
