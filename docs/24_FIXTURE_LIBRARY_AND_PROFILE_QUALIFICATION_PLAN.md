# EmberLights Fixture Library and Profile Qualification Plan

Status: **Binding companion to the active core recovery program**  
Date: 2026-08-11  
Governing plan: `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`  
Primary objective: turn fixture/profile uncertainty into explicit evidence, reproducible raw-channel tests, and portable qualified profiles without making SoundSwitch, a cloud service, or one user's rig a runtime dependency.

## 1. Immediate decision

The current 71-fixture SoundSwitch migration project is a staging project, not a physically decoded rig. Its profiles and addresses must not be used as proof that the SoundSwitch Micro transport is broken, and successful USB writes must not be used as proof that the profiles are correct.

The recovery program therefore separates three independent facts:

1. **Transport truth** — the adapter emits a reproducible DMX universe.
2. **Profile truth** — one exact fixture model and DMX mode maps semantic properties to the correct raw slots and ranges.
3. **Patch truth** — a physical unit is on the selected universe, address, and mode.

A fixture may be internally schema-valid while all three physical facts remain unverified.

## 2. New SoundSwitch Fixture Manager finding

SoundSwitch's official Fixture Manager provides a supported file exchange path:

- `File > Open Fixture…` loads an individual fixture personality from disk.
- `File > Save Fixture as…` saves the selected fixture personality to disk.
- Production, Public, and Local profiles can be placed in the Fixture Manager workspace.
- Production profiles are created by SoundSwitch; Public profiles are user-contributed; Local profiles are owned workspace files.
- SoundSwitch explicitly states that some Production personalities originated from third parties and cannot have all content read or shared because of licensing.
- SoundSwitch does not guarantee Public profiles and recommends matching profile DMX data against the complete manufacturer manual.

This establishes the preferred integration boundary:

> EmberLights imports user-selected, user-exported fixture personality files and preserves their provenance. EmberLights does not scrape the SoundSwitch cloud, capture account credentials, bypass Fixture Manager restrictions, or redistribute profiles whose license is unknown.

Official references:

- https://support.soundswitch.com/en/support/solutions/articles/69000844505-soundswitch-fixture-manager-complete-user-guide
- https://support.soundswitch.com/en/support/solutions/articles/69000848673-soundswitch-how-to-locate-fixture-profiles-in-the-soundswitch-dmx-library
- https://cdn.inmusicbrands.com//soundswitch/FixtureManager/files/Fixture_Manager_Manual.pdf

## 3. Source-of-truth hierarchy

No machine-readable profile outranks the physical manufacturer's DMX documentation. Use sources in this order and retain every source used:

1. **Exact manufacturer manual for the exact model/revision** — normative channel, mode, range, default, and safety evidence.
2. **Manufacturer-provided machine profile** — such as an official QXF, OFL contribution, ShowXpress profile, or other vendor download, when licensing permits inspection and conversion.
3. **SoundSwitch Production personality exported through Fixture Manager** — strong corroborating evidence, subject to export/readability and license restrictions.
4. **SoundSwitch Local personality exported by the operator** — useful when created from the exact manual; provenance must identify the creator and source manual.
5. **SoundSwitch Public personality exported through Fixture Manager** — untrusted until manual-matched and bench-qualified.
6. **QLC+/Open Fixture Library profile** — useful, searchable, and importable through the existing quarantine boundary, but still manual- and bench-qualified.
7. **Controlled channel discovery** — last resort when documentation/profile evidence is absent or contradictory.

When sources disagree, do not silently choose one. Record the contradiction, select a bounded test hypothesis, and resolve it through raw DMX observation.

## 4. Immediate fixture strategy for the morning build

The next installer does not need a complete global fixture library to prove the core. It needs one exact, safe bench profile and a path that scales.

### 4.1 Select one bench fixture

Choose the physically easiest owned fixture for which we can obtain all of the following:

- exact manufacturer and model;
- exact firmware/hardware revision when shown;
- exact selected DMX mode and channel count;
- exact physical DMX start address;
- complete DMX chart from the manual or an exportable profile;
- at least one safe, visible non-strobe color/intensity test.

Preferred candidates:

- **CHAUVET DJ Wash FX Hex** in a simple documented mode. CHAUVET publishes the manual and a ShowXpress profile and lists 3-, 6-, 11-, and 40-channel modes.
- **Both Lighting BO-IR4 LED Mini Spotlight** when the exact manual or SoundSwitch Production/Local personality can be exported.

The final choice is operational, not architectural. The tooling must work with any fixture.

### 4.2 Build a one-fixture project

The bench project contains only:

- one exact normalized profile revision;
- one fixture instance on universe 1 at the operator-confirmed address;
- Micro universe 1 enabled;
- other outputs disabled unless explicitly needed;
- Blackout;
- safe direct looks for supported Red, Green, Blue, White, and Intensity properties;
- no dependency on OS2L, Autoloops, migration data, or the 71-fixture staging project.

### 4.3 Compare raw and semantic output

For every successful visible state:

1. Send explicit raw channel/value pairs from Hardware Test.
2. Record the full 512-slot frame hash and nonzero channel/value pairs.
3. Trigger the equivalent semantic Look through Runner.
4. Compare the two universe frames byte-for-byte.
5. Explain and approve every difference.

This is the acceptance bridge between hardware transport and the fixture compiler.

## 5. Qualification state model

Profile quality and patch quality are different and must be stored separately.

### 5.1 Profile qualification

Recommended monotonic states:

```text
Discovered
Preserved
Parsed
SchemaValid
ManualMatched
RawBenchConfirmed
RunnerMatched
Qualified
Rejected
```

Definitions:

- `Discovered` — source exists but has not been preserved or parsed.
- `Preserved` — original bytes and SHA-256 are stored without modification.
- `Parsed` — a versioned importer recognized the source structure.
- `SchemaValid` — normalized profile passes structural validation.
- `ManualMatched` — model, modes, channel offsets, ranges, defaults, cells, and safety functions were checked against an exact manual.
- `RawBenchConfirmed` — explicit raw channel tests produced expected physical behavior.
- `RunnerMatched` — semantic Runner output matched the successful raw frame.
- `Qualified` — reconnect, blackout, and bounded soak evidence passed for this profile/mode on owned hardware.
- `Rejected` — source is wrong, contradictory, malformed, unsafe, or unsupported.

A state transition records who/what made the assertion, the date, application/parser version, evidence hashes, and notes. Do not represent this as a single editable boolean.

### 5.2 Patch-instance qualification

A fixture instance needs separate evidence:

```text
UniverseUnverified / UniverseConfirmed
AddressUnverified  / AddressConfirmed
ModeUnverified     / ModeConfirmed
SignalUnverified   / SignalConfirmed
RigUnqualified     / RigQualified
```

Changing a physical address or mode invalidates the relevant instance evidence but does not invalidate a profile whose channel map remains unchanged.

### 5.3 Project readiness

A project is not `PhysicalReady` merely because it validates. Recommended aggregate readiness:

```text
SchemaValid
OutputConfigured
ContainsUnverifiedProfiles
ContainsUnverifiedPatch
BenchReady
PhysicalReady
GigQualified
```

The migrated color-rig project must report at least:

```text
MIGRATED_PATCH_UNVERIFIED:
This project uses a non-overlapping staging patch and provisional profiles.
Verify every physical fixture's universe, address, DMX mode, and profile before relying on output.
```

## 6. Provenance and evidence model

Every imported or authored profile revision needs an immutable evidence record.

Recommended fields:

```text
profileId
profileRevision
manufacturer
model
modeName
footprint
sourceKind
sourceDisplayName
sourceUriOrReference
sourceFileName
sourceSha256
sourceSizeBytes
sourceModifiedTimeHint
sourceLicenseStatus
sourceTrustClass
importerId
importerVersion
importedAt
manualFileName
manualSha256
manualRevision
manualPageReferences
normalizationDecisionLog
qualificationState
qualificationEvidence[]
```

`sourceLicenseStatus` should distinguish at least:

```text
KnownRedistributable
KnownPrivateUseOnly
UnknownDoNotRedistribute
MetadataOnly
```

Source artifacts with unknown or restrictive licensing may be used locally when lawful but must not be committed to this repository, embedded in public installers, uploaded to EmberLights services, or redistributed to other users.

## 7. Content-addressed local fixture library

Use a local, offline-first library distinct from project files.

Recommended Windows root:

```text
%LOCALAPPDATA%\EmberLights\FixtureLibrary\
```

Recommended layout:

```text
objects/<sha256>                         immutable source artifacts
profiles/<profile-id>/<revision>.json   normalized EmberLights profiles
reports/<sha256>.json                   probe/import/validation reports
index/fixture-index.json                regenerated searchable index
quarantine/                             rejected or unsupported inputs
```

Rules:

- Source artifacts are immutable and content-addressed.
- Normalized revisions are immutable; an edit creates a new revision.
- The search index is disposable and rebuilt from canonical records.
- Projects embed a frozen normalized profile snapshot or exact content identity so a gig does not depend on a mutable global library.
- A project can later relink to a newer profile revision only through an explicit migration with diff preview.
- Runtime/Runner never needs SoundSwitch, Fixture Manager, internet access, or cloud credentials.

## 8. SoundSwitch personality import program

### Phase SS-F0 — operator export workflow

Document and surface this supported workflow:

1. Open SoundSwitch Fixture Manager.
2. Search Production, Public, and Local for the exact manufacturer/model/mode.
3. Add the fixture to the Workspace.
4. Inspect the configured DMX data and compare it with the complete manual.
5. Use `File > Save Fixture as…` to save a user-selected personality file.
6. Import that file into EmberLights.

For profiles that Fixture Manager will not expose because of licensing, retain metadata and use the manufacturer manual or another lawful profile source. Do not bypass the restriction.

### Phase SS-F1 — bounded file probe

Add a read-only `emberlights_fixture_probe` utility or equivalent Studio action.

It must:

- accept only an explicitly selected file or directory;
- default to no network access;
- read with a bounded maximum size;
- calculate SHA-256 before parsing;
- detect common containers and encodings without trusting extensions;
- report magic bytes, text/binary structure, archive entries, schemas, strings, and version markers;
- reject path traversal, external entities, decompression bombs, recursion bombs, and oversized structures;
- never modify the source;
- produce a machine-readable report;
- preserve unrecognized bytes losslessly when the user elects to retain the artifact.

The first build may support probe/preserve before semantic import if no representative exported fixture file is available in the repository.

### Phase SS-F2 — versioned personality parser

Only add a semantic parser after representative exported files establish a stable, bounded format.

Parser contract:

```text
source bytes
  -> container/version recognition
  -> lossless parsed source model
  -> normalized fixture candidate
  -> validation issues and unsupported constructs
  -> operator-reviewed import
```

Rules:

- Unknown versions fail closed into quarantine.
- Unknown fields are preserved, not discarded.
- No parser guesses channel offsets from names alone.
- Multiple modes and cells remain first-class.
- The parser emits provenance and a decision log.
- A parsed profile remains unqualified until manual and bench gates pass.

### Phase SS-F3 — optional assisted comparison

After the importer exists, provide a side-by-side comparison of:

- SoundSwitch-exported personality;
- exact manual chart;
- existing QXF/OFL/vendor profile;
- proposed EmberLights normalization.

The comparison should highlight footprint, mode names, channel numbers, coarse/fine pairs, ranges, wheels, master cell semantics, defaults, and unsupported attributes.

## 9. Normalized profile schema growth

The current `FixtureProfileDefinition` is adequate for simple linear channels but cannot faithfully model the full fixture domain described by SoundSwitch's Fixture Manager or typical manuals.

Add capabilities incrementally without blocking the raw Micro proof.

### 9.1 Required model additions

- **Multiple modes per fixture family** rather than treating each mode as unrelated metadata.
- **Cells/emitters**, including pixel geometry and a distinct master cell.
- **Channel functions and DMX ranges** rather than one property with only a global min/max.
- **Coarse/fine pairs** with explicit byte order and resolution.
- **Default, home, highlight, blackout, open, and safe values** where applicable.
- **Wheels and indexed functions** for color, gobo, prism, animation, and macro channels.
- **Shutter/strobe ranges** that distinguish closed, open, random, pulse, and rate ranges.
- **Master dimmer/strobe** and fixture-wide controls.
- **Raw/unsupported attributes** retained with names and range tables instead of discarded.
- **Physical capability flags** such as RGB, RGBW, RGBAW+UV, pan/tilt, multi-cell, effect fixture, fog/haze, laser, spark, and media-server controls.
- **Safety classification** for hazardous or disruptive functions.
- **Provenance and qualification metadata** attached to every revision.

### 9.2 Range model

Recommended concept:

```text
ChannelDefinition
  offset
  fineOffset?
  resolution
  defaultValue
  functions[]

ChannelFunction
  semanticProperty?
  sourceName
  dmxStart
  dmxEnd
  physicalStart?
  physicalEnd?
  unit?
  snapOrContinuous
  safetyClass
  recommendedTestValue?
```

A fixture's channel may contain several mutually exclusive functions. Mapping the entire byte as one linear semantic property is not acceptable for shutter, program, wheel, macro, speed, or effect channels.

### 9.3 Migration safety

If a format-version bump would delay the P0 recovery, first store qualification/provenance as a versioned sidecar or forward-compatible project record. Do not delay Hardware Test and one-fixture proof for a full schema rewrite. Promote the sidecar into the canonical schema in the next bounded migration.

## 10. Controlled channel discovery

Use channel discovery only when the exact manual/profile is unavailable or contradictory.

Required behavior:

- one fixture connected or isolated when practical;
- explicit start address and bounded footprint;
- one channel changed at a time;
- visible current channel/value;
- configurable safe value set;
- automatic timeout and blackout;
- no automatic full-range sweep for fog, haze, laser emission, spark, motor reset, calibration, or high-rate strobe channels;
- operator annotations for observed behavior;
- report every tested value and observation;
- convert observations into a candidate profile only through review.

Discovery evidence is not a substitute for a manual when safety-critical functions exist.

## 11. Search, aliases, and duplicate handling

Fixture search must normalize common naming variance while retaining exact identity.

Index keys should include:

- manufacturer canonical name and aliases;
- model canonical name, punctuation-insensitive form, and known aliases;
- mode name and channel count;
- fixture type and capabilities;
- source kind and qualification state;
- source/profile hashes.

Deduplication must not merge solely by display name. Two files with the same manufacturer/model may represent different firmware revisions, channel modes, or incorrect community profiles.

## 12. Profile editing and revision rules

- Imported profiles are not edited in place.
- An operator edit forks a new Local EmberLights revision with a parent identity.
- The UI shows a structural diff before replacing a project profile.
- A changed channel map invalidates prior `RawBenchConfirmed`, `RunnerMatched`, and `Qualified` evidence for that revision.
- Cosmetic alias changes may retain physical evidence only when the normalized channel model hash is unchanged.
- Every project save retains the exact profile revision used to compile its package.

## 13. Fixture validation layers

Validation should report separate categories:

### Structural

- footprint bounds;
- unique offsets;
- coarse/fine validity;
- range bounds and overlap rules;
- cell references;
- required fields;
- mode footprint consistency.

### Semantic

- duplicate conflicting properties;
- missing intensity/shutter assumptions;
- impossible or ambiguous ranges;
- unsafe functions without safety class;
- multi-cell/master inconsistencies;
- unknown attributes preserved but unsupported.

### Evidence

- missing manual;
- unknown source license;
- source hash missing;
- manual mismatch;
- unqualified profile;
- unverified patch address/mode/universe.

### Runtime

- rendered slots outside footprint;
- default/home values not represented;
- semantic command has no supported channel function;
- layer output is being suppressed by a higher priority source;
- selected adapter universe does not contain the fixture.

## 14. Frame-inspector integration

The frame inspector must resolve every displayed nonzero slot back through this evidence chain:

```text
Universe/channel/value
  -> fixture instance and physical address
  -> profile revision and mode
  -> channel definition/function range
  -> semantic property or raw constant
  -> winning layer/source
  -> qualification state
```

For unowned slots, report `Unpatched`. For provisional profiles, display `Unverified profile mapping`. For staged migration addresses, display `Unverified patch address`.

## 15. Automated tests and fixture corpus

Create a legally distributable test corpus containing synthetic fixtures plus profiles whose license permits repository inclusion.

Minimum cases:

- simple RGB 3-channel;
- dimmer + RGB/RGBW;
- RGBWA+UV;
- 8/16-bit pan and tilt;
- shutter with closed/open/strobe ranges;
- color wheel;
- custom macro channel;
- multi-cell fixture with master cell;
- duplicate channel conflict;
- invalid fine offset;
- overlapping ranges;
- unsupported source version;
- zip/path traversal/decompression-bomb probes;
- source artifact with unknown license;
- same model name with different modes/revisions.

Golden tests must cover normalized output, provenance hashes, validation warnings, project snapshot stability, and raw-to-semantic frame equivalence.

## 16. Acceptance gates

### Gate FXL-1 — fixture source truth

- User can export an individual personality through Fixture Manager and select it in EmberLights.
- EmberLights preserves source bytes and SHA-256 without modification.
- Unsupported formats produce a useful probe report and remain quarantined.
- No SoundSwitch login, cloud scraping, or credential capture exists in EmberLights.

### Gate FXL-2 — one exact profile

- Exact model/mode/manual evidence is recorded.
- Channel offsets/ranges/defaults/cells match the manual.
- Profile validation has zero errors and no undisclosed unsupported constructs.
- Qualification remains visibly incomplete until physical testing.

### Gate FXL-3 — raw bench

- A known safe raw frame produces expected physical output.
- Automatic blackout works.
- The report records the successful exact channel/value pairs.

### Gate FXL-4 — Runner equivalence

- A semantic Look emits the same approved raw frame.
- Frame inspector identifies the profile, channel function, layer, and qualification state.
- Clearing the state returns safely to blackout or the intended lower layer.

### Gate FXL-5 — rig qualification

- Physical universe/address/mode are confirmed for each fixture instance.
- Reconnect and ten-minute output pass.
- Project has no undisclosed unverified profiles or patch instances.
- Only then may the project be labeled `PhysicalReady`; longer event qualification still gates `GigQualified`.

## 17. Overnight implementation order

The core build agent should integrate this plan into the existing recovery slices without expanding scope prematurely:

1. Finish the shared Micro session and raw Hardware Test first.
2. Add migrated-profile/patch warnings and frame truth.
3. Add the content-addressed artifact/provenance skeleton.
4. Add the SoundSwitch exported-file probe and preserve path.
5. Create one exact bench profile from lawful/manual evidence.
6. Compare raw and semantic frames.
7. Defer broad fixture-cloud ingestion and a polished library browser until the physical core gate passes.

## 18. Required morning evidence

The next installer should allow Joshua to provide or record:

- exact fixture model;
- physical display mode/channel count;
- physical start address;
- exported SoundSwitch personality file when available;
- complete manufacturer manual or vendor profile;
- Hardware Test report;
- successful raw channel/value pairs;
- Runner frame comparison;
- reconnect/blackout observation.

These inputs should be captured by the application/report rather than buried in chat.

## 19. Non-goals and prohibitions

- Do not mirror or scrape SoundSwitch's Production/Public servers.
- Do not ask for or store inMusic credentials.
- Do not redistribute unknown-license SoundSwitch personalities.
- Do not treat Public profiles as verified.
- Do not treat `Check Fixture` or EmberLights schema validation as physical proof.
- Do not infer complex channel ranges from names alone.
- Do not merge fixture identity by display name alone.
- Do not couple the Runtime to an online fixture library.
- Do not delay raw Micro proof for a complete fixture-editor redesign.
- Do not destructively modify original user exports or manuals.

## 20. Product direction after the recovery

Once the core is physically qualified, EmberLights can grow this foundation into:

- searchable offline fixture library;
- importer adapters for SoundSwitch exports, QXF/OFL, manufacturer profiles, and future formats;
- guided profile creation and validation;
- manual/profile diffing;
- controlled channel discovery;
- community profile sharing with signatures, provenance, moderation, and reproducible qualification evidence;
- portable semantic projects that embed their exact profile revisions.

The long-term advantage is not merely having more fixture files. It is knowing exactly **where each profile came from, what it claims, what was physically tested, and which projects depend on it**.