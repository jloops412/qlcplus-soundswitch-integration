# LLE Booth Node — Backlog and Decisions

Last updated: 2026-08-24

This ledger prevents good ideas from being forgotten without turning all of them into immediate scope.

## Controlling architecture decisions

| Decision | Status | Rationale |
|---|---|---|
| ReadyNet becomes the permanent booth gateway/switch/upstream failover device | Accepted | Stable event network and reuse of existing hardware |
| DJ laptop and booth node use wired Ethernet | Accepted | Show control should not depend on Wi-Fi |
| QLC+ runs on the booth node | Accepted; deployment pending | Removes lighting workload and USB complexity from the DJ laptop |
| Control One connects directly to the booth node | Accepted | QLC+ is its current consumer; avoids network MIDI forwarding |
| Micro/Control One DMX connects directly to booth node | Accepted | Keeps lighting I/O with lighting runtime |
| QLC+ remains the only lighting application | Binding | No bridge daemon or revived EmberLights runtime |
| Normal booth-node operation requires no local monitor | Binding | DJ laptop browser is the operating surface |
| Built-in QLC+ web UI is the first control interface | Accepted | Avoid custom dashboard until a real gap is proven |
| Show LAN remains routed/NAT and private | Binding | Venue DHCP/bridge behavior cannot own show addressing |
| Guest traffic never shares the show segment | Binding | Security, stability, and LTE-cost protection |
| Primary event recording writes to DJ-laptop local SSD | Accepted for validation | Capture remains independent of booth/router failure |
| Primary archival format begins with 24-bit FLAC | Accepted for validation | Lossless quality and smaller files than WAV |
| Booth receives a hash-verified completed-file copy | Accepted for validation | Redundancy with no quality loss or live transport dependency |
| Live feedback suppression is a separate audio appliance | Accepted | Experimental DSP must not threaten QLC+ or DJ playback |
| Automatic software/driver/firmware updates are frozen after qualification | Binding | Reproducible show system and rollback integrity |

## Current execution order

### Priority 0 — Preserve current V23 truth

- keep V20/V21/V22/V23 and installer receipt;
- do not redesign QLC+ control ownership;
- finish the short V23 owner observation;
- preserve exact compatibility hashes;
- keep DJ-laptop lighting as rollback.

### Priority 1 — Dedicated lighting appliance

Deliverable:

```text
power/login booth node
  -> exact V23 opens
  -> Control One works
  -> selected SoundSwitch DMX works
  -> QLC web works
  -> no second monitor
```

Acceptance is controlled by the deployment and validation runbooks.

### Priority 2 — Wired network OS2L and headless browser operation

Deliverable:

- stable private IP reservations;
- OS2L direct target on booth node;
- five-second reconnect keepalive;
- authenticated V23 browser console on DJ laptop;
- no dependence on venue Internet;
- LAN survives WAN/LTE changes.

### Priority 3 — Highest-quality event recording

Deliverable:

- verified REV7 `MIX(REC OUT)` source;
- 24-bit local FLAC/WAV capture;
- microphone/source routing and headroom test;
- sufficient storage;
- post-close SHA-256-verified booth copy;
- privacy/retention decision before standard production use.

This was moved ahead of generalized monitoring/cache work because the owner specifically values archival quality and the REV7 provides a low-complexity digital path.

### Priority 4 — Emergency backup playback

Goal: allow the booth node to play critical event audio through an independent mixer input if the DJ laptop fails.

First scope only:

- introductions;
- ceremony cues when relevant;
- first dance;
- parent dances;
- cake/anniversary/last dance;
- a small emergency background/open-dance set;
- simple, obvious playback interface from the DJ laptop;
- separate inexpensive audio output into a labeled mixer channel.

Guardrails:

- no attempt to make the booth node a second full DJ workstation initially;
- no auto-failover that can unexpectedly play audio;
- files synced and verified before leaving;
- emergency output stays muted until deliberately selected;
- QLC+ receives priority over non-critical services.

### Priority 5 — Minimal booth health visibility

Start with evidence, not a custom platform:

- ReadyNet reachable;
- booth ping/packet loss;
- QLC TCP 9996/9999;
- QLC process state;
- disk free space;
- recording-copy status;
- Control One/Micro device presence where Windows exposes it;
- WAN/LTE state if the ReadyNet offers a safe supported status interface.

A small read-only page may be justified later. It must not become a second control authority or require a database/container stack merely to display green checks.

### Priority 6 — Local event resilience/cache

Goal: retain a read-only last-known-good event pack locally while the DJ laptop continues using Twenty, EmberShare, Drive, and normal web tools.

Potential cached fields:

- event name/date/locations;
- timeline;
- client and venue contacts;
- vendor contacts;
- introductions and pronunciation notes;
- special songs;
- ceremony notes;
- announcements;
- emergency/contingency details;
- selected files/attachments.

Guardrails:

- read-only cache first;
- no second CRM or conflicting write authority;
- encrypted/private data at rest where appropriate;
- clear “last synchronized” timestamp;
- event purge/retention policy;
- useful from the DJ laptop browser when Internet is absent.

## Parked easy wins

### Guestbook Hotline analog phone

The ReadyNet FXS ports and existing Guestbook Hotline configuration make this a likely easy win. It remains parked until lighting deployment is stable.

Potential deployment:

```text
physical phone -> ReadyNet FXS/SIP -> Guestbook Hotline
```

### Voice request line -> Ask The DJ

Owner concept:

```text
caller message
  -> transcription
  -> request-intent extraction
  -> artist/title cleanup
  -> fuzzy catalog resolution
  -> explicit/duplicate/spam checks
  -> VirtualDJ Ask The DJ
```

Most business-side pieces may already exist. It remains parked to avoid mixing telephony/NLP work into the lighting deployment.

### EmberShare event Wi-Fi entry point

Potential guest flow:

```text
event SSID / QR
  -> protected, isolated, rate-limited guest network
  -> optional LLE/event landing page
  -> event-specific EmberShare guest page
```

Requirements before work:

- a truly isolated guest segment;
- bandwidth and LTE-usage limits;
- no route to QLC+, SMB, router administration, or show devices;
- captive-portal behavior tested across modern phones;
- offline behavior defined;
- client-facing copy/branding decision.

The current ReadyNet may not provide all isolation/captive-portal controls safely enough. No guest SSID is created on the production show segment.

### Offline staff voice/chat/PTT

Goal: local communications without Twilio, cellular data, or paid per-minute service.

Possible future approaches:

- ReadyNet FXS/SIP for wired analog extensions;
- local SIP PBX and phone apps;
- browser/PWA text and push-to-talk;
- WebRTC voice rooms;
- dedicated local walkie-talkie-style interface.

Required characteristics:

- fully local operation;
- simple join/identity flow;
- no guest access;
- clear push-to-talk behavior;
- usable on staff phones;
- does not consume the lighting node's critical resources;
- no assumption that phone OS background restrictions will permit reliable PTT without testing.

### Physical emergency/operations phone

A wired analog phone on the ReadyNet remains attractive for:

- Guestbook Hotline test/use;
- operations line;
- emergency backup contact path;
- later local extension/intercom.

Parked pending telephony inventory and exact ReadyNet model/VoLTE/SIP configuration.

## Separate-project integration boundary: feedback suppression

The wireless/remote automatic feedback-suppression project is valuable but technically riskier than QLC+ separation.

Architecture boundary:

```text
ceremony microphones/receiver
  -> dedicated audio interface
  -> dedicated feedback-suppression audio node
  -> PA/mixer

booth LAN -> remote UI/status only
```

Rules:

- do not run real-time feedback suppression on the lighting node during the beta;
- do not route primary DJ playback through experimental DSP;
- fail safely/bypass cleanly;
- preserve a direct analog recovery path;
- measure end-to-end latency, sound quality, CPU headroom, feedback detection, false positives, and crash behavior;
- the booth system may later show status/control, but QLC+ must not depend on it.

## Practical future ideas worth retaining

### Power resilience

- small UPS for ReadyNet and booth node;
- graceful shutdown policy;
- labeled power sequence;
- router/booth power separated from lighting fixtures when practical;
- surge protection and strain relief;
- boot-after-power-loss behavior.

### Configuration backups

- ReadyNet config export;
- complete QLC+ pinned folder;
- V23/V22 workspaces;
- Control One profile;
- plug-in installer receipt/backup;
- Windows task/firewall export;
- recording manifests;
- hardware inventory and cable map.

### Local names

Human-friendly names such as `lighting.lle` or `booth.lle` would be convenient, but raw reserved IP bookmarks are the first reliable baseline. Local DNS/mDNS is deferred until it can be made deterministic without adding another fragile service.

### Event/system log

A local append-only event log could eventually record:

- boot/shutdown;
- QLC start/stop;
- OS2L connect/disconnect;
- WAN/LTE changes;
- Control One/Micro reconnects;
- recording start/stop/copy verification;
- operator timeline markers.

Do not log client content, credentials, or more personal data than needed.

### Show-state integration

Longer-term cross-system actions remain interesting:

```text
FIRST DANCE
  -> select QLC+ Priority Look
  -> display the relevant event notes
  -> add a recording marker
  -> optionally update the guest-facing event state
```

This remains explicitly deferred. First establish reliable independent systems and manual authority. Do not build an orchestration engine before the underlying services are stable.

## Explicitly rejected for the current phase

- reviving standalone EmberLights;
- running QLC+ plus another lighting runtime;
- a custom bridge daemon for normal show operation;
- replacement Control One firmware;
- Docker/Kubernetes merely to host a status page;
- direct primary recording to an SMB/network path;
- live AI processing in the QLC+/DMX path;
- guest devices on the show LAN;
- exposing QLC+ web control to WAN/LTE;
- auto-updating QLC+, the SoundSwitch plug-in, device drivers, or ReadyNet firmware;
- moving timelines/CRM editing into the booth node merely because it exists;
- building all future features before the lighting node is gig-qualified.

## Hardware-day unknowns to close

- exact booth computer make/model and Windows edition/build;
- CPU/RAM/storage and free capacity;
- wired NIC and USB topology;
- exact ReadyNet unit: LTE520 vs LTE520S and firmware/hardware revision;
- whether its Wi-Fi repeater mode preserves routed NAT/DHCP;
- final private LAN subnet/reservations;
- exact path of the working pinned QLC+ folder;
- Control One/Micro driver state on the booth computer;
- Windows interactive autologon approach;
- full remote Windows administration choice;
- booth recording share/credential design;
- whether REV7 USB B exposes simultaneous `MIX(REC OUT)` to an independent recorder;
- whether a small UPS is already available.

Close these through observation and bench tests, not assumptions.