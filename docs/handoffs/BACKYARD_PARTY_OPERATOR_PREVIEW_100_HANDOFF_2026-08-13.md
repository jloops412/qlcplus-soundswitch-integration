# Backyard-party operator Preview 100 handoff — 2026-08-13

Status: contract-tested unsigned Windows x64 testing preview. Native
installed-Windows UI/lifecycle/accessibility and physical hardware evidence
remain pending.

## Primary outcome

Preview 100 turns the shared profile-backed fixture-function catalog into a
professional, modeless **Fixture Control Inspector** across Live Overrides,
Static Looks, committed Autoloop V2 editing, and MIDI Mapping.

A Fixture Control is one direct profile DMX channel or one named function in a
documented channel range. It is not a SoundSwitch Attribute Cue. Attribute
Cues remain a future reusable authored-asset layer above these controls so an
imported cue can retain identity and update every script or Autoloop reference
when edited.

The Inspector adds bounded multi-field search, category counts, target-scoped
session favorites, favorites-only filtering, stable selection, a normalized
value/range-position slider with presets, explicit slot/range/direct input
semantics, target coverage and availability, and exact per-fixture profile/DMX
diagnostics. **Use Selected Control** returns the stable identity to the
existing destination editor; raw bytes remain diagnostic rather than becoming
a second command path.

Availability is surface-aware. Live requires one exact atomic semantic command
for every target member. Static Looks and Autoloop may retain exact per-fixture
values for supported members. MIDI may compile a homogeneous group mapping or
bounded per-fixture mappings. Protected functions remain absent; existing
safety gates are unchanged.

Registry `1.5.0`, generation 2, digest
`7e22086416145ec19d10d309e492d64eb60e96d3e8a8a59b2f81ffdbadc7026f`
is compatible-additive. The project format, compiled package schema, 29
commands, 39 states, Runner scheduling, output routing, blackout, and hazards
are unchanged.

## Installer evidence

- Version: `0.1.0-preview.100.0`
- Source commit: `e8693be9378e9282b3f0f7b345cec66f6d6a8a64`
- Installer: `EmberLights-0.1.0-preview.100.0-Setup.exe`
- Installer SHA-256: `e368928de33b97bb7a7510360ea421a18fd9d6abe5cbfcdfac7d3a0d78bca2d5`
- Installer size: `1,997,777` bytes
- Payload manifest: `EmberLights-0.1.0-preview.100.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `3152ffc02567c9ba151eb34fea0512bd8e9a0c801c9204268ef4362d03b1be74`
- Checksum evidence: `EmberLights-0.1.0-preview.100.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `4cbf8c68704be26ed1ab9eefeb88ab987d813484fec3a5b77d49db6f8fa8d671`

## Verification evidence

- warning-fatal full native Make build passed;
- all 34 native test executables passed, including new surface-specific
  partial/mixed/safety, favorites, stable-selection, and diagnostic cases;
- registry generation/check, all 12 governance tests, generated Ember Action
  adapter check, surface-contract gate, and updated deterministic digest
  goldens passed;
- WinMM and DMX USB Pro Windows syntax gates passed;
- clean Windows x64 Zig/Clang application and supported tool targets built;
- package-contract regressions passed 18/18;
- exact 20-file payload plus manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and its normalized 21-file product
  payload matched the staged bytes exactly;
- output-disabled V1 template and editable output-disabled IR-4 bench project
  remain included.

Evidence class: **contract-tested unsigned testing preview** on a non-Windows
host. This is not installed-tested, accessibility-qualified, physically
qualified, gig-qualified, or a public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch. Run the Preview 100 installer on Windows
   11 and allow it to upgrade the existing per-user EmberLights installation.
2. Confirm About reports `0.1.0-preview.100.0` and source commit
   `e8693be9378e9282b3f0f7b345cec66f6d6a8a64`; confirm the prior project and
   settings still open.
3. Keep every physical output disabled. In **Static Looks**, choose a fixture or
   group and open **Browse Controls…**. Exercise search, categories, a favorite,
   Favorites only, presets, the slider, diagnostics, double-click, and **Use
   Selected Control**. Confirm the chosen control returns to the Look editor.
4. Repeat **Browse Controls…** in **Live > Fixture Overrides**, committed
   **Autoloop V2 fixture controls**, and **MIDI > Set Fixture Control**. Compare
   coverage and availability for the same mixed-profile group; Live should be
   stricter when one atomic command cannot be exact.
5. Inspect at least one 16-bit Pan/Tilt channel, one direct color/intensity
   channel, one Gobo/Prism slot, and one continuous range. Confirm the profile
   channels, fine channel, selected raw realization, defaults, blackout,
   highlight, and revision match the active fixture mode.
6. Verify a protected reset/service range is absent. If a safety-classified
   function exists, confirm Live/Autoloop/MIDI refuse it and Static Looks retain
   the existing Runner warning/gate instead of bypassing policy.
7. Report missing/duplicated controls, wrong coverage, wrong channel/range,
   clipped text, stale selection, focus/keyboard trouble, crash, preservation
   issue, SmartScreen message, or installer lifecycle problem verbatim with the
   fixture profile/mode and screenshot.

Keep an independent backup controller and do not use this preview as the only
lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps
  uninstall, shortcut/registry cleanup, and project/settings preservation;
- Windows DPI, keyboard traversal, Narrator/UI Automation, contrast, long-list,
  and modeless focus evidence;
- persistent operator favorites and production `.emberskin` activation;
- reusable Attribute Cue assets and dependency-updating SoundSwitch migration;
- pinned OFL corpus, bounded GDTF adapter, richer QXF cases, physical units,
  neutral/home semantics, and head/cell geometry;
- full Autoloop V2 timeline and Palette/Position/Attribute/Movement/Effect
  authoring;
- owned-fixture/controller observations, eight-hour soak, shadow rehearsals,
  Authenticode signing, and gig qualification.
