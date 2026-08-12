# Fixture Truth and Static Look Builder Checkpoint

Checkpoint date: 2026-08-12

Primary issue: #52 — Fixture truth, capability-aware Static Looks, and IR-4 color qualification

Related program: #46 — Studio V1 authoring and migration

This checkpoint continues the fixture plans in documents 23, 24, and 28 and the Studio boundary in documents 28–30. `Look` remains the persisted/runtime entity; “scene” is user-facing workflow language, not a second domain model.

## 1. Outcome of this slice

The first production-shaped Static Look slice is implemented:

- one canonical manual-backed Both Lighting BO-IR4 lineage with 6CH and 10CH profile IDs;
- shared fixture and group capability inspection reused by Live and Studio;
- capability-aware target/property editing instead of editable assignment CSV;
- explicit `RELEASE`, `SET`, and `FORCE_ZERO` ownership;
- Windows RGB picker plus independent Red, Green, Blue, White, Amber, UV, and Master controls;
- pure-emitter and Black swatches;
- full-color ownership that writes zeroes for every supported unselected emitter, opens a real master when present, and forces modeled strobe off;
- exact offline DMX rendering through the production compiler, engine, layers, and fixture renderer;
- per-channel fixture/profile/mode/revision/property/ownership/layer/byte trace and deterministic frame SHA-256;
- project validation for unsupported Look properties, closed master intensity, Look limits/name/fade, all-Release no-ops, and duplicate names;
- dependency-aware Look deletion covering Autoloops, TrackScripts, and MIDI bindings;
- toolkit-neutral Static Look drafts that commit through `StudioDocumentService` as one generation and one Undo transaction;
- Static Look clear/release using the authored fade instead of a hard-coded 100 ms;
- full SHA-256 source and behavior evidence for the QXF adapter and known profile upgrades.

The transitional Win32 application still holds the legacy project mirror and history for its broad authoring surface. The new Static Look service is ready for the planned strangler migration, but this slice does not pretend that all Win32 editors already use `StudioDocumentService` as their sole authority.

## 2. IR-4 source truth

Authoritative evidence is the Both Lighting USA BO-IR4 manual, PDF page 5:

<https://cdn.shopify.com/s/files/1/0716/8645/5572/files/BL_IR-4_BO-IR4.pdf?v=1679519527>

Source SHA-256:

`1267e289b2c0577ec749f0de5265105db5e86b6ae3b2e12414cc00777fd3c03a`

| Channel | 6CH | 10CH | EmberLights treatment |
| ---: | --- | --- | --- |
| 1 | Red | Master intensity | Direct emitter / master |
| 2 | Green | Red | Direct emitter |
| 3 | Blue | Green | Direct emitter |
| 4 | White | Blue | Direct emitter |
| 5 | Amber | White | Direct emitter |
| 6 | Purple/UV | Amber | Direct emitter; ambiguity retained |
| 7 | — | Purple/UV | Direct emitter; ambiguity retained |
| 8 | — | Strobe | Ranged strobe with safe default byte 0 |
| 9 | — | Compound program/macro selector | Quarantined at constant byte 0 |
| 10 | — | Color selection/speed | Quarantined at constant byte 0 |

The manual does not provide enough semantics to expose channels 9–10 as honest linear controls. They are therefore not presented as `Custom1`/`Custom2` sliders. Rich named ranges and hardware evidence must exist before those channels become authorable.

The 10CH profile has a physical master. A color with Master left at zero is physically dark even if the emitter channel is nonzero. Full Color opens Master intentionally; project validation and offline preview diagnose a closed master.

The 6CH profile has no physical master. This slice does not invent one. Virtual intensity that proportionally scales direct emitters remains a separate renderer/safety change.

## 3. Color contract

The RGB picker represents conventional RGB intent only. It never guesses White, Amber, or UV extraction. Those emitters stay directly authorable until a revisioned, measured per-profile calibration exists.

Applying Full Color to a target:

1. resolves the fixture or group through shared capability inspection;
2. writes `SET` for every supported RGBWAUV emitter, including zero values;
3. writes `SET` for physical Intensity where supported;
4. writes `FORCE_ZERO` for modeled Strobe where supported;
5. skips unsupported group members explicitly and reports support counts;
6. sorts expanded fixture assignments deterministically.

This prevents a red Look from leaking Green, Blue, White, Amber, UV, or Strobe from a lower layer. Removing a property is distinct from authoring `RELEASE`; `FORCE_ZERO` remains distinct from `SET(0)` throughout persistence and rendering.

## 4. Preview and safety boundary

The builder’s preview is offline only. It compiles an immutable candidate and renders the Look through the same semantic path used by Runner, but it opens no Art-Net, sACN, USB-DMX, SoundSwitch, MIDI, or OS2L adapter.

The trace answers:

`fixture -> profile/mode/revision -> universe/address/channel -> property -> ownership -> winning layer -> rendered byte`

A bounded physical preview/test bench remains open work. It must run only while Live is stopped, start and end black, isolate selected fixtures, reject unknown/hazard functions, enforce a timeout, and black out on every stop/failure/destruction path. Raw Hardware Test remains the physical qualification authority.

## 5. Verification completed

- Full native Makefile suite passes with warnings as errors.
- Windows Release cross-build completes, including `EmberLights.exe`.
- IR-4 6CH pure Red, Green, Blue, White, Amber, and UV each produce one exact nonzero slot.
- IR-4 10CH pure-emitter frames open channel 1 Master, drive channels 2–7 correctly, and hold channels 8–10 at zero.
- A deliberately closed 10CH Master is detected in validation and preview.
- Unsupported fixture/property assignments fail validation instead of disappearing in rendering.
- Mixed-capability groups expose support counts and modify only supporting fixtures.
- RGB hex round-trips without changing White, Amber, or UV.
- Draft commit, stale generation rejection, Undo, Redo, ownership round-trip, Look limit, and MIDI dependency tests pass.

These are software qualifications. Physical emitter identity, 6CH/10CH fixture display mode, address, transport, channel 8–10 behavior, and perceived/calibrated color remain unqualified until observed on both owned IR-4 fixtures.

## 6. Ordered continuation

| Order | Work package | Priority | Completion evidence |
| ---: | --- | --- | --- |
| 1 | Run the two-fixture IR-4 raw emitter bench in 6CH and 10CH | P0 gate | saved raw/semantic frames, observed emitter, mode/address, blackout evidence |
| 2 | Add bounded physical preview and Stop/timeout blackout | P0 | fault-injection tests plus installed Windows observation |
| 3 | Make `StudioDocumentService` authoritative across Win32 authoring | P1 | all editor commits/Undo/Redo/New/Open/Save generation tests |
| 4 | Add structured channel functions/ranges and neutral/open/blackout values | P1 | wheel/strobe/macro boundary goldens; no generic linear macro controls |
| 5 | Add virtual master intensity for dimmerless direct-emitter fixtures | P1 | 6CH IR-4 intensity/safety cap and raw-diagnostic bypass tests |
| 6 | Add structured profile evidence/revisions and qualification invalidation | P1 | edit/repatch/mode/address/source hash invalidation tests |
| 7 | Build a Studio-only fixture catalog/index and pinned OFL exporter adapter | P1 | license manifest, provenance, quarantine, golden corpus, immutable snapshots |
| 8 | Add measured per-profile color calibration | P2 | revisioned bench measurements, behavior hash, calibrated/uncalibrated UI state |
| 9 | Add multi-cell, switching-channel, head, wheel, and matrix support | P2 | representative conformance corpus and capacity tests |

Do not load a global catalog into `CompiledFixtureLibrary`; it is a fixed-capacity per-show runtime store. Studio searches a separate catalog and embeds only selected immutable profile snapshots.

Do not bundle OFL, QLC+, or proprietary fixture data until the repository’s third-party license inventory and distribution gate are complete.
