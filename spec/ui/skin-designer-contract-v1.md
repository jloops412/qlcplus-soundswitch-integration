# EmberLights Skin Designer Contract v1

Status: **provisional binding product and authoring contract** for custom controls, overlays, and full skin creation.

Related:

- `command-state-skin-contract-v0.md`
- `emberskin-package-and-safety-limits-v0.md`
- `native-component-contracts-v0.md`
- `user-customization-and-action-composition-v0.md`
- `ember-actions-contract-v1.md`
- `schema/layout.schema.json`
- `schema/skin-manifest.schema.json`
- `../../docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`

## 1. Purpose

The Skin Designer lets operators and advanced users author EmberLights control surfaces without hand-editing package files.

The designer edits toolkit-neutral source documents and compiles them into validated runtime artifacts. It never becomes a second lighting engine, does not own show timing, and is not required in the lean Perform/Runner process.

## 2. Authoring levels

### Level 0 — Bindings

- keyboard shortcut editor;
- MIDI/HID/controller Learn;
- typed command/action arguments;
- transforms, encoder modes, soft takeover, modifiers/layers;
- controller feedback mapping;
- conflict detection;
- profile import/export/reset.

### Level 1 — Custom Controls and Pages

- add controls to declared customizable slots;
- add/reorder/resize responsive custom pages and pad banks;
- configure label/icon/accent/size/accessibility;
- bind registered commands, states, and Ember Actions;
- save as immutable-base overlay;
- reset/import/export/relink.

### Level 2 — Layout Overlay

- move/resize approved base regions;
- show/hide optional regions;
- adjust split/dock/tab/page order;
- define responsive overrides;
- retain mandatory Live/health/safety regions.

### Level 3 — Full Skin

- create/fork complete `.emberskin` packages;
- edit full hierarchy, theme, variants, components, actions, bindings, localization, and assets;
- validate/package/export/install;
- compare and migrate against base/registry updates.

The same project model supports all levels. Promotion to a broader level preserves stable IDs, source history, and provenance.

## 3. Designer source project

The editable source project is not automatically the distributable skin.

It contains:

```text
project manifest
base-skin/fork relationship
workspace and variant source trees
reusable component definitions
semantic theme source
bindings and controller-association source
action source and dependencies
localization source
asset source/provenance
simulation scenarios
validation suppressions with rationale
source history/recovery metadata
export targets
```

The final container/extension is an implementation decision, but it must:

- remain deterministic and versioned;
- keep editable metadata distinct from runtime artifacts;
- avoid embedding secrets or arbitrary executable content;
- support read-only inspection and migration;
- preserve unknown future-compatible metadata only where explicitly allowed;
- produce reproducible packages from the same source and registry/toolchain versions.

## 4. Compilation outputs

One designer project may produce:

- full `.emberskin`;
- `.emberoverlay`;
- one or more `.emberaction` artifacts or an action pack;
- controller profile(s);
- preview images/metadata;
- compatibility and dependency report;
- source migration/provenance report;
- optional project recommendation manifest.

Runtime meaning is determined by validated compiled artifacts, not hidden designer metadata.

## 5. Project identity and inheritance

Each source project declares:

- stable project ID;
- author/owner and version;
- base skin ID/version range if forked or overlay-based;
- supported workspaces and variants;
- compatible app/schema/registry ranges;
- required components/capabilities;
- export artifact IDs and versions;
- content/provenance hashes.

### Immutable base rule

Bundled and installed third-party base skins remain immutable. User changes are stored as overlays or forks.

### Overlay rules

An overlay may modify only declared slots/regions/properties permitted by the base contract. Full-skin forks may replace broader structure but retain source provenance and compatibility diagnostics.

### Stable element IDs

All bindable/customizable/base-referenced elements use stable semantic IDs. Generated list item instances use stable context IDs and are not addressed by visual index.

## 6. Workspace model

Initial workspaces:

```text
studio
live
safe-preview
```

Safe is app-owned and cannot be replaced by a user package. The designer may preview Safe and validate recovery transitions, but cannot author the trusted fallback implementation.

Variants may include:

```text
compact
standard
wide
touch-live
high-contrast
```

A package may define fewer variants only when the runtime can select a compatible fallback and mandatory journeys still pass.

## 7. Layout and canvas

The designer exposes the accepted layout model:

- Row;
- Column;
- Stack;
- Grid;
- WrapGrid;
- Dock;
- SplitPane;
- Scroll;
- Panel;
- Drawer;
- Tabs;
- Overlay;
- Spacer;
- Separator;
- approved virtualized/repeater containers.

### Required editing operations

- drag/drop and keyboard insert;
- cut/copy/paste/duplicate/delete;
- multi-select and grouping;
- parent/reparent and hierarchy editing;
- grid/snap/guides/rulers;
- align/distribute/equal size/equal spacing;
- min/max/preferred dimensions;
- row/column/span/gap/padding/margin;
- grow/shrink/wrap/dock/anchor/overflow;
- breakpoint and variant inheritance;
- visibility and availability predicates;
- focus/tab order;
- reusable component extraction/instance editing;
- locked/reference layers;
- diff against inherited/base values.

Absolute coordinates are allowed only inside explicitly bounded artboard/overlay components. Ordinary application layout remains responsive and constraint-driven.

## 8. Responsive and DPI preview

The designer previews at minimum:

- 1366×768;
- 1600×900;
- 1920×1080;
- 2560×1440;
- 3840×2160;
- 100%, 125%, 150%, 175%, and 200% DPI where supported;
- mouse/keyboard and touch input modes;
- compact/standard/wide/touch-live variants.

Authors can define custom preview dimensions, but package validation always runs the project qualification matrix.

Preview must identify:

- clipping/overflow;
- unreachable controls;
- undersized targets;
- text truncation;
- focus traps/order problems;
- excessive active nodes/subscriptions;
- hidden mandatory health/safety paths;
- unsupported component/variant combinations.

## 9. Component palette

### Primitive controls

- Button;
- Toggle;
- Pad;
- Fader;
- Knob;
- ValueField;
- Label;
- Icon;
- StatusBadge;
- Progress;
- Meter;
- Tabs;
- SegmentedControl;
- SearchField;
- MenuButton;
- List;
- Tree;
- Table;
- Tooltip/Popover anchors.

### Native complex components

Use registry-backed native components for product-shaped complexity:

- TimelineEditor;
- WaveformBeatgrid;
- LibraryBrowser;
- Inspector;
- FixtureProfileEditor;
- PatchChart;
- GroupEditor;
- AutoloopMatrix;
- StaticLookEditor;
- StaticLookMatrix;
- MappingEditor;
- ConnectionPanel;
- DiagnosticsPanel;
- MigrationReport;
- PreviewVisualizer;
- future approved performance components.

The palette derives metadata, properties, events, states, capabilities, version, performance class, and usage guidance from the Component Registry.

## 10. Reusable components and templates

Users may create reusable components from validated primitives/native-component configurations.

Rules:

- no recursive/cyclic expansion;
- bounded instance/expansion depth;
- explicit public properties/events/slots;
- stable IDs and version;
- local/package scope;
- dependency manifest;
- no hidden command/state access beyond declared dependencies;
- base/template update diff and migration.

Bundled templates should include:

- Live health/safety strip;
- Autoloop four-bank window;
- Static Look page;
- Performance Override panel;
- Moments/custom pad page;
- connection drawer;
- diagnostics summary;
- Studio header/library/inspector patterns;
- compact and touch navigation patterns.

## 11. Theme editor

The designer edits semantic tokens rather than arbitrary per-control colors by default.

Token groups:

- app/window/background/surface/elevated surface;
- primary/secondary/accent;
- content category colors;
- selected/active/queued/focus;
- success/info/warning/danger/fault;
- blackout/work-light/safety/hazard;
- text/muted/disabled/inverse text;
- border/divider/shadow/elevation;
- spacing/radius/target size;
- typography family/scale/weight using approved fonts;
- motion duration/easing with reduced-motion alternatives.

### Theme tools

- color picker and numeric entry;
- swatches/palette generation;
- token references and inheritance;
- usage search;
- state comparison;
- contrast and non-color-state diagnostics;
- light/dark/high-contrast variant comparison where supported;
- content-color versus status/safety-color collision warnings.

Per-control overrides are allowed only where the schema permits and remain inspectable.

## 12. Icons and assets

The designer provides:

- bundled icon library with semantic names;
- search, categories, preview, and state variants;
- bounded original/licensed SVG/raster import;
- crop/scale/fit/tint metadata where supported;
- asset usage and duplicate detection;
- attribution/license/provenance fields;
- package budget and decode preview;
- missing/invalid asset fallbacks.

V1 does not embed arbitrary fonts, scripts, shaders, animated formats, remote URLs, or executable assets.

User-owned reference screenshots may be added as locked non-exported design references unless explicit license/export permission is supplied.

## 13. Localization editor

Required features:

- localization key creation/search/rename with compatibility warning;
- default-locale value;
- additional locale tables;
- missing/unused key diagnostics;
- plural/format schema only when the runtime contract supports it;
- preview with long strings, RTL where supported, and pseudolocalization;
- accessibility-name localization;
- export/import of locale files without changing IDs.

Display text is never used as a command, state, component, action, or target identifier.

## 14. Binding editor

The binding editor is registry-driven.

For a selected control/event it shows:

- compatible commands/actions;
- human label/description plus stable ID;
- argument types, units, ranges, defaults, and pickers;
- availability/capability requirements;
- realtime class;
- persistence and Undo behavior;
- safety class/gate;
- invocation results;
- feedback states;
- deprecation/replacement;
- current software/keyboard/MIDI/controller bindings and conflicts.

It supports:

- press/release/value/long/double/encoder/modifier entry points;
- constant/context/state/input arguments;
- transform chains;
- stable project target picker/relink;
- Command/State/Action Explorer search;
- MIDI/HID/keyboard Learn;
- controller feedback Learn and output mapping;
- copy binding between compatible controls;
- conflict and unreachable-binding diagnostics.

No binding writes domain state directly.

## 15. Action Graph editor

The Action Graph editor implements `ember-actions-contract-v1.md`.

Required features:

- typed node palette;
- ports and compatibility checking;
- sequence/branch/parallel visualization;
- command/action/state search;
- typed argument/property inspector;
- result branches and feedback outputs;
- dependency and required-capability view;
- node/branch/call-depth/resource meters;
- cycle and mixed-realtime-class diagnostics;
- simulation and node-level trace;
- collapse into reusable action;
- visual/text split view and deterministic formatter;
- round-trip equality/digest display for expert source edits.

The editor cannot create a node unavailable in the canonical action schema.

## 16. State and result simulation

The designer ships deterministic scenario fixtures for:

- project lifecycle and validation;
- Runner lifecycle/health;
- DJ transport and fallback;
- controller and output states;
- Autoloop selected/queued/active/progress/repeat/filter/overlay/replace;
- Static Look active/held/released/missing;
- Track Script state;
- override ownership/count;
- Blackout, Work Light, hazards, and faults;
- command Accepted/NoChange/Unavailable/MissingTarget/QueueFull/SafetyRejected/StartedAsync/InternalError;
- empty, small, maximum, missing, and deprecated content collections.

Simulation state is isolated from live state and cannot invoke commands.

### Explicit live-safe test

A separate Test action may invoke the real binding through normal command validation and safety gates. The UI must clearly indicate real output, active project, target, and exit/stop path. Hazardous commands require normal arming/interlocks. The designer cannot own adapter handles or bypass Runner.

## 17. Accessibility editor

The designer provides:

- accessible name/description fields with metadata defaults;
- icon-only warnings;
- focus-order canvas/list;
- keyboard operation preview;
- target-size checks;
- text/non-text contrast;
- non-color active/queued/fault checks;
- reduced-motion preview;
- high-contrast preview;
- screen-reader/UI Automation property preview where toolkit supports it;
- mandatory-control reachability test;
- localized accessibility-string coverage.

Validation errors that make Live unsafe or unusable block activation.

## 18. Performance inspector

The designer shows estimated and measured package/runtime costs:

- active/total nodes;
- tree/component depth;
- bindings and actions;
- state subscriptions by update class;
- native component instances;
- assets/decoded pixels/bytes;
- layout/repaint hot regions;
- hidden-panel subscriptions;
- focusable controls;
- compiled graph/action memory;
- validation/compile/first-frame times from preview runs.

Warnings and hard limits derive from package/action/component policies. The inspector cannot waive hard security/safety limits.

## 19. History, save, recovery, and source control

Required:

- document Undo/Redo;
- autosave draft and crash recovery;
- explicit Save and Save As/Fork;
- deterministic formatting/serialization;
- source version and migration history;
- compare current versus last saved, active package, base package, and imported source;
- stable IDs preserved across visual reordering;
- conflict-aware base update/merge;
- export reproducibility report;
- no automatic activation of an invalid or partially compiled draft.

Git integration may be added later but is not required for ordinary users.

## 20. Validation and activation

Validation pipeline:

```text
source schema
  -> IDs/references/inheritance
  -> base/slot/overlay permissions
  -> layout/component expansion and limits
  -> theme/localization/assets
  -> command/state/component/capability/action resolution
  -> binding/action type/safety/realtime/persistence checks
  -> mandatory-control/focus/accessibility checks
  -> responsive/DPI scenarios
  -> simulation smoke tests
  -> compile View Graph/Action IR/Binding Tables
  -> hidden candidate instantiation
  -> transactional activation or package output
```

Failed first skin activation reaches Safe. Failed update/reload retains the current valid skin. Runner/project/show package are not reloaded by UI activation.

## 21. Import and migration workspace

Imported source opens in a separate evidence/review workspace showing:

- source files/version/hash/provenance;
- parsed layout/assets/actions/mappings;
- exact, translated, approximated, opaque, unsupported, and conflicted items;
- source versus proposed Ember representation;
- target/capability relink choices;
- license/export warnings;
- preserved unknown source data;
- deterministic re-import status;
- final validation before creating a designer project/package.

Source artifacts remain read-only and are never executed.

## 22. Process and fault isolation

The designer may run inside Studio initially, but architecture must allow a separate authoring process.

- Perform/Runner does not require designer libraries.
- Source parsers, asset decoders, and migration workers are cancellable and bounded.
- Designer crash does not stop Runner or DMX.
- Live current skin remains usable while a candidate compiles.
- Activation uses generation IDs and atomic swap.
- stale worker results cannot replace a newer draft/candidate.
- full paths and local identities remain owner-diagnostic data, not normal Live state.

## 23. Required qualification journeys

1. Add and persist a Static Look pad to a bundled-skin slot.
2. Add Autoloop pads with selected/queued/active/progress/repeat feedback.
3. Add a group-intensity fader, MIDI Learn, LED/value feedback, and soft takeover.
4. Create compact/standard/touch variants from one overlay.
5. Fork SoundSwitch Reference and change theme/layout/pages without changing domain behavior.
6. Build one bounded multi-command Ember Action visually and round-trip through expert text.
7. Simulate every invocation result and live state without touching output.
8. Explicitly test one safe real command while Runner remains active.
9. Update the base skin and resolve preserved, migrated, optional, and conflicted overlay changes.
10. Import/export on another machine with paths/serials redacted.
11. Reject malformed/oversized/cyclic/unsafe package/action/source input.
12. Recover a designer crash/autosave draft without changing the active skin.
13. Switch/reload/fail skins under active DMX with no output interruption.
14. Pass DPI, keyboard, UIA/accessibility, target-size, non-color, and low-end performance gates.
15. Verify Default, Reference, software, keyboard, MIDI, and controller invoke identical commands/results.

## 24. V1 acceptance

- Non-programmers can create useful custom pages and overlays visually.
- Advanced users can fork/create complete responsive skins without manual package editing.
- All controls bind only registered commands, states, actions, and capabilities.
- Action Graph and expert text remain one typed canonical system.
- Base packages remain immutable, resettable, and independently installable.
- Missing/deprecated/incompatible dependencies produce exact diagnostics and relink/migration paths.
- Mandatory Live health/safety/emergency controls remain reachable.
- Invalid candidates never displace a working surface.
- Designer/importer failure cannot stop Runner/DMX.
- Export/import preserves intended scopes and redacts machine-local identity.
- Runtime packages remain inside security, memory, startup, repaint, accessibility, and scheduler-jitter limits.
