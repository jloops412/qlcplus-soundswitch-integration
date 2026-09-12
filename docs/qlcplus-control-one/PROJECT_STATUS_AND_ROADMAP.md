# Project Status and Roadmap

## V34 reliability work — 2026-09-12

See [the V34 testing release record](V34_TESTING_PACKAGE_RELEASE.md) for the
candidate, exact evidence and package status. The owner has authorized repairs,
Windows testing handoff and GitHub publication. V34 addresses STOP/Flash release,
independent WHITE/BLACK/UV ownership, final BLACK priority, per-cell color
brightness, discrete Focus transitions and selected creative score defects.
It preserves the eleven-fixture patch, 128 loops, 32 Priority Looks and existing
playback owners. The dedicated raw-loop feedback observers avoid depending on
visual-strip enable state. Select the real U1 physical output once in QLC+;
private U3–U6 routes use stable UIDs. Hardware/gig qualification remains pending.

## Mission and selected architecture

The selected architecture is QLC+ plus one focused native hardware/workflow
plug-in.

At show time:

```text
VirtualDJ -- direct OS2L --> QLC+
Control One -- MIDI ------> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

QLC+ owns fixtures, Scenes, Chasers, beat timing, Autoplay order, dwell, the
Virtual Console, persistence, and routing. The SoundSwitch plug-in exists only
for proprietary SoundSwitch USB transport, Control One
translation/feedback/reconnect, full-frame Priority Look selection, and
intensity routing that QLC+ cannot express cleanly. V30 retains the focused,
build-matched QLC+ OS2L correction; the stock core executable remains
unchanged.

## Current status: V32 source/workspace review candidate

See `V32_CREATIVE_PASS.md`, `V32_VALIDATION_EVIDENCE.json` and
`v32-review/V32_CREATIVE_CATALOG.md`. V32 contains all 128 redesigned 16-step
Autoloops, all 32 redesigned Priority Looks and an internal native effects
layer for MOVE and STROBE. The exact V31 checkpoint is preserved. Existing
physical patch, owner/seek/hold/feedback controls and 849 widgets are retained;
hidden speed tables follow the new common musical timing. All six native
software targets and V32 preservation checks pass locally. The owner authorized
publication on 2026-09-11 through PR #112 and issue #113. See
`V32_GITHUB_PUBLICATION.md` for exact source identities and the PR Checks tab
for CI. Pinned-host, physical and gig qualification remain pending.

## Protected V31 reliability checkpoint

The active first-pass result is described in `V31_RELIABILITY_PASS.md`; the
complete reconciled history and outstanding gates are in
`V31_CONTINUITY_AUDIT.md`. It starts from PR #112 V30 at
`f842153acb8edee790b8c47650c81b0285dcd33d`, not from the older main-branch V26.
V31 repairs mouse/controller parity, transient release and playback-state
regressions, private performance/position coverage, and Priority buffer
ownership. Original fixture addresses, all raw Autoloop creative payloads,
and Priority Look creative payloads are preserved.

V31 was the unpublished first-pass source/workspace checkpoint. Its tree is
preserved in the V32 publication; the creative pass and publication request
have now followed. Do not claim
a matched Windows DLL, pinned-host test, physical rig test, or gig qualification
from deterministic software results.

V32 implements the second-pass MOVE/STROBE ownership and programming. The
IR-4 custom fixture definition was recovered as an independently authored
profile verified against its original manufacturer manual.
Old package-manifest mismatches and V28/V29's unavailable artifacts are
recorded explicitly in the continuity audit.

## V30 source checkpoint

V30 was the corrective candidate entering this audit. It is generated deterministically from
the reviewed V27 full-rig workspace and repairs four live-performance contracts
before the Autoloop creative library is reviewed again: working raw-loop speed,
exact selected-loop start during sequential or randomized autoplay, reliable
Priority Look ownership/feedback, and independent Shift-held color overrides
across the physical and private fixture layers.

The V30 workspace validator covers all 128 raw Autoloops, all 512 possible
Bank/All sequential/randomized selected starts, all 22 physical/private fixture
instances, all latch/hold override routes, and the unchanged private Priority
patch. The matched plug-in adds focused seek and Priority state tests. These are
software and structural results only; the exact Windows host and physical rig
still require controlled observation. See `V30_PERFORMANCE_RECOVERY.md`.

Nine raw loops with variable per-step timing are intentionally normalized to
their reviewed top-level timing in V30 so the speed control is authoritative.
Their Function IDs are 573, 576, 632, 633, 635, 636, 645, 657, and 658. Their
creative timing can be revisited in the later Autoloop-quality pass without
reintroducing the broken PerStep speed contract.

## V27 Full Rig source and rollback

V27 is the preserved full-rig source and packaged baseline. It extends the exact immutable V26
Autoplay Clarity workspace with one Chauvet Wash FX Hex and two American DJ
Focus Spot Two movers. The generated workspace passes the independent V27
structural regression against protected V26 SHA-256
`ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570`.

This status is deliberately narrow:

- V27 is structurally validated at the workspace level.
- The candidate SoundSwitch plug-in passed protocol, intensity, and plug-in
  load smoke tests in CI.
- The candidate has not yet been loaded into the exact pinned Windows QLC+
  host.
- The changed workspace, new fixture definition, new intensity routing, full
  physical rig, repeated hot-plug, combined workload, and gig route remain
  unqualified.

CI run `33241230755` built candidate `soundswitch.dll` SHA-256
`19074A37AA915E1E39124CD14441025A2A83AB06EBDB14BD0953E2910B801DE3`
from reviewed source commit
`bf85057b5958608034decacae8927b0714ee98ed`. Its evidence explicitly records
`pinnedQlcHostAbiTested=false`.

V26 remains the protected generation source and immediate rollback. Do not
edit its workspace, package, or hashes in place, and do not infer that V26's
earlier hardware observations prove V27 hardware behavior.

## Exact V27 fixture patch

Fixture displays use one-based addresses; QLC+ XML stores each start address
one lower. Every V26 IR-4 and tube address is retained. New fixtures occupy
only previously free channels, and display addresses 117–174 remain free.

| Physical ID | Private ID | Fixture | Mode | Physical U1 display span | Private U3 span |
|---:|---:|---|---|---:|---:|
| 0 | 100 | Both Lighting IR-4 1 | `10 Channel` | 001–010 | 001–010 |
| 1 | 101 | Both Lighting IR-4 2 | `10 Channel` | 011–020 | 011–020 |
| 2 | 102 | Both Lighting IR-4 3 | `10 Channel` | 021–030 | 021–030 |
| 3 | 103 | Both Lighting IR-4 4 | `10 Channel` | 031–040 | 031–040 |
| 4 | 104 | Chauvet Wash FX Hex | `40 Channel` | **041–080** | **041–080** |
| 9 | 109 | American DJ Focus Spot Two A | `18 Channel` | **081–098** | **081–098** |
| 10 | 110 | American DJ Focus Spot Two B | `18 Channel` | **099–116** | **099–116** |
| — | — | Reserved | — | **117–174 free** | **117–174 free** |
| 5 | 105 | Both Lighting BO-TUBE192 1 | `40 Channel` | 175–214 | 175–214 |
| 6 | 106 | Both Lighting BO-TUBE192 2 | `40 Channel` | 215–254 | 215–254 |
| 7 | 107 | Both Lighting BO-TUBE192 3 | `40 Channel` | 255–294 | 255–294 |
| 8 | 108 | Both Lighting BO-TUBE192 4 | `40 Channel` | 295–334 | 295–334 |

Both layers remain 334 channels. Universe 3 is a private Priority buffer on
`soundswitch:priority-layer`, line 4. It must never be routed to physical DMX,
Art-Net, sACN, or another output.

## SoundSwitch source review and creative closure

The previously shared `2026.ssproj.zip`, SHA-256
`2C58ED57965CD12A0702252595D4966EF8CAEF4A3B024E24BC001E245FCFE11C`,
was reviewed as creative and fixture provenance. Its addresses were not copied:
the owner's QLC+ patch remains authoritative. The source used a Wash 11-channel
personality at 147 and two Focus 18-channel instances at 31 and 49. V27 instead
uses the exact 40-channel/18-channel patch above so all V26 addresses remain
unchanged.

The source contains a Wash target in all 112 placed timelines, Focus references
in all 112, and explicit Focus group/motion blocks in 72. V27 closes that gap by
integrating the Wash and both Focus fixtures into every live QLC+ creative path,
not just the source timelines that happened to contain explicit mover blocks.

The V27 candidate contains 2,100 Functions: 1,812 Scenes, 150 Chasers, and 138
Collections. Its live creative closure is exactly 1,140 Scene leaves:

- 1,024 raw Autoloop steps across 128 eight-step Chasers;
- 102 Priority Look leaves;
- 5 performance Scenes; and
- 9 color overrides.

Every raw Autoloop step contains a complete physical frame for all eleven
fixtures. Every Priority leaf contains the corresponding complete private
frame. Performance Scenes include the tubes, Wash, and Focus pair. Sparse color
overrides cover all tube emitters, all six Wash zones, and both Focus color
wheels while leaving movement and intensity underneath them.

Nine exact decoded Focus A/B position Scenes use Function IDs 2175–2183, and
Function 2184 (`MOVEMENT — FOCUS A/B SWEEP`) is assigned to the existing MOVE
control. The source proves A/B values, not a physical left/right assignment.
`Disco Ball` is the position collection header, not a tenth position preset.
Installed A/B identity and every aim remain bench checks.

## V27 intensity routing

| Target | Fixture channels affected |
|---|---|
| Global | All designated Group 1–4 intensity/emitter spans in the physical or active private Priority frame |
| Group 1 | Four IR-4 fixtures |
| Group 2 | Wash direct RGBAWUV zone emitters only |
| Group 3 | Four BO-TUBE192 tubes |
| Group 4 | Focus main and UV dimmers only |
| Scripted | Retained control state |

Wash program, speed, auto-dimmer, and strobe channels are excluded from Group
2. Focus movement, color, gobos, prism, shutters, focus, internal shows, and
function/reset channels are excluded from Group 4. Internal Focus shows and
function/reset ranges remain zero in released programming.

## Release lineage

### V20 — protected creative baseline

V20 remains the protected pre-reliability creative baseline. Do not rewrite or
delete it.

### V21 — reliability rollback

V21 introduced the pinned QLC+ 5.3.0 tuple, Control One stale-handle recovery,
LED retry/restore, direct VirtualDJ OS2L keepalive, complete mouse controls, and
the self-testing install/rollback package.

### V22 — unified creative rollback

V22 merged the reviewed Variety Pro Colorful/Flashy donor steps into the V21
control and reliability host while preserving fixture, I/O, ownership, public
ID, and logical-channel contracts.

### V23 — Live Console rollback

V23 kept all 2,090 lighting Functions and reorganized only native Virtual
Console feedback and presentation.

### V24 — Runtime Feedback rollback

V24 unified Surface feedback, corrected positive-edge mouse commands, kept
mouse operation independent of the Control One MIDI-output handle, and added
native current-loop feedback. Its isolated software/runtime tests passed.

### V25 — reviewed V26 source

V25 preserved the V24 Engine and creative show while reducing the Autoplay
tracker footprint. It is V26's reviewed source, not a public deployment target.

### V26 — protected Autoplay Clarity source and rollback

V26 preserves the V25 Engine byte-for-byte and changes only Virtual Console
presentation. It packages the build-matched SoundSwitch plug-in and focused
`os2l.dll` correction. V26 is immutable and is the exact V27 generation source,
not a file to modify or overwrite during V27 work.

### V27 — Full Rig candidate

V27 adds the three physical fixtures and their private Priority mirrors,
complete live creative coverage, full-rig performance and override Scenes,
exact decoded Focus position controls, a full-rig visual bench, and four-group
intensity routing. It is a candidate until the exact package, pinned Windows
host, physical fixtures, fault routes, and combined workload pass their named
gates.

### V30 — Performance Recovery candidate

V30 preserves the V27 fixture patch and creative leaves while correcting the
raw-loop speed architecture, exact sequential/randomized autoplay seek,
Priority state/feedback handoff, complete physical/private color coverage, and
Shift-held color operation. Focus position shortcuts move from Shift + color to
Shift + performance pads 1–9. V30 remains a candidate until the matched plug-in
and workspace pass the controlled physical rig route.

## Physical evidence and safety boundary

Preceding V26 workspaces produced live output through SoundSwitch Micro,
Control One DMX 1, and Control One DMX 2 independently; Control One MIDI/core
LEDs, OS2L, Priority behavior, and essential manual/Autoplay ownership were
also observed. Those observations remain V26 provenance only.

No V27 physical-output, headless, combined-soak, or gig qualification is
claimed. In particular:

- the exact pinned QLC+ Windows host has not yet loaded the V27 candidate DLL;
- the Wash 40-channel personality and physical zone orientation are unresolved;
- Focus A/B placement, nine decoded aims, pan/tilt clearance, focus, gobos,
  prism, shutter behavior, and physical addresses require controlled bench
  observation; and
- simultaneous intended output paths, repeated hot-plug/LED restoration,
  fault recovery, and the combined two-hour workload remain pending.

The Focus Spot Two manual identifies its UV source as Risk Group 3. V27 keeps
both real Focus UV shutters closed and both UV dimmers at zero in every released
Autoloop, Priority Look, performance Scene (including `UV`), override, and
movement frame. The `UV` performance look uses the Wash/tubes plus low visible
Focus main colors. Real Focus UV is outside the released V27 programming and is
not part of qualification.

## Historical V27 qualification route (apply the V31 delta checks too)

1. Run the V27 builder, independent workspace validator, and completed package
   validator while proving the protected V26 SHA remains unchanged.
2. Install the custom Focus fixture definition and candidate plug-ins into an
   isolated copy of the exact pinned QLC+ `5.3.0 GIT a124abe` Windows tuple.
3. Open V27 with physical outputs disabled and complete the Engine Monitor,
   patch, mode, input, Priority-isolation, performance, override, Autoloop, and
   movement review.
4. Follow
   [`FULL_RIG_PATCH_AND_BENCH.md`](../../releases/qlcplus-control-one/v27/FULL_RIG_PATCH_AND_BENCH.md)
   with one named output path and one fixture class at a time. Test mover motion
   with shutters closed and dimmers at zero before opening visible light.
5. Run every raw Autoloop for one complete cycle, every Priority Look, all
   performance Scenes and overrides, Groups 1–4, Global, Blackout, Stop, and
   release behavior.
6. Only after the controlled bench passes, complete repeated hot-plug, required
   simultaneous-port, fault-recovery, and combined two-hour workload gates.
7. Promote V27 only when recorded evidence supports the exact claim. Otherwise,
   restore the backed-up plug-ins and open the preserved V26 workspace.

## Shared engineering priorities

1. Complete the exact pinned-host plug-in load test.
2. Complete the controlled Wash and Focus bench with installed A/B identity and
   safe-position notes.
3. Observe the complete full-rig performance, override, Autoloop, Priority,
   movement, and intensity routes.
4. Complete repeated Control One hot-plug and LED restoration qualification.
5. Qualify every intended output path, including simultaneous paths only if
   they are part of the actual show configuration.
6. Complete the combined two-hour DJ/audio/OS2L/MIDI/LED/DMX soak before any
   gig-qualified claim.
7. Re-check whether a later official QLC+ release incorporates equivalent OS2L
   timing before carrying the focused patch forward.

## Scope guardrails

- One runtime lighting application: QLC+.
- No second lighting runtime, bridge daemon, replacement firmware, or custom
  lighting engine.
- Keep custom code only where QLC+ cannot cleanly express the hardware or
  workflow.
- Preserve public Function IDs, logical channels, and the immutable V26 source.
- Keep Universe 3 private and disconnected from physical output.
- Treat structural validation, software tests, physical output, combined soak,
  and gig qualification as separate claims.
