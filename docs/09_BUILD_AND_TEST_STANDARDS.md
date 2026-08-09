# Build and Test Standards

## Correctness

- Every public domain rule has unit tests.
- Packet formats use official specifications and byte-exact golden fixtures.
- Normalized input adapters have captured replay corpora.
- Deterministic replay with identical input/package must produce identical universe frames and state transitions.
- Unknown enum, fixture capability, migration field, or adapter event fails closed and remains inspectable.

## Real-time behavior

- Allocate buffers and effect instances during package load, not on the scheduling thread.
- No blocking file, network-discovery, logging, UI, or analysis operation on the DMX thread.
- Use monotonic clocks for scheduling and transport prediction.
- Timestamp input at adapter ingress.
- Bound every queue and define overflow behavior.
- Never hold the last hazardous trigger merely because an input queue stalls.

## Safety

- Fog, haze, laser, spark, and similar effects are denied unless explicitly armed by policy and operator state.
- Blackout behavior is an explicit policy; crashes must not silently convert to blackout.
- Strobe frequency/intensity and mover speed/intensity support configurable caps.
- Emergency and work-light actions remain locally available even if DJ/network/audio inputs fail.
- Output recovery never replays a stale momentary hazardous command.

## Performance gates

Measure release builds, not debug builds. Record machine, OS, compiler, package, fixture count, frame rate, input sources, and sample duration.

| Metric | Target | Release ceiling |
| --- | ---: | ---: |
| Headless Runner RSS | 50 MB | 100 MB |
| Runner with performance UI | 75 MB | 125 MB |
| Runner idle CPU | 0.5% | 1% |
| Two-universe active CPU | 2% | 5% |
| Cold start to output-ready | 2 s | 4 s |
| MIDI-to-render | 20 ms | 30 ms |
| Input-to-visible-DMX typical | 35 ms | 60 ms |
| DMX scheduling jitter p99 | 2 ms | 5 ms |
| Continuous stress | 8 hours | no lost engine state |

## Fault matrix

Tests must cover:

- malformed/oversized OS2L;
- OS2L delay, disconnect, reconnect, duplicate/out-of-order beat;
- seek, loop, reverse, pitch, cue jump, scratch, slip, and track replacement;
- MIDI storm, disconnect, duplicate device, stuck note, and feedback loop;
- Art-Net/sACN receiver loss and network change;
- USB output disconnect/reconnect and driver failure;
- corrupt show package, fixture, track association, or imported payload;
- UI crash/restart while Runner maintains output;
- audio worker crash or low-confidence beat;
- clock discontinuity and system sleep/resume.

## Code quality

- Warnings are errors in CI/release builds.
- Use sanitizers and static analysis where supported.
- Favor small dependency surfaces in Runner; every dependency needs purpose, license, maintenance, and footprint notes.
- Avoid undefined behavior and raw ownership; production-language choice must provide a credible memory/concurrency safety strategy.
- Public formats and adapter contracts are versioned.

## Persistence and migration

- Saves are transactional and checksummed.
- Keep last-known-good versions and automatic history.
- Import works on copies/read-only streams.
- Preserve original source bundle and unknown data.
- Migration reports are deterministic and machine-readable.

## Licensing/provenance

- Maintain `THIRD_PARTY_NOTICES` before distributing any reused component.
- Preserve Apache 2.0 notices and modified-file markers for QLC+ code if copied/adapted.
- Preserve OFL/MIT attribution and source metadata for fixture conversions.
- Obtain an Art-Net OEM code and include required attribution before product distribution.
- Do not copy SoundSwitch source, assets, fixture library, or trade dress.

## Release evidence

Every milestone release includes:

- exact commit/source snapshot;
- compiler/toolchain versions;
- passing unit/integration/fault tests;
- benchmark report;
- known limitations and unsupported hardware;
- show-package schema version;
- recovery/rollback instructions.
- a machine-readable release manifest, artifact checksums, and applicable qualification reports.
