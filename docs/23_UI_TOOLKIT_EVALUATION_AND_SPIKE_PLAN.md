# UI Toolkit Evaluation and Spike Plan

Status: binding decision process. No production UI toolkit is accepted until this spike produces measured evidence.

UI course correction (2026-08-14): issue #37 must now implement the binding
Fixtures + Static Looks acceptance slice from
`44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md`. A dashboard,
placeholder counter demo, or recreation of the legacy property forms is not an
equivalent product-shaped spike.

Related:

- `03_ARCHITECTURE.md`
- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `21_UI_IMPLEMENTATION_PROGRAM.md`
- `spec/ui/command-state-skin-contract-v0.md`
- issue #37

## Decision objective

Select the smallest maintainable native UI foundation that can render both bundled skins and complex Studio components while preserving the Runner's footprint, determinism, installer reliability, Windows accessibility, high-DPI behavior, and future portability.

The toolkit is an implementation detail behind the public command/state/skin/component contracts. A toolkit choice must not make `.emberskin` packages or controller mappings toolkit-specific.

## Non-negotiable requirements

### Runtime and deployment

- native Windows desktop application;
- direct C++20 integration or a narrow stable FFI boundary;
- no embedded browser required;
- fully offline after installation;
- no dedicated GPU required;
- software-rendering or reliable integrated-GPU fallback;
- compatible with the existing Inno Setup/portable ZIP/startup-smoke pipeline;
- skin failure cannot stop the Runner;
- no UI lock or allocation dependency on the DMX scheduler.

### UI capability

- responsive compact/standard/wide/touch-live layouts;
- 100%, 125%, 150%, and 200% Windows DPI;
- keyboard focus/navigation and UI Automation/screen-reader path;
- custom drawing for timeline, waveform, curves, patch chart, meters, and pad progress;
- virtualized or bounded efficient lists/grids;
- text input, tables, trees, menus, dialogs, drag/drop, context menus, docking/splits;
- semantic theme tokens;
- ordinary controls generated from the validated view graph;
- native complex components embedded without duplicating skin semantics;
- deterministic UI testing/golden capture hooks.

### Performance ceilings

The representative Live UI must remain inside the existing Runner-with-UI targets:

- target under 75 MB resident memory; ceiling 125 MB;
- target under 2% CPU for the complete two-universe system on the reference DJ laptop; UI must leave meaningful headroom;
- cold start target under two seconds; ceiling four seconds;
- active progress/health repaint must not increase DMX jitter beyond established limits;
- hidden/idle panels must substantially reduce repaint/subscription work.

A framework's marketing claim is not acceptance evidence; measure the packaged EmberLights spike.

## Shortlist

### Candidate A — Slint with C++ integration

Why it leads the first spike:

- declarative and responsive UI model;
- native C++ integration;
- no browser runtime;
- software and multiple GPU rendering backends;
- current documentation exposes OS accessibility integration;
- intended for lightweight desktop/embedded UI;
- a royalty-free desktop/mobile/web license is available with attribution, alongside GPL/commercial options;
- strong fit for reusable components and semantic properties.

Risks to prove:

- maturity for a dense desktop-authoring workstation;
- docking/multi-panel behavior;
- very large trees/tables and virtualization;
- custom timeline/waveform performance;
- Windows text input, IME, drag/drop, file dialogs, clipboard, and accessibility quality;
- runtime interpretation is not used for untrusted skins—the EmberLights validator/view graph remains authoritative;
- packaged binary/runtime footprint is measured, not inferred from the core runtime claim.

Official references:

- https://slint.dev/
- https://docs.slint.dev/latest/docs/cpp/
- https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backend_winit/
- https://docs.slint.dev/latest/docs/rust/slint_interpreter/
- https://slint.dev/blog/making-slint-desktop-ready

### Candidate B — WinUI 3 / Windows App SDK with C++/WinRT

Why it is the Windows-native control candidate:

- Microsoft's recommended current native Windows UI platform;
- strong built-in Windows controls, keyboard, focus, UI Automation, and accessibility guidance;
- XAML and C++/WinRT integration;
- Windows design/DPI behavior and testing ecosystem.

Risks to prove:

- Windows-only implementation increases later macOS cost;
- Windows App SDK runtime/deployment can materially increase installer/output size;
- custom skin view-graph translation and complex timeline rendering may require substantial custom controls;
- framework-dependent versus self-contained runtime management;
- startup and resident memory against Runner targets;
- unpackaged/traditional-installer behavior with the existing Inno pipeline.

Official references:

- https://learn.microsoft.com/windows/apps/
- https://learn.microsoft.com/windows/apps/design/accessibility/accessibility-overview
- https://learn.microsoft.com/windows/apps/package-and-deploy/
- https://learn.microsoft.com/windows/apps/package-and-deploy/unpackage-winui-app

### Candidate C — Qt 6 Widgets/Quick, bounded to LGPL-compatible modules

Why it remains a maturity fallback:

- mature C++ desktop framework;
- established models, trees, tables, docking, accessibility, custom painting, and cross-platform support;
- capable of sophisticated Studio authoring interfaces.

Risks to prove:

- footprint and cold-start cost;
- deployment size and plugin complexity;
- Qt licensing/module audit and LGPL compliance for a proprietary distribution;
- some modules are GPL-only under open-source licensing;
- risk of using QML or Qt-specific concepts as the public skin schema;
- unnecessary general-framework weight in Live.

Official references:

- https://doc.qt.io/qt-6/licensing.html
- https://doc.qt.io/qt-6/qtquick-index.html
- https://doc.qt.io/qt-6/qtwidgets-index.html

### Candidate D — Custom retained-mode Win32 + Direct2D/DirectWrite

Why it remains the control/fallback implementation:

- smallest Windows dependency surface;
- exact control over memory, repaint, and scheduler isolation;
- direct compatibility with the current native shell and installer;
- suitable for the permanently bundled Safe fallback and possibly a minimal Live renderer.

Risks:

- very high engineering cost for responsive layout, text, focus, accessibility, controls, input, drag/drop, localization, and complex Studio components;
- higher long-term defect and maintenance risk;
- macOS renderer must later be built separately;
- likely wastes more development effort than it saves unless constrained to Safe/Live.

## Explicitly non-leading options

### Dear ImGui

Useful for diagnostics, engineering tools, and very fast spikes, but not the production end-user surface. The official project describes it as oriented toward content-creation/debug tools rather than average end users and explicitly states that full internationalization and accessibility are not supported. It may remain available for isolated developer utilities, never as the sole public UI.

Official reference: https://github.com/ocornut/imgui

### Browser/WebView/Electron/Tauri-style Runner UI

Rejected as the default Runner foundation because it introduces a browser/runtime tax, expands the attack and packaging surface, and conflicts with the accepted no-embedded-browser Runner direction. A future authenticated remote web control surface is a separate adapter/product decision.

### Extending raw Win32 common controls indefinitely

Rejected as the production skin system. The current shell remains a strangler host and emergency fallback while command/state and skin infrastructure replace its fixed pages/layout.

### JUCE

Not prioritized for the spike. It is capable of audio-oriented custom UIs, but its current licensing and framework fit require a separate business/legal evaluation, and the shortlist already covers a lighter declarative option, a Windows-native option, a mature cross-platform option, and a custom control.

## Recommended architecture hypothesis

Start by testing **one toolkit across Studio and Live**. Avoid committing early to two unrelated UI renderers.

Preferred hypothesis:

```text
C++ domain/Runner
  -> typed command/state facades
  -> validated EmberLights view graph
  -> Slint-backed ordinary components
  -> native custom components for timeline/waveform/patch/preview
```

Permanent emergency fallback:

```text
Runner
  -> tiny hard-coded Win32/Direct2D Safe surface
```

Only consider a split renderer after evidence:

```text
Studio: richer toolkit
Live: smaller renderer
```

A split may be accepted only if one-toolkit performance or complex-authoring capability fails and the public skin schema/component contracts remain shared.

## Spike application

Build a disposable but production-shaped `emberlights_ui_spike` target. It must consume mock/generated command and state registries shaped like the accepted contract rather than call arbitrary demo callbacks.

### Required screens

1. **Safe Live**
   - Runner state;
   - Universe 1/2 status;
   - Work Light;
   - Release All;
   - Stop;
   - Blackout.

2. **Reference Live**
   - pinned health strip;
   - three large parameter controls;
   - movement/strobe/color override groups;
   - 32-pad Autoloop matrix;
   - four-bank window;
   - active/progress/repeat/exclusive states;
   - three or more vertical intensity faders;
   - diagnostics drawer.

3. **Default Live**
   - different layout over the identical command/state data;
   - page/panel switching and responsive collapse.

4. **Fixtures + Static Looks Studio slice**
   - fixture-profile Library search/import/provenance and useful empty/error/read-only states;
   - ordered channel grid with 8/16-bit encoding and named DMX ranges;
   - fixture/group and Static Look selection;
   - color mixer, XY position, faders, rate controls, slot choices, and safety-gated triggers driven by `ember.fixtureControlSurface`;
   - explicit `RELEASE`/`SET`/`FORCE_ZERO` ownership;
   - bounded preview state that is clearly unavailable while Live is running;
   - raw DMX diagnostics only in an explicit Advanced drawer;
   - completion of the workflow at both 1366×768 and 1920×1080 without a separate modeless Inspector.

5. **Dense Studio stress surface**
   - library tree with at least 10,000 synthetic rows;
   - track hierarchy with 256 synthetic tracks;
   - timeline canvas with dense cue blocks/curves;
   - waveform placeholder/custom draw surface;
   - split/dock resizing.

6. **Skin failure**
   - invalid package load;
   - failed reload while the current skin remains active;
   - Safe fallback first-load behavior.

### Required update loads

- BPM/phase state at 20–60 Hz;
- active Autoloop progress at 30 Hz;
- adapter/health state at 4 Hz;
- static project/library state on demand;
- hidden panel subscription suspension;
- simulated command acknowledgements/rejections;
- 30-minute active repaint test;
- repeated skin switch/reload test.

## Measurement matrix

Run each viable candidate under equivalent release configuration.

| Measurement | Required result |
| --- | --- |
| clean-process cold start | median and p95 |
| installed size delta | executable + runtime/plugins/assets |
| resident working set | startup, idle Live, active Live, Studio dense |
| CPU | idle, progress, timeline pan/zoom, diagnostics open |
| UI frame/repaint | median/p95 and dropped frames |
| input latency | button, fader, keyboard, touch simulation |
| command dispatch | UI event to facade timestamp |
| state propagation | snapshot publication to rendered feedback |
| DPI | 100/125/150/200% screenshots and clipping report |
| software renderer | launch and operation without discrete GPU |
| accessibility | focus tree, names, roles, values, Narrator/UIA inspection |
| package behavior | valid, invalid, oversized, missing asset, incompatible schema |
| installer | clean Windows VM installation/startup smoke/uninstall |
| testability | deterministic component and golden capture capability |

The spike does not need real DMX, but it must run beside the normal deterministic Runner/qualification harness or a representative scheduler load to detect contention and jitter impact.

## Pass/fail gates

A candidate fails the primary role when any of the following cannot be remedied without invalidating its advantage:

- cannot meet Runner-with-UI ceiling;
- requires an embedded browser;
- lacks a credible Windows accessibility path;
- cannot render the dense Studio skeleton responsively;
- cannot support custom timeline/waveform components;
- deployment conflicts with installer/offline requirements;
- licensing cannot support the intended ownership/distribution model;
- skin switching/reload causes output/scheduler coupling;
- public schema must expose toolkit-specific implementation details.

## Decision scoring

| Dimension | Weight |
| --- | ---: |
| Live footprint/startup/CPU | 25 |
| Studio capability and custom rendering | 20 |
| C++ integration and scheduler isolation | 15 |
| Accessibility/input/DPI | 15 |
| Skin/runtime fit | 10 |
| Deployment/installer | 5 |
| Licensing/ownership | 5 |
| Cross-platform trajectory | 5 |

No candidate wins solely on total score if it fails a pass/fail gate.

## Current recommendation

**Slint is the first implementation spike candidate**, not yet the accepted production toolkit. It most directly matches the desired declarative, responsive, native, C++-integrated, non-browser, lightweight architecture and documents both software rendering and accessibility support.

**WinUI 3 is the control comparison** for Windows-native accessibility and widgets, with deployment/footprint as the central risk.

**Qt 6 is tested only if Slint fails complex Studio capability or if a small targeted benchmark can be produced efficiently.** It is the mature fallback, not the default assumption.

**Custom Direct2D/DirectWrite remains the Safe fallback baseline** and the escape hatch if every framework violates Live requirements.

### 2026-08-13 resource refresh

- Slint 1.17.1 is the current upstream release reviewed for the next spike. Official C++ documentation provides Windows/Linux x86-64 binary packages that require no Rust development environment; source builds currently require Rust 1.92 or newer. The first Windows measurement should use the supported MSVC package, then separately qualify the repository's cross-build route.
- Slint's `native` style resolves to Fluent on Windows and explicit `fluent-dark` is available. EmberLights semantic tokens remain authoritative; a toolkit style may supply ordinary-control behavior but may not replace status/safety/content meanings.
- Microsoft Segoe Fluent Icons is the immediate Windows-supplied glyph bridge, with the Windows 10 MDL2 fallback. The MIT [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) repository is the candidate vector source once assets are packaged for the accepted renderer.
- QLC+ now identifies its source as Apache 2.0. That supports audited selective adaptation with notices, but it does not make a full Qt UI fork automatically small, portable, or license-governance-free. Preview 88 copied no QLC+ source or visual asset.
- `36_UI_RESOURCE_ADOPTION_AND_DEFAULT_2_1_CHECKPOINT.md` implements the reusable token/navigation/layout bridge and a visible Win32 strangler improvement. It is input to the equivalent toolkit spike, not evidence that raw Win32 won.

## Decision output

Issue #37 must commit:

- spike source and reproducible build commands;
- exact framework versions and licenses/modules used;
- machine specifications;
- raw machine-readable measurements;
- screenshots and accessibility inspection evidence;
- installer/startup-smoke evidence;
- failures/workarounds;
- weighted decision record;
- accepted toolkit or next bounded experiment;
- updates to `03_ARCHITECTURE.md`, decision ledger, and release gate.

No toolkit is accepted by prose opinion alone.
