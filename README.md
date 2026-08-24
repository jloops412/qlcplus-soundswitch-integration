# QLC+ SoundSwitch Hardware and Control One Workflow

Use SoundSwitch Micro and Control One hardware directly from QLC+, with a SoundSwitch-familiar live workflow, VirtualDJ OS2L timing, four Autoloop banks, full-frame Priority Looks, parameter overrides, group intensity, LED feedback, and a complete mouse fallback.

At show time there is one lighting application: **QLC+**. There is no bridge, daemon, replacement lighting engine, or standalone EmberLights process.

> **Current alpha candidate:** [QLC+ SoundSwitch V22 Unified Pro](https://github.com/jloops412/EmberLights/releases/tag/v22). V22 is exact-build-pinned and structurally validated. Its merged creative content and active-loop display still need the short owner test before production promotion.

## The design in one picture

```text
VirtualDJ ── OS2L ───────────────▶ QLC+
Control One ── MIDI ─────────────▶ QLC+
QLC+ ── SoundSwitch plugin ──────▶ Micro DMX
                              └──▶ Control One DMX 1 / DMX 2
```

QLC+ remains responsible for fixtures, Scenes, Chasers, beat timing, Autoplay, Virtual Console pages, project storage, and normal routing. The focused native plug-in handles only behavior QLC+ cannot express cleanly by itself:

- proprietary SoundSwitch USB DMX transport;
- Control One MIDI translation and LED feedback;
- reconnect handling;
- full-frame Priority Look selection; and
- temporary current-rig group-intensity scaling.

The retired standalone EmberLights code remains historical research only. It is not a runtime dependency.

## V22 Unified Pro

V22 resolves a split development history. It keeps the V21 reliability, Control One, priority, Autoplay, and UI framework, then imports the authored improvements from the later **All Banks Variety Pro** workspace.

- 128 native QLC+ Autoloops across Medium, Colorful, Slow Dance, and Flashy banks.
- 22 upgraded Colorful/Flashy Autoloops from Variety Pro.
- 176 supporting native QLC+ Scene steps imported with collision-safe private IDs.
- Existing public Function IDs, logical MIDI channels, fixtures, I/O, manual ownership, Autoplay parents, Priority Looks, and plug-in binary preserved.
- A read-only outline bound to every raw Chaser, so the selected pad follows manual playback, Auto Bank, Auto All, and live seek as the active loop changes.
- The native Now Playing strip remains the authoritative text readout.
- V20 and V21 remain untouched rollback points.

The exact merge can be regenerated with [`Merge-V22UnifiedPro.ps1`](qlcplus/workspace-tools/Merge-V22UnifiedPro.ps1), and the distributable validates itself with [`Test-V22Package.ps1`](releases/qlcplus-control-one/v22/Test-V22Package.ps1).

## Supported hardware

| Device | Current behavior | Evidence |
|---|---|---|
| SoundSwitch Micro | One QLC+ DMX output | Physical output confirmed |
| Control One DMX 1 | First QLC+ DMX output | Physical output confirmed |
| Control One DMX 2 | Second QLC+ DMX output | Physical output confirmed independently |
| Control One MIDI | Performance surface and workflow | Core workflow physically confirmed |
| Control One LEDs | State feedback and reconnect restoration | Software-tested; repeated hot-plug observation remains |

Multiple devices enumerate normally. Simultaneous dual-port operation under full MIDI, LED, VirtualDJ, and audio load remains a qualification gate rather than a finished community-support claim.

## Download and upgrade

Download the Windows archive and SHA-256 file from the [V22 release](https://github.com/jloops412/EmberLights/releases/tag/v22), then extract the whole archive.

In PowerShell inside the extracted folder:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\Test-V22Package.ps1
```

If V21 is already installed against the pinned QLC+ build, the plug-in is already correct: open the V22 workspace and leave the DLL alone.

For a new or recovery installation, close QLC+ and run:

```powershell
.\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

Then:

1. open `IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw`;
2. select the desired Micro or Control One output in QLC+ Input/Output if it was not retained;
3. associate `SoundSwitch-Control-One-Performance.qxi` with the Control One MIDI input if needed; and
4. follow the included VirtualDJ OS2L note when both applications share a laptop.

The workspace contains no personal USB serial number or machine path.

## Exact QLC+ compatibility

The plug-in is a QLC+ binary module, not a version-independent standalone DLL.

| Component | Pinned value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |
| Build headers | Qt `6.8.1` |
| Qualified installed runtime | Qt `6.10.2` |
| Compiler | MSYS2 MinGW-w64 GCC `16.2.0` |
| `soundswitch.dll` SHA-256 | `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D` |

The installer rejects a different QLC+ core by default. Install future QLC+ updates side-by-side, rebuild the plug-in against that exact source, and qualify the new tuple before changing a production system.

## Included example show

The V22 `.qxw` is a complete show for this patch:

- four Both Lighting IR-4 fixtures in 10-channel mode at addresses 1, 11, 21, and 31;
- four Both Lighting BO-TUBE192 fixtures in 40-channel mode at addresses 175, 215, 255, and 295; and
- matching private duplicates on QLC+ Universe 3 for full-frame Priority Looks.

Universe 3 is an internal priority layer and must not be routed directly to physical DMX.

The show file is immediately useful for that rig. The hardware plug-in and input profile are reusable for another DJ, but another fixture rig needs its own QLC+ patch, Scenes/Chasers, priority duplicates, and intensity configuration. Current group-intensity addresses are still encoded for the example rig and are the largest remaining portability limitation.

## Control One workflow

| Control | QLC+ behavior |
|---|---|
| 32 pads | Latch/replace an Autoloop or select an exclusive Priority Look |
| Auto Loop | Toggle the surface between Autoloops and Priority Looks |
| Banks 1–4 | Select Medium, Colorful, Slow Dance, or Flashy |
| Shift + Bank | Start automatic playback for that bank |
| Bank/All scope | Choose one-bank or all-bank Autoplay |
| Autoloop Override | Toggle sequential/random order in this adaptation |
| Play/Pause | Run or pause the selected manual/automatic owner without requiring a playing song |
| Pan/Speed encoder | Beat-aligned Chaser speed multiplier, independent of dwell |
| Intensity strip | Global, fixture-group, or Scripted intensity target |
| Color pads | Sparse color override while unrelated loop behavior continues |
| Stop All | Emergency global stop |

Priority Looks can be still Scenes or moving Chasers. While a Look is held or latched, it is sole full-frame DMX authority; the underlying Autoloop keeps advancing and returns when the Look releases.

Controls that do not yet have a useful QLC+ or current-rig role—such as OLED/firmware-specific behavior—remain intentionally deferred.

## Rollback and promotion

V22 does not overwrite V20 or V21. The fastest show rollback is to close V22 and reopen a known-good earlier `.qxw`.

If the plug-in itself must be restored, close QLC+ and use `Rollback-SoundSwitchPlugin.ps1`; the installer records and verifies its backup.

Do not delete older local workspaces immediately after downloading V22. First run the focused test in the [V22 package guide](releases/qlcplus-control-one/v22/README.md). After it passes, designate one V22 working copy as current and archive—not destroy—older experiments while retaining V20, V21, the plug-in backup, and the release archives.

## Evidence and qualification

- **Structurally validated:** package hashes, XML, patch, IDs, references, creative merge, priority closure, and monitor coverage pass automated checks.
- **Software-tested:** protocol tests and plug-in ABI/virtual-output smoke tests pass against the pinned build.
- **Physical-output-tested:** the named device or port visibly controlled fixtures.
- **Gig-qualified:** the complete DJ/audio/OS2L/MIDI/LED/DMX workflow survives the defined fault and soak test.

V22 is structurally validated and inherits the V21 software-tested runtime. Its unchanged hardware path inherits prior physical evidence for Micro, each Control One port independently, MIDI, OS2L, pad playback, and Priority Look takeover/release. The 22 merged loops and advancing outline need the focused owner observation. Repeated hot-plug, simultaneous Control One ports, and a combined two-hour workload remain explicit gig-qualification gates.

## Documentation

- [V22 package and first-test guide](releases/qlcplus-control-one/v22/README.md)
- [V22 release notes](releases/qlcplus-control-one/v22/RELEASE_NOTES.md)
- [Project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [MIDI and logical-channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [State model and architecture](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [V22 merge provenance](docs/qlcplus-control-one/V22_UNIFIED_MERGE_PROVENANCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
- [Post-test promotion and cleanup](docs/qlcplus-control-one/POST_TEST_PROMOTION_AND_CLEANUP.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## Repository layout

```text
releases/qlcplus-control-one/v22/   current distributable workspace and tools
releases/qlcplus-control-one/v21/   reliability rollback release
releases/qlcplus-control-one/v20/   protected creative rollback baseline
qlcplus/plugins/soundswitch/        native plug-in source and deterministic tests
qlcplus/workspace-tools/            reproducible workspace merge tooling
docs/qlcplus-control-one/           workflow, architecture, mapping, and maintenance
native-core/ and installer/         archived standalone EmberLights history
```

## Next work

1. Complete the short V22 owner test and promote it to the local alpha baseline.
2. Finish repeated Control One hot-plug and simultaneous-port qualification.
3. Move rig-specific intensity scaling from plug-in code into configuration/native workspace logic.
4. Build more purposeful event Priority Looks.
5. Grade every Autoloop and Scene for musical phrasing and fixture separation.
6. Publish a general starter workspace/configuration guide for other fixture rigs.

No standalone EmberLights revival, second lighting program, bridge service, custom firmware, or replacement lighting engine is part of the current plan.

## Independence and licensing

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, or the QLC+ project. Product names describe compatibility only.

The plug-in and included QLC+ interface code are distributed under Apache License 2.0. No QLC+ core, SoundSwitch software/assets/database, or firmware is distributed. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
