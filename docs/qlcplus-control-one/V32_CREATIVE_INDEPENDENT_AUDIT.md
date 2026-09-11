# V32 independent creative audit

Date: 2026-09-11. Reviewed repository commit:
`1f37441f2971af4057a68b7c5a8a7518516ef545`.

Reviewed workspace:
`IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw`, SHA-256
`16c676c531aa9aefa354eb2052e0f643be3c27d9e5fa71953deebf99257478c0`.

This is an independent source/XML audit of all 128 Autoloops and 32 Priority
Looks. No product file was changed. No Windows host, Control One, DMX output,
physical fixture, venue, or visual quality test was performed. These findings
do not establish SoundSwitch feature parity or gig qualification. They apply
to V32; an independently supplied V33 must be checked separately.

## Confirmed strengths

`Test-V32Workspace.py --self-test` passed in this audit, including two
deterministic rebuilds and eight rejected corruption cases. The resulting
workspace has 2,048 complete raw Scene frames, 102 complete Priority frames,
and all eleven fixture instances in every appropriate frame. Every fixture
has some nonzero intended light output somewhere in every raw loop. The 128
full scores are byte-distinct and have at least four distinct frames each.

Timing stays native to QLC+: Medium is four measures, Colorful two, Slow Dance
eight, and Flashy one or two at 1x. The complete common duration/fade and speed
preset checks passed. Slow Dance retains a lit room bed and two-beat fades.
Flashy has zero fades for hard intensity cuts. The small integer rounding at
4x is not a material musical issue.

Within each loop, Focus wheel, gobo, prism and focus values remain fixed;
authored adjacent position targets, including the seam, satisfy the existing
600-pan/250-tilt-unit bounds. Real Focus UV and internal-show/reset channels
remain disabled. There are 22 genuinely still Priority Scenes and ten animated
Priority Chasers; all ten retain eight leaves and eight-measure phrases.

## Findings and recommended corrections

### P1: Discrete Focus parameters can fade through unintended values

The Focus fixture nodes 9/10, 109/110 and 209/210 contain no `ExcludeFade`
entry. In the exact pinned QLC+ engine, `Fixture::channelCanFade()` returns
true unless a channel is explicitly excluded. `FadeChannel::autoDetect()`
uses that result, and `Scene::processValue()` applies the running fade time
to each fadeable channel. The common Chaser fade is passed to its Scene.

Therefore, holding a wheel or optic constant *within* a loop does not prevent
intermediate values during a first step or change to another loop. A color
transition from 0 to 124 can pass through the intervening wheel colors. A
gobo-rotation transition from 0 to the intended slow clockwise value 185 can
pass through 128, which this fixture definition labels fast clockwise. The
source-level transition path is established; its visible severity has not
been observed. The existing validator tests within-loop stability and does
not test this handoff behavior.

Recommended bounded correction for a successor workspace: explicitly exclude
discrete channels from fading on both physical and private Focus fixtures.
Review zero-based channels `4,5,6,7,8,10,13,14,15,16,17` (color, gobo,
rotation-mode, prism, shutters, show/mode/speed/function). Keep pan/tilt,
their fine bytes, dimmers and optical focus fadeable. Preserve the V32 file
and validate the intentional fixture metadata delta. This prevents numerical
sweeps through modes; it does not prevent a physical wheel from visibly moving
between slots. Do not enable untested fixture blackout/reset macros as a
shortcut.

Pinned implementation evidence:

- [Fixture fade exclusions](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/fixture.cpp), `channelCanFade()`.
- [FadeChannel channel classification](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/fadechannel.cpp), `autoDetect()`.
- [Scene fade application](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/scene.cpp), `processValue()`.
- [Chaser fade inheritance](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/chaserrunner.cpp), `stepFadeIn()` and `startNewStep()`.

### P2: Several chase hits never reach the final spatial endpoint at full strength

`V32CreativeProgram.py` uses `travel = (phase * 2) % 1`, then blanks odd
steps in Flashy hit/comet patterns. Lit travel positions are therefore
0, 0.25, 0.5 and 0.75 before restarting. The x=1 endpoint receives only
the Gaussian tail. The exact XML gives these peak dimmer values:

| Flashy pad / Function | Four IR-4 peak dimmers | Focus A/B peak dimmers |
|---|---|---|
| 9 / 636, Red Fixture Hits | 230, 187, 187, 36 | 175, 28 |
| 10 / 637, Blue Fixture Hits | 36, 187, 187, 230 | 28, 175 |
| 11 / 638, Rainbow Fixture Hits | 230, 187, 187, 36 | 175, 28 |
| 12 / 639, Center White Punch | 36, 187, 187, 36 | 28, 28 |
| 23 / 650, Comet Drop | 230, 204, 204, 78 | 175, 59 |
| 24 / 651, Reverse Comet Drop | 78, 204, 204, 230 | 59, 175 |

This is a substantial programmed imbalance, not evidence of hardware failure.
The directional pair reverses which side is weak. For an intentional complete
fixture sweep, derive travel from the lit-step index and include both 0 and 1,
or author explicit endpoint hits. Keep the existing common step timing and
off beats. Check physical orientation before deciding that A is left or right.

### P2: Some named multicolor scores omit declared palette colors

For many textures, `color_phase = t // 8` and spatial color offset is only
0 or 1. This selects at most palette indices 0, 1 and 2. The tube tip adds up
to a 16% blend of the final palette entry; this does not introduce omitted
middle entries or provide a full final-color event.

Concrete examples from the authored score and all sixteen generated frames:

- Colorful pads 19/20, Functions 582/583, declare red/orange/gold/green/cyan/blue.
  The main color selection uses only red/orange/gold; green and cyan never
  participate, while blue appears only in the tube accent blend.
- Colorful pads 21/22, Functions 584/585, and Flashy pad 11, Function 638,
  declare red/gold/green/cyan/blue/magenta. Their main selection uses only
  red/gold/green; cyan and blue do not participate, while magenta is only the
  tube accent blend.
- Flashy pad 28, Function 655, White Color White, declares white/pink/white/blue.
  Its main selection never reaches blue.

Recommended correction: give the affected ladder/comet/hit/alternate scores
an explicit color-phase progression covering the intended palette while
keeping the Focus mechanical wheels fixed per phrase. Validate coverage of
intended palette roles, not just whole-frame uniqueness. Do not recolor the
entire library immediately before the show without operator review.

### P2: Two timing labels do not match the programmed pulse rate

| Flashy pad / Function | Actual 1x program | Consequence |
|---|---|---|
| 1 / 628, White Beat Flash | 0.25-beat steps, alternating on/off | Two flashes per beat, not one |
| 26 / 653, Half-Beat Rainbow | 0.5-beat steps, two on/two off | One beat lit, one beat dark |

The patterns are valid musical rhythms, but the labels can mislead the
operator. Decide the intended rhythm and either correct the mask or the
operator-facing label in a bounded follow-up. Eighth-Note Rainbow, Function
652, does correctly produce two hits per beat. 2-Beat Color Slam, Function
654, correctly stays lit for two beats and dark for two.

### P2: Focus calibration remains necessary for the claimed visual quality

All non-soft scores use optical focus 0; soft scores use 96. Those values
are not calibrated to the venue projection distance. The wheel has discrete
colors, so mappings such as purple to blue or amber to light yellow are
approximations, not color matching against the LED fixtures.

Observed programmed Focus main-dimmer ranges are 40–100 for Medium, 50–125
for Colorful, 43–62 for Slow Dance, and 0–175 for Flashy. Priority uses
61/68 for A/B. These are intentional reduced levels, not full-output looks;
there is no photometric evidence yet that the resulting balance works.

All 32 Flashy loops hold the same Focus center positions. Other motion uses
a small orbit/fan or drift around fixed, uncalibrated coordinates. Adjacent
target bounds do not guarantee smooth takeover from another position, especially
when enabling MOVE, changing banks, releasing a position latch, or releasing
a Priority Look. The handoff and actual mounting orientation need observation.

Recommended action tonight: qualify aim and focus with the actual fixtures,
then select a short approved set. Keep movement and beam intensity under
operator control until those observations are recorded. Do not represent
numeric frame checks as professional visual approval.

## Compact physical review for the actual test package

First record the installed package/version and matching DLL/workspace identity;
this audit is not automatically evidence for another version. With the
correct fixture modes/addresses and the normal show output selected:

1. Review Slow Dance pads 1 and 20 and Priority Warm/Candlelight. Check smooth
   persistent room light, pleasing white balance, Focus aim and gobo focus.
2. Compare Medium pad 1, then pad 19 (gobo rotation), then Colorful pad 4.
   Watch both beams during the *transition*, not just after settling. Repeat
   at 0.25x and 1x to expose unintended wheel/rotation ramps.
3. Compare Flashy pads 9 and 10, then 12 and 13. Observe whether every intended
   fixture receives a convincing hit, particularly the two outer fixtures
   and Focus pair. The endpoint imbalance above is the predicted V32 result.
4. Review Colorful pads 19–22 and Flashy pad 11 for actual full-palette travel.
   Verify Flashy pads 1, 25 and 26 against a clear 120 BPM track and select
   the rhythm that matches the intended music.
5. While an approved loop runs, engage/release a Priority Look, a latched
   color and a Shift-held color, then MOVE and a position latch. Confirm the
   correct underlying look resumes and movement does not jump unexpectedly.
   Check STROBE keeps color/aim and its release restores the active look.

This is a targeted creative review, not the complete fault/soak qualification
route. Any observed failure should identify package, bank, pad, modifier and
fixture. Correct the bounded cause and recheck that route; preserve the
known-working file. Only the physically approved scores should form the
tomorrow-night working set.
