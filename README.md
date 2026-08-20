# EmberLights

EmberLights is an independently owned, general-purpose, SoundSwitch-first DJ and event lighting workstation.

## Product thesis

Build **at least the complete relevant SoundSwitch functional surface with an open controller/hardware model**, then exceed it with safer persistence, migration, portability, and event-aware tools. Joshua's hardware is the first qualification rig, not the product boundary.

The system has two operational modes:

- **Studio** prepares fixtures, venues, Autoloops, song scripts, controller mappings, migrations, and immutable show packages.
- **Runner** performs a compiled show with a small deterministic engine and no internet, AI, waveform, or library-scanning dependency.

SoundSwitch is the primary product reference. Wolfmix is a secondary source of useful live-control ideas. QLC+ is an optional compatibility bridge and implementation reference, not the product model.

## Binding V1 constraints

- Windows-launch-first, offline-first, and exceptionally lean; macOS follows later without blocking Windows delivery.
- Exactly two exposed DMX universes in V1.
- VirtualDJ and OS2L first; Serato is the second direct DJ-integration priority; live-audio BPM is automatic fallback, not the normal clock.
- Same-computer and separate-lighting-computer operation are equal requirements.
- Device-agnostic MIDI with a bundled SoundSwitch Control One profile.
- A scalable 64-bank/2,048-loop Autoloop library; four-bank controllers page through it.
- SoundSwitch migration is a core adoption feature.
- No AI or dynamic content generation in the live DMX path.
- No licensing-server dependency.

## Repository map

- [`docs/00_START_HERE.md`](docs/00_START_HERE.md) — authority, status, and reading order.
- [`docs/01_PRODUCT_REQUIREMENTS.md`](docs/01_PRODUCT_REQUIREMENTS.md) — product and user requirements.
- [`docs/03_ARCHITECTURE.md`](docs/03_ARCHITECTURE.md) — Studio/Runner design and runtime contracts.
- [`docs/04_V1_SCOPE_AND_ACCEPTANCE.md`](docs/04_V1_SCOPE_AND_ACCEPTANCE.md) — what V1 is and how it earns gig use.
- [`docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`](docs/08_DECISIONS_AND_OPEN_QUESTIONS.md) — decision log and questions for Joshua.
- [`docs/13_SOUNDSWITCH_PARITY_LEDGER.md`](docs/13_SOUNDSWITCH_PARITY_LEDGER.md) — binding feature-parity and evidence checklist.
- [`docs/14_RUNNER_LAB.md`](docs/14_RUNNER_LAB.md) — safe usage and limitations of the first end-to-end laboratory Runner.
- [`docs/15_WINDOWS_V1_TESTING.md`](docs/15_WINDOWS_V1_TESTING.md) — installer, setup, safety, and qualification guidance for Windows testing builds.
- [`docs/16_QLC_FIXTURE_IMPORT.md`](docs/16_QLC_FIXTURE_IMPORT.md) — safe QLC+ QXF fixture import workflow, conversion rules, and current limits.
- [`docs/17_PRODUCTION_RELEASE_GATE.md`](docs/17_PRODUCTION_RELEASE_GATE.md) — evidence required for gig qualification, public beta, and parity-complete 1.0.
- [`docs/31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md`](docs/31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md) — the owner-plus-three preview test and feedback checklist.
- [`docs/47_ADVANCED_MANUAL_DMX_TEST.md`](docs/47_ADVANCED_MANUAL_DMX_TEST.md) — safe fixture-agnostic channel/value inspection using the installed Advanced tool.
- [`spec/showpack.schema.json`](spec/showpack.schema.json) — initial portable authoring/show-package schema.
- [`native-core`](native-core) — dependency-light native reference engine and test harness.

## Current milestone

The current milestone is a coherent installable Windows V1 testing build. The native application now brings project creation, safe QLC+ `.qxf` fixture import, local fixture-profile editing, two-universe patching, reusable fixture groups and roles, Static Looks, a 64-by-32 Autoloop library, MIDI Learn, VirtualDJ/OS2L timing, Art-Net/sACN output, per-universe ENTTEC DMX USB Pro–compatible serial output, independently implemented native SoundSwitch Micro JLS1/WinUSB output pending physical qualification, and a contract-tested Control One two-port JLC1/JLS1 production Runner route with explicit experimental opt-in plus an installed fail-closed booth qualifier. The installed Advanced Manual DMX Test separately provides time-bounded literal channel/value inspection for an isolated unknown fixture without treating raw DMX as normal authoring vocabulary. Editable safety policy, live controls, diagnostics, validation, and recovery remain in one desktop shell. The first typed command/state facade now separates Live/safety behavior from the legacy Win32 presentation so later Default, Reference, controller, and user-skin surfaces can share the same actions. Installed builds also include a 128-fixture soak/qualification tool and machine-readable release evidence. The portable core remains independently tested and allocation-free on the DMX scheduling path.

Every push is compiled and tested on Windows and Linux. The Windows packaging job stages a self-contained portable build, creates an Inno Setup installer, installs it, and launches the installed GUI and hardware probe smoke tests. Linux packaging creates an installable Debian package and portable archive, installs the Debian package, runs the installed Runner/qualification/migration/MIDI smoke tests, and removes it cleanly. Tagged commits publish both platforms with checksums and release manifests.

`installer/EmberLights.nsi` is the limited-beta offline packaging fallback when the hosted Windows runner is unavailable; `installer/EmberLights.iss` and the installed-smoke workflow remain the authoritative release path.

Do not use this at a live event until the acceptance gates in `docs/04_V1_SCOPE_AND_ACCEPTANCE.md` are satisfied.
