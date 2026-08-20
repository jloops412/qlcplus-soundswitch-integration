# Structured fixture-profile workbench checkpoint

Status: historical source-complete checkpoint for Windows preview 89. The permanent White/Amber UI described below is superseded by `38_MODULAR_FIXTURE_PARAMETER_CATALOG_CHECKPOINT.md`; its tested backend transaction remains available for compatibility/migration work. It does not claim physical qualification of any fixture or completion of the production skins renderer.

## Outcome

Studio → Fixture Profiles now treats a profile as an ordered channel workbench:

- the full channel map is a selectable table with channel, property, encoding, DMX range, default, and fine-channel columns;
- selecting a row loads its structured fields for editing;
- add/replace/remove operations validate a draft transaction before changing it;
- Dimmer 1CH, RGB 3CH, RGBW 4CH, RGBA 4CH, RGBWA+UV 6CH, and Master+RGBWA+UV 7CH templates create common safe direct-channel maps in one action;
- exact QXF import and official OFL search/download remain available for real fixture definitions;
- imported and built-in snapshots remain immutable until duplicated, preserving provenance.

The reusable authority is `emberlights/fixture_profile_editor.hpp`. It contains no Win32 control IDs and exposes template descriptors, table row view models, validated draft mutations, accessibility text, and absolute White/Amber assignment planning. A future Slint renderer, Default/Reference/Safe skins, and the Skin Designer can consume the same model instead of re-implementing channel semantics.

## White and Amber correction

The previous UI action always swapped the two labels. That backend transaction was correct, but the button was a relative toggle: clicking it again reversed the previous result, and the raw editor made the active saved profile hard to audit.

The new UI shows the desired final state explicitly:

`White output uses CHx` and `Amber output uses CHy`

Applying the current assignment is a successful no-op. Applying the reverse pair uses the existing transactional correction planner, so an immutable source is forked to a Local snapshot and every currently bound fixture is atomically rebound; a Local profile is updated in place. The plan still stale-checks the complete source snapshot and affected fixture set, then validates and compiles a project copy before replacing the document.

This does not guess physical fixture truth. The operator must select the exact physical mode and verify isolated White and Amber output. The canonical manual-backed Both Lighting BO-IR4 6CH definition remains White CH4 / Amber CH5; the 10CH definition remains White CH5 / Amber CH6 until physical evidence proves otherwise.

## QLC+ and OFL adoption boundary

This checkpoint adopts the proven fixture-editor interaction pattern documented by [QLC+](https://github.com/mcallegari/qlcplus-docs/blob/master/pages/11.fixture-definition-editor/03.channels/default.v4.md): an ordered channel list, selection-driven editor, property/preset assistance, defaults, and capabilities/ranges. It does not import Qt widgets or copy QLC+ visual assets. QLC+ remains an Apache-2.0 audited workflow/source reference.

[Open Fixture Library](https://github.com/OpenLightingProject/open-fixture-library/blob/master/docs/plugins.md) remains an adapter boundary rather than a runtime dependency: Studio downloads an exact QLC+ export snapshot through the existing importer, preserves source/hash/attribution evidence, and marks it unreviewed. OFL itself advises external consumers to use plugin transformations instead of binding directly to its evolving JSON format.

## Surface-contract reconciliation

- Commands/states/capabilities added to the public skin registry: none.
- New transitional Win32 callback: Replace Channel Map; recorded in the direct-callback bypass ledger under Authoring.
- Existing correction callback behavior: changed from relative toggle presentation to absolute final assignment while retaining the same atomic backend transaction.
- Project schema and persisted channel format: unchanged.
- Runner/compiler/property semantics: unchanged.
- New modular component contract: fixture-profile editor templates, rows, mutations, and assignment plan.
- Compatibility: additive authoring UX; existing projects and imported profile snapshots remain readable.

## Verification

- `fixture_profile_editor_tests`: templates, ordered row/accessibility model, safe structured add/replace/remove, refusal of unsafe generic strobe presets, absolute no-op assignment, immutable-source fork/rebind, and repeated-apply no-op.
- `core_tests`: canonical IR-4 profile correction and renderer semantics remain green.
- `ofl_fixture_catalog_tests`, `static_look_authoring_tests`, `static_look_physical_preview_tests`, and `studio_foundation_tests`: green.
- Windows x64 warnings-as-errors build, package contract, and installer archive evidence are recorded in the preview 89 handoff after packaging.

Native Windows UI inspection, keyboard/DPI/Narrator evidence, physical IR-4 mode truth, and clean/upgrade/uninstall lifecycle remain tester gates.
