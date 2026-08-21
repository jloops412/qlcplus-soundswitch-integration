# EmberLights Windows V1 Testing Guide

## Build status

EmberLights V1 testing builds are unsigned pre-release software. GitHub Actions compiles and tests the same source on Windows and Linux before it creates:

- `EmberLights-<version>-Setup.exe` — per-user installer with Start-menu entry, optional desktop shortcut, uninstaller, and `.emberlights` file association;
- `EmberLights-<version>-Portable.zip` — the same self-contained application without installation.

Windows 10 build 1809 or later and Windows 11 are the current implementation target. Real-machine qualification is still required before that range becomes a release guarantee.

The NSIS bootstrap deliberately does not perform its own architecture or OS-version check. The packaged application is verified as a 64-bit Windows executable, and the Windows loader is the authoritative compatibility check; this avoids false rejections of supported Windows 11 x64 systems by the 32-bit installer bootstrap.

Setup recognizes the stable EmberLights identity used by earlier Inno Setup and NSIS testing builds. It closes over those per-user uninstall records, removes the older program files before replacement, and preserves projects and user settings outside `%LocalAppData%\Programs\EmberLights`. The new build remains available under **Installed apps** and through **Start → EmberLights → Uninstall EmberLights**.

Every packaged build records its product version and source commit in About/Diagnostics. GitHub artifacts also contain a SHA-256 checksum file and machine-readable release manifest.

The Windows package additionally carries `EmberLights-Windows-payload-manifest.json`. It binds the package version and exact checked-out commit to the SHA-256 and size of every CMake-staged file, and records the testing-preview/non-outputting/no-physical-claim safety boundary. CI rejects the package if `git rev-parse HEAD` differs from that declared commit. On pull requests the exact checkout may be GitHub's synthetic merge commit; the contributor head is recorded separately as `sourceHeadCommit` in the external release manifest rather than being mislabeled as the packaged tree. Packaging also fails if a required application, tool, template, operator document, or notice is missing; if paths collide under Windows rules; or if the staged, portable, and installed payloads differ. The external release evidence retains the same payload manifest plus the report from the installed `emberlights_qualify.exe` smoke.

Windows CI verifies a clean isolated install, `.emberlights` association, full installed-payload hashes, GUI startup, non-outputting Micro and Control One self-tests, a two-second qualification smoke with network output disabled, uninstall, and association cleanup. It separately extracts and verifies the portable ZIP and launches its GUI smoke. These checks do not yet exercise upgrade from an older build, rollback, Authenticode, hardware, or physical DMX; the format-2 release manifest reports upgrade and rollback as `not-run` until those distinct gates exist.

Only one EmberLights process runs per Windows session, preventing duplicate DMX transmitters and OS2L listeners. Opening an `.emberlights` file while the app is already running forwards that project to the existing window, including its normal unsaved-changes prompt. The installer also asks the running app to close before replacing files.

## Safe first launch

1. Install EmberLights or extract the portable ZIP.
2. Start **EmberLights**. Preview 106 opens the replacement-shell beta at **Studio → Fixtures + Static Looks** with an output-disabled demo project. Use **New**, **Open**, **Save Project As**, Undo/Redo, profile-derived fixture controls, and offline **Simulate** directly in this shell. The footer identifies the exact build and the pinned Slint runtime.
3. Choose **Safe / Live** in the header, or **Start → EmberLights → EmberLights Safe - Live**, for Connections, Autoloops, migration, Diagnostics, Live, hardware tools, and every legacy workflow that has not yet crossed the replacement-shell acceptance gate. This opens the frozen Win32 bridge as a separate process; it is deliberately not the default product presentation.
4. All DMX output remains disabled in a new project. The replacement shell cannot open an output adapter unless it was explicitly launched with a project and the internal physical-preview authority was armed; ordinary installer/file-association launch keeps fixture preview locked. Safe / Live retains the established explicit Connections and Start Show controls.
5. Build and validate a project before enabling output in Safe / Live:
   - choose or create a Fixture Profile, use **Import Fixture File (.qxf)...**, or search the official Open Fixture Library directly on Profiles and choose **Download + Import Selected**;
   - add each fixture under Patch with the correct universe, DMX address, and optional role tags;
   - create reusable Groups from the stable fixture IDs shown in Patch;
   - create Static Looks using `fixture-id,property,value` or `group-id,property,value` rows;
   - create Autoloops from those look IDs;
   - save the project as an `.emberlights` file;
   - use **Show → Validate Project**.
6. Use a visualizer or isolated test node before connecting production fixtures. Confirm universe numbering and address maps independently.
7. Under Connections, choose one or more output paths:
   - enable Art-Net or sACN and enter the receiver address; sACN accepts `multicast` as its destination;
   - for an ENTTEC DMX USB Pro or compatible interface that Windows exposes as a COM port, choose its port for universe 1 or universe 2. Choose **Refresh MIDI + USB-DMX** after connecting a device. One single-universe device cannot be assigned to both universes, and projects using this adapter are capped at 40 Hz.
   - for the SoundSwitch Micro (`VID_15E4/PID_0053`), close SoundSwitch and assign **SoundSwitch Micro (WinUSB)** to universe 1 or 2. The protocol is fixed to **SoundSwitch native JLS1**; the disproved preview.310 A/B/C choices no longer appear.
   - press **Save & Apply Connections**. This validates and atomically saves every setting on the page into the project. If Runner is active and the adapter graph changed, EmberLights performs the normal zero-frame stop and restarts it from the saved settings.
8. Start the show from Live. Confirm clock, adapter, frame, error, and jitter state under Diagnostics before triggering content.

Imported QXF/OFL modes are read-only snapshots with recorded provenance. Duplicate one to make an editable local profile. Official catalog downloads retain the OFL key/source URLs, MIT attribution, exact QXF SHA-256, and adapter version, but remain **unreviewed** because catalog/import success cannot prove the selected physical mode or fixture behavior. Verify every imported channel against the fixture's official DMX chart—especially shared shutter/strobe functions, hazardous output, custom lanes, and multi-head fixtures—before enabling physical output. See `docs/16_QLC_FIXTURE_IMPORT.md` for the exact conversion and quarantine rules.

Profiles use a structured channel table and one shared semantic parameter catalog. Select a row to load its Property, Encoding, Fine, Min, Max, and Default fields. Properties are grouped as Intensity, Color, Position, Beam, Image, Effect, Atmosphere, and Custom—the same stable semantics used by Static Looks, Autoloops, Live overrides, MIDI/controllers, and future skins. **Apply Safe Defaults** immediately maps a direct emitter or other conservative linear control without typing ranges. It refuses strobe, shutter, wheel, macro, rotation, hazard, and Custom functions until their exact DMX-chart range/default is entered. **Add / Replace Channel** applies fully manual field edits. The audit names open footprint slots, chart-defined rows, safety restrictions, repeated semantics, and Custom rows. Templates remain starting points rather than fixture truth; QXF or an exact official OFL result remains preferable when available.

White and Amber no longer have a permanent repair button. Core compiler/renderer regressions prove that each semantic reaches the offset in the active saved profile; the application does not globally invert those colors. If the physical fixture responds backwards:

1. In Fixture Patch, identify the exact profile used by the affected fixture. In Profiles, **PATCH USAGE** must list that fixture, universe, and address.
2. If the profile is built-in or imported, choose **Duplicate to Edit**. Do not edit an unused copy.
3. Select the physical channel that produces amber, choose **Color • Amber**, then **Apply Safe Defaults**. Select the physical channel that produces white, choose **Color • White**, then **Apply Safe Defaults**.
4. Choose **Save Profile**. When prompted, choose **Yes** to atomically rebind every listed fixture from the immutable source to this validated/compiled Local copy.
5. While normal Live is stopped, preview isolated White and Amber at low output. Save the project only after both outputs and blackout match the physical fixture mode.

The manufacturer-backed BO-IR4 6CH definition remains R/G/B/White/Amber/UV (White CH4, Amber CH5) because that is the published chart. A unit or firmware that behaves differently should use a separately named Local physical variant; do not silently rewrite the immutable source snapshot.

## Static Look hardware preview

While normal Live is stopped, Studio → Static Looks can preview the selected fixture/group on the configured output:

1. Select a patched fixture or non-empty group and apply at least one Full Color, swatch, or property.
2. Choose **Preview Selected Target on Fixtures** and acknowledge the bounded-output warning.
3. Apply colors and properties normally. The active physical preview recompiles and updates those edits in realtime without extending its deadline.
4. Choose **STOP PREVIEW**, leave Static Looks, or wait for the 30-second timeout. Each route stops through Runner's terminal blackout frames.

The physical authoring preview keeps only the selected target, zeros profile defaults, disables OS2L/MIDI/Autoloops/Track Scripts, caps direct emitters and master at 35%, and refuses positive strobe/fog/haze/laser/spark/custom output or profiles with unsafe nonzero constant channels. The Live page labels this state **STUDIO HARDWARE PREVIEW** instead of claiming the show is running. It is useful visual feedback, not fixture qualification; use the Raw Hardware Test for channel/transport evidence.

## Advanced manual DMX test

Use **Start → EmberLights → Advanced Manual DMX Test** when an unknown or newly acquired fixture needs literal channel/range inspection before a trustworthy profile exists. This is an Advanced diagnostic, not normal fixture or Look authoring. Close SoundSwitch and EmberLights, isolate only the intended fixture, and disconnect hazardous or unrelated devices before arming.

The first installed mode targets a selected SoundSwitch Micro universe. It requires an exact plan-bound acknowledgement before opening the adapter, sends blackout before arming and before every replacement preset, displays the exact held channel/value projection and frame SHA-256, limits one preset to 64 channels, auto-blackouts every nonzero preset after a configurable 1–30 second hold, and ends the whole session after ten minutes. `BLACKOUT NOW`, `QUIT`, Escape/Ctrl+C, device loss, write failure, timeout, and normal destruction all enter its blackout/close path. See `docs/47_ADVANCED_MANUAL_DMX_TEST.md` for command syntax and the evidence boundary.

The native USB paths implement the published single-universe DMX USB Pro serial framing and an independently implemented SoundSwitch Micro WinUSB adapter. For the Micro, Diagnostics deliberately says **Open**, then separately reports accepted native frame writes, write failures, the last Windows error, and the number of nonzero rendered slots. Open/accepted writes prove host-side progress; only a responding physical receiver proves DMX interoperability. SoundSwitch must remain closed while EmberLights owns the Micro.

Before assigning the Micro to a normal project, run **EmberLights Hardware Test** from the Start menu with the isolated IR-4 bench in `docs/MORNING_HARDWARE_TEST.md`. The guided test now includes raw output, compiled Runner output after a clean reopen, and an unplug/replug recovery stage through the same production session lifecycle. Its Desktop report names the first incomplete gate and only reports `passed` after operator-confirmed red and blackout in all three stages.

Preview 95 also installs **IR-4 6CH Editable Bench** in the EmberLights Start-menu folder. That project is a separate Local, manual-derived diagnostic fixture profile with Blackout/Red/Green/Blue/White/Amber looks; every physical output is disabled in the distributed file. Follow `docs/IR4_6CH_RUNNER_FRAME_TEST.md`, save a working copy, and explicitly apply only the isolated Micro U1 route before starting Runner. Diagnostics then reports the exact pre-blackout and routed two-universe frame hashes, nonzero channels, fixture/profile/mapping/property attribution, and per-backend attempted/accepted results. When one of the bench looks is active, it also compares the routed frame with the immutable manual reference (White CH4, Amber CH5).

An exact Diagnostics comparison establishes software-frame parity only. It does not prove that the selected fixture mode, cable, interface, or physical light interprets that channel as expected. If Diagnostics reports White on CH4 and Amber on CH5 while the fixture visibly does the reverse, use the general Local profile channel workbench to exchange those two compatible functions, save the profile/project, then Stop Show and Start Show before repeating the test. Preserve both reports rather than changing the immutable built-in reference.

## VirtualDJ and MIDI

- Same-computer VirtualDJ/OS2L defaults to `127.0.0.1:9996`. EmberLights now advertises `_os2l._tcp` through native Windows DNS-SD while its listener is active. In VirtualDJ Settings → Options, first try `os2l=Auto` with `os2lDirectIp` blank. Start the EmberLights show, restart VirtualDJ once after changing those options, and confirm Discovery becomes **Ready** and OS2L moves from **Waiting** to **Ready** without pressing a DMX pad.
- If automatic discovery is unavailable on the test machine, use Connections → **Copy VirtualDJ Setup**. The fallback sets `os2l=Yes`, uses the displayed direct endpoint, and supplies this Keyboard-mapper ONINIT action: `wait 100ms & os2l_button 'EmberLights Keepalive' off`. `EmberLights Keepalive` is a reserved no-op: it opens VirtualDJ's direct-IP path without clearing an intentional blackout, changing a Look, or consuming a performance mapping.
- After **Start Show**, Live and Diagnostics must show OS2L **Waiting** with the actual listening endpoint before VirtualDJ connects, then **Ready** after the TCP client connects. **Discovery** reports the independent DNS-SD advertisement state. Diagnostics records separate socket and discovery errors; a listener port of `0` means the endpoint is not open.
- A VirtualDJ action `os2l_button "Exact Look Name"` activates/releases that Static Look through the same target-aware state used by Live and MIDI. `os2l_button "Look: Exact Look Name"` is the explicit form; use `os2l_button "Autoloop: Exact Autoloop Name"` for an Autoloop. Names and capitalization must match the active project. A delayed off event for an older target cannot clear a newer selection.
- Diagnostics records dropped named OS2L actions. Keep that count at zero during qualification; a nonzero value means the bounded input queue overflowed and the session is not clean evidence.
- For a separate lighting computer, bind OS2L to that computer's private LAN address and try automatic discovery on the same private network. If discovery is filtered, configure VirtualDJ's direct OS2L destination to match and use the copied ONINIT fallback. Do not expose the listener to an untrusted network.
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

The installer is not code-signed yet, so Windows may show an unknown-publisher warning. Signing, cross-version upgrade/rollback validation, and public distribution are release gates rather than hidden limitations. A matching payload hash, startup smoke, or synthetic qualification report proves package identity and software execution only; it does not graduate a hardware attempt or establish physical output.

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
