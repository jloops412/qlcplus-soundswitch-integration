# UI resource adoption and Default 2.1 checkpoint

Status: source-complete implementation checkpoint for Windows preview 88 and issue #37 continuity. This is a visible improvement to the trusted Win32 strangler plus reusable UI contracts; it is not acceptance of a production toolkit or completion of the `.emberskin` runtime.

## Outcome

The prior application was mechanically useful but still presented most work as flat native forms. This checkpoint adds a persistent operator-health hierarchy, coherent icon/navigation language, card surfaces, modern list rows and active-content feedback, and responsive shell geometry without changing lighting behavior.

The reusable part is `emberlights/ui_visual.hpp`, not a collection of new magic colors inside `windows_app.cpp`. It provides:

- one toolkit-neutral Ember Dark semantic token map;
- stable navigation target, accessible label, workspace, and icon metadata;
- deterministic Compact/Standard/Wide shell geometry;
- semantic health tones independent of Win32 brushes;
- a narrow contract test that a future Slint renderer can consume without importing Win32 control IDs.

## Resource decision

| Resource | Decision for this checkpoint | Reason / next gate |
| --- | --- | --- |
| [Microsoft Segoe Fluent Icons](https://learn.microsoft.com/windows/apps/design/iconography/segoe-fluent-icons-font) | Use the Windows-supplied font for navigation; fall back to Segoe MDL2 Assets on Windows 10; always retain visible accessible text. No font file is bundled. | Immediate coherent iconography with no installer/runtime payload. |
| [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) | Qualified as the MIT vector source for a later toolkit-neutral curated asset pack; no SVG is bundled yet. | Slint/Default/Reference should share versioned vector assets rather than PUA glyphs once the renderer is accepted. |
| [Slint](https://github.com/slint-ui/slint) | Remains the first issue #37 production-shaped spike candidate, currently 1.17.1; not linked or shipped in preview 88. | Its C++ integration, Windows x64 package, Fluent style, software renderer, accessibility, footprint, deployment, and dense-Studio behavior still need equivalent measured evidence. |
| [QLC+](https://github.com/mcallegari/qlcplus) | Continue using its Apache-2.0 fixture/console workflows as audited interaction reference; do not fork the full Qt UI or copy its artwork. | Reuse concepts such as design/operate separation, preview, channel capability editing, pads, sliders, feedback, and input binding where they fit EmberLights contracts. Selective source adaptation remains possible only with file-level license/notice review. |
| Qt 6 | Retain as the mature fallback from issue #37; do not add its runtime to this preview. | A wholesale QLC+/Qt adoption would add deployment, footprint, ABI, and LGPL/module-governance work before proving it beats Slint for EmberLights. |
| WinUI 3 | Retain as the Windows-native control comparison. | It must earn its Windows App SDK installer/startup/working-set cost against the same screens and measurements. |

No SoundSwitch or QLC+ visual asset, screenshot, logo, font, icon, or source file was copied into this checkpoint.

## Default 2.1 visible changes

### Persistent operator health

Every workspace now retains a compact top strip for:

- Project and saved/unsaved state;
- Runner/Studio-preview state and active Look/Autoloop summary;
- sync source/state and BPM;
- aggregate output readiness/fault;
- manual override count;
- blackout, work-light, and hazard state.

Start/Stop and Blackout remain spatially isolated at the right edge. They invoke the same registered facade commands as the existing Live controls and F5/F8; no second implementation or callback was added.

### Navigation and surfaces

- Live, Studio, and Setup remain distinct workspaces with last-page memory.
- Navigation uses a consistent Fluent glyph vocabulary plus permanent text labels.
- Selected workspace, selected page, active content, keyboard focus, warning/fault, primary action, destructive action, and emergency action use distinct semantic treatments.
- The page canvas has a raised authoring surface instead of controls floating on one undifferentiated background.
- Classic sunken `WS_EX_CLIENTEDGE` fields/lists and list-view grid walls were removed.
- List rows are taller, separated, keyboard-focused, and show selected versus Runner-active state separately.
- Live Static Looks, Autoloops, and Track Scripts use stronger section hierarchy.

### Responsive shell bridge

The navigation, persistent health bar, page, status bar, margins, and list density are computed for Compact, Standard, and Wide client widths. This does not yet make every fixed page fully responsive; it establishes one tested outer geometry contract and removes global shell constants from the Win32 host.

## Surface-contract reconciliation

- Commands added/changed/deprecated/removed: none.
- States added/changed/deprecated/removed: none; the strip formats existing authoritative project and Runner snapshots.
- Capabilities/components: no public component generation change; one internal toolkit-neutral visual bridge was added.
- Theme tokens affected: 27 existing Ember Dark design targets are now represented in native semantic form, including alpha-aware selection/brand-soft values.
- Registry generation/digest: unchanged.
- Bundled skins: Default strangler presentation changed; Safe, Reference, and `.emberskin` packages remain unimplemented and therefore unchanged.
- Mappings/actions/profiles/migration: unchanged.
- Direct callback bypasses: zero added and zero removed. Persistent Start/Stop and Blackout are duplicate surfaces over the existing shared commands.
- Compatibility classification: additive presentation-only checkpoint; project schema and Runner behavior are unchanged.

## Verification

- `ui_visual_tests`: token uniqueness/values, stable navigation metadata, accessible labels, and Compact/Standard/Wide geometry pass under warnings-as-errors.
- Windows x64 Zig/Clang warnings-as-errors target: `EmberLights.exe` compiles and links as PE32+ GUI.
- Generated registry and direct-callback expectations remain covered by the surface-contract gate.
- Native Windows rendering, DPI, Narrator/UI Automation, clean/upgrade install, and physical hardware remain tester evidence gates; no Wine/native Windows runtime was available on the build host.

## Next issue #37 slice

Build the production-shaped Slint 1.17.x spike against this token/navigation/state bridge and the generated command/state catalogs. Use the official C++ binary package on a Windows/MSVC qualification host first so the experiment measures the supported deployment path rather than conflating toolkit behavior with cross-toolchain setup. The spike must still include Safe/Reference/Default Live, the dense Studio skeleton, invalid-skin fallback, accessibility inspection, working set, cold start, CPU/repaint, and installer deltas before any toolkit acceptance.
