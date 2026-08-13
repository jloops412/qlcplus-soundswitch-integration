# Backyard-party operator preview 91 handoff — 2026-08-13

Status: source-complete, synced to GitHub, and locally contract-tested. Windows installer `0.1.0-preview.91.0` was built from clean GitHub source `d5a0d008bb340d384b5b4eb6953026d54025ac5a`.

## Primary outcome

This preview replaces the next major fixture-profile dead end: one physical DMX channel can now hold several named functions or ranges without asking the operator to invent encoded EmberLights parameter IDs. The model, QLC+ ingestion, Studio editor, compiler, Runner, controller-binding resolver, persistence, and safety rules all use the same structured capability rows.

The permanent White/Amber repair button remains removed. White and Amber still use ordinary semantic mappings; physical disagreement is resolved by selecting or creating the correct profile/mode variant and confirming that the patched fixture actually uses it.

## What changed

- Fixture Profiles now has a dedicated **Named DMX ranges…** workbench for the selected channel.
- Operators choose a range name, shared Parameter dropdown, DMX From/To/Preferred values, named-slot or continuous behavior, safety access, functional role, direction, and fixture/head/cell owner.
- EmberLights proposes the next unused range and generates a stable binding path for later MIDI, Control One, custom-controller, and skin surfaces.
- Real compound chart rows are representable: shutter closed/open/strobe speed, indexed color and gobo slots, rotation, prism, macros/effects, and protected reset/service ranges.
- Channel blackout/highlight values and owner metadata are explicit instead of inferred from display text.
- Imported and built-in profiles remain inspectable but read-only; **Duplicate to Edit** creates the Local authoring boundary and the normal Save flow can rebind every patched fixture transactionally.
- QLC+ and OFL-imported QXF capability rows are retained as unreviewed native ranges when they are non-overlapping and safe to represent. Switching aliases and ambiguous overlaps remain quarantined.
- Protected reset, service, reserved, and unverified custom ranges cannot bind or render. Safety-gated strobe/atmosphere/laser ranges still require the existing arming policy.
- Two different semantic functions trying to own one byte at the same layer fail closed to the channel blackout value.
- Old single-property profiles and old format-1 projects retain their previous behavior. New metadata uses additive, backward-preservable records.
- The Windows resource is now self-contained and cross-buildable while retaining the real as-invoker, PerMonitorV2 manifest and Preview file/product version.

## White/Amber truth

Regression tests continue to prove that White and Amber write the offsets in the active profile; the application does not globally invert them. The manual-backed Both Lighting BO-IR4 mappings are:

| Fixture mode | White | Amber |
| --- | --- | --- |
| 6CH | CH4 | CH5 |
| 10CH | CH5 | CH6 |

If a physical unit responds in the opposite order after Patch confirms the expected Local profile, record the exact display mode/firmware and create a named Local physical variant. Do not change the global renderer or reintroduce a permanent swap button.

## First profile test route

1. Stop normal Live output. In Studio → Fixture Patch, note the fixture's exact profile, universe, address, and physical display mode.
2. In Fixture Profiles, select that exact profile and confirm `PATCH USAGE` lists the fixture. If the profile is imported or built-in, choose **Duplicate to Edit**.
3. For a normal one-function channel such as IR-4 White or Amber, select the channel and choose the semantic Parameter from the dropdown; no encoded ID should be typed.
4. For a compound channel, select it and open **Named DMX ranges…**. Enter the fixture manual's exact non-overlapping rows. Use `Protected` for reset/service/reserved values and `Safety-gated` for strobe or hazardous output.
5. Save the Local profile and accept the reviewed rebind when the affected fixture list is correct.
6. Reopen Fixture Patch and confirm the fixture now names the Local profile.
7. With normal Live still stopped, use the bounded Static Look physical preview at low output to test isolated functions. Record the exact physical response and mode; preview success is not automatic profile qualification.

## Verified before packaging

- complete Make-driven native regression suite: passed, including core, Studio, Static Look physical preview, OFL, profile editor, Live/UI, connections, hardware-probe protocol, Autoloops V2, and allocation-free Ember Action execution;
- focused structured profile authoring, overlap rejection, protected binding refusal, compound rendering, safety gating, same-layer conflict blackout, metadata mutation, and project round-trip: passed;
- QLC+ compound shutter/strobe and hazardous-gate import regressions: passed;
- generation-2 UI registry checks: 29 commands, 39 states, 5 planned components, 1 planned capability, 11 value contracts, digest `d3f5c6edc1226a5184ddcf7d7ed2405605534131e6c6ab88b167f111b1614945`;
- Ember Action registry adapter generation check: passed;
- Windows x64 Zig/Clang warnings-as-errors shipped targets: passed;
- real Windows COFF resource, as-invoker/PerMonitorV2 manifest, and Preview 91 file/product version inspection: passed;
- package-contract regressions: 17/17 passed;
- `git diff --check`: passed.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.91.0-Setup.exe`
- Size: `1,837,924` bytes
- SHA-256: `2d6fb856d83cced614ab9e91b44a2996940310a03ca57b5de04773a3ea6c95e4`
- Payload manifest SHA-256: `724b8651da9e6fe2535d0efbdad916d3909f1147eb41b5fb1cd89e6ad681eff9`
- Source: `d5a0d008bb340d384b5b4eb6953026d54025ac5a`
- Source tree: `c59606a56b011193a5171975c8c451cb3c53394f`
- Version: `0.1.0-preview.91.0`
- Evidence: exact clean GitHub worktree; 17/17 package-contract regressions; exact 18-file payload manifest; repeated byte-identical NSIS compilation from the verified stage; current 7-Zip NSIS archive test/extraction; exact 19-file staged/extracted payload comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Windows SmartScreen may warn. Native Windows clean/upgrade install, launch, Installed Apps uninstall, shortcut/registry cleanup, project/settings preservation, GUI/DPI/accessibility behavior, and physical hardware remain tester gates.

## Honest remaining boundaries

- named capability browsers still need to replace percentage-centric authoring in Static Looks, Autoloops, Live overrides, MIDI, Control One, Ember Actions, and future EmberSkin components;
- controller device profiles, modifiers, paging, feedback, user layouts, overlays, and physical Control One qualification;
- grouped multi-head/cell and pixel/matrix realization, switching-channel dependency graphs, and richer 16-bit capability ranges;
- pinned offline fixture corpus, immutable profile revisions, qualification/invalidation state, and representative conformance fixtures;
- production `.emberskin` runtime, Reference skin, Skin Designer, responsive layout/accessibility evidence, and removal of transitional Win32 bypasses;
- installed-Windows lifecycle and physical BO-IR4/tube/Wash FX Hex/mover/controller qualification.

Passing this handoff means the software representation, safety behavior, package identity, and embedded payload are internally consistent. It does not prove a downloaded fixture profile, physical DMX mode/address, output adapter, controller, or light response.
