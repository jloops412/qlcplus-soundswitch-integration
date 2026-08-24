# EmberLights QLC+ SoundSwitch Integration

Use SoundSwitch Micro and Control One hardware directly from QLC+, with a SoundSwitch-familiar Control One workflow, VirtualDJ OS2L beat timing, four Autoloop banks, full-frame Priority Looks, parameter overrides, group intensity, LED feedback, and a complete mouse fallback.

At show time there is one lighting application: **QLC+**. The former standalone EmberLights application is archived and is not part of this runtime.

> **Current release:** [QLC+ SoundSwitch V21](https://github.com/jloops412/EmberLights/releases/tag/v21) — Windows, exact-build-pinned, structurally validated and software-tested. Read the qualification boundary before using it at an event.

## Why this exists

SoundSwitch hardware is useful, but a DJ moving to QLC+ should not have to run a bridge, daemon, second lighting engine, or retired custom application. This project keeps QLC+ responsible for lighting and adds only the hardware/workflow behavior QLC+ cannot express by itself.

```text
VirtualDJ ── OS2L ───────────────▶ QLC+
Control One ── MIDI ─────────────▶ QLC+
QLC+ ── SoundSwitch plugin ──────▶ Micro DMX
                              └──▶ Control One DMX 1 / DMX 2
```

QLC+ owns fixtures, Scenes, Chasers, beat timing, Autoplay, the Virtual Console, project persistence, and normal routing. The custom plugin is limited to proprietary SoundSwitch USB DMX transport, Control One MIDI translation/LED feedback, reconnect handling, full-frame Priority Look selection, and the current rig’s temporary group-intensity scaling.

## V21 at a glance

- Native SoundSwitch Micro and Control One DMX output—no localhost bridge.
- Control One MIDI surface with four 32-pad Autoloop banks and a shared 32-pad Priority Look mode.
- Manual repeat-one, Auto Bank, Auto All, sequential/random order, live 1/2/4/8/16-measure dwell, seek, Play/Pause, and independent 0.25x–4x chase speed.
- Priority Looks can be still Scenes or moving Chasers. A Look becomes sole full-frame DMX authority while the underlying Autoloop continues advancing and returns on release.
- Color-only overrides preserve the loop’s unrelated intensity/movement behavior.
- Global and fixture-group intensity, including the BO-TUBE192 emitters that lack a hardware master channel in 40-channel mode.
- Control One reconnect recovery and restoration of known LED state.
- Mouse-operable Live page for the essential show workflow.
- Direct local VirtualDJ OS2L with a lightweight keepalive for late application starts/restarts.
- Hash-pinned plugin install, automatic backup, verified rollback, and a self-testing release package.

V21 deliberately preserves all 1,906 V20 Functions exactly. The release pass did not rewrite the 128 Autoloops, Priority Looks, manual functions, fixtures, or public bindings.

## Supported hardware

| Device | Current support | Status |
|---|---|---|
| SoundSwitch Micro | One DMX universe | Physical output confirmed |
| SoundSwitch Control One DMX 1 | First DMX port | Physical output confirmed |
| SoundSwitch Control One DMX 2 | Second DMX port | Physical output confirmed independently |
| SoundSwitch Control One MIDI | Performance workflow and controls | Core workflow physically confirmed |
| SoundSwitch Control One LEDs | State feedback plus reconnect restoration | Software-tested; V21 hot-plug observation remains |

Multiple devices can be enumerated. Simultaneous dual-port operation under full MIDI/LED/VirtualDJ load is still a field-qualification gate, not a finished community-support claim.

## Current example show

The included `.qxw` workspace is a complete working show for:

- four Both Lighting IR-4 fixtures in 10-channel mode at addresses 1, 11, 21, and 31;
- four Both Lighting BO-TUBE192 fixtures in 40-channel mode at addresses 175, 215, 255, and 295;
- private duplicate fixtures on QLC+ Universe 3 for full-frame Priority Looks.

The 32 pads appear as four columns by eight rows, matching the physical Control One orientation. The banks are currently organized as Medium, Colorful, Slow Dance, and Flashy.

## Download and install

Start with the [V21 GitHub release](https://github.com/jloops412/EmberLights/releases/tag/v21). Download the Windows archive and its SHA-256 file, extract the whole archive, then read the package README.

The short path is:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\Test-V21Package.ps1
.\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

Then open `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw`, select the desired SoundSwitch output in QLC+ Input/Output, and associate the included `.qxi` profile with Control One MIDI.

Important compatibility rule: `soundswitch.dll` is a QLC+ plugin, not a portable standalone DLL. V21 is pinned to QLC+ source commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`, UI version `5.3.0 GIT a124abe`, and a specific `qlcplus5.exe` hash. The installer rejects another core by default. Future QLC+ upgrades should be installed side-by-side and receive a matching plugin rebuild before production changes.

## Control One workflow

| Control | QLC+ behavior |
|---|---|
| 32 performance pads | Latch/replace Autoloops or exclusive Priority Looks |
| Auto Loop | Toggle the pad surface between Autoloops and Priority Looks |
| Banks 1–4 | Select Medium, Colorful, Slow Dance, or Flashy |
| Shift + Bank | Start automatic playback for that bank |
| Bank/All scope | Choose one-bank or all-bank Autoplay |
| Autoloop Override | Toggle sequential/random order in this QLC+ adaptation |
| Play/Pause | Start or pause the selected manual/automatic owner, even without a playing song |
| Pan/Speed encoder | Beat-aligned chase-speed multiplier |
| Intensity strip | Global, group, or Scripted intensity target |
| Color pads | Color-only overrides; unrelated loop behavior continues |
| Stop All | Emergency global stop |

Select, Pan, Tilt, position, Smoke, Strobe, Hue, Back/Link, OLED, and firmware roles are documented where known; roles without a useful current-rig behavior remain intentionally deferred.

## Reliability and rollback

V21 validates Windows MIDI handles using device IDs, responds to device-close callbacks, retries one failed LED write after reconnecting, and restores known controller state without intentionally resetting QLC+ playback ownership.

The installer:

1. verifies the QLC+ executable and packaged plugin hashes;
2. refuses to replace a plugin while QLC+ is running;
3. backs up the previous DLL below the selected QLC+ directory;
4. writes a receipt containing the old/new hashes;
5. verifies the installed file.

`Rollback-SoundSwitchPlugin.ps1` restores the latest verified backup. Keeping V20 and the existing QLC+ installation untouched until the first V21 physical pass remains the safest production transition.

## What “released” means here

Evidence is labeled precisely:

- **Structurally validated:** package hashes, XML, fixture patch, IDs, and references pass automated checks.
- **Software-tested:** protocol tests and plugin ABI/virtual-output smoke tests pass against the pinned build.
- **Physical-output-tested:** the named device/port visibly controlled fixtures.
- **Gig-qualified:** the complete DJ/audio/OS2L/MIDI/LED/DMX workflow survived the defined fault and soak test.

V21 satisfies the first two categories. Micro, both Control One ports independently, MIDI, OS2L, and core show behavior have physical evidence from the preceding baseline. V21 is not yet called gig-qualified; repeated hot-plug/LED recovery, simultaneous Control One ports, and the combined two-hour workload remain explicit gates.

## Documentation map

- [V21 package and first-plug guide](releases/qlcplus-control-one/v21/README.md)
- [V21 release notes](releases/qlcplus-control-one/v21/RELEASE_NOTES.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
- [Control One workflow specification](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [MIDI and logical-channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Plugin source and tests](qlcplus/plugins/soundswitch/)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## Repository layout

```text
releases/qlcplus-control-one/v21/   distributable workspace, profile, plugin and tools
qlcplus/plugins/soundswitch/        native plugin source and deterministic tests
docs/qlcplus-control-one/           workflow, architecture, mapping and maintenance
research/ and older docs/           historical provenance; not the live runtime plan
native-core/ and installer/         archived standalone EmberLights work
```

## Next work

The engineering order is intentionally narrow:

1. finish V21 hot-plug, simultaneous-port, and combined-workload qualification;
2. move current-rig intensity addresses from plugin code into configuration;
3. build purposeful event Priority Looks;
4. meticulously grade and improve each Autoloop/Scene;
5. finish only the Control One roles that provide real live value;
6. create a more general starter workspace/configuration for other DJs.

No bridge, standalone EmberLights revival, replacement firmware, custom lighting engine, or second show-time application is planned.

## Independence and licensing

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, or the QLC+ project. Product names are used only to describe compatibility.

The V21 plugin and included QLC+ interface code are distributed under Apache License 2.0; the release contains the license text. No QLC+ core, SoundSwitch software/assets/database, or firmware is distributed. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for boundaries and attribution.
