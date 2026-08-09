# EmberLights Native Core

This is the dependency-light C++20 implementation behind the EmberLights Windows testing application and its deterministic Runner. It is not yet live-gig-qualified.

It proves:

- sparse per-property layers;
- broad semantic fixture rendering for additive/subtractive colors, movement, beam, effect, hazardous, and custom attribute lanes;
- strict fixture-profile validation, per-channel safe defaults, constant channels, inverted ranges, and 8/16-bit values;
- compiled fixture-profile storage with stable IDs, provenance, and fail-independent quarantine;
- two fixed DMX universes;
- fail-closed fog, haze, laser, and spark arming plus strobe/intensity safety policy enforcement;
- OS2L flat-message parsing;
- bounded OS2L TCP stream framing and a direct-IP capture server;
- reconnecting OS2L server lifecycle with bounded event delivery and partial-packet reset;
- predictive timing/fallback states;
- device-agnostic MIDI mapping and soft takeover;
- portable short-MIDI codec plus an isolated Windows WinMM adapter for enumeration, 16 simultaneous inputs/outputs, timestamped callbacks, bounded per-port queues, feedback output, and controller capture;
- generic sparse Static Looks with interruption-safe crossfades;
- 64 banks of 32 generic semantic Autoloops (2,048 total), with pageable four-bank control views plus one-shot, infinite, and track-duration playback;
- ArtDMX packet generation and IPv4 unicast output.
- sACN/E1.31 packet generation plus unicast and standards-based multicast output;
- versioned, checksummed `.emberlights` project persistence with atomic replacement, backup recovery, and unknown-record preservation;
- a fixed-capacity project compiler and three-thread service Runner that keeps input and network output away from the DMX scheduler;
- a native Windows Studio/Live shell and installer packaging path;
- a two-universe laboratory Runner with a dedicated scheduling thread, bounded OS2L beat queue, non-droppable blackout state, dry-run default, network output, status counters, and finite-duration smoke mode.

## Build

```bash
make
make test
make bench
make smoke
```

The build uses only a C++20 compiler, the standard library, and operating-system sockets. `make bench` measures both raw rendering and full Static Look/Autoloop performance updates.

Cross-platform CMake builds are also supported:

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --config Release
ctest --test-dir build-cmake --build-config Release --output-on-failure
```

The repository workflow compiles and tests the core on Windows and Linux, then produces an Inno Setup installer and portable Windows ZIP.

## Capture VirtualDJ OS2L

Local machine only (safe default):

```bash
./build/os2l_capture --bind 127.0.0.1 --port 9996
```

For a separate lighting computer, bind to its private LAN address and set VirtualDJ's `os2lDirectIp` to that address and port. Do not bind the capture tool to an untrusted/public network.

## Capture Windows MIDI / Control One

List WinMM ports:

```bash
./build/midi_capture --list
```

Monitor a port by its displayed discovery index:

```bash
./build/midi_capture --input 0
```

Use `--all` to monitor every enumerated input and `--duration SECONDS` for a bounded capture session. The WinMM implementation is compiled only on Windows; other platforms retain a safe unsupported stub.

## Run the end-to-end laboratory path

Dry-run with a manual clock:

```bash
./build/runner_lab --manual-bpm 120
```

VirtualDJ on the same computer can connect to the default OS2L endpoint at `127.0.0.1:9996`. To emit the two reference universes to a receiver:

```bash
./build/runner_lab --artnet 192.168.1.50
```

This executable uses a hard-coded two-fixture RGB reference patch and is for transport/output validation only. It is not yet a user-configurable or gig-qualified Runner.

## Boundary

QLC+ compatibility is achieved by sending the same Art-Net frames to a QLC+ input configured to forward them to a USB adapter. QLC+ is not linked into this core.
