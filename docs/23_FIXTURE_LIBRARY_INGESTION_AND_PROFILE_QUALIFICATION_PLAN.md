# EmberLights Fixture Library Ingestion and Hardware Qualification Plan

Status: **Binding supporting plan for the active core-recovery program**  
Date: 2026-08-11  
Primary objective: remove fixture-profile, DMX-mode, and patch uncertainty without confusing library ingestion with physical hardware proof.  
Authority: execute together with `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`; where scope conflicts, the raw-output and safety gates in that plan win.

Implementation note (2026-08-13): the native project/compiler now retains bounded non-overlapping named capability ranges, channel owner/head/cell labels, blackout/highlight, safety access, role, and direction; the QXF/OFL bridge imports compatible rows as unreviewed snapshots. See `39_MULTI_CAPABILITY_FIXTURE_CHANNEL_CHECKPOINT.md`. Qualification evidence, pinned corpus, grouped cell/head realization, and hardware proof in this plan remain open.

## 1. Immediate decision

SoundSwitch's Fixture Manager and fixture library are useful evidence sources, but they do not replace controlled hardware qualification.

The immediate implementation path is:

1. Prove one raw DMX channel/value through the SoundSwitch Micro independently of any project or fixture profile.
2. Select exactly one physical fixture with a complete manual, confirmed DMX mode, confirmed universe, and confirmed address.
3. obtain an exact profile candidate from a trusted source;
4. normalize that candidate into EmberLights' stable native profile model;
5. compare the candidate line-by-line with the manufacturer's DMX chart;
6. reproduce the known-working raw frame through Runner;
7. freeze the verified native profile and qualification evidence inside the project;
8. only then scale to the rest of the rig and a searchable fixture catalog.

A fixture-library match, successful import, SoundSwitch `Check Fixture` result, or EmberLights schema validation is not physical proof. A profile becomes **Hardware Qualified** only after raw and semantic output both activate the expected real fixture behavior and blackout is confirmed.

## 2. Research conclusions

### 2.1 SoundSwitch Fixture Manager is a legitimate targeted interoperability source

The official Fixture Manager can:

- browse fixture personalities;
- expose Production, Public, and Local library categories;
- show manufacturer, model, modes, cells, attributes, coarse/fine offsets, and ranges;
- open a personality from a file;
- save the selected personality to a file;
- verify a selected personality with **Fixtures > Check Fixture**;
- create a local personality from a manufacturer's DMX chart.

That makes it useful for exact profile lookup, manual cross-checking, and operator-authorized export of a specific selected personality.

It is not an acceptable source for wholesale server scraping or redistributing the SoundSwitch Production library. SoundSwitch explicitly states that some Production personalities originated with third parties and their licensing does not permit all personality contents to be shared. SoundSwitch also states that Public personalities are user-created and are not guaranteed to function correctly.

Therefore:

- EmberLights may ingest a specific file that Joshua explicitly exports or creates locally;
- EmberLights may use an inspected SoundSwitch profile as comparison evidence;
- EmberLights must preserve its origin and hash;
- EmberLights must not automate account access, crawl the cloud library, bypass access controls, or package protected Production personalities;
- a SoundSwitch Production match is useful evidence, not redistribution permission;
- a SoundSwitch Public match enters quarantine until manual and bench verification.

### 2.2 Open Fixture Library is the preferred redistributable seed source

Open Fixture Library (OFL) is an open-source, MIT-licensed fixture-definition project with fixture JSON, schemas, import/export plugins, tests, and multiple output formats. It is the preferred source for building an EmberLights catalog that can be legally pinned, transformed, tested, and redistributed with required notices.

OFL explicitly warns that its internal JSON format is not intended to be consumed directly by unrelated applications because breaking changes can occur. It recommends transforming the data through a plugin into an application-specific stable format.

Therefore EmberLights must:

- pin an exact OFL commit and schema version for each library build;
- transform OFL data in Studio/build tooling, never in Runner;
- store the resulting EmberLights-native profile snapshot in the project;
- record the OFL fixture key, source commit, source-content hash, adapter version, and resulting native-profile hash;
- never let an OFL update silently mutate an active show.

### 2.3 The existing QLC+ importer is the fastest safe bridge

EmberLights already has a bounded Studio-only QLC+ `.qxf` importer with:

- source hashing and provenance;
- per-mode native profile creation;
- coarse/fine mapping;
- common semantic groups and presets;
- safe handling for ordinary ranged/hazardous activation channels;
- quarantine for switching channels and ambiguous features;
- XML entity, size, nesting, and malformed-input protections;
- an explicit rule that import success is not physical proof.

The fastest first fixture-library path is therefore:

```text
OFL fixture/mode
  -> OFL QLC+ export
  -> existing EmberLights QXF importer
  -> manual comparison
  -> raw bench proof
  -> Runner byte comparison
  -> frozen qualified native profile
```

A direct OFL adapter is valuable later for richer cells, matrices, wheels, switching channels, capability ranges, and update metadata, but it must not delay the current Micro/one-fixture proof.

## 3. Non-negotiable architecture boundaries

1. **Runner remains library-free.** Runner consumes only compiled, bounded EmberLights profiles already contained in the activated show package.
2. **No network in live output.** Fixture discovery, download, parsing, and update checks are Studio/tooling functions.
3. **Profiles are immutable project snapshots.** An active project never points to a mutable cloud entry.
4. **Source trust and hardware qualification are separate axes.** An official-looking source can still be the wrong mode or address; a locally authored profile can become fully qualified after evidence.
5. **Adapter proof and fixture proof are separate.** Raw Micro output must work before fixture semantics are blamed or trusted.
6. **Unknown semantics are quarantined, not guessed.** A footprint match alone is insufficient.
7. **Hazardous outputs fail safe.** Fog, haze, laser, spark, and strobe ranges are not exercised by automatic discovery.
8. **Updates are opt-in diffs.** A new library version creates a reviewable candidate; it never overwrites a project profile in place.
9. **Original source files are preserved read-only.** Imported files receive hashes and are never destructively rewritten.
10. **Licensing follows the source.** Only content with a redistribution basis may be bundled into the product library.

## 4. Source classification and trust

Source classification describes provenance, not whether a physical fixture has passed.

| Source class | Default trust | Bundling policy | Required review |
| --- | --- | --- | --- |
| Manufacturer manual / DMX chart | Highest documentary authority | Store reference hash/locator; redistribute only when permitted | Exact mode, footprint, offsets, ranges, defaults, and safety behavior |
| EmberLights built-in, manually authored from a manual | High after review | May bundle when source rights permit and attribution is recorded | Full fixture tests and profile regression corpus |
| SoundSwitch Production reference | Useful official comparison evidence | Do not bulk copy or redistribute protected personality content | Manual match plus independent native authoring/import and bench proof |
| Open Fixture Library pinned source | Strong open seed, not automatically physical truth | May bundle under MIT terms and notices | Adapter conversion report, manual match, bench proof |
| QLC+ `.qxf` / OFL QLC+ export | Useful compatibility source | Preserve upstream provenance/license | Import report, manual match, bench proof |
| SoundSwitch Public personality | Community candidate; SoundSwitch does not guarantee it | Do not bundle without confirmed rights | Full manual and hardware qualification |
| SoundSwitch Local / operator-exported personality | User-authorized local evidence | Keep private unless author grants redistribution | Format parse, manual match, bench proof |
| EmberLights local profile | Unknown until reviewed | Local by default | Manual and bench proof |
| SoundSwitch migration inference | Low; staging only | Preserve with source project, never claim verified | Exact patch/mode/profile reconstruction and bench proof |
| Raw channel discovery | Temporary empirical evidence | Store report, not a reusable profile by itself | Convert findings to a documented profile and re-test through Runner |

## 5. Qualification state model

Every profile and patched fixture must expose an explicit state. Do not overload `source` or `validation passed` to imply readiness.

### 5.1 Profile qualification states

Conceptual state machine:

```text
ImportedUnreviewed
  -> SchemaValid
  -> ManualMatched
  -> RawBenchMatched
  -> RunnerMatched
  -> HardwareQualified
```

Terminal/side states:

```text
Quarantined
Rejected
Superseded
```

Definitions:

- **ImportedUnreviewed:** bytes were accepted and provenance was captured.
- **SchemaValid:** the normalized native profile is internally consistent.
- **ManualMatched:** exact manufacturer/model/mode/footprint/channel layout and relevant ranges were compared with the manual.
- **RawBenchMatched:** a successful raw Hardware Test frame has been mapped to the expected channel semantics.
- **RunnerMatched:** Runner emits the expected universe bytes for the equivalent semantic command/look.
- **HardwareQualified:** physical response, blackout, close/reopen, and repeat behavior passed on the identified fixture/mode.
- **Quarantined:** unsupported or ambiguous data is retained but cannot be activated as production-ready.
- **Rejected:** evidence proves the candidate is incorrect or unsafe.
- **Superseded:** a newer qualified native snapshot replaces this snapshot for future opt-in use; existing projects retain the old version.

### 5.2 Patch qualification is per fixture instance

A qualified profile does not prove the patched instance is correct. Each `FixtureDefinition` also needs evidence for:

- physical fixture identity/name;
- selected profile hash;
- universe;
- start address;
- physical DMX mode;
- address verified;
- mode verified;
- universe/output path verified;
- last hardware qualification result;
- optional rig/location notes.

A fixture patched with a qualified profile but an unverified address must still warn.

### 5.3 Transport qualification is separate

The SoundSwitch Micro session retains its own qualification evidence:

- software open/initialized;
- frame writes accepted;
- raw physical response operator-confirmed;
- reconnect/blackout qualified.

Profile qualification must reference a transport test report ID/hash rather than inferring transport success from counters.

## 6. Native data-model additions

The existing `FixtureProfileDefinition` is a good runtime-facing snapshot but does not yet carry enough evidence. Extend the Studio/project model without bloating `showcore::FixtureProfile` or the real-time render path.

### 6.1 Preserve the compact runtime profile

`showcore::FixtureProfile` and `ChannelMapping` remain bounded compiled structures. Rich source files, manuals, URLs, screenshots, notes, and qualification history stay outside the real-time structures.

### 6.2 Add profile evidence

Add an append-only project record/model conceptually equivalent to:

```text
FixtureProfileEvidenceDefinition
  profile_id
  native_profile_sha256
  source_kind
  source_locator
  source_revision
  source_content_sha256
  source_license
  original_file_name
  importer_id
  importer_version
  imported_at_utc
  manufacturer_manual_locator
  manufacturer_manual_sha256
  manual_mode_name
  notes
```

The native-profile hash must deterministically cover manufacturer, model, mode, footprint, every channel mapping, defaults, ranges, fine relationships, and safety-relevant flags.

### 6.3 Add profile qualification evidence

Conceptually:

```text
FixtureProfileQualificationDefinition
  profile_id
  native_profile_sha256
  state
  manual_checked
  channel_map_checked
  defaults_checked
  hazard_ranges_checked
  raw_test_report_sha256
  runner_frame_sha256
  qualified_fixture_description
  qualified_at_utc
  qualified_by
  notes
```

Qualification becomes invalid/stale when the native-profile hash changes.

### 6.4 Add fixture-instance qualification

Conceptually:

```text
FixturePatchQualificationDefinition
  fixture_id
  profile_id
  native_profile_sha256
  universe
  address
  physical_mode
  address_verified
  mode_verified
  output_path_verified
  hardware_response_verified
  blackout_verified
  verified_at_utc
  notes
```

Changing profile, universe, address, or physical mode invalidates the applicable qualification flags.

### 6.5 Source enum growth

Append source values rather than reinterpreting existing ones. Recommended additions include:

```text
ManufacturerManual
SoundSwitchProductionReference
SoundSwitchPublic
SoundSwitchLocal
HardwareDiscovery
```

A profile independently authored from a manufacturer manual while merely cross-checked against SoundSwitch should remain `ManufacturerManual`; SoundSwitch is recorded as secondary evidence, not falsely presented as the content's redistributable source.

### 6.6 Project-file compatibility

Prefer optional append-only records such as:

```text
PROFILE_EVIDENCE
PROFILE_QUALIFICATION
FIXTURE_QUALIFICATION
```

Maintain the repository's existing unknown-record round-trip behavior. Older builds may ignore qualification records but must not destroy them when round-tripping if their compatibility contract permits opening the file. New builds must treat missing evidence as unverified, never as implicitly qualified.

Use a project-format bump only if append-only compatibility cannot be made truthful and testable.

## 7. Fixture-source adapter contract

All fixture sources normalize through one Studio-only interface. No source-specific behavior enters Runner.

Conceptual contract:

```text
probe(bytes, filename) -> confidence / format
inspect(bytes) -> source metadata, fixtures, modes, warnings
convert(source fixture, selected mode) -> native profile candidate + conversion report
preserve(bytes) -> content-addressed source snapshot
```

Every adapter must provide:

- stable adapter ID and version;
- maximum input size and parser limits;
- source hash;
- selected fixture/mode identity;
- exact conversion warnings and unsupported semantics;
- deterministic output for identical input and adapter version;
- tests with valid, malformed, oversized, and ambiguous fixtures.

### 7.1 Existing QLC+ adapter

Keep it as the immediate production path. Extend its report and evidence persistence rather than replacing it.

Required additions:

- write the source SHA-256 and importer version into profile evidence;
- emit an explicit lossy/quarantine code for cells, switching channels, aliases, unsupported ranges, and custom lanes;
- generate a manual-review checklist for each imported mode;
- allow creation of a one-fixture bench project from one selected imported mode;
- never mark an imported mode Hardware Qualified automatically.

### 7.2 OFL adapter strategy

#### Immediate path

Use OFL's QLC+ export with the existing QXF adapter. Record both OFL provenance and QXF conversion provenance when available.

#### Direct adapter follow-up

Build a version-pinned OFL transformer when richer semantics are needed. Pin:

- OFL repository commit;
- OFL schema version;
- selected fixture key and source file hash;
- EmberLights adapter version.

The direct adapter should eventually cover:

- modes and channel order;
- defaults and highlights;
- fine channels;
- capability ranges;
- HTP/LTP precedence where relevant;
- matrices/cells/pixels;
- switching channels;
- wheels/gobos;
- RDM metadata;
- physical dimensions and categories where useful to Studio.

Unsupported data remains in the conversion report and source snapshot. Do not flatten multi-cell fixtures silently.

### 7.3 SoundSwitch Fixture Manager file adapter

The first step is evidence capture, not speculative parsing.

1. Joshua selects or creates one exact personality in Fixture Manager.
2. Use **Save Fixture as…** where the application permits.
3. Preserve the file unchanged and calculate SHA-256.
4. Record Fixture Manager version, library category, manufacturer/model/mode, and whether the profile was Production, Public, or Local.
5. Inspect the file format in an isolated parser/probe.
6. If the format is safely parseable, create a bounded import adapter with golden sample tests.
7. If the file is encrypted, access-controlled, or not safely interpretable, do not bypass protections. Use its visible channel table as comparison evidence and author/import the native profile from the manufacturer manual or an open source.

Never:

- scrape the Production/Public servers;
- capture account credentials;
- intercept private network traffic as a prerequisite for users;
- redistribute a protected exported personality;
- assume an exported profile is correct because Fixture Manager opens or checks it.

### 7.4 Manual profile adapter/editor

Manual authoring remains a first-class deterministic path for missing or generic fixtures.

It must support:

- exact manufacturer/model/mode naming;
- footprint;
- channel offset and optional fine offset;
- encoding/range/default;
- safe inactive value;
- custom semantic lane with description;
- source-manual hash and page/range notes;
- duplicate-and-edit flow so imported evidence remains immutable.

### 7.5 Raw channel discovery

Raw discovery is a bounded diagnostic source, not a general automatic profiler.

Rules:

- start with blackout;
- require an explicit address/range;
- skip known hazardous categories by default;
- cap hold duration;
- show the exact channel/value being sent;
- record operator observation;
- return automatically to blackout;
- convert discoveries into a manual/native profile candidate;
- re-run through Runner before qualification.

## 8. Local fixture catalog and project snapshots

### 8.1 Catalog responsibilities

A future local catalog may provide search, source updates, and reusable qualified profiles, but it is not the live source of truth.

Recommended local layout:

```text
%LOCALAPPDATA%/EmberLights/FixtureLibrary/
  manifests/
  sources/<sha256>.blob
  normalized/<native-profile-sha256>.json
  reports/<native-profile-sha256>.json
  manuals/<sha256>.reference
  indexes/
```

Do not copy manuals unless redistribution/storage rights permit; a reference may contain only locator, hash, title, and operator notes.

### 8.2 Immutable project snapshot

When a profile is added to a project:

- embed the complete normalized native profile;
- embed/source-link its evidence and qualification records;
- record the native-profile hash;
- do not require the local catalog or internet to run the project;
- do not auto-replace it when the catalog changes.

### 8.3 Update workflow

When a newer source profile exists:

1. import it as a new candidate;
2. compute a semantic and raw-channel diff;
3. show changes in footprint, offsets, encodings, ranges, defaults, fine channels, safety flags, and unsupported semantics;
4. invalidate inherited qualification unless the native-profile hash is unchanged;
5. require explicit project replacement;
6. preserve the old project history and last-known-good package;
7. re-run bench qualification before production use.

## 9. One-fixture qualification workflow

This workflow is the bridge from the active Micro recovery plan to a trustworthy library.

### Gate 0 — transport proof

Use **EmberLights Hardware Test** with no project loaded. Produce a report proving:

- Micro initialized through the deterministic lifecycle;
- a known channel/value caused physical response;
- blackout worked;
- the test repeated after close/reopen;
- ideally, it repeated after unplug/replug.

If Gate 0 fails, stop. Do not edit profiles as a substitute.

### Gate 1 — exact fixture identity

For one fixture only, record:

- manufacturer and exact model from the manual/label;
- manual file/hash;
- exact DMX mode and footprint shown on the fixture;
- universe and address shown on the fixture;
- whether the test is direct wired or through the known-good transmitter;
- output adapter and selected universe.

The first fixture should be selected by quality of documentation and certainty of mode/address, not by which fixture seems easiest to program.

Reported candidate inventory includes Both Lighting IR-4 units, Both Lighting 360 LED Tubes, and CHAUVET DJ Wash FX HEX units. These are validation targets only; exact manuals, model variants, modes, quantities, and addresses must be confirmed rather than inferred.

### Gate 2 — source candidate

Search in this order:

1. exact manufacturer manual/DMX chart;
2. exact SoundSwitch Production personality for comparison, when visible;
3. exact OFL fixture/mode;
4. exact QLC+ QXF;
5. SoundSwitch Public/Local personality;
6. manual native profile creation;
7. controlled raw discovery for unresolved channels.

For generic fixtures, search by the exact channel count and compare every channel with the manual rather than trusting a similar product name.

### Gate 3 — native conversion and review

Generate an import report containing:

- source identity/hash/version;
- selected mode and footprint;
- every native channel mapping;
- defaults and safe values;
- unsupported or lossy features;
- hazardous ranges;
- manual comparison checklist;
- deterministic native-profile hash.

The operator/developer marks each manual comparison explicitly; no implicit “all reviewed” checkbox from a successful parse.

### Gate 4 — raw-to-semantic frame comparison

1. Save the successful raw Hardware Test channel/value set.
2. Generate a one-fixture EmberLights bench project using the candidate profile, exact universe, and exact address.
3. Trigger a semantic action or Static Look that should produce the same state.
4. Capture Runner's rendered/output frame.
5. Compare universe bytes exactly.
6. Explain every difference, including defaults, master dimmer, shutter/open, macros, and unused channels.
7. Correct the native profile, not the raw report.

### Gate 5 — physical semantic test

Confirm at minimum, as supported by the fixture:

- blackout/off;
- intensity/open shutter;
- red;
- green;
- blue;
- white or equivalent emitter;
- one safe non-color function if relevant;
- close/reopen repeat;
- release from Static Look back to a running Autoloop after Static Look work lands.

Do not automatically test fog, haze, laser, spark, or high-rate strobe functions.

### Gate 6 — freeze qualification

Store:

- native-profile hash;
- source and manual hashes;
- exact patched universe/address/mode;
- raw report hash;
- Runner frame hash;
- operator result;
- date/build commit;
- qualification state.

Only then expose **Hardware Qualified** for that profile/fixture combination.

## 10. Validation and diagnostic codes

Add actionable warning/error codes. At minimum:

```text
MIGRATED_PATCH_UNVERIFIED
PROFILE_SOURCE_UNVERIFIED
PROFILE_MANUAL_NOT_MATCHED
PROFILE_MODE_UNVERIFIED
PROFILE_CAPABILITY_LOSSY
PROFILE_SWITCHING_CHANNEL_QUARANTINED
PROFILE_MULTI_CELL_FLATTENED
PROFILE_HAZARDOUS_RANGE_UNREVIEWED
PROFILE_NATIVE_HASH_CHANGED
PROFILE_QUALIFICATION_STALE
FIXTURE_ADDRESS_UNVERIFIED
FIXTURE_MODE_UNVERIFIED
FIXTURE_UNIVERSE_UNVERIFIED
FIXTURE_HARDWARE_UNVERIFIED
LIBRARY_UPDATE_AVAILABLE
LIBRARY_SOURCE_LICENSE_UNKNOWN
```

Rules:

- schema errors block compile/activation;
- quarantine/lossy errors block a Hardware Qualified claim;
- unverified warnings may allow an isolated bench project but remain visible;
- a migrated 71-fixture staging project must not report zero warnings;
- warnings must identify the affected fixture/profile and the exact corrective action.

## 11. Safety and unsupported semantics

The current native profile model intentionally supports a bounded semantic subset. The importer must not pretend that every upstream feature can be represented.

Review/quarantine examples include:

- switching channels and aliases;
- multiple capability ranges for one channel when one native range is insufficient;
- multi-cell topology and matrix ordering;
- wheels and indexed gobo/color slots;
- shared shutter/strobe channels with multiple pulse/random ranges;
- macros/program channels whose default can unexpectedly activate behavior;
- 16-bit relationships that are ambiguous or reversed;
- virtual dimmers and per-cell master channels;
- pan/tilt inversion and range metadata;
- fixture-specific reset, lamp-on, calibration, and service ranges;
- fog/haze/laser/spark functions.

If exact behavior cannot be represented safely, retain the source and report, quarantine the mode, and either extend the native model deliberately or select another physical mode. Never map an unknown control to a generic intensity/color property merely to make validation pass.

## 12. Implementation program

### Slice F0 — fixture truth in the active recovery build (P0)

Deliver after or alongside the raw Hardware Test, without delaying it:

- explicit migrated-patch warnings;
- profile and fixture qualification states/records;
- deterministic native-profile hash;
- expanded QXF import evidence/report;
- one-click one-fixture bench-project generation from one selected profile/mode;
- raw-report versus Runner-frame comparison;
- installed `MORNING_FIXTURE_TEST.md`;
- no broad catalog UI.

### Slice F1 — targeted SoundSwitch Fixture Manager evidence (P0/P1)

- document the exact operator export/capture workflow;
- accept and preserve one user-exported personality as an opaque source file;
- add format probe and bounded parser only after real samples exist;
- record Production/Public/Local origin separately;
- do not block qualification if parsing is unavailable—manual/OFL/QXF authoring remains the fallback;
- add golden tests for any parsed format before enabling project import.

### Slice F2 — pinned OFL catalog builder (P1)

- pin OFL commit/schema;
- produce EmberLights-native catalog artifacts through a deterministic adapter/build job;
- retain license/attribution manifest;
- run conversion tests and produce per-profile warnings;
- keep the catalog out of Runner;
- support search by manufacturer/model/mode/channel count in Studio/tooling;
- import selected immutable snapshots into projects.

### Slice F3 — profile diff, update, and qualification reuse (P1)

- local catalog cache;
- source/native hash indexes;
- semantic/raw-channel diff;
- explicit opt-in replacement;
- qualification invalidation rules;
- reuse qualification only when native-profile hash and patch identity remain exactly unchanged.

### Slice F4 — richer fixture model (post-core)

Only after one-fixture and Micro gates pass:

- native multi-cell/matrix semantics;
- richer capability ranges;
- wheels/gobos;
- switching channels;
- physical positioning/beam metadata;
- RDM-assisted discovery where supported;
- community contribution/update workflows.

## 13. Automated test requirements

### 13.1 Source and parser tests

- exact source hash and deterministic normalized hash;
- malformed, oversized, deeply nested, duplicate, and invalid inputs;
- unsupported source format remains preserved and does not crash;
- source classification and license metadata round-trip;
- same input + adapter version yields byte-identical native candidate/report;
- different source revisions do not collide.

### 13.2 Profile semantic tests

- channel 1/coarse offset 0 and footprint boundary;
- coarse/fine ordering;
- defaults inside valid ranges;
- duplicate offsets rejected;
- unsafe default/range combinations flagged;
- custom lanes preserved or reported;
- cell/switching/wheel lossiness reported;
- imported mode footprint matches channel layout;
- profile hash changes for every behavior-affecting mutation.

### 13.3 Project compatibility tests

- old project opens as unverified rather than qualified;
- evidence/qualification records survive save/reopen/history/restore;
- changing profile/address/universe/mode invalidates qualification;
- unknown future records remain preserved under the established compatibility contract;
- catalog update cannot change a compiled active project;
- project remains fully offline and portable.

### 13.4 Hardware comparison tests

Using fake transport plus Joshua's physical test:

- raw channel/value report serializes exactly;
- generated bench project emits the expected slot values;
- frame inspector attributes values to fixture/profile/property/winning layer;
- blackout bytes match between Hardware Test, Runner, shutdown, and reconnect warm-up;
- operator-confirmed evidence is never synthesized from accepted writes.

## 14. Required next-build deliverables

The next testable installer should include, in priority order:

1. The standalone Hardware Test from the core recovery plan.
2. A one-fixture bench-project workflow that does not depend on the 71-fixture migration.
3. A frame inspector/export showing exact nonzero channel/value pairs.
4. Visible `MIGRATED_PATCH_UNVERIFIED` warnings for the current converted project.
5. Profile/fixture evidence and qualification state persisted in the project.
6. An expanded QXF import report suitable for manual comparison.
7. A fixture source-capture path that can preserve one SoundSwitch Fixture Manager export without claiming it is already parseable.
8. `MORNING_HARDWARE_TEST.md` and `MORNING_FIXTURE_TEST.md` installed with the tools.
9. Release notes tied to the exact commit and explicit pending operator gates.

Do not spend this recovery build on a large fixture-browser UI. A simple file selector, exact mode selector, report, and bench-project generator are sufficient until physical output is proven.

## 15. Morning fixture test for Joshua

After the new installer is available:

1. Close SoundSwitch and any process that may own the Micro.
2. Use Hardware Test to prove one physical raw channel/value and automatic blackout.
3. Choose one fixture and confirm its exact label/manual, physical DMX mode, universe, and address.
4. In SoundSwitch Fixture Manager, search the exact model and channel count; inspect the exact channel table against the manual.
5. Where permitted, use **Save Fixture as…** and retain that single file for EmberLights evidence/import testing.
6. Import or create one exact profile in EmberLights; do not use the 71-fixture migrated project for this proof.
7. Generate the one-fixture bench project.
8. Trigger the equivalent semantic Look and compare its emitted frame to the successful raw frame.
9. Confirm expected color/intensity behavior and blackout.
10. Save, close, reopen, and repeat.
11. Export the Hardware Test, fixture conversion, frame comparison, and Diagnostics reports.

## 16. Acceptance criteria

The fixture-library foundation is ready for broader rig onboarding when:

- raw Micro output is physically confirmed;
- one exact fixture manual/mode/address is known;
- one native profile has complete provenance and deterministic hash;
- manual comparison is recorded;
- raw and Runner frames match for the qualified state;
- physical semantic behavior and blackout pass;
- evidence persists after save/reopen/history restore;
- a source update cannot mutate the active project;
- unverified migration fixtures remain visibly unverified;
- protected SoundSwitch content is neither scraped nor redistributed;
- OFL/QLC content enters through pinned, tested, Studio-only adapters;
- Runner remains deterministic, bounded, offline, and independent of every external fixture library.

## 17. Explicit prohibitions

- Do not treat the current 71-fixture converter output as decoded SoundSwitch addressing.
- Do not mark a profile qualified from a matching footprint or channel count alone.
- Do not bulk scrape SoundSwitch Production or Public servers.
- Do not bypass encryption, authentication, or licensing restrictions in exported fixture files.
- Do not package SoundSwitch personality content without a redistribution basis.
- Do not directly bind Runner to OFL JSON, QXF XML, SoundSwitch files, or network APIs.
- Do not auto-update project profiles.
- Do not silently flatten unsupported multi-cell/switching/wheel semantics.
- Do not run hazardous channel discovery automatically.
- Do not let fixture-catalog work displace the raw Micro proof, Connections repair, OS2L startup, or Static Look core semantics.

## 18. Primary references

Official SoundSwitch references:

- Fixture Manager manual: `https://cdn.inmusicbrands.com//soundswitch/FixtureManager/files/Fixture_Manager_Manual.pdf`
- Fixture Manager complete guide: `https://support.soundswitch.com/en/support/solutions/articles/69000844505-soundswitch-fixture-manager-complete-user-guide`
- Locating profiles in the SoundSwitch DMX library: `https://support.soundswitch.com/en/support/solutions/articles/69000848673-soundswitch-how-to-locate-fixture-profiles-in-the-soundswitch-dmx-library`

Open Fixture Library references:

- Project: `https://github.com/OpenLightingProject/open-fixture-library`
- About: `https://open-fixture-library.org/about`
- Fixture format: `https://github.com/OpenLightingProject/open-fixture-library/blob/master/docs/fixture-format.md`
- Plugin architecture: `https://github.com/OpenLightingProject/open-fixture-library/blob/master/docs/plugins.md`

EmberLights references:

- `docs/16_QLC_FIXTURE_IMPORT.md`
- `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`
- `native-core/include/showcore/fixture.hpp`
- `native-core/include/showcore/fixture_library.hpp`
- `native-core/include/emberlights/project.hpp`
- `native-core/src/qlc_fixture_import.cpp`
- `native-core/src/soundswitch_v1.cpp`
