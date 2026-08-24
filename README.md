# QLC+ for SoundSwitch Micro and Control One

Use SoundSwitch Micro or Control One hardware directly from QLC+, with a SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks, Priority Looks, overrides, intensity groups, LED feedback, and a complete mouse fallback.

At a show, run **one lighting application: QLC+**. No bridge, daemon, replacement firmware, or standalone EmberLights app is required.

> **Current alpha candidate:** [V23 Live Console](https://github.com/jloops412/EmberLights/releases/tag/v23). It is structurally validated and uses the same tested hardware plug-in as V21/V22. Run the five-minute physical check before an event.

## What runs

```text
VirtualDJ ── OS2L ─────────────▶ QLC+
Control One ── MIDI ───────────▶ QLC+
QLC+ ── SoundSwitch plug-in ───▶ Micro DMX
                             └─▶ Control One DMX 1 / DMX 2
```

QLC+ owns fixtures, Scenes, Chasers, Autoplay, beat timing, the Virtual Console, and show files. The focused plug-in handles only SoundSwitch USB transport, Control One MIDI/LED/reconnect, full-frame Priority Look selection, and current rig intensity scaling.

## Already using V21 or V22?

1. Download and extract V23.
2. Run `Test-V23Package.ps1`.
3. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.

Do not reinstall the plug-in. V23 uses the same DLL.

## New installation

1. Install the complete pinned QLC+ **5.3.0 GIT a124abe** build.
2. Install the normal manufacturer driver/software for the SoundSwitch device, then close SoundSwitch.
3. Download and extract the whole [V23 release](https://github.com/jloops412/EmberLights/releases/tag/v23).
4. In PowerShell inside the extracted folder:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V23Package.ps1
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

5. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.
6. Select SoundSwitch Micro, Control One DMX 1, or Control One DMX 2 in QLC+ Input/Output.
7. For Control One MIDI, associate `SoundSwitch-Control-One-Performance.qxi` if QLC+ did not retain it.
8. Follow the included VirtualDJ OS2L note when both programs share a laptop.

Do not replace the entire Control One composite USB driver with a generic driver; its MIDI interface must remain available.

The complete walkthrough is in the [community migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

## What V23 improves

- Fixes the mouse `AUTOLOOPS ⇄ PRIORITY LOOKS` switch by using one persistent control instead of two competing copies.
- Fixes automatic-play feedback with a four-bank live rail beside every pad; amber follows the raw Chaser during manual playback, Auto Bank, Auto All, and seek.
- Enlarges the native current-loop tracker so the selected loop name and step remain visible.
- Refreshes the Live Console into a 1600×900 dark performance surface with clearer spacing, grouping, button sizes, colors, transport state, dwell, banks, scope, overrides, and intensity.
- Preserves all V22 fixtures, creative Functions, addresses, I/O routes, Control One mappings, and playback behavior.

## Live workflow

| Control | QLC+ behavior |
|---|---|
| 32 pads | Latch/replace an Autoloop or toggle an exclusive Priority Look |
| Auto Loop | Switch the pad surface between Autoloops and Priority Looks |
| Banks 1–4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank / All | Sequential or random automatic playback |
| Dwell | 1, 2, 4, 8, or 16 measures, adjustable while running |
| Speed | 0.25×–4× Chaser multiplier, separate from dwell |
| Priority Look | Sole full-frame authority; underlying Autoloop continues |
| Color pads | Color-only override; unrelated behavior continues |
| Play/Pause | Start or pause the selected playback owner |
| Stop | Emergency global stop |

The 32 onscreen pads match the Control One: four columns by eight rows. The four small live indicators beside a pad are Banks 1–4 from top to bottom. Amber marks what is running; the tracker underneath gives the exact name.

## Included show patch

The V23 project is ready for:

- 4 × Both Lighting IR-4, 10-channel mode, addresses **1, 11, 21, 31**;
- 4 × Both Lighting BO-TUBE192, 40-channel mode, addresses **175, 215, 255, 295**; and
- private duplicates on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal and must not be routed directly to physical DMX.

Other DJs can reuse the hardware plug-in and Control One profile. Another fixture rig needs its own QLC+ patch and creative Functions; use V23 as the protected template and follow the [migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

## Supported hardware and evidence

| Device | Behavior | Current evidence |
|---|---|---|
| SoundSwitch Micro | One QLC+ DMX output | Physical output confirmed |
| Control One DMX 1 | First QLC+ DMX output | Physical output confirmed |
| Control One DMX 2 | Second QLC+ DMX output | Physical output confirmed independently |
| Control One MIDI | Performance workflow | Core workflow physically confirmed |
| Control One LEDs | Feedback and reconnect restoration | Software-tested; repeated hot-plug observation remains |

Simultaneous dual-port use with MIDI, LED feedback, VirtualDJ, and audio remains a qualification gate.

## Exact compatibility

The plug-in is a build-matched QLC+ binary module—not a universal standalone DLL.

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Build headers | Qt `6.8.1` |
| Qualified installed runtime | Qt `6.10.2` |
| Compiler | MSYS2 MinGW-w64 GCC `16.2.0` |
| `soundswitch.dll` SHA-256 | `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D` |

For a newer QLC+ version, install it side-by-side, rebuild the plug-in against that exact source, and qualify the new tuple before changing a show system. Never solve corrupted text or missing runtime DLLs by mixing files from different QLC+ installations.

## Rollback

V23 does not overwrite V21 or V22. The fastest show rollback is to close V23 and open the earlier `.qxw`.

The plug-in installer creates a hash-checked backup and receipt. With QLC+ closed:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

## Validation labels

- **Structurally validated:** hashes, XML, IDs, references, fixtures, mappings, monitor coverage, and release files pass automated checks.
- **Software-tested:** deterministic plug-in and workspace behavior passed against the pinned build.
- **Physical-output-tested:** the named device/port visibly controlled fixtures.
- **Gig-qualified:** the combined DJ/audio/OS2L/MIDI/LED/DMX workload survives the fault and soak plan.

V23 is structurally validated and inherits the V21/V22 software-tested runtime plus preceding physical hardware evidence. Its corrected UI needs the short owner observation. Repeated hot-plug, both Control One ports together, and the combined two-hour workload remain open gig-qualification gates.

## Documentation

- [V23 package and first test](releases/qlcplus-control-one/v23/README.md)
- [Switch from SoundSwitch](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [Logical channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [V23 provenance](docs/qlcplus-control-one/V23_LIVE_CONSOLE_PROVENANCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)

## Repository layout

```text
releases/qlcplus-control-one/v23/   current package
releases/qlcplus-control-one/v22/   unified creative rollback
releases/qlcplus-control-one/v21/   reliability rollback
releases/qlcplus-control-one/v20/   protected creative baseline
qlcplus/plugins/soundswitch/        native plug-in source and tests
qlcplus/workspace-tools/            deterministic workspace builders/validators
docs/qlcplus-control-one/           workflow, mapping, migration, and maintenance
native-core/ and installer/         archived standalone EmberLights history
```

The standalone EmberLights application is archived. Its useful interoperability research was retained; it is not a runtime dependency or active product direction.

## Independence and licensing

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, or QLC+.

The plug-in and included QLC+ interface code are distributed under Apache License 2.0. No SoundSwitch application, assets, database, firmware, or third-party driver is distributed. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
