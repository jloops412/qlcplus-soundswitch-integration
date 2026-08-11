# Prioritized Backlog

Priority is based on replacing SoundSwitch safely, not feature novelty.

## Cross-cutting UI/UX and skin spine

`18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md` is binding for all UI work. These items cut across the milestone bands below and should be implemented before large amounts of additional hard-coded UI accumulate.

- **U1 — P0:** Inventory existing Studio/Live callbacks and define stable typed command and state registries for every current user action/status.
- **U2 — P1:** Route compatible keyboard/MIDI/controller actions through the shared command model; add command metadata, validation, and safe feedback/state lookup.
- **U3 — P1:** Define the versioned `.emberskin` package/schema, validator, semantic theme tokens, responsive layout rules, and safe fallback skin.
- **U4 — P2:** Re-express the current minimal UI as `EmberLights Default v0` through the skin path; add Command Explorer/control introspection and connection/persistence scope UX.
- **U5 — P2:** Ship `SoundSwitch Reference v0` using original EmberLights assets but familiar SoundSwitch Studio/Performance information architecture; parity QA must be able to exercise implemented SoundSwitch-equivalent workflows through it.
- **U6 — P2:** Persist last-opened project, chosen skin/layout locally; make logical project connection/output settings durable and connection changes immediately persisted with explicit reconnect/restart messaging rather than ambiguous Apply/Save behavior.
- **U7 — P2:** Auto-connect/reconnect configured DJ sources, MIDI, and DMX outputs; expose actionable health in the Live status strip without requiring a mode-changing settings flow.
- **U8 — P3:** Add user-customizable buttons/pads/faders/knobs and shared binding editor comparable in spirit to VirtualDJ custom controls/pad pages.
- **U9 — P4:** Add visual skin/layout designer, responsive variants, skin export/import, and validation-before-activation.
- **U10 — Gate:** Benchmark both bundled skins at 1366×768, 1920×1080, high DPI, and wide layouts before accepting the production UI toolkit; skin/UI work cannot violate Runner CPU/memory/jitter ceilings.

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
    - First native published-protocol USB output: ENTTEC DMX USB Pro serial framing, Windows COM discovery, per-universe mapping, reconnect/status, and stale-frame suppression implemented; physical hardware and broader USB matrix remain.
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
