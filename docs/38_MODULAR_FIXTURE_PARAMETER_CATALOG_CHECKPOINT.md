# Modular fixture-parameter catalog checkpoint

Status: source-complete checkpoint for Windows preview 90. This removes the permanent White/Amber repair surface and establishes the shared semantic vocabulary required by fixture profiles, Static Looks, Autoloops, Live overrides, MIDI/controllers, and future skins. It does not claim physical fixture qualification, complete multi-capability channel persistence, or completion of the production skins renderer.

Successor: `39_MULTI_CAPABILITY_FIXTURE_CHANNEL_CHECKPOINT.md` implements the multi-capability persistence/compiler/editor package named as this checkpoint's next boundary. Preview 90 remains the historical baseline.

## Operator outcome

Studio → Fixture Profiles now supports a generic correction workflow:

- every current semantic parameter appears under Intensity, Color, Position, Beam, Image, Effect, Atmosphere, or Custom;
- display labels are separated from stable IDs, so a skin can change presentation without breaking projects, mappings, or actions;
- selecting a channel row loads structured fields—no encoded parameter text is required;
- **Apply Safe Defaults** maps direct emitters and other conservative linear controls in one action;
- chart-defined wheel, shutter, strobe, macro, rotation, hazard, and Custom controls fail closed instead of receiving a guessed 0–255 mapping;
- the profile audit reports described/open DMX slots, semantic rows, safe constants, chart-defined functions, safety restrictions, repeated semantics, and Custom lanes;
- **PATCH USAGE** lists the exact fixtures, universes, and addresses using the selected saved profile;
- saving a duplicated Local profile can atomically rebind every affected fixture after full project validation and compilation.

White and Amber are therefore normal Color parameters. To correct observed behavior, duplicate an immutable profile if needed, assign White and Amber to the physical channels shown by isolated testing, save, accept the generic rebind prompt, and preview both emitters. The legacy transactional White/Amber backend remains tested for migration/compatibility code but has no permanent operator button.

## Shared contract

`emberlights/fixture_parameter_catalog.hpp` is toolkit-neutral and defines, for every `showcore::Property`:

- stable semantic ID;
- display name and description;
- parameter category;
- normalized control kind;
- conservative direct-default versus manual-DMX-chart policy;
- safety class;
- fine-channel support;
- availability to Profiles, Static Looks, Autoloops, Live overrides, and controllers.

The catalog is complete and ordinal-checked in tests. Windows currently consumes the same descriptors for Profile, Static Look, Live Override, and MIDI property selectors. A future Default, Reference, Safe, or user skin can render knobs, faders, pads, selectors, and status without inventing another property namespace. Control One and any other MIDI/HID surface map stable semantic parameters rather than raw fixture-channel numbers; profiles remain the device-specific realization boundary.

`emberlights/fixture_profile_editor.hpp` remains the toolkit-neutral profile-authoring service and now also owns:

- structured profile audits with stable issue codes;
- safe descriptor-driven channel defaults;
- an atomic `rebind_fixture_profile_instances` transaction that validates and compiles before replacing the project.

## Why the model follows proven fixture systems

The implementation is original and does not copy another application's UI or source. Its behavior is informed by official primary documentation:

- QLC+ Channel Editor documents selection-driven channel presets and multiple DMX capability ranges: `https://github.com/mcallegari/qlcplus-docs/blob/master/pages/11.fixture-definition-editor/03.channels/default.v4.md`.
- Open Fixture Library documents typed capabilities whose meaning changes across DMX ranges: `https://github.com/OpenLightingProject/open-fixture-library/blob/master/docs/fixture-format.md`.
- SoundSwitch distinguishes basic intensity/color/position/strobe tracks from reusable Attribute Cues such as gobo, prism, focus, and zoom: `https://support.soundswitch.com/en/support/solutions/articles/69000828942-soundswitch-controlling-gobo-prism-more-with-attribute-cues`.
- WOLFmix groups live behavior into Color, Move, Beam, and direct channel editing while applying the fixture profile underneath: `https://wolfmix.com/wolfmix-w1`.

This checkpoint adopts the shared architectural lesson—typed semantic controls over fixture-specific DMX realization—without adopting protected trade dress, Qt widgets, vendor assets, or proprietary fixture content.

## White/Amber truth boundary

Portable compiler/renderer tests prove that semantic White and Amber reach the offsets stored in the active compiled profile. Other direct colors use the same path. There is no application-wide White/Amber inversion.

The immutable manual-backed BO-IR4 definitions therefore remain:

| Mode | White | Amber |
| --- | --- | --- |
| 6CH | CH4 | CH5 |
| 10CH | CH5 | CH6 |

If an owned unit behaves in the opposite order, its physical display mode, firmware/manual variant, and raw channel observations must be recorded. The correct project representation is a separately named Local physical variant bound to those fixtures, not mutation of the manufacturer-backed source snapshot.

## Capability-range boundary

The current persisted `ChannelDefinition` still represents one semantic mapping and one active range per coarse/fine channel. It can safely express direct 8/16-bit levels, constants, discrete values, and a single ranged function. It cannot yet faithfully persist every named capability on a compound channel—for example shutter closed/open/strobe segments, color-wheel slots plus rotation, direction/speed bands, switching aliases, or cell/head geometry.

The catalog and audit deliberately expose that boundary. The next profile-model package must add multiple named DMX capabilities per physical channel, exact neutral/open/home/blackout/highlight values, cell/head ownership, import conformance, and migration without placing the global catalog or network in Runner.

## Surface-contract reconciliation

- Public skin command/state registry: unchanged; this is shared value/capability metadata, not a new callable command.
- Project schema and stable property IDs: unchanged.
- Runner/compiler property behavior: unchanged.
- New modular component: complete `fixture_parameter_catalog` descriptors.
- New toolkit-neutral transaction: generic profile instance rebind.
- Transitional Win32 callbacks: safe-default draft mutation and duplicate/rebind confirmation remain recorded in the direct-callback bypass ledger until typed Studio authoring commands replace the host.
- Compatibility: existing projects, QXF/OFL snapshots, MIDI maps, Looks, and Autoloops remain readable.

## Verification

- `fixture_profile_editor_tests` covers catalog completeness/unique stable IDs, every supported surface, safe direct defaults, refusal of chart-defined strobe defaults, profile audits, generic atomic rebind, and the retained backend White/Amber transaction.
- Native warnings-as-errors focused build and tests pass.
- Windows x64 LLVM/MinGW warnings-as-errors `EmberLights.exe` and focused profile-test compilation pass.
- Full portable CTest/package/installer evidence is recorded in the preview 90 handoff after packaging.

Installed Windows layout/accessibility, project upgrade/uninstall, physical BO-IR4 behavior, Control One mapping capture, and hardware output remain tester gates.
