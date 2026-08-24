# V23 Live Console Provenance

V23 is generated from the published V22 workspace. It is a Virtual Console-only correction and modernization pass.

## Inputs and result

| Role | File | SHA-256 |
|---|---|---|
| Source | `IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw` | `7AC6ED5413E2C4593B79D414C58C5ADADBAB2C3474A665F7E8FF3631FBA012E7` |
| Result | `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw` | `E953C3483EB09D2E600D32495887B27A03021DC47EF7BA5797552C4F5A21547B` |

The deterministic builder is `qlcplus/workspace-tools/Build-V23ConsolePolish.ps1`. The source hash is enforced before the builder writes output.

## Active-loop defect and correction

V22 created 128 disabled raw-Chaser monitor Buttons but assigned only 32 geometries. Four banks therefore occupied each visual position. An inactive widget from a later bank could paint over the active Chaser's Monitoring border.

V23 keeps all 128 monitors and their raw Function bindings, but assigns four non-overlapping indicators per physical pad. Bank order is top-to-bottom. The monitor Frame remains:

- disabled/read-only;
- outside every playback-owner SoloFrame;
- free of external Inputs; and
- bound exactly once to raw Chasers `532–659`.

This preserves native QLC+ playback authority and avoids the previous one-second ownership flash.

## Mode-switch defect and correction

V22 widgets `1405` and `1406` both referenced Function `1993` and logical channel `811`. Since both monitored the same Function, feedback could cause the plug-in's edge-agnostic mouse action to toggle twice.

V23 removes only widget `1406`, reparents `1405` to the Live page, removes its mode-specific `Page` attribute, and preserves its Function/Input bindings. The result is one persistent two-way mouse control.

## Presentation delta

- Live page: 1450×700 to 1600×900.
- Unified dark performance palette and typography.
- Transport, status, dwell, mode, banks, scope, order, pad grid, speed, overrides, intensity, and tracker receive explicit non-overlapping geometry.
- The 4×8 physical pad orientation is retained.
- Ten existing CueLists remain bound to Autoplay parent Chasers `788–797` and expand to show the current selected row.
- Other established manual/map pages retain their control positions and receive only the common dark foundation.

## Invariants proved

- 2,090 source Functions retained with byte-equivalent parsed XML.
- Source Fixture, InputOutputMap, FixtureGroup, Monitor, and ChannelsGroup XML unchanged.
- 16 fixtures retained: eight physical plus eight private Priority Layer fixtures.
- Virtual Console Function/Input bindings retained for every surviving widget.
- Actual widget IDs remain unique.
- All Function references resolve.
- Exactly one logical channel `811` mouse source remains and targets Function `1993`.
- Live rail covers all 128 raw Chasers exactly once with four non-overlapping bank indicators per pad.
- Ten native Autoplay trackers remain.
- No personal path, username, serial, or secret is published.

V23 reuses the exact V21/V22 plug-in binary; no QLC+ core or plug-in source change is required.
