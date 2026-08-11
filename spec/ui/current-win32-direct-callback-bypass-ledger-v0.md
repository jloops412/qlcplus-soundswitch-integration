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

The Win32 buttons and F5/F8 accelerators invoke the same facade used by direct
tests. Invocation returns an explicit accepted/no-change/unavailable/invalid/
queue-full/safety/unsupported/internal result.

## Temporary direct callbacks

| Area | Current bypass | Reason retained | Follow-up |
| --- | --- | --- | --- |
| Active content | Static Look, Autoloop, Track Script controls call Runner methods | Requires stable object-ID argument facade and authoritative active/progress state | Issue #31 G1B |
| Overrides | Fixture/group property set/release calls Runner directly | Requires typed target/property/value parameter variant | Issue #31 G1B |
| Connections | Save/apply/restart, OS2L, MIDI, output settings are Application-owned | Requires persistence/result and adapter-health commands; Control One health is now exposed | Issue #31 G1C / #35 |
| Project lifecycle | New/open/save/validate/compile/activate and history call Application methods | Requires blocking-utility progress/cancellation and document state | Issue #31 G1D |
| Authoring | Profiles, patch, groups, Looks, loops, scripts, mappings mutate drafts directly | Requires typed Undo-aware Studio commands | Issue #31 G1E |
| Navigation | Fixed Win32 page selection | Current shell remains the strangler host until skin runtime | Issue #31 G1F / #32 |

No new direct callback should be added unless this ledger records its bounded
reason, owner, and removal gate.
