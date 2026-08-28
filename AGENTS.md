# Project Instructions for Build Agents

## Current owner directive

The standalone EmberLights application is retired. The active project is a
QLC+ workspace, creative show programming, and the smallest necessary
SoundSwitch Micro/Control One hardware integration.

Do not resume the native EmberLights application, its custom engine/UI, old
Preview installer line, Studio/Runner design, skin platform, or historical
standalone-app backlog unless the owner explicitly reopens that work.

At show time, QLC+ is the only lighting application:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch hardware plug-in --> Micro or Control One DMX
```

## Read first

1. `docs/00_START_HERE.md`
2. `docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md`
3. `docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md`
4. `docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md`
5. `docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md`
6. `CONTRIBUTING.md` and `docs/DEVELOPMENT.md` for repository changes

Closed issues, PRs, deleted default-branch files, and older commits are
historical provenance only. They never override these current records.

## Current release and rig

- Current alpha: `releases/qlcplus-control-one/v26/`.
- Workspace: `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw`.
- QLC+ source: `a124abebe0b5ad6077727c561a5a0e1f3730810c`.
- QLC+ UI: `5.3.0 GIT a124abe`; Qt build headers: `6.8.1`.
- Four Both Lighting IR-4 fixtures, 10-channel mode, Universe 1 addresses 1,
  11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, Universe 1 addresses
  175, 215, 255, and 295.
- Private duplicate fixtures on Universe 3 provide full-frame Priority Looks.
  Universe 3 must not be routed directly to physical DMX.

V25 is V26's reviewed source. V24 is the Runtime Feedback rollback, V22 the
unified creative rollback, V21 the reliability rollback, and V20 the protected
creative baseline. Preserve all release directories and their hashes.

## Workspace rules

- Preserve public Function IDs and logical channels used by Control One, OS2L,
  and the Virtual Console.
- Resolve Scene fixture IDs from the workspace. Never infer them from names or
  DMX addresses.
- Back up before edits. Validate XML, fixture patch/modes, Function references,
  Virtual Console widget IDs, and ID uniqueness afterward.
- For creative-only work, leave the Virtual Console/control layer unchanged.
- For UI/control-only work, prove creative Functions are unchanged.
- Prefer small, physically testable passes over bulk replacement.
- Keep personal paths, usernames, hardware serials, client data, tokens, and
  secrets out of published files.

## Plug-in rules

- Limit custom code to SoundSwitch USB transport, Control One MIDI
  translation/feedback/reconnect, full-frame Priority ownership, and behavior
  QLC+ cannot express cleanly.
- Do not add a bridge, daemon, second runtime, replacement firmware, or new
  lighting engine.
- Treat both DLLs as build-matched, not ABI-stable across QLC+/Qt versions.
- Keep QLC+ Function state authoritative. Retain only minimal translation and
  reconnect state.
- Preserve newest-frame-wins output, reconnect recovery, LED restoration,
  Priority Look behavior, and safe failure handling.
- Move the current hard-coded IR-4/tube intensity ranges into configuration or
  native workspace logic before calling the plug-in general-purpose.

## Validation and claim boundaries

Run the current package validator after every default-branch change:

```powershell
releases/qlcplus-control-one/v26/Test-V26Package.ps1
```

Use these labels precisely:

- **Structurally validated:** XML, references, IDs, patching, mappings, hashes,
  and package structure pass automated checks.
- **Software-tested:** deterministic plug-in/workspace behavior passed.
- **Physical-output-tested:** the named device, port, fixture, mode, and address
  visibly responded.
- **Gig-qualified:** soak, fault recovery, audio, OS2L, MIDI, LED feedback, DMX,
  and operator workflow passed together.

V26 is structurally validated. Earlier tests physically confirmed Micro output,
each Control One DMX port independently, Control One MIDI/core LEDs, OS2L,
Priority behavior, and the essential Autoloop workflow. V26 still requires its
final owner observation, repeated hot-plug, simultaneous ports, and the combined
two-hour workload. Never collapse those boundaries.

## GitHub and release rules

- The default branch's README, this file, and current QLC+ documents are the
  active project record.
- Work through a bounded issue and pull request. Do not push unreviewed product
  changes directly to `main`.
- Active CI validates the V26 package and repository hygiene. It does not prove
  hardware behavior.
- Never reintroduce the archived native-core or standalone installer workflow.
- A release installer may install only the build-matched QLC+ plug-ins and
  rollback/validation files. It is not a desktop EmberLights installer.
- Bind every package to exact hashes, source/build compatibility, install
  receipt, rollback path, and evidence.
- Never destructively modify the user's only show file or protected rollback
  workspace.

## End of work

1. Run applicable structural and software validation.
2. Update only records whose truth changed.
3. State exact files, hashes, limitations, and physical evidence boundaries.
4. Preserve unrelated work and protected releases.
5. Report the user-visible QLC+ result first, then evidence and one focused
   next action.
