# LLE Booth Node — Start Here

Status: **pre-hardware deployment plan; not yet gig-qualified**  
Last updated: 2026-08-24

## Purpose

The LLE booth node is a dedicated Windows computer on the ReadyNet booth LAN. Its first and controlling purpose is to remove lighting from the DJ laptop without creating another lighting application or bridge.

At show time, the supported lighting path remains:

```text
VirtualDJ on DJ laptop -- wired LAN / OS2L --> QLC+ on booth node
Control One -- USB MIDI --------------------> QLC+ on booth node
QLC+ -- SoundSwitch plug-in ----------------> Micro or Control One DMX
```

The active lighting application is QLC+ only. The retired standalone EmberLights runtime, a second lighting daemon, replacement firmware, and an all-in-one show engine remain out of scope.

## Non-negotiable operating rules

1. **No dedicated booth-node monitor is required during normal operation.** QLC+ must be controllable from the DJ laptop through its built-in web interface.
2. **DJ-to-booth show control is wired Ethernet.** Wi-Fi is allowed for upstream Internet and non-critical phones/tablets, not for the primary OS2L path.
3. **The show LAN is private and stable.** Venue Wi-Fi, venue Ethernet, and LTE are upstream choices; none may renumber the DJ laptop or booth node.
4. **QLC+ and its SoundSwitch plug-in remain one pinned compatibility bundle.** Do not mix QLC+/Qt/runtime files or move the DLL into an arbitrary newer build.
5. **Lighting remains useful without Internet.** LTE/WAN loss must not interrupt OS2L, MIDI, QLC+, DMX, or browser control on the local LAN.
6. **Secondary services earn their way onto the node.** Nothing is added if it can starve, destabilize, or complicate recovery of QLC+.
7. **Every production change has a rollback.** V20, V21, V22, V23, the installer receipt, the plug-in backup, and a known-good DJ-laptop fallback remain preserved.

## Current lighting baseline

The deployment target is the V23 alpha-candidate package:

```text
releases/qlcplus-control-one/v23/
  IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw
  SoundSwitch-Control-One-Performance.qxi
  soundswitch.dll
  Install-SoundSwitchPlugin.ps1
  Rollback-SoundSwitchPlugin.ps1
  Test-V23Package.ps1
  SHA256SUMS.txt
```

V23 is pinned to:

- QLC+ UI: `5.3.0 GIT a124abe`
- QLC+ source commit: `a124abebe0b5ad6077727c561a5a0e1f3730810c`
- `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`
- `soundswitch.dll` SHA-256: `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D`
- V23 workspace SHA-256: `E953C3483EB09D2E600D32495887B27A03021DC47EF7BA5797552C4F5A21547B`

V23 is structurally validated and inherits the software-tested V21/V22 plug-in runtime. It is not yet gig-qualified on the separate booth node.

## Build order

### Milestone 0 — Inventory and freeze

Capture the booth computer model, Windows edition/build, CPU, RAM, storage health/free space, Ethernet adapter, USB controllers, power supply, QLC+ location, SoundSwitch driver version, and attached device identities. Do not make the booth node the production default yet.

### Milestone 1 — Stable private booth LAN

Keep the ReadyNet in routed/NAT mode with a persistent LLE subnet. Reserve addresses for the router, DJ laptop, and booth node. Prove that changing the upstream connection between venue Ethernet, venue Wi-Fi/repeater, LTE, and no Internet does not change or interrupt the local show LAN.

### Milestone 2 — Reproduce V23 locally

Install the exact pinned QLC+ build as one coherent folder, validate the V23 package, install the bundled plug-in with the existing installer, load the V23 workspace, and reproduce current Micro/Control One behavior before adding headless startup.

### Milestone 3 — Headless operation

Enable the authenticated QLC+ web interface, create an at-logon startup task, prevent sleep on AC power, and prove that a cold boot reaches a usable browser console without attaching a display.

### Milestone 4 — Network OS2L

Change VirtualDJ from localhost to the reserved booth-node address, preserve the five-second keepalive/reconnect mapper, and validate beat/BPM behavior over wired Ethernet.

### Milestone 5 — Hardware and recovery qualification

Qualify Control One MIDI/LED recovery, Micro, Control One DMX 1, Control One DMX 2, any intended simultaneous outputs, cable/hot-plug behavior, QLC+ restart, booth-node reboot, router reboot, and rollback to the DJ laptop.

### Milestone 6 — Combined soak

Run VirtualDJ audio, OS2L, QLC+, Control One MIDI/feedback, and the intended DMX output for at least two hours. Observe audio, UI responsiveness, dropped network packets, USB errors, LED state, and visible DMX output.

### Milestone 7 — Highest-quality event recording

Use the REV7's digital record return and VirtualDJ's 24-bit FLAC or WAV recorder as the primary capture. Record to the DJ laptop's local SSD, then make a hash-verified copy to the booth node. A second independent digital capture through the REV7's unused USB port is a later physical experiment, not an assumed capability.

## Documents in this section

1. `01_WINDOWS_DEPLOYMENT_RUNBOOK.md` — exact hardware-day setup sequence.
2. `02_NETWORK_HEADLESS_AND_OS2L.md` — LAN, ports, browser control, and VirtualDJ targeting.
3. `03_EVENT_RECORDING_ARCHITECTURE.md` — archival-quality recording and redundancy.
4. `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` — acceptance gates and failure drills.
5. `05_BACKLOG_AND_DECISIONS.md` — ordered future capabilities and parked ideas.

## Prepared helper scripts

The scripts under `tools/booth-node/` are deliberately narrow:

- `Install-LLEBoothNode.ps1` validates the pinned files, creates local-only firewall rules, installs an at-logon QLC+ launch task, and optionally prevents AC sleep.
- `Test-LLEBoothConnection.ps1` runs from the DJ laptop and verifies address reachability plus the QLC+ OS2L and web ports.
- `Copy-LLERecording.ps1` copies a completed recording to the booth node and verifies SHA-256 equality before calling the copy successful.

They do not install another show runtime, change the ReadyNet configuration, store passwords in Git, or automatically promote the booth node to production.

## Promotion rule

The booth node becomes the default lighting host only after every required gate in `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` passes and the existing DJ-laptop setup remains available as a tested rollback. Until then, all claims must remain precise: configured, structurally validated, software-tested, physical-output-tested, or gig-qualified.