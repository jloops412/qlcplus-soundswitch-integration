# Unified Fixture Attribute checkpoint

Status: source-complete Default 2.3 / fixture-truth checkpoint for issues #52,
#33, and #66 on the open integration branch. It advances the shared fixture
control contract; it is not physical-fixture qualification, a finished skin
runtime, or completion of the fixture-library corpus.

## Outcome

EmberLights now presents one profile-backed **Fixture Attribute** catalog
instead of treating ordinary fixture channels and named DMX ranges as unrelated
UI concepts.

The catalog contains:

- direct semantic channels such as Intensity, RGBWAUV, Pan, Tilt, Focus, Zoom,
  Iris, and other profile mappings;
- named compound-channel capabilities such as Shutter Open, Gobo slots, Prism
  Insert, and bounded Strobe/rotation ranges;
- exact target coverage for one fixture or a mixed-profile group;
- channel, encoding, coarse/fine, DMX range, selected raw value,
  default/blackout/highlight, profile revision, and owner/head/cell diagnostics;
- stable choice identity independent of visible list position or physical
  channel order.

Direct attributes and named capabilities still resolve to the existing
normalized semantic property/value contract. Raw DMX is diagnostic evidence,
not a second command or persistence API.

## Priority-workspace integration

- **Fixture Profiles:** ordered channel mappings remain the source of truth;
  operator wording now distinguishes semantic attributes from exact named
  functions on compound channels.
- **Static Looks:** the profile-backed picker is primary and can apply direct or
  named attributes transactionally across a fixture/group. The generic semantic
  picker remains an explicit advanced fallback.
- **Autoloops:** committed V2 programs can expand either kind into exact
  per-fixture `PropertyBlock` events with the same generation, capacity,
  collision, and safety checks.
- **Live:** exact compatible attributes construct the existing atomic fixture
  or group override commands. Partial or profile-divergent group choices remain
  visible to toolkit-neutral diagnostics but are not approximated.
- **MIDI/controllers:** direct continuous attributes resolve true 0..1 semantic
  endpoints, including 16-bit profile mappings, while slot capabilities remain
  fixed values. Existing scaling, curves, inversion, soft takeover, fan-out,
  and feedback architecture is retained.
- **Skins/Ember Actions:** the compatible stable component ID
  `ember.fixtureFunctionBrowser` advances to model version 2 and registry label
  **Fixture Attribute Browser**. It consumes the same catalog and registered
  override commands; no skin receives direct output access.

## Product-reference findings applied

- SoundSwitch's separation of basic control tracks from reusable Attribute Cues
  supports one operator vocabulary over direct attributes and reusable special
  functions; EmberLights keeps both on one portable semantic spine.
- WOLFMIX reinforces category-first Color/Move/Beam controls, palettes/groups,
  and immediate flash behavior; it remains secondary workflow inspiration.
- QLC+'s channel capabilities and Click And Go pickers reinforce explicit named
  ranges on compound channels, HTP/LTP-aware semantics, and picker-first UX.
- GDTF's `DMXChannel -> LogicalChannel -> ChannelFunction -> ChannelSet`
  hierarchy is the rich interchange target for a future bounded Studio adapter.

No SoundSwitch or WOLFMIX code/assets were copied. No QLC+ runtime was added.
This pass contains original implementation over the existing EmberLights model,
so it adds no third-party binary or license obligation.

## Fixture-library direction

The accepted intake order remains:

1. Open Fixture Library as the MIT-licensed, pin-able open foundation;
2. GDTF Share as a richer industry-standard source through an authenticated,
   versioned Studio-only adapter and immutable native snapshot;
3. QLC+ QXF definitions as a valuable secondary compatibility/fallback source.

All sources enter quarantine with provenance and audit evidence. Runner consumes
only the compiled EmberLights profile and never parses OFL, GDTF, QXF, or a
network response.

## Safety and compatibility

- Protected reset/service/custom ranges remain absent from callable catalogs.
- Safety-classified attributes retain existing Runner policy; authoring does not
  arm or bypass hazards.
- Direct compound channels with named capabilities do not also publish an
  ambiguous full-range direct control.
- Group identity matches semantic owner/property/occurrence rather than assuming
  matching physical channel numbers across profiles.
- Project format, compiled package schema, 29 registered commands, 39 states,
  Runner scheduling, output routing, blackout, and hazard authority are
  unchanged.
- Registry `1.4.0`, generation 2, has 8 components, 1 capability, and 11 value
  contracts. Digest `3f44d8129607601db67f462b3710b52bf4c889d6238cebc10a7f4a2a957cf659`
  remains compatible-additive against the frozen V1 baseline with zero breaking
  changes or removals.

## Focused verification

- Fixture Attribute component tests prove a direct 16-bit coarse/fine Intensity
  row beside mixed-profile wheel/shutter rows, exact diagnostics, stable search,
  and typed Live command construction.
- Static Look tests prove common direct RGB controls across both IR-4 profiles
  preserve normalized values while reporting their exact profile realization.
- Autoloop tests prove a direct profile attribute expands into exact per-fixture
  events through the existing transactional service.
- Controller tests prove a common direct 16-bit attribute becomes one continuous
  group mapping with 0..1 endpoints and soft takeover retained.
- Existing mixed-profile, named-slot, continuous-range, protected, safety,
  stale-selection, capacity, collision, persistence, and renderer tests remain
  green.

## Next bounded fixture passes

1. Add a searchable virtualized attribute browser/Inspector with category tabs,
   value widgets derived from control kind, favorites, and accessible keyboard
   operation in the production skin runtime.
2. Pin and package a representative OFL conformance corpus; add a bounded GDTF
   adapter spike and richer QXF fallback coverage without adding global-library
   work to Runner.
3. Model richer physical units, neutral/home semantics, color temperature,
   indexed media, and head/cell geometry while retaining stable semantic IDs.
4. Replace legacy Autoloop quick-look steps with the committed V2 timeline and
   reusable Palette/Position/Attribute/Movement/Effect assets.
5. Qualify the installed Windows UX and owned fixtures/controllers, then use the
   resulting evidence to tune widgets and defaults without encoding one rig into
   the product model.
