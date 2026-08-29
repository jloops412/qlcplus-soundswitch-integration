# Start Here

This project integrates SoundSwitch Micro and Control One hardware with QLC+.

At show time, run one lighting application:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

## I want to install or test it

Read these in order:

1. [Main project overview](../README.md)
2. [V27 Full Rig package, installation, first test, and rollback](../releases/qlcplus-control-one/v27/README.md)
3. [V27 full-rig patch and controlled bench](../releases/qlcplus-control-one/v27/FULL_RIG_PATCH_AND_BENCH.md)
4. [Community migration guide](qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
5. [Control One workflow](qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
6. [Validation and maintenance](qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)

The complete V27 candidate is under `releases/qlcplus-control-one/v27/`. Keep
the entire [V26 package](../releases/qlcplus-control-one/v26/README.md) intact as
the protected rollback. V27 preserves the V26 fixture addresses, adds the Wash
at 041–080 and Focus A/B at 081–098 and 099–116, leaves 117–174 free, and keeps
the tubes at 175/215/255/295.

Install the bundled Focus `.qxf` in the QLC+ user `Fixtures` folder and the
Control One `.qxi` in the user `InputProfiles` folder before opening the V27
workspace. Both DLLs remain matched to QLC+ `5.3.0 GIT a124abe`; do not install
them into an arbitrary QLC+ version.

V27 currently has structural and CI evidence only. It is not physical-output-
tested or gig-qualified. Its released programming always keeps the real Focus
UV shutters closed and UV dimmers at zero.

## I want to contribute

Read these in order:

1. [Contributing](../CONTRIBUTING.md)
2. [Development guide](DEVELOPMENT.md)
3. [Project status and roadmap](qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
4. [State model and architecture](qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
5. [Mapping reference](qlcplus-control-one/MAPPING_REFERENCE.md)
6. [V27 source provenance](../releases/qlcplus-control-one/v27/SOUNDSWITCH_SOURCE_PROVENANCE.md)
7. [V26 provenance](qlcplus-control-one/V26_AUTOPLAY_CLARITY_PROVENANCE.md)

Plug-in source is under `qlcplus/plugins/soundswitch/`. The focused OS2L delta
is under `qlcplus/patches/`. Deterministic workspace tools are under
`qlcplus/workspace-tools/`.

## Release lineage

- V27: current Full Rig alpha candidate; structurally/CI checked, awaiting the
  controlled physical bench and gig qualification.
- V26: protected Autoplay Clarity rollback and immutable V27 source baseline.
- V25: reviewed local source for V26.
- V24: Runtime Feedback rollback.
- V23: Live Console rollback.
- V22: Unified Pro creative rollback.
- V21: reliability rollback.
- V20: protected creative baseline.

Do not rewrite or delete the protected rollback packages.
