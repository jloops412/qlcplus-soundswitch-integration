# Prioritized Backlog

Priority is based on replacing SoundSwitch safely, not feature novelty.

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
27. sACN/E1.31 sender.
28. QLC+ local bridge example project/configuration.
29. Same-PC versus LAN equivalence tests. Endpoint-neutral lab Runner exists; Windows/VirtualDJ comparison remains.
30. Minimal Runner status/blackout UI. Console laboratory status and non-droppable blackout path complete; production performance UI remains.

## P2 — Gig-safe SoundSwitch-style workflow

31. Fixture/venue authoring source model.
32. Versioned OFL Studio adapter, stable-profile compiler, provenance, and profile quarantine. Native compiled-store/quarantine foundation complete; upstream adapter and corpus remain.
33. Fixture groups, categories, roles, and stable identities.
34. Named color palettes.
35. Named position and attribute cues.
36. Static Look authoring and sparse playback.
37. Scalable Autoloop library, 32-slot banks, pageable controller windows, lengths, fades, and manual selection. Native 64-bank/2,048-loop catalog and four-bank window foundation complete; package compiler/UI remain.
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
49. Eight-hour soak/fault suite.
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
