# QLC+ Native SoundSwitch Hardware Pivot — DJ PC Build / Install Handoff

**Date:** 2026-08-21  
**Primary goal:** Get a reliable, gig-usable, free lighting setup running on the DJ laptop with **one lighting application at runtime: QLC+**.  
**Priority:** Reliability and maintainability first. Reuse only the proven SoundSwitch hardware transport work from EmberLights when necessary. Do **not** continue building EmberLights as a separate lighting application for this milestone.

---

## 1. Architectural decision

The preferred architecture is:

```text
VirtualDJ
   |
   | OS2L / MIDI as supported by QLC+
   v
QLC+ 5.2.2
   |
   +-- native QLC+ MIDI plugin <- SoundSwitch Control One used as MIDI controller
   |
   +-- native QLC+ OS2L plugin <- VirtualDJ tempo/beat/control integration
   |
   +-- DMX output
        |
        +-- FIRST CHOICE: any already-owned USB-DMX device that stock QLC+ supports
        |
        +-- SECOND CHOICE: native SoundSwitch output plugin inside QLC+
             +-- SoundSwitch Micro
             +-- Control One DMX 1
             +-- Control One DMX 2
```

### Hard constraints

- **One lighting application at gigs:** QLC+.
- **No EmberLights runtime.**
- **No localhost bridge/daemon/helper process as the permanent solution.**
- **No new custom DMX engine.**
- **No custom scene/chaser/fixture engine unless a specific QLC+ limitation is proven later.**
- **Do not redesign QLC+ UI for this milestone.** Use QLC+ as-is first.
- Pin the first production build to **QLC+ 5.2.2** rather than following `master`.
- Keep any custom QLC+ changes narrowly scoped to SoundSwitch hardware I/O.

The absolute lowest-maintenance outcome is **stock QLC+ + an already-owned natively supported DMX interface**. Test this before writing custom hardware code. If none of the owner's existing interfaces work natively, then build the SoundSwitch hardware plugin.

---

## 2. Why this is the chosen path

QLC+ already provides the functions that matter for the immediate SoundSwitch replacement use case:

- Fixture definitions and patching
- Manual DMX control / testing
- Scenes (use as SoundSwitch-style Static Looks)
- Chasers / sequences (use as SoundSwitch-style Autoloops)
- Beat-based timing
- EFX / movement functions
- Virtual Console
- MIDI input/output
- OS2L support for VirtualDJ
- Art-Net / sACN and other standard output paths
- Project/workspace persistence

QLC+ 5.2.2 is the current stable target for this work. QLC+ is Apache-2.0 licensed and intentionally uses plugins for physical-device I/O.

This means the project should only solve what QLC+ does **not** already solve: compatibility with the owner's SoundSwitch USB hardware and, later, SoundSwitch project migration / AI-assisted programming.

---

## 3. Phase 0 — protect the gig machine before changing anything

Before building or changing drivers:

1. Record Windows version/build.
2. Record currently installed SoundSwitch version and whether the current SoundSwitch setup still works.
3. Record Device Manager entries for every connected USB-DMX device and Control One.
4. Record VID/PID, device interface, current driver provider/version, and hardware IDs.
5. Preserve a working rollback path:
   - Do not uninstall working SoundSwitch yet.
   - Do not replace the Control One's entire USB/composite driver.
   - Do not use Zadig against the whole Control One device.
   - If a driver change is ever required, target only the specific proprietary DMX interface and document exactly what changed.
6. Back up current lighting project/config files.
7. Disable Windows USB selective suspend / aggressive power saving for the relevant USB hubs/devices if it causes disconnects, but do not make unrelated system changes.

The build agent should keep the existing paid SoundSwitch install available as a temporary fallback until the QLC+ path passes the release gates below.

---

## 4. Phase 1 — install stock QLC+ and inventory all existing dongles

### 4.1 Install stock QLC+ 5.2.2

Install the official Windows build of **QLC+ 5.2.2** first. Do not begin by compiling a custom version.

Confirm:

- QLC+ launches normally.
- Fixtures / Functions work.
- Simple Desk works.
- Virtual Console works.
- MIDI plugin is available.
- OS2L plugin is available.
- DMX USB / HID / other relevant output plugins are loaded.

### 4.2 Test every already-owned USB-DMX interface

This is a mandatory stop/go gate.

Connect each USB-DMX interface the owner already possesses, one at a time, including the myDMX Buddy and any other dongles on hand. Inspect QLC+ Inputs/Outputs and Windows USB identification.

QLC+ natively supports a number of FTDI/Enttec/DMXKing/Eurolite-compatible devices and protocols. Do not assume a dongle is unsupported just because its brand is unfamiliar; inspect VID/PID/chipset and try QLC+'s supported modes where appropriate.

For each device, record:

- Windows device name
- VID/PID
- USB chipset if identifiable
- QLC+ plugin that detects it
- Output line name
- Whether DMX actually reaches a fixture
- Whether output is stable for at least 30 minutes
- Whether blackout / zero values work
- Whether unplug/replug recovery works

### Decision gate A

If one already-owned adapter works reliably in stock QLC+:

**STOP custom DMX-driver development for the immediate gig milestone.**

Use stock QLC+ with that adapter and proceed directly to Phase 7 (Control One MIDI), Phase 8 (DJ workspace), and Phase 9 (gig qualification).

Only continue to the SoundSwitch plugin if there is a real reason to prefer the Micro / Control One DMX outputs.

---

## 5. Phase 2 — establish a clean QLC+ development fork only if needed

If no existing adapter is satisfactory, create a dedicated QLC+ fork/checkout for the custom hardware plugin.

### Repository strategy

Do **not** merge the QLC+ source tree into the EmberLights repository.

Preferred layout:

```text
jloops412/qlcplus-ember     <- QLC+ fork, very small delta from upstream
jloops412/EmberLights       <- legacy/reference repo; source of proven SS hardware work only
```

On the DJ PC:

```text
C:\src\qlcplus-ember\
C:\src\EmberLights\
```

For `qlcplus-ember`:

- `origin` = owner's fork
- `upstream` = `mcallegari/qlcplus`
- baseline/tag = `QLC+_5.2.2`
- create a working branch such as `feature/soundswitch-output`

Keep the custom delta narrowly under something like:

```text
plugins/soundswitch/
```

plus the single top-level plugin registration/build entry necessary to include it.

Do not modify the QLC+ lighting engine, fixture engine, UI architecture, scene engine, chaser engine, MIDI implementation, or OS2L implementation for this milestone.

---

## 6. Phase 3 — extract only the useful EmberLights hardware code

The EmberLights repository is **reference material, not the new runtime**.

Useful existing files include:

```text
native-core/include/showcore/soundswitch_micro.hpp
native-core/src/soundswitch_micro.cpp
native-core/src/soundswitch_micro_probe.cpp
native-core/tests/test_soundswitch_micro_session.cpp

native-core/include/showcore/soundswitch_control_one.hpp
native-core/src/soundswitch_control_one.cpp
native-core/src/soundswitch_control_one_probe.cpp
native-core/tests/test_soundswitch_control_one.cpp
```

Do not drag in `windows_app`, the Ember UI, Ember project model, custom scene/autoloop engine, runner, or unrelated output architecture.

### Existing Micro knowledge to preserve

The current Ember implementation identifies the SoundSwitch Micro as:

```text
VID 15E4
PID 0053
```

It opens the device using WinUSB and writes DMX to bulk OUT endpoint `0x01`.

The known frame begins with:

```text
s T R t
01 00 02 02
00 00
[512 DMX bytes]
```

The existing Ember implementation should be treated as the source of truth for device initialization, settling/warmup, frame timing, close/blackout behavior, reconnect handling, and USB endpoint use until hardware tests prove otherwise.

### Existing Control One knowledge to preserve

The current Ember implementation identifies Control One as:

```text
VID 15E4
PID 0054
```

It uses the same general `sTRt` framing concept, with a port selector byte before the DMX payload. The current implementation models the two physical DMX ports separately.

Again: port the smallest proven transport/session layer possible. Do not port the Ember application around it.

---

## 7. Phase 4 — implement a native QLC+ SoundSwitch output plugin

Use QLC+'s existing plugin interface and the `dummy` / `dmxusb` plugins as structural references.

QLC+'s `QLCIOPlugin` interface already provides the correct lifecycle:

- `init()`
- `outputs()` / `outputsUID()`
- `openOutput()`
- `writeUniverse()`
- `closeOutput()`
- `outputInfo()`

### Target outputs

When devices are connected, expose lines like:

```text
SoundSwitch Micro — DMX
Control One — DMX 1
Control One — DMX 2
```

Only show output lines for devices actually present.

Use stable UIDs for project persistence. Prefer hardware serial number / stable device path where available. Do not base persistence solely on enumeration order if a better stable identifier is available.

### `writeUniverse()` behavior

QLC+ may pass 0–512 bytes. Normalize to a complete 512-channel DMX universe, zero-padding missing channels.

Avoid blocking QLC+'s main/UI thread with slow USB transfers. Preserve the existing device-safe output cadence from EmberLights. A reasonable implementation is:

1. `writeUniverse()` copies the newest 512-byte state into a per-output buffer.
2. A small internal plugin worker sends the newest frame at the already-proven device cadence.
3. Newest-frame-wins; do not queue stale DMX frames indefinitely.
4. Keepalive behavior should match what the physical device requires.
5. On close, send the proven blackout/close sequence before releasing handles.

This is still one QLC+ process; the worker is internal to the plugin, not a second application.

### Device lifecycle requirements

The plugin must handle:

- device not connected at QLC+ startup
- device connected after startup
- device unplugged while output is active
- reconnect without requiring Windows reboot
- clean QLC+ shutdown
- output open/close cycles
- USB transfer timeout/failure
- no busy-loop on device failure
- no stale nonzero DMX left intentionally on close if the device supports explicit blackout transmission

Use QLC+'s `configurationChanged()` mechanism when supported device lines appear/disappear.

---

## 8. Phase 5 — Windows driver rules

### SoundSwitch Micro

First attempt to use the currently installed device interface/driver exactly as the existing Ember WinUSB code does.

Do not replace drivers unless the custom plugin cannot open the device and there is a clearly understood reason.

### Control One

This is more sensitive because Control One is also needed as a MIDI controller.

**Do not replace the whole Control One composite USB driver.**

The desired state is:

```text
Control One MIDI interface -> remains visible to Windows / QLC+ MIDI
Control One proprietary DMX interface -> opened by the native SoundSwitch QLC+ plugin
```

If driver/interface binding must be changed, change only the DMX-specific interface. Confirm MIDI still appears and functions after every driver-related change.

A solution that gains DMX but breaks Control One MIDI is not acceptable.

---

## 9. Phase 6 — build QLC+ for Windows

Follow the current QLC+ Windows Qt6/CMake build instructions rather than inventing a new toolchain.

Recommended first baseline:

- MSYS2 / MINGW64
- CMake
- Ninja
- official Qt 6.x supported by QLC+ 5.2.2
- prefer Qt **6.8.1** initially because QLC+ documentation notes a Windows audio regression in later Qt versions; this is not critical for the current DMX goal but avoiding a known regression is preferable

The QLC+ documentation currently expects roughly:

```bash
export QTDIR=/c/path/to/Qt/6.8.1/mingw_64
cd /c/src/qlcplus-ember
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake" -Dqmlui=ON ..
ninja
ninja install
```

Adapt the exact Qt path and dependencies to the PC.

The SoundSwitch plugin CMake target must link the Windows libraries required by the existing transport, including WinUSB / SetupAPI as appropriate.

### Packaging

After qualification, use QLC+'s own Windows packaging path rather than introducing a custom installer framework.

Produce an installer/build clearly labeled, for example:

```text
QLC+ 5.2.2 + SoundSwitch Hardware r1
```

Install side-by-side with the official QLC+ build during development. Do not overwrite the known-good stock installation until the custom build passes all tests.

At the end of the milestone, the owner should have a normal Start Menu/Desktop shortcut that launches one application: QLC+.

---

## 10. Phase 7 — Control One as MIDI controller

Do not write custom Control One MIDI code initially.

Use QLC+'s existing MIDI plugin and MIDI learn/mapping.

The Control One should remain a normal MIDI control surface for:

- Autoloop / chaser selection
- Static Look / scene selection
- Intensity
- Strobe
- Blackout if desired
- Color overrides
- Movement / position functions
- Bank/page selection

LED/OLED feedback is a later enhancement and is **not** required for the immediate gig milestone.

If standard MIDI mapping provides enough tactile control, leave it alone.

---

## 11. Phase 8 — build the initial DJ-focused QLC+ workspace

The goal is not to reproduce SoundSwitch nomenclature exactly. Use QLC+ concepts comfortably while preserving the important workflow.

### Mapping

```text
SoundSwitch Static Look -> QLC+ Scene
SoundSwitch Autoloop    -> QLC+ Chaser / sequence of Scenes, beat-timed
SoundSwitch fixture patch -> QLC+ Fixtures / Universes
SoundSwitch live controls -> QLC+ Virtual Console + Control One MIDI
```

### Minimum workspace

Create a clean owner-specific QLC+ project with:

1. Actual fixture patch
2. Fixture groups
3. A small set of useful Scenes / Static Looks
4. A small set of beat-based Chasers / Autoloops
5. Master intensity
6. Blackout
7. Strobe / FX controls where appropriate
8. Simple color override controls
9. Movement/position controls for movers
10. A compact Virtual Console performance page
11. Control One MIDI mapping
12. VirtualDJ OS2L enabled and verified

Do not spend days recreating every previous SoundSwitch look before proving the workflow at a real fixture rig.

Start with enough content to run one representative event comfortably, then expand.

---

## 12. Phase 9 — mandatory hardware qualification before gig use

No custom SoundSwitch hardware output is considered production-ready merely because it compiles or flashes a light once.

### Micro release gate

With a real fixture or DMX tester connected:

- All 512 slots can be changed without corruption.
- Verify low/mid/high values such as 0, 1, 127, 128, 254, 255.
- Run continuous changing output for at least 60 minutes.
- Confirm no visible flicker attributable to packet cadence.
- Confirm blackout.
- Confirm QLC+ project close/open.
- Confirm QLC+ restart.
- Unplug/replug Micro while QLC+ is running and verify clean recovery.
- Confirm no runaway CPU usage or repeated error storm.
- Confirm Windows sleep/power policy is configured so the interface is not unexpectedly suspended during gigs.

### Control One DMX release gate

Test **after Micro**, not instead of Micro.

- DMX 1 independently.
- DMX 2 independently.
- Both ports simultaneously with different universes/data.
- Control One MIDI simultaneously while both DMX ports are active.
- 60+ minutes sustained changing output.
- Unplug/replug.
- QLC+ restart.
- Blackout / close behavior.
- Verify no MIDI disappearance, lockup, or device reset caused by the DMX interface access.

### Decision gate B

Preferred gig output order after testing:

1. Any stock QLC+-supported adapter that proves most stable, **or**
2. Control One DMX if both MIDI + DMX are fully stable, **or**
3. SoundSwitch Micro if it is more stable than Control One DMX.

The goal is reliability, not proving a particular reverse-engineered interface can be used.

If a custom SoundSwitch path is not yet reliable, use an already-owned QLC+-compatible dongle for the gig. Do not risk an event to avoid switching hardware.

---

## 13. Phase 10 — make the result easy to manage

Once a configuration passes the release gate:

- Pin the exact QLC+ source tag/commit.
- Pin the SoundSwitch plugin commit.
- Tag a known-good release such as `dj-gig-v1`.
- Save the installer used on the DJ laptop.
- Save the exact QLC+ project/workspace separately from source code.
- Back up the project automatically to a second location/cloud folder.
- Disable auto-updating the custom QLC+ build immediately before gigs.
- Update QLC+ only deliberately, after testing the SoundSwitch plugin against the new version.
- Keep stock QLC+ and/or the last known-good installer available for rollback.

Maintenance should be limited to periodically rebasing the small QLC+ fork on upstream and confirming `plugins/soundswitch/` still compiles and passes the hardware qualification tests.

---

## 14. Explicit non-goals for this build

Do not burn time on these now:

- Custom EmberLights UI
- Ember scene/chaser engine
- Custom fixture engine
- Song scripting/timeline parity
- SoundSwitch scripted-track import
- Control One screen/OLED support
- Fancy Control One LED feedback
- AI scene generation
- AI fixture profile generation
- AI patching
- Full SoundSwitch project migration
- Custom skins
- A separate bridge process
- Mobile remote

Those can be evaluated **after** the basic free QLC+ workflow is gig-proven.

---

## 15. Next major feature after the gig-safe baseline: SoundSwitch migration

Once QLC+ + hardware is reliable, investigate a separate importer that translates SoundSwitch project semantics into QLC+:

```text
SoundSwitch patch        -> QLC+ fixtures/universes
SoundSwitch Static Looks -> QLC+ Scenes
SoundSwitch Autoloops    -> QLC+ Scenes + Chasers
Fixture groups           -> QLC+ groups
Colors/positions         -> QLC+ palettes/scenes where appropriate
```

Do not make migration a blocker for the initial QLC+ deployment.

The importer should eventually target QLC+'s project model rather than reintroducing EmberLights as a parallel lighting engine.

---

## 16. Later AI integration direction

After the project format/workflow is stable, AI can add high value without owning the real-time lighting engine.

Desired future actions include:

- Create fixture profile from manual / channel chart
- Suggest patch addresses and detect collisions
- Generate Scenes from natural language
- Generate beat-based Chasers/Autoloops
- Generate coordinated movement/color/intensity patterns by fixture group
- Analyze an existing QLC+ project and improve it
- Translate SoundSwitch content into QLC+ functions

AI should generate/edit deterministic QLC+ project/function data. QLC+ remains responsible for real-time DMX execution.

---

# Execution instruction to the DJ-PC agent

You are working directly on the owner's DJ laptop. Execute this plan rather than merely describing it.

**Order of operations:**

1. Protect/record the currently working setup.
2. Install and validate stock QLC+ 5.2.2.
3. Inventory and physically test every already-owned USB-DMX adapter in stock QLC+.
4. If one is reliable, use it and skip custom USB development for now.
5. If none are suitable, create a clean QLC+ 5.2.2 fork and implement only `plugins/soundswitch/`.
6. Port the smallest proven Micro transport from EmberLights first.
7. Physically qualify Micro.
8. Then port/qualify Control One DMX while preserving Control One MIDI.
9. Use QLC+'s native MIDI and OS2L implementations; do not rebuild them.
10. Build the initial DJ-oriented QLC+ project/workspace.
11. Map Control One using standard MIDI learn.
12. Run the mandatory sustained hardware tests.
13. Package/install the known-good QLC+ build if a custom plugin was required.
14. Leave a desktop/start-menu shortcut and a known-good project ready to launch.
15. Commit/push all custom source and document the exact build/version/driver state.

**Do not expand scope.** The success condition is not a new lighting product. The success condition is:

> The owner can launch one QLC+ application on the DJ laptop, use Control One as the familiar physical controller, send stable DMX through hardware already owned, create/use Scenes and beat-based Chasers as Static-Look/Autoloop equivalents, and run an upcoming gig without needing a SoundSwitch subscription.
