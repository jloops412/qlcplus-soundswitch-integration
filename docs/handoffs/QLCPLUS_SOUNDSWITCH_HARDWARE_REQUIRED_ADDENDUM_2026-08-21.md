# QLC+ SoundSwitch Hardware Support — Required Addendum

**Date:** 2026-08-21  
**Status:** Authoritative priority update to `QLCPLUS_NATIVE_DJ_PC_HANDOFF_2026-08-21.md`

## Owner directive

SoundSwitch Micro and SoundSwitch Control One support are **both required deliverables**, not optional fallback work.

The project goal is no longer merely to get the owner's immediate gig rig working with any available QLC+ interface. The goal is to produce a simple, maintainable QLC+ setup that also makes existing SoundSwitch hardware useful without a SoundSwitch subscription.

This matters for:

- the owner's own rigs,
- backup/failover use,
- other DJs who already own a SoundSwitch Micro,
- other DJs who already own a Control One,
- future community use if the implementation is clean enough to distribute.

## Architecture remains deliberately simple

At runtime, the target remains **one lighting application: QLC+**.

```text
VirtualDJ
   |
   | OS2L
   v
QLC+
   |
   +-- native QLC+ MIDI plugin
   |      ^
   |      |
   |   Control One MIDI
   |
   +-- native SoundSwitch hardware output plugin
          |
          +-- SoundSwitch Micro — DMX
          |
          +-- Control One — DMX 1
          |
          +-- Control One — DMX 2
```

Do **not** introduce a second runtime application, bridge daemon, localhost transport, or EmberLights process for the finished solution.

## Required hardware deliverables

### A. SoundSwitch Micro

Required behavior:

- autodetect attached Micro units,
- expose each unit as a QLC+ DMX output,
- stable 512-channel DMX output,
- correct zero/blackout handling,
- safe open/close behavior,
- clean unplug/replug recovery,
- deterministic output cadence,
- no UI-thread blocking,
- stable UID/device identity for project persistence when possible,
- support multiple Micro devices if Windows/device enumeration permits it.

Known existing EmberLights work must be reused rather than rediscovered:

- VID `15E4`
- PID `0053`
- WinUSB transport
- bulk OUT endpoint `0x01`
- existing `sTRt` DMX packet framing
- existing initialization, warmup, stream, reconnect, and close logic where proven correct.

### B. SoundSwitch Control One DMX

Required behavior:

- detect Control One,
- expose `DMX 1` and `DMX 2` as separate QLC+ outputs,
- permit independent universes/data on both ports,
- permit both ports to run simultaneously,
- preserve the Control One MIDI interface at the same time,
- do not bind/replace the entire composite USB device with a generic driver,
- recover cleanly from disconnect/reconnect,
- no conflict between MIDI and proprietary DMX interface access,
- stable output for gig-length operation.

Known existing EmberLights work must be reused:

- VID `15E4`
- PID `0054`
- existing WinUSB/session work,
- existing `sTRt`-family framing,
- existing per-port selector semantics,
- existing probes/tests as reference evidence.

## Implementation shape

The preferred long-term codebase is a small QLC+ output plugin, conceptually:

```text
plugins/soundswitch/
    CMakeLists.txt
    soundswitchplugin.h/.cpp
    usb_device_discovery.h/.cpp
    micro_transport.h/.cpp
    control_one_transport.h/.cpp
    output_worker.h/.cpp
    tests/
```

Do not copy EmberLights architecture wholesale. Port only the minimal transport/session logic needed for hardware interoperability.

### QLC+ plugin responsibilities

The plugin should implement only what QLC+ needs:

- `init()`
- `capabilities()`
- `pluginInfo()`
- `outputs()`
- `outputsUID()`
- `openOutput()`
- `writeUniverse()`
- `closeOutput()`
- `outputInfo()`
- hotplug/configuration refresh as practical.

QLC+ remains responsible for fixtures, patching, scenes, chasers, effects, MIDI mapping, OS2L, project state, and UI.

## Output-worker rule

If either SoundSwitch device requires continuous frame transmission, keep that worker **inside the QLC+ plugin process**.

Recommended design:

1. QLC+ calls `writeUniverse()`.
2. Copy/publish the newest 512-channel frame into a per-output buffer.
3. A small internal worker sends frames at the device-safe cadence.
4. Newest-frame-wins; never accumulate a long queue of stale DMX frames.
5. USB errors back off and transition to a reconnect state; never busy-loop.
6. Closing an output performs the proven safe blackout/close sequence.

This preserves the one-program-at-gigs requirement.

## Driver strategy

### Micro

Prefer using the interface/driver arrangement already proven by the existing EmberLights WinUSB implementation. Avoid changing drivers unless required.

### Control One

The Control One is a composite device and MIDI is mandatory.

The acceptable state is:

```text
Control One MIDI interface -> Windows / QLC+ MIDI
Control One DMX interface  -> QLC+ SoundSwitch plugin
```

Never replace the whole Control One device with WinUSB/libusb in a way that removes MIDI.

If interface-specific rebinding is required, document exactly which child/interface is changed and provide a rollback procedure.

## Required validation matrix

Neither device is considered supported because a single fixture flashed once.

### SoundSwitch Micro qualification

- real fixture or DMX tester,
- values 0, 1, 127, 128, 254, 255,
- multiple channels changing continuously,
- at least 2 hours continuous output for release candidate,
- blackout/restore,
- project reopen,
- QLC+ restart,
- unplug/replug while QLC+ remains open,
- Windows suspend/power-setting sanity,
- CPU/memory stability,
- no repeating USB error storm,
- test after VirtualDJ and Control One MIDI are also active on the laptop.

### Control One qualification

- MIDI detected before DMX test,
- DMX 1 independently,
- DMX 2 independently,
- both ports simultaneously,
- different universe data on each port,
- MIDI controls working while both DMX ports are transmitting,
- at least 2 hours continuous output for release candidate,
- blackout/restore,
- QLC+ restart,
- unplug/replug,
- no MIDI disappearance or frozen controls,
- no device reset loop,
- no visible DMX flicker attributable to USB scheduling.

### Combined stress qualification

Before calling the package suitable for real DJ use, test a representative load:

```text
VirtualDJ playing music
+ QLC+ OS2L active
+ Control One MIDI active
+ Control One DMX 1/2 active
+ SoundSwitch Micro active if available
+ normal audio interface/controller workload
```

The fact that the DJ laptop is simultaneously handling low-latency audio is why this combined test is mandatory.

## Packaging for the owner and other DJs

The first distributable milestone should be easy to understand and undo.

Preferred options, in order:

1. If accepted upstream by QLC+, distribute through normal QLC+ releases.
2. Otherwise maintain a very small QLC+ fork/release that differs primarily by `plugins/soundswitch/`.
3. If technically possible and ABI-safe, later investigate distributing a plugin package compatible with the official QLC+ binary, but do not force this if QLC+ plugin ABI/build constraints make a matched custom build more reliable.

Release artifacts should identify the exact compatible QLC+ version.

Example naming:

```text
QLC+ 5.2.2 + SoundSwitch Hardware Support r1
```

For other DJs, provide:

- installer or clearly repeatable plugin install,
- supported-device table,
- Windows driver prerequisites,
- how to verify Micro detection,
- how to verify Control One MIDI + DMX,
- rollback/uninstall steps,
- troubleshooting/log collection steps,
- explicit note that the project is community interoperability software and is not an official SoundSwitch/InMusic product.

Do not redistribute vendor firmware, proprietary SoundSwitch application files, or third-party drivers unless redistribution rights are clear.

## Repository and maintenance strategy

For maintainability, the QLC+ hardware implementation should live in a focused QLC+ fork/repository rather than continuing inside the broad EmberLights application.

EmberLights remains a research/reference archive until useful material has been extracted.

Keep a provenance note listing which existing EmberLights files/commits were used so future maintainers do not need to reverse-engineer the same protocol again.

Keep the delta from upstream QLC+ very small so updates are normally:

1. merge/rebase current QLC+ stable,
2. resolve plugin API/build changes if any,
3. rebuild,
4. rerun hardware smoke/stress tests.

## Required milestone order

1. Install and understand stock QLC+ workflow.
2. Establish a clean QLC+ development checkout/fork.
3. Port and validate **SoundSwitch Micro**.
4. Port and validate **Control One DMX 1/2 without breaking MIDI**.
5. Validate Control One MIDI mappings in stock QLC+ MIDI support.
6. Run combined DJ-laptop stress test.
7. Build an initial DJ-focused QLC+ workspace using Scenes and beat-timed Chasers.
8. Produce a repeatable Windows install/package.
9. Preserve exact known-good build + rollback package for gigs.
10. Only then move to SoundSwitch project migration and AI-assisted QLC+ programming.

## Non-goals for this milestone

Do not spend time on:

- rebuilding the EmberLights UI,
- a custom lighting engine,
- custom fixture runtime,
- custom scene runtime,
- custom chaser runtime,
- a separate bridge application,
- song scripting parity,
- AI generation,
- extensive QLC+ reskinning,
- Control One screens/LED feedback unless trivial after base MIDI works.

Those can be evaluated only after Micro + Control One + core QLC+ workflow are stable.

## Definition of done

This milestone is complete only when the owner can use one Windows laptop and launch **QLC+ as the sole lighting application**, then choose either:

```text
SoundSwitch Micro -> DMX fixtures
```

or:

```text
Control One MIDI -> controls QLC+
Control One DMX 1/2 -> DMX fixtures
```

with both hardware families independently supported and documented well enough that another DJ with the same hardware could reasonably install and test the same solution.

The point is not merely to avoid buying another dongle. The point is to make already-owned SoundSwitch hardware useful in a free, maintainable QLC+ workflow.