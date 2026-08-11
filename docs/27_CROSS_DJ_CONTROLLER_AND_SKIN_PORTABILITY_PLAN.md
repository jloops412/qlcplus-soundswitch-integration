# Cross-DJ, Controller, and Skin Portability Plan

Status: binding integration/UX direction. VirtualDJ/OS2L remains the first production path; Serato remains the second direct DJ-integration priority after VirtualDJ is stable.

Related:

- `01_PRODUCT_REQUIREMENTS.md`
- `03_ARCHITECTURE.md`
- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `spec/ui/command-state-skin-contract-v0.md`
- `spec/ui/user-customization-and-action-composition-v0.md`

## Objective

A user’s lighting project must not be rewritten merely because they change:

- DJ software;
- DJ controller;
- lighting controller/MIDI surface;
- EmberLights skin/layout;
- same-PC versus separate-PC topology;
- one supported DMX adapter for another.

Portability is achieved through normalized adapters, semantic project content, typed commands/state, and independent controller/skin profiles.

## Separation of concerns

```text
DJ software adapter
  -> normalized transport/mixer/identity state
  -> Runner sync and content selection

Controller profile
  -> normalized input events
  -> binding engine
  -> typed commands

Skin/UI
  -> typed commands and shared state

Project/show
  -> fixtures, groups, Looks, Autoloops, Track Scripts, mappings, policies

Output adapter
  <- rendered universe frames
```

No adapter or skin owns lighting content or layer resolution.

## Normalized DJ adapter contract

### Adapter identity and lifecycle

```text
adapter.kind
adapter.version
adapter.instanceId
adapter.endpoint
adapter.topology: samePc | lan | localAudio | manual
adapter.status: disabled | starting | waiting | connected | stale | fault
adapter.lastError
adapter.lastEventAgeMs
adapter.capabilities
```

### Deck state

The normalized model supports one to four decks even when the current adapter supplies fewer.

Per deck:

```text
deck.present
deck.playing
deck.paused
deck.cueing
deck.trackIdentity
deck.trackMetadata
deck.duration
deck.playhead
deck.beatPosition
deck.bpm
deck.pitch
deck.direction
deck.loop.active
deck.loop.length
deck.slip
deck.sync
deck.upfader
deck.audibleWeight
deck.lastUpdateTimestamp
```

### Mixer state

```text
mixer.crossfader
mixer.crossfaderCurve
mixer.deckRouting
mixer.masterLevel
mixer.deckGain[]
mixer.deckEq[] optional
mixer.externalMixerMode
```

### Clock state

```text
clock.bpm
clock.phase
clock.beatIndex
clock.barIndex optional
clock.confidence
clock.source
clock.healthyAgeMs
clock.timestampQuality
```

### Track identity

Track association is not based only on a display title. Adapters may supply:

- application/library stable ID;
- canonical path hint;
- content hash/fingerprint where available;
- artist/title/remix metadata;
- duration;
- library/database provenance.

The resolver ranks evidence and never silently associates low-confidence identity with exact scripted playback.

## Capability negotiation

Each adapter publishes explicit capabilities, for example:

```text
beatClock
barPhase
trackIdentity
exactPlayhead
seekEvents
loopEvents
cueJumpEvents
reverse
pitch
upfaders
crossfader
fourDecks
feedbackCommands
customButtons
libraryLookup
```

Commands and UI adapt to capability state:

- supported — visible/enabled;
- temporarily unavailable — visible with reason when useful;
- unsupported by adapter — hidden from common path or shown disabled in expert inspection;
- vendor-bound/unknown — clearly labeled research/unsupported.

No skin fakes exact Track Script behavior when the current adapter only provides BPM.

## Synchronization hierarchy in the UI

Display one normalized sync state regardless of adapter:

```text
Exact
Predictive Hold
Audio Fallback
Manual/Tap
Safe Unsynchronized
Recovering
Fault
```

The DJ drawer shows:

- selected adapter/source;
- endpoint/topology;
- capabilities;
- current sync state/confidence;
- track identity confidence;
- last event/error;
- configured fallback chain;
- Retry/Reconnect/Test/Capture where safe;
- exact reason a command/feature is unavailable.

## VirtualDJ first path

### Accepted initial transport

- OS2L beat/BPM/button/command/feedback;
- same-PC and direct-IP LAN operation;
- adapter lifecycle starts/listens automatically when enabled;
- configured endpoint persists in the project/machine scopes defined elsewhere;
- no EmberLights-side requirement to press a pad before the listener exists;
- VirtualDJ-specific `os2l=Yes` or other documented configuration is represented as external-source configuration, not hidden app state.

### Expansion path

The versioned VirtualDJ adapter may add, only through documented/verified mechanisms:

- Network Control;
- database/library information;
- native plugin path;
- richer track identity/playhead/mixer state;
- custom button/pad feedback integration.

Every expansion maps into the same normalized contract.

## Serato second direct path

Before implementation claim:

- identify official/public integration mechanism;
- review licensing/distribution constraints;
- capture deterministic transport/mixer/identity evidence;
- map capabilities/gaps to the normalized contract;
- build replay corpus and fault behavior;
- preserve fallback operation where direct exact data is unavailable.

The UI does not become “Serato mode”; it selects a Serato adapter profile and reports capabilities.

## Other integration tiers

### Direct adapters

Target exact transport/identity/mixer data when a lawful official/public path exists.

### Standard clock/transport adapters

- Ableton Link;
- MIDI Clock;
- MTC;
- documented network/OSC protocols where appropriate.

These may provide clock without track identity. UI and command availability reflect that limit.

### Live audio fallback

On-demand worker provides BPM/phase/confidence when direct clock is unavailable. It never pretends to know track identity or exact playhead.

### Manual

Tap tempo/manual BPM and safe unsynchronized content remain universally available.

## DJ source selection UX

### Automatic default

Use the project’s preferred adapter when available. Auto-connect/reconnect without requiring a separate UI mode.

### Multiple detected sources

- rank project preferred and currently healthy source;
- never switch exact track authority silently during active scripted playback unless policy explicitly permits it;
- show source conflict;
- allow deliberate selection;
- retain fallback chain;
- log source changes/generation.

### Source change while Live

1. Candidate connects and publishes capabilities/state.
2. Sync manager evaluates continuity/confidence.
3. UI shows pending/recovering state.
4. Authority switches only through the approved sync state machine.
5. Track Script behavior pauses/reassociates safely when identity changes.
6. Autoloop/manual content continues according to fallback policy.

## Controller abstraction

A controller is not a skin and does not define project capacity.

### Logical device profile

```text
profile.id
profile.name
profile.version
deviceMatchers[]
inputPorts[]
outputPorts[]
controls[]
layers[]
defaultBindings[]
feedbackMappings[]
physicalLayout optional
capabilities
provenance
```

### Device matching

Persist logical matching evidence, not only unstable system index:

- manufacturer/product names;
- VID/PID when appropriate/public;
- port names;
- optional serial only in app-local scope;
- message signature/capability;
- owner-confirmed alias.

If multiple devices match, ask once and save a local preference. Never write personal device serials into a portable project by default.

## Control One

- generic MIDI is the production path once captured/qualified;
- bundled profile follows verified message capture;
- software and hardware feedback share state keys;
- four physical bank selectors are a pageable window over 64 banks;
- proprietary OLED/DMX/storage remain separate vendor-bound research;
- UI never claims onboard proprietary support without evidence.

## Generic controller migration journey

1. Connect new MIDI/HID device.
2. EmberLights detects ports/matcher evidence.
3. Choose existing profile or **Create from Learn**.
4. Map high-value commands by category.
5. Configure faders/encoders/soft takeover.
6. Configure feedback where available.
7. Run conflict and target validation.
8. Test against dry-run/preview or active safe show.
9. Save reusable controller profile.
10. Optionally select a different skin/custom panel matching the hardware arrangement.

The show project remains unchanged unless project-specific mappings are deliberately authored.

## Skin portability

Skins declare required/optional commands, states, components, and capabilities—not DJ/controller names.

Example:

```text
Reference Live requires:
  autoloop.launch
  autoloop.active.progress
  staticLook.activate
  output.blackout

It may optionally expose:
  transport.exactPlayhead
  mixer.crossfader
```

When a different DJ adapter lacks exact playhead:

- the skin still loads;
- exact-only controls become unavailable/hidden according to declaration;
- Autoloop/manual workflows remain;
- a clear capability explanation appears in Command/State Explorer and DJ drawer.

## Project portability

### Portable project content

- semantic fixture profiles and stable fixture IDs;
- venue/patch/groups/roles;
- Looks/Autoloops/Track Scripts;
- audio content identity with replaceable path hints;
- controller mappings using stable command and semantic target IDs where explicitly project-authored;
- preferred adapter kind/fallback policy;
- logical output routing;
- safety policies.

### Machine-local content

- installed app paths;
- controller/adapter serial and port resolution hints;
- local audio file paths;
- selected monitor/window geometry;
- skin/user overlay preference;
- credentials/tokens;
- network-interface identity.

Export/import clearly separates these scopes.

## Cross-software migration assistant

A future wizard can compare current versus target DJ adapter capabilities:

```text
VirtualDJ profile
  Exact track identity: yes/partial
  OS2L beat: yes
  Crossfader: adapter-specific
  Custom commands: yes

Target adapter
  ...
```

It reports:

- preserved features;
- remapped commands;
- fallback behavior;
- unavailable exact-script features;
- controller bindings requiring relink;
- tests to run before a gig.

It never converts lighting content merely to change DJ software.

## Same-PC and LAN topology

The same adapter configuration model uses explicit endpoints.

### Same PC

- loopback default;
- firewall exposure minimized;
- low setup friction;
- UI shows `This computer`.

### Separate PC

- explicit private-LAN endpoint;
- connection test and latency/packet-loss state;
- interface/network guidance;
- unauthenticated control endpoints restricted to trusted event network;
- no behavior/content difference from loopback.

Topology change may require adapter reconnect but not project/content migration.

## Security

- adapter inputs are parsed/validated/bounded;
- network control does not expose remote administration by default;
- untrusted network cannot invoke unrestricted commands;
- future remote control requires separate authentication/authorization;
- controller profiles/skins cannot open devices directly;
- secrets remain app-local secure storage;
- diagnostics redact sensitive endpoint/path/serial information in ordinary exports unless owner opts in.

## UI requirements across adapters

Every bundled skin must show:

- adapter kind/source;
- status and sync state;
- BPM/confidence;
- exact versus fallback capability;
- active track identity when trustworthy;
- unavailable-feature reason;
- Retry/Reconnect/source selection;
- fallback/manual controls;
- no vendor-specific dead controls without capability gating.

The SoundSwitch Reference skin may visually resemble the familiar source selector/status model, but it uses the normalized adapter data.

## Conformance fixtures

Create adapter-neutral replay fixtures:

1. exact two-deck play/mix;
2. four-deck state;
3. BPM-only source;
4. seek/loop/reverse/pitch changes;
5. crossfader Blend/Cut/Scratch/Upfader policies;
6. identity lost while clock remains;
7. packet loss/predictive hold/recovery;
8. adapter disconnect/audio fallback/manual fallback;
9. source switch conflict;
10. same-PC versus LAN timing;
11. stale/out-of-order events;
12. track metadata collision/low-confidence association.

Every direct adapter maps its captured events into these normalized expectations.

## Cross-controller conformance fixtures

1. note/pad momentary and toggle;
2. absolute fader with pickup;
3. multiple relative encoder dialects;
4. modifier/layer;
5. output LED/ring feedback;
6. disconnect/reconnect/resync;
7. multiple simultaneous devices;
8. duplicate message conflict;
9. unstable system index/logical matching;
10. profile import/version/deprecation;
11. missing project target;
12. bank paging over full Autoloop catalog.

## Qualification journeys

### DJ software change

1. Open unchanged project.
2. Select target DJ adapter.
3. Review capability comparison and fallback.
4. Connect and validate clock/identity/mixer.
5. Rehearse Tracks/Autoloops/faders/fault recovery.
6. Save adapter preference, not rewritten lighting content.
7. Return to prior adapter and confirm identical project content.

### Controller change

1. Disconnect old controller; UI remains operational.
2. Connect new controller and select/create profile.
3. Bind high-value commands and feedback.
4. Verify soft takeover and bank paging.
5. Run same Live journey through software and hardware.
6. Save reusable profile; project content unchanged.

### Skin change

1. Run active show.
2. Switch Default → Reference/custom.
3. Preserve source, active content, progress, faders/overrides, health, and mappings.
4. Verify no DMX stop/recompile.
5. Switch back.

### Topology change

1. Move DJ software to separate PC/private LAN.
2. Change endpoint/reconnect.
3. Validate timing/health.
4. Preserve project, controller, skin, and output behavior.

## Acceptance

1. Normalized DJ/controller contracts are versioned and adapter-independent.
2. Capability negotiation controls feature availability honestly.
3. VirtualDJ, future Serato, fallback clocks, skins, and controllers do not create separate lighting engines.
4. Project content survives adapter/controller/skin/topology changes.
5. Machine-local identity does not leak into portable projects by default.
6. Same command/state suite works through software and hardware surfaces.
7. Missing capabilities/targets degrade visibly without silent substitution.
8. Same-PC and LAN use the same behavior/contracts.
9. Security and resource bounds remain enforced.
10. Migration/rehearsal/conformance evidence is committed per adapter/profile.
