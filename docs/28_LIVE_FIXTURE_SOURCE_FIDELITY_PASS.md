# Live, fixture truth, and SoundSwitch source-fidelity pass

Date: 2026-08-11  
Scope: Live runtime/UI only, fixture/profile truth, source preservation, and Control One readiness. Studio editors and Autoloop authoring remain owned by the Studio workstream.

## Outcome

This pass repairs one exact known-bad IR-4 profile signature, adds manufacturer-manual 6-channel and 10-channel profiles, makes manifestless SoundSwitch application-data backups losslessly inspectable/bundleable, adds explicit project-to-source binding audits, and moves Live actions behind one typed command/state facade and toolkit-neutral view model.

It does **not** claim that undocumented SoundSwitch payloads have been semantically decoded. The supplied SoundSwitch archive and supplied EmberLights project are demonstrably different source sets.

## IR-4 manufacturer truth

Authoritative evidence: Both Lighting USA, *IR-4 User Manual*, model BOIR4, printed/PDF page 8:

<https://cdn.shopify.com/s/files/1/0716/8645/5572/files/IR-4_User_Manual.pdf?v=1785942928>

| DMX channel | 6-channel mode | 10-channel mode |
|---:|---|---|
| 1 | Red | Master intensity |
| 2 | Green | Red |
| 3 | Blue | Green |
| 4 | White | Blue |
| 5 | Amber | White |
| 6 | Purple/UV | Amber |
| 7 | — | Purple/UV |
| 8 | — | Strobe |
| 9 | — | Program/macro |
| 10 | — | Color selection/speed |

The manual's specifications and manual-control sections call the sixth emitter UV, while its DMX table says Purple. EmberLights uses the native `UV` semantic and retains the ambiguity in the profile/evidence label until the physical emitter is observed.

The supplied `EmberLights-2026-V1.emberlights` embedded this exact stale 10-channel order:

`Intensity, Red, Green, Blue, Amber, White, UV, Strobe, Custom1, Custom2`

White and Amber were reversed. The reviewed candidate preserves that old profile for audit, adds both manual-backed profiles, and rebinds only `ir4-1` and `ir4-2` to the corrected 10-channel profile. It does not alter addresses, universes, Looks, Autoloops, MIDI mappings, or unrelated profiles.

The upgrade is deliberately all-or-nothing. It refuses user-edited near matches, changed fixture reference sets, replacement-ID collisions, or a plan changed after review. It invalidates physical qualification and requires an explicit 6/10 mode check.

## Supplied SoundSwitch source

The supplied ZIP is a SoundSwitch application-data backup, not an exported `.ssproj` project.

| Evidence | Value |
|---|---|
| ZIP SHA-256 | `3149db9af051473cef88419217b677d9dc78a85e20c7ab82c896a02df7b91784` |
| Files | 7,598 |
| Uncompressed bytes | 453,902,493 |
| Inner logical-backup inventory SHA-256 | `4b108cecbfbb01af7105af5fcd86640f14ec697e920be4237694e44c79451139` |
| Venue DB SHA-256 | `1f75d0d4ed6e243171f3f05bdf609283758816c62f1edfb55f3e5556f7acaa6c` |
| Autoloop DB SHA-256 | `29eb9de78a96d21d3ddfb9af08103da8a268bd39682423fc677ef793424c32e7` |
| Active track scripts | 7,527 |
| Active Autoloop scripts | 32 |
| Backup payloads | 33 |
| Fixture personalities | 1 |
| Unknown payloads | 1 (`Diagnostics.bin`) |

The ZIP remains unchanged. Inspection schema v2 classifies the source as `applicationDataBackup`; `.plfix` is a `fixturePersonality`, and `.bak` retains its underlying artifact kind plus `backup: true`. Every payload is hashed in sorted root-relative path order. Selecting a wrapper directory intentionally produces a different inventory hash because its relative paths have an extra prefix; users must select the folder directly containing `SoundSwitchVenues.bin`, `SoundSwitchAutoLoops.bin`, and `SoundSwitchTrackMap.bin`.

### Source-binding defect

The supplied EmberLights project claims these hashes:

- Venue: `22c78d611b5d9a005a615d5d6d90a2063647badc974e41c05a595fce8366c508`
- Autoloops: `27abb5c0d0232e79673ea09b7812bae1ac796a8f895aad0c742da69320a23560`

Neither matches the supplied archive. Therefore:

- the supplied project was created from a different, currently absent SoundSwitch source;
- the supplied ZIP is preserved and completely inventoried, but is not semantically imported into that project;
- the project must retain `sourceMismatch` and `semantic-import-unqualified` evidence;
- source coverage for fixture patch, Static Looks, Autoloop timelines, track scripts, audio links, and MIDI mappings remains unqualified until bounded decoders exist.

The one local `.plfix` identifies `Unknown / 6x12w 6in1 LED`. Its 10-channel layout conflicts with the BOIR4 manual and it must never be assigned to IR-4.

## Live runtime/UI slice

The Live command facade now owns:

- Static Look activate/toggle/hold/clear;
- Autoloop launch/clear/next/previous and bank-filter operations;
- Track Script start/clear;
- capability-checked fixture/group property set/release;
- existing Runner, blackout, Work Light, BPM, tap, hazard, and Release All controls.

Stable IDs are resolved against the immutable active project before bounded Runner commands are posted. The Windows Live/Overrides callbacks now use this facade rather than directly calling Runner content/override methods.

`LiveViewModel` is toolkit-neutral and projects:

- a selected-bank 32-pad Autoloop surface;
- an independent four-bank hardware window;
- active/selected/filter/progress states;
- Static Look and Track Script catalogs;
- stable active-content IDs/names;
- fixture/group capability counts;
- output, controller, transport, safety, and override state.

The transitional Windows renderer has a first dark skin/IA pass, larger Live actions, clear Live/Studio/Control/System navigation labels, and capability-filtered override properties. Fog, haze, laser, and spark are no longer exposed as ordinary override attributes; Strobe is hidden when project safety disallows it.

This is the bridge to a real skin renderer, not the final UI. The production skin engine still needs the planned toolkit spike, layout/package loader, accessibility/DPI qualification, and 32-pad visual performance renderer.

## Control One

Official sources verify the logical behavior—four banks × 32 Autoloops, exclusive/All Banks gestures, Static Look pad mode, touch-strip intensities, overrides, and feedback—but no official public Note/CC/channel/value table was found.

The bundled logical controller template is therefore `captureRequired` and inactive. Every input and feedback packet is empty. An inactive profile cannot map or send anything. `midi_capture` can now write labeled JSON Lines short-MIDI evidence one physical control at a time; it never probes unknown output messages or OLED.

Do not map cross-surface Static Look Hold until Runner activation owner/generation semantics are implemented. Do not fake master, Autoloop, or Track Script intensity with per-fixture overrides; those require engine-owned multipliers.

## Physical qualification checklist

Before using the repaired project for a show:

1. Confirm each physical IR-4 display is set to `10` for the candidate's active profile, or deliberately rebind to the included 6-channel profile if its display is `06`.
2. Confirm universe and start address for `ir4-1` and `ir4-2`.
3. With output isolated, test pure Red, Green, Blue, White, Amber, and UV/Purple one at a time.
4. In 10-channel mode, verify safe neutral behavior for channels 8–10 before enabling effects/macros.
5. Verify Photo White drives physical White and Amber drives physical Amber.
6. Keep the original project and SoundSwitch archive as fallbacks.
7. With SoundSwitch closed, capture the physical Control One input surface one labeled control at a time, then capture official SoundSwitch feedback traffic before enabling any outbound mapping.

## Validation in this pass

- Exact manufacturer channel-order and stale-profile upgrade regression tests.
- Transaction rollback, preservation, idempotency, and save/reopen tests.
- Application-data source classification, deterministic inventory, backup/personality classification, and lossless bundle tests.
- Explicit source-match/mismatch audit tests.
- Full core, SoundSwitch Micro session, Control One DMX protocol, UI facade/state, and Live facade/view-model test suites.

