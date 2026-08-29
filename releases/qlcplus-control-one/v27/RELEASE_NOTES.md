# V27 release notes — Full Rig

V27 is the additive full-rig successor to V26 Autoplay Clarity. It keeps QLC+ as the sole show engine and integrates one Chauvet Wash FX Hex and two American DJ Focus Spot Two movers throughout every live creative path.

> Status boundary: these are candidate release notes. V27 must not be described as physical-output-tested or gig-qualified until the exact packaged workspace, fixture modes, output device/port, and full rig complete the documented bench and soak gates.

## Added

- One Chauvet Wash FX Hex in `40 Channel` mode at Universe 1 address 041, QLC fixture ID 4.
- Two American DJ Focus Spot Two fixtures in `18 Channel` mode at addresses 081 and 099, QLC fixture IDs 9 and 10.
- Exact private Priority duplicates on QLC+ Universe 3: IDs 104, 109, and 110.
- A reviewed custom `American-DJ-Focus-Spot-Two.qxf`, required because the pinned QLC+ fixture library does not contain this model.
- Nine sparse Focus A/B position Scenes at Function IDs 2175–2183.
- Function 2184, `MOVEMENT — FOCUS A/B SWEEP`, assigned to the existing MOVE control.
- Exclusive shifted position controls on logical channels 164–172.
- All eleven physical fixtures in the QLC+ Engine Monitor/visual bench, with no private fixtures represented there.
- Wash and Focus operator controls and labels without changing established public Control One identities.

## Full creative integration

The V27 candidate contains 2,100 Functions: 1,812 Scenes, 150 Chasers, and 138 Collections.

Its live creative closure is exactly 1,140 Scene leaves:

- 1,024 raw Autoloop step Scenes across 128 eight-step Chasers;
- 102 Priority Look leaf Scenes;
- 5 performance Scenes; and
- 9 color overrides.

Every raw Autoloop Scene now contains the complete physical rig, including a full 40-channel Wash frame and two full 18-channel Focus frames. Every Priority leaf contains the exact corresponding private rig. Each of those full frames contains 276 channel/value pairs.

The Focus programming uses independent complementary movement, color-wheel selection, gobos, rotation, prism, shutter, intensity, and stable motor-focus values. Internal shows and reset/sound-active function ranges remain disabled.

The Wash programming translates the existing musical tube/IR-4 intent across six direct RGBAWUV zones. Direct zone emitters—not the auto-program dimmer—participate in Group 2 intensity.

## Existing-show improvements

- BLACKOUT, WHITE, UV, ANNOUNCEMENT, and FULL COLOR now include the tubes, Wash, and Focus pair instead of stopping at the four IR-4s.
- RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, PURPLE, PINK, and FULL COLOR overrides now include all tube emitters, all six Wash zones, and both Focus color wheels.
- Color overrides remain sparse by design so Autoloop movement and intensity continue underneath them.
- Strobe Chaser 808 inherits full-rig output through the enriched WHITE and BLACKOUT Scenes without changing its public Function ID or timing.
- The visual bench no longer omits the new fixtures or depends on overlapping tube positions.

## Position-source correction

The nine decoded source presets are, in order:

1. Crossed Out Down
2. Crossed In Down
3. Stage Right
4. Stage Left
5. Straight Ahead
6. Crossed Out Up
7. Crossed In Up
8. Up
9. Down

`Disco Ball` is the source collection/container header, not a tenth position preset. The decoded A and B 16-bit coordinates are exact. The source does not prove a physical left/right relationship, so V27 retains neutral A/B names; record which installed unit is A/address 081 and B/address 099 during the bench.

## Intensity routing

- Group 1: four IR-4 fixtures.
- Group 2: Wash direct zone emitters.
- Group 3: four BO-TUBE192 fixtures.
- Group 4: Focus main and UV dimmers.
- Global continues to multiply every group, including the active full-frame Priority layer.

Movement, optics, shutters, strobe, auto programs, internal shows, and reset/function channels are excluded from group scaling.

## Preserved from immutable V26

- Every existing physical and private fixture address and mode.
- The free display-address range 117–174.
- A 334-channel physical frame and 334-channel private Priority frame.
- All existing public Function IDs and logical channels.
- Raw Autoloop root IDs 532–659, their eight-step references, names, timings, and the reviewed Variety Pro donor steps.
- Manual owners 660–787, Autoplay parents 788–797, Autoplay owners 798–807, and Strobe 808.
- Bank, order, dwell, speed, seek, Priority ownership, mouse operation, OS2L timing, and Control One feedback behavior.
- The private Universe 3 isolation contract and stock QLC+ core.
- V26 and all earlier protected rollback directories and hashes.

The 17 empty UI-control Scenes remain empty by design. Unreferenced historical helper Scenes remain unreachable and are not revived as show content.

## Safety and unresolved physical boundaries

- Focus UV is Risk Group 3. V27 leaves both real Focus UV shutters closed and UV dimmers at zero in every released creative frame, including the `UV` performance Scene. That look uses visible Focus main colors instead.
- Focus A/B physical orientation, all nine installed aims, motor focus, pan/tilt clearance, gobo/prism appearance, and shutter behavior require a controlled physical bench.
- Wash head/zone physical orientation requires a one-zone-at-a-time bench check.
- Micro, Control One DMX 1, and Control One DMX 2 must be tested as named output paths; prior V26 evidence cannot be inherited as proof for the changed workspace and intensity plug-in.
- Repeated hot-plug, simultaneous ports, fault recovery, and the combined two-hour workload remain separate qualification gates.

## Current software evidence

CI run `33241230755` built candidate `soundswitch.dll` SHA-256 `19074A37AA915E1E39124CD14441025A2A83AB06EBDB14BD0953E2910B801DE3` from reviewed remote source commit `bf85057b5958608034decacae8927b0714ee98ed`. Protocol, intensity, and plug-in-load smoke tests passed.

That evidence explicitly records `pinnedQlcHostAbiTested=false`. It does not prove loading in the exact pinned Windows QLC+ host, physical DMX output, fixture behavior, or gig readiness.

## Upgrade and rollback

Install V27 as a new folder. Keep the custom Focus `.qxf` in the package, install a backed-up copy in the QLC+ user `Fixtures` folder, install the profile in the user `InputProfiles` folder, back up the existing build-matched plug-ins, and open the V27 workspace first with outputs disabled. Follow the manual install and rollback routes in [README.md](README.md) and the controlled fixture procedure in [FULL_RIG_PATCH_AND_BENCH.md](FULL_RIG_PATCH_AND_BENCH.md).

Rollback restores the backed-up V26 plug-ins, restores or removes the installed user Focus `.qxf` according to its install receipt, and opens the preserved V26 workspace. Existing IR-4 and tube addresses never move, so no legacy fixture must be repatched.
