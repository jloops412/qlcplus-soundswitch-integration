# QLC+ SoundSwitch V27 — Full Rig

V27 extends the immutable V26 Autoplay Clarity show to the complete eleven-fixture rig: four Both Lighting IR-4s, one Chauvet Wash FX Hex, two American DJ Focus Spot Two movers, and four Both Lighting BO-TUBE192 tubes.

QLC+ remains the only lighting application. V27 adds no bridge, daemon, replacement firmware, or second show engine.

```text
VirtualDJ -- direct OS2L --> QLC+
Control One -- MIDI ------> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

> Evidence boundary: this release documentation describes the V27 candidate contract. It does not claim that V27 has been physical-output-tested, headless-qualified, combined-soak-qualified, or gig-qualified. Do not connect the full rig until the candidate workspace, fixture definition, plug-ins, and hashes have passed the package checks and the controlled bench in [FULL_RIG_PATCH_AND_BENCH.md](FULL_RIG_PATCH_AND_BENCH.md).

## Package contract

A deployable V27 folder must contain the exact reviewed versions of:

- `IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw`;
- `American-DJ-Focus-Spot-Two.qxf`;
- `SoundSwitch-Control-One-Performance.qxi`;
- the V27 build-matched `soundswitch.dll`;
- the pinned build-matched `os2l.dll`;
- package validation, installation, rollback, and SHA-256 records; and
- these release and bench documents.

Do not deploy a partial folder or substitute a DLL from another QLC+/Qt build. The Focus fixture definition is a required dependency, not optional documentation.

`SHA256SUMS.txt` must cover every published package file except itself and must pin the workspace, Focus `.qxf`, both DLLs, profile, scripts, and documentation. The installer must back up any same-name user fixture definition before installing the V27 `.qxf`, and rollback must either restore that prior file or remove the V27 copy when no prior file existed.

## Exact full-rig patch

QLC+ stores addresses internally as zero-based values. This table uses the one-based addresses shown on the fixtures.

| Fixture | QLC fixture ID | Universe | Mode | Display address/span |
|---|---:|---:|---|---:|
| IR-4 1 | 0 | 1 | `10 Channel` | 001–010 |
| IR-4 2 | 1 | 1 | `10 Channel` | 011–020 |
| IR-4 3 | 2 | 1 | `10 Channel` | 021–030 |
| IR-4 4 | 3 | 1 | `10 Channel` | 031–040 |
| Wash FX Hex | 4 | 1 | `40 Channel` | **041–080** |
| Focus Spot Two A | 9 | 1 | `18 Channel` | **081–098** |
| Focus Spot Two B | 10 | 1 | `18 Channel` | **099–116** |
| Reserved | — | 1 | — | **117–174 free** |
| BO-TUBE192 1 | 5 | 1 | `40 Channel` | 175–214 |
| BO-TUBE192 2 | 6 | 1 | `40 Channel` | 215–254 |
| BO-TUBE192 3 | 7 | 1 | `40 Channel` | 255–294 |
| BO-TUBE192 4 | 8 | 1 | `40 Channel` | 295–334 |

Every V26 fixture remains at its original address. V27 fills only the previously unused range 041–116 and deliberately leaves 117–174 open. The physical output frame remains 334 channels.

The Wash FX Hex must be set to its manufacturer `40 Channel` personality. Each Focus Spot Two must be set to `18 Channel`. A fixture placed in a different personality will not respond safely or predictably to this workspace.

## Private Priority layer

QLC+ Universe 3 contains exact private duplicates of the physical rig:

- physical IDs 0–10 map to private IDs 100–110;
- each private fixture uses the same address, model, mode, and channel count as its physical partner; and
- the private frame remains on SoundSwitch priority line 4 with 334 channels.

Universe 3 is an internal full-frame buffer. Never route it to Micro DMX, Control One DMX 1, Control One DMX 2, Art-Net, sACN, or another physical output. While a Priority Look is active, the plug-in substitutes that private frame for physical Universe 1 while the underlying Autoloop continues advancing.

## Complete creative closure

V27 is built from the exact immutable V26 workspace and preserves its public Function IDs, logical channels, raw Chaser roots, step references, ownership, dwell, speed, and Control One mapping.

The candidate contains 2,100 Functions:

- 1,812 Scenes;
- 150 Chasers; and
- 138 Collections.

The live creative closure contains exactly 1,140 Scene leaves:

- 1,024 raw Autoloop step Scenes: 128 loops × 8 steps;
- 102 Priority Look leaf Scenes: 22 direct looks plus 10 eight-step moving looks;
- 5 performance Scenes; and
- 9 color overrides.

Each raw Autoloop Scene carries a complete 276-channel-pair physical frame for all eleven fixtures. Each Priority leaf carries the corresponding complete 276-pair private frame. Performance Scenes now include the tubes, Wash, and movers. Color overrides now include all tube emitters, all six Wash zones, and the Focus color wheels while deliberately leaving movement and intensity underneath them.

Nine additive Focus A/B position Scenes use Function IDs 2175–2183. Function 2184, `MOVEMENT — FOCUS A/B SWEEP`, sequences those positions and is assigned to the existing MOVE control without changing that public widget or input identity.

The exact position data and the required A/B orientation check are in [FULL_RIG_PATCH_AND_BENCH.md](FULL_RIG_PATCH_AND_BENCH.md).

## Intensity groups

| Target | V27 role |
|---|---|
| Global | Multiplies all four designated emitter/dimmer groups on the physical or active Priority frame |
| Group 1 | Four IR-4 fixtures |
| Group 2 | Wash FX Hex direct RGBAWUV emitters only |
| Group 3 | Four BO-TUBE192 tubes |
| Group 4 | Focus Spot Two main and UV dimmers only |
| Scripted | Retained control state |

Wash program, speed, auto-dimmer, and strobe channels are not intensity-scaled. Focus movement, color, gobos, prism, shutters, focus, internal shows, and function/reset channels are not intensity-scaled.

## Focus fixture definition

Pinned QLC+ source `a124abebe0b5ad6077727c561a5a0e1f3730810c` does not contain the Focus Spot Two definition. Keep the bundled file named exactly:

```text
American-DJ-Focus-Spot-Two.qxf
```

in the V27 package. Before opening the workspace, install a copy in the QLC+ user fixture directory:

```text
%USERPROFILE%\QLC+\Fixtures\American-DJ-Focus-Spot-Two.qxf
```

Close and reopen QLC+ after installing the definition. Do not continue if QLC+ reports a missing fixture definition, substitutes a generic fixture, or cannot resolve the exact `18 Channel` mode.

## Manual installation

Normal installation uses File Explorer; PowerShell is optional.

1. Preserve the complete V26 folder and its recorded hashes. Do not overwrite it.
2. Close SoundSwitch and every QLC+ window.
3. Extract the complete V27 folder to a new location and verify its recorded SHA-256 values.
4. Confirm the target application is the pinned complete QLC+ `5.3.0 GIT a124abe` installation. Do not mix executables, Qt libraries, or plug-ins from another build.
5. Back up the installed `soundswitch.dll` and `os2l.dll` from the `Plugins` folder beside `qlcplus5.exe`.
6. Copy the V27 package's build-matched DLLs into that `Plugins` folder only after their package hashes validate.
7. Back up `%USERPROFILE%\QLC+\Fixtures\American-DJ-Focus-Spot-Two.qxf` if that path already exists. Copy the V27 `.qxf` there.
8. Back up any same-name profile, then copy `SoundSwitch-Control-One-Performance.qxi` to `%USERPROFILE%\QLC+\InputProfiles\` and keep the packaged original unchanged.
9. Set the physical Wash to `40 Channel`, the two Focus units to `18 Channel`, and every fixture to the address in the patch table.
10. Start QLC+ with physical outputs disabled, open the V27 workspace, and resolve any fixture-definition or mode error before proceeding.
11. In Input/Output, confirm Universe 1 is the only physical show universe and Universe 3 remains private on `soundswitch:priority-layer`, line 4.
12. Complete the no-output visual pass and controlled physical bench before enabling the full rig.

Do not replace the whole Control One composite USB driver with a generic driver; that can remove the MIDI interface.

### Optional checked PowerShell route

From the extracted V27 folder, maintainers can validate the exact package and perform a receipt-backed install:

```powershell
.\Test-V27Package.ps1
.\Install-V27.ps1 -QlcRoot 'C:\QLC+'
```

Use the actual folder containing `qlcplus5.exe` if it is not `C:\QLC+`. The installer refuses an unpinned core, validates the package hashes, and backs up the existing plug-ins, Focus definition, and input profile before copying anything. Its final output names the recoverable backup folder.

To restore that recorded pre-V27 state later:

```powershell
.\Rollback-V27.ps1 -QlcRoot 'C:\QLC+'
```

Rollback uses the install receipt and preserves any post-install file that changed before restoring or removing the V27 targets. This automated route does not replace the no-output inspection or controlled physical bench.

## UV and mover safety

The Focus Spot Two manual identifies its UV source as Risk Group 3. V27 never enables the real Focus UV emitters: every released loop, performance Scene (including `UV`), override, and Priority frame keeps the Focus UV shutter closed and UV dimmer at zero. The `UV` performance look uses the Wash/tubes plus low visible-color output from the Focus main source.

Leave Focus UV disabled until a qualified operator has applied the manufacturer instructions, excluded people from the exposure area, and confirmed aim and distance. Never stare into the aperture or aim UV output where eyes can enter the beam.

Before any mover test, provide mechanical clearance through the full pan/tilt range, secure each fixture and safety cable correctly, close both shutters, set both dimmers to zero, and keep an immediate Stop/Blackout action available.

## Validation and first physical test

The current CI plug-in build evidence is deliberately narrow:

| Evidence | Recorded value |
|---|---|
| CI run | `33241230755` |
| Reviewed remote source commit | `bf85057b5958608034decacae8927b0714ee98ed` |
| Candidate `soundswitch.dll` SHA-256 | `19074A37AA915E1E39124CD14441025A2A83AB06EBDB14BD0953E2910B801DE3` |
| Deterministic V27 workspace SHA-256 | `A4F7559930E93485F3EA2815A0B44CA8E40ACDFCC43FB3B0430E863C64B2DC4B` |
| Passed in CI | Protocol, intensity, and plug-in-load smoke tests |
| Exact pinned QLC+ host ABI tested | **No** — evidence records `pinnedQlcHostAbiTested=false` |

This CI result is software evidence, not proof that the DLL has loaded inside the exact pinned Windows QLC+ host or driven hardware. Package promotion still requires the pinned-host and physical routes below.

From the repository root, the candidate builder and independent structural validator are:

```text
python qlcplus/workspace-tools/Build-V27FullRig.py
python qlcplus/workspace-tools/Test-V27Workspace.py
```

These checks prove structure and regression invariants only. They do not prove real addresses, fixture personalities, A/B placement, beam aim, output hardware, UV exposure safety, visual quality, or gig readiness.

The first hardware session must follow [FULL_RIG_PATCH_AND_BENCH.md](FULL_RIG_PATCH_AND_BENCH.md). Test one output path and one fixture class at a time before running the complete show.

## Rollback to V26

1. Stop playback, invoke global Blackout, disable physical outputs, and close QLC+.
2. Power down or disconnect the Wash and both Focus fixtures. V26 never targets their new addresses, but physical isolation is the safest rollback boundary.
3. Restore the backed-up V26 `soundswitch.dll` and `os2l.dll`, or reinstall the exact pinned V26 plug-in tuple.
4. Restore the pre-existing user fixture/profile backups. If none existed before V27, remove only the V27 copies installed in `%USERPROFILE%\QLC+\Fixtures` and `%USERPROFILE%\QLC+\InputProfiles`. Keep the packaged originals inside the separate V27 folder.
5. Open the preserved `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw` with its matching Control One profile.
6. Confirm the original IR-4 and tube addresses remain 001/011/021/031 and 175/215/255/295.
7. Confirm Universe 3 is still private, then re-enable one selected output and perform the V26 owner check.

Never delete or rewrite the V26 release to perform a rollback.

## Compatibility boundary

| Component | Required value |
|---|---|
| QLC+ UI | `5.3.0 GIT a124abe` stock core |
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| Qt build headers | `6.8.1` |
| Target | Windows x64 |
| Workspace, DLL, profile, and fixture hashes | Exact values recorded by the completed V27 package |

The plug-ins are build-matched QLC+ modules, not universal Windows drivers. A later QLC+ build requires a side-by-side installation, a rebuild against that source, and a new qualification pass.

This is independent community interoperability work. It is not affiliated with or endorsed by SoundSwitch, inMusic, Chauvet, ADJ, VirtualDJ, or QLC+.
