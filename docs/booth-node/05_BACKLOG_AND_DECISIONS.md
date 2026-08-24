# LLE Booth Node — Backlog and Decisions

Last updated: 2026-08-24

This ledger keeps the Booth project focused. It records the accepted architecture, exact execution order, hardware-day unknowns, and explicit exclusions so unrelated Love & Light systems do not enter the Booth install or backlog.

## Booth project boundary

### Included

- ReadyNet routed/private show LAN;
- dedicated Windows Booth Node;
- exact pinned QLC+ V23 deployment;
- Control One MIDI/LED behavior;
- SoundSwitch Micro and Control One DMX output;
- wired VirtualDJ OS2L;
- authenticated built-in QLC+ browser operation;
- headless startup and recovery;
- primary REV7/VirtualDJ event recording plus verified Booth copy;
- a deliberately simple emergency audio-playback capability after lighting is qualified;
- minimal read-only Booth/network/QLC+ health evidence;
- power, cable, configuration-backup, and rollback hardening.

### Not part of this Booth project

- Twenty or any CRM/business-data stack;
- Portal hosting or business-manager infrastructure;
- event-data cache/synchronization;
- surveys, quizzes, forms, or guest requests;
- Guestbook Hotline, voice-request processing, or telephony;
- EmberShare, guest Wi-Fi, captive portals, or guest-facing pages;
- offline staff chat, voice, or push-to-talk;
- feedback-suppression DSP;
- cross-system show orchestration;
- a general-purpose application, container, database, or service host.

These exclusions are not “later Booth features.” They require separate plans, owners, threat/failure models, and—where appropriate—separate hardware. Do not install or stage them on the Booth Node merely because it is available.

## Controlling architecture decisions

| Decision | Status | Rationale |
|---|---|---|
| ReadyNet is the Booth gateway/switch/upstream failover device | Accepted | Stable local production network using existing hardware |
| ReadyNet remains routed/NAT and DHCP authority | Binding | Venue networks cannot own or renumber show devices |
| DJ laptop and Booth Node use wired Ethernet | Binding | Primary show control must not depend on Wi-Fi |
| QLC+ V23 runs on the Booth Node | Accepted; hardware deployment pending | Removes lighting workload and show USB devices from the DJ laptop |
| QLC+ remains the only lighting application | Binding | No second runtime, bridge daemon, or revived EmberLights app |
| Control One connects directly to Booth Node | Accepted | QLC+ is its consumer; avoids network MIDI forwarding |
| Micro/Control One DMX connects directly to Booth Node | Accepted | Keeps lighting I/O with the lighting runtime |
| Universe 1 is physical; Universe 3 stays private/internal | Binding | Preserves the full-frame Priority Look design without duplicate physical output |
| Normal Booth operation has no local monitor | Binding | DJ-laptop browser is the normal visual surface |
| Built-in QLC+ web UI is the first/only planned console | Binding for current phase | No custom dashboard until a proven Booth-only gap exists |
| Guest/untrusted devices never share the show segment | Binding | Security and stability |
| QLC+ web is never exposed to WAN/LTE | Binding | Basic HTTP auth is acceptable only on the isolated private LAN |
| Primary recording writes to DJ-laptop local SSD | Accepted for validation | Capture survives Booth/router failure |
| Primary archival format starts at 24-bit FLAC | Accepted for validation | Lossless quality with lower storage cost than WAV |
| Booth copy happens only after file close and SHA-256 match | Accepted for validation | Redundancy without live-network dependence or quality loss |
| Automatic software/driver/firmware updates freeze after qualification | Binding | Reproducible show system and rollback integrity |
| DJ-laptop QLC+ remains installed until Booth promotion | Binding | Physical rehearsed rollback is mandatory |

## Current execution order

### Priority 0 — Preserve V23 truth and rollback

Deliverables:

- preserve V20, V21, V22, and V23;
- preserve the exact compatibility hashes;
- preserve the plug-in installer receipt/backup;
- finish the short owner V23 console observation;
- keep the current DJ-laptop QLC+ path intact;
- do not redesign QLC+ Function ownership, public IDs, logical channels, fixture patch, or private Universe 3 behavior.

Exit gate: the source package and both software/hardware rollback paths are identifiable and recoverable before Booth changes begin.

### Priority 1 — Inventory exact hardware

Use `Get-LLEBoothInventory.ps1` on the Booth Node and DJ laptop, then complete the generated manual worksheet.

Required facts:

- computer make/model, Windows edition/build, CPU/RAM, storage/free space/health;
- wired NIC and USB-controller/physical-port topology;
- exact ReadyNet label model, hardware revision, firmware, mode, and config-backup location;
- QLC+ source/package/workspace/rollback locations and hashes;
- Control One/Micro Windows driver/device state;
- cable and power-supply identities;
- sensitive values stored privately, not in Git.

Exit gate: claim only **inventory captured**. No firmware or production-host change occurs from assumptions.

### Priority 2 — Dedicated lighting appliance

Deliverable:

```text
power/login Booth Node
  -> exact pinned QLC+ V23 opens
  -> Control One works
  -> one selected SoundSwitch DMX output works
  -> built-in QLC+ web works
  -> normal operation needs no Booth monitor
```

Required order:

1. copy the complete coherent QLC+ folder;
2. validate the V23 package;
3. install the build-matched plug-in with QLC+ closed;
4. reproduce V23 locally with a temporary display;
5. prove physical Universe 1 output and keep Universe 3 un-routed;
6. prove authenticated web operation manually;
7. install guarded at-logon startup only after manual success;
8. cold/warm boot without a display.

Exit gate: Gates A, B, and D in the validation runbook pass on the exact Booth computer.

### Priority 3 — Private ReadyNet LAN and wired OS2L

Deliverables:

- stable routed private subnet;
- ReadyNet DHCP reservations for DJ and Booth;
- wired DJ-to-ReadyNet-to-Booth path;
- QLC+ OS2L TCP 9996 and web TCP 9999 restricted to the Private local subnet;
- VirtualDJ direct target on Booth address;
- five-second unmapped `QLC KEEPALIVE` reconnect mapper;
- local lighting/browser/OS2L operation with WAN, LTE, and all Internet removed;
- upstream changes never renumber local devices.

Upstream qualification order:

1. venue Ethernet into ReadyNet WAN;
2. LTE;
3. no Internet while the LAN remains operating;
4. Wi-Fi-as-WAN only after the exact ReadyNet proves it remains routed/NAT.

Exit gate: Gates C and E plus the ReadyNet fault rows pass. Reject bridge/repeater behavior that hands addressing to the venue.

### Priority 4 — Hardware recovery and combined soak

Deliverables:

- Control One starts before/after QLC+;
- Control One unplug/replug restores MIDI and known LEDs without QLC+ restart;
- Micro unplug/replug recovers;
- Control One DMX 1 and DMX 2 are proven separately;
- any intended simultaneous-output topology is proven exactly as used;
- DJ Ethernet, ReadyNet, WAN/LTE/no-Internet, browser, and QLC+ recovery drills are recorded;
- rehearsed Booth-to-DJ-laptop rollback;
- two-hour combined VirtualDJ/audio/OS2L/QLC+/MIDI/LED/DMX/browser soak.

Exit gate: Gates F and G pass with no unexplained audio dropout, QLC+ crash/starvation, DMX corruption, USB error storm, frozen Control One, or LAN packet loss.

### Priority 5 — Highest-quality event recording

Deliverables:

- REV7 `USB 5/6 Mixer Output = MIX(REC OUT)` physically verified;
- VirtualDJ primary capture to DJ-laptop local SSD;
- 24-bit FLAC default, or WAV only for a specific downstream requirement;
- intended microphones/sources present and no worst-case clipping;
- adequate free space;
- recording survives Booth shutdown and ReadyNet reboot;
- completed-file copy to Booth only after close/finalization;
- source/destination SHA-256 match and manifest;
- privacy/consent/retention decision before standard production use.

The later REV7 USB-B independent recorder experiment is allowed only as a secondary capture and only after it proves simultaneous driver exposure and no effect on DJ audio/controller operation.

Exit gate: Gate H passes for the primary capture and verified copy.

### Priority 6 — Emergency backup playback

Goal: allow deliberate playback of a small critical-event audio set through an independent labeled mixer input if the DJ laptop fails.

First scope only:

- introductions;
- ceremony cues when relevant;
- first dance;
- parent dances;
- cake/anniversary/last dance;
- a small emergency background/open-dance set;
- verified local files;
- simple manual playback control;
- inexpensive independent Booth audio output;
- output muted until deliberately selected.

Guardrails:

- never auto-play or auto-failover;
- do not make Booth a second full DJ workstation;
- QLC+ retains resource and recovery priority;
- the playback application must be lightweight and independently stoppable;
- file sync/verification occurs before departure, never by assumption;
- failure cannot disturb QLC+, Control One, DMX, OS2L, or DJ-laptop audio;
- this capability receives its own two-hour combined test before event use.

No playback application is selected until the Booth machine inventory and lighting soak establish available OS/audio endpoints and headroom.

### Priority 7 — Minimal Booth health visibility

Start with evidence and existing interfaces:

- ReadyNet gateway reachable;
- Booth ping/loss/latency;
- QLC+ TCP 9996/9999;
- QLC+ process count/state;
- Booth disk free space;
- scheduled-task/launch-log state;
- recording-copy verification status;
- Control One/Micro presence where Windows exposes it;
- WAN/LTE state only if the exact ReadyNet provides a safe supported read-only method.

The existing inventory and connection-test scripts are the first implementation. A small read-only Booth-only status page may be considered later only if these tools and QLC+ web leave a proven operational gap.

Guardrails:

- no second control authority;
- no database/container stack merely for green checks;
- no business/event/client data;
- no WAN exposure;
- no watchdog that restarts QLC+ during a live moment;
- no polling load that affects audio, OS2L, MIDI, or DMX.

### Priority 8 — Operations hardening

After the Booth is stable:

- small qualified UPS for ReadyNet/Booth;
- labeled power sequence and boot-after-power-loss test;
- cable map and tested spares;
- ReadyNet config export stored privately;
- complete coherent QLC+ folder and V23/V22 recovery copies;
- Windows task/firewall/power-state export;
- periodic inventory comparison;
- controlled Windows/driver/firmware maintenance windows;
- recovery-USB integrity check;
- ventilation, transport, mounting, and strain-relief plan.

These improve the Booth appliance itself and remain within scope.

## Explicitly rejected

- reviving standalone EmberLights;
- running QLC+ plus another lighting runtime;
- a custom bridge/daemon for normal show operation;
- replacement Control One firmware;
- direct primary recording to SMB/network storage;
- live network audio as the only/master recorder;
- unqualified USB hubs, docks, extenders, or long runs;
- guest devices on the show LAN;
- exposing QLC+ web control to WAN/LTE;
- bridge/repeater mode that delegates DHCP to the venue;
- automatic QLC+, plug-in, Windows driver, or ReadyNet firmware updates;
- installing CRM/Portal/event-cache/guest/telephony/staff-chat systems on Booth;
- Docker/Kubernetes/database infrastructure on Booth;
- live AI/DSP in the QLC+/DMX or primary DJ audio path;
- cross-system orchestration before each independent system is qualified;
- calling the Booth “working” or “gig-qualified” from documentation/software evidence alone.

## Hardware-day unknowns to close

- exact Booth computer make/model and Windows edition/build;
- CPU/RAM/storage type, health, and capacity;
- wired NIC and USB controller/physical port topology;
- exact ReadyNet variant: LTE520 versus LTE520S;
- ReadyNet hardware revision and firmware;
- whether its Wi-Fi upstream mode preserves routed NAT/DHCP;
- final stable private subnet and reservations;
- exact path/source of the working complete QLC+ folder;
- Control One/Micro manufacturer driver state;
- QLC+ web-auth behavior on the pinned Windows build;
- interactive Booth-account/autologon security approach;
- full Windows remote-administration choice, if any;
- recording-share storage and least-privilege credential design;
- microphone/aux inclusion in the REV7 recording return;
- whether REV7 USB B exposes simultaneous independent `MIX(REC OUT)`;
- whether a suitable UPS and independent Booth audio output already exist.

Close these by observation and bench evidence, not inference.

## Continuity rule after each work session

Update, in the same change:

1. the relevant runbook when steps or behavior change;
2. this ledger when priority/scope/decision state changes;
3. the validation document when a new failure mode or gate appears;
4. exact hashes/compatibility tuple when a runtime artifact changes;
5. private qualification evidence for physical tests;
6. PR status/description with precise claim language;
7. rollback location and rehearsal status.

Do not record private machine identifiers or client data in Git. Do not let a future idea silently become a Booth dependency.
