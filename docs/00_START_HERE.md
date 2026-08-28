# Start Here

The active project is the QLC+ SoundSwitch integration. The standalone
EmberLights application is retired and preserved only in Git history.

At show time, run one lighting application:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

## I want to install or test it

Read these in order:

1. [Main project overview](../README.md)
2. [V26 package, installation, first test, and rollback](../releases/qlcplus-control-one/v26/README.md)
3. [Community migration guide](qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
4. [Control One workflow](qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
5. [Validation and maintenance](qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)

The complete V26 package is under `releases/qlcplus-control-one/v26/`. Both
DLLs are matched to QLC+ `5.3.0 GIT a124abe`; do not install them into an
arbitrary QLC+ version.

## I want to contribute

Read these in order:

1. [Contributing](../CONTRIBUTING.md)
2. [Development guide](DEVELOPMENT.md)
3. [Project status and roadmap](qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
4. [State model and architecture](qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
5. [Mapping reference](qlcplus-control-one/MAPPING_REFERENCE.md)
6. [V26 provenance](qlcplus-control-one/V26_AUTOPLAY_CLARITY_PROVENANCE.md)

Plug-in source is under `qlcplus/plugins/soundswitch/`. The focused OS2L delta
is under `qlcplus/patches/`. Deterministic workspace tools are under
`qlcplus/workspace-tools/`.

## Release lineage

- V26: current Autoplay Clarity alpha.
- V25: reviewed local source for V26.
- V24: Runtime Feedback rollback.
- V23: Live Console rollback.
- V22: Unified Pro creative rollback.
- V21: reliability rollback.
- V20: protected creative baseline.

Do not rewrite or delete the protected rollback packages.

## Historical warning

Old standalone-app plans, installers, PRs, issues, and source files may appear
in Git history or closed GitHub records. They are not active merely because
they are detailed or recent. See
[Archived standalone application](ARCHIVED_STANDALONE_APP.md).
