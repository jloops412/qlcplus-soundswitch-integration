# EmberLights Windows V1 Testing Guide

## Build status

EmberLights V1 testing builds are unsigned pre-release software. GitHub Actions compiles and tests the same source on Windows and Linux before it creates:

- `EmberLights-<version>-Setup.exe` — per-user installer with Start-menu entry, optional desktop shortcut, uninstaller, and `.emberlights` file association;
- `EmberLights-<version>-Portable.zip` — the same self-contained application without installation.

Windows 10 build 1809 or later and Windows 11 are the current implementation target. Real-machine qualification is still required before that range becomes a release guarantee.

## Safe first launch

1. Install EmberLights or extract the portable ZIP.
2. Start the app. Network DMX output is disabled in a new project; OS2L listens only on `127.0.0.1` by default.
3. Build and validate a project before enabling output:
   - choose or create a Fixture Profile;
   - add each fixture under Patch with the correct universe, DMX address, and optional role tags;
   - create reusable Groups from the stable fixture IDs shown in Patch;
   - create Static Looks using `fixture-id,property,value` or `group-id,property,value` rows;
   - create Autoloops from those look IDs;
   - save the project as an `.emberlights` file;
   - use **Show → Validate Project**.
4. Use a visualizer or isolated test node before connecting production fixtures. Confirm universe numbering and address maps independently.
5. Under Connections, enable either Art-Net or sACN and enter the receiver address. sACN accepts `multicast` as its destination.
6. Start the show from Live. Confirm clock, adapter, frame, error, and jitter state under Diagnostics before triggering content.

## VirtualDJ and MIDI

- Same-computer VirtualDJ/OS2L defaults to `127.0.0.1:9996`.
- For a separate lighting computer, bind OS2L to that computer's private LAN address and configure VirtualDJ's direct OS2L destination to match. Do not expose the listener to an untrusted network.
- Select a MIDI input under Connections before using MIDI Learn. Stop and restart the show after changing connection settings or mappings so the new compiled configuration becomes active.
- Control One is treated as standard MIDI until its owned-device map is captured and verified. Its proprietary DMX, display, firmware, and storage behavior is not claimed.

## Safety and recovery

- Blackout bypasses the semantic renderer and transmits raw zeroed DMX frames.
- Fog, haze, laser, and spark properties fail closed unless explicitly armed.
- The Safety page controls whether each hazard requires an explicit arm and caps normalized strobe/intensity output; conservative arming defaults are enabled in new projects.
- Work Light is locally available even when DJ or MIDI input is unavailable.
- Saves are written to a temporary file, read back, checksum-verified, and atomically promoted. The previous valid file is retained as `.bak`; EmberLights attempts that backup when the primary file is corrupt.
- Clean Runner shutdown sends three zero frames. Keep a rehearsed independent backup lighting path during testing.

## Not yet qualified

Do not use this build as the only lighting controller at a live event. It still needs the acceptance evidence in `04_V1_SCOPE_AND_ACCEPTANCE.md`, including real VirtualDJ/OS2L capture, Control One capture and feedback, representative Art-Net/sACN receiver tests, interface disconnect/reconnect tests, low-end Windows measurements, eight-hour soak tests, shadow rehearsals, and a low-risk pilot.

The installer is not code-signed yet, so Windows may show an unknown-publisher warning. Signing, upgrade/rollback validation, and public distribution are release gates rather than hidden limitations.
