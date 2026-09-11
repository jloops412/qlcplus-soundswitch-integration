# V32 creative pass

**Publication follow-up:** The owner authorized GitHub publication on
2026-09-11. See [V32_GITHUB_PUBLICATION.md](V32_GITHUB_PUBLICATION.md) for
the current PR and exact published source identities. The original second-pass
record below describes the checkpoint before publication.

Date: 2026-09-11. Status: unpublished source/workspace review candidate.

V32 continues the completed V31 reliability checkpoint at
`7808ed1aad81716eff0fe9ae3da4fc1a120d29a3`. That exact commit and tree were
recovered from `QLCPlus_V31_First_Pass_Checkpoint.zip`, then V31's structural
and corruption checks passed before creative editing began. GitHub PR #112
remains at V30 `f842153acb8edee790b8c47650c81b0285dcd33d`.

This is the owner's second pass: full-rig creative redesign and the known
MOVE/STROBE ownership gap. GitHub publication remains reserved for the later
explicit third-pass request. No remote branch, issue, PR, release or workflow
was written or triggered in this pass.

**This checkpoint is not an installable Windows release.** It requires a new
matched `soundswitch.dll`; no V26/V27/V30/V31 binary implements its private
effects route. The exact QLC+ source remains
`a124abebe0b5ad6077727c561a5a0e1f3730810c`, UI `5.3.0 GIT a124abe`,
Qt `6.8.1`, Windows x64 MinGW 13.1.0. The stock executable, MIDI translation
and focused OS2L timing patch are unchanged.

## Creative result

All 128 public Autoloops and all 32 Priority Looks have new complete fixture
scores. Their public names, pad order, Function IDs, fixture patch, native
owners, current-loop monitors, selected-start mapping, dwell and latch/hold
routes remain. Each raw loop now has 16 equal subdivisions. The original
eight Scene IDs remain and eight new Scene IDs extend each phrase.

| Bank | Programming role | Phrase at 1x | Timing character |
|---|---|---|---|
| Medium | Saturated room colour with pixel rolls, responses, comets and restrained paired movement | 4 measures | One beat per step; smooth or defined transitions |
| Colorful | Deliberate two-to-six-colour palettes, zone geometry and pixel travel | 2 measures | Half-beat steps; wheel colours remain stable |
| Slow Dance | Warm/soft palettes, persistent room light, shallow breathing and small mover drift | 8 measures | Two-beat crossfades; no rhythmic blackout |
| Flashy | Short hits, syncopation, alternating fixtures, builds and drops | 1 or 2 measures | Quarter/half-beat steps with clean dimmer cuts |

The IR-4s provide the room colour, the tubes carry fine spatial rhythm, the
six Wash zones supply larger patterns and accents, and the Focus pair supplies
beam shape and restrained movement. Wash/Focus values are authored directly;
they are no longer derived from averaged IR-4/tube output. Dark space and
quiet intervals are deliberate in energetic banks. All fixture classes still
participate in every full-rig score.

Priority still Looks use gradients, differentiated room/zone/tube intensity
and a secondary palette accent. The ten moving Looks retain their public
Chaser types and eight leaves, now with coherent eight-measure phrases.
Static Looks remain static. Candlelight, first-dance, warm-white and calm
Looks do not acquire strobe or prism changes.

All nine previous timing exceptions (`573,576,632,633,635,636,645,657,658`)
now use the same common musical timing contract as the rest of their banks.
Syncopation lives in repeated equal-length steps and light/dark patterns.
Raw `SpeedModes` remain `Common`; no PerStep duration bypass is reintroduced.
All five speed presets are rebound to the final score timings. Zero fade
remains exactly zero at every multiplier. QLC's integer beat units require
rounding a 250-unit duration to 62 at 4x; this is under one beat-thousandth
per step, not a second clock.

The full inventory is in `v32-review/V32_CREATIVE_CATALOG.md` and `.json`.
`V32CreativeProgram.py` is the independently authored score source, organized
by existing pad rather than fixture-name inference.

## Movement and optics

Autoloops use a compact continuous pan/tilt envelope near the decoded forward
aims, avoiding the prior large jumps and the source Stage Left tilt near
65,535. Successive authored raw poses differ by no more than 600 pan units
and 250 tilt units, including the loop seam. Slow Dance uses a smaller envelope;
Flashy holds its beam position while dimmer rhythms do the work.

Wheel colours, gobo, prism and focus settings stay fixed within each raw
phrase. There is no rainbow wheel spin or repeated optic indexing while lit.
Gobo rotation is used only by the named rotation scores. Native QLC fades own
motion speed, so the Focus pan/tilt speed byte is zero instead of adding a
second fixture-side speed limiter. Existing nine decoded position latches
retain their exact physical/private values.

These are uncalibrated coordinates. They do not prove a safe physical aim,
left/right placement, focus sharpness, identical colour between fixtures, or
visual quality in the venue. Bench A/B identity, Wash zone order, tilt
clearance, optics and relative levels before qualification. Real Focus UV,
reset/internal-show channels, and unqualified IR-4/Wash strobe macros remain
zero throughout the changed creative frames.

## Native MOVE/STROBE ownership

The pinned QLC+ source confirms that `Scene::flash()` uses a flashing fader and
ForceLTP, while an ordinary Chaser does not implement that Scene override
contract. Sources reviewed:

- [Scene implementation at the pinned commit](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/scene.cpp)
- [Chaser runner](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/chaserrunner.cpp)
- [Generic dimmer definition and fixture loading](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/fixture.cpp)

V32 programs MOVE and STROBE in the previously empty native Universe 4.
It adds only two private Focus instances and three generic HTP control bytes.
No extra show-time application, timer, scheduler, bridge or core patch exists.

| Internal item | Identity and purpose |
|---|---|
| Universe 4 output | Only `soundswitch:effect-layer`; never physical DMX/network |
| Focus IDs 209/210 | Same profiles and addresses 81/99; private MOVE pan/tilt/speed |
| Generic fixture 211, addresses 1-3 | MOVE-active, STROBE-active, brightness gate; native HTP dimmers |
| Existing MOVE Function 2184 | Native 16-step Focus sweep, new leaves 2402-2417 |
| Existing STROBE Function 808 | Native beat Chaser, open/closed gate leaves 2400/2401 |
| Existing position Scenes 2175-2183 | Add exact matching pan/tilt payloads for 209/210 |

The plug-in composes in this order: existing base/Priority selection, native
MOVE parameter mask, native STROBE intensity multiplication, existing Global
and Group 1-4 intensity. MOVE copies only pan, pan fine, tilt, tilt fine and
pan/tilt speed. Position Flash latches write into the same native layer and
therefore retain precedence. Releasing a position while MOVE runs returns to
its currently advancing sweep.

STROBE now gates the active look's brightness. It retains its colour and aim;
it does not force white, open a closed UV source or turn on a dark fixture.
Colour holds remain subject to the gate. Its two half-beat steps give one
open/closed cycle per beat, independent of Autoloop chase-speed selection.
The gate/active-marker ratio avoids applying native Grand Master gain twice.

Effect ownership markers and values arrive in the same universe frame.
Stopped native Functions release the HTP markers. The plug-in stores owned
frame bytes, ignores absent/inactive markers and clears the cache on private
route removal or invalid frame length. It does not infer Function state or
generate timing. The extra advertised output is appended without reordering
existing advertised output lines and is identified by UID.

MOVE's first step seeds its private pose immediately, then subsequent steps
fade. Activation can move from an arbitrary underlying aim to that first pose;
this handoff and the cycle seam still need the physical bench. Do not claim
smooth physical takeover from the source-level envelope checks.

## Preservation and validation

- Exact V31 source workspace SHA-256 remains
  `c6e03f865cfeda3fb5c578221ade3ef6883d87c033072a41bf5f1511f7f13651`.
- Eleven physical fixtures and eleven private Priority fixtures remain
  byte-for-byte unchanged. The three added fixtures are internal only.
- All 849 widget IDs and rectangles, all 138 native owners, all pad/seek/
  mode/dwell/transport/override/feedback channels remain. Only hidden speed
  timing tables and two explanatory captions change in the Virtual Console.
- All 980 Functions outside the explicit creative/effect/position scope are
  unchanged. There are now 3,297 Functions, 2,048 raw Scene leaves and 102
  Priority leaves. No old Function is removed.
- V32 validation checks 1,280 raw speed preset bindings, original Scene IDs,
  full frames, fixture and graph integrity, effect isolation, all protected
  control structure, timing, motion bounds, optic stability, and eight
  deliberate corruption cases. The checked-in candidate must match two
  deterministic builds.
- The actual-source protocol, intensity, performance/seek, Priority, MIDI
  and new effects targets pass in Linux with Qt 6.8.1. MIDI uses the existing
  fake WinMM test boundary. Plug-in and smoke-test translation units also
  pass syntax compilation against the pinned interfaces using that test
  boundary. Neither result is a Windows DLL or pinned-host load test.
- V26/V27 protected package checks and the unchanged V31 baseline checks are
  recorded in `V32_VALIDATION_EVIDENCE.json`.

The offline review derives all preview frames from the actual V31/V32 XML.
It approximates RGB colour and phrase transitions; it is not an optical or
photometric simulator. Its JavaScript passes syntax validation. Browser layout
and interaction inspection was unavailable because this session's browser
could not reach the local preview (`ERR_BLOCKED_BY_CLIENT`); do not record it
as a successful browser test.

## Continue from here

1. Read this file, the V32 validation evidence and catalog. Preserve V31 and
   every existing release folder. Rebuild V32 from its protected input with
   `Build-V32Creative.py`; verify using `Test-V32Workspace.py --self-test`.
2. On the owner's third-pass instruction, build final source with the exact
   Windows tuple. The prepared workflow now includes effects tests and the
   extra virtual-output plug-in smoke check. Validate the pinned QLC+ host.
3. Bench speed/dwell/seek and Priority handoff, then position + MOVE + STROBE +
   colour-hold combinations. Check STOP, private route removal, native Grand
   Master, all four groups, reconnect, and every intended physical output.
4. Review all 160 scores on the rig and adjust relative levels/aim/optics from
   actual observation. V32 is a complete programming pass, not a claim that
   an algorithm or schematic preview establishes professional visual quality.
5. Assemble one matching package with both required DLLs, definitions/profile,
   source tuple, exact hashes, simple File Explorer installation/rollback,
   receipts and evidence. Publish through the existing repository lane only
   when that third pass is requested.

The older independently scheduled Priority-universe late-frame boundary,
physical output, repeated hot-plug, combined workload and gig qualification
remain open. No new build inherits physical qualification from V26/V27.
