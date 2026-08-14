# UI course correction and replacement-shell gate

UI course correction (2026-08-14).

Status: binding product-direction correction for issues #29, #31, #33, #37,
#38, #52, and #66. This checkpoint changes the classification and acceptance
of prior UI work; it does not change Runner, output, safety, or project behavior.

## The correction

EmberLights has a strong modular engine and increasingly useful fixture-domain
models, but the installed presentation drifted toward a larger legacy Win32
form system. That presentation is not the product UX, is not EmberLights
Default, and is not an acceptable path to a skin platform.

The correction preserves the reusable domain and contract work while rejecting
the product claim attached to the legacy adapters:

| Checkpoint | What remains valid | Correct classification |
| --- | --- | --- |
| Preview 98 | Toolkit-neutral authoring collection, selection, draft, search, and responsive-geometry model | Authoring-model checkpoint with a legacy bridge; not Default 2.2 |
| Preview 99 | Unified profile-backed direct-channel and named-range Fixture Control catalog | Fixture-domain contract checkpoint; not Default 2.3 |
| Preview 100 | Surface-aware fixture-control snapshot, stable selection, coverage, safety, and diagnostics | Model plus rejected legacy Inspector adapter; not Default 2.4 or professional UI |

The Win32 application remains available only as a trusted transitional/Safe
bridge until the replacement shell earns activation. It may receive safety,
defect, accessibility, and bridge-removal changes. It may not receive another
top-level editor, modeless property form, Win32-only feature, or product-skin
claim. The numeric call-site ceilings in
`spec/ui/ui-course-correction-v1.json` make that freeze testable.

## Product UX target

The product is a modern, fast, pro-level lighting workstation whose visible
controls follow the operator's task:

- fixture parameters render as color mixers, XY position pads, faders, rate
  controls, visual slot choices, and explicit safety-gated triggers;
- Profiles expose ordered channels, 8/16-bit encoding, named ranges, defaults,
  blackout/highlight, provenance, audit, duplication, and patch impact;
- Static Looks expose fixture/group selection, categorized parameters,
  `RELEASE`/`SET`/`FORCE_ZERO` ownership, validation, and bounded realtime
  preview only while normal Live is stopped;
- raw channels and DMX values remain available for profile work, testing, and
  troubleshooting, but ordinary authoring does not begin in a diagnostic table;
- the same stable fixture-control identity serves Static Looks, Autoloops,
  Live, MIDI/controller mappings, Ember Actions, migration, and future skins;
- skins choose layout, composition, styling, visibility, and bindings. They do
  not own domain behavior, timing, persistence, output, or safety.

SoundSwitch remains the primary workflow and migration reference. WOLFMIX is
secondary live-control inspiration. QLC+ is a workflow, fixture-definition,
and selectively audited open-source reference rather than the product shell.
OFL remains the preferred open fixture foundation, with GDTF Share the rich
industry interchange direction and QLC+ QXF a secondary compatibility source.

## First product-shaped slice

The first replacement-shell slice is **Fixtures + Static Looks**, because it
forces the toolkit and component boundary to solve real pro-DMX work rather
than a decorative dashboard:

1. search/import a fixture profile and inspect its provenance;
2. inspect or edit ordered channel mappings and named DMX ranges;
3. patch/select a fixture or group;
4. create/select a Static Look;
5. edit categorized parameters with appropriate visual controls;
6. choose `RELEASE`, `SET`, or `FORCE_ZERO` ownership;
7. validate and run bounded physical preview only while Live is stopped;
8. open raw DMX diagnostics through explicit Advanced disclosure.

`spec/ui/examples/default-studio-fixtures-looks-standard.layout.json` is the
renderer-neutral acceptance fixture. `ember.fixtureControlSurface` is the new
task-facing component contract. `ember.fixtureFunctionBrowser` remains a
diagnostic/advanced bridge, not the ordinary editing surface.

The slice must work at 1366×768 and 1920×1080, with keyboard-visible focus,
usable touch/pointer targets, clear empty/error/read-only states, and no
separate modeless Inspector required to finish the workflow.

The first implementation checkpoint is recorded in
`45_SLINT_FIXTURES_LOOKS_LAB_CHECKPOINT.md`. It adds a renderer-neutral shell
composition model and an opt-in pinned Slint lab while preserving every gate
in this document. It is implementation evidence, not toolkit acceptance.
The shell model's second source pass applies D-095: category/search/control
projection comes from the complete selected-target profile catalog, so the
renderer cannot quietly regress to an Intensity/RGB/position allowlist as new
fixture parameters, named ranges, skins, or controller surfaces are added.

## Replacement-shell gate

The dependency order remains:

1. reconcile command/state/component registry authority;
2. run issue #37 with the real Fixtures + Static Looks slice: Slint/C++ first,
   WinUI 3 as the supported-Windows control comparison, and Direct2D/Win32 only
   as the Safe baseline;
3. select a toolkit from measured startup, memory, repaint, DPI, accessibility,
   deployment, and scheduler-impact evidence—no toolkit is selected yet;
4. implement `.emberskin` validation/runtime plus the trusted Safe surface;
5. activate the product-shaped Default slice;
6. build the evidence-bounded SoundSwitch Reference presentation over the same
   behavior.

Issue #38 still gates broad production rollout and hardware claims. Bounded
product-shaped models and toolkit spikes may proceed in parallel because they
do not change or delay Runner/output qualification.

## Installer rule

A model, document, or rearranged Win32 form does not earn another product
preview number. The next UI installer must either activate this first slice in
the replacement-shell toolkit or be labeled explicitly as a non-product lab
build. Preview 100 remains the most recent installed legacy-bridge build until
that bar is met.

## Acceptance evidence

This correction is enforced by:

- `spec/ui/ui-course-correction-v1.json`;
- the Fixtures + Static Looks layout fixture;
- the generated registry entry for `ember.fixtureControlSurface`;
- `fixture_control_surface_tests`, which prove visual grouping, stable
  semantics, availability, safety, and Advanced hiding;
- `fixtures_looks_shell_tests`, which prove the first slice across profile,
  target, Static Look, ownership, responsive, stale, read-only, and empty states;
- `test_slint_lab_contract.py`, which proves the lab/product boundary, source
  workflows, minimum viewport, accessibility declarations, Advanced-only raw
  diagnostics, existing-domain mutation path, and optional exact-compiler check;
- `test_ui_direction.py`, which prevents new legacy top-level/form growth and
  verifies the first-slice composition and installer policy;
- normal registry compatibility, generated-artifact, warning-fatal build, and
  full native tests.
