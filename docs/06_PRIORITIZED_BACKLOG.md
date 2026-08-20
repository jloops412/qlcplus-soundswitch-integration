# Prioritized Backlog

Priority is based on replacing SoundSwitch safely, not feature novelty.

## Current execution override — 2026-08-19

The granular backlog below remains the long-range product inventory, but it is **not the immediate agent execution order** while issue #87 Gate A is open. Read `handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md` first.

### NOW — blocking critical path

1. **P0-A / #87 — Repository convergence.** Reconcile exact `main@86694f0` with PR #86 / `agent/backyard-party-v2@7efd0b0`; preserve current SoundSwitch evidence, broad implementation, generated contracts, and artifact history; publish one validated convergence PR/commit and make `main` unambiguous.
2. **P0-B / #90 — Current SoundSwitch 2026 source adapter/import.** Implement a version-specific read-only bundle probe, exact project-local Venue map, placement/length parser, evidence-bearing A/B/Static Look records, V2 semantic reconciliation, output-disabled review/commit, and idempotent re-import. Begin with exactly `Medium / S1 / Red - Smooth Pulse`; no broad generated approximation.
3. **P0-C / #89 — OS2L/VirtualDJ reliability.** Preserve the bounded Blackout-feedback code already present on PR #86, capture real installed VirtualDJ plain/on/off/keepalive and song-transition traffic, then finish application-owned listener/discovery/reconnect lifetime. Generic `os2l_cmd` waits for P0 proof.
4. **P0-D / #52/#79 — Truthful testability and evidence.** Exact fixture qualification must not hard-block opening/importing/simulating/saving an output-disabled project. It still gates automatic output enablement, physical support claims, attestation, and gig qualification. Current destination rig is four IR-4 at U1 001/011/021/031 plus four BO-TUBE192 at U1 041/121/201/281; no duplicate generic destination uplights.
5. **P1-A / #37/#32/#33 — Replacement shell and first product slice.** Finish the measured Slint/WinUI/Safe comparison, select the production path, activate one installed product-shaped Fixtures + Static Looks workflow, and stop expanding product-facing Win32 forms.
6. **P1-B / #88 — Perceptual fade quality.** Improve transition/easing/dimmer/color smoothness at the engine/profile layer after source timing/target correctness is separated from rendering quality.

### Current beta release checkpoint — 2026-08-20

The Preview 105 candidate stacks Preview 104's application-owned OS2L service and first exact current-2026 Autoloop import slice with the Advanced-only FIX-13 manual DMX tester. The new fixture-agnostic core and installed `--manual-dmx` workflow reuse the production SoundSwitch Micro session, bind arming to one adapter/universe/timing-plan digest, support deterministic multi-channel presets, expose the exact held frame, and fail to repeated blackout plus close on timeout, cancellation, device loss, or write failure. It remains separate from normal fixture/Look vocabulary and from the sealed one-fixture `--active-test` qualification workflow.

After Joshua's installed test, keep the next work in this order:

1. reconcile native Windows/VirtualDJ evidence from Preview 103/104/105 and fix only reproduced #89 lifecycle or feedback defects;
2. run the isolated installed manual-DMX walkthrough, retain exact observed channel/range behavior, and fix only reproduced safety/transport defects;
3. compare the single imported `Red - Smooth Pulse` source slice before expanding #90 to more loops or SoundSwitch Static Looks;
4. finish #37 and activate the first modern Fixtures + Static Looks Default journey with complete profile-backed controls, modern RGBWAUV picking, patch/profile workflows, save/Undo, and raw DMX kept behind Advanced;
5. expand Autoloop/Static Look authoring UX, Live control, mappings, and then the visual `.emberskin`/Ember Actions designer over the shared registry—never as separate engines.

### Work already present on the integration lineage—inspect before rebuilding

- **UI course correction (2026-08-14):** legacy Win32 is a frozen transitional/Safe bridge; product UI proceeds through the renderer-neutral Fixtures + Static Looks replacement-shell gate, with raw DMX kept behind Advanced diagnostics.
- raw hardware/Runner frame evidence and qualification foundations;
- profile-backed fixture controls and Static Look authoring/preview;
- canonical UI/action registry foundations and UI course correction;
- Autoloops V2 source/compiler, authoring, 128-placement original starter pack, persistence, deterministic AutoScript, Studio bridge, palette realization, preview, director/runtime/Runner;
- first bounded OS2L Blackout outbound-feedback software slice;
- current SoundSwitch decoder research and failed/corrected pilot evidence.

### Explicitly deferred behind the NOW queue

WOLFMIX parsing, AI/model lighting, broad new default packs, another Autoloop/fixture/project/UI model, new product-facing Win32 editors, broad skin designer/marketplace/cloud work, proprietary Control One expansion, macOS, and new preview numbers without a meaningful validated operator slice.

---

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

## Immediate core recovery and hardware qualification gate — historical sequencing section

This section records the earlier core-recovery program. The current execution override above supersedes it as the universal first-work order. Remaining unclosed physical/support evidence still matters.

`21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` was the binding implementation program until its core-ready acceptance definition. UI/skin expansion remains subordinate to safety/output truth, but branch convergence and current-project migration now precede rebuilding already-delivered foundations.

- **CR-1 — P0:** Ship a safe standalone Hardware Test using the production SoundSwitch Micro session; prove raw channel output independently of projects and fixture profiles.
- **CR-2 — P0:** Harden the Micro open/init/settle/blackout-warm-up/stream/reconnect/blackout-close lifecycle with exact stage telemetry and golden tests.
- **CR-3 — P0:** Add bounded frame/channel/value/source inspection and make the conservative SoundSwitch migration patch visibly unverified until physical addresses, modes, and profiles are confirmed.
- **CR-4 — P0:** Build and qualify one exact one-fixture bench project; compare its Runner frame byte-for-byte with a successful raw Hardware Test frame.
- **CR-5 — P0:** Repair Connections so Save & Apply is always visible at supported sizes/DPI; persist desired state and report Saved, Applied, and Active outcomes truthfully.
- **CR-6 — P0:** Make VirtualDJ direct-IP startup deterministic with a copyable ONINIT action, persistent listener lifecycle, actionable OS2L states, and reconnect tests.
- **CR-7 — P0/P1:** Implement Static Look Toggle/Hold through shared commands with activation ownership, authoritative feedback, EventMoment priority, and background Autoloop continuation/return tests.
- **CR-8 — P1:** Extract the first ConnectionCoordinator/DjTransportService/OutputRouter boundaries without delaying raw hardware proof; preserve deterministic Runner and newest-frame-wins output.
- **CR-9 — P1:** Profile and reduce the observed scheduler jitter after physical output is proven; pass a ten-minute hardware stream before the eight-hour qualification gate.
- **CR-10 — Gate:** Do not claim SoundSwitch Micro support from device-open or accepted-write counters. Physical response, blackout, repeat open, and unplug/replug evidence are required.

## Cross-cutting UI/UX and skin spine

Start UI work from `UI_PROGRAM_START_HERE.md`. `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md` and `21_UI_IMPLEMENTATION_PROGRAM.md` are binding. Planning, evidence capture, behavior-preserving command/state extraction, and bounded toolkit spikes may proceed beside the current core/import gates; broad Default/Reference implementation waits for the replacement-shell/runtime acceptance path.

- **U1 — P0 / #31:** Complete the current Win32 `Page`/`ControlId`/callback inventory and route existing behavior through stable typed command/state facades. Preserve the non-droppable blackout path and publish explicit invocation outcomes, state update classes, Command Explorer, State Explorer, and the legacy-bypass ledger.
- **U2 — P1 / #31:** Route compatible keyboard, MIDI, controller, and external actions through the same command model; validate arguments, availability, safety, persistence, Undo behavior, feedback, conflicts, soft takeover, and reconnect/package changes.
- **U3 — P1 / #37:** Run the product-shaped UI toolkit spike: Slint/C++ first, WinUI 3 control comparison, Win32/Direct2D Safe baseline, and Qt only if evidence triggers it. Measure Safe, Default Live/Studio, Reference Live/Studio, accessibility, DPI, deployment, memory, CPU, repaint, startup, and scheduler effect before accepting a production toolkit.
- **U4 — P1 / #32:** Implement the versioned `.emberskin` package/runtime, strict validator and resource limits, immutable responsive view graph, semantic themes, ordinary primitives, native-component adapters, visibility-aware state subscriptions, transactional activation, cache/provenance, and trusted Safe fallback.
- **U5 — P2 / #33:** Re-express all currently supported functionality as `EmberLights Default v0`: Project Hub, Studio, Live Home/Autoloops/Static Looks/Moments/Overrides, contextual Inspector, utility drawers, pinned health/safety, complete product journeys, original EmberLights design system, and explicit bounded legacy bridges.
- **U6 — P2 / #30 + #34:** Complete the native SoundSwitch screenshot/state/measurement corpus and ship `SoundSwitch Reference v0` with familiar Studio/Performance information architecture, original EmberLights assets, evidence-tagged tokens, explicit Preserve/Improve/Reject deviations, and parity journeys over the same domain behavior.
- **U7 — P2 / #35:** Finish app-local, project-authored, controller-profile, and live-transient persistence scopes; last-project reopening; immediate persistence where safe; exact Save/Apply/Reconnect/Restart outcomes; auto-connect/reconnect; and actionable DJ/controller/U1/U2/USB/safety drawers shared by both skins.
- **U8 — Gate / #36:** Qualify registries, bindings, package abuse limits, native components, both skins, Safe fallback, goldens, 1366×768 through 4K/high DPI, keyboard/UI Automation/accessibility, persistence, faults, cross-surface equivalence, startup/memory/CPU/repaint/jitter, and skin-switch/failure DMX continuity with machine-readable evidence.
- **U9 — P3 / #39:** After the qualified platform, add VirtualDJ-style customization safely: keyboard/MIDI/HID binding editor, bounded custom Button/Toggle/Pad/Fader/Knob/Status panels and pad pages, typed command/state selection, immutable base skin plus validated overlays, import/export/reset/relink, and mandatory-control reachability.
- **U10 — P4:** Add approved layout overlays and later a full visual skin designer only after the two bundled skins, Safe runtime, compatibility rules, accessibility, signing/provenance, and qualification evidence are stable. Public sharing/marketplace remains a separate later product decision.

## P0 — Preserve the plan and prove the core

The numbered inventory below records product foundations and later completeness work; it is not a claim that every item is still unimplemented.

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
34. Named color palettes.
35. Named position and attribute cues.
36. Static Look authoring and sparse playback.
37. Scalable Autoloop library, 32-slot banks, pageable controller windows, lengths, fades, and manual selection. Native 64-bank/2,048-loop catalog, project compiler, editor, Live selection, and four-bank window foundation complete; shipped content and richer organization remain.
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
70. Deterministic AutoScripting styles.
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
