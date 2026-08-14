# Fixture Control Inspector checkpoint

> UI course correction (2026-08-14): the surface-aware fixture-control model
> remains valid, but the modeless Win32 Inspector presentation is rejected as
> the product direction. This is not Default 2.4 or professional UI. The
> Inspector is frozen as an Advanced diagnostic bridge while ordinary control
> moves to `ember.fixtureControlSurface` in the replacement shell.

Status: contract-packaged fixture-control model and historical legacy-adapter
checkpoint for issues #52 and #66 on the open integration branch.

## Outcome

EmberLights now separates two concepts that must remain distinct for
professional control and faithful migration:

- a **Fixture Control** is a direct profile DMX channel or one named function
  inside a channel's documented DMX range;
- an **Attribute Cue** is a future reusable authored asset that selects and
  configures one or more Fixture Controls, can be placed in scripts/Autoloops,
  and can update every reference when edited.

The existing stable component type `ember.fixtureFunctionBrowser` is retained
for file, binding, Action, and future skin compatibility. Its operator label is
now **Fixture Control Inspector** and its toolkit-neutral model advances to
version 3.

## Default adapter workflow

Live Overrides, Static Looks, committed Autoloop V2 editing, and MIDI Mapping
now provide **Browse Controls…**. The modeless Inspector provides:

- whitespace-token search across category, semantic control, named function,
  owner/head/cell, manufacturer, model, mode, profile revision, fixture, binding
  path, and stable control ID;
- category filtering with total/search/favorite counts;
- target-scoped session favorites and a favorites-only filter;
- stable-ID selection independent of row order;
- a value/range-position slider with 0/25/50/100 presets;
- explicit direct-channel, continuous-range, and exact-slot input semantics;
- coverage and availability for the destination surface;
- per-fixture profile/channel, coarse/fine encoding, selected raw realization,
  DMX range, default, blackout, highlight, and revision diagnostics;
- double-click or **Use Selected Control** handoff to the existing destination
  editor.

Favorites are intentionally session-local in this checkpoint. They do not
pollute show files or claim the future operator-preference/skin-persistence
contract.

## Surface-aware availability

One catalog identity does not mean every surface has identical exactness rules.
The component now evaluates the destination explicitly:

| Surface | Availability rule |
| --- | --- |
| Live Overrides | A fixture/group control is enabled only when one complete atomic semantic command is exact for every member. Partial coverage, profile-divergent values, protected functions, and safety-gated functions are refused. |
| Static Looks | Supported fixtures may retain exact per-profile values in one transaction. Partial group coverage is explicit; unsupported fixtures follow lower content. Safety-classified controls remain authorable but retain Runner arming/caps and physical-preview blocks. |
| Autoloop V2 | Exact per-fixture event expansion is allowed for supported direct channels and named functions. Safety-gated controls remain unavailable until an explicit safety-authoring contract exists. |
| MIDI/controllers | Stable control provenance can compile one homogeneous group mapping or bounded per-fixture mappings for mixed profiles. Protected and unconfirmed safety-gated controls remain unavailable. |

The Profile editor remains the authoritative place to define channel order,
encoding, named ranges, owner, defaults, and source evidence. The target-facing
Inspector cannot edit profile truth.

## SoundSwitch, WOLFMIX, and QLC+ findings applied

Official SoundSwitch guidance distinguishes basic Intensity, Color, Pan/Tilt,
and Strobe control tracks from reusable Attribute Cues for special functions
such as Gobo, Prism, Focus, and Zoom. Editing an Attribute Cue updates scripts
and Autoloops that reference it. EmberLights therefore does not rename every
fixture channel an Attribute Cue; future SoundSwitch migration will preserve
those authored assets and their references above the Fixture Control catalog.

WOLFMIX reinforces fast group/category control, definable palettes and presets,
position/gobo palettes, live edits, direct channel testing, and a visible DMX
level view. The Inspector adopts category-first discovery and direct diagnostic
visibility without coupling the engine to a hardware layout.

QLC+ models channel capabilities as named DMX value ranges and exposes direct
capability selection above channel faders. EmberLights retains that useful
channel/function distinction while keeping raw bytes diagnostic and preserving
semantic commands for skins, MIDI, Looks, loops, and migration.

No SoundSwitch or WOLFMIX code/assets were copied. No QLC+ runtime or source was
incorporated. The implementation is original code over the existing
EmberLights semantic/profile model.

## Migration and skins foundation

The intended SoundSwitch import boundary is now explicit:

1. imported fixture definitions populate direct channels and named DMX
   functions with provenance;
2. imported SoundSwitch Attribute Cues become reusable authored assets that
   reference stable Fixture Control IDs and preserve source evidence;
3. imported timeline/Autoloop placements reference those assets rather than
   duplicating guessed bytes;
4. unresolved vendor data remains loss-preserved and visibly approximate.

WOLFMIX remains a secondary evidence target until a lawful controlled-delta
corpus exists. No parser or fidelity claim was added in this pass.

Future `.emberskin` implementations consume the same component type, model,
stable selection, surface, category, favorites, and diagnostic properties.
Skins do not gain raw DMX writes, profile mutation, timing, safety authority, or
output access.

## Registry and compatibility

- registry set `1.5.0`, generation 2;
- digest `7e22086416145ec19d10d309e492d64eb60e96d3e8a8a59b2f81ffdbadc7026f`;
- 29 commands, 39 states, 8 components, 1 capability, and 11 reusable values;
- compatible-additive against the frozen generation-1 baseline;
- zero command/state ordinal changes, removals, or breaking changes;
- `ember.fixtureFunctionBrowser` contract `0.3.0` adds category, surface,
  favorites-only, stable selection, and interaction events;
- generated UI registry and Ember Action adapters share the exact digest.

Project format, compiled package schema, Runner scheduling, output routing,
blackout, and hazard authority are unchanged.

## Verification and package evidence

- the warning-fatal full native build and all 34 native test executables pass;
- tests cover surface-specific partial/mixed/safety rules, favorites,
  favorites-only filtering, stable selection, direct 16-bit diagnostics, stale
  selection refusal, and Live-only invocation construction;
- all twelve registry governance tests, deterministic generation/check, frozen
  baseline diff, and generated Ember Action adapter check pass;
- the surface-contract, WinMM, and DMX USB Pro gates pass;
- the clean Windows x64 Zig/Clang application and supported tools compile and
  link with the new Inspector window and destination bridges;
- package-contract regressions pass 18/18; the exact 20-file payload and
  manifest verify; repeated NSIS construction is byte-identical; archive
  extraction and normalized 21-file product-payload comparison pass;
- unsigned installer `0.1.0-preview.100.0` is bound to exact feature source
  `e8693be9378e9282b3f0f7b345cec66f6d6a8a64` at SHA-256
  `e368928de33b97bb7a7510360ea421a18fd9d6abe5cbfcdfac7d3a0d78bca2d5`;
  payload manifest SHA-256 is
  `3152ffc02567c9ba151eb34fea0512bd8e9a0c801c9204268ef4362d03b1be74`.

This is contract-tested unsigned preview evidence from a non-Windows host, not
installed-Windows, accessibility, physical-output, or gig qualification. See
`handoffs/BACKYARD_PARTY_OPERATOR_PREVIEW_100_HANDOFF_2026-08-13.md`.

## Remaining boundaries

- activate this component through the production `.emberskin` runtime and
  persist operator favorites in the accepted preference contract;
- replace the Autoloop post-commit form with the full V2 timeline and reusable
  Palette/Position/Attribute/Movement/Effect asset workflow;
- create the reusable Attribute Cue asset/dependency-update contract before
  claiming SoundSwitch cue migration fidelity;
- add a pinned representative OFL corpus, bounded GDTF adapter, richer QXF
  conformance cases, units, neutral/home values, and head/cell geometry;
- complete installed-Windows DPI, keyboard, Narrator/UI Automation, long-list,
  upgrade, and lifecycle evidence;
- complete owned-fixture/controller qualification and shadow rehearsals before
  any gig-ready claim.
