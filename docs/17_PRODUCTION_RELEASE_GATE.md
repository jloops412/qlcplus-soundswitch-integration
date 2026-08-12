# EmberLights Production Release Gate

Last updated: 2026-08-10.

## Release claims

EmberLights uses evidence-based release labels. A version number, installer, or successful compile does not by itself advance a build.

| Label | Meaning | Minimum evidence |
| --- | --- | --- |
| Testing preview | A coherent installed application intended for controlled development tests. | Windows/Linux CI, package test, known limitations, safe defaults. |
| Gig-qualified V1 | Suitable for Joshua's rehearsed qualification rig with an independent backup path. | Hardware/DJ gates, eight-hour soak, shadow rehearsals, low-risk pilot. |
| Public beta | Supportable by unrelated Windows users on the published hardware matrix. | Repeatable onboarding, searchable offline fixtures, signed upgrade/rollback path, crash/diagnostic evidence, beta hardware corpus. |
| General public 1.0 | The finished free SoundSwitch alternative described by the product contract. | Every applicable parity-ledger row is Verified/Equivalent or accepted Vendor-bound, plus all production gates. |

The current build is a **testing preview**. Gig-qualified V1 is the next claim to earn; public beta and parity-complete 1.0 remain later gates, not scope reductions.

## Production blockers

### Software gates we can advance without owned hardware

| Gate | Current evidence | Required before public production |
| --- | --- | --- |
| Deterministic scheduling | Allocation-free engine and measured native benchmarks pass. | Installed Windows p99 jitter, latency, cold-start, CPU, and memory evidence on minimum and typical PCs. |
| Sustained reliability | Unit, sanitizer, smoke, and million-tick tests pass. | Eight-hour installed-app soak, system sleep/resume, clock discontinuity, queue-storm, and fault-injection reports. |
| Runtime containment | Input/output work is off the scheduler thread; generation-stamped atomic package activation and persistent last-known-good snapshots are implemented. | Runner must still move to a separately contained process and survive Studio/UI failure; installed Windows fault-injection evidence remains required. |
| Diagnostics | Live counters and project validation are visible. | Durable structured session log, crash evidence, redacted export, release/build identity, and supportable fault codes. |
| Project durability | Atomic save, checksum, `.bak` recovery, verified 20-version local restore history, 100-state Studio Undo/Redo, and unknown-record retention pass. | Schema migration tests, cross-version open/save corpus, asset bundle import/export, and cross-machine rollback workflow. |
| Fixture onboarding | Local profiles and bounded QXF/OFL-export import work. | Searchable pinned offline catalog, conformance corpus, safe updates/sharing, richer range editing, and cell-aware fixtures. |
| Installer lifecycle | The source workflow requires one CMake-staged, version/commit-bound payload manifest; byte verification of the staged, portable, and installed trees; isolated clean install, file association, GUI/probe/qualification smoke, uninstall, and cleanup; and external checksums plus format-2 release evidence. The next Windows CI run must supply the actual passing artifact evidence. | Authenticode signing, cross-version upgrade and rollback tests (still reported `not-run`), repeated real-machine coverage, and a supported Windows matrix. |
| Network/security | OS2L defaults to loopback; Runner is offline-first. | Trusted-network warnings, endpoint threat review, dependency/SBOM evidence, malformed-input fuzzing, and no-internet operation proof. |
| Licensing/provenance | Third-party notices and QLC+/OFL boundaries are documented. | Art-Net OEM code, final fixture-corpus attribution, complete distributed-file audit, and release license decision. |

### Evidence that requires Joshua's Windows and lighting environment

1. VirtualDJ/OS2L two-hour synchronization, disconnect, predictive hold, and recovery capture.
2. Control One input map, safe LED feedback map, MIDI latency, disconnect/reconnect, and bundled profile.
3. Physical Art-Net, sACN, ENTTEC DMX USB Pro, and QLC+ bridge output tests.
4. Inventory and lawful compatibility route for MyDMX Buddy and the SoundSwitch USB interface.
5. Eight-hour strict qualification run on the DJ laptop and a lower-end Windows reference PC.
6. Three complete recorded-event shadow rehearsals with frame/log comparison.
7. One deliberately low-risk live pilot with the rehearsed backup controller available.

### Parity work required before general public 1.0

The binding detail remains in `13_SOUNDSWITCH_PARITY_LEDGER.md`. Major unfinished tracks include:

- metadata-assisted/cancellable audio-library resolution, beatgrid/waveform editing, exact track timelines, manual scripting, and effect generators; SHA-256/size media identity plus strict one-file and bounded folder relinking are now available in Studio;
- deterministic AutoScripting for tracks and Autoloops;
- SoundSwitch read-only inspection and lossless source bundling now work; semantic conversion, relinking, and conflict resolution still require a representative versioned sample corpus;
- full VirtualDJ mixer/transport behavior, audio fallback, then Serato and remaining documented integrations;
- complete MIDI behavior/feedback, named position and attribute cues, multi-cell fixtures, and production fixture distribution;
- smart-lighting and vendor-bound interoperability decisions where an official route exists.

## Qualification tool

Installed builds include `Tools\emberlights_qualify.exe`. It compiles and runs a 128-fixture, two-universe semantic show at 40 Hz, repeatedly exercises live commands and emergency state, measures scheduling health, and writes a machine-readable JSON report.

The tool does not transmit DMX by default. Windows package CI runs that non-outputting default from the installed executable and retains its version/commit-bound JSON report. `--network-loopback` additionally exercises Art-Net and sACN packet transmission, but use that flag only on an isolated machine with no local QLC+ bridge or other software capable of forwarding loopback packets to real hardware.

Quick smoke run:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_qualify.exe" --duration 120
```

Production timing/soak run:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_qualify.exe" --strict --duration 28800
```

The strict profile enforces the current four-second Runner-start, 5% one-core CPU, 100 MB headless RSS, and 5 ms p99 scheduling ceilings, plus no scheduler resynchronization, no output queue drops/send failures, no command rejection, and continuous frame progress. Short non-strict package smoke runs record timing observations but do not pass or fail an unstable hosted machine on the strict deadline-miss ratio. Passing this synthetic test is necessary but not sufficient: it does not replace VirtualDJ, MIDI, receiver, fixture, or live-event qualification.

Keep the generated JSON with the exact installer version, Windows build, machine specifications, normal background workload, and test notes. A production claim requires reports from both the primary DJ computer and the minimum/reference computer.

## Immediate execution order

1. Land and exercise machine-readable qualification and timing evidence.
2. Capture real VirtualDJ and Control One behavior with the installed build.
3. Qualify physical output paths and disconnect/reconnect behavior.
4. Isolate Runner lifecycle from Studio/UI and complete atomic activation/recovery.
5. Run the eight-hour/fault matrix and shadow rehearsals.
6. Harden fixture onboarding, updates, signing, installer lifecycle, and public support evidence.
7. Continue the parity ledger through scripting, automation, migration, and integrations before public 1.0.
