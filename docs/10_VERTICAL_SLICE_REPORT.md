# Native Vertical Slice Report

Date: 2026-08-09.

## Outcome

The dependency-light native core now ships inside a coherent Windows Studio/Live testing application and installer. It is not yet hardware-qualified or safe to use as the only controller at a live event.

Implemented:

- two fixed 512-slot universe frames;
- stable numeric fixture identities, patch validation, and strict fixture-profile validation;
- fixed-capacity compiled fixture-profile storage with stable IDs, manufacturer/model/mode metadata, source provenance, hazardous-channel flags, and independent profile quarantine;
- sparse eight-level property stack with `RELEASE`, `SET`, and `FORCE_ZERO`;
- fail-closed arming for fog, haze, laser, and spark plus strobe permission/cap and intensity cap;
- broad semantic and custom-attribute lanes covering additive/subtractive colors, movement, shutter/beam, focus/zoom, effect, and hazardous functions;
- semantic 8-bit, discrete 8-bit, inverted-range, constant-channel, and 16-bit fixture rendering with explicit safe defaults;
- active-range 8-bit rendering whose released/off state can use a safe DMX default outside the active range, including literal-zero safety override;
- literal raw-DMX-zero enforcement for `FORCE_ZERO`, independent of a fixture's normal encoded range;
- baseline OS2L beat/button/command/feedback parser with bounded messages;
- bounded TCP stream decoder that handles split/concatenated JSON objects and braces inside strings;
- cross-platform direct-IP OS2L capture server with loopback-safe default;
- reusable OS2L TCP lifecycle server that remains listening across client disconnects, resets partial packets, and exposes connection/error counters;
- OS2L healthy/predictive/audio/recovery/manual/safe synchronization states;
- fixed-capacity SPSC beat delivery from adapter to scheduler, with explicit overflow accounting;
- non-droppable OS2L blackout state independent of the beat queue;
- device-agnostic MIDI mappings, multiple-device matching, curves, inversion, relative encoders, and soft takeover;
- portable WinMM short-message encode/decode with ingress timestamps, plus an isolated Windows adapter for enumeration, up to 16 simultaneous input/output ports, per-input callback queues, dropped-message counters, and short-message feedback output;
- `midi_capture` utility for listing ports and recording one, several, or all Windows MIDI inputs, ready for Control One protocol capture;
- fixture-agnostic compiled Static Looks with sparse SET/RELEASE/FORCE_ZERO assignments, interruption-safe switching, configurable fade-in, and crossfaded clearing;
- semantic Autoloops that reference generic looks rather than hard-coded RGB values;
- 64 banks of 32 Autoloops (2,048 total), a controller-neutral pageable four-bank window, deterministic next/previous selection, per-bank/exclusive/all-bank modes, duplication, and cross-bank slot swaps;
- one-shot natural return over a scripted layer, infinite repeat, track-duration repeat, progress, and completed-cycle state;
- byte-exact ArtDMX encoding and IPv4 unicast sender;
- exact ArtDMX delivery/contents verified through a real loopback UDP receiver;
- byte-exact sACN/E1.31 encoding with unicast and standards-based multicast senders;
- byte-exact ENTTEC DMX USB Pro serial framing, Windows COM discovery, one-device-per-universe mapping, bounded output-thread writes, retry/status handling, and zero-frame shutdown;
- latest-frame-wins output queue consumption so a temporarily blocked adapter does not replay stale frames after it recovers;
- end-to-end two-universe `runner_lab` with separate adapter/status and scheduling threads, dry-run default, optional Art-Net output, finite-duration smoke mode, and live status/jitter counters;
- versioned/checksummed `.emberlights` projects, atomic save/verify/replace, last-known-good backup recovery, and unknown-record preservation;
- fixture/profile/patch/group/role, Static Look, Autoloop, MIDI Learn, Connections, Safety, Live, and Diagnostics workflows in the native Windows application;
- bounded Studio-only QLC+ QXF import with OFL-export provenance detection, coarse/fine pairing, semantic/range conversion, native validation, approximation reports, and unsafe-mode quarantine;
- self-contained portable ZIP and per-user Inno Setup installer built only after Windows/Linux tests pass;
- optional QLC+ bridge boundary through Art-Net;
- zero-allocation render test and deterministic replay test.

## Verification

- Compiler: g++ 13.3.0, C++20.
- Flags: `-O2 -Wall -Wextra -Wpedantic -Werror -MMD -MP`.
- Unit tests: passed.
- Address/undefined sanitizers: passed; leak detection is unavailable in this sandbox because LeakSanitizer cannot inspect the supervised process tree.
- Show-package schema v4: syntactically valid; retains fixture provenance, represents QLC+ source and safe active-range channel encoding, and keeps explicit 32-slot Autoloop coordinates across 64 banks/2,048 loops.
- One million complete two-universe render ticks: completed without crash or state divergence.
- Loopback TCP integration: passed with concatenated `beat` and `blackout` button events.
- OS2L lifecycle integration: passed through connect, multi-event decode, incomplete message, disconnect, reconnect, and clean post-reconnect decode.
- End-to-end Runner integration: 10 TCP events at 126 BPM produced 82 frames with zero decode errors, queue drops, or send failures in dry-run mode.
- Runner smoke: one second at 40 Hz completed with 42 frames.
- Portable MIDI codec plus Linux-hosted WinMM and DMX USB Pro Windows-boundary syntax tests pass.
- Windows and Linux native compilation/tests and packaged Windows application tests passed for the preceding installed checkpoint; this change is re-gated by the repository workflow before publication.
- ENTTEC packet framing, COM1–COM256 validation, duplicate-device prevention, 40 Hz cap, project round-trip, and stale-frame supersession tests pass.
- QXF tests cover entities/DOCTYPE rejection, QLC+/OFL provenance, emitter colors, 8/16-bit pairing, reversed presets, safe active ranges, fan/haze separation, unknown lanes, switching-mode quarantine, and laser arming.

## Initial benchmark

Test load: 128 RGB fixtures split evenly across both universes, four semantic properties per fixture, one million full frame renders.

| Measurement | Observed |
| --- | ---: |
| Engine object | 875,552 bytes |
| Release binary (`core_bench`) | about 22 KB loadable sections |
| Full render tick | about 16.3 microseconds |
| Theoretical full renders/second | about 61,400 |
| Observed process max RSS | about 4.6 MB |

An additional full-path benchmark continuously ran generic Autoloop interpolation, overlapping Static Look transitions, layer resolution, and two-universe rendering across 128 fixtures with 512 assignments per look:

| Measurement | Observed |
| --- | ---: |
| Full performance update | about 141.3 microseconds |
| Estimated one-core use at 40 Hz | about 0.57% |
| Observed process max RSS | about 5.2 MB |

The performance playback path completed 20,000 scheduling updates without a heap allocation after activation.

At 40 DMX updates/second, 16.3 microseconds per render is roughly 0.065% of one CPU core for this fixture load, before sockets, UI, MIDI, OS2L, logging, and operating-system overhead. Expanding the Autoloop catalog to 2,048 entries adds roughly 16 KB of fixed catalog storage and does not run a catalog scan on the scheduling path. This is encouraging architectural evidence, not a Windows release claim.

## Known limitations

- The OS2L TCP lifecycle is implemented and loopback-tested; real VirtualDJ/Windows discovery/direct-IP soak is pending.
- The WinMM adapter and capture utility compile on Windows CI but have not been exercised with Control One; its message map has not yet been captured.
- Native DMX USB Pro output compiles on Windows CI only after publication of this slice; physical interface output and unplug/replug qualification remain pending.
- The serial adapter covers the published single-universe DMX USB Pro framing. Pro Mk2 dual-port support and third-party compatible hardware are not claimed.
- Art-Net direct unicast delivery is loopback-verified; node/visualizer qualification and discovery/subscription compliance are pending.
- Fixture mapping now covers a broad fixed semantic/custom surface and safely imports QLC+ QXF/OFL-export profiles. A searchable pinned OFL catalog, richer function-range editing, switching-channel aliases, and cell-aware multi-head/pixel topology remain pending.
- The Autoloop runtime and editor have complete 64×32 capacity plus activation/return policies, but shipped default content, richer organization, and random/automatic selection policies remain pending.
- The native Windows UI is functional but its look/profile/Autoloop authoring still needs guided controls, undo/history, and usability qualification.
- Live-audio fallback, track scripting, AutoScripting, SoundSwitch migration, Serato, smart-light integrations, and remaining parity-ledger items are still pending.

## Next gate

Qualify the installed Windows application against real DJ and lighting hardware:

1. VirtualDJ/OS2L two-hour sync and reconnect run.
2. Control One MIDI capture, mapping, feedback, and reconnect tests.
3. DMX USB Pro physical output plus unplug/replug test; inventory Joshua's other USB interfaces.
4. Art-Net/sACN receiver and QLC+ bridge verification.
5. Eight-hour installed-app soak and fault matrix.
6. Windows benchmarks on Joshua's DJ laptop and a lower-end PC.
