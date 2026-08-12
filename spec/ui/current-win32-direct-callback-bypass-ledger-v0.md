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
| Authoring | Profiles, patch, groups, Looks, loops, scripts, mappings mutate drafts directly | Requires typed Undo-aware Studio commands | Issue #31 G1E |
| Navigation | Fixed Win32 page selection | Current shell remains the strangler host until skin runtime | Issue #31 G1F / #32 |

Static Look Hold is facade-routed but remains **ineligible for cross-surface or
Control One mapping** until Runner activation owner/generation semantics reject
stale releases. Mouse/keyboard Toggle remains the supported transitional UI
behavior.

No new direct callback should be added unless this ledger records its bounded
reason, owner, and removal gate.
