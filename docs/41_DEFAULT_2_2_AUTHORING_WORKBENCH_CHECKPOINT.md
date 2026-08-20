# Default 2.2 authoring workbench checkpoint

> UI course correction (2026-08-14): this is a reusable authoring-model
> checkpoint with a legacy Win32 adapter, not an accepted Default 2.2 product
> UI. The model remains; further product presentation moves to the replacement
> shell defined in `44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md`.

Status: source-complete bounded authoring-model checkpoint for issue #33 and the open integration branch. This is a reusable authoring-pattern implementation over the existing project services; it is not completion of the `.emberskin` runtime, acceptance of a production toolkit, or a change to Runner/output behavior.

## Outcome

The six primary Studio asset editors now use one consistent **Library + contextual Inspector** workflow:

- Fixture Profiles;
- Fixture Patch;
- Fixture Groups;
- Static Looks;
- Autoloops;
- Track Scripts.

Each Library has bounded search, visible result counts, useful empty/no-match states, stable-ID-aware selection, and page-local create/duplicate actions. Each Inspector identifies creating, editing, read-only, and unsaved-edit states. Selection changes and **New** protect an edited draft with an explicit discard decision. `Ctrl+F` focuses the active Studio Library and `Esc` clears its filter.

The dense Static Look and Autoloop editors were also reflowed into responsive Inspector sections. Their controls and action bars stay within the supported minimum window instead of depending on a wide, flat form.

## Shared foundation

`emberlights/ui_authoring.hpp` and `ui_authoring.cpp` define the toolkit-neutral portion:

- six stable authoring resource kinds and user-facing descriptors;
- an item projection keyed by stable resource identity rather than visible row number;
- deterministic whitespace-token AND search across names, IDs, and resource metadata;
- ASCII case folding while preserving non-ASCII UTF-8 bytes;
- a 256-byte query bound and fail-closed overlong-query state;
- collection count, empty, no-match, selected, and hidden-selection summaries;
- Inspector mode/status headings;
- Compact, Standard, and Wide two-panel geometry with Standard or Wide Library emphasis.

The Win32 application is one adapter over that contract. Future Default, Reference, Safe, and user-skin renderers can reuse the projection, status, and geometry rules without importing `Page`, `ControlId`, HWND, or callback behavior.

## Default adapter behavior

- Search metadata is resource-specific: profile manufacturer/model/mode/channel functions; patch profile/role/universe/address; group membership; Look targets/properties/fades; Autoloop bank/slot/Look references; Track Script audio/cue references.
- Filtered list rows retain their original project index as item data, so save/delete/select operations never confuse a visible row with a source record.
- An existing selected resource may remain valid while hidden by the active filter; the summary says so explicitly.
- Built-in/imported profile snapshots remain visibly read-only and continue to require duplication before modification.
- New, Save, Duplicate, Delete, physical preview, profile mapping, named functions, audio linking, and all existing validation paths remain the same domain operations.
- Verbose inline instruction walls were removed from the densest Inspector layouts where contextual labels/status already carry the task.

## Registry and compatibility reconciliation

- Registry set version: `1.3.0`.
- Registry generation: unchanged at `2`.
- Commands: unchanged at `29`.
- States: unchanged at `39`.
- Components: `8`, adding bridged `ember.authoringWorkbench`.
- Capabilities: unchanged at `1`.
- Values: unchanged at `11`.
- Source digest: `a3edd0e488a49bbd15b2655be4fb2ee36506dd23a98498874e2bddbb46b790de`.
- V1-baseline classification: `compatibleAdditive`; zero breaking, removed, or manually gated changes.

The new component declares bounded query input, resource kind, collection emphasis, stable-ID visibility, Library/Inspector slots, create/save/delete/duplicate/selection/query events, virtualization, accessibility-region semantics, and Safe fallback. It does not publish a new callable command or alternate mutation path.

Because Ember Action cache and executable identities intentionally include the canonical registry digest, their deterministic golden identities were regenerated. Action source, semantics, callable contracts, and compiler generation are unchanged.

## Safety and persistence boundaries

- Project schema and serialized asset formats are unchanged.
- Runner compilation, scheduling, ownership, output routing, blackout, and hazard behavior are unchanged.
- Search and Inspector state are presentation-local and are not written into a project.
- The adapter still saves through the existing validated project transactions.
- No skin, script, component, or UI callback can write DMX directly.
- No new legacy-command bypass was introduced.

## Verification

- Focused `ui_authoring_tests` cover descriptors, stable identity, multi-token filtering, UTF-8 preservation, query bounds, summaries, Inspector modes, deterministic geometry, and containment.
- Warning-fatal full native Make suite passes.
- Registry generator, 12 registry governance tests, generated Ember Action adapter check, and surface-contract gate pass.
- Windows x64 Zig/Clang build compiles and links `EmberLights.exe` as a PE32+ GUI target.
- Native installed-Windows layout, DPI, keyboard focus traversal, Narrator/UI Automation, clean/upgrade install, and physical fixture behavior remain tester evidence gates.

## Next bounded UI pass

Continue the same pattern instead of adding isolated visual polish:

1. extract the shared create/save/delete/duplicate operations into the canonical command surface where their transaction semantics are settled;
2. add reusable field, validation-summary, action-bar, and split-pane component contracts;
3. replace remaining instruction-heavy editors with progressive disclosure and resource browsers;
4. run the issue #37 toolkit comparison against this real dense authoring workbench;
5. preserve the Safe fallback, immutable active Runner package, and shared registry/action spine throughout.
