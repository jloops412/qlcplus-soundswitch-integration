# EmberLights / QLC+ SoundSwitch Integration

EmberLights is now the working name for a QLC+ show project and a focused SoundSwitch hardware integration. The former standalone EmberLights lighting application is archived; it is reference material only and is not part of the live runtime.

The target runtime is deliberately simple:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch Hardware plug-in --> Micro or Control One DMX
```

QLC+ owns fixtures, Scenes, Chasers, beat timing, the Virtual Console, project persistence, and output routing. The custom plug-in is limited to SoundSwitch USB transport, Control One MIDI translation and feedback, reconnect handling, the full-frame Priority Look selector, and the current rig's temporary intensity scaling.

## Current release candidate

The latest staged set is V20:

- `releases/qlcplus-control-one/v20/IR4-TUBES-CONTROL-ONE-V20-UIUX-PORTABLE.qxw` — portable QLC+ workspace;
- `releases/qlcplus-control-one/v20/SoundSwitch-Control-One-Performance.qxi` — QLC+ input profile;
- `releases/qlcplus-control-one/v20/soundswitch.dll` — matched Windows plug-in binary from the tested DJ-PC build;
- `qlcplus/plugins/soundswitch/` — current plug-in source and smoke tests.

`.qxw` is the QLC+ workspace/project extension. `.qxi` is an input profile. The portable workspace has the original Micro USB serial removed, so the desired output must be selected once in QLC+ Input/Output.

V20 is a release candidate, not a community-ready release. Its XML and reference structure have been audited, and the core V19 behavior was preserved, but the final V20 UI pass still needs physical regression testing.

## Start here

- [Current status and next-session plan](docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
- [V20 release notes](docs/qlcplus-control-one/V20_RELEASE_NOTES.md)
- [Control One workflow](docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
- [Architecture and discoveries](docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
- [MIDI/logical-channel map](docs/qlcplus-control-one/MAPPING_REFERENCE.md)
- [Validation and maintenance](docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)

This is independent community interoperability work and is not an official SoundSwitch, inMusic, or QLC+ release.
