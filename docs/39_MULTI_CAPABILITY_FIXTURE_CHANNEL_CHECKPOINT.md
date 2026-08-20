# Multi-capability fixture-channel checkpoint

Status: **implemented and packaged in Preview 91; portable, Windows-source, and package-contract tests green**

Successor: `40_NAMED_FIXTURE_CONTROL_SURFACES_CHECKPOINT.md` exposes these profile functions through the shared target-choice catalog in Static Looks and Live while preserving this checkpoint's safety and persistence contract.

Date: 2026-08-13

Owners: fixture issue #52, UI facade issue #31, controller/binding issue #66

## Operator outcome

Fixture Profiles now has a **Named DMX ranges…** workbench for the selected channel. It is intended for real fixture-chart rows such as:

- shutter closed, shutter open, and strobe speed on one byte;
- indexed colors or gobos plus rotation ranges;
- prism insertion, direction, and speed;
- fixture programs/effects;
- protected reset, service, reserved, or unverified custom ranges.

The operator chooses human-readable fields instead of writing EmberLights parameter IDs:

- range name;
- semantic Parameter dropdown from the shared catalog;
- DMX From / To / Preferred;
- named-slot or continuous behavior;
- selectable, safety-gated, or protected access;
- functional/open/closed/home/blackout/direction/reset/service/custom role;
- reverse direction;
- fixture/head/cell owner plus blackout/highlight values.

EmberLights proposes the next unused DMX span and generates a stable binding path such as `profile-id/ch3/strobe-slow-fast`. Imported and built-in profiles remain inspectable but read-only; **Duplicate to Edit** creates the Local authoring boundary. No permanent White/Amber repair button returns.

## Persisted model

Format-1 projects retain old `CHANNEL` records and add backward-preservable secondary records:

```text
CHANNEL_META_V1
CHANNEL_CAPABILITY_V1
```

Each physical channel can carry bounded, non-overlapping `ChannelCapabilityDefinition` rows containing stable identity, display name, semantic property, exact byte range, preferred byte, behavior, access, role, and direction. Channel metadata records safe blackout/highlight values and an owner label such as `fixture`, `head.1`, or `cell.4`.

Old projects read with the same defaults they had before this change. Older format-1 readers preserve the new records as unknown records instead of discarding them.

## Compiler and Runner contract

Studio validates and compiles rich rows into compact immutable `ChannelCapabilityMapping` arrays. Runner receives no names, URLs, fixture-library client, XML, or dynamically sized authoring data.

At render time:

1. protected ranges are unavailable and never emitted;
2. every selectable capability uses the ordinary semantic layer resolver and safety policy;
3. the highest active layer owns the physical byte;
4. two different semantic properties attempting to own the same byte at the same layer fail closed to the channel blackout value;
5. named slots emit their documented preferred value;
6. continuous ranges scale only inside their documented bounds and respect direction;
7. zero Strobe retains the existing documented inactive/default behavior;
8. no owned semantic value retains the channel default.

This is still the existing property/layer engine, not a separate fixture, skin, or controller engine.

## QLC+ and OFL ingestion

The clean-room QXF adapter now retains non-overlapping `<Capability>` rows as unreviewed native named ranges. It derives conservative semantic properties, slot/continuous behavior, roles, safety access, and direction from documented QLC+ metadata. Hazard gates such as laser shutter open/closed remain associated with the hazardous property so normal arming cannot be bypassed.

The importer still:

- quarantines switching aliases and `ActsOn` modes;
- reports and excludes overlapping capability rows;
- protects reset/service/reserved/custom rows;
- marks every imported mode unreviewed;
- makes no physical or semantic qualification claim.

## Controller and skin boundary

`resolve_fixture_channel_capability` converts a generated binding path into the same ordinary property/normalized-value contract already consumed by Static Looks, Autoloops, Live overrides, MIDI, and future skins. Protected rows refuse resolution.

This checkpoint provides the shared data and resolver foundation. The current MIDI, Static Look, Autoloop, and Live editors still need named-choice browsers so an operator can pick `Open`, `Gobo 4`, or `Strobe slow–fast` directly instead of reasoning about a normalized percentage. Controller device profiles, modifiers, feedback, and Control One qualification remain issue #66 work.

## White and Amber truth

White and Amber continue through the same direct semantic renderer as Red, Green, and Blue. Regression tests prove the active profile offset determines the output byte. There is no global White/Amber inversion.

For the manual-backed Both Lighting BO-IR4 profiles:

| Mode | White | Amber |
| --- | --- | --- |
| 6CH | CH4 | CH5 |
| 10CH | CH5 | CH6 |

If an isolated raw test shows the opposite behavior on a physical unit, record its exact display mode/firmware and create a named Local physical variant. Do not mutate the manufacturer-backed snapshot or add a permanent global swap control.

## Verification completed before packaging

- `fixture_profile_editor_tests`: structured authoring, overlap rejection, protected binding refusal, generated stable path, compound rendering, safety gating, same-layer conflict blackout, metadata mutation, and project round-trip.
- `core_tests`: QLC+ compound shutter/strobe import, hazardous gate retention, native validation, serialization, and rendering.
- native GNU warnings-as-errors compilation passes.
- Windows x64 Zig/Clang warnings-as-errors `EmberLights.exe` compile and PE link pass with the real COFF resource, embedded as-invoker/PerMonitorV2 manifest, and Preview 91 file/product version strings. The RC source is now self-contained instead of pulling all of `windows.h`, so the same resource contract works with MSVC and reproducible MinGW cross-builds.
- `git diff --check` passes.

The Preview 91 handoff records the full portable suite, exact source commit, package manifest, installer checksum, and archive verification after packaging.

## Honest remaining gaps

- installed Windows GUI, keyboard, DPI, screen-reader, install/upgrade/uninstall evidence;
- named capability pickers on Autoloops, MIDI/Control One, Ember Actions, and EmberSkin components; Static Looks and Live are implemented by the successor checkpoint;
- real grouped multi-head/cell and pixel/matrix realization;
- switching-channel dependency graphs and richer 16-bit capability ranges;
- pinned offline fixture corpus and direct OFL transformer;
- immutable profile revision/qualification invalidation;
- physical BO-IR4, tube, Wash FX Hex, mover, and controller qualification.

Passing software tests means the representation and fail-closed behavior are internally consistent. It does not prove an imported profile, physical DMX mode, address, output interface, or fixture response.
