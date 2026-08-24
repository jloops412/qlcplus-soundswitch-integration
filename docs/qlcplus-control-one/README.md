# QLC+ Control One and SoundSwitch Hardware

This directory records the reusable findings from making SoundSwitch Micro and Control One useful directly inside QLC+.

## Division of responsibility

QLC+ remains the lighting application. It owns fixture definitions, patching, Scenes, Chasers, Collections, beat timing, OS2L, the Virtual Console, project files, and normal output routing.

The SoundSwitch plug-in handles only the capabilities QLC+ does not provide for this hardware:

- Micro and Control One proprietary USB DMX transport;
- Control One performance-surface MIDI translation;
- Control One LED feedback;
- periodic device rescan and reconnect;
- full-frame Priority Look selection;
- temporary rig-specific intensity scaling that should later move to configuration.

No bridge, daemon, EmberLights process, custom lighting engine, or firmware replacement is required during a show.

## Current rig encoded in V23

- Four Both Lighting IR-4 fixtures, 10-channel mode, DMX addresses 1, 11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, DMX addresses 175, 215, 255, and 295.
- A private duplicate fixture layer on QLC+ Universe 3 provides full-frame Priority Looks.
- Universe 1 can feed Micro and/or Control One DMX 1; Universe 2 is reserved for Control One DMX 2 and carries the Control One MIDI/feedback patch.

The BO-TUBE192 40-channel mode is eight RGBWY zones and has no master dimmer. The current plug-in therefore scales its 160 emitter channels for Group 3 intensity. That is useful for this rig but is the largest remaining show-specific coupling in reusable plug-in code.

## Documentation

- `CONTROL_ONE_WORKFLOW_SPEC.md` — performer-facing behavior.
- `STATE_MODEL_AND_ARCHITECTURE.md` — ownership model and hard-won implementation findings.
- `MAPPING_REFERENCE.md` — stable QLC+ logical channels and Control One roles.
- `COMMUNITY_MIGRATION_GUIDE.md` — simple installation and SoundSwitch-switching guide for another DJ.
- `V20_RELEASE_NOTES.md` — the protected creative rollback baseline.
- `releases/qlcplus-control-one/v23/README.md` — current Live Console package, first-test check, self-test, and rollback.
- `releases/qlcplus-control-one/v22/README.md` — unified creative rollback package.
- `releases/qlcplus-control-one/v21/README.md` — V21 reliability rollback package.
- `V22_UNIFIED_MERGE_PROVENANCE.md` — authoritative V21/Variety Pro merge inputs, exact delta, and active-outline rationale.
- `V23_LIVE_CONSOLE_PROVENANCE.md` — deterministic UI correction, live-rail, and mode-switch delta.
- `VALIDATION_AND_MAINTENANCE.md` — regression, hot-plug, upgrade, and release checks.
- `POST_TEST_PROMOTION_AND_CLEANUP.md` — recovery-first local promotion and legacy archive rules.
- `PROJECT_STATUS_AND_ROADMAP.md` — verified status, unresolved risks, and prioritized work.
