# EmberLights Autoloop Source v1

Status: canonical source contract for AL2-001. This contract is vendor-neutral
and is not the EmberLights project format. Rich project persistence requires a
separate, explicit format transition.

## Authorities and boundaries

- Musical time is a signed 64-bit integer at 960 pulses per quarter note.
- All event intervals are half-open: `[startTick, endTick)`.
- `AutoloopAsset` is stable authored content identity and metadata.
- `AutoloopPlacement` is the mutable 64-bank by 32-slot address of an asset.
- `AutoloopProgram` owns targets, lanes, integer timing, and semantic events.
- `AutoloopLaunchProfile` owns repeat, quantization, phase, playback, and return
  policy.
- `AutoloopProvenance` owns producer, generator, seed, version, source-object,
  and evidence links.

Moving an asset changes only its placement. It does not rename the asset,
program, launch profile, provenance, targets, lanes, events, or referenced
content.

Canonical events never contain a vendor record, absolute path, raw DMX channel
sequence, UI pointer, or mutable Studio object. They use stable IDs, semantic
properties, declared target capability requirements, versioned payloads, and
bounded generator parameters.

## Source types

The normative C++ declarations are in
`native-core/include/emberlights/autoloop_source.hpp`. Version 1 provides:

- targets: Master, stable Group, stable Fixture, or stable role/category
  selector;
- events: legacy Static Look, property block, property curve, Palette,
  Position, Attribute, Movement, and Effect;
- property values: Set, Release, or ForceZero;
- interpolation: Hold, Linear, or SmoothStep;
- repeat: Once, Infinite, or TrackDuration;
- launch quantization: Immediate, NextBeat, NextBar, or NextPhrase;
- phase origin: Launch, Track, Bar, or Phrase;
- playback: Overlay or Replace.

Lane priority is explicit. Two events may overlap on the same target and
property only when their lane priorities differ. Equal-priority overlap with
opaque ownership (for example, a whole Static Look) is invalid. Vector order is
never precedence.

Curves contain two through 256 strictly increasing points. Their first and
last ticks equal the event start and end ticks. Movement/effect parameters are
finite and bounded, and their seed is serialized explicitly.

## Deterministic text interchange

The canonical byte stream is UTF-8, tab-separated, and line-oriented. It begins
with:

```text
EMBERLIGHTS_AUTOLOOP_SOURCE<TAB>1<LF>
```

Record types are `ASSET`, `ASSET_TAG`, `PLACEMENT`, `PROGRAM`, `TARGET`,
`LANE`, `EVENT`, `CURVE`, `LAUNCH`, and `PROVENANCE`. Unknown record types are
rejected in this standalone contract. Percent escapes use uppercase hexadecimal
for percent, tab, carriage return, line feed, and other control bytes.

Collections are serialized in stable-ID order; tags, capability requirements,
and curve points are sorted by their semantic keys. Floating values use the
shortest round-trippable `max_digits10` representation. The source digest is
lowercase SHA-256 over exactly these canonical bytes. Duplicate IDs, tags, or
capability requirements are validation failures rather than silently removed
during normalization.

## Format-1 compatibility adapter

`adapt_format1_autoloops` is pure and does not mutate `ProjectDocument` or its
unknown records. For legacy ID `L`, it derives:

| Concept | Stable ID |
| --- | --- |
| Asset | `L` |
| Placement | `L.placement` |
| Program | `L.program` |
| Master target | `L.program.target.master` |
| Legacy lane | `L.program.lane.legacy` |
| Event N | `L.program.event.N` |
| Launch profile | `L.launch` |
| Provenance | `L.provenance` |

Finite format-1 beat values are multiplied in double precision by 960 and
rounded to the nearest tick, with exact half-ticks away from zero. Each legacy
step becomes a `LegacyLook` event whose end is the next step or the program
length. Look IDs, Cut/Linear transitions, bank/slot, repeat mode, and name are
preserved. TrackDuration launch profiles explicitly require track-boundary
evidence.

### Downgrade boundary

A format-1 document remains byte-semantically governed by the permanent
format-1 reader/writer and unknown-record preservation rules. The adapter is a
runtime/compiler view only. A V2 program can be represented in format 1 without
loss only when it is exactly one Master lane of non-overlapping `LegacyLook`
events using legacy Cut/Linear transitions and legacy repeat/placement policy.
Property curves, scoped targets, reusable semantic references, generator
parameters, rich launch policy, and provenance have no implicit downgrade.
Writers must reject or require an explicit lossy export choice instead of
silently flattening those features.
