# EmberLights Windows V1 Testing Guide

## Build status

EmberLights V1 testing builds are unsigned pre-release software. GitHub Actions compiles and tests the same source on Windows and Linux before it creates:

- `EmberLights-<version>-Setup.exe` — per-user installer with Start-menu entry, optional desktop shortcut, uninstaller, and `.emberlights` file association;
- `EmberLights-<version>-Portable.zip` — the same self-contained application without installation.

Windows 10 build 1809 or later and Windows 11 are the current implementation target. Real-machine qualification is still required before that range becomes a release guarantee.

Every packaged build records its product version and source commit in About/Diagnostics. GitHub artifacts also contain a SHA-256 checksum file and machine-readable release manifest.

Only one EmberLights process runs per Windows session, preventing duplicate DMX transmitters and OS2L listeners. Opening an `.emberlights` file while the app is already running forwards that project to the existing window, including its normal unsaved-changes prompt. The installer also asks the running app to close before replacing files.

## Safe first launch

1. Install EmberLights or extract the portable ZIP.
2. Start the app. All DMX output is disabled in a new project; OS2L listens only on `127.0.0.1` by default. After one successful open or save, EmberLights reopens that project automatically on later launches.
3. Build and validate a project before enabling output:
   - choose or create a Fixture Profile, or use **Profiles → Import QLC+ Fixture (.qxf)...** and review the conversion report;
   - add each fixture under Patch with the correct universe, DMX address, and optional role tags;
   - create reusable Groups from the stable fixture IDs shown in Patch;
   - create Static Looks using `fixture-id,property,value` or `group-id,property,value` rows;
   - create Autoloops from those look IDs;
   - save the project as an `.emberlights` file;
   - use **Show → Validate Project**.
4. Use a visualizer or isolated test node before connecting production fixtures. Confirm universe numbering and address maps independently.
5. Under Connections, choose one or more output paths:
   - enable Art-Net or sACN and enter the receiver address; sACN accepts `multicast` as its destination;
   - for an ENTTEC DMX USB Pro or compatible interface that Windows exposes as a COM port, choose its port for universe 1 or universe 2. Choose **Refresh MIDI + USB-DMX** after connecting a device. One single-universe device cannot be assigned to both universes, and projects using this adapter are capped at 40 Hz.
   - for the SoundSwitch Micro (`VID_15E4/PID_0053`), close SoundSwitch and assign **SoundSwitch Micro (WinUSB)** to universe 1 or 2. The protocol is fixed to **SoundSwitch native JLS1**; the disproved preview.310 A/B/C choices no longer appear.
   - press **Save & Apply Connections**. This validates and atomically saves every setting on the page into the project. If Runner is active and the adapter graph changed, EmberLights performs the normal zero-frame stop and restarts it from the saved settings.
6. Start the show from Live. Confirm clock, adapter, frame, error, and jitter state under Diagnostics before triggering content.

Imported QXF modes are read-only snapshots with recorded provenance. Duplicate one to make an editable local profile. Verify every imported channel against the fixture's official DMX chart—especially shared shutter/strobe functions, hazardous output, custom lanes, and multi-head fixtures—before enabling physical output. See `docs/16_QLC_FIXTURE_IMPORT.md` for the exact conversion and quarantine rules.

The native USB paths implement the published single-universe DMX USB Pro serial framing and an independently implemented SoundSwitch Micro WinUSB adapter. For the Micro, Diagnostics deliberately says **Open**, then separately reports accepted native frame writes, write failures, the last Windows error, and the number of nonzero rendered slots. Open/accepted writes prove host-side progress; only a responding physical receiver proves DMX interoperability. SoundSwitch must remain closed while EmberLights owns the Micro.

Before assigning the Micro to a normal project, run **EmberLights Hardware Test** from the Start menu with the isolated IR-4 bench in `docs/MORNING_HARDWARE_TEST.md`. The guided test now includes raw output, compiled Runner output after a clean reopen, and an unplug/replug recovery stage through the same production session lifecycle. Its Desktop report names the first incomplete gate and only reports `passed` after operator-confirmed red and blackout in all three stages.

## VirtualDJ and MIDI

- Same-computer VirtualDJ/OS2L defaults to `127.0.0.1:9996`. In VirtualDJ Settings → Options, set `os2l` to **Yes**, not **Auto**, set `os2lDirectIp` to `127.0.0.1:9996`, then restart VirtualDJ. VirtualDJ's Auto mode waits until an OS2L action such as a DMX pad is used; Yes initiates the connection without that pad press.
- After **Start Show**, Live and Diagnostics must show OS2L **Waiting** with the actual listening endpoint before VirtualDJ connects, then **Ready** after beat traffic begins. Diagnostics also records the last socket error; a port of `0` means the listener is not open.
- A VirtualDJ action `os2l_button "Exact Look Name"` activates/releases that Static Look through the same target-aware state used by Live and MIDI. `os2l_button "Look: Exact Look Name"` is the explicit form; use `os2l_button "Autoloop: Exact Autoloop Name"` for an Autoloop. Names and capitalization must match the active project. A delayed off event for an older target cannot clear a newer selection.
- Diagnostics records dropped named OS2L actions. Keep that count at zero during qualification; a nonzero value means the bounded input queue overflowed and the session is not clean evidence.
- For a separate lighting computer, bind OS2L to that computer's private LAN address and configure VirtualDJ's direct OS2L destination to match. Do not expose the listener to an untrusted network.
- Select a MIDI input under Connections before using MIDI Learn. Use **Save & Apply Connections** after changing adapter settings; EmberLights restarts Runner when required.
- Static Looks in Live use **Toggle**: selecting a different look switches to it, while toggling the active look clears it with the normal anti-snap fade. For MIDI mappings, **Toggle** uses the same press-again rule, **Latch** selects without releasing on button-up, and **Momentary** holds only while its button is down. Releasing an older held pad cannot clear a newer selected look.
- **Save & Apply Connections** is transactional: if saving is cancelled or fails, EmberLights restores the prior project settings and does not apply the proposal. When the show is stopped, the confirmation says the saved settings will open on **Start Show**; when a live adapter graph changes, success is reported only after Runner restarts and reaches **Running**.
- Control One is treated as standard MIDI until its owned-device map is captured and verified. Its proprietary DMX, display, firmware, and storage behavior is not claimed.
- Diagnostics now lists the same backend health fields for every configured transport: opening/ready/recovering/fault state, open attempts, reconnects, accepted/failed frames, last error, and last nonzero-slot count. These are host-side facts; only a responding physical receiver proves DMX interoperability.
- Control One's two-port DMX transport, production Runner route, and installed qualifier are implemented and contract-tested. Connections can enable it only through the explicit `Experimental (unqualified)` option; jack 1 carries U1 and jack 2 carries U2. Physical jack isolation, blackout, and replug recovery remain required before any support or gig-qualified claim. WOLFmix is a later standalone/DMX-input bridge track, not a USB-dongle option.

## Safety and recovery

- Blackout bypasses the semantic renderer and transmits raw zeroed DMX frames.
- Fog, haze, laser, and spark properties fail closed unless explicitly armed.
- The Safety page controls whether each hazard requires an explicit arm and caps normalized strobe/intensity output; conservative arming defaults are enabled in new projects.
- Work Light is locally available even when DJ or MIDI input is unavailable.
- Saves are written to a temporary file, read back, checksum-verified, and atomically promoted. The previous valid file is retained as `.bak`; EmberLights attempts that backup when the primary file is corrupt.
- Clean Runner shutdown sends three zero frames to every open network and USB-DMX output. Keep a rehearsed independent backup lighting path during testing.

## Not yet qualified

Do not use this build as the only lighting controller at a live event. It still needs the acceptance evidence in `04_V1_SCOPE_AND_ACCEPTANCE.md`, including real VirtualDJ/OS2L capture, Control One capture and feedback, representative Art-Net/sACN/USB-DMX receiver tests, interface disconnect/reconnect tests, low-end Windows measurements, eight-hour soak tests, shadow rehearsals, and a low-risk pilot.

The installer is not code-signed yet, so Windows may show an unknown-publisher warning. Signing, upgrade/rollback validation, and public distribution are release gates rather than hidden limitations.

## Machine qualification

The installer includes `Tools\emberlights_qualify.exe`. Start with a two-minute smoke on each Windows machine:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_qualify.exe" --duration 120
```

Before gig qualification, run the strict eight-hour profile under the machine's normal DJ workload:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_qualify.exe" --strict --duration 28800
```

Retain the generated JSON report with the machine specifications and installer version. This synthetic test exercises 128 fixtures across two universes and live/emergency commands; it does not replace VirtualDJ, controller, receiver, or fixture testing. See `docs/17_PRODUCTION_RELEASE_GATE.md`.
