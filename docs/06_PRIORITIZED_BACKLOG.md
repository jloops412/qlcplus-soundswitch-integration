# Prioritized Backlog

Priority is based on replacing SoundSwitch safely, not feature novelty.

## Active integration checkpoint — 2026-08-12

- **Main registry spine merged and extended compatibly:** PR #81 landed at `47bb45d`. Local integration commit `18c4d5d` advances the canonical contract to registry 1.1.0/generation 2/digest `d3f5c6ed...` with five planned non-callable components, one accepted non-callable Static Looks capability, and eleven reusable value/unit/target contracts. The accepted 29 commands, 39 states, native ordinals, and runtime behavior remain exact; the machine diff is compatible-additive with zero breaking changes/removals.
- **Autoloops V2 local-green checkpoint:** PR #82 / issues #58–#60 now have the normalized 960-PPQ source model, immutable used-content compiler/evaluator, transactional source authoring, dependency-safe 64×32 placements, an original deterministic 128-placement EmberLights starter pack, allocation-free Autonomous/Scripted/Manual director, optional immutable `CompiledShow` package, and generation-controlled Runner activation/status/commands (`074b4df`). Canonical rich source now persists through a bounded version-1 record with authoritative Studio generation/stamp transactions, Undo/Redo, and save/reopen (`5f79eb5`). Deterministic bounded AutoScript proposals commit through that same document authority (`d083226`, `6b26c64`), and exact stable-ID Palette references resolve into output-disabled production preview with explicit exact/degraded/fail-closed evidence (`1ad93db`). Trustworthy TrackDuration epoch, Position/Attribute/Movement/Effect and legacy/transition resolution, migration decoding, broad UI, installed Windows, and final-DMX evidence remain bounded follow-on work.
- **Ember Actions local-green checkpoint:** PR #83 / issue #65 now has the bounded reader/semantic validator, deterministic generation-2 registry adapter, immutable IR/cache identity, and executable `Sequence` / `InvokeCommand` / `If` / `Switch` / `OnResult` / `Return` (`24626aa`). Literal/declared-parameter predicates and selectors plus prior-command result routing have deterministic trace, cancellation, honest worst-case node/dispatch/depth/expression budgets, zero-allocation repeat execution, and copied-IR integrity/metric checks before dispatch. State/context reads, `Let`, `MapValue`, `Parallel`, nested Actions, activation/UI wiring, Studio transactions, async operations, and expert round-trip remain open; direct device/file/network/state-write/timing paths remain prohibited.
- **Studio local-green checkpoint:** PR #53 / issue #46 was restored and local commits `9fe2634` + `f4afec4` add rich semantic color authoring, compiled no-output Look/format-1 Autoloop preview, and bounded versioned project-owned palettes with generation-checked transactions and Undo/Redo. Local commits `5b25992` and `1ad93db` extend the same structurally output-disabled service with exact V2 compiler/evaluator/renderer preview, bounded transport/digest trace, and stable-swatch Palette realization with exact/degraded/missing/ambiguous/unsupported evidence and last-good retention. The production picker/canvas/timeline, toolkit integration, Position/Attribute/Movement/Effect and legacy/transition matrices, source-decoder fidelity, fixture truth, and physical output remain open.
- **Fixture graduation local-green checkpoint:** issue #79 / integration commits `a963e02` and `62c895f` provide the dedicated single-fixture Raw Hardware Test plus an evidence-bound operator behind the existing installed `soundswitch_micro_probe.exe --active-test` surface. It binds a strict manifest and typed acknowledgement to fixture/unit/backend/project basis/candidate-file hashes, uses only the production SoundSwitch Micro session, audits every terminal attempt, re-hashes before graduation, and never treats accepted writes alone as success. Installed-Windows execution, real observations for both IR-4s, tubes, Wash FX Hex and the owned Micro, plus separate CR-4 raw-to-Runner parity remain operator gates; no physical claim was made.
- **Combined integration green:** `lane/production-integration` through feature commit `76275de` combines registry generation 2, Studio color/palettes/V2 preview, rich Autoloop persistence/AutoScript/Palette realization, bounded Ember Action control/result flow, Autoloops V2 Runner activation, evidence-bound fixture qualification, truthful Connections state plus a DPI-safe scrollable/sticky Win32 surface, generation-safe Static Look ownership, and deterministic Windows package evidence. A never-used `build-release-76275de` directory passed all twenty-three warnings-as-errors native test executables plus all targets, the generation-2 29-command/39-state registry and Action gates, WinMM/DMX USB syntax, and Runner smoke at 42 frames with zero decode errors, dropped beats, or send failures. Thirteen package-contract checks pass; Action probes remain allocation-free; the exact-head benchmark is approximately 0.56% of one core at 40 Hz with about 5.7 MiB peak RSS. A Windows-target warnings-fatal `windows_app.cpp` compile and focused Connections PE32+ link pass; fresh exact-head ASan+UBSan core, Live UI/Static Look, Connections layout, and Action result-flow tests pass with LeakSanitizer disabled only for the ptrace environment. CMake declares twenty-nine tests but is unavailable locally. The branch is local-only because the required `gh` publish prerequisite is absent; GitHub issue checkpoints are partially current, PR #53/#82/#83 remote code heads remain unchanged, and no Windows artifact exists for this head yet.
- **Canonical skins program:** issue #63 owns the program. #64 registry, #65 Actions, #66 bindings/custom controls/overlays, #67 full Designer, #68 SoundSwitch-familiar Reference kit, #70 migration adapters, and #72 package/trust/qualification are the canonical packages. Duplicate planning issues #69/#71/#73/#74/#75 are closed with their detail preserved and mapped to these owners.
- **Safety gate unchanged:** physical SoundSwitch Micro/Control One/fixture evidence and installed-Windows qualification remain operator gates. No source merge, content-addressed software attestation, or accepted USB write is physical/gig qualification.

## Fixture truth and Static Look builder — issue #52

The current implementation checkpoint is `32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md`. It keeps the persisted/runtime entity as `Look` while delivering the first capability-aware Static Scene/Look workflow. Software-complete items below still require merge/installed-Windows and physical-hardware evidence where noted.

- **FX-1 — Implemented:** Canonicalize the manual-backed Both Lighting BO-IR4 6CH/10CH identities, source revision, and behavior SHA-256; quarantine unmodeled 10CH program/color-speed channels at a safe constant zero.
- **FX-2 — Implemented:** Share fixture/group capability inspection across Live and Studio; reject unsupported Look assignments rather than silently ignoring them.
- **SL-1 — Implemented:** Replace editable Look CSV with target/property ownership controls, an RGB picker, direct RGBWAUV/Master values, pure-emitter swatches, deterministic group expansion, and full-color ownership.
- **SL-2 — Implemented:** Build exact offline DMX preview through the production compiler/renderer with fixture/profile/mode/revision, winning-layer, byte trace, warnings, and frame SHA-256.
- **SL-3 — Implemented foundation / P1 integration:** Static Look drafts commit through `StudioDocumentService` in one transaction; migrate the remaining Win32 document mirror, Undo/Redo, New/Open/Save, and legacy editors to that authority.
- **FX-3 — Implemented:** Validate Look count/name/fade/assignment capacity, duplicate names, all-Release no-ops, closed masters, and fixture-property support; include MIDI in Look deletion dependency reports.
- **FX-4 — P0 gate:** Physically qualify both owned IR-4 fixtures in 6CH and 10CH with pure emitter frames, verified mode/address, safe channel 8–10 behavior, blackout, reconnect, and saved evidence.
- **SL-4 — P0:** Add a selected-fixture-only physical preview with Live-stopped interlock, reduced default scale, bounded timeout, explicit Stop, and fail-closed blackout on every exit/fault path.
- **FX-5 — P1:** Add named channel functions/ranges plus explicit neutral/open/home/blackout/highlight values. Do not expose unknown wheel, macro, program, switching, or service functions as generic linear sliders.
- **FX-6 — P1:** Add virtual intensity for dimmerless direct-emitter fixtures so master/safety intensity can scale IR-4 6CH without corrupting raw qualification mode.
- **FX-7 — P1:** Persist structured profile evidence, immutable revisions, qualification state, and invalidation on behavior hash/profile/mode/universe/address changes.
- **FX-8 — P1:** Build a separate Studio fixture catalog/index with a pinned OFL exporter adapter, provenance/license manifest, immutable embedded snapshots, and quarantine. Never load the global catalog into the fixed-capacity Runner library.
- **FX-9 — P2:** Add revisioned, measured color calibration/gamma/white balance/emitter limits. Until then, the RGB picker controls RGB only and White/Amber/UV remain direct.
- **FX-10 — P2:** Add multi-cell, multi-head, switching-channel, wheel, matrix, and richer 16-bit/function conformance coverage.

## Immediate core recovery and hardware qualification gate

`21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` is the binding implementation program until its core-ready acceptance definition passes on Joshua's owned Windows hardware. UI/skin expansion is subordinate to these gates.

- **CR-1 — Implemented software workflow / P0 physical gate:** The dedicated Raw Hardware Test uses the production SoundSwitch Micro session behind a one-fixture-only transport boundary and internally generates only blackout or exact-footprint one-hot frames. The installed `--active-test` compatibility entry now routes through the strict manifest/acknowledgement/audit/graduation operator; installed-Windows verification and owned-hardware observations remain required before physical qualification.
- **CR-2 — Session lifecycle foundation implemented / P0 telemetry and qualification:** The shared Micro session performs the exact inspect/init/200 ms settle/50-frame blackout warm-up/stream/blackout-close sequence and is used by Runner and the raw operator. Add an injectable transport seam covering every stage failure, enter and expose `Recovering` plus descriptors/warm-up telemetry, prove newest-frame/no-stale reconnect and blackout-write failure, then pass installed ten-cycle/unplug/endurance evidence.
- **CR-3 — Migration warning and offline Look trace implemented / P0 live inspection:** Migrated patches already report `MIGRATED_PATCH_UNVERIFIED`, qualification requires evidence, and offline Static Look preview includes semantic channel attribution and frame SHA-256. Add a bounded snapshot of the actual last Runner and routed pre/post-blackout output frame, selected adapter/route, and winning source across all active layers/defaults/constants.
- **CR-4 — Exact IR-4 comparator and isolated raw operator implemented / P0 installed parity:** The one-fixture U1/A1 6CH project/comparator and evidence-bound raw workflow prove fixed offline Red/JLS1 equivalence. Package a complete Blackout/R/G/B/W/A bench asset, bind a successful installed raw attempt to the actual reopened Runner output snapshot from CR-3, retain byte/packet diffs, and qualify it on owned hardware.
- **CR-5 — Coordinator and visible layout foundations implemented / P0 production integration:** The bounded coordinator owns generation-stamped desired/saved/optional-active truth and exact restart/failure boundaries. The Win32 Connections page now has tested DPI-aware sticky Refresh/Save & Apply actions, vertical scrolling, focus reveal, Enter/default and page-scoped Alt+A at the required geometry matrix. Route Win32 validation, atomic persistence, Runner blackout/close/restart, and user-visible Saved/Applied/Active outcomes through the coordinator; add section headings plus native installed-Windows HWND/DPI/accessibility and failure-path evidence.
- **CR-6 — Discovery/fallback foundation implemented / P0 lifecycle evidence:** Windows DNS-SD `_os2l._tcp` advertisement and the reserved no-op `EmberLights Keepalive` direct-IP fallback exist with separate status/error reporting. Move listener ownership outside Start Show, add authoritative outbound feedback, and qualify automatic discovery, fallback, all launch orders, restart/reconnect, and port-conflict behavior on installed Windows with VirtualDJ.
- **CR-7 — Owner/generation-safe core implemented / P0/P1 integration evidence:** Runner now owns one package- and activation-generation-stamped Static Look record with trusted owner feedback identity, atomic Toggle/Hold/Clear, same-Look multi-source stale-release safety, conservative package-change clearing, authoritative transition state, and explicit EventMoment-over-advancing-Autoloop return tests across direct/UI/MIDI/OS2L paths. Publish the richer ownership/transition state through the canonical broker, add controller-disconnect/owner-loss cleanup and installed UI/MIDI/Control One/physical confirmation; do not add a claim stack.
- **CR-8 — ConnectionCoordinator foundation implemented / P1 integration:** Complete production wiring of the isolated coordinator, then extract replaceable `DjTransportService` and `OutputRouter` boundaries while preserving deterministic Runner and newest-frame-wins output. The current coordinator opens no adapters, persists no files, and has no Runner, Win32 UI, or registry dependency.
- **CR-9 — P1:** Profile and reduce the observed scheduler jitter after physical output is proven; pass a ten-minute hardware stream before the eight-hour qualification gate.
- **CR-10 — Gate:** Do not claim SoundSwitch Micro support from device-open or accepted-write counters. Physical response, blackout, repeat open, and unplug/replug evidence are required.

## Cross-cutting UI/UX and skin spine

Start UI work from `UI_PROGRAM_START_HERE.md`. `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md` and `21_UI_IMPLEMENTATION_PROGRAM.md` are binding. Planning, evidence capture, behavior-preserving command/state extraction, and bounded toolkit spikes may proceed beside the core gate; broad Default/Reference implementation waits for issue #38's core-ready acceptance.

- **U1 — P0 / #31:** Complete the current Win32 `Page`/`ControlId`/callback inventory and route existing behavior through stable typed command/state facades. Preserve the non-droppable blackout path and publish explicit invocation outcomes, state update classes, Command Explorer, State Explorer, and the legacy-bypass ledger.
- **U2 — P1 / #31:** Route compatible keyboard, MIDI, controller, and external actions through the same command model; validate arguments, availability, safety, persistence, Undo behavior, feedback, conflicts, soft takeover, and reconnect/package changes.
- **U3 — P1 / #37:** Run the product-shaped UI toolkit spike: Slint/C++ first, WinUI 3 control comparison, Win32/Direct2D Safe baseline, and Qt only if evidence triggers it. Measure Safe, Default Live/Studio, Reference Live/Studio, accessibility, DPI, deployment, memory, CPU, repaint, startup, and scheduler effect before accepting a production toolkit.
- **U4 — P1 / #32:** Implement the versioned `.emberskin` package/runtime, strict validator and resource limits, immutable responsive view graph, semantic themes, ordinary primitives, native-component adapters, visibility-aware state subscriptions, transactional activation, cache/provenance, and trusted Safe fallback.
- **U5 — P2 / #33:** Re-express all currently supported functionality as `EmberLights Default v0`: Project Hub, Studio, Live Home/Autoloops/Static Looks/Moments/Overrides, contextual Inspector, utility drawers, pinned health/safety, complete product journeys, original EmberLights design system, and explicit bounded legacy bridges.
- **U6 — P2 / #30 + #34:** Complete the native SoundSwitch screenshot/state/measurement corpus and ship `SoundSwitch Reference v0` with familiar Studio/Performance information architecture, original EmberLights assets, evidence-tagged tokens, explicit Preserve/Improve/Reject deviations, and parity journeys over the same domain behavior.
- **U7 — P2 / #35:** Finish app-local, project-authored, controller-profile, and live-transient persistence scopes; last-project reopening; immediate persistence where safe; exact Save/Apply/Reconnect/Restart outcomes; auto-connect/reconnect; and actionable DJ/controller/U1/U2/USB/safety drawers shared by both skins.
- **U8 — Gate / #36:** Qualify registries, bindings, package abuse limits, native components, both skins, Safe fallback, goldens, 1366×768 through 4K/high DPI, keyboard/UI Automation/accessibility, persistence, faults, cross-surface equivalence, startup/memory/CPU/repaint/jitter, and skin-switch/failure DMX continuity with machine-readable evidence.
- **U9 — P3 / #39:** After the qualified platform, add VirtualDJ-style customization safely: keyboard/MIDI/HID binding editor, bounded custom Button/Toggle/Pad/Fader/Knob/Status panels and pad pages, typed command/state selection, immutable base skin plus validated overlays, import/export/reset/relink, and mandatory-control reachability.
- **U10 — P1 / #63 + #64:** Extend the merged command/state registry spine into canonical capability, component, interaction, theme, value-schema, result, and package cross-reference families without introducing planned-only callable behavior.
- **U11 — P1 / #65:** Complete Ember Action generated-registry adaptation, immutable IR, bounded executor/trace, deterministic expert round-trip, and abuse/performance tests without making Actions a script VM, timing engine, or direct device/state path.
- **U12 — P2 / #66:** Build the first visual customization milestone: shared binding editor, custom controls, overlays, pad pages, MIDI/HID Learn/feedback, import/export/reset, and mandatory-control reachability over the same registry/actions.
- **U13 — P3 / #67:** Build the full responsive visual Skin Designer only after #32 runtime, #37 toolkit, #64 registry, #65 Action IR, and #66 overlay/editor seams are proven. Keep Designer source toolkit-neutral and compile a lean immutable Perform view graph.
- **U14 — P2 / #68:** Produce original-asset, SoundSwitch-familiar Performance templates and a forkable Reference authoring kit over shared behavior; exact visual freeze still depends on #30 evidence.
- **U15 — P3 / #70:** Add source-preserving skin/mapping/action migration IR with the evidence-bounded VirtualDJ adapter first. Foreign formats and scripts never enter the Runner as alternate engines.
- **U16 — Gate / #72:** Qualify package lifecycle, compatibility preflight, provenance/trust, recovery, and release behavior before public sharing. Public marketplace remains a separate later product decision.

## P0 — Preserve the plan and prove the core

1. Maintain the decision ledger and handoff package.
2. Define stable fixture, property, layer, transport, mapping, and package contracts.
3. Build sparse per-property resolver with `SET`, `RELEASE`, and `FORCE_ZERO`.
4. Build two-universe fixed-frame renderer.
5. Add patch overlap/range/profile validation.
6. Encode ArtDMX packets from the official Art-Net specification.
7. Parse baseline OS2L beat/button/command events.
8. Implement clock healthy/hold/fallback/recovery states.
9. Define device-agnostic MIDI messages and actions.
10. Implement scaling, inversion, curves, relative encoders, and soft takeover.
11. Implement one semantic Autoloop pattern engine.
12. Add zero-allocation render-path test.
13. Add deterministic replay and million-tick benchmark.
14. Define QLC+ bridge boundary.
15. Define portable show package schema.

## P1 — Real VirtualDJ, MIDI, and network output

16. OS2L TCP server lifecycle. Reconnecting cross-platform listener and loopback lifecycle test complete; VirtualDJ/Windows soak remains.
17. OS2L direct-IP and discovery workflow.
18. OS2L feedback events.
19. VirtualDJ transport test mapper for DDJ-REV7 workflow.
20. WinMM MIDI device enumeration/input/output. Isolated adapter, short-message codec, enumeration, input callbacks, output feedback, and capture CLI are implemented; Windows compile/hardware qualification remains.
21. Multiple simultaneous MIDI ports. Fixed-capacity 16-input/16-output adapter with independent per-input queues is implemented; Windows device/hot-plug stress remains.
22. MIDI Learn capture and conflict detection.
23. Control One message capture utility. Cross-platform CLI exists and becomes active through WinMM on Windows; owned-device capture remains.
24. Control One bundled profile v0.
25. Art-Net unicast sender and receiver diagnostics. Exact loopback UDP delivery test and lab Runner sender complete; node/visualizer diagnostics remain.
26. ArtPoll/subscription discovery compliance.
27. sACN/E1.31 sender. Byte-exact unicast/multicast sender and loopback tests complete; physical receiver qualification remains.
    - First native published-protocol USB output: ENTTEC DMX USB Pro serial framing, Windows COM discovery, per-universe output/reconnect state, and latest-frame-wins backpressure are implemented; physical hardware and broader USB matrix remain.
28. QLC+ local bridge example project/configuration.
29. Same-PC versus LAN equivalence tests. Endpoint-neutral lab Runner exists; Windows/VirtualDJ comparison remains.
30. Minimal Runner status/blackout UI. Native Live/Diagnostics pages, non-droppable blackout, work light, adapter states, counters, and jitter display complete; usability/hardware qualification remains.

## P2 — Gig-safe SoundSwitch-style workflow

31. Fixture/venue authoring source model.
32. Versioned OFL Studio adapter, stable-profile compiler, provenance, and profile quarantine. Native compiled-store/quarantine foundation plus a bounded QLC+ QXF/OFL-export ingestion path are complete; direct pinned OFL adapter, offline corpus, search/index UI, and conformance qualification remain.
33. Fixture groups, categories, roles, and stable identities.
34. Named color palettes. Bounded versioned project-owned palette assets, deterministic persistence, transactional edits, no-output swatch preview, and stable-ID Autoloop V2 Palette realization through `realize_studio_color` are local-green; production UI/toolkit exposure and measured fixture calibration remain.
35. Named position and attribute cues.
36. Static Look authoring and sparse playback.
37. Scalable Autoloop library, 32-slot banks, pageable controller windows, lengths, fades, and manual selection. Native 64-bank/2,048-loop catalog, project compiler, editor, Live selection, and four-bank window foundation are complete. The local-green V2 path adds normalized and persisted rich source, deterministic immutable used-content arenas/evaluator, transactional placement authoring, an original 128-placement starter pack, Runner-owned director/activation with explicit V1/V2 layer ownership, exact output-disabled Studio preview, stable-ID Palette realization, and deterministic bounded AutoScript proposal/commit through Studio history. TrackDuration epoch, Position/Attribute/Movement/Effect and legacy/transition resolution, migration decoding, broad UI, and installed/physical qualification remain.
38. Manual color, movement, position, intensity, strobe, white, and UV controls.
39. Blackout, work light, and recovery look.
40. Fog/haze explicit arming and timeout.
41. Strobe and movement safety caps.
42. Crossfader modes and per-attribute transitions.
43. Immutable package compiler and checksum validation.
44. Atomic hot-swap and last-known-good package.
45. Structured diagnostics/event log and export.
46. Output/controller disconnect recovery.
47. On-demand audio fallback worker.
48. Manual tap-tempo fallback.
49. Eight-hour soak/fault suite. Installed 128-fixture qualification tool, p99/deadline/resync health metrics, JSON evidence, and CI smoke are implemented; eight-hour Windows runs and full adapter/system fault injection remain.
50. Shadow-mode frame comparison against SoundSwitch.

## P3 — Track-specific parity and migration

51. Layered track resolver using DJ identity, fingerprint/hash, metadata, and duration.
52. Exact playhead/seek/loop/reverse adapter path.
53. One-to-four-deck normalized mixer state.
54. Track timeline and semantic cue curves.
55. Master, group, and fixture script tracks.
56. Venue adaptation and fixture replacement without cue deletion.
57. `.ssproj` container inspector.
58. Venue/fixture/patch/group importer.
59. Static Look/position/attribute/Autoloop importer.
60. Packaged lighting-file inspector.
61. Copied audio metadata scanner.
62. Lossless source bundle and unknown-field retention.
63. Migration confidence report and manual conflict resolver.
64. Script version history and relinking.

## P4 — Studio and automation

65. Native Studio shell and project browser.
66. Waveform cache and editor.
67. Beatgrid editing and validation.
68. Phrase/section/energy analysis.
69. Manual timeline authoring tools.
70. Deterministic AutoScripting styles. A bounded version-1 rule-based proposal/preview/commit foundation with explicit seed, section/energy inputs, hard work/content limits, cancellation, provenance/digests, semantic editable output, and authoritative Studio Undo/save/reopen is local-green; production UI orchestration, richer styles/analysis inputs, bulk generation, and quality evaluation remain.
71. Bulk analysis and generation.
72. Visual fixture/venue preview.
73. Custom presets and style profiles.
74. Offline AI suggestions that compile before performance.

## P5 — Optional event-workflow superiority

These templates organize the event-agnostic core; none becomes a required engine mode.

75. Ceremony, cocktail, dinner, toasts, formal dances, open dance, last dance, and send-off modes.
76. Grand-entrance sequence builder.
77. Photographer-safe mode.
78. Venue work-light/breakdown mode.
79. Couple/brand palette import.
80. Love & Light portal timeline integration.
81. Venue spatial targets.
82. Client-approved restrictions.
83. Authenticated tablet/phone control.
84. Backup Runner and seamless takeover.
85. Post-V1 universe expansion.

## Deferred/experimental

- Proprietary Control One OLED, internal DMX, or onboard storage.
- Serato implementation until VirtualDJ V1 is stable; Serato is accepted as the second direct DJ integration.
- Hue/Nanoleaf/Ableton Link/MTC.
- Lasers, sparks, and other hazardous effects beyond explicit safety adapters.
- Public marketplace, subscriptions, or cloud accounts.
