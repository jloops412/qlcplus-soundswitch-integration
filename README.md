# QLC+ for SoundSwitch Micro and Control One

Use SoundSwitch Micro or Control One hardware directly from QLC+, with a SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks, Priority Looks, overrides, intensity groups, LED feedback, and a complete mouse fallback.

At a show, run **one lighting application: QLC+**. No bridge, daemon, replacement firmware, or standalone EmberLights app is required.

> **Current alpha candidate:** [V24 Runtime Feedback](releases/qlcplus-control-one/v24/README.md). It is structurally validated and software-tested against the pinned QLC+ build. Run the five-minute physical check before an event.

## What runs

```text
VirtualDJ ── OS2L ─────────────▶ QLC+
Control One ── MIDI ───────────▶ QLC+
QLC+ ── SoundSwitch plug-in ───▶ Micro DMX
                             └─▶ Control One DMX 1 / DMX 2
```

QLC+ owns fixtures, Scenes, Chasers, Autoplay, beat timing, the Virtual Console, and show files. The focused plug-in handles only SoundSwitch USB transport, Control One MIDI/LED/reconnect, full-frame Priority Look selection, and current rig intensity scaling.

## Already using V21, V22, or V23?

1. Download and extract the complete V24 package.
2. Close QLC+.
3. Run `Test-V24Package.ps1` and `Install-SoundSwitchPlugin.ps1`.
4. Open `IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw`.

V24 includes a new plug-in; do not keep the V21–V23 DLL with the V24 workspace.

## New installation

1. Install the complete pinned QLC+ **5.3.0 GIT a124abe** build.
2. Install the normal manufacturer driver/software for the SoundSwitch device, then close SoundSwitch.
3. Download and extract the whole [V24 package](releases/qlcplus-control-one/v24/).
4. In PowerShell inside the extracted folder:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V24Package.ps1
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

5. Open `IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw`.
6. Select SoundSwitch Micro, Control One DMX 1, or Control One DMX 2 in QLC+ Input/Output.
7. For Control One MIDI, associate `SoundSwitch-Control-One-Performance.qxi` if QLC+ did not retain it.
8. Follow the included VirtualDJ OS2L note when both programs share a laptop.

Do not replace the entire Control One composite USB driver with a generic driver; its MIDI interface must remain available.

The complete walkthrough is in the [community migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

## What V24 improves

- Repairs QLC+'s single feedback route so clickable Banks, mode, dwell, transport, order, and speed reach the plug-in.
- Processes empty UI command Scenes on their positive edge only, preventing double dispatch.
- Keeps mouse operation available while Control One is unplugged and reconnecting.
- Shows the active raw Chaser in 32 visible four-bank live rails during manual play, Auto Bank, Auto All, and seek.
- Verifies that Start Bank/All stay latched, advance, and move the current-loop indicator.
- Preserves all V23 creative Functions, fixtures, addresses, Priority Looks, mappings, and playback ownership.

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

The V24 project is ready for:

- 4 × Both Lighting IR-4, 10-channel mode, addresses **1, 11, 21, 31**;
- 4 × Both Lighting BO-TUBE192, 40-channel mode, addresses **175, 215, 255, 295**; and
- private duplicates on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal and must not be routed directly to physical DMX.

Other DJs can reuse the hardware plug-in and Control One profile. Another fixture rig needs its own QLC+ patch and creative Functions; use V24 as the protected template and follow the [migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

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
| `soundswitch.dll` SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |

For a newer QLC+ version, install it side-by-side, rebuild the plug-in against that exact source, and qualify the new tuple before changing a show system. Never solve corrupted text or missing runtime DLLs by mixing files from different QLC+ installations.

## Rollback

V24 does not overwrite V21, V22, or V23. The fastest show rollback is to close V24 and open the earlier `.qxw`.

The plug-in installer creates a hash-checked backup and receipt. With QLC+ closed:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

## Validation labels

- **Structurally validated:** hashes, XML, IDs, references, fixtures, mappings, monitor coverage, and release files pass automated checks.
- **Software-tested:** deterministic plug-in and workspace behavior passed against the pinned build.
- **Physical-output-tested:** the named device/port visibly controlled fixtures.
- **Gig-qualified:** the combined DJ/audio/OS2L/MIDI/LED/DMX workload survives the fault and soak plan.

V24 is structurally validated and software-tested against the pinned build. Its isolated runtime check covered bank selection, two-way mode switching, chase speed, latched Start Bank/All, parent advancement, and current-loop feedback. It inherits preceding physical hardware evidence, but still needs the short fixture observation. Repeated hot-plug, both Control One ports together, and the combined two-hour workload remain open gig-qualification gates.

## Documentation

- [V24 package and first test](releases/qlcplus-control-one/v24/README.md)
- [Switch from SoundSwitch](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [Logical channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [V24 provenance](docs/qlcplus-control-one/V24_RUNTIME_FEEDBACK_PROVENANCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)

## Repository layout

```text
releases/qlcplus-control-one/v24/   current package
releases/qlcplus-control-one/v23/   Live Console rollback
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
