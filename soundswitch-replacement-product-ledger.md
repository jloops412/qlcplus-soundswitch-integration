# SoundSwitch Replacement — Product Ledger

Status: native vertical slice implemented and verified; Windows hardware laboratory next

Last updated: 2026-08-12

## Product direction

Build an independently owned, general-purpose, gig-ready DJ lighting application that can be used with rigs unrelated to Joshua's. SoundSwitch is the primary product reference. Wolfmix is only a secondary reference for live-control ideas that improve the SoundSwitch-style workflow and may help Wolfmix users migrate.

The minimum finished-product bar is full relevant SoundSwitch functional parity: track-aware automation, Autoloop-style fallback shows, detailed manual scripting, fixture and venue management, MIDI control, supported synchronization/integration paths, and reliable lighting output without subscription or ecosystem lock-in. Delivery remains staged for safety, but deferred parity items are not optional.

## Confirmed decisions

- SoundSwitch features and workflows take priority over Wolfmix features.
- The application is for general users; Joshua's fixtures, controller, and event workflow are validation inputs rather than product scope.
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md` is the binding completeness checklist. No build is called parity-complete or public 1.0 while relevant rows remain unverified.
- Wolfmix-derived ideas belong after the SoundSwitch-style core unless they naturally strengthen the same workflow.
- V1 supports up to two DMX universes.
- OS2L is the primary DJ-software integration for V1.
- Live-audio BPM detection is the fallback when richer DJ metadata is unavailable.
- The runtime must remain lean enough to coexist with DJ software on a performance laptop.
- The first gig-ready release must support both arrangements equally:
  - editor/runtime on the same computer as the DJ software;
  - runtime on a separate lighting computer connected over the local network.
- MIDI is a first-class, device-agnostic subsystem rather than a Control One workaround.
- MIDI scope includes MIDI Learn, multiple simultaneous controllers, buttons, pads, CCs, faders, knobs, relative encoders, modifiers/layers, scaling, inversion, curves, soft takeover, and outbound LED/ring feedback where supported.
- SoundSwitch Control One should have a bundled MIDI profile.
- Control One MIDI input/feedback and its restricted onboard DMX interface are separate engineering tracks.
- SoundSwitch project migration is a core product capability and a major adoption advantage, not a post-launch afterthought.
- The import process must preserve unknown/unsupported source data so later importer improvements can recover it without requiring another migration.
- Windows is the first production operating system. The design should remain portable, but V1 validation targets Windows DJ workflows first.
- macOS follows the Windows production launch and is not a launch gate; portable boundaries remain a design goal.
- Serato is the second direct DJ-integration priority after VirtualDJ is stable.
- The usual DJ hardware is a Pioneer DDJ-REV7 running VirtualDJ.
- The performance benchmark should include low-end computers even though most target DJs use mid- to high-end systems.
- The user currently expects to test a SoundSwitch Control One, a likely ADJ MyDMX Buddy, and the simple SoundSwitch USB-DMX interface. Exact USB identities still need to be captured before compatibility is promised.
- The user can later provide a real `.ssproj` export and copies of scripted audio files for importer research.
- Static Looks and Autoloops are the first usable-workflow priorities. Custom scripted-track migration is not a first-pilot gate, but manual scripting and scripted playback remain mandatory parity capabilities.
- The engine remains event-agnostic: dinner, special events, entrances, first dances, open dancing, ceremonies, and non-wedding events use reusable content/templates rather than hard-coded modes.
- The supplied SoundSwitch 2.10.3 export confirms four 16-cell Both Lighting 360 tubes, a four-uplight BO-S601 bank, one logical Wash FX HEX dance-floor wash, and two BO-IR4 spotlights. The export also contains multiple alternative mover/effect profiles; physical mode/address confirmation still gates output.
- The architecture uses a custom DJ/event domain core rather than a full QLC+ fork.
- QLC+ is an optional compatibility bridge, reference implementation, and possible source of selectively adapted Apache-2.0 components after dependency/license review.
- The default Runner must not require QLC+, Qt, or a second application.
- Studio and Runner are logical modes. The lean default is one Runner process, one Studio process when authoring, an on-demand analysis worker, and an optional isolated adapter/QLC+ process only when required.
- The live path is deterministic and contains no AI model.
- The initial product language remains provisional. Rust is the preferred product-core candidate; a portable C++20 reference spike is being built and benchmarked because the current build runtime does not include Rust.
- Open Fixture Library ingestion belongs in a versioned Studio adapter that emits our stable profile format with provenance and quarantines unsupported profiles. Runner never parses OFL source JSON.

## SoundSwitch migration evidence and design target

Official SoundSwitch documentation says a `.ssproj` project can carry venues, fixtures, Autoloops, static looks, position cues, attribute cues, and—when selected during export—lighting files. Manually scripted and autoscripted lightshows also involve data saved with the associated audio files. Therefore migration should be layered:

1. Import project, venue, fixture patch, groups/categories, DMX addresses, looks, positions, attributes, and Autoloops.
2. Import lighting files packaged with an exported project when their representation can be decoded safely.
3. Scan scripted audio files for SoundSwitch metadata and reconstruct track associations and cue timelines when possible.
4. Produce a migration report listing imported, translated, approximated, unsupported, and missing items.
5. Retain the original `.ssproj`, lighting-file payloads, and unrecognized fields in a read-only source bundle.

Real-world migration details recovered from the user's earlier SoundSwitch work:

- They have used SoundSwitch Shortcut `.ssproj` packages, including an upgrade with 59 Autoloops.
- Their projects use fixture groups, fixture/group blackout and flash actions, static scenes, attribute cues, color pads, gobo/beam cues, position scripting, movement statics, builds, and drops.
- Earlier manual SoundSwitch-to-SoundSwitch migration required remapping mover addresses and sometimes recreating mover offsets.
- Deleting a fixture could remove associated attribute cues, so our importer and editor must use stable fixture identity and warn before destructive changes.
- Their rig has used one universe for Both Lighting fixtures; two-universe operation may involve separate wireless DMX transmitters.
- The 2026 export contains 4,185 payloads and a 173,970,312-byte track map. First-pilot conversion intentionally prioritizes the active color rig and the first 32 named Autoloops; opaque track shows remain source-preserved rather than guessed.

## Provisional live-control model

Use context-sensitive, non-destructive overlays while an automated song show continues underneath:

- Momentary actions such as flash, blinder, strobe, blackout, and fog apply only while held, then return with a short anti-snap fade (provisional default 100 ms; blackout and safety actions may cut immediately).
- Continuous controls temporarily own only the mapped parameter and target fixtures; the song show continues to drive everything else.
- Static Looks latch until changed or cleared and crossfade back to the still-running lower layer (provisional default 750 ms).
- A one-shot Autoloop begins immediately, completes its full musical length, and returns at its natural boundary. Infinite and track-duration repeat modes remain available.
- Returning continuous control should never jump: use soft takeover and a short configurable blend.
- Every mapping can override the default with momentary, toggle, latch, timed, or release-at-boundary behavior.

This matches SoundSwitch's documented Autoloop behavior while making Static Look and momentary-FX return timing explicit. The defaults remain subject to A/B observation on Joshua's rig.

## Open decisions

- A/B validation of the provisional live-MIDI takeover and return timings.
- Minimum supported Windows version; macOS timing is already deferred until after Windows launch.
- Which SoundSwitch artifacts are available as importer fixtures: `.ssproj`, exported project with lighting files, original scripted audio, and/or Control One export.
- Exact official/public Serato integration route and first qualification workflow; priority is already accepted as second after VirtualDJ.
- Supported DMX transports and exact USB interfaces for the first hardware test matrix.
- Scope and workflow of manual track scripting versus Autoscripting in the first usable release.
- Product name and visual identity.
- Exact Windows minimum version and concrete low-end/reference benchmark machines.
- Whether the QLC+ bridge is acceptable as a temporary path for any owned USB interface that lacks a native driver.
- Which aspects of QLC+ are worth selectively adapting after the reference-core benchmark.
- Rust versus C++ for the production Runner, based on measured footprint, latency, toolchain, safety, and Windows driver integration.
- Exact MIDI messages and feedback behavior exposed by the user's Control One.

## Product principles

- Gig safety beats novelty.
- Automation-first, live-controllable at any moment.
- No destructive import or opaque conversion.
- Same project and behavior across same-computer and networked-computer modes.
- Graceful degradation: scripted show, then DJ-aware automation, then beat-synced Autoloops, then safe static look.
- Open controller and DMX hardware choices wherever technically possible.

## 2026-08-08 implementation checkpoint

- Durable handoff/specification package created under `docs/`.
- Portable show-package schema created under `spec/`.
- Dependency-light C++20 native reference core created under `native-core/`.
- Implemented and tested: sparse property layers; strict fixture-profile/patch validation; broad semantic and custom attribute lanes; defaults, constants, inverted/discrete 8-bit and 16-bit fixture rendering; literal raw-zero safety; fail-closed fog/haze/laser/spark policy; two universe frames; OS2L flat-message parsing; sync failover; MIDI mapping/soft takeover; Autoloop pattern; ArtDMX encoding; IPv4 unicast; deterministic replay; and zero-allocation rendering.
- Added a fixed compiled fixture-profile store with provenance and fail-independent quarantine, generic Static Look playback with interruption-safe crossfades, and a scalable 64×32 Autoloop catalog/player with a pageable four-bank control window, one-shot natural return, infinite repeat, track-duration repeat, progress, bank selection, duplication, and slot movement.
- Added transient Live Overrides for fixtures and named groups with one-touch MIDI control: targetless Release All, normalized named-group property mappings, and scoped literal-zero group blackout. Groups compile to immutable deduplicated fixture-ID arrays, the actions share bounded MIDI-to-Runner paths, the release clears only the ManualOverride layer, persisted action values remain compatible, and active override counts return to Live and Diagnostics. MIDI pads can select a bank, enable/disable it through normal mapping behavior, or restore all 64 Autoloop banks through the existing Runner status mask.
- Release benchmark with 128 fixtures and one million full renders measured approximately 16.2 microseconds per tick, an 875,552-byte engine object, and about 4.5 MB max RSS in the current Linux environment.
- A 128-fixture full performance benchmark with continuous Autoloop interpolation and Static Look transitions measured approximately 113.6 microseconds per update, about 0.45% of one CPU core at 40 Hz, and about 5.2 MB max RSS in this environment.
- This checkpoint does not constitute Windows, VirtualDJ, Control One, or live-event qualification.

## 2026-08-10 SoundSwitch V1 conversion checkpoint

- Added a read-only `convert-v1` workflow qualified against the supplied SoundSwitch 2.10.x color rig. It hashes the venue and active-Autoloop inputs, validates required model signatures, emits a native checksummed project, and writes an approximation-aware JSON report.
- The generated project contains 71 logical fixtures, 9 groups, 18 Static Looks, and 32 semantic Autoloops. Four pixel tubes are represented as 16 RGB cells each so the existing semantic renderer can address the full 48-channel fixtures without duplicate property mappings.
- The generated patch occupies universe 1 addresses 1–263 without overlap. Art-Net, sACN, and USB-DMX are all off; those addresses are staging defaults and must be compared with the physical rig before output is enabled.
- Movers, GigBars, PartyBars, cold sparks, and track shows remain deferred and source-preserved. This checkpoint is a practical first-pilot color-rig V1, not SoundSwitch parity or hardware qualification.

## 2026-08-12 fixture truth and Static Look checkpoint

- Canonical manual-backed Both Lighting BO-IR4 6CH and 10CH profiles now preserve the exact RGBWAUV order and source SHA-256. Unknown 10CH program/color-speed functions are held at safe constant zero instead of exposed as generic sliders.
- Static Looks now have shared fixture/group capability inspection, explicit ownership, a native RGB picker, independent RGBWAUV/Master values, pure-emitter swatches, and full-color ownership that closes unwanted emitters and modeled strobe.
- Exact offline preview runs the production compiler/renderer without opening hardware and reports fixture/profile/mode/revision, property ownership, winning layer, rendered byte, warnings, and a deterministic frame SHA-256.
- Validation now rejects unsupported Look properties and enforces Look capacities/name/fade; Look deletion reports Autoloop, TrackScript, and MIDI dependencies.
- Static Look drafts have a generation-checked `StudioDocumentService` transaction with one Undo entry. The remaining Win32 editors still need the planned document-authority migration.
- The full native suite and Windows Release cross-build pass. Physical qualification of both IR-4 fixtures, a fail-closed physical preview, virtual intensity for the dimmerless 6CH mode, rich channel functions/ranges, immutable profile revisions, the Studio-only fixture catalog, and measured color calibration remain open.
- Binding checkpoint and ordered continuation: `docs/32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md`; tracked in issue #52.

## 2026-08-12 registry, Studio, Autoloops, and skins integration checkpoint

- PR #81 merged the deterministic canonical UI registry spine at `47bb45d`: the existing 29 commands and 39 states retain their IDs, ordinals, and behavior while generated native/catalog/reference outputs, semantic compatibility baselines, and anti-drift gates become authoritative.
- PR #82 / issues #58–#60 remain the remote Autoloops V2 lanes. Their accepted 960-PPQ source/compiled boundary is extended locally by transactional authoring, an original starter pack, allocation-free director, optional immutable `CompiledShow` package, explicit V1/V2 Runner layer ownership, generation-controlled activation/status/commands, canonical rich-source persistence/history/save, deterministic bounded AutoScript plus its Studio transaction bridge, and exact output-disabled V2 preview with stable-ID Palette realization. TrackDuration epoch, Position/Attribute/Movement/Effect and legacy/transition references, migration decoding, broad UI, and installed/physical evidence remain open.
- PR #83 / issue #65 remains the remote Ember Actions lane. The local integration extends bounded canonical validation and generated-registry IR with `Sequence` / `InvokeCommand` / `If` / `Switch` / `OnResult` / `Return` through the injected approved command-control interface. Literal/declared-parameter control flow and prior-command result routing preserve cancellation, hard stops, deterministic trace, honest worst-case budgets, copied-IR integrity checks, and zero-allocation repeat execution. State/context reads, transforms, parallel/nested Actions, activation/UI wiring, Studio transactions, async work, and expert round-trip remain open.
- PR #53 / issue #46 remains the remote rich Studio color/preview lane. The local integration retains structural no-output isolation while adding bounded V2 transport, generation/source/compiled/frame digests, production compiler/evaluator/renderer frames, semantic ownership, and exact/degraded/missing/ambiguous/unsupported Palette-resolution evidence with last-good retention. Preview cannot qualify fixture truth or hardware.
- Issue #63 is the canonical skins epic with work packages #64–#68, #70, and #72. Duplicate planning issues #69/#71/#73/#74/#75 were closed as mapped duplicates with their detailed bodies preserved.
- Physical SoundSwitch Micro, Control One dual-output, owned-fixture behavior, installed Windows launch, and live-event qualification remain unproven gates. SoundSwitch remains the first migration/parity target; Wolfmix and other adapters remain later sources.

## 2026-08-12 local production integration checkpoint

- Registry authority remains the merged PR #81 spine. Local compatible-additive generation 2 preserves its exact 29 accepted commands and 39 accepted states while adding five planned non-callable components, one accepted non-callable Static Looks capability, eleven reusable value/unit/target contracts, explicit result metadata, deterministic generated outputs, and digest `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`.
- The restored Studio lane now has semantic RGB/HSV/HSL/CMY/hex/Kelvin-tint authoring, explicit White/Amber/UV/Lime/Indigo intent, compiled no-output Look/Autoloop preview, and bounded `COLOR_PALETTE_V1`/`COLOR_SWATCH_V1` project assets with deterministic round-trip and generation-checked Undo/Redo. It does not open output or claim fixture-calibrated color.
- The restored Ember Actions lane now validates bounded canonical Action documents against the generated registry, produces immutable resolved-reference/cache-key and executable-IR identities, and executes the schema-shaped `Sequence` / `InvokeCommand` / `If` / `Switch` / `OnResult` / `Return` subset through injected `EmberActionCommandControl`. Literal/parameter boolean composition, typed comparisons, bounded enum/integer/string cases, and prior-slot native result cases/defaults have deterministic trace; skipped branches never dispatch and QueueFull/SafetyRejected/InternalError remain hard stops. Cancellation, node/dispatch/depth/expression/result/trace budgets, registry checks, typed results, integrity seals/metric recomputation, and transaction refusal are covered. It has no production activation service, direct Runner/UI coupling, devices, I/O, state writer, or alternate timing engine.
- The restored Autoloops V2 lane now normalizes a stable 960-PPQ source model, compiles only used content into deterministic immutable arenas, provides transactional content/placement authoring plus an independently authored 128-placement starter pack, and runs an allocation-free Autonomous/Scripted/Manual director with deterministic bank/repeat/return behavior. `CompiledShow` optionally owns the immutable package; Runner activates it by generation and composes private V2 layers with explicit mutually exclusive legacy/V2 `TrackScript` ownership. A bounded additive record now persists canonical rich source through authoritative Studio history/save/reopen without changing typed format-1 `AUTOLOOP`/`STEP` behavior. Deterministic AutoScript proposals commit through that same authority, and project Palette references resolve by stable swatch ID into exact/degraded output-disabled preview. Public registry IDs, output adapters, and physical evidence remain unchanged.
- Fixture qualification issue #79 now has a local software contract for active-input/candidate/binding identities, complete per-slot one-hot/blackout observations, append-only content-addressed attestations, exact marker supersession, revocation/requalification, independent unit/backend coverage, and a physical-output validation gate. The guarded operator is available through the existing installed `soundswitch_micro_probe.exe --active-test` entry and binds fixture/unit/backend/project/candidate hashes to typed acknowledgement, production-session observations, audit, post-attempt re-hash, and transactional graduation. Installed-Windows execution, all owned-unit observations, physical response, and separate CR-4 raw-to-Runner parity remain unperformed.
- The local production integration now reaches `76275de`. A standalone `ConnectionCoordinator` owns generation-stamped desired/saved/optional-active truth, separates persisted changes from effective live-graph changes, preserves pending apply authority, exposes the stopped/starting interval of a live restart, and distinguishes restored versus stopped failure outcomes with exact remaining endpoint drift. The Win32 Connections page separately consumes a toolkit-neutral DPI-aware geometry model with sticky actions, scrolling, focus reveal, Enter/default and page-scoped Alt+A; the required pure geometry matrix and a Windows-target compile pass. Coordinator-to-persistence/Runner/adapters wiring, native installed HWND/DPI/accessibility evidence, section headings, and broader shell font/minimum-size DPI debt remain open.
- Runner Static Looks now use one authoritative package- and activation-generation-stamped record with trusted owner kind/feedback token, behavior, transition status, and stale-release checks. Direct UI, MIDI, and OS2L paths share the same scheduler semantics; adapter leases prevent repeated generation-less begin events from reinterpreting delayed releases. Package activation clears conservatively, EventMoment remains the only Static Look layer, and tests prove the covered lower Autoloop advances and returns at its current phase. Rich state-broker publication, owner-loss/controller-disconnect cleanup, installed cross-surface feedback, and physical confirmation remain open; no prior-Look claim stack is implied.
- The Windows release workflow now treats the CMake install tree as authoritative, binds every payload file to the exact checked-out commit with deterministic path/size/SHA-256 evidence, verifies staged/portable/installed equivalence, retains a non-outputting installed qualification report, checks file association and uninstall cleanup, and emits format-2 release evidence. The integration is published as remote commit `b58f036` / draft PR #84, but push workflow run `31630884781` was rejected before runner assignment because GitHub reported failed account payment or an insufficient Actions spending limit. The real Windows job/artifact, Authenticode, cross-version upgrade/rollback, supported-machine coverage, and hardware qualification therefore remain open.
- The native Make test surface declares twenty-three warnings-as-errors executables, including `connection_layout_tests` and `connection_coordinator_tests`; CMake declares 29 tests total, 27 cross-platform plus two Windows-only. A never-used exact-head Make directory passes every test and all targets; generation-2 registry gates, WinMM/DMX USB syntax, a 42-frame zero-error/drop/send-failure Runner smoke, 13 package-contract checks, and a Windows-target warnings-fatal app compile plus focused PE32+ Connections link also pass. Action and Static Look status/command probes observe zero repeat-execution allocations. The exact-head benchmark measures approximately 0.56% of one core at 40 Hz and about 5.7 MiB peak RSS. Fresh exact-head ASan+UBSan core, Live UI/Static Look, Connections layout, and Ember Action result-flow tests pass; LeakSanitizer remains disabled because the execution wrapper uses ptrace. CMake itself is unavailable locally, so its declared tests await the Windows job.
- No physical/gig, installed-Windows, source-decoder fidelity, complete Action-platform, complete SoundSwitch-parity, or migration-decoder claim is made. GitHub reservation/evidence checkpoints and the exact integration tree are published through draft PR #84; PR #53/#82/#83 remain separate review lanes. No downloadable installer exists until the GitHub Actions billing/spending gate is corrected and the Windows package job passes.

## Sources consulted

- SoundSwitch, “How do I move my SoundSwitch Project to a different computer?”
- SoundSwitch, “Getting Started with Engine Lighting.”
- SoundSwitch, “What’s New in SoundSwitch 2.9?”
- SoundSwitch, “Phrase Editing.”
