# Backyard-party operator Preview 95 handoff — 2026-08-13

Status: contract-tested unsigned Windows x64 testing preview.
Native warnings-fatal Make, fresh CMake/CTest, exact payload/archive checks,
and repeat NSIS identity pass. Native installed-Windows lifecycle and physical
hardware evidence remain pending.

## Primary outcome

EmberLights can now show what the production Runner actually rendered and what
the output thread actually routed, rather than inferring final output from a
profile draft, offline preview, or cumulative write counter. The installer
also carries an output-disabled editable IR-4 6CH project with exact
Blackout/R/G/B/W/A looks so White/Amber truth can be isolated without a
permanent special-case correction button.

## What changed

- Every rendered slot has fixed bounded attribution for fixture, mapping,
  capability, semantic property, winning layer, value mode, encoding/fine
  role, and default/constant/property/capability/conflict/safety origin.
- Runner's scheduler still performs fixed render/copy/SPSC work. The output
  thread publishes one immutable latest snapshot under a reader-side mutex
  after attempting the configured routes.
- The snapshot contains generation, sequence, monotonic render time,
  pre-global-blackout and actual routed frames, global-blackout status, and
  per-backend configured/attempted/accepted/error evidence.
- `RunnerFrameInspector` is toolkit-neutral and read-only. It resolves project
  fixture/profile/channel metadata, hashes exact two-universe and per-universe
  bytes, bounds rows/diagnostics, identifies stale or invalid evidence, and
  compares complete raw references or one-hot frames with cause labels.
- System Diagnostics includes this report through the existing Copy/Save
  workflow and no longer jumps to the top every 250 ms while an operator is
  reading it. An active recognized IR-4 bench look gets an automatic manual
  reference comparison.
- The immutable IR-4 software contract now covers Blackout, Red, Green, Blue,
  White, and Amber. White is CH4 and Amber is CH5 in the manual-backed 6CH
  reference; CH6 Purple/UV stays zero. Full DMX and JLS1 differences are
  retained, and any unrelated U2 output fails the comparison.
- `Templates/EmberLights-IR4-6CH-Editable-Bench.emberlights` is a separate
  Local manual-derived clone. Every physical output starts disabled. It opens
  from the Start menu, can exchange W/A or any other compatible direct pair in
  the general channel workbench, and must be saved plus Stop/Start before Live
  uses a revised profile.
- `Tools/emberlights_migrate.exe template-ir4-6ch-bench` can regenerate the
  output-disabled operator project without adding another project schema.

## Installed IR-4 test route

1. Isolate one fixture in exact 6CH mode at U1/A1 and close every other DMX
   application. Keep a physical disconnect within reach.
2. Open **Start > EmberLights > IR-4 6CH Editable Bench**, then immediately
   **Save As** outside the installation folder.
3. In Connections, explicitly select SoundSwitch Micro U1 and **Save & Apply**.
   Do not enable another backend.
4. Start Show and trigger Blackout/R/G/B/W/A. Save Diagnostics after White and
   Amber. Compare the actual routed byte/property with the observed fixture.
5. If routed White=CH4 and Amber=CH5 are exact but physical colors are reversed,
   Stop Show, exchange the two compatible functions in the Local profile,
   Save Profile/project, then Start Show and repeat. Retain before/after reports.

The complete installed procedure is `docs/IR4_6CH_RUNNER_FRAME_TEST.md`.

## Source verification at this checkpoint

- warning-fatal Make test: complete 33-executable surface passed;
- fresh Release CMake configure/build: passed;
- fresh CMake/CTest: 38/38 passed;
- frame-inspector, IR-4 six-look/JLS1, existing Raw Hardware Test, Static Look
  physical preview, profile workbench, Autoloops V2, MIDI/controller, Action,
  migration review, and package-contract regressions pass;
- package-contract Python regressions: 18/18 passed;
- registry remains compatible at set `1.2.0`, generation 2, 29 commands,
  39 states, seven component descriptions, digest
  `0a647969b836a52106395709a8d83e5b22126f3c173d52466f9d056d4bf83699`;
- fresh Windows x64 Zig/Clang warnings-as-errors application/tool build:
  passed;
- exact 20-file stage plus manifest and 21-file extracted installer comparison:
  passed;
- repeat NSIS installer identity: byte-for-byte passed;
- continuity diff check: passed at the source checkpoint.

## Installer evidence

- Version: `0.1.0-preview.95.0`
- Source commit: `68cc13be7c0b7d9735609b8ca7533be762b90094`
- Source tree: `3096d9e2c793f17ad1d25da3298d4eb122b3ae77`
- Installer: `EmberLights-0.1.0-preview.95.0-Setup.exe`
- Installer SHA-256: `a099852f1fe5642d55eb69e2cd4f4653fd94e367b73703af14b77160ad5e2dac`
- Installer size: `1,949,363` bytes
- Payload manifest: `EmberLights-0.1.0-preview.95.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `2d35c338e363b535b08e8231d8e11fa685e7510f5b3017f5f51ae2a41665e82e`
- Checksum evidence: `EmberLights-0.1.0-preview.95.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `7bb73787f585e05c203c889c1b3d8034b5703116068b2015ccd913d44f754604`
- Evidence class: contract-tested unsigned testing preview; native Windows
  lifecycle is not claimed.

## Honest remaining boundaries

- run native Windows clean install/upgrade/launch, Start-menu bench, file
  association, uninstall/cleanup, and project/settings-preservation checks;
- bind a successful installed Raw Hardware Test attempt to the reopened Runner
  snapshot, then physically observe both IR-4 units, blackout/no-spill,
  disconnect/reconnect, and endurance;
- publish frame inspection through the canonical broker/component state path;
  the current Win32 Diagnostics adapter remains a recorded transitional bypass;
- activate the bridged fixture components in the production skin runtime and
  persist/activate/bind authored Actions through Studio history;
- complete virtual intensity, revisioned qualification/invalidation, cell-aware
  fixtures, a pinned offline fixture corpus, and measured color calibration;
- acquire authorized SoundSwitch controlled-delta evidence before exact decoder
  claims; WOLFMIX remains research-only with no parser/import claim;
- continue the accepted Static Look, Autoloop, scripted-track, open-controller,
  UI/skin/Designer, reliability, soak, and gig-readiness program.

Matching software bytes prove only software behavior. They do not establish
physical fixture truth, installed-Windows qualification, interface readiness,
or gig safety.
