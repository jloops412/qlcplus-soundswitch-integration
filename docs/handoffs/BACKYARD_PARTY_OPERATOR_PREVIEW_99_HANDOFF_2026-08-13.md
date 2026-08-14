# Backyard-party operator Preview 99 handoff — 2026-08-13

Status: contract-tested unsigned Windows x64 testing preview. Native
installed-Windows UI/lifecycle/accessibility and physical hardware evidence
remain pending.

## Primary outcome

Preview 99 closes the split between generic fixture properties and named DMX
functions. One profile-backed **Fixture Attribute** picker now includes both:

- direct controls such as Intensity, RGBWAUV, Pan, Tilt, Focus, Zoom, Iris, and
  other mapped channels;
- named compound-channel selections such as Shutter Open, Gobo slots, Prism,
  and bounded effect ranges.

Fixture Profiles remain the source of truth for channel number, encoding,
coarse/fine mapping, DMX range, default, blackout, highlight, owner/head/cell,
and source revision. Static Looks, committed Autoloop V2 authoring, Live
overrides, MIDI/controller mappings, Ember Actions, and the bridged skin
component all consume the same stable semantic choice identity. Raw DMX is
diagnostic evidence, not a second control engine.

The Default adapter places the profile-backed picker before the generic
semantic fallback in Live Overrides and Static Looks. Labels show profile
channel/range/encoding evidence; direct 16-bit controls retain coarse/fine
diagnostics. Protected reset/service/custom ranges remain absent and existing
safety policy is unchanged.

Registry `1.4.0`, generation 2, digest
`3f44d8129607601db67f462b3710b52bf4c889d6238cebc10a7f4a2a957cf659`
is compatible-additive. The project format, compiled package schema, 29
commands, 39 states, Runner scheduling, output routing, blackout, and hazards
are unchanged.

## Installer evidence

- Version: `0.1.0-preview.99.0`
- Source commit: `9e3da847ae425bdab015e3a982ab70c417b2272c`
- Installer: `EmberLights-0.1.0-preview.99.0-Setup.exe`
- Installer SHA-256: `345e97f23f209b4aa86708e4a7bb2cabc6dcc80b7c0bd87e0033685f399361cc`
- Installer size: `1,978,154` bytes
- Payload manifest: `EmberLights-0.1.0-preview.99.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `3f841357b2cb52ff193f651fd346c73e9331e8708e121786275b2202d68d3718`
- Checksum evidence: `EmberLights-0.1.0-preview.99.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `33f9241964ab8d66f261d438ab042b69dee735b26637a64b9495b187cf79244e`

## Verification evidence

- warning-fatal full native Make build passed;
- the complete native Make test surface passed, including new direct 8-bit,
  direct 16-bit coarse/fine, mixed-profile, Static Look, Autoloop, Live command,
  MIDI/controller, and Ember Action regressions;
- registry generator/check/diff, all 12 governance tests, generated Ember Action
  adapter check, and surface-contract gate passed;
- WinMM and DMX USB Pro Windows syntax gates passed;
- clean Windows x64 Zig/Clang application and supported tool targets built;
- package-contract regressions passed 18/18;
- exact 20-file payload plus manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test/extraction passed and its normalized 21-file product payload
  matched the staged bytes exactly;
- output-disabled V1 template and editable output-disabled IR-4 bench project
  remain included.

Evidence class: **contract-tested unsigned testing preview** on a non-Windows
host. This is not installed-tested, accessibility-qualified, physically
qualified, gig-qualified, or a public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch. Run the Preview 99 installer on Windows
   11 and allow it to upgrade the existing per-user EmberLights installation.
2. Confirm About reports `0.1.0-preview.99.0` and source commit
   `9e3da847ae425bdab015e3a982ab70c417b2272c`; confirm the prior project and
   settings still open.
3. Open the packaged editable IR-4 bench project with physical outputs still
   disabled. In **Studio > Fixture Profiles**, inspect the IR-4 channel map and
   confirm direct emitter rows read as semantic attributes/functions with exact
   channel and encoding data.
4. In **Static Looks**, select the IR-4 fixture/group. Use the primary **Fixture
   Attribute • profile-backed** picker to apply direct Red at 25%, then preview
   offline. Confirm the assignment and DMX preview follow each active profile's
   channel order. Keep physical preview off for this UI pass.
5. In **Live > Fixture Overrides** with outputs disabled, confirm the primary
   Fixture Attribute picker contains ordinary direct channels as well as any
   named capabilities. Select direct Red/Intensity and verify the advanced
   semantic fallback mirrors the same attribute without changing the project.
6. In **Autoloops > AutoScript fixture controls**, confirm the picker offers
   direct profile attributes in addition to named capabilities and can add one
   exact committed V2 event without stable-ID or raw-DMX entry.
7. In **MIDI**, choose a fixture/group Set Attribute action and select a direct
   continuous Fixture Attribute. Learn a spare fader and confirm the mapping
   keeps Continuous behavior and Soft Takeover; repeat with a named slot if the
   active profile has one.
8. Report any missing/duplicated attributes, wrong channel/fine-channel label,
   confusing picker order, clipped text, value mismatch, stale choice, crash,
   preservation issue, SmartScreen message, or installer lifecycle problem
   verbatim with the fixture profile/mode and screenshot.

Keep an independent backup controller and do not use this preview as the only
lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps
  uninstall, shortcut/registry cleanup, and project/settings preservation;
- Windows DPI, keyboard traversal, Narrator/UI Automation, contrast, and long
  attribute-list evidence;
- searchable virtualized attribute Inspector/category tabs/favorites and full
  production `.emberskin` activation;
- pinned OFL offline corpus, bounded GDTF adapter, broader QXF fallback corpus,
  richer physical units, head/cell geometry, and neutral/home semantics;
- final Autoloop V2 timeline/palette/position/movement/effect workflow;
- owned-fixture/controller observations, eight-hour soak, shadow rehearsals,
  Authenticode signing, and gig qualification.
