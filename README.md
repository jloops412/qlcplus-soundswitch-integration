# QLC+ SoundSwitch Integration

[![Status: alpha](https://img.shields.io/badge/status-alpha-orange)](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
[![QLC+ 5.3.0](https://img.shields.io/badge/QLC%2B-5.3.0%20GIT%20a124abe-blue)](https://github.com/mcallegari/qlcplus)
[![Validation](https://github.com/jloops412/EmberLights/actions/workflows/qlcplus-validate.yml/badge.svg)](https://github.com/jloops412/EmberLights/actions/workflows/qlcplus-validate.yml)
[![License: Apache 2.0](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

Use SoundSwitch Micro or Control One hardware directly from QLC+, with a
SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks,
Priority Looks, parameter overrides, group intensity, controller feedback, and
a complete mouse fallback.

> [!IMPORTANT]
> **EmberLights is the historical repository name, not a separate application.**
> The former standalone EmberLights desktop app is retired. At a show, run one
> lighting application: **QLC+**. There is no EmberLights engine, UI, bridge,
> daemon, tracker, or replacement firmware to install.

**Current alpha:** [V26 Autoplay Clarity](releases/qlcplus-control-one/v26/README.md)

V26 is structurally validated and built against one exact QLC+ version. It is
ready for controlled testing, but it is not yet gig-qualified. Read the
[validation boundary](#current-status-and-safety) before using it at an event.

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

The stock QLC+ executable is not replaced or patched by V26.

## What works today

- SoundSwitch Micro DMX output.
- Control One DMX outputs 1 and 2, each physically confirmed independently.
- Control One MIDI translation, core LED feedback, and reconnect recovery.
- Four banks of 32 native QLC+ Autoloops: Medium, Colorful, Slow Dance, and
  Flashy.
- Manual repeat-one, Auto Bank, Auto All, sequential/random order, pad seek,
  and live dwell changes.
- Independent 0.25x, 0.5x, 1x, 2x, and 4x Chase Speed.
- Still or moving full-frame Priority Looks that release back to the continuing
  Autoloop.
- Color-only overrides, Global/group intensity, direct VirtualDJ OS2L timing,
  and essential mouse controls.
- Deterministic workspace builders, package validation, hash-checked install,
  and rollback.

## Quick start for DJs and testers

### Requirements

- Windows x64.
- SoundSwitch Micro and/or Control One.
- A complete QLC+ **5.3.0 GIT a124abe** installation.
- The complete [V26 package folder](releases/qlcplus-control-one/v26/).
- Fixtures configured to known DMX modes and addresses.

Both custom DLLs are matched to this exact QLC+/Qt build. They are not
universal drivers. Do not copy them into another QLC+ version.

### Install

1. Install the normal manufacturer driver/software for the SoundSwitch device.
2. Close SoundSwitch and QLC+ so neither holds the device or DLLs.
3. Download or clone this repository, then open PowerShell in
   `releases/qlcplus-control-one/v26/`.
4. Validate the package before installation:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V26Package.ps1
   ```

5. Install the two build-matched plug-ins into the exact QLC+ folder:

   ```powershell
   .\Install-V26.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

6. Open `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
7. In QLC+ Input/Output, select SoundSwitch Micro, Control One DMX 1, or
   Control One DMX 2 for the intended physical universe.
8. Associate `SoundSwitch-Control-One-Performance.qxi` with the Control One MIDI
   input if QLC+ did not retain it.
9. Follow `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` when VirtualDJ and QLC+ share one
   computer.
10. Run the five-minute test in the [V26 package README](releases/qlcplus-control-one/v26/README.md)
    before connecting a complete show rig.

Do not replace the complete Control One composite USB driver with a generic
driver. Doing so can remove its MIDI interface.

### Roll back

Close QLC+ and run:

```powershell
.\Rollback-V26.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

The installer keeps hash-checked backups of the previous plug-ins. Previous
workspace releases remain available and are not overwritten.

## Included example rig

| Fixtures | Mode | Universe 1 addresses |
|---|---:|---|
| 4 x Both Lighting IR-4 | 10 channel | 1, 11, 21, 31 |
| 4 x Both Lighting BO-TUBE192 | 40 channel | 175, 215, 255, 295 |

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

## Exact V26 compatibility

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` stock core |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Qt build headers | `6.8.1` |
| `soundswitch.dll` SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |
| `os2l.dll` SHA-256 | `EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513` |
| V26 workspace SHA-256 | `ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570` |

Install future QLC+ versions side by side. Rebuild both plug-ins against the
exact new source and toolchain, then qualify the new tuple before changing a
production show machine.

## Current status and safety

V26 is **structurally validated** and its modified plug-ins compile against the
pinned source. Earlier field tests confirmed Micro output, each Control One DMX
port independently, Control One MIDI, core LEDs, OS2L connectivity, Priority
Look behavior, and the essential Autoloop workflow.

The following remain open before the project can be called gig-qualified:

- the complete V26 owner observation on the physical rig;
- repeated Control One hot-plug and LED restoration;
- simultaneous Control One DMX 1 and DMX 2 with MIDI and feedback;
- a combined two-hour VirtualDJ, audio, OS2L, MIDI, LED, and DMX workload; and
- moving rig-specific intensity ranges out of reusable plug-in code.

Disconnect or disable fog, sparks, lasers, pyrotechnics, and other hazardous
loads during development. Structural validation never proves physical safety.

## Contributing

Contributions are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md) and the
[developer guide](docs/DEVELOPMENT.md). Open work is tracked in
[GitHub Issues](https://github.com/jloops412/EmberLights/issues).

Useful contribution areas include:

- reproducible Windows builds for the pinned QLC+ plug-ins;
- fixture-neutral intensity configuration;
- Control One reconnect and dual-output qualification;
- fixture-neutral starter workspaces and migration documentation;
- deterministic QLC+ workspace validation; and
- preparing the focused OS2L correction for upstream review.

Keep proposals inside the selected architecture. Do not revive the retired
standalone app, create a second lighting runtime, or move normal show behavior
out of QLC+.

## Repository map

```text
releases/qlcplus-control-one/v26/   current Windows alpha package
releases/qlcplus-control-one/v20-24 protected rollback releases
qlcplus/plugins/soundswitch/        hardware/workflow plug-in source and tests
qlcplus/patches/                     focused OS2L source correction
qlcplus/input-profiles/              Control One QLC+ input profile
qlcplus/workspace-tools/            deterministic builders and validators
docs/qlcplus-control-one/           workflow, mapping, architecture, and roadmap
docs/DEVELOPMENT.md                  contributor build and test route
```

The retired standalone EmberLights application is preserved in Git history,
not in the active default-branch tree. See
[Archived standalone application](docs/ARCHIVED_STANDALONE_APP.md).

## Documentation

- [Start here](docs/00_START_HERE.md)
- [V26 package, install, test, and rollback](releases/qlcplus-control-one/v26/README.md)
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
for QLC+, Qt, hardware, fixture, and historical-scope details.

This is independent community interoperability work. It is not affiliated with
or endorsed by SoundSwitch, inMusic, VirtualDJ, Both Lighting, or QLC+.
