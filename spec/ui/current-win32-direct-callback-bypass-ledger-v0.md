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
| Connections | Save/apply/restart, OS2L, MIDI, output settings are Application-owned | Requires persistence/result and adapter-health commands; Control One health is now exposed | Issue #31 G1C / #35 |
| Project lifecycle | New/open/save/validate/compile/activate and history call Application methods | Requires blocking-utility progress/cancellation and document state | Issue #31 G1D |
| Authoring | Profiles, patch, groups, Looks, loops, scripts, mappings, and AutoScript Studio mutate drafts/workflows directly. The structured profile table/template/default actions, generic duplicate-and-rebind confirmation, bounded Static Look hardware-preview lease, and Studio-only OFL search/download/import also enter through this transitional host. | Requires typed Undo-aware Studio commands plus registered progress/result state. Profile parameter descriptors, rows, audits, mutations, and atomic rebind live in toolkit-neutral `fixture_parameter_catalog` / `fixture_profile_editor`; AutoScript uses `StudioAutoloopAutoscriptWorkflow` with immutable proposals and one authoritative document commit; physical preview is isolated behind `StaticLookPhysicalPreviewService`; OFL network work runs on an authoring worker and never enters Runner. Regressions: `fixture_profile_editor_tests`, `autoloop_autoscript_workflow_tests`, `static_look_physical_preview_tests`, `ofl_fixture_catalog_tests`, and `core_tests`. | Owners: issue #31 G1E, AutoScript issue #60, and fixture issue #52. Removal gate: registered Studio authoring/profile/catalog/preview commands and authoritative state replace these Win32 callbacks without changing backend proposal, safety, or provenance contracts. |
| Navigation | Fixed Win32 page selection | Current shell remains the strangler host until skin runtime | Issue #31 G1F / #32 |

Static Look Hold is facade-routed but remains **ineligible for cross-surface or
Control One mapping** until Runner activation owner/generation semantics reject
stale releases. Mouse/keyboard Toggle remains the supported transitional UI
behavior.

No new direct callback should be added unless this ledger records its bounded
reason, owner, and removal gate.
