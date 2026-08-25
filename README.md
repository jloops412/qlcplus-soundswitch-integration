# QLC+ for SoundSwitch Micro and Control One

Use SoundSwitch Micro or Control One hardware directly from QLC+, with a SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks, Priority Looks, parameter overrides, group intensity, controller feedback, and a complete mouse fallback.

At a show, run **one lighting application: QLC+**. No bridge, daemon, tracker process, replacement firmware, or standalone EmberLights app is required.

> **Current alpha:** [V26 Autoplay Clarity](releases/qlcplus-control-one/v26/README.md). It is structurally validated against the pinned QLC+ build and ready for the short owner check before show use.

## What runs

```text
VirtualDJ ── direct OS2L ───────────────▶ QLC+
Control One ── MIDI ───────────────────▶ QLC+
QLC+ Scenes / Chasers / Virtual Console │
QLC+ ── SoundSwitch output plug-in ─────┼─▶ Micro DMX
                                        ├─▶ Control One DMX 1
                                        └─▶ Control One DMX 2
```

QLC+ owns fixtures, creative Functions, Autoplay, sequential/random order, beat-counted dwell, chase speed, priority behavior, the Virtual Console, project files, and show routing.

The focused custom boundary contains only what the pinned stock build cannot provide cleanly:

- `soundswitch.dll` for proprietary SoundSwitch USB output, Control One MIDI/LED/reconnect translation, and full-frame Priority Look ownership;
- `os2l.dll`, a focused QLC+ plug-in correction that uses OS2L's reported BPM rather than bursty packet-arrival timing.

There is no custom QLC+ core executable in V26.

## Install or upgrade

1. Install the complete pinned QLC+ **5.3.0 GIT a124abe** build.
2. Install the normal manufacturer driver/software for the SoundSwitch device, then close SoundSwitch.
3. Download and extract the complete [V26 package](releases/qlcplus-control-one/v26/).
4. Close QLC+ and run inside the extracted folder:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V26Package.ps1
   .\Install-V26.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

5. Open `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
6. Select SoundSwitch Micro, Control One DMX 1, or Control One DMX 2 in QLC+ Input/Output.
7. Associate `SoundSwitch-Control-One-Performance.qxi` with Control One MIDI if QLC+ did not retain it.
8. Follow the included VirtualDJ OS2L reconnect note when both programs share one computer.

Do not replace the entire Control One composite USB driver with a generic driver; its MIDI interface must remain available.

## What V26 improves

- Keeps all **2,090 lighting Functions**, fixtures, addresses, Priority Looks, creative content, public IDs, mappings, and I/O routing from the reviewed V25 workspace.
- Uses QLC+'s ten native Autoplay Chasers as the only sequencing engine.
- Keeps ten clipped native Cue Lists for absolute pad seek without a visible or polling tracker.
- Observes all 128 raw Autoloops through read-only QLC+ Function monitors.
- Gives every pad a full-width four-bank running-state strip that follows manual play, Start Bank, Start All, and pad seek.
- Restores visible `1M / 2M / 4M / 8M / 16M` dwell choices on every multipage state.
- Keeps dwell live-adjustable and independent from the `0.25× / 0.5× / 1× / 2× / 4×` Chase Speed multiplier.
- Stabilizes direct VirtualDJ OS2L timing so a 75 BPM song cannot become a false 240 BPM reading because packets arrived in a burst.
- Leaves the stock QLC+ executable and stock Virtual Console color rules intact.

Stock QLC+ colors a Function started by another Chaser with its native **amber Monitoring** state. V26 enlarges that authoritative state instead of carrying a core/UI fork solely to make it green.

## SoundSwitch-familiar workflow

| Control | QLC+ behavior |
|---|---|
| 32 pads | Latch/replace an Autoloop or toggle one exclusive Priority Look |
| Auto Loop | Switch the pad surface between Autoloops and Priority Looks |
| Banks 1–4 | Medium, Colorful, Slow Dance, and Flashy |
| Start Bank | Sequential/random automatic playback in the selected bank |
| Start All | Sequential/random playback through Banks 1→4 |
| Dwell | 1/2/4/8/16 measures per selected loop, adjustable while running |
| Chase Speed | 0.25×–4× loop multiplier, separate from dwell |
| Priority Look | Sole full-frame authority; underlying Autoloop keeps advancing |
| Color pads | Color-only override; unrelated intensity/movement continues |
| Play/Pause | Control the selected playback owner |
| Stop | Emergency global stop |

The 32 onscreen pads match the Control One's four-column by eight-row orientation. The strip beneath each pad contains one native monitor per bank; the amber segment is the raw Chaser QLC+ is actually running.

## Included show patch

- 4 × Both Lighting IR-4, 10-channel mode, DMX addresses **1, 11, 21, 31**;
- 4 × Both Lighting BO-TUBE192, 40-channel mode, DMX addresses **175, 215, 255, 295**; and
- private duplicates on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is internal and must not be routed directly to physical DMX.

Other DJs can reuse the hardware plug-ins and Control One profile. Another fixture rig needs its own QLC+ patch and creative Functions; follow the [community migration guide](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md).

## Supported hardware and evidence

| Device | Behavior | Evidence |
|---|---|---|
| SoundSwitch Micro | One QLC+ DMX output | Physical output confirmed |
| Control One DMX 1 | First QLC+ DMX output | Physical output confirmed |
| Control One DMX 2 | Second QLC+ DMX output | Physical output confirmed independently |
| Control One MIDI | Performance workflow | Core workflow physically confirmed |
| Control One LEDs | Feedback and reconnect restoration | Core feedback observed; repeated hot-plug remains a qualification gate |

Simultaneous dual-port use with MIDI, LED feedback, VirtualDJ, and audio remains a qualification gate.

## Exact compatibility

Both DLLs are build-matched QLC+ modules, not universal drivers.

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` stock core |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Qt build headers | `6.8.1` |
| `soundswitch.dll` SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |
| `os2l.dll` SHA-256 | `EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513` |
| V26 workspace SHA-256 | `ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570` |

Install future QLC+ versions side-by-side, rebuild both plug-ins against the exact new source, and qualify the tuple before switching a production show. Never solve corrupted text or missing DLLs by mixing files from different QLC+ installations.

## Validation boundary

V26's release check validates hashes, XML, Function/reference integrity, fixture patching, Autoplay parents, native seek coverage, dwell timing, all 128 running monitors, the Control One profile, script syntax, and removal of personal paths.

It does not replace the final owner observation or a physical show test. Before an event, verify Start Bank/All progression, moving native feedback, live dwell changes, stable BPM, Priority Look release, controller reconnect, and the selected DMX output. A combined two-hour VirtualDJ/audio/OS2L/MIDI/LED/DMX workload remains the gig-qualification target.

## Rollback

The installer backs up both previous plug-ins and writes a hash-checked receipt. With QLC+ closed:

```powershell
.\Rollback-V26.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

The workspace is non-destructive; previous `.qxw` releases remain available.

## Documentation

- [V26 package and first test](releases/qlcplus-control-one/v26/README.md)
- [Switch from SoundSwitch](docs/qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [Logical channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
- [V26 provenance](docs/qlcplus-control-one/V26_AUTOPLAY_CLARITY_PROVENANCE.md)
- [Focused QLC+ compatibility patch](qlcplus/patches/README.md)

## Repository layout

```text
releases/qlcplus-control-one/v26/   current alpha package
releases/qlcplus-control-one/v24/   prior runtime-feedback rollback
releases/qlcplus-control-one/v22/   unified creative rollback
releases/qlcplus-control-one/v21/   reliability rollback
releases/qlcplus-control-one/v20/   protected creative baseline
qlcplus/plugins/soundswitch/        native hardware/workflow plug-in source and tests
qlcplus/patches/                     focused OS2L compatibility source patch
qlcplus/workspace-tools/            deterministic workspace builders and validators
docs/qlcplus-control-one/           workflow, mapping, migration, and maintenance
native-core/ and installer/         archived standalone EmberLights history
```

Standalone EmberLights is archived. Its useful hardware research was retained; it is not a runtime dependency or active product direction.

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, VirtualDJ, or QLC+.
