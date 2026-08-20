# Fixture Catalog and QLC+ Import

EmberLights can search the official Open Fixture Library (OFL), download its QLC+ export for one exact fixture, or import a local QLC+ Fixture Definition (`.qxf`) into the current project. These are Studio conveniences for using the broad QLC+/OFL ecosystem without making QLC+, Qt, upstream fixture formats, or a network API part of Runner.

## Official catalog workflow

1. Open **Profiles** and enter the manufacturer/model printed on the fixture or official manual under **Search Official Open Fixture Library**.
2. Choose **Search**. EmberLights calls the official OFL search API on a bounded authoring worker; Live and the DMX scheduler do not wait on the network.
3. Select only the exact fixture identity. A similar name is not a substitute. OFL currently returns no exact Both Lighting BO-IR4/IR-4 entry, and the Chauvet DJ WashFX result is not Wash FX Hex.
4. Choose **Download + Import Selected**. EmberLights downloads the official QLC+ 4.12.2 export over HTTPS and passes it through the same bounded quarantine-aware QXF adapter used for local files.
5. Review every imported mode and warning against the manufacturer DMX chart, then patch and test the exact physical mode.

Each accepted mode embeds an immutable native snapshot and a source-evidence record containing the OFL key, fixture page, exact download URL, MIT attribution, exact QXF SHA-256, and EmberLights adapter version. The live OFL endpoint does not expose its deployment commit, so the UI reports that limitation and never auto-updates the project snapshot. Catalog availability or conversion success does not qualify a fixture.

Official contracts used by this adapter:

- <https://github.com/OpenLightingProject/open-fixture-library/blob/master/ui/api/openapi.json>
- <https://github.com/OpenLightingProject/open-fixture-library/blob/master/ui/api/routes/get-search-results.json>
- <https://open-fixture-library.org/about/plugins/qlcplus_4.12.2>
- <https://github.com/OpenLightingProject/open-fixture-library/blob/master/LICENSE>

## Local QXF workflow

1. Obtain a `.qxf` from a trusted QLC+ fixture collection or export a fixture through Open Fixture Library's QLC+ plugin.
2. Open **Profiles** and choose **Import Fixture File (.qxf)...**.
3. Read the conversion summary. Each safely representable QLC+ mode becomes a separate EmberLights profile; an unsupported mode can be quarantined without blocking the others.
4. Select the imported mode and compare every offset, default, and function with the fixture manufacturer's official DMX chart.
5. Duplicate the imported profile if corrections are needed. Imported snapshots stay read-only so provenance remains meaningful; the duplicate is an editable local profile.
6. Patch and test the profile with output disabled, then with a visualizer or isolated fixture before using production hardware.

## What converts automatically

- manufacturer, model, mode, footprint, creator metadata, source hash, and QLC+/OFL provenance;
- common QLC+ presets and groups for dimmer, RGB-family emitters, CMY, pan/tilt, shutter/strobe, color wheel, gobo, prism, focus, zoom, iris, frost, effects, fan, fog, and haze;
- QXF `<Colour>` emitter metadata;
- coarse/fine channel pairs as native 16-bit mappings;
- channel defaults and constant/no-function slots;
- non-overlapping QLC+ capability rows as named native slots or continuous ranges, including shared shutter/strobe and wheel-style channels;
- regular strobe or hazardous activation ranges whose safe inactive value sits outside the active DMX range; hazardous rows retain their semantic arming gate;
- up to sixteen otherwise unknown functions as preserved custom semantic lanes.

## What is reported or quarantined

- QLC+ switching-channel aliases and `ActsOn` modes are quarantined instead of guessed.
- Multi-head/cell topology is still flattened and reported for manual verification. Channel owner metadata exists, but cell-aware grouped effects and matrix realization are not complete.
- Shared shutter, strobe, wheel, and effect ranges are retained when they do not overlap. Every imported row remains unreviewed; reset/service/reserved/custom rows are Protected and unavailable until an operator creates and qualifies an intentional Local model.
- Overlapping or switching/alias-dependent capabilities remain source evidence only and are reported for manual reconstruction.
- Heuristic name matches and custom lanes are reported as approximations.
- A malformed channel, invalid footprint, ambiguous duplicate channel name, or failed native validation prevents that affected fixture/mode from entering the project.
- XML external entities, DTD subsets, oversized documents, and excessive nesting/nodes are rejected.

## Runtime boundary

OFL HTTPS/search and the QXF parser run only during Studio authoring. The project stores the resulting stable native fixture profile, and the normal compiler validates it before show activation. Runner does not parse XML, load QLC+, access the network, or scan a fixture library during a performance.

Import success means the document was converted consistently; it is not proof that an upstream fixture definition or approximation matches the physical fixture. The manufacturer's DMX chart and an isolated hardware test remain authoritative.
