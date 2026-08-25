# LLE Booth Node — Owner and Operator System Guide

Status: **system definition and operating guide; physical Booth hardware is not yet gig-qualified**

This is the plain-language map of what the Booth project is, what every major component/file does, how the pieces are intended to operate, and where to start when something fails. Use the numbered deployment and validation runbooks for exact bench steps.

## The one-sentence definition

The Booth Node is a dedicated Windows lighting computer on a private ReadyNet LAN that runs the exact pinned QLC+ V24 system, receives VirtualDJ beat/control data over wired Ethernet, receives Control One commands over USB MIDI, and sends DMX through the SoundSwitch hardware plug-in while remaining controllable from the DJ laptop browser.

## What this project is not

The Booth Node is not:

- Twenty, a CRM, or an event-data authority;
- the Love & Light Portal or another business application host;
- a survey, quiz, request, guestbook, guest Wi-Fi, or captive-portal system;
- an offline staff chat/PTT server;
- a feedback-suppression audio processor;
- a second full DJ workstation;
- the retired standalone EmberLights application;
- a replacement lighting engine, bridge daemon, or custom dashboard.

Those systems can have their own projects and hardware. They are not dependencies, backlog items, or install targets for this Booth Node.

## The complete show-control path

```text
VirtualDJ on DJ laptop
  -- wired Ethernet / OS2L TCP 9996 -->
QLC+ V24 on Booth Node
  -- build-matched SoundSwitch plug-in -->
SoundSwitch Micro or Control One DMX
  --> fixtures

Control One
  -- direct USB MIDI -------------------> QLC+ V24

DJ-laptop browser
  -- authenticated TCP 9999 -----------> QLC+ V24 web console
```

The ReadyNet provides the private routed LAN. Venue Ethernet, LTE, and later-qualified Wi-Fi-as-WAN are optional upstream Internet paths. Internet is not part of the lighting path.

## Physical component catalog

| Component | What it is for | Normal connection | If it fails |
|---|---|---|---|
| DJ laptop | Runs VirtualDJ, audio performance, primary event recording, and the browser lighting console | Wired Ethernet to ReadyNet; REV7 USB | DJ/audio and OS2L are affected; Booth QLC+ can still be operated manually through Control One |
| Booth Node | Dedicated Windows host for QLC+ V24 | Wired Ethernet to ReadyNet; Control One/Micro USB | Use the rehearsed DJ-laptop QLC+ rollback |
| ReadyNet LTE520/LTE520S | Private Booth gateway, DHCP authority, small Ethernet switch, and optional WAN/LTE failover | DJ and Booth on LAN; venue Ethernet on WAN when available | Local browser/OS2L disappear during LAN outage; QLC+/Control One can continue locally |
| Control One | Physical QLC+ performance surface and optional DMX output | Direct USB to Booth Node | Use the QLC+ browser; test reconnect only when operationally safe |
| SoundSwitch Micro | Compact optional DMX output | Direct USB to Booth Node | Use qualified Control One DMX port or rollback output path |
| DDJ-REV7 | Main DJ controller/mixer and digital record-return source | Primary USB to DJ laptop | Outside Booth-lighting ownership; follow DJ/audio recovery plan |
| Ethernet cables | Deterministic OS2L and browser transport | DJ -> ReadyNet LAN; Booth -> ReadyNet LAN | Replace with labeled tested spare; do not substitute show-control Wi-Fi |
| USB cables | Direct Control One/Micro connection | Known physical ports recorded in qualification evidence | Replace one cable/port only when safe; avoid unqualified hubs/docks |
| Temporary display/keyboard/mouse | First install and recovery only | Direct to Booth Node during bench work | Normal gigs must not require them after headless qualification |
| Recovery USB drive | Offline copy of coherent QLC+, V24/V23, docs, receipts, and rollback material | Stored with show kit, not normally attached | Rebuild becomes slower and riskier; verify the kit before each pilot |
| Optional UPS | Keeps ReadyNet/Booth power stable during short interruptions | Qualified power path only | System still needs a known power-up sequence and DJ-laptop rollback |

## Network identity

Recommended clean layout:

| Device | Address |
|---|---:|
| ReadyNet gateway | `10.52.0.1` |
| DJ laptop | `10.52.0.10` reservation |
| backup DJ laptop | `10.52.0.11` reservation |
| Booth Node | `10.52.0.20` reservation |
| ordinary DHCP pool | `10.52.0.100–199` |
| subnet mask | `255.255.255.0` |

An already stable private subnet may be retained. The real requirements are:

- ReadyNet remains DHCP authority;
- DJ and Booth addresses remain stable;
- the LAN works without Internet;
- venue networks never renumber show devices;
- no guest device joins the show segment;
- TCP 9996/9999 are never exposed to WAN/LTE.

## Canonical V24 lighting contract

This is the exact release the Booth staging plan targets:

| Item | Pinned value |
|---|---|
| Published commit | `ed50f76001866d5e0279dc14011e380d68646104` |
| Release tag | `v24` |
| Package | `releases/qlcplus-control-one/v24/` |
| Workspace | `IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw` |
| Workspace SHA-256 | `DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF` |
| Plug-in SHA-256 | `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC` |
| QLC+ | `5.3.0 GIT a124abe` |
| `qlcplus5.exe` SHA-256 | `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533` |

V24 fixes on-screen Bank 1–4 selection, two-way Autoloops/Priority switching, latched Start Bank/Start All, live current-loop highlighting for manual and automatic playback, mouse operation with Control One unplugged, and trailing-zero double-dispatch. Chase multiplier is `0.25x / 0.5x / 1x / 2x / 4x`; Autoplay dwell is `1 / 2 / 4 / 8 / 16 measures`. These controls are independent.

VirtualDJ/OS2L is the only beat source. Do not enable, add, or stage a synthetic or second beat generator. QLC+ is the only lighting runtime; standalone EmberLights remains archived.

Software/runtime validation has passed. Physical lights and Booth-machine qualification remain pending, so this release is staged only and must not be physically deployed before inventory/hardware day.

## Software catalog

### QLC+ `5.3.0 GIT a124abe`

Purpose: the only show-time lighting application.

Expected Booth path:

```text
C:\LLE\QLC\5.3.0-GIT-a124abe\
```

Treat the entire folder as one appliance image. `qlcplus5.exe`, Qt libraries, FFmpeg/runtime DLLs, and plug-ins must come from the same coherent build. Never repair it by mixing files from another QLC+ version.

Pinned `qlcplus5.exe` SHA-256:

```text
16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533
```

### V24 workspace `.qxw`

Purpose: contains the actual lighting fixture patch, Autoloops, Priority Looks, overrides, intensity behavior, Control One/Virtual Console mappings, and QLC+ I/O configuration.

Expected Booth path:

```text
C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw
```

Pinned SHA-256:

```text
DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF
```

V24 is the alpha-candidate target. V23 is the Live Console rollback, V22 the creative rollback, V21 the reliability rollback, and V20 the protected creative baseline. Do not creatively edit the deployment copy while qualifying the machine.

### `SoundSwitch-Control-One-Performance.qxi`

Purpose: tells QLC+ how Control One MIDI inputs/outputs and feedback map to logical controls.

It is a QLC+ input profile, not firmware and not a standalone program. Associate it only where the V24 workflow requires it.

### `soundswitch.dll`

Purpose: the minimal QLC+ plug-in for SoundSwitch Micro/Control One USB DMX transport, Control One MIDI translation/LED feedback, device rescan/hot-plug behavior, and the private full-frame Priority Look mechanism QLC+ cannot express by ordinary merging alone.

Expected installed path:

```text
C:\LLE\QLC\5.3.0-GIT-a124abe\Plugins\soundswitch.dll
```

Pinned SHA-256:

```text
2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC
```

This DLL is build-matched. It is not a universal drop-in for a newer QLC+ installation.

### V24 package folder

Expected Booth path:

```text
C:\LLE\Packages\qlcplus-control-one-v24\
```

It is the install source and immutable release reference. Do not use the package folder as the editable project workspace.

## Every V24 package helper

### `Test-V24Package.ps1`

Purpose: validates the release structure before installation. It parses the workspace/profile, verifies IDs and references, checks the exact fixture patch and V24 console invariants, rejects personal/secrets content, and verifies the allowed V23-to-V24 boundary.

Run it before installing. A failure is a stop condition, not something to bypass.

### `Install-SoundSwitchPlugin.ps1`

Purpose: with QLC+ closed, checks the compatible core, backs up the previous DLL, installs the exact packaged plug-in, verifies the result, and creates rollback evidence.

It installs only the plug-in. It does not configure ReadyNet, Windows startup, OS2L, or QLC+ web users.

### `Rollback-SoundSwitchPlugin.ps1`

Purpose: restores the installer-backed-up plug-in if the new copy must be reversed.

It requires the receipt/backup produced during installation. Preserve that backup until a later release is separately qualified.

### `SHA256SUMS.txt`

Purpose: release-integrity record. Hashes prove file identity; they do not prove the machine, USB hardware, DMX output, LAN, or operator workflow.

## Booth helper scripts

Repository folder:

```text
tools\booth-node\
```

### `Get-LLEBoothInventory.ps1`

Runs on the Booth Node or DJ laptop. Creates a private, sanitized inventory package with JSON, an owner summary, a manual worksheet, and hashes. It changes no system setting. Use it before changes and at qualification checkpoints.

Full instructions: `06_HARDWARE_INVENTORY_AND_EVIDENCE.md`.

### `Install-LLEBoothNode.ps1`

Runs on the Booth Node from elevated PowerShell after manual local QLC+ and network testing pass.

It:

- verifies the pinned core, plug-in, and workspace hashes;
- writes a guarded local QLC+ launcher;
- creates an at-logon scheduled task;
- optionally creates Private/LocalSubnet firewall rules for 9996/9999;
- optionally disables AC sleep/hibernate timeouts;
- optionally starts the task.

It does not configure automatic Windows sign-in, ReadyNet, QLC+ web credentials, VirtualDJ, USB drivers, or DMX routing.

### `Test-LLEBoothConnection.ps1`

Runs on the DJ laptop while QLC+ is active on the Booth Node.

It measures ping success/loss and latency, tests TCP 9996 and 9999, and accepts the expected HTTP authentication challenge as evidence that the QLC+ web endpoint is reachable. It can save JSON evidence.

It does not prove beat timing, physical DMX, Control One, browser usability, or Internet failover; those remain manual gates.

### `Copy-LLERecording.ps1`

Runs on the DJ laptop after VirtualDJ has stopped and finalized the primary local recording.

It waits for an exclusive/stable source file, stages a `robocopy` transfer, compares source/destination SHA-256, promotes only the verified copy, writes JSON/text hash evidence, and never deletes the source.

It is not a live recorder and must not read the growing VirtualDJ file during the event.

## Machine-local folder catalog

| Path | Contents | Owner action |
|---|---|---|
| `C:\LLE\Inventory` | Timestamped private machine captures | Keep private; compare before/after changes |
| `C:\LLE\Packages` | Immutable copied release packages | Do not edit in place |
| `C:\LLE\QLC` | Complete coherent QLC+ installation folders | Pin; update only side-by-side after qualification |
| `C:\LLE\Projects` | Active deployment workspace copy | Back up before intentional edits |
| `C:\LLE\Recordings` | Verified booth-side recording copies | Protect, retain, and purge under the separate recording policy |
| `C:\LLE\Logs` | Qualification/operational evidence | Review after tests and incidents |
| `C:\LLE\Recovery` | V22/DJ rollback, plug-in backup pointer, recovery material | Keep offline copy too |
| `C:\LLE\Qualification\DATE-booth-node` | Evidence for a specific qualification attempt | Never mix failed and passed attempts |
| `C:\ProgramData\LLEBooth` | Generated launcher, QLC web-auth file, local logs | System-local; never publish credentials |

## Generated Windows objects

### `C:\ProgramData\LLEBooth\Start-LLEBoothQLC.ps1`

Generated by `Install-LLEBoothNode.ps1`. It waits briefly after login, refuses to start a duplicate QLC+ process, opens the exact workspace with authenticated web access, and writes a dated launch log.

Do not hand-edit it as the normal configuration method. Rerun the installer helper with deliberate parameters so the generated state is reproducible.

### Scheduled task `LLE Booth - QLC+ V24`

Starts the generated launcher at interactive logon for the chosen Booth account. It is intentionally at-logon, not an unproven pre-login service, because QLC+ is a desktop application and Control One/web behavior must be qualified in that session.

Automatic Windows login is not created by repository scripts. Password handling needs a separate, secure physical-machine decision after cold-boot testing.

### QLC+ web-auth file

Expected path:

```text
C:\ProgramData\LLEBooth\qlc-web-auth
```

QLC+ stores the administrator/operator web configuration there. It is local secret-bearing state. Never commit, copy into the release package, or paste it into documentation.

Create:

- one administrator for configuration/recovery;
- one daily operator restricted to the Virtual Console.

The web connection is HTTP Basic authentication, not HTTPS. It is allowed only because the show LAN is private and isolated.

### Windows Firewall rules

The helper can create:

- `LLE Booth - QLC OS2L` — inbound TCP 9996;
- `LLE Booth - QLC Web` — inbound TCP 9999.

Both are limited to `LocalSubnet` and the Windows Private profile. SMB recording copy and remote desktop are separate decisions and are not silently opened.

## QLC+ universes and ownership

- Universe 1 is the physical show output.
- Universe 3 is a private duplicated fixture layer used to build full-frame Priority Looks.
- Universe 3 must never be routed to a physical output.
- One selected Micro or Control One DMX output is routed first; simultaneous outputs are qualified later as the exact intended topology.
- QLC+ Function state is the lighting authority, including when Control One reconnects.

## Normal power-up sequence

1. Power ReadyNet and allow its private LAN to stabilize.
2. Power the Booth Node.
3. Connect/power the intended Control One and SoundSwitch DMX hardware in the qualified port layout.
4. Sign into the qualified Booth Windows account.
5. Wait for the at-logon QLC+ task.
6. On the DJ laptop, connect wired Ethernet to ReadyNet.
7. Open the bookmarked `http://10.52.0.20:9999` using the operator account.
8. Confirm the exact V24 console and one QLC+ instance.
9. Confirm Control One input/LED state and the selected DMX output.
10. Start VirtualDJ and confirm direct OS2L reaches port 9996.
11. Start primary event recording locally on the DJ laptop only after its own pre-event test.

Use the actual reserved Booth address if different.

## What “normal” looks like

- ReadyNet, DJ, and Booth keep the same private addresses with or without Internet.
- The browser opens the V24 Runtime Feedback Virtual Console and asks for credentials.
- Control One pads, banks, mode, transport, intensity, and known LEDs respond.
- A selected Autoloop or Autoplay owner continues normally.
- Priority Looks temporarily own the complete physical frame, then reveal the still-advancing underlying loop.
- VirtualDJ beat/BPM reaches QLC+ over wired OS2L.
- only one QLC+ process runs;
- Universe 3 stays internal;
- selected DMX output reaches the fixtures;
- the DJ laptop remains the primary local recorder;
- venue Internet loss changes Internet only, not local lighting.

## Show-day quick check

Before doors/program start:

- [ ] labeled ReadyNet/DJ/Booth Ethernet cables connected;
- [ ] ReadyNet gateway and Booth browser reachable;
- [ ] one QLC+ process and exact V24 workspace;
- [ ] Universe 3 not routed physically;
- [ ] Control One input and known LEDs;
- [ ] selected DMX output and safe fixture response;
- [ ] VirtualDJ direct OS2L target and five-second keepalive;
- [ ] no unexpected Windows restart/update pending;
- [ ] DJ-laptop QLC+ rollback files and cables present;
- [ ] 60-second primary recording test contains music and microphone;
- [ ] adequate DJ-laptop recording free space.

Do not promote a failed check with “it will probably be fine.” Use the rollback or simplify the topology.

## Recording operation

Primary path:

```text
REV7 MIX(REC OUT) -> VirtualDJ -> 24-bit FLAC -> DJ-laptop internal SSD
```

Starting settings:

```text
recordFormat = flac
recordBitDepth = 24
recordAutoStart = off
recordWaitForSound = off
recordPauseOnSilence = off
recordAutoSplit = off
recordMicrophone = on
```

The exact microphone/source route must be proven with a physical playback test. After the event, stop/finalize the file, spot-check beginning/microphone/end, then use `Copy-LLERecording.ps1` for the verified Booth copy. Never delete the source merely because the copy command ran; require matching hashes and the manifest.

## Failure cheat sheet

| Symptom | Keep running with | First safe checks | Escalation |
|---|---|---|---|
| Browser unavailable, lighting works | Control One | ping, TCP 9999, reopen/re-authenticate | diagnose after critical program moment |
| OS2L unavailable, QLC+ works | safe manual Autoloop/Priority Look | Ethernet, ping, TCP 9996, keepalive | DJ-laptop local QLC+ if unstable for production |
| Control One unavailable, QLC+ works | browser V24 console | one safe USB reseat, allow rescan | finish with browser; diagnose later |
| Booth QLC+ stopped | safest fixture state/manual recovery | confirm no duplicate, launch prepared task once | rehearse DJ-laptop rollback instead of repeated experiments |
| Booth computer failed | DJ-laptop QLC+ rollback | move labeled USB/DMX, set OS2L localhost | resume with simple safe look first |
| ReadyNet Internet failed | local QLC+/Control One/browser/OS2L | local gateway and LAN state | continue offline; Internet is non-critical |
| ReadyNet LAN failed | local QLC+/Control One | cable/power/router | use local manual lighting or DJ-laptop rollback as rehearsed |
| Primary recording concern | continue DJ/audio safely | verify timer/path/free space when safe | preserve any file; document, never improvise live network master |

Do not restart working lighting during a critical song merely to restore a non-critical status surface.

## Change rules

### Safe to change only through qualification

- ReadyNet subnet/reservations/mode/firmware;
- Windows build, driver, power, login, firewall, or scheduled-task state;
- QLC+ executable/runtime folder;
- SoundSwitch plug-in;
- V24 workspace/I/O mapping;
- Control One/Micro port or cable layout;
- VirtualDJ OS2L target/offset/mapper;
- recording format/input/path;
- simultaneous DMX outputs;
- SMB/remote administration.

For each change:

1. capture the before state;
2. preserve a rollback;
3. change one bounded thing;
4. capture the after state;
5. run the relevant fast regression;
6. rerun affected physical/fault/soak gates;
7. update the dated qualification evidence and current status;
8. never upgrade immediately before an event.

### Never change casually

- public QLC+ Function IDs or logical channels;
- fixture IDs/addresses/modes;
- Universe 3 private-layer behavior;
- QLC+/Qt/runtime/plugin build tuple;
- the unmapped `QLC KEEPALIVE` behavior;
- the DJ-laptop rollback before Booth promotion;
- guest isolation or WAN exposure rules.

## Qualification language

| Claim | What it means |
|---|---|
| Configured | settings/files are present |
| Structurally validated | hashes/XML/references/package checks pass |
| Software-tested | named software behavior passed without claiming physical output |
| Physical-output-tested | named port/device/fixture visibly responded |
| Headless-qualified | cold/warm boot and normal operation passed without Booth display |
| Combined-soak-qualified | exact intended workload passed the full duration |
| Gig-qualified | all required gates, fault drills, rehearsal/pilot, and rollback passed |

Current Booth claim: **pre-hardware deployment staged; not gig-qualified**.

## Ordered first-build path

1. Capture both machines and exact ReadyNet with `Get-LLEBoothInventory.ps1`.
2. Complete manual hardware/router/port/cable evidence.
3. Establish routed private ReadyNet LAN and DHCP reservations.
4. Copy the complete coherent pinned QLC+ folder and V24 package.
5. Run V24 validation; install the build-matched plug-in with QLC+ closed.
6. Reproduce local V24, Control One, and one physical DMX output.
7. Prove authenticated QLC+ browser control manually.
8. Install guarded at-logon startup/firewall/power settings.
9. Point VirtualDJ direct OS2L to the Booth reservation and preserve the keepalive.
10. Run hot-plug, DMX-port, ReadyNet upstream/no-Internet, reboot, and rollback drills.
11. Pass the combined two-hour soak.
12. Prove primary REV7/VirtualDJ recording and hash-verified Booth copy.
13. Run a controlled pilot with the full rollback kit.
14. Only then decide whether the Booth Node becomes the normal lighting host.

## Documentation order at the machine

1. `07_OWNER_SYSTEM_GUIDE.md` — understand the system and files.
2. `06_HARDWARE_INVENTORY_AND_EVIDENCE.md` — capture facts before changes.
3. `01_WINDOWS_DEPLOYMENT_RUNBOOK.md` — perform the build in order.
4. `02_NETWORK_HEADLESS_AND_OS2L.md` — configure and prove the private LAN/browser/OS2L.
5. `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` — run gates and drills.
6. `03_EVENT_RECORDING_ARCHITECTURE.md` — qualify primary capture and verified copy.
7. `05_BACKLOG_AND_DECISIONS.md` — see what is now, next, or explicitly excluded.

When something conflicts, the latest owner directive plus `00_START_HERE.md` and the ordered Booth documents control. Preserve dated evidence of what the physical machines actually passed.
