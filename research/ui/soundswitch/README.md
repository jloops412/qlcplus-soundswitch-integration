# SoundSwitch UI Evidence Workspace

This directory is the repository index for issue #30 and the capture process in `docs/19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`.

Do not commit third-party screenshots casually. Native captures and third-party reference images may have licensing/privacy implications and can bloat the repository. The approved default is:

- keep original evidence in the designated private evidence storage or a Git LFS/private artifact location;
- commit metadata, hashes, annotations, measurements, derived tokens, and approved small crops only when necessary;
- never use SoundSwitch screenshots as EmberLights skin assets;
- never claim a screenshot reveals literal CSS, source code, or private component names.

## Directory contract

```text
research/ui/soundswitch/
  README.md
  capture-manifest.template.json
  screen-analysis.template.md
  deviation-ledger.template.md
  manifests/          committed metadata for captured sessions
  analyses/           one analysis record per screen/state
  measurements/       derived geometry/token tables
  ledgers/            Preserve/Improve/Reject decisions
  approved-crops/      optional narrowly scoped research crops
```

Create directories only when they contain an artifact; do not add empty placeholder trees.

## Capture session procedure

1. Record exact SoundSwitch version/build.
2. Record Windows version, monitor resolution, app client size, DPI/scale, theme, and capture tool.
3. Record active project, venue, track, source, controller, output hardware, and connection state.
4. Set the application to the state named in the capture matrix.
5. Capture lossless PNG without resizing.
6. Hash the original file with SHA-256.
7. Create a manifest from `capture-manifest.template.json`.
8. Create an analysis from `screen-analysis.template.md`.
9. Tag every claim `MEASURED`, `ESTIMATED`, `DESIGN_TARGET`, or `BEHAVIORAL`.
10. Update the deviation ledger when a product decision is made.

## Required capture naming

```text
ss-<version>-<os>-<clientWxH>-<dpi>-<workspace>-<screen>-<state>-<sequence>.png
```

Example:

```text
ss-2.10.2-win11-1920x1032-100-live-autoloops-active-progress-001.png
```

Use the application client size when known; store full monitor resolution separately in the manifest.

## Minimum Tier A set

### Studio

- normal loaded track;
- Master/Group/Fixture hierarchy;
- selected cue and Inspector/dialog state;
- Autoloop banks 1–4;
- Autoloop context menu, duplicate/move/populate;
- Static Look editor;
- fixture library;
- fixture address/DMX chart;
- Preferences major tabs;
- saved/unsaved or save-related state when observable.

### Live

- source selector;
- Performance normal;
- Autoloops normal;
- Autoloop selected;
- Autoloop active and progress;
- exclusive bank;
- infinite repeat;
- track-duration repeat;
- Static Looks normal/active;
- DJ connected/disconnected;
- hardware connected/disconnected/multiple;
- notification;
- override active/inactive;
- blackout or equivalent safety state where safely observable.

### Dimensions

- 1366×768 / 100%;
- 1920×1080 / 100%;
- 2560×1440 or 4K with recorded high-DPI scaling when available.

## Review rules

A derived exact value cannot move into the approved Reference theme/layout unless:

- its source manifest exists;
- the original image hash exists;
- measurement method is described;
- app client size and DPI are known;
- article/video overlays are excluded;
- at least one reviewer confirms the value or the value remains marked `ESTIMATED`.

## Privacy scrub

Before committing metadata or crops, remove or redact:

- account email/name;
- local file paths containing personal names;
- music-library details not needed for analysis;
- license/activation identifiers;
- device serial numbers;
- venue/client names;
- notification content unrelated to UI research.

Keep the original evidence unmodified in private storage and record that a redacted derivative was created.
