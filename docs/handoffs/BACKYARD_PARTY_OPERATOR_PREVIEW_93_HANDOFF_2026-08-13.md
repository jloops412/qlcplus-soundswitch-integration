# Backyard-party operator Preview 93 handoff — 2026-08-13

Status: exact committed-source Windows installer and local package contract
passed. This is an unsigned testing preview; native Windows lifecycle and
physical fixture/controller qualification remain deferred.

## Primary outcome

The readable fixture-profile functions introduced in Preview 92 no longer stop
at Static Looks and Live Overrides. The same stable semantic choices now drive
committed Autoloop V2 event edits and persistent MIDI/controller mappings.
Mixed fixture profiles remain exact by expanding one user gesture to bounded
per-fixture semantic records when one shared value is not truthful.

## What changed

- Studio → AutoScript can select a committed V2 placement, fixture/group,
  named profile function, exact start/end beat range, and range position.
- **Preview + Add Function** builds a complete candidate, validates it, renders
  it through the production compiler with every output adapter disabled, and
  shows a frame SHA-256 before the existing Studio document transaction keeps
  the source edit.
- Mixed-profile groups create exact fixture targets, lanes, and semantic
  Property Blocks. The source contains no raw DMX addresses/bytes and no
  display-label inference.
- MIDI Learn can select an optional named fixture function. One homogeneous
  group uses the existing group-property mapping; a mixed-profile group expands
  atomically to exact fixture-property mappings within project and
  actions-per-message limits.
- Fixed slots bind their exact semantic value. Continuous ranges bind their
  exact normalized endpoints and retain applicable curve/inversion/pickup
  settings.
- A stable named-choice ID persists in the existing format-1 MIDI record;
  older projects keep an empty value and continue to round-trip.
- Protected choices never appear. Safety-gated functions remain hidden from
  these quick-pick paths and fail closed in the reusable services without an
  explicit authoring opt-in.
- A deterministic migration-portability review now separates source artifact
  identity from semantic decoder qualification across Probe, Inventory,
  Decode, Reconcile, Plan, Commit, and Upgrade.
- WOLFMIX is explicitly research/evidence unavailable under
  `wolfmix.controlled_delta_corpus_unavailable`. This build contains no WOLFMIX
  source parser, project importer, or vendor-specific runtime engine.

## Quick test route

1. Open or create a project with a patched fixture profile that has named
   ranges/slots. Confirm the exact physical mode and patch before enabling any
   output.
2. In Studio → AutoScript, generate/preview/commit a V2 loop if the project has
   none. In the lower named-function editor select the placement, fixture or
   group, function, and a beat range inside the loop.
3. Choose **Preview + Add Function**. Confirm the summary says output adapters
   are disabled, lists the exact fixture write count, and shows a frame digest.
   Save and reopen the project, then preview the placement again.
4. In Studio → MIDI Mapping, choose Set Fixture Property or Set Group Property,
   select the target and optional named fixture function, then Learn the desired
   control. Save/reopen and confirm the mapping retains its readable function
   provenance.
5. For a deliberately mixed-profile group, verify one MIDI gesture creates the
   complete fixture fanout and that a fixed slot does not wait on soft
   takeover. Capacity or safety rejection must create no partial mappings.
6. For White/Amber disagreement, continue to verify profile/mode/address and
   isolated raw response. Preview 93 does not add a global W/A swap or invert
   the renderer.

## Focused verification at source checkpoint

- warning-fatal Make `all`: passed;
- complete 30-executable Make test surface: passed;
- `autoloop_fixture_controls_tests`: passed;
- `fixture_controller_binding_tests`: passed;
- `migration_portability_review_tests`: passed;
- fresh CMake configure/build and focused CTest registration: 3/3 passed;
- CMake and Make both include the modules in `showcore` and include all three
  dedicated tests in their ordinary test surfaces;
- build/docs diff check: passed at this checkpoint.

## Surface/registry boundary

No callable command, state, component, capability, registry generation, or
Runner path is added. The Win32 fields are transitional adapters that select
arguments for existing Studio authoring and MIDI semantic-action paths. They
remain in the existing Authoring direct-callback ledger area with issue #31
G1E/#66 as the typed command/state/component removal gate.

## Installer evidence

- Version: `0.1.0-preview.93.0`
- Source commit: `375348e72ec37f7ed302f0cdc52d7c7dacaff865`
- Source tree: `8398d1f87a747c32698ec73612ebdf3ff6096464`
- Installer: `EmberLights-0.1.0-preview.93.0-Setup.exe`
- Installer size: 1,904,991 bytes
- Installer SHA-256: `cda9457c6c2b3aca8c1624d52fe19ab36088c08e070b628aefaa4efd2864a69f`
- Payload manifest: `EmberLights-0.1.0-preview.93.0-Windows-payload-manifest.json`
- Payload-manifest SHA-256: `00d77eef6afe39db9483192528d992bd445734916a1f63798988f00c1b15aec4`
- Package-contract regressions: 17 passed
- Stage: 18 product files plus generated manifest; verified
- NSIS: two compiles from the same verified stage were byte-identical
- Archive: 7-Zip test/extraction passed; all 19 embedded payload files matched
  the verified stage byte-for-byte; normalized extracted contract passed

Evidence label: **contract-tested unsigned testing preview** built on a
non-Windows host. Native Windows clean install, upgrade, GUI launch, Installed
Apps uninstall, shortcut/registry cleanup, project/settings preservation, and
hardware output were not asserted by this package run.

## Honest remaining boundaries

- production toolkit controls, type-ahead/category browsing, responsive layout,
  accessible mapping editor, controller feedback, conflicts, and bundled
  Control One qualification;
- Ember Action and EmberSkin/native-component consumption of the same catalog;
- reusable Position/Attribute/Movement/Effect assets and their Autoloop
  authoring/resolution workflows;
- authorized matching SoundSwitch controlled-delta evidence and exact decoder
  coverage; matching hashes still do not prove semantic fidelity;
- any WOLFMIX parser/import/mapping-adapter claim;
- installed-Windows lifecycle, DPI/accessibility, real fixture/controller,
  output-interface, soak, and gig qualification.

Passing software tests proves deterministic source/mapping construction and
fail-closed bounds. It does not prove a physical profile, fixture mode/address,
controller feedback, decoder fidelity, Windows installation, or gig readiness.
