# User Customization and Action Composition v0

Status: future-facing architecture contract. Not a blocker for the first bundled-skin milestone; it prevents the initial runtime from closing off VirtualDJ-style customization later.

Related:

- `command-state-skin-contract-v0.md`
- `binding-definition.schema.json`
- `emberskin-package-and-safety-limits-v0.md`
- `../../docs/18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `../../docs/21_UI_IMPLEMENTATION_PROGRAM.md` G6
- issue #38

## Objective

Allow a DJ or lighting operator to customize software and hardware control surfaces without writing application code, modifying the lighting engine, or forking a skin package.

The intended progression is analogous to the useful parts of VirtualDJ custom buttons, pad pages, controller mappings, and skins, but with stronger typing, safety, validation, portability, and deterministic Runner boundaries.

A user should eventually be able to:

- add a button, pad, fader, knob, status value, or small panel;
- choose a command from the Command Explorer;
- bind typed target/arguments;
- assign keyboard/MIDI/HID input and feedback;
- customize label/icon/accent/size/placement within approved areas;
- create reusable pad pages and control sets;
- export/import a user overlay or controller profile;
- fork a bundled skin into a full custom skin later;
- switch DJ software/controller without rewriting show content.

## Architectural layers

```text
Bundled or third-party base skin
        +
App-local user layout overlay
        +
Project-preferred optional overlay/profile selection
        +
Controller/keyboard bindings
        +
Typed command/state registry
        =
Rendered and validated user surface
```

The base package remains immutable. User changes are stored as overlays so Reset, updates, compatibility checks, and comparison remain possible.

## Customization levels

### Level 0 — Binding customization

Available first:

- keyboard shortcut editor;
- MIDI/HID Learn;
- command/target/behavior selection;
- transforms, inversion, relative mode, soft takeover;
- feedback state mapping;
- conflict detection;
- profile import/export/reset.

Does not change layout.

### Level 1 — Custom Control Panels

First end-user UI customization milestone:

- designated user panel(s) in Default/Reference;
- add Button, Toggle, Pad, Fader, Knob, Status, Progress, Label, Separator;
- choose command/state from registry;
- set typed arguments/context;
- configure label/icon/accent/size;
- reorder/resize within a bounded responsive grid;
- create named pages/pad banks;
- duplicate/delete/reset/export.

No arbitrary panel docking or base-layout mutation yet.

### Level 2 — Layout Overlay

- move/resize approved existing regions;
- show/hide optional panels;
- configure split ratios and dock locations;
- create responsive variants from an existing base skin;
- configure Live page order and quick actions;
- save named workspace layouts.

Mandatory health/safety regions cannot be removed; they may use approved compact presentation.

### Level 3 — Full Skin Designer

- create/fork complete `.emberskin` packages;
- component library and hierarchy editor;
- semantic token/theme editor;
- responsive constraints/variants;
- localization and asset management;
- validation, preview, export/package.

This follows stable schemas, two bundled skins, qualification, and user-overlay experience.

## User overlay model

Provisional package:

```text
<skin-id>.<user>.emberoverlay
  manifest.json
  controls.json
  layout-overrides.json
  bindings.json
  theme-overrides.json
```

### Overlay manifest

Records:

- overlay ID/version;
- base skin ID and compatible version range;
- minimum skin schema/app version;
- author/local owner identity optional;
- modified components/regions;
- required commands/states/components;
- content hash;
- app-local/project association policy.

### Overlay resolution

1. Load and validate base skin.
2. Validate overlay against base IDs/slots and current registries.
3. Apply additive controls and approved overrides into a candidate view graph.
4. Re-run graph, focus, safety, command/state, and package limits.
5. Activate transactionally.
6. On failure, retain base/current view and report exact overlay problem.

An incompatible overlay never corrupts or modifies the base skin.

## Customizable slots

Base skins declare explicit slots such as:

```text
live.quickActions
live.customPage1
live.customPage2
live.bottomStrip
studio.toolbar.custom
studio.inspector.customActions
studio.utility.custom
```

Slot definition includes:

- allowed widget/component types;
- maximum items/nodes;
- grid/size behavior;
- supported workspaces/variants;
- minimum target/accessibility rules;
- whether a control may be hidden;
- mandatory neighbors and safety separation;
- performance/update budget.

A future full designer can replace broader layout, but Level 1 cannot write outside declared slots.

## Control editor flow

1. Enter **Customize Controls** from app menu or long-press/right-click in an approved slot.
2. Live output continues; editing operates on a candidate overlay.
3. Add or select a control.
4. Choose control type.
5. Search Command Explorer or State Explorer.
6. Choose command/state and typed arguments/format.
7. Configure interaction, label, icon, accent, size, and accessibility name.
8. Optionally Learn keyboard/MIDI and feedback.
9. Preview accepted/unavailable/rejected states using a deterministic simulation.
10. Validate all variants/focus/safety.
11. Save and activate overlay transactionally.
12. Cancel discards the candidate; Reset removes overlay changes.

Customization mode cannot invoke live commands accidentally. Test invocation requires an explicit safe Test action and obeys normal safety/availability.

## Custom control definition

Conceptual example:

```json
{
  "id": "user.first-dance-look",
  "type": "Pad",
  "slot": "live.quickActions",
  "label": "First Dance",
  "icon": "heart",
  "accent": "content:staticLook:first-dance",
  "binding": {
    "command": "staticLook.activate",
    "arguments": {
      "lookId": "static-look:first-dance"
    },
    "behavior": "trigger"
  },
  "feedback": {
    "state": "staticLook.active.id",
    "equals": "static-look:first-dance"
  },
  "accessibility": {
    "name": "Activate First Dance static look"
  },
  "variants": {
    "compact": { "columnSpan": 1 },
    "touch-live": { "columnSpan": 2 }
  }
}
```

Exact schema is derived from the main skin/widget and binding schemas, not a separate behavior system.

## Command selection UX

The Command Explorer groups commands by job:

- Project and workspace;
- Show/Runner;
- Autoloops;
- Static Looks;
- Track Scripts;
- Moments;
- Fixtures/groups;
- Overrides;
- Movement/color/intensity/strobe/beam;
- DJ/sync/transport;
- Connections/outputs/controllers;
- Safety;
- View/panels;
- Diagnostics.

Each command displays:

- human name/description;
- ID for experts;
- availability and required capability;
- interaction types compatible with it;
- parameter controls/ranges/targets;
- persistence and Undo behavior;
- safety gate;
- current keyboard/MIDI/control bindings;
- feedback states;
- deprecation/replacement;
- `Add to Custom Panel`.

## Parameter and target binding

Arguments may be:

- constant literal;
- selected project object from an approved picker;
- context value supplied by the containing component/slot;
- bounded control value transformed from input;
- enum choice;
- current visible bank/slot context where explicitly supported.

A binding cannot refer to arbitrary memory, filesystem data, or unregistered state expressions.

### Stable target handling

Project targets use stable IDs. When a target is missing after project change/import:

- binding becomes unavailable, not redirected silently;
- UI shows missing target;
- owner may relink to a compatible object;
- original target ID remains for recovery/diagnostics;
- cross-project reusable profiles use semantic roles or parameterized targets where the command supports them.

## Pad pages and banks

A custom page contains a bounded grid/list of controls and optional page-local context.

Examples:

- Wedding Moments;
- Color Overrides;
- Venue Work Lights;
- Photographer Safe;
- Autoloop Favorites;
- Corporate Presentation;
- Fixture Tests;
- Backup/Recovery.

Pages are presentation organization only. They do not create event modes in the engine.

Page navigation can be mapped to software controls, keyboard, and MIDI. Active content remains visible globally even when its page is not selected.

## Action composition

### V0 rule

A custom control invokes **one registered command** with typed arguments. This is sufficient for the first custom panel milestone and prevents an early unsafe scripting language.

### Future Action Set

A validated `actionSet` may compose a small bounded graph of registered commands after the single-command model is qualified.

Allowed initial composition:

- ordered immediate commands;
- explicit parallel group when all commands are independently safe;
- conditional branch on approved state predicate;
- rollback policy for Studio document transactions;
- named domain command that schedules a musical-boundary operation inside Runner.

Prohibited:

- arbitrary loops/recursion;
- arbitrary code;
- filesystem/network/device calls;
- UI-thread sleep/delay as show timing;
- random/unbounded execution;
- bypass of safety/availability/priority;
- atomic claims across unrelated Runner commands without domain support;
- replaying emergency commands in loops.

### Timing

Do not implement macros as UI timers.

If a user needs:

- “launch on next beat/bar”;
- timed hold/release;
- repeat until track end;
- transition over N beats;
- event sequence;

then the command/domain model must own that timing and expose an appropriate typed command. The UI merely requests it.

### Action Set provisional limits

- maximum 32 nodes;
- maximum branch depth 8;
- no cycle;
- maximum 8 state predicates;
- maximum one project transaction boundary;
- maximum total serialized definition 16 KiB;
- all commands/capabilities validated before activation;
- emergency commands excluded except explicitly approved one-step controls.

## MIDI/controller profile model

Controller profiles are separate from skins but share binding definitions.

Profile contains:

- logical device matcher(s);
- input/output ports;
- controls and message identities;
- default bindings;
- layers/modifiers;
- feedback maps;
- optional physical layout metadata for the mapping UI;
- version/provenance/compatibility;
- owner overlays.

Changing skin does not remove a controller profile. Changing controller does not alter show content.

## Scope and portability

### App-local

- personal custom panels/layout overlays;
- keyboard shortcuts;
- device matching hints;
- theme/accessibility choices.

### Controller profile

- reusable hardware bindings/feedback independent of one project.

### Project-authored optional

- project-recommended mapping/layout profile IDs;
- project Moments/action sets built from content;
- semantic role bindings intended to travel with the show.

### Live-transient

- active page/layer;
- temporary modifier state;
- current focus/selection;
- soft takeover pickup state.

Export UI must disclose included scopes and redact local device/path identity by default.

## Import/export

Every imported overlay/profile:

- remains untrusted data;
- validates package limits and schemas;
- previews required/missing commands/states/components/targets;
- shows conflicts and replacements;
- preserves unknown compatible metadata where approved;
- does not overwrite the user's current overlay/profile without explicit choice;
- supports side-by-side install/copy/merge;
- records provenance/hash.

## Compatibility

When app/skin/registry changes:

- stable command/state/component IDs remain preferred;
- deprecated IDs use declared replacements where types remain compatible;
- missing base slot/control produces an exact overlay conflict;
- overlay can be disabled without affecting base skin;
- migration preview lists kept, changed, removed, and manual items;
- Reset always returns to the validated base skin/profile.

## Safety and mandatory controls

Customization cannot:

- remove Safe fallback;
- remove all paths to Blackout, Stop, Work Light, Release All, and Diagnostics in Live;
- make emergency controls invisible/unlabeled/unfocused;
- map a routine ambiguous gesture to a hazardous command without explicit owner approval and the core safety gate;
- persist hazard arming;
- hide fault state or replace it with decorative color only;
- intercept F8 emergency blackout delivery;
- use Action Sets to bypass command restrictions.

The runtime performs a mandatory-control reachability check for every Live variant before activation.

## Accessibility

The control editor requires:

- accessible default names from command metadata;
- warning when an icon-only control lacks a name;
- keyboard layout editing alternative;
- focus-order preview;
- target-size and contrast validation;
- non-color active/error feedback;
- touch-live validation;
- automated mandatory-control reachability.

## Performance

User controls count against the package/view graph/subscription limits.

V0 overlay limits may be tighter than full skin limits:

- 256 custom nodes total;
- 128 active nodes per variant;
- 128 bindings;
- 128 state subscriptions;
- 8 custom pages;
- 64 controls per page;
- 2 MiB asset budget;
- no custom fonts/animations/scripts.

Hidden custom pages suspend high-frequency subscriptions.

## Qualification journeys

1. Add a Static Look pad, bind and activate it, restart app, verify app-local persistence.
2. Add Autoloop bank/slot pads and show selected versus active/progress.
3. Add a group-intensity fader with MIDI Learn and soft takeover.
4. Map one controller button to Release All and verify software/controller feedback.
5. Import an overlay whose target is missing; verify unavailable/relink without silent redirect.
6. Update the base skin; migrate overlay and show conflicts.
7. Create compact and touch-live variants; verify mandatory controls and focus.
8. Load malformed/oversized overlay; retain base/current skin.
9. Switch DJ software/controller profile; retain lighting project and custom software panel.
10. Reset overlay; return exactly to bundled skin.
11. Export/import on another machine without leaking paths/serials.
12. Verify DMX and Runner remain uninterrupted during edit/preview/activation/failure.

## Sequencing

Do not implement full freeform customization before:

- #31 command/state facade;
- #37 toolkit evidence;
- #32 skin runtime and Safe surface;
- #33 and #34 bundled skins;
- #36 relevant package/switch/accessibility qualification.

Level 0 binding work may mature earlier because MIDI Learn already exists and the shared registry is foundational. Level 1 Custom Control Panels follows the bundled-skin milestone. Levels 2/3 are later.

## Acceptance for first customization milestone

1. Base skins remain immutable and resettable.
2. User controls bind only registered commands/states.
3. Overlay validates transactionally and failure preserves current view.
4. Mandatory Live safety/health controls remain reachable.
5. Software, keyboard, MIDI, and controller feedback remain consistent.
6. No UI timer becomes action/show timing authority.
7. Imported profiles/overlays are untrusted and bounded.
8. Compact/standard/touch variants, focus, accessibility, and performance pass.
9. Export/import preserves intended scope and redacts local identity.
10. Runner/DMX remain uninterrupted throughout customization and activation.
