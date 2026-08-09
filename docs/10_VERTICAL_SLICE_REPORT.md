# Native Vertical Slice Report

Date: 2026-08-08.

## Outcome

The first dependency-light native reference core compiles and runs. It is not yet a Windows hardware build or a live-gig application.

Implemented:

- two fixed 512-slot universe frames;
- stable numeric fixture identities, patch validation, and strict fixture-profile validation;
- fixed-capacity compiled fixture-profile storage with stable IDs, manufacturer/model/mode metadata, source provenance, hazardous-channel flags, and independent profile quarantine;
- sparse eight-level property stack with `RELEASE`, `SET`, and `FORCE_ZERO`;
- fail-closed arming for fog, haze, laser, and spark plus strobe permission/cap and intensity cap;
- broad semantic and custom-attribute lanes covering additive/subtractive colors, movement, shutter/beam, focus/zoom, effect, and hazardous functions;
- semantic 8-bit, discrete 8-bit, inverted-range, constant-channel, and 16-bit fixture rendering with explicit safe defaults;
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
- end-to-end two-universe `runner_lab` with separate adapter/status and scheduling threads, dry-run default, optional Art-Net output, finite-duration smoke mode, and live status/jitter counters;
- optional QLC+ bridge boundary through Art-Net;
- zero-allocation render test and deterministic replay test.

## Verification

- Compiler: g++ 13.3.0, C++20.
- Flags: `-O2 -Wall -Wextra -Wpedantic -Werror -MMD -MP`.
- Unit tests: passed.
- Address/undefined sanitizers: passed; leak detection is unavailable in this sandbox because LeakSanitizer cannot inspect the supervised process tree.
- Show-package schema v3: syntactically valid; retains fixture provenance and expands explicit 32-slot Autoloop coordinates to 64 banks/2,048 loops.
- One million complete two-universe render ticks: completed without crash or state divergence.
- Loopback TCP integration: passed with concatenated `beat` and `blackout` button events.
- OS2L lifecycle integration: passed through connect, multi-event decode, incomplete message, disconnect, reconnect, and clean post-reconnect decode.
- End-to-end Runner integration: 10 TCP events at 126 BPM produced 82 frames with zero decode errors, queue drops, or send failures in dry-run mode.
- Runner smoke: one second at 40 Hz completed with 42 frames.
- Portable MIDI codec and non-Windows adapter-boundary tests pass; the WinMM branch awaits Windows CI/hardware execution.

## Initial benchmark

Test load: 128 RGB fixtures split evenly across both universes, four semantic properties per fixture, one million full frame renders.

| Measurement | Observed |
| --- | ---: |
| Engine object | 875,552 bytes |
| Release binary (`core_bench`) | about 22 KB loadable sections |
| Full render tick | about 16.4 microseconds |
| Theoretical full renders/second | about 60,900 |
| Observed process max RSS | about 4.5 MB |

An additional full-path benchmark continuously ran generic Autoloop interpolation, overlapping Static Look transitions, layer resolution, and two-universe rendering across 128 fixtures with 512 assignments per look:

| Measurement | Observed |
| --- | ---: |
| Full performance update | about 134.6 microseconds |
| Estimated one-core use at 40 Hz | about 0.54% |
| Observed process max RSS | about 5.2 MB |

The performance playback path completed 10,000 scheduling updates without a heap allocation after activation.

At 40 DMX updates/second, 16.4 microseconds per render is roughly 0.066% of one CPU core for this fixture load, before sockets, UI, MIDI, OS2L, logging, and operating-system overhead. Expanding the Autoloop catalog to 2,048 entries adds roughly 16 KB of fixed catalog storage and does not run a catalog scan on the scheduling path. This is encouraging architectural evidence, not a Windows release claim.

## Known limitations

- Linux test environment only; no Windows cross-compiler is installed here.
- The OS2L TCP lifecycle is implemented and loopback-tested; real VirtualDJ/Windows discovery/direct-IP soak is pending.
- The WinMM adapter and capture utility are implemented but not yet compiled or exercised on Windows; Control One data has not yet been captured.
- No real sACN or USB-DMX adapter yet.
- Art-Net direct unicast delivery is loopback-verified; node/visualizer qualification and discovery/subscription compliance are pending.
- Fixture mapping now covers a broad fixed semantic/custom surface and safely compiles normalized profiles, but the versioned OFL adapter and multi-cell pixel topology remain pending.
- The Autoloop runtime now has complete 64×32 capacity, pageable four-bank controller views, and core activation/return policies, but the Studio editor, default content, random selection policies, package loader, and performance UI remain pending.
- The lab Runner has console status only; no production performance UI, persistence compiler, audio analysis, or `.ssproj` parser yet.

## Next gate

Move from pure contracts to a Windows hardware/DJ laboratory build:

1. OS2L TCP server and capture tool.
2. Windows CI compile, WinMM device enumeration/capture/feedback test, and Control One map.
3. Control One map.
4. Art-Net receiver/node and QLC+ bridge verification.
5. USB VID/PID/driver inventory.
6. Windows benchmarks on Joshua's DJ laptop and a lower-end PC.
