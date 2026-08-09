# QLC+ Fixture Import

EmberLights can import a QLC+ Fixture Definition (`.qxf`) into the current project. This is a Studio convenience for using the broad QLC+/Open Fixture Library ecosystem without making QLC+, Qt, or upstream fixture formats part of Runner.

## Recommended workflow

1. Obtain a `.qxf` from a trusted QLC+ fixture collection or export a fixture through Open Fixture Library's QLC+ plugin.
2. Open **Profiles** and choose **Import QLC+ Fixture (.qxf)...**.
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
- regular strobe or hazardous activation ranges whose safe inactive value sits outside the active DMX range;
- up to sixteen otherwise unknown functions as preserved custom semantic lanes.

## What is reported or quarantined

- QLC+ switching-channel aliases and `ActsOn` modes are quarantined instead of guessed.
- Multi-head/cell topology is flattened and reported for manual verification; cell-aware effects are not represented yet.
- Shared shutter channels expose the ordinary strobe range and safe open/inactive value. Pulse, random, and other ranges require manual profile review.
- Heuristic name matches and custom lanes are reported as approximations.
- A malformed channel, invalid footprint, ambiguous duplicate channel name, or failed native validation prevents that affected fixture/mode from entering the project.
- XML external entities, DTD subsets, oversized documents, and excessive nesting/nodes are rejected.

## Runtime boundary

The QXF parser runs only during Studio import. The project stores the resulting stable native fixture profile, and the normal compiler validates it before show activation. Runner does not parse XML, load QLC+, access the network, or scan a fixture library during a performance.

Import success means the document was converted consistently; it is not proof that an upstream fixture definition or approximation matches the physical fixture. The manufacturer's DMX chart and an isolated hardware test remain authoritative.
