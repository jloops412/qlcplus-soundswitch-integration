# Skin Migration IR v1 — Cross-Application UI, Action, and Mapping Translation Contract

Status: **proposed Studio-only migration contract** for the EmberLights skin platform. It complements the loss-preserving SoundSwitch migration architecture and does not authorize unsupported format claims.

## 1. Purpose

EmberLights needs a standard path for users moving from other applications without coupling the core UI runtime to each source format.

Every supported adapter therefore converts a read-only source bundle into a normalized **Skin Migration IR**. Translation from the IR into `.emberskin`, `.emberoverlay`, controller-profile, theme, component, or action candidates is a separate deterministic step.

```text
foreign source
  -> source-specific parser
  -> Skin Migration IR
  -> semantic matcher/translator
  -> EmberLights candidate artifacts
  -> compatibility/licensing/manual-review report
```

The same pipeline supports:

- VirtualDJ skins and selected VDJScript concepts;
- selected QLC+ Virtual Console concepts;
- selected TouchOSC/Companion control-surface concepts;
- future documented lighting/DJ software formats;
- internal migration between older EmberLights skin generations.

SoundSwitch remains the primary workflow reference, but its familiar UI is delivered first through the bundled SoundSwitch Reference skin rather than by assuming a portable foreign-skin file exists.

## 2. Non-goals

- executing foreign scripts;
- redistributing proprietary source assets;
- scraping protected online libraries;
- claiming exact translation without evidence;
- treating a matching label as semantic proof;
- loading foreign formats in Perform;
- silently replacing unsupported actions;
- converting show/fixture content through the skin importer;
- using one importer’s source model as the EmberLights runtime model.

## 3. Source-bundle contract

Every migration begins with an immutable source bundle manifest.

```json
{
  "schemaVersion": 1,
  "adapterId": "org.emberlights.migration.virtualdj-skin",
  "adapterVersion": "0.1.0",
  "sourceProduct": "VirtualDJ",
  "sourceVersion": "observed-or-unknown",
  "capturedAt": "2026-08-12T00:00:00Z",
  "rootHash": "sha256:...",
  "files": [
    {
      "logicalPath": "skin.xml",
      "byteSize": 1234,
      "sha256": "...",
      "mediaType": "application/xml",
      "role": "layout-source"
    }
  ],
  "rights": {
    "ownerConfirmedImportRights": false,
    "redistributionAllowed": false,
    "notes": null
  }
}
```

Rules:

- sources are opened read-only;
- no source file is modified, renamed, rewritten, or normalized in place;
- relative paths are preserved as evidence but validated before use;
- source bytes and private user data never enter Git fixtures;
- public tests use synthetic or explicitly licensed samples;
- output directories are separate from source directories;
- duplicate, missing, unsupported, encrypted, or malformed inputs are reported explicitly;
- aggregate hash is deterministic and content-based.

## 4. IR top-level model

Conceptual shape:

```json
{
  "schemaVersion": 1,
  "migrationId": "...",
  "source": { ... },
  "surfaces": [ ... ],
  "elements": [ ... ],
  "styles": [ ... ],
  "assets": [ ... ],
  "foreignActions": [ ... ],
  "feedbackRules": [ ... ],
  "controllerBindings": [ ... ],
  "localization": [ ... ],
  "opaqueRecords": [ ... ],
  "diagnostics": [ ... ]
}
```

The IR is descriptive. It does not grant runtime behavior.

## 5. Provenance on every record

Every material record includes:

```text
source file hash
source location: byte range, XML path, JSON pointer, or equivalent
adapter version
observation/parse confidence
license/rights metadata where applicable
original source identifier/name
normalization notes
```

A translated item must always be traceable back to source evidence.

## 6. Translation status

Every source and generated item uses one of:

| Status | Meaning |
| --- | --- |
| `Exact` | Semantics and presentation are proven equivalent within declared limits |
| `Translated` | Different representation with equivalent intended behavior |
| `Approximated` | Useful bounded approximation; differences are explicit |
| `ManualReview` | Candidate exists but owner review/relink is required |
| `OpaquePreserved` | Source retained as evidence; no semantic claim |
| `Unsupported` | Known construct has no current EmberLights equivalent |
| `RejectedUnsafe` | Construct would violate safety/security/runtime policy |
| `Conflict` | Multiple source items or targets cannot be resolved deterministically |
| `IgnoredDecorative` | Non-behavioral source metadata intentionally omitted with reason |

Status is not inferred solely from parser success. A parsed action can remain semantically unverified.

## 7. Confidence

Separate confidence dimensions:

```text
parseConfidence
semanticConfidence
visualConfidence
targetResolutionConfidence
rightsConfidence
```

Each is `high`, `medium`, `low`, or `unknown` with evidence notes. A single aggregate percentage is prohibited because it hides which dimension is weak.

## 8. Surface model

A source surface describes one window/page/panel/controller view.

```json
{
  "id": "surface:main",
  "name": "Main",
  "kind": "mainWindow",
  "logicalWidth": 1920,
  "logicalHeight": 1080,
  "coordinateModel": "fixedAbsolute",
  "inputModes": ["mouseKeyboard"],
  "backgroundAssetId": "asset:bg",
  "children": ["element:..."],
  "source": { ... }
}
```

Supported `kind` values may include:

```text
mainWindow
secondaryWindow
page
panel
dialog
controllerSurface
touchSurface
unknown
```

Coordinate models:

```text
fixedAbsolute
relative
flow
constraint
grid
hybrid
unknown
```

## 9. Element model

All source elements normalize into broad semantic classes while retaining source-specific fields separately.

### 9.1 Layout/container elements

```text
Group
Panel
Page
Stack
Row
Column
Grid
Scroll
Dock
Split
AbsoluteLayer
Clip/Mask
UnknownContainer
```

### 9.2 Control elements

```text
Button
Toggle
Pad
Fader
Knob
XY
Meter
Progress
Text/Label
Icon/Image
Status
Tabs/PageSelector
Menu
List/Tree/Table
Waveform
Timeline
Browser
Matrix
UnknownControl
```

### 9.3 Element fields

```text
stable IR ID
source type/name/ID
parent and ordered children
source geometry and anchors
z-order and clipping
visibility/enabled source expressions
text/localization
visual state references
input events
foreign action references
feedback references
accessibility metadata when present
source-specific payload
translation status/confidence/provenance
```

Unknown payloads are bounded and preserved as opaque data or hashes according to privacy/licensing policy.

## 10. Geometry and layout conversion

### 10.1 Preserve mode

`Preserve fixed canvas` generates a bounded `LegacyCanvas`/`AbsoluteLayer` candidate:

- logical source dimensions retained;
- scale-to-fit and letterbox policies explicit;
- source z-order and clipping retained where possible;
- no claim of responsive behavior;
- mandatory EmberLights safety chrome remains platform-owned/outside the legacy canvas;
- incompatible source geometry is reported.

### 10.2 Modernize mode

`Modernize responsively` analyzes:

- repeated alignments and spacing;
- source groups/panels/pages;
- rows/columns and repeated matrices;
- common size classes;
- edge anchoring and stretch behavior;
- semantic roles such as status strip, pad grid, fader bank, browser, inspector;
- source resolution/aspect assumptions.

It generates candidate Grid/Row/Column/Dock/Split constraints. Visual similarity alone does not make this exact; generated constraints normally begin as `Approximated` or `ManualReview`.

### 10.3 Hybrid mode

A skin may retain one fixed/visual centerpiece inside responsive platform chrome. The report identifies which regions remain fixed and their fallback behavior.

## 11. Style model

Styles normalize into semantic or raw source observations:

```text
fill/background
foreground/text
border/stroke
radius
font family/size/weight/style
padding/margin/gap
opacity
image/source rectangle
state-specific variants
animation description
```

Translation prefers EmberLights semantic tokens. Raw source colors/sizes may be imported as candidate custom tokens but are not automatically considered accessible or responsive.

Every generated theme includes:

- missing required token report;
- contrast/target-size/focus findings;
- state-role coverage;
- asset/license report;
- raw override count;
- recommended token consolidation.

Unsupported animations remain decorative evidence and never become unbounded runtime scripts.

## 12. Asset model

```json
{
  "id": "asset:...",
  "sourceLogicalPath": "...",
  "sha256": "...",
  "byteSize": 0,
  "kind": "png",
  "dimensions": {"width": 0, "height": 0},
  "usage": ["background", "icon", "stateSprite"],
  "license": null,
  "redistributionAllowed": false,
  "translationStatus": "ManualReview"
}
```

Rules:

- validate package path and decode limits before preview;
- never fetch external asset URLs during import/Perform;
- do not copy assets into a generated distributable package until rights are confirmed;
- allow local-use candidates with explicit non-redistributable provenance policy if product/legal policy accepts it;
- strip/ignore unsafe metadata and external SVG references;
- record sprite/source-rectangle relationships where relevant;
- do not convert executable or script assets.

## 13. Foreign action model

A parser produces a source-specific AST or opaque source record before semantic mapping.

Conceptual structure:

```json
{
  "id": "foreignAction:...",
  "language": "VDJScript",
  "languageVersion": "observed-or-unknown",
  "sourceTextHash": "sha256:...",
  "sourceText": "optional-bounded-owner-local",
  "ast": { ... },
  "referencedSourceEntities": [ ... ],
  "source": { ... }
}
```

The source AST may describe:

- command/action verb;
- query/state expression;
- parameters;
- sequence/conditional/repeat/wait structure;
- source page/panel/deck/context;
- display/feedback expression;
- unknown tokens.

No foreign source is passed to the Ember Actions runtime.

## 14. Semantic action mapping

Each adapter has a versioned mapping table:

```text
source construct
source preconditions/context
Ember command/state/action pattern
argument/target transform
semantic evidence
known differences
translation status
required manual review
```

Examples:

- source button action -> registered Ember command;
- source state query -> registered Ember state predicate;
- source page visibility -> view-local state/visibility;
- source fader value -> typed command + binding transform;
- source repeat/wait -> unsupported unless a matching bounded Ember domain plan exists;
- source device message -> separate controller-profile candidate, never skin device access.

Name similarity is insufficient. Mapping requires documented behavior and tested fixtures.

## 15. Target resolution

Foreign references may use names, indices, paths, deck numbers, or source-specific IDs. The IR preserves them and records candidate semantic targets.

Resolution states:

```text
ResolvedStableId
ResolvedSemanticRole
Ambiguous
Missing
UnsupportedTargetKind
ProjectDependent
OwnerConfirmationRequired
```

Rules:

- no silent nearest-name target;
- numeric source indices do not become Ember stable IDs;
- project-dependent candidates remain parameterized or unresolved until a project is selected;
- generated reusable skins prefer semantic context/roles over one project’s IDs;
- original source target remains available for repair diagnostics.

## 16. Feedback mapping

Source visual feedback normalizes independently of source actions.

Possible mappings:

```text
active/selected
queued
progress/value
on/off
available/disabled
connection/safety health
enum-to-style
text/icon/value formatting
```

Generated candidates read registered Ember state. They do not infer success from the action that was pressed.

## 17. Controller-binding separation

When a source package contains device/controller mappings:

- physical matchers/messages go to a controller-profile candidate;
- semantic action references may point to generated Ember Actions;
- skin controls retain visual bindings independently;
- machine-local serials/ports/paths are redacted by default;
- conflicts, unsupported feedback, and relative-encoder behavior are reported;
- changing skin does not remove the generated controller profile.

## 18. Opaque records

Unknown records are preserved only when bounded and lawful.

Each opaque record includes:

```text
source location
content hash
bounded raw content or external source reference
reason unknown
privacy classification
rights classification
adapter version
future-decoder namespace
```

Opaque preservation does not mean the generated skin can use the record.

## 19. Adapter interface

A source adapter implements:

```text
probe(sourceSet) -> source identity/candidate files
inventory(sourceSet) -> immutable source manifest
parse(manifest) -> Migration IR + parse diagnostics
validate(ir) -> structural/provenance/rights diagnostics
```

The shared translator implements:

```text
match(ir, current catalogs) -> semantic mapping candidates
plan(ir, options) -> translation plan/report
materialize(plan) -> candidate artifacts in separate output
validateCandidate(artifacts) -> normal Ember package validation
```

Adapter code does not call Runner, output adapters, MIDI devices, or project mutation APIs.

## 20. Adapter options

Common options:

```text
layoutMode: preserve | modernize | hybrid
assetPolicy: referenceOnly | copyConfirmed | omit
unknownPolicy: preserveEvidence | omitWithReport
bindingPolicy: skinOnly | includeControllerCandidate
conflictPolicy: stop | sideBySideCandidates
outputScope: fullSkin | overlay | selectedPage | themeCandidate
```

Defaults favor non-destructive side-by-side output and explicit manual review.

## 21. Generated artifacts

A migration may produce:

- candidate `.emberskin` source directory/archive;
- candidate `.emberoverlay`;
- controller-profile candidate;
- theme/component/action candidates;
- compatibility lock;
- source manifest;
- migration IR;
- human-readable and machine-readable reports;
- preview screenshots from deterministic mock state;
- package-local scenarios;
- unresolved-target/relink list;
- licensing/provenance manifest.

No candidate overwrites an installed skin or profile without explicit owner selection.

## 22. VirtualDJ initial adapter profile

The first research/implementation slice should be narrow and evidence-driven.

### 22.1 Inputs

User-supplied skin package containing observed combinations of:

- XML layout;
- image/sprite assets;
- panels/groups/decks/windows;
- buttons, sliders, text, visual states;
- VDJScript action/query strings;
- optional custom pad-page or mapping artifacts when separately supplied.

Exact supported versions/formats are recorded from corpus evidence rather than assumed globally.

### 22.2 Initial supported candidates

Prioritize:

- fixed geometry and source rectangles;
- group/panel/page hierarchy;
- button/toggle/slider/knob/text/image primitives;
- simple visibility/query expressions;
- simple one-verb actions with clear Ember equivalents;
- deck/context placeholders that can map to normalized DJ adapter state;
- page navigation and non-domain view actions;
- controller mappings as separate candidates where source format is documented.

### 22.3 Explicit initial exclusions

- arbitrary plugin/vendor extensions;
- undocumented binary payloads;
- general wait/repeat/timer behavior;
- dynamic script evaluation;
- actions with no Ember domain equivalent;
- asset redistribution without rights;
- pixel-perfect responsive claim;
- automatic conversion of DJ/audio content or lighting shows.

### 22.4 Qualification corpus

Use:

- synthetic legal fixtures for parser/security tests;
- user-owned samples stored outside Git with content-free manifests;
- explicitly licensed public samples where available;
- one controlled source change at a time;
- expected IR snapshots and translation reports;
- manual side-by-side visual/action verification.

## 23. SoundSwitch workflow integration

The SoundSwitch path uses the same migration-report language even though UI familiarity initially comes from the bundled Reference skin.

A SoundSwitch project migration report may recommend:

```text
Suggested skin: SoundSwitch Reference
Suggested Perform page: Static Looks / Autoloops
Mapped content: exact/translated/approximate/manual
Mapped MIDI actions: supported/unresolved
Unsupported source UI preference: not applicable/unknown
```

Show-content migration remains governed by the separate source-manifest/migration-IR contracts for fixtures, Looks, Autoloops, Track Scripts, audio identity, and unknown payloads.

## 24. Security and resource limits

Adapters and IR enforce:

- package/source byte, file-count, path-length, nesting, XML/JSON depth, string, node, asset, and expression limits;
- no entity expansion/external XML resources;
- no archive path traversal, symlinks, reparse points, absolute/UNC/drive escape, or case-collision ambiguity;
- no execution of scripts/macros/plugins;
- no external URL fetching;
- no source-path disclosure in normal reports;
- bounded logs and opaque content;
- cancellation and partial-output cleanup;
- separate source/output/cache directories;
- deterministic failures and correlation IDs.

A parser crash or malformed input cannot affect active Perform/Runner output.

## 25. Versioning

Version independently:

- source adapter;
- source grammar profile;
- Migration IR schema;
- semantic mapping table;
- Ember command/state/component catalogs used;
- materializer/generator;
- report schema.

Re-running the same adapter/generator/catalog versions on identical source bytes/options must produce identical IR, plan, report, and candidate content hashes, excluding explicitly non-content timestamps.

## 26. Testing

### Parser/security

- malformed XML/JSON/archive;
- entity/expansion and compression bombs;
- deep nesting/huge strings/file fan-out;
- path traversal/collision/symlink/reparse;
- invalid Unicode/encodings;
- missing/duplicate/cyclic IDs;
- oversized/hostile images/SVG;
- unknown extension records;
- cancellation/out-of-memory simulation.

### Determinism/provenance

- source-byte immutability;
- stable manifest/aggregate hash;
- stable IR ordering/IDs;
- exact source-location traceability;
- no private rows/paths/assets in public fixtures;
- separate output directories.

### Semantic mapping

- known primitive/action/state mappings;
- unsupported timer/repeat behavior;
- ambiguous/missing targets;
- controller/skin separation;
- feedback from state rather than action inference;
- exact translation-status and confidence dimensions;
- catalog-version compatibility.

### Candidate package

- normal `.emberskin` validation;
- mandatory safety reachability;
- fixed-canvas and responsive previews;
- accessibility/contrast/focus findings;
- import side-by-side/update/fork/reset;
- invalid candidate leaves installed/current skin unchanged;
- Perform/DMX continuity.

## 27. Acceptance

The Migration IR foundation is complete when:

1. Every source file is inventoried and immutable.
2. Parsed items retain exact provenance and separate confidence dimensions.
3. Layout, controls, styles, assets, actions, feedback, mappings, and unknowns have normalized bounded representations.
4. Exact/translated/approximated/manual/opaque/unsupported/unsafe/conflict statuses are machine-readable and visible.
5. A source adapter cannot execute code, access devices, mutate projects, or enter Perform.
6. Target ambiguity never redirects silently.
7. Generated artifacts are side-by-side candidates with complete compatibility/licensing reports.
8. Identical input/version/options produce deterministic IR and output hashes.
9. A narrow VirtualDJ corpus produces useful fixed-canvas candidates and explicit action gaps without overclaiming.
10. The architecture admits future adapters without changing the `.emberskin` runtime contract.