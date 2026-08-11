# EmberLights Default UI — Information Architecture and User Journeys

Status: binding product UX specification for `EmberLights Default v0` and issue #33.

Related:

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `21_UI_IMPLEMENTATION_PROGRAM.md`
- `23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`
- `spec/ui/command-state-skin-contract-v0.md`
- `spec/ui/current-win32-command-state-inventory-v0.md`

## Product experience statement

EmberLights Default is the modern, opinionated EmberLights experience. It must remain immediately legible to a mobile DJ or lighting operator while reducing SoundSwitch’s mode fragmentation, hidden state, fixed layout, and persistence ambiguity.

The default UI should feel:

- calm and dependable in the booth;
- expressive when programming lighting;
- fast to scan under pressure;
- powerful without presenting every advanced option at once;
- consistent between authoring, rehearsal, and live operation;
- portable across VirtualDJ, future DJ integrations, controllers, and output hardware.

The default UI is not the only allowed arrangement. It proves the recommended workflow over the shared command/state/platform architecture.

## Primary user goals

1. Open or migrate a show project without reconstructing the rig.
2. Confirm fixtures, patch, groups, output, DJ sync, and controller health.
3. Create or edit Static Looks and Autoloops quickly.
4. Associate or author track-specific lighting when needed.
5. Map any controller through a searchable command model.
6. Rehearse and validate before a gig.
7. Run an event with large, immediate, safe controls.
8. Recover from DJ, controller, project, or output failure without losing the show.
9. Understand exactly what is saved, transient, disconnected, degraded, or unsafe.
10. Change skins/controllers/DJ software without rewriting lighting content.

## Application-level information architecture

```text
EmberLights
├── Project Hub / startup routing
├── Studio workspace
│   ├── Project + Venue context
│   ├── Library / Assets
│   ├── Authoring canvas
│   ├── Contextual Inspector
│   ├── Rehearsal / Preview
│   └── Utility drawers
│       ├── Connections
│       ├── Mapping
│       ├── Diagnostics
│       ├── Migration Report
│       ├── Validation
│       └── History / Recovery
├── Live workspace
│   ├── Persistent gig-health strip
│   ├── Now Playing / active-content summary
│   ├── Performance pages
│   │   ├── Home
│   │   ├── Autoloops
│   │   ├── Static Looks
│   │   ├── Moments
│   │   └── Overrides
│   ├── Group / content intensities
│   └── Non-modal drawers
│       ├── DJ / Sync
│       ├── Controllers
│       ├── Outputs
│       ├── Safety
│       └── Diagnostics
└── Safe fallback surface
```

Connections, Mapping, Diagnostics, Safety, and Migration are not alternate lighting engines or modes. They are contextual panels over the same project and Runner state.

## Startup routing

### Default launch behavior

1. Start the deterministic Runner/service boundary.
2. Validate app-local preferences and bundled Safe skin.
3. Attempt to reopen the last project when enabled and the file remains valid.
4. Validate the project and last-known-good compiled package.
5. Load the preferred skin; fall back safely if invalid.
6. Restore window/monitor/workspace state only after bounds validation.
7. Auto-connect configured inputs/controllers/outputs according to policy.
8. Present the appropriate destination:
   - **Live** when a valid active package was last running or the user selected gig-start behavior;
   - **Studio** when the project is valid but not active;
   - **Project Hub** when no project is available;
   - **Recovery** when the project or recovery draft needs a deliberate choice.

No startup path may require the user to browse for the same project every time unless reopening is disabled or the project is unavailable.

### Project Hub

The Project Hub is a sparse launch surface, not a cloud dashboard.

Primary actions:

- Continue last project;
- Open recent project;
- Open another project;
- Create project;
- Migrate from SoundSwitch;
- Recover project/history;
- Open Safe Live with a known-good package when available.

Each recent item shows:

- project name;
- venue/rig summary;
- modified and last-verified-save times;
- validation status;
- active package availability;
- missing external audio count;
- connection-profile summary;
- recovery/history warning if relevant.

## Persistent application chrome

### Studio chrome

Always shows:

- EmberLights identity;
- project name;
- saved/unsaved/recovered/save-failed state;
- active venue;
- Studio/Live switch;
- validation summary;
- DJ/sync status;
- controller count/status;
- Universe 1 and Universe 2 output status;
- notifications;
- Command Explorer;
- user/app menu.

Studio may collapse lower-priority health details at compact sizes, but faults remain visible and actionable.

### Live gig-health strip

Always visible and not skin-page-dependent:

```text
Project | Venue | DJ/Sync | BPM | Controller | U1 | U2 | Active Content | Overrides | Safety | Work Light | Blackout
```

Rules:

- text/icon/color communicate health;
- each health item opens the narrow relevant drawer;
- Blackout is spatially isolated;
- Work Light and Release All Overrides remain reachable;
- fault details do not replace the primary surface with a modal;
- output continues while drawers open or skins switch.

## Studio workspace

### Default standard layout

```text
┌───────────────────────────────────────────────────────────────────────────┐
│ Application / project / health / workspace chrome                        │
├───────────────┬──────────────────────────────────────────┬────────────────┤
│ Library       │ Authoring canvas                         │ Inspector      │
│ Assets        │ Timeline / Venue / Matrix / Mapping      │ Contextual     │
│ Search/filter │                                          │ properties     │
├───────────────┴──────────────────────────────────────────┴────────────────┤
│ Transport / waveform / rehearsal / optional utility drawer               │
└───────────────────────────────────────────────────────────────────────────┘
```

### Library / Assets dock

One searchable source for:

- Music / audio associations;
- Autoloops;
- Static Looks;
- Track Scripts;
- Palettes;
- Positions;
- Attributes;
- Effects;
- Fixtures and profiles;
- Groups;
- Imported/migrated assets;
- Favorites and recently used items.

The dock changes available filters by asset type but keeps search, sort, favorite, duplicate, and drag behavior consistent.

### Authoring canvas modes

The center canvas hosts native complex components selected by work intent:

- **Timeline** — track-specific/Master/Group/Fixture authoring;
- **Autoloop Editor** — bank/slot library and musical-step/semantic editor;
- **Static Look Editor** — target/property ownership and value editor;
- **Venue/Patch** — fixture placement, address/universe, roles, groups, validation;
- **Mapping** — controller event → command binding editor;
- **Migration Review** — imported/approximated/unsupported/conflicted assets;
- **Preview/Rehearsal** — output-safe visual and state preview.

Changing canvas mode never changes the underlying project object identity or Live state implicitly.

### Contextual Inspector

The Inspector reflects the current selection and provides:

- name/identity;
- semantic scope;
- editable properties;
- validation errors/warnings;
- command/action menu;
- relationships/dependencies;
- source/provenance where imported;
- `Learn MIDI` / `Add to Custom Panel` where appropriate;
- `What is this?` and command/state introspection.

It does not show irrelevant properties from every object type simultaneously.

### Utility drawers

Drawers are non-destructive overlays/panes and remember app-local size/position.

- **Connections** — DJ, controllers, network and USB output.
- **Diagnostics** — structured health/events/counters and report export.
- **Validation** — errors/warnings grouped by project object and severity.
- **History** — Undo/Redo and durable restore points.
- **Migration Report** — source preservation and translation confidence.
- **Command Explorer** — capabilities and bindings.

## Live workspace

### Home page

The Home page answers three questions:

1. **What is happening now?**
2. **What can I safely do next?**
3. **Is anything unhealthy?**

Default contents:

- Now Playing / active-content card;
- master intensity;
- movement rate and size;
- strobe rate/cap/status;
- color override and release;
- active moment/Static Look/Autoloop/Track Script summary;
- primary group faders;
- Release All Overrides;
- configurable quick-action strip;
- upcoming/selected pad or bank context where useful.

### Autoloops page

- pageable banks over the complete 64×32 catalog;
- four-bank quick window where appropriate;
- 32-pad selected-bank matrix;
- selected, active, progress, queued, disabled, exclusive, and repeat states;
- Previous/Next and Clear;
- All Banks / enabled-bank filter;
- one-shot, infinite, and track-duration repeat indicators;
- active loop continues visibly even if the user navigates to another bank;
- direct hardware/keyboard bindings invoke identical commands.

### Static Looks page

- searchable/groupable Look matrix;
- clear active state;
- transition/fade status;
- active Look does not hide underlying Track/Autoloop state;
- Clear returns ownership to lower layers according to the core layer model;
- optional categories such as Dinner, Formalities, Dance, Work, Venue, Custom are organizational metadata, never hard-coded engine modes.

### Moments page

Moments are user/project templates over existing commands and content, not a separate show engine.

Examples:

- Grand Entrance;
- First Dance;
- Parent Dances;
- Toasts;
- Cake Cutting;
- Open Dance;
- Last Dance;
- Send-off;
- Photographer-safe;
- Work/Breakdown.

A Moment may activate a Look, Autoloop, Track Script, palette, intensity policy, and temporary page arrangement through validated commands. It cannot bypass safety or write project content while Live.

### Overrides page

Organize by intent:

- Color;
- Intensity;
- Movement;
- Position;
- Strobe;
- White/UV;
- Beam/attribute;
- Fixture/group target;
- Release selected property;
- Release target;
- Release All.

The page always shows the active override count and which target/properties are owned by the manual layer.

## First-run journey

1. Launch EmberLights.
2. Choose **Migrate SoundSwitch**, **Create project**, or **Open project**.
3. Select DJ integration, with VirtualDJ/OS2L recommended first.
4. Select or skip output hardware; dry-run/visual preview remains possible.
5. Add/import fixtures and venue/rig.
6. Patch, group, and validate.
7. Optionally connect a MIDI controller and use Learn.
8. Create or import initial Static Looks and Autoloops.
9. Compile/activate a package.
10. Enter Rehearsal, then Live.

The first-run flow is resumable. It must not force cloud signup or a full tutorial before the user can open an existing project.

## SoundSwitch migration journey

1. Choose source project/folder through a read-only dialog.
2. Create immutable source inventory and hashes.
3. Show what can be imported, approximated, preserved only, or requires review.
4. Select destination project and venue strategy.
5. Import known fixture/venue/group/Look/Autoloop content.
6. Preserve unknown data and original source bundle references.
7. Open the Migration Review canvas.
8. Resolve fixture/profile/address conflicts.
9. Validate output-disabled imported patch until explicitly enabled.
10. Rehearse and compare against SoundSwitch where available.
11. Save an EmberLights project and durable migration report.

The migration path never mutates the user's SoundSwitch source or only audio copy.

## Venue/rig setup journey

1. Create or select venue.
2. Search/import fixture profiles.
3. Add fixtures with stable identities.
4. Assign universe/address and physical/semantic roles.
5. View overlap/range/profile validation live.
6. Create groups using direct selection/search rather than comma-separated IDs.
7. Define named positions/attributes.
8. Save/validate.
9. Test output or preview selected fixtures safely.
10. Compile/activate only when blocking errors are resolved.

## Static Look authoring journey

1. Create or duplicate a Look.
2. Name, categorize, and choose transition.
3. Select groups/fixtures/properties.
4. For each property choose:
   - `RELEASE` — lower layer continues;
   - `SET(value)` — Look owns the value;
   - `FORCE_ZERO` — Look explicitly turns it off.
5. Preview at a safe output level or virtual preview.
6. Inspect conflicts/safety caps.
7. Save as one Undo transaction.
8. Activate in Rehearsal/Live through the same command as pads/MIDI.

## Autoloop authoring journey

1. Create, duplicate, import, or choose Next Empty.
2. Name and classify the loop.
3. Assign bank/slot without silent overwrite.
4. Set musical length, repeat default, transition, and semantic targets.
5. Build steps/effects against Master, Group, or Fixture scope.
6. Preview against manual BPM or DJ clock.
7. Validate portability and unsupported fixture capabilities.
8. Save and expose immediately in the same stable bank/slot in Live.
9. Use Move/Swap/Copy explicitly; every destructive operation participates in Undo/Redo.

## Track Script journey

1. Add or identify external audio without copying/mutating the source.
2. Associate by stable content identity.
3. Validate/relink moved audio.
4. Inspect waveform and beatgrid.
5. Add Master/Group/Fixture control tracks.
6. Add semantic cues, curves, effects, Positions, and Attributes.
7. Test seek/rewind/loop behavior against available transport evidence.
8. Save and compile.
9. Rehearse automatic association or trigger manually when integration evidence is incomplete.

## Controller mapping journey

1. Open Mapping drawer/canvas.
2. Select connected device or create a generic profile.
3. Press/move a hardware control to Learn.
4. Search the Command Explorer.
5. Choose command and typed target/parameters.
6. Set interaction behavior, transform, range, inversion, encoder mode, and soft takeover.
7. Configure feedback state/LED behavior where supported.
8. Detect conflicts and unavailable targets.
9. Test without mutating lighting content unexpectedly.
10. Save mapping in the correct app/profile/project scope.

The mapping editor reads command metadata; it does not maintain a separate shadow action list.

## Rehearsal and activation journey

1. Validate project and inspect blocking/warning issues.
2. Select dry-run, preview, or real output.
3. Start Runner with the candidate package.
4. Verify DJ clock/source, both universes, controller, safety, and active package generation.
5. Exercise Looks, Autoloops, scripts, overrides, release behavior, blackout, and recovery.
6. Compare expected/actual output where possible.
7. Activate atomically as the new known-good package.
8. Retain the prior package until all activation participants acknowledge the generation.

## Gig startup journey

Provide a compact optional checklist driven by real state—not manually checked boxes:

- Project loaded and verified;
- correct venue active;
- valid package active;
- DJ source connected or fallback explicitly accepted;
- BPM/sync healthy;
- controller connected/profile recognized;
- Universe 1 ready;
- Universe 2 ready or intentionally disabled;
- fixture/output test completed where desired;
- hazards disarmed by default;
- blackout/work light tested;
- fallback and last-known-good available.

The user can enter Live with warnings after deliberate acknowledgement, but blocking safety/project errors remain blocking.

## Normal gig journey

1. Live opens on Home with complete health.
2. DJ state drives Autoloop or Track Script playback.
3. User switches pages without changing active content.
4. User launches a Look, Loop, Moment, or temporary override.
5. Now Playing shows the winning/active layers and progress.
6. Hardware LEDs and software surfaces reflect the same state.
7. Release returns to the appropriate lower layer naturally.
8. Connection faults appear in the strip and relevant drawer while output continues safely.

## Fault and recovery journeys

### DJ/OS2L lost

1. Status moves Healthy → Hold → Fallback/Manual/Safe according to sync state machine.
2. Exact track scripting pauses when identity/playhead is untrustworthy.
3. Beat-driven content continues through configured fallback when safe.
4. DJ drawer shows endpoint, last event, error, Retry, and fallback status.
5. Recovery is smooth and stateful; no mandatory project reload.

### Controller lost

1. Lighting continues.
2. Controller status degrades.
3. UI remains fully usable.
4. Reconnect/profile matching occurs automatically.
5. Feedback resynchronizes from shared state without replaying stale inputs.

### Output lost

1. Only affected universe/adapter shows fault.
2. Runner continues rendering and supersedes stale queued frames.
3. Drawer shows adapter, last error, reconnect/backoff, frame counters, and safe action.
4. Reconnect does not replay obsolete lighting.
5. Shutdown/fault safety behavior remains adapter-specific and core-controlled.

### Project or package activation failed

1. Invalid candidate is rejected.
2. Current known-good package remains active.
3. Exact validation/activation failure is visible.
4. User returns to Studio without interrupting current output.

### Skin failed

1. Invalid reload leaves the current skin active.
2. Invalid first load opens Safe fallback.
3. Runner/output remain active.
4. Error identifies package/path/schema/widget/command/state cause.
5. User can choose Default or reset layout safely.

## End-of-gig journey

1. Stop track/Autoloop/override content deliberately.
2. Use Work Light or shutdown look.
3. Confirm hazards disarmed.
4. Stop show/output safely.
5. Save authored project only if Studio changes exist; Live transient state is not silently saved.
6. Export diagnostics only when needed.
7. Close; last project and app-local workspace state are remembered.

## Empty, loading, degraded, and error states

Every complex panel defines:

- empty project;
- no selection;
- loading/indexing;
- unavailable because Runner is stopped;
- unavailable because capability is not in the active package;
- disconnected;
- degraded/fallback;
- validation error;
- permission/IO error;
- unsupported imported data;
- stale external asset;
- safe action and next step.

Do not show an empty gray panel with no explanation.

## Search and Command Explorer

A global command/search affordance can find:

- commands;
- settings;
- fixtures/profiles;
- groups;
- Looks;
- Autoloops;
- Track Scripts;
- audio assets;
- diagnostics/help topics;
- panels/workspaces.

Results show availability, current bindings, scope, parameters, and safety. Safe commands can execute directly; others navigate to the relevant context.

## Responsive behavior

### Compact

- one primary canvas;
- Library and Inspector become drawers or mutually exclusive side panes;
- gig-health strip compresses labels but retains faults and emergency controls;
- Live uses fewer simultaneous faders and pageable groups;
- no critical control clips below 1366×768.

### Standard

- Library + canvas + Inspector;
- multiple Live control groups visible;
- utility drawer at bottom/right;
- primary qualification target: 1920×1080.

### Wide

- optional simultaneous Preview/Diagnostics/Mapping;
- expanded timeline and track hierarchy;
- additional Live group faders/pad metadata;
- whitespace is used to improve grouping, not merely stretch controls.

### Touch-live

- large targets;
- simplified labels and page count;
- no hover-dependent action;
- long-press opens control context;
- destructive controls require deliberate spatial/gesture treatment.

## Accessibility and ergonomics

- complete keyboard focus order;
- Enter/Space activation for ordinary controls;
- Escape closes top-most non-destructive panel;
- focus remains visible at every theme/DPI;
- icons have accessible names;
- state is not color-only;
- numerical controls expose value/range/unit;
- reduced motion preference;
- tabular numeric alignment for BPM, addresses, frame counts, timings;
- dangerous controls are distinct and not adjacent to routine navigation;
- no critical action requires a right-click or hover only.

## Default keyboard intent

Exact bindings remain customizable, but the command model should support consistent defaults:

```text
Ctrl+N             New project
Ctrl+O             Open project
Ctrl+S             Save
Ctrl+Shift+S       Save As
Ctrl+Z / Ctrl+Y    Undo / Redo
F5                 Start/Stop show
F8                 Blackout
Space              Contextual transport play/pause in Studio only
Ctrl+K or Ctrl+P   Command Explorer
Esc                Close top panel/cancel safe operation
```

Blackout retains the established emergency path and cannot be shadowed silently by a skin binding.

## Default v0 acceptance journeys

Default v0 must demonstrate:

1. reopen last project and identify save/validation state;
2. create/import fixture profile, patch fixture, create group, validate;
3. author and activate a Static Look with Release and Force Zero distinctions;
4. author/place/move an Autoloop and launch it from Live;
5. associate a Track Script with external audio and trigger/clear it;
6. map a MIDI control using the shared Command Explorer;
7. start Live, inspect full health, launch/release content, and use blackout;
8. recover from simulated OS2L loss without stopping output;
9. recover from invalid skin load through Safe fallback;
10. switch between Default and SoundSwitch Reference with identical domain state;
11. complete the workflows at compact and standard sizes;
12. produce measured CPU/memory/startup/repaint evidence.

## Non-goals for Default v0

- public cloud account/dashboard;
- freeform visual skin designer;
- marketplace;
- full AutoScripting production workflow before its engine exists;
- pretending unimplemented parity features work;
- replacing native complex editors with generic form fields permanently;
- event-company-specific hard-coded modes.

## Implementation rule

When adding a screen or control to Default:

1. identify the user job;
2. identify the stable command(s);
3. identify shared state and update class;
4. identify persistence scope;
5. identify availability and safety;
6. define compact/standard behavior;
7. define empty/fault states;
8. define keyboard/accessibility behavior;
9. verify the equivalent domain capability can appear in the Reference skin and mappings;
10. qualify footprint and Runner neutrality.
