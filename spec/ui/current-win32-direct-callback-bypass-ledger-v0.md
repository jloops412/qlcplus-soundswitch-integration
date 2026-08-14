# Current Win32 direct-callback bypass ledger v0

Baseline: UI issue #31, G1A Live/safety facade extraction.

## Migrated through the typed facade

- Show Start / Stop / Toggle (`show.*`).
- Emergency blackout set/toggle (`output.blackout.*`), retaining the atomic
  non-droppable Runner authority.
- Work Light set/toggle (`output.workLight.*`).
- Release All manual overrides (`override.releaseAll`).
- Manual BPM and Tap (`transport.*`).
- Hazard arm/disarm and Disarm All (`safety.hazard.*`).
- Static Look activate/toggle/hold/clear (`staticLook.*`).
- Autoloop launch/clear/next/previous and bank filters (`autoloop.*`).
- Track Script start/clear (`trackScript.*`).
- Fixture/group property override set/release with active-profile capability
  and safety checks (`fixture.override.*`, `group.override.*`).

The Win32 buttons and F5/F8 accelerators invoke the same facade used by direct
tests. Invocation returns an explicit accepted/no-change/unavailable/invalid/
queue-full/safety/unsupported/internal result.

## Temporary direct callbacks

| Area | Current bypass | Reason retained | Follow-up |
| --- | --- | --- | --- |
| Connections | Save/apply/restart, OS2L, MIDI, and output settings are Application-owned. The existing timed Diagnostics view and Copy/Save actions also directly request the read-only latest Runner output snapshot, format it with current project metadata, and join a recognized IR-4 look to embedded Raw Hardware Test audit evidence. | Requires persistence/result and adapter-health commands; Control One health is now exposed. CR-3/CR-4 need installed evidence before the full broker/skin state migration. `RunnerFrameInspector` and `RunnerRawHardwareParity` are toolkit-neutral, bounded, read-only, deterministic, and contain no Win32, device, output, or Runner-mutation path; the Win32 host only adapts their reports. The parity report explicitly keeps host acceptance, historical observation, current project basis, and present physical response separate. | Owners: issue #31 G1C/state migration, issue #35, and issue #40 CR-3/CR-4. Diagnostics removal gate: publish generation-stamped bounded frame-inspection/parity state through the canonical broker/component contract without exposing raw mutation or per-frame UI authority. |
| Project lifecycle | New/open/save/validate/compile/activate and history call Application methods | Requires blocking-utility progress/cancellation and document state | Issue #31 G1D |
| Authoring | Profiles, patch, groups, Looks, loops, scripts, mappings, and AutoScript Studio mutate drafts/workflows directly. The structured profile table/template/default actions, general Local channel-map workbench, named compound-channel capability editor, named target-control picker, post-commit V2 named-function editor, named MIDI Learn picker, generic duplicate-and-rebind confirmation, bounded Static Look hardware-preview lease, and Studio-only OFL search/download/import also enter through this transitional host. The workbench stages add-next, safe-gap fill, and reviewed compatible direct-channel exchanges; imported/built-in sources remain duplicate-to-edit and Save Profile remains the validation/compile/rebind boundary. | Requires typed Undo-aware Studio/profile/mapping commands plus registered progress/result state. Profile descriptors, 53 parameter choices, named ranges, target-specific choices, safety/access rules, stable binding resolution, audits, transactional mutations, atomic rebind, exact Autoloop event proposals, persistent controller mapping plans, migration-readiness evidence, bounded fixture-function component snapshots, and deterministic exact fixture/group Action plans live in toolkit-neutral `fixture_parameter_catalog` / `fixture_profile_editor` / `fixture_capabilities` / `fixture_function_component` / `fixture_control_action` / `static_look_authoring` / `autoloop_fixture_controls` / `fixture_controller_binding` / `migration_portability_review`; the Win32 controls only adapt those services. The Live picker and fixture-function invocation builder construct existing registered atomic fixture/group override commands and add no private Runner path. The bridged component model is not skin-runtime activation, and the Action planner is not persistence or activation. AutoScript uses `StudioAutoloopAutoscriptWorkflow`; named V2 edits preview through the output-disabled `StudioPreviewService` and commit through the existing persisted-source document transaction; physical preview is isolated behind `StaticLookPhysicalPreviewService`; OFL network work stays off Runner. Regressions include `fixture_profile_editor_tests`, `fixture_function_component_tests`, `fixture_control_action_tests`, `static_look_authoring_tests`, `live_ui_tests`, `static_look_physical_preview_tests`, `ofl_fixture_catalog_tests`, `autoloop_fixture_controls_tests`, `fixture_controller_binding_tests`, `migration_portability_review_tests`, and `core_tests`. | Owners: issue #31 G1E, AutoScript issue #60, fixture issue #52, Ember Actions issue #65, and bindings issue #66. Removal gate: registered Studio authoring/profile/catalog/preview/mapping/Action commands and authoritative state replace these Win32 callbacks without changing backend proposal, safety, exactness, provenance, or immutable Live-snapshot contracts. |
| Navigation | Fixed Win32 page selection | Current shell remains the strangler host until skin runtime | Issue #31 G1F / #32 |

Static Look Hold is facade-routed but remains **ineligible for cross-surface or
Control One mapping** until Runner activation owner/generation semantics reject
stale releases. Mouse/keyboard Toggle remains the supported transitional UI
behavior.

No new direct callback should be added unless this ledger records its bounded
reason, owner, and removal gate.
