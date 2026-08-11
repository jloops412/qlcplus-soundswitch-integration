# EmberLights Command, State, Binding, and Skin Contract v0

Status: architecture/specification draft for implementation. This contract exists to prevent UI layouts, controllers, and DJ adapters from becoming alternate implementations of show behavior.

## Architectural rule

```text
Input surface/event
    -> Binding
    -> Typed Command
    -> Studio controller or Runner command boundary
    -> Domain state change
    -> Bounded observable state snapshot
    -> UI/controller feedback
```

Surfaces include:

- Default EmberLights skin;
- SoundSwitch Reference skin;
- keyboard;
- MIDI/HID/controller profiles;
- Control One;
- OS2L/custom DJ commands;
- future remote/touch surfaces.

The domain behavior exists once. A skin or mapping chooses how to invoke and display it.

## Required implementation layers

### 1. Command registry

A versioned registry of every callable user/domain action.

### 2. State registry

A versioned registry of observable values and capability/health state.

### 3. Binding engine

Maps a surface event/value to a command invocation and maps state back to surface feedback.

### 4. Skin package/runtime

Loads validated declarative presentation/layout packages and binds widgets to commands/state.

### 5. Native complex components

Reusable timeline, waveform, browser, matrix, mapping, patch, diagnostics, and inspector components placed/configured by skins.

### 6. Safe fallback view

A bundled minimal surface for Runner status, output health, Work Light, Release All Overrides, Stop, and Blackout. It remains available even when an optional skin fails.

## Command definition

Proposed conceptual schema:

```json
{
  "id": "autoloop.launch",
  "since": 1,
  "label": "Launch Autoloop",
  "description": "Launches one compiled Autoloop by stable bank and slot.",
  "scope": ["live", "midi", "keyboard", "external"],
  "interaction": "trigger",
  "realtimeClass": "runnerCommand",
  "parameters": {
    "bank": {"type": "integer", "min": 0, "max": 63},
    "slot": {"type": "integer", "min": 0, "max": 31}
  },
  "availability": "runner.active && autoloop.exists(bank,slot)",
  "safetyGate": null,
  "feedback": ["autoloop.active.bank", "autoloop.active.slot"],
  "deprecated": false
}
```

The actual implementation may use generated C++ types rather than JSON at runtime. The schema is the documentation/tooling contract.

## Command metadata fields

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | Stable namespaced identifier |
| `since` | yes | Registry/schema version introduced |
| `label` | yes | Localizable human name |
| `description` | yes | User/build-agent explanation |
| `scope` | yes | Studio, Live, MIDI, keyboard, external, internal |
| `interaction` | yes | trigger, momentary, toggle, latch, absolute, relative, selection |
| `realtimeClass` | yes | Studio mutation, Runner command, app-local view action, blocking utility |
| `parameters` | yes | Typed argument schema, possibly empty |
| `availability` | yes | Capability/state predicate |
| `safetyGate` | no | Core gate that cannot be bypassed by surfaces |
| `feedback` | no | Associated state keys |
| `undoable` | yes | Whether Studio document history records it |
| `persistentScope` | yes | app, project, live-transient, none |
| `deprecated` | yes | Versioning state |
| `replacement` | no | Replacement command ID |

## Command classification

### App/view commands

Examples:

```text
app.quit
app.about.open
workspace.open
view.panel.open
view.panel.close
view.panel.toggle
view.commandExplorer.open
view.skin.select
view.skin.reload
view.layout.reset
```

These never alter lighting output except through separately registered Runner commands.

### Project/document commands

```text
project.new
project.open
project.openRecent
project.save
project.saveAs
project.restoreHistory.open
project.validate
project.compile
project.activate
project.close
undo.perform
redo.perform
```

### Venue/fixture/profile commands

```text
venue.select
venue.create
venue.duplicate
venue.rename
venue.delete.request
venue.validate
profile.create
profile.duplicate
profile.importQxf
profile.update
profile.delete.request
fixture.create
fixture.update
fixture.repatch
fixture.replaceProfile
fixture.delete.request
group.create
group.update
group.delete.request
```

### Timeline/editor commands

```text
timeline.play
timeline.pause
timeline.stop
timeline.seek
timeline.zoom.in
timeline.zoom.out
timeline.zoom.fit
timeline.snap.set
timeline.selection.set
timeline.selection.clear
timeline.cue.create
timeline.cue.update
timeline.cue.delete.request
timeline.cue.duplicate
track.mute.toggle
track.solo.toggle
track.collapse.toggle
editor.tool.select
editor.tool.draw
editor.tool.range
```

### Autoloop commands

```text
autoloop.select
autoloop.create
autoloop.update
autoloop.duplicate
autoloop.move
autoloop.swap
autoloop.delete.request
autoloop.populateEmpty.request
autoloop.resetDefaults.request
autoloop.launch
autoloop.clear
autoloop.next
autoloop.previous
autoloop.repeat.set
autoloop.intensity.set
autoloop.intensity.release
autoloop.bank.select
autoloop.bank.enable.set
autoloop.bank.exclusive.set
autoloop.banks.enableAll
autoloop.window.previous
autoloop.window.next
```

### Static Look commands

```text
staticLook.select
staticLook.create
staticLook.update
staticLook.duplicate
staticLook.delete.request
staticLook.activate
staticLook.clear
staticLook.preview.start
staticLook.preview.stop
```

### Track Script commands

```text
trackScript.select
trackScript.create
trackScript.update
trackScript.duplicate
trackScript.delete.request
trackScript.activate
trackScript.clear
trackScript.intensity.set
trackScript.intensity.release
trackScript.audio.add
trackScript.audio.relink
trackScript.audio.verify
```

### Live override commands

```text
override.fixtureProperty.set
override.fixtureProperty.release
override.groupProperty.set
override.groupProperty.release
override.target.releaseAll
override.releaseAll
override.masterIntensity.set
override.masterIntensity.release
override.movementRate.set
override.movementRate.release
override.movementSize.set
override.movementSize.release
override.strobeRate.set
override.strobeRate.release
override.color.set
override.color.release
group.intensity.set
group.intensity.release
```

### Transport/sync commands

```text
transport.manualBpm.set
transport.tap
transport.fallback.forceManual
transport.fallback.returnAuto
connection.dj.reconnect
connection.dj.disconnect
connection.dj.capture.start
connection.dj.capture.stop
```

### Output and safety commands

```text
runner.start
runner.stop
output.blackout.set
output.blackout.toggle
safety.workLight.set
safety.workLight.toggle
safety.hazard.arm
safety.hazard.disarm
safety.strobeAllowed.set
safety.maxStrobe.set
safety.maxIntensity.set
connection.output.reconnect
connection.output.test.request
```

Blackout and hazard commands always pass through core authority. A skin cannot supply a different implementation.

### Mapping commands

```text
mapping.learn.start
mapping.learn.cancel
mapping.create
mapping.update
mapping.delete.request
mapping.profile.select
mapping.profile.import
mapping.profile.export
```

### Migration commands

```text
migration.inspectSoundSwitch
migration.compareSoundSwitch
migration.bundleSoundSwitch
migration.convertSoundSwitch
migration.report.open
migration.issue.resolve
```

## Realtime classes

| Class | Rule |
| --- | --- |
| `viewLocal` | UI thread/local state only |
| `studioMutation` | Document/controller mutation; Undo/history rules apply |
| `runnerCommand` | Bounded command into scheduler-owned state |
| `runnerPriority` | Non-droppable or independently authoritative command such as blackout |
| `utilityAsync` | File/import/analysis work off the UI and scheduler threads |
| `blockingForbiddenLive` | Must not execute while Live without explicit transition |

The registry must make this classification inspectable so a skin cannot accidentally bind a blocking utility as a normal performance pad.

## Invocation result

Every command invocation returns or publishes a typed result:

```text
Accepted
RejectedUnavailable
RejectedInvalidArgument
RejectedSafetyGate
RejectedQueueFull
RejectedValidation
StartedAsync(operationId)
NoOpAlreadyInState
```

User-facing surfaces translate these into consistent feedback. Queue-full or safety rejection is never silently ignored.

## State definition

Conceptual schema:

```json
{
  "key": "connection.dj.status",
  "since": 1,
  "type": "enum",
  "values": ["disabled", "connecting", "connected", "holding", "fallback", "fault"],
  "scope": "live",
  "updateClass": "event",
  "description": "Current normalized DJ transport connection state."
}
```

## State metadata

| Field | Meaning |
| --- | --- |
| `key` | Stable namespaced key |
| `type` | boolean, integer, number, string, enum, object, bounded array |
| `scope` | app, project, document, Runner, connection |
| `updateClass` | event, low-frequency, frame-throttled, on-demand |
| `description` | Human/tooling documentation |
| `sensitive` | Whether remote/logging surfaces must redact it |
| `deprecated` | Versioning state |

## Initial state catalog

### Application/project

```text
app.version
app.workspace
app.skin.id
app.skin.status
app.layout.variant
project.loaded
project.id
project.name
project.pathDisplay
project.saveState
project.lastSavedAt
project.validation
project.activeGeneration
project.recoveryAvailable
undo.canUndo
redo.canRedo
```

### Venue/authoring

```text
venue.list
venue.active.id
venue.active.name
venue.active.validation
selection.kind
selection.id
editor.tool
timeline.playing
timeline.playhead
timeline.duration
timeline.zoom
timeline.snap
timeline.selection
track.list
library.query
library.filter
```

### Runner/transport

```text
runner.state
runner.health
runner.packageGeneration
runner.jitterP99
runner.deadlineMisses
transport.source
transport.bpm
transport.phase
transport.syncState
transport.confidence
transport.activeDeck
transport.trackIdentity
```

### Connections/output

```text
connection.dj.kind
connection.dj.status
connection.dj.detail
connection.dj.lastEventAt
controller.connectedCount
controller.expectedCount
controller.status
controller.devices
output.universe[1].status
output.universe[1].adapter
output.universe[1].detail
output.universe[2].status
output.universe[2].adapter
output.universe[2].detail
```

### Live content

```text
autoloop.active.id
autoloop.active.name
autoloop.active.bank
autoloop.active.slot
autoloop.active.progress
autoloop.active.cycles
autoloop.active.repeat
autoloop.bankWindow.start
autoloop.bankWindow.items
autoloop.enabledBankMask
autoloop.exclusiveBank
staticLook.active.id
staticLook.active.name
trackScript.active.id
trackScript.active.name
trackScript.elapsedBeat
trackScript.consumedCueCount
moment.active.id
override.activeCount
```

### Safety

```text
safety.blackout
safety.workLight
safety.fogArmed
safety.hazeArmed
safety.laserArmed
safety.sparkArmed
safety.strobeAllowed
safety.maxStrobe
safety.maxIntensity
safety.fault
```

## Snapshot and subscription rules

- Runner owns live state.
- UI cannot write state directly; it invokes commands.
- Runner publishes bounded immutable snapshots.
- High-frequency state is throttled for UI/controller feedback without changing scheduler timing.
- Each widget declares state dependencies so hidden widgets do not receive unrelated updates.
- Arrays/collections are bounded or paged.
- String/path data exposed to Live is bounded and preformatted where practical.
- UI may miss intermediate visual states but must converge to the latest snapshot.
- Blackout and other priority state use an independently reliable path.

## Binding definition

Conceptual schema:

```json
{
  "id": "binding.live.autoloop.pad.1",
  "source": {
    "type": "skin",
    "control": "live.autoloop.slot.1",
    "event": "press"
  },
  "command": "autoloop.launch",
  "arguments": {
    "bank": "${context.bank}",
    "slot": "${context.slot}"
  },
  "condition": "runner.state == 'running'",
  "feedback": {
    "active": "autoloop.active.id == context.id",
    "progress": "autoloop.active.id == context.id ? autoloop.active.progress : 0"
  }
}
```

The expression language must remain declarative and bounded.

## Binding transforms

Supported initial transforms:

- constant;
- state/context reference;
- clamp;
- scale;
- invert;
- dead zone;
- linear curve;
- limited predefined response curves;
- absolute MIDI value;
- relative encoder dialect;
- press/release;
- toggle;
- soft takeover;
- alternate action with modifier/layer;
- enum/value mapping.

No unbounded loop, filesystem call, network call, reflection, or arbitrary code.

## Binding behavior model

Each binding may define:

```text
onPress
onRelease
onValue
onDoublePress
onLongPress
onModifier
feedbackState
availabilityState
```

Long-press behavior must not delay ordinary press unless the command design explicitly requires it. Exclusive-bank selection may use a visible explicit menu/action in addition to long press.

## Shared mapping principle

Skin, keyboard, and MIDI mappings should serialize through related concepts and target the same command IDs. Device-specific message details remain in the source section, not the command.

## Skin package

Proposed package:

```text
SkinName.emberskin
  manifest.json
  theme.json
  layouts/
    studio.standard.json
    studio.compact.json
    studio.wide.json
    live.standard.json
    live.compact.json
    live.touch.json
  bindings/
    skin-bindings.json
  assets/
    icons/
    images/
  localization/
    en.json
```

A ZIP-compatible container is acceptable if package limits and path traversal protections are enforced.

## Manifest v0

```json
{
  "schemaVersion": 0,
  "id": "com.emberlights.skin.example",
  "name": "Example",
  "author": "Example Author",
  "version": "0.1.0",
  "minimumAppVersion": "0.1.0",
  "workspaces": ["studio", "live"],
  "variants": [
    {"id": "studio.standard", "layout": "layouts/studio.standard.json"},
    {"id": "live.standard", "layout": "layouts/live.standard.json"}
  ],
  "theme": "theme.json",
  "bindings": "bindings/skin-bindings.json",
  "fallbackVariant": "live.standard",
  "requiredCapabilities": [],
  "assetBudgetBytes": 8388608
}
```

Exact numeric budgets are implementation decisions, but every package must have enforced limits.

## Layout model

Layout uses containers rather than absolute coordinates by default:

- Row
- Column
- Stack
- Grid
- WrapGrid
- SplitPane
- Dock
- Scroll
- Panel
- Drawer
- Overlay
- Spacer
- Separator

Absolute positioning may be permitted only in bounded design surfaces such as venue preview, not as the normal application layout model.

## Primitive widgets

- Button
- Toggle
- Pad
- Fader
- Knob
- ValueField
- Label
- Icon
- StatusBadge
- Progress
- Meter
- Tabs
- SegmentedControl
- SearchField
- MenuButton
- List
- Tree
- Table
- Tooltip/Popover anchor

## Native complex widgets

- TimelineEditor
- WaveformBeatgrid
- LibraryBrowser
- Inspector
- FixtureProfileEditor
- PatchChart
- GroupEditor
- AutoloopMatrix
- StaticLookEditor
- StaticLookMatrix
- MappingEditor
- ConnectionPanel
- DiagnosticsPanel
- MigrationReport
- PreviewVisualizer

Complex widgets expose a stable skin-facing property/event/state interface. Their internals are not authored in skin JSON.

## Widget definition

Conceptual example:

```json
{
  "type": "Pad",
  "id": "live.autoloop.slot",
  "contextSource": "autoloop.bankWindow.slots",
  "label": "${context.name}",
  "secondaryLabel": "${context.lengthBars} bars",
  "onPress": {
    "command": "autoloop.launch",
    "arguments": {"bank": "${context.bank}", "slot": "${context.slot}"}
  },
  "state": {
    "selected": "selection.id == context.id",
    "active": "autoloop.active.id == context.id",
    "disabled": "!context.enabled",
    "warning": "context.validation != 'valid'",
    "progress": "autoloop.active.id == context.id ? autoloop.active.progress : 0"
  },
  "styleRole": "autoloop.pad"
}
```

## Conditional presentation

The initial predicate language may support:

- equality/inequality;
- boolean and/or/not;
- numeric comparison;
- null/existence;
- enum membership;
- bounded collection count;
- capability checks;
- current variant/workspace.

It may not call commands, mutate state, or perform arbitrary computation.

## Theme model

Themes use semantic tokens. Widget definitions refer to roles, not raw colors where possible.

Required token families:

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

Unknown optional tokens fall back to the base theme. Missing required tokens invalidate the theme variant, not Runner.

## Responsive selection

Variant selection uses:

- client width/height;
- DPI scale;
- input mode hint: mouse/keyboard, touch, controller-first;
- workspace;
- skin capability.

Switching variants preserves:

- workspace;
- selected domain object where available;
- active Runner state;
- open non-modal panel intent when the target variant supports it;
- focus where practical.

It does not restart Runner or mutate project content.

## Package validation

Before activation, validate:

- manifest/schema versions;
- package size and file count;
- path normalization/path traversal;
- asset types and dimensions;
- widget count and nesting depth;
- unique IDs;
- known widget types;
- required properties;
- command IDs and argument types;
- state keys and compatible types;
- predicate complexity;
- variant minimum size;
- focus/navigation reachability for critical controls;
- mandatory Live safety controls or platform-provided safety overlay;
- referenced assets and hashes;
- localization fallback.

## Failure behavior

- package parse failure: retain current skin;
- first-load failure: load bundled safe skin;
- one invalid optional variant: disable that variant and use fallback;
- unknown deprecated command with registered replacement: migration warning and controlled rewrite only in Studio;
- unknown command without replacement: validation error;
- runtime widget fault: isolate widget, report diagnostics, preserve Runner and safety overlay;
- asset decode failure: placeholder, bounded error, no repeated hot-loop decoding.

## Security

A skin cannot:

- execute native or scripting code;
- load DLLs;
- access arbitrary filesystem paths;
- issue network requests;
- enumerate USB/MIDI devices directly;
- bypass permission or safety checks;
- create unbounded threads/timers;
- allocate on the DMX scheduler;
- hold scheduler locks;
- replace the platform blackout implementation;
- modify compiled show memory;
- conceal a platform-enforced active blackout indicator.

## Versioning

### Command/state IDs

- stable once public;
- additive changes preferred;
- removals require deprecation period and replacement metadata;
- parameter changes use a new command version/ID when incompatible;
- aliases are resolved during skin compilation, not in the scheduler hot path.

### Skin schema

- schema version is explicit;
- Studio may upgrade a copied skin package and preserve the original;
- Runner loads only supported/precompiled schema;
- migration reports exact rewrites and unresolved controls;
- bundled skins are tested against every supported command/state schema.

## Tooling

Required developer/user tooling:

- Command Explorer;
- State Explorer in Diagnostics;
- skin validator CLI;
- skin package inspector;
- binding conflict report;
- layout tree inspector in development builds;
- golden screenshot runner;
- focus/navigation test;
- package performance report;
- fallback test harness.

## Migration from current Win32 UI

The current `windows_app.cpp` should not be discarded in one rewrite. Use a strangler migration.

### Phase 0 — inventory

- map every `ControlId` and menu/accelerator to a proposed command;
- map every displayed value to a state key;
- classify each action by realtime class and persistence scope;
- identify direct callbacks that combine validation, persistence, and view work.

### Phase 1 — command facade

- add typed command definitions;
- route current Win32 `WM_COMMAND` handling through the facade;
- preserve existing behavior and tests;
- add invocation result logging;
- move F8 blackout into registered default binding while preserving the accelerator.

### Phase 2 — state facade

- publish app/document/Runner snapshots through state keys;
- make current Win32 controls subscribe/read from the facade;
- stop deriving active progress/timing in controls;
- add State Explorer.

### Phase 3 — controller separation

- extract project/Studio controller actions from window code;
- extract app-local preference service;
- separate view navigation from project mutation;
- make Connections/Safety fields use explicit persistence services instead of page-local Apply logic where safe.

### Phase 4 — skin runtime

- implement validator and immutable view graph;
- implement containers/primitives;
- wrap existing native complex editors as components;
- add safe fallback surface;
- load `EmberLights Default v0` externally/bundled.

### Phase 5 — reference skin

- implement `SoundSwitch Reference v0` using the same registry/components;
- run parity journeys and golden tests;
- compare performance with Default;
- prohibit new domain behavior from being added only to one skin.

### Phase 6 — customization

- control introspection;
- Custom Panel and custom controls;
- binding editor;
- visual designer later.

## Acceptance tests

### Contract tests

- every registered command has unique ID and valid metadata;
- every binding argument type matches command schema;
- every referenced state key exists and type-checks;
- deprecated ID migration is deterministic;
- safety-gated commands reject bypass attempts;
- Runner commands use bounded transport;
- Studio undoable commands create correct history entries.

### Skin tests

- load both bundled skins;
- switch skins while DMX runs;
- invalid package falls back safely;
- package limits are enforced;
- critical controls remain reachable;
- focus traversal completes;
- high-DPI variants select correctly;
- no missing localization crashes;
- golden layouts pass within approved tolerances.

### Cross-surface equivalence tests

For representative actions, invoke from:

- Default skin;
- SoundSwitch Reference skin;
- keyboard;
- MIDI mapping;
- direct test command.

Verify identical domain result and feedback state for:

1. blackout;
2. work light;
3. launch/clear Autoloop;
4. select exclusive bank;
5. restore all banks;
6. activate/clear Static Look;
7. set/release group intensity;
8. release all overrides;
9. set/tap BPM;
10. reconnect DJ source.

## V0 completion gate

The contract is implemented when:

- current hard-coded UI behavior is routed through command/state facades;
- two external/bundled skins render through the skin path;
- keyboard/MIDI representative bindings use the same commands;
- safe fallback is proven;
- exact validation/errors are exposed;
- skin switching is Runner-neutral;
- high-DPI and performance gates pass;
- no new UI callback bypasses the registry without an explicitly documented platform-only reason.
