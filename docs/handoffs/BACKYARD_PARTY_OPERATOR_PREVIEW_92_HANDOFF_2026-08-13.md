# Backyard-party operator preview 92 handoff — 2026-08-13

Status: source-complete and locally contract-tested. Windows installer `0.1.0-preview.92.0` was built from clean source `bff65d731a5ba35b04afdfa59c2ecf55210a09f4`. Source publication is pending explicit approval of the `jloops412/EmberLights` remote after the execution environment blocked the push as an unverified destination.

## Primary outcome

Static Looks and Live Overrides can now browse readable functions defined by the selected patched fixture profiles instead of requiring operators to translate every compound channel into an unexplained percentage. Color/gobo wheel slots, shutter states, strobe ranges, rotations, macros, and other authored profile functions all use one toolkit-neutral catalog intended for later Autoloop, MIDI/Control One, Ember Action, and EmberSkin surfaces.

## What changed

- Static Looks adds a **Named function from fixture profile** picker. Slot choices use each profile's exact preferred selection; continuous choices use the visible 0–100 field as position inside the documented range.
- A mixed-profile group can write different exact semantic values per fixture in one Static Look transaction, then update offline or bounded physical preview.
- Live Overrides adds the same named picker and routes through the existing registered atomic fixture/group override commands.
- A Live group choice appears only when every group member supports it and one semantic value is exact for all members. Partial or profile-specific group functions are never approximated; use an individual fixture or a Static Look instead.
- Protected reset/service/reserved/unverified-custom functions never appear. Safety-gated functions retain the existing Runner policy.
- The choice service and Static Look transaction contain no Win32 types. Current controls are replaceable adapters for the modular skin/controller program.
- White and Amber remain ordinary profile-backed Color properties. No permanent swap button or global renderer inversion was introduced.

## Quick test route

1. In Studio → Fixture Patch, confirm the exact profile, mode, universe, and address assigned to the physical light.
2. In Studio → Fixture Profiles, duplicate an imported/built-in profile before editing and enter the fixture manual's exact direct channels or named ranges.
3. In Studio → Static Looks, select a fixture/group and choose a **Named function**. For a continuous function, set **Level / range position**; then choose **Use Named Function**.
4. With Live stopped, use the capped physical preview to confirm the selected function in realtime. Stop preview or wait for its 30-second fail-closed timeout.
5. Save the Look, start Live, and use Live → Fixture Overrides for temporary named functions. Mixed groups intentionally omit functions that cannot be represented exactly by one command.
6. For White/Amber disagreement, first compare the Patch profile and physical display mode. Record a raw isolated response before creating a Local physical variant; do not alter global color code.

## Verification

- complete Make-driven native regression suite: passed;
- exact mixed-profile slot, shared slot, continuous safety-gated range, protected exclusion, and offline DMX-byte regressions: passed;
- generation-2 registry and Ember Action adapter checks: passed;
- Windows x64 Zig/Clang warnings-as-errors shipped-target build: passed;
- real Windows COFF resource and embedded manifest: passed;
- package-contract regressions: 17/17 passed;
- staged payload contract: 18 product files plus generated manifest passed;
- repeated NSIS compilation: byte-identical;
- 7-Zip archive test/extraction and exact 19-file staged/extracted comparison: passed;
- `git diff --check`: passed before the source commit.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.92.0-Setup.exe`
- Size: `1,874,082` bytes
- SHA-256: `9729777492013ab7df6f4398462ee2ed69308a30f35cef0716dbd6e2dd5c049e`
- Payload manifest SHA-256: `cb38359ceb9c68c5b27dc12c1d34ddcf7ad0004bc79d45e3c55b268d287c9b4f`
- Source: `bff65d731a5ba35b04afdfa59c2ecf55210a09f4`
- Source tree: `f055b88216b10342ab19566a2657b76214191d27`
- Version: `0.1.0-preview.92.0`
- Boundary: contract-tested unsigned preview built on Linux. Native Windows clean/upgrade install, GUI launch, Installed Apps uninstall, shortcuts/registry cleanup, project/settings preservation, DPI/accessibility, and physical hardware remain tester gates.

## Honest remaining boundaries

- Autoloop event authoring, MIDI/Control One mapping, Ember Actions, and EmberSkin components still need to browse the same catalog;
- responsive production-toolkit controls, category/type-ahead filtering, controller feedback, overlays, and the visual Skin Designer;
- richer Position/Attribute assets, switching dependencies, multi-head/cell and pixel/matrix realization, and 16-bit ranges;
- pinned offline fixture corpus, immutable revision/qualification invalidation, installed-Windows evidence, and physical fixture/controller qualification.

Passing this handoff proves internal mapping, fail-closed behavior, package identity, and embedded payload consistency. It does not prove a downloaded profile, physical fixture mode/address, interface, controller, installed Windows lifecycle, or gig readiness.
