# LLE Booth Node — Start Here

Status: **pre-hardware deployment staged; not yet gig-qualified**  
Last updated: 2026-08-24

## Purpose

The LLE Booth Node is a dedicated Windows lighting computer on the ReadyNet private Booth LAN. Its first and controlling purpose is to remove lighting from the DJ laptop without creating another lighting application or bridge.

At show time, the supported lighting path remains:

```text
VirtualDJ on DJ laptop -- wired LAN / OS2L --> QLC+ on Booth Node
Control One -- USB MIDI --------------------> QLC+ on Booth Node
QLC+ -- SoundSwitch plug-in ----------------> Micro or Control One DMX
DJ-laptop browser -- private LAN -----------> QLC+ built-in web console
```

The active lighting application is QLC+ only. The retired standalone EmberLights runtime, a second lighting daemon, replacement firmware, and an all-in-one show engine remain out of scope.

## Scope boundary

This repository section covers Booth hardware, ReadyNet networking, QLC+ V24, Control One/Micro, wired OS2L, headless browser operation, recovery, primary recording/verified copy, later emergency playback, and minimal Booth health evidence.

It does **not** cover Twenty, CRM/business data, the Portal, event caches, surveys/quizzes/forms, Guestbook/telephony/voice requests, guest Wi-Fi/captive portals, EmberShare, staff chat/PTT, feedback suppression, or cross-system orchestration. Those are separate projects and must not be installed or staged on the Booth Node.

## Non-negotiable operating rules

1. **No dedicated Booth monitor is required during normal operation.** QLC+ must be controllable from the DJ laptop through its built-in web interface.
2. **DJ-to-Booth show control is wired Ethernet.** Wi-Fi is allowed only as a separately qualified upstream Internet method, not for the primary OS2L path.
3. **The show LAN is private and stable.** Venue Wi-Fi, venue Ethernet, and LTE are upstream choices; none may renumber the DJ laptop or Booth Node.
4. **QLC+ and its SoundSwitch plug-in remain one pinned compatibility bundle.** Do not mix QLC+/Qt/runtime files or move the DLL into an arbitrary newer build.
5. **Lighting remains useful without Internet.** LTE/WAN loss must not interrupt local OS2L, MIDI, QLC+, DMX, or browser control.
6. **The Booth Node is not a general app server.** Only Booth-critical capabilities may be considered, and QLC+ retains resource/recovery priority.
7. **Every production change has a rollback.** Preserve the V24 source package plus V23/V22/V21/V20 rollback material, installer receipt, plug-in backup, coherent QLC+ folder, and known-good DJ-laptop fallback.
8. **Claims follow evidence.** Files and plans do not make the machine headless-, soak-, or gig-qualified.

## Current lighting baseline

The deployment target is the published V24 Runtime Feedback alpha-candidate package:

```text
releases/qlcplus-control-one/v24/
  IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw
  SoundSwitch-Control-One-Performance.qxi
  soundswitch.dll
  Install-SoundSwitchPlugin.ps1
  Rollback-SoundSwitchPlugin.ps1
  Test-V24Package.ps1
  SHA256SUMS.txt
```

V24 is pinned to:

- Published commit: `ed50f76001866d5e0279dc14011e380d68646104`
- Release tag: `v24`
- Canonical package: `releases/qlcplus-control-one/v24/`
- QLC+ UI: `5.3.0 GIT a124abe`
- QLC+ source commit: `a124abebe0b5ad6077727c561a5a0e1f3730810c`
- `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`
- `soundswitch.dll` SHA-256: `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC`
- V24 workspace SHA-256: `DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF`

V24 is structurally validated and software-tested against the pinned QLC+ build. Physical lights, the separate Booth Node, headless operation, fault recovery, and combined soak remain unqualified.

## Build order

### Milestone 0 — Inventory and freeze

Run `Get-LLEBoothInventory.ps1` on both the Booth Node and DJ laptop, complete the generated manual worksheet, identify the exact ReadyNet label/hardware revision/firmware, and preserve the existing QLC+ rollback before changing anything.

Claim after completion: **inventory captured**.

### Milestone 1 — Stable private Booth LAN

Keep ReadyNet in routed/NAT mode with a persistent LLE subnet. Reserve addresses for the router, DJ laptop, and Booth Node. Prove that changing upstream state between venue Ethernet, LTE, and no Internet does not change or interrupt the local show LAN. Qualify Wi-Fi-as-WAN only if the exact router preserves routed DHCP/NAT.

### Milestone 2 — Reproduce V24 locally

Install the exact pinned QLC+ build as one coherent folder, run the no-argument V24 validator, install the bundled V24 plug-in even if V21–V23 previously worked, load the V24 workspace, and reproduce the V24 runtime-feedback behavior before adding headless startup.

### Milestone 3 — Headless operation

Enable the authenticated QLC+ web interface, create the guarded at-logon startup task, prevent sleep on AC power, and prove that cold/warm boot reaches a usable browser console without attaching a display.

### Milestone 4 — Network OS2L

Change VirtualDJ from localhost to the reserved Booth address, preserve the five-second keepalive/reconnect mapper, and validate beat/BPM behavior over wired Ethernet.

### Milestone 5 — Hardware and recovery qualification

Qualify Control One MIDI/LED recovery, Micro, Control One DMX 1, Control One DMX 2, intended simultaneous outputs, cable/hot-plug behavior, QLC+ restart, Booth reboot, router reboot, and rollback to the DJ laptop.

### Milestone 6 — Combined soak

Run VirtualDJ audio, OS2L, QLC+, Control One MIDI/feedback, the intended DMX output, and the browser for at least two hours. Observe audio, QLC+ responsiveness, network loss/latency, USB errors, LED state, and visible DMX output.

### Milestone 7 — Highest-quality event recording

Use the REV7 digital record return and VirtualDJ 24-bit FLAC (or WAV for a specific requirement) as the primary capture. Record to the DJ laptop local SSD, then make a hash-verified completed-file copy to the Booth Node. A second independent digital capture through the REV7 unused USB port is a later physical experiment, not an assumed capability.

### Milestone 8 — Controlled pilot and promotion decision

Run the exact qualified topology at a representative controlled event with the rehearsed DJ-laptop rollback kit present. Record what actually happened before promoting the Booth Node to normal lighting host.

## Read these at the Booth machine

1. `07_OWNER_SYSTEM_GUIDE.md` — what the system is, every major component/file, normal operation, and failure cheat sheet.
2. `06_HARDWARE_INVENTORY_AND_EVIDENCE.md` — exact first capture and evidence handling.
3. `01_WINDOWS_DEPLOYMENT_RUNBOOK.md` — ordered hardware-day build.
4. `02_NETWORK_HEADLESS_AND_OS2L.md` — ReadyNet, addresses, ports, browser, and VirtualDJ target.
5. `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` — gates, fault drills, soak, and DJ-laptop fallback.
6. `03_EVENT_RECORDING_ARCHITECTURE.md` — primary recording and verified Booth copy.
7. `05_BACKLOG_AND_DECISIONS.md` — binding scope, execution order, exclusions, and unknowns.

## Prepared helper scripts

The scripts under `tools/booth-node/` are deliberately narrow:

- `Get-LLEBoothInventory.ps1` creates a private, sanitized before/after evidence package and changes no system setting.
- `Install-LLEBoothNode.ps1` validates the pinned files, creates local-only firewall rules, installs an at-logon QLC+ launch task, and optionally prevents AC sleep.
- `Test-LLEBoothConnection.ps1` runs from the DJ laptop and verifies address reachability plus QLC+ OS2L/web ports.
- `Copy-LLERecording.ps1` copies a completed recording to the Booth Node and verifies SHA-256 equality before calling the copy successful.

They do not install another show runtime, change ReadyNet configuration, store passwords in Git, configure Windows autologon, or automatically promote the Booth Node to production.

## Promotion rule

The Booth Node becomes the default lighting host only after every required gate in `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` passes and the existing DJ-laptop setup remains available as a tested rollback. Until then, use precise claims: configured, structurally validated, software-tested, physical-output-tested, headless-qualified, combined-soak-qualified, or gig-qualified.
