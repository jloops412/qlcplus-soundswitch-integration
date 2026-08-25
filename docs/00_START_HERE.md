# Start Here

The active project is the QLC+ SoundSwitch integration. Do not resume the retired standalone EmberLights application unless a future owner explicitly reopens that scope.

For the current system, use these documents in order:

1. [Project status and roadmap](qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md)
2. [V24 package and release notes](../releases/qlcplus-control-one/v24/README.md)
3. [Community SoundSwitch migration guide](qlcplus-control-one/COMMUNITY_MIGRATION_GUIDE.md)
4. [Control One workflow specification](qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md)
5. [State model and architecture](qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md)
6. [Mapping reference](qlcplus-control-one/MAPPING_REFERENCE.md)
7. [V24 Runtime Feedback provenance](qlcplus-control-one/V24_RUNTIME_FEEDBACK_PROVENANCE.md)
8. [V23 Live Console provenance](qlcplus-control-one/V23_LIVE_CONSOLE_PROVENANCE.md)
9. [V22 creative merge provenance](qlcplus-control-one/V22_UNIFIED_MERGE_PROVENANCE.md)
10. [Validation and maintenance](qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
11. [Post-test promotion and cleanup](qlcplus-control-one/POST_TEST_PROMOTION_AND_CLEANUP.md)

The V24 alpha-candidate workspace, profile, plug-in, installer, rollback, package test, and hashes are under `releases/qlcplus-control-one/v24/`. V23 is the Live Console rollback, V22 the unified creative rollback, V21 the reliability rollback, and V20 the protected creative baseline. Plug-in source is under `qlcplus/plugins/soundswitch/`; deterministic workspace tools are under `qlcplus/workspace-tools/`.

## Dedicated booth-node deployment

The ReadyNet private booth LAN, separate Windows QLC+ host, headless browser operation, network OS2L, recording architecture, and recovery gates are documented separately so they do not change the QLC+ control architecture:

1. [Booth node start here](booth-node/00_START_HERE.md)
2. [Windows deployment runbook](booth-node/01_WINDOWS_DEPLOYMENT_RUNBOOK.md)
3. [Network, headless control, and OS2L](booth-node/02_NETWORK_HEADLESS_AND_OS2L.md)
4. [Highest-quality event recording](booth-node/03_EVENT_RECORDING_ARCHITECTURE.md)
5. [Validation, recovery, and rollback](booth-node/04_VALIDATION_RECOVERY_AND_ROLLBACK.md)
6. [Backlog and decisions](booth-node/05_BACKLOG_AND_DECISIONS.md)
7. [Hardware inventory and evidence](booth-node/06_HARDWARE_INVENTORY_AND_EVIDENCE.md)
8. [Owner and operator system guide](booth-node/07_OWNER_SYSTEM_GUIDE.md)

The booth node is a staged deployment wrapper around the pinned V24 system. Its software/runtime contract is validated; physical lights and Booth-machine qualification remain pending. It does not revive EmberLights, add a bridge daemon, or introduce another show-time lighting application.

Historical handoffs under `docs/handoffs/` explain how the native QLC+ direction was chosen. They are retained for provenance, but this page and the V24 documents are authoritative when they differ.
