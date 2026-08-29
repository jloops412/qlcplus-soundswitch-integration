# QLC+ SoundSwitch Integration

[![Status: alpha](https://img.shields.io/badge/status-alpha-orange)](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
[![QLC+ 5.3.0](https://img.shields.io/badge/QLC%2B-5.3.0%20GIT%20a124abe-blue)](https://github.com/mcallegari/qlcplus)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

Use SoundSwitch Micro or Control One hardware directly from QLC+, with a
SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks,
Priority Looks, parameter overrides, group intensity, controller feedback, and
a complete mouse fallback.

**Current alpha candidate:** [V27 Full Rig](releases/qlcplus-control-one/v27/README.md)

V27 extends the protected V26 Autoplay Clarity show across the complete rig. It
has structural and CI evidence, but it has not yet passed the physical bench or
gig qualification. Preserve the complete
[V26 package](releases/qlcplus-control-one/v26/README.md) as the rollback.

## Architecture

```text
VirtualDJ -- direct OS2L ----------------> QLC+
Control One -- MIDI --------------------> QLC+
QLC+ Scenes / Chasers / Virtual Console  |
QLC+ -- SoundSwitch output plug-in ------+--> Micro DMX
                                         +--> Control One DMX 1
                                         +--> Control One DMX 2
```

QLC+ owns fixture profiles and patches, Scenes, Chasers and Autoloops, beat
timing, Autoplay, the Virtual Console, project persistence, and output routing.

Custom code is deliberately narrow:

- `soundswitch.dll` provides proprietary SoundSwitch USB output, Control One
  MIDI translation, LED feedback, reconnect behavior, and full-frame Priority
  Look ownership.
- `os2l.dll` is a focused QLC+ plug-in correction that uses OS2L's reported BPM
  instead of bursty packet-arrival timing.

The stock QLC+ executable is not replaced or patched by V27.

## What works today

- Previously confirmed with V26: SoundSwitch Micro output, each Control One
  DMX port independently, Control One MIDI translation, core LED feedback, and
  reconnect recovery. These observations do not physically qualify V27.
- Four banks of 32 native QLC+ Autoloops: Medium, Colorful, Slow Dance, and
  Flashy, now programmed across all eleven fixtures.
- Manual repeat-one, Auto Bank, Auto All, sequential/random order, pad seek,
  and live dwell changes.
- Independent 0.25x, 0.5x, 1x, 2x, and 4x Chase Speed.
- Still or moving full-frame Priority Looks that release back to the continuing
  Autoloop, plus additive Focus A/B position Scenes and a sweep.
- Full-rig performance Scenes and color-only overrides, Global/group intensity,
  direct VirtualDJ OS2L timing, and essential mouse controls.
- Deterministic workspace builders, package validation, manual installation,
  and rollback guidance.

## Quick start for DJs and testers

### Requirements

- Windows x64.
- SoundSwitch Micro and/or Control One.
- A complete QLC+ **5.3.0 GIT a124abe** installation.
- The complete [V27 package folder](releases/qlcplus-control-one/v27/).
- Fixtures configured to known DMX modes and addresses.

Both custom DLLs are matched to this exact QLC+/Qt build. They are not
universal drivers. Do not copy them into another QLC+ version.

### Install

1. Install the normal manufacturer driver/software for the SoundSwitch device.
2. Install the exact QLC+ build, normally in `C:\QLC+`. The correct folder is
   the one containing `qlcplus5.exe` and a `Plugins` folder.
3. Download the complete
   [`releases/qlcplus-control-one/v27`](releases/qlcplus-control-one/v27/)
   folder. Keep every file together.
4. Close SoundSwitch and every QLC+ window.
5. In File Explorer, open `C:\QLC+\Plugins` (or the `Plugins` folder inside
   your actual QLC+ installation). Back up any existing `soundswitch.dll` and
   `os2l.dll`, then copy the V27 versions into that folder.
6. Back up any same-name files, then install
   `American-DJ-Focus-Spot-Two.qxf` in `%USERPROFILE%\QLC+\Fixtures\` and
   `SoundSwitch-Control-One-Performance.qxi` in
   `%USERPROFILE%\QLC+\InputProfiles\`. Restart QLC+ after installing the
   fixture definition.
7. With physical outputs disabled, open
   `IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw` in QLC+.
8. Resolve any missing fixture definition or mode error before continuing. In
   QLC+ Input/Output, select SoundSwitch Micro, Control One DMX 1, or
   Control One DMX 2 for the intended physical universe.
9. Associate `SoundSwitch-Control-One-Performance.qxi` with the Control One MIDI
   input if QLC+ did not retain it.
10. Follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` when VirtualDJ and QLC+ share one
   computer.
11. Follow the [V27 controlled bench](releases/qlcplus-control-one/v27/FULL_RIG_PATCH_AND_BENCH.md)
    one fixture class and output path at a time before connecting the complete
    show rig.

No PowerShell command is required for normal installation. The included scripts
are optional maintainer tools for checksum validation, automated backups, and
repeatable lab deployment.

Do not replace the complete Control One composite USB driver with a generic
driver. Doing so can remove its MIDI interface.

### Roll back

Close QLC+, disable outputs, restore the backed-up plug-ins and user fixture/
profile files, and open the preserved
[V26 workspace package](releases/qlcplus-control-one/v26/README.md). If you did
not back up the plug-ins, reinstall the exact pinned QLC+ build. Never overwrite
or rewrite V26 during an upgrade or rollback.

## Included example rig

| Fixtures | Mode | Display starts | Occupied span |
|---|---:|---|---|
| 4 x Both Lighting IR-4 | 10 channel | 001, 011, 021, 031 | 001–040 |
| 1 x Chauvet Wash FX Hex | 40 channel | 041 | 041–080 |
| 2 x ADJ Focus Spot Two (A/B) | 18 channel | 081, 099 | 081–098, 099–116 |
| Reserved | — | — | 117–174 free |
| 4 x Both Lighting BO-TUBE192 | 40 channel | 175, 215, 255, 295 | 175–334 |

V27 does not move any V26 fixture. It fills the open 041–116 range and leaves
117–174 free.

Private duplicate fixtures on QLC+ Universe 3 provide full-frame Priority
Looks. **Universe 3 is internal and must not be routed directly to physical
DMX.**

Other DJs can reuse the hardware plug-ins and Control One profile. The included
workspace is not a universal fixture show. A different rig needs its own QLC+
patch, fixture IDs, creative Functions, Priority layer, and intensity mapping.
Start with the [community migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

## SoundSwitch-familiar live workflow

| Control | QLC+ behavior |
|---|---|
| 32 pads | Latch/replace an Autoloop or select one exclusive Priority Look |
| Auto Loop | Switch the pad surface between Autoloops and Priority Looks |
| Banks 1-4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank | Sequential/random playback in the selected bank |
| Start All | Sequential/random playback through all four banks |
| Dwell | 1, 2, 4, 8, or 16 measures per loop, adjustable while running |
| Chase Speed | 0.25x to 4x loop multiplier, independent from dwell |
| Priority Look | Sole full-frame authority while the underlying loop advances |
| Color pads | Color-only override; unrelated intensity/movement continues |
| Play/Pause | Control the selected playback owner |
| Stop | Emergency global stop |

The onscreen 4 x 8 layout matches Control One. A four-segment strip beneath
each pad observes the real QLC+ Chaser state across all banks. QLC+'s native
amber Monitoring state identifies the currently running raw Chaser.

## Exact compatibility

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` stock core |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Qt build headers | `6.8.1` |
| V27 `soundswitch.dll` SHA-256 | `19074A37AA915E1E39124CD14441025A2A83AB06EBDB14BD0953E2910B801DE3` |
| `os2l.dll` SHA-256 | `EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513` |
| V27 workspace SHA-256 | `A4F7559930E93485F3EA2815A0B44CA8E40ACDFCC43FB3B0430E863C64B2DC4B` |
| V26 workspace SHA-256 | `ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570` |

The completed V27 package records every candidate artifact in its
`SHA256SUMS.txt`. V26 remains the immutable compatibility and rollback
baseline.

Install future QLC+ versions side by side. Rebuild both plug-ins against the
exact new source and toolchain, then qualify the new tuple before changing a
production show machine.

## Current status and safety

V27 is an **alpha candidate** with deterministic workspace checks and CI
protocol, intensity, and plug-in-load smoke evidence. That evidence does not
prove loading in the exact pinned Windows host, physical DMX output, fixture
behavior, beam safety, visual quality, or gig readiness. Earlier V26 hardware
observations cannot be inherited as proof for the changed V27 workspace and
intensity routing.

The Focus Spot Two UV source is Risk Group 3. V27 keeps both real Focus UV
shutters closed and both UV dimmers at zero in every released creative frame,
including the `UV` performance Scene.

The following remain open before the project can be called gig-qualified:

- exact pinned-host plug-in loading and the complete V27 owner observation;
- one-class-at-a-time Wash and Focus address, mode, aim, and movement checks;
- repeated Control One hot-plug and LED restoration;
- simultaneous Control One DMX 1 and DMX 2 with MIDI and feedback;
- a combined two-hour VirtualDJ, audio, OS2L, MIDI, LED, and DMX workload; and
- moving rig-specific intensity ranges out of reusable plug-in code.

Disconnect or disable fog, sparks, lasers, pyrotechnics, and other hazardous
loads during development. Structural validation never proves physical safety.

## Contributing

Contributions are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md) and the
[developer guide](docs/DEVELOPMENT.md). Open work is tracked in
[GitHub Issues](../../issues).

Useful contribution areas include:

- reproducible Windows builds for the pinned QLC+ plug-ins;
- fixture-neutral intensity configuration;
- Control One reconnect and dual-output qualification;
- fixture-neutral starter workspaces and migration documentation;
- deterministic QLC+ workspace validation; and
- preparing the focused OS2L correction for upstream review.

Keep proposals inside the selected architecture: QLC+ owns the show, while the
custom plug-ins remain limited to hardware integration and focused gaps.

## Repository map

```text
releases/qlcplus-control-one/v27/   current full-rig alpha candidate
releases/qlcplus-control-one/v26/   protected Autoplay Clarity rollback
releases/qlcplus-control-one/v20-24 earlier protected rollback releases
qlcplus/plugins/soundswitch/        hardware/workflow plug-in source and tests
qlcplus/patches/                     focused OS2L source correction
qlcplus/input-profiles/              Control One QLC+ input profile
qlcplus/workspace-tools/            deterministic builders and validators
docs/qlcplus-control-one/           workflow, mapping, architecture, and roadmap
docs/DEVELOPMENT.md                  contributor build and test route
```

## Documentation

- [Start here](docs/00_START_HERE.md)
- [V27 package, install, test, and rollback](releases/qlcplus-control-one/v27/README.md)
- [V27 release notes](releases/qlcplus-control-one/v27/RELEASE_NOTES.md)
- [V27 full-rig patch and controlled bench](releases/qlcplus-control-one/v27/FULL_RIG_PATCH_AND_BENCH.md)
- [V27 SoundSwitch source provenance](releases/qlcplus-control-one/v27/SOUNDSWITCH_SOURCE_PROVENANCE.md)
- [Protected V26 rollback](releases/qlcplus-control-one/v26/README.md)
- [Community migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [Logical channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
- [V26 provenance](docs/qlcplus-control-one/V26_AUTOPLAY_CLARITY_PROVENANCE.md)

## License and independence

Current project source and documentation are licensed under the
[Apache License 2.0](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md)
for QLC+, Qt, hardware, fixture, and distribution-scope details.

This is independent community interoperability work. It is not affiliated with
or endorsed by SoundSwitch, inMusic, VirtualDJ, Both Lighting, Chauvet, ADJ, or
QLC+.
