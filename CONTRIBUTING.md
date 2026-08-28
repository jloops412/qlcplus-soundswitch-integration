# Contributing

Thank you for helping improve the QLC+ SoundSwitch integration.

## Read this first

QLC+ is the lighting runtime. Contributions must fit this architecture:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

QLC+ owns fixtures, patches, Scenes, Chasers and Autoloops, beat timing,
Autoplay, Virtual Console behavior, persistence, and routing. Custom code is
limited to SoundSwitch hardware transport, Control One translation/feedback,
reconnect behavior, and narrow gaps QLC+ cannot express cleanly.

Keep normal show behavior in QLC+. Custom code must stay bounded to the
SoundSwitch hardware integration and focused compatibility gaps.

## Before starting

1. Read [docs/00_START_HERE.md](docs/00_START_HERE.md).
2. Read the [project status and roadmap](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md).
3. Search existing issues and pull requests.
4. For a substantial change, open an issue describing one bounded outcome.
5. Do not include private SoundSwitch projects, client data, serial numbers,
   credentials, personal paths, or unlicensed vendor assets.

## Good contribution types

- QLC+ workspace builders, validators, Scenes, Chasers, and Virtual Console
  improvements.
- SoundSwitch Micro or Control One plug-in fixes with bounded tests.
- Control One MIDI, LED feedback, reconnect, and dual-output qualification.
- Fixture-neutral configuration and starter workspaces.
- Documentation, reproducible build tooling, and test evidence.
- A narrowly scoped upstreamable OS2L timing correction.

## Development setup

The active release is pinned to:

- QLC+ source commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`;
- QLC+ UI `5.3.0 GIT a124abe`;
- Qt build headers `6.8.1`; and
- Windows x64 for hardware plug-in qualification.

Follow [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for the exact source layout,
workspace validation, plug-in integration, and evidence boundaries.

## Branch and pull-request workflow

1. Fork the repository or create a topic branch from `main`.
2. Keep the change limited to one issue or one physically testable outcome.
3. Preserve public QLC+ Function IDs and logical channels unless the issue
   explicitly authorizes a coordinated migration.
4. Add or update deterministic tests for every behavior change.
5. Run the applicable checks below.
6. Open a pull request using the repository template.

Do not commit generated build directories, machine-local QLC+ installations,
installer receipts, device serials, or private show/source files.

## Required checks

For any default-branch change, run:

```powershell
releases/qlcplus-control-one/v26/Test-V26Package.ps1
```

For workspace changes, also run:

```powershell
qlcplus/workspace-tools/Test-V26Workspace.ps1 `
  -SourceWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw `
  -CandidateWorkspace releases/qlcplus-control-one/v26/IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw
```

For plug-in changes:

- build against the exact pinned QLC+ source and Qt/compiler tuple;
- run `soundswitch_protocol_tests` and `soundswitch_plugin_smoke_tests`;
- update source hashes only when the reviewed source actually changes; and
- do not replace a released DLL or checksum without a new versioned release
  package and complete provenance.

The pull-request workflow reruns repository/package validation. It does not
replace physical hardware testing.

## Evidence language

Use these terms precisely:

- **Structurally validated:** XML, references, IDs, patching, mappings, hashes,
  and package structure pass automated checks.
- **Software-tested:** deterministic plug-in or workspace behavior passed.
- **Physical-output-tested:** the named device, port, fixture, mode, and address
  visibly responded.
- **Gig-qualified:** fault recovery, soak, audio, OS2L, MIDI, LED feedback, DMX,
  and operator workflow passed together.

Never convert a structural or software result into a physical claim.

## Hardware and safety

- Close SoundSwitch before testing QLC+ ownership of the device.
- Disconnect fog, sparks, lasers, pyrotechnics, movers, and other hazardous
  loads unless the test explicitly requires them and proper controls are in
  place.
- Begin with a tester or simple low-risk LED fixture.
- Require an explicit operator action before any test emits live DMX.
- Ensure every failure and exit path returns the tested output to blackout.
- Never route the V26 private Universe 3 Priority layer directly to physical
  DMX.

## Licensing

Unless a file says otherwise, contributions are accepted under the
[Apache License 2.0](LICENSE). By submitting a contribution, you confirm that
you have the right to license it under those terms.

Do not copy SoundSwitch code, firmware, fixture libraries, artwork, trade dress,
or project content. Interoperability work must be independently implemented
from lawful testing, public documentation, and user-owned hardware.

## Questions

Use [GitHub Issues](SUPPORT.md) according to the support guide.
Security-sensitive reports belong in [SECURITY.md](SECURITY.md), not a public
issue.
