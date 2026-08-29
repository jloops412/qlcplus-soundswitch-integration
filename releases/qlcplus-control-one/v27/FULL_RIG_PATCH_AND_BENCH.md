# V27 full-rig patch and visual/physical bench

This is the authoritative V27 patch and first-hardware-session route. It separates structural evidence, no-output visual inspection, controlled fixture testing, and gig qualification.

Do not interpret a successful workspace validator or QLC+ Engine Monitor preview as proof that a real fixture is in the correct mode, is physically oriented as expected, or is safe to illuminate.

## Address plan

QLC+ XML addresses are zero-based. Fixture displays and this operator route are one-based.

| Physical ID | Private ID | Fixture | Mode | U1 XML address | U1 display span | U3 private span |
|---:|---:|---|---|---:|---:|---:|
| 0 | 100 | IR-4 1 | `10 Channel` | 0 | 001–010 | 001–010 |
| 1 | 101 | IR-4 2 | `10 Channel` | 10 | 011–020 | 011–020 |
| 2 | 102 | IR-4 3 | `10 Channel` | 20 | 021–030 | 021–030 |
| 3 | 103 | IR-4 4 | `10 Channel` | 30 | 031–040 | 031–040 |
| 4 | 104 | Wash FX Hex | `40 Channel` | 40 | **041–080** | **041–080** |
| 9 | 109 | Focus Spot Two A | `18 Channel` | 80 | **081–098** | **081–098** |
| 10 | 110 | Focus Spot Two B | `18 Channel` | 98 | **099–116** | **099–116** |
| — | — | Reserved | — | 116 | **117–174 free** | **117–174 free** |
| 5 | 105 | BO-TUBE192 1 | `40 Channel` | 174 | 175–214 | 175–214 |
| 6 | 106 | BO-TUBE192 2 | `40 Channel` | 214 | 215–254 | 215–254 |
| 7 | 107 | BO-TUBE192 3 | `40 Channel` | 254 | 255–294 | 255–294 |
| 8 | 108 | BO-TUBE192 4 | `40 Channel` | 294 | 295–334 | 295–334 |

The V26 addresses are immutable. Nothing may be inserted by shifting an IR-4 or tube. Both output layers remain 334 channels.

Universe 3 exists only as the private Priority buffer on `soundswitch:priority-layer`, line 4. It must have no physical DMX, Art-Net, or sACN route.

## Fixture personality checks

Before plugging in DMX:

1. Set the Wash FX Hex to manufacturer personality `40 Channel` and address 041.
2. Set Focus A to `18 Channel` and address 081.
3. Set Focus B to `18 Channel` and address 099.
4. Confirm the four IR-4s remain in `10 Channel` mode at 001/011/021/031.
5. Confirm the four BO-TUBE192s remain in `40 Channel` mode at 175/215/255/295.
6. Record which physical Focus is cabled as A/081 and B/099. Do not infer left/right from the source project.
7. Confirm every fixture has safe rigging, power, termination, ventilation, and clearance according to its manufacturer documentation.

If the fixture menu abbreviates the personality name, use the channel count and manufacturer manual to confirm the selection. Do not guess from a similar mode.

## Channel boundaries

### Wash FX Hex, 40-channel mode

| Fixture channel | U1 display channel | Purpose | Bench rule |
|---:|---:|---|---|
| 1 | 041 | Auto programs | Keep 0 during direct-color bench |
| 2 | 042 | Auto speed/sound | Keep 0 |
| 3 | 043 | Auto/sound-program dimmer | Not a master for direct zones; keep 0 |
| 4 | 044 | Strobe | Keep 0 until a named strobe test |
| 5–40 | 045–080 | Six RGBAWUV direct zones | Test one zone/emitter at a time |

Group 2 scales only display channels 045–080. It must not scale or accidentally activate channels 041–044.

### Focus Spot Two, 18-channel mode

| Fixture channel | Purpose | Safe starting state |
|---:|---|---:|
| 1–4 | Pan, pan fine, tilt, tilt fine | Calibrated position; clear motion envelope |
| 5 | Color wheel | 0, white/open segment |
| 6 | Gobo wheel | 0, open |
| 7 | Gobo rotation/index | 0 |
| 8 | Prism | 0, off |
| 9 | Main shutter/strobe | 0, closed |
| 10 | Main dimmer | 0 |
| 11 | UV shutter/strobe | 0, closed |
| 12 | UV dimmer | 0 |
| 13 | Motor focus | Stable bench starting point; calibrate physically |
| 14 | Internal show | 0, off |
| 15 | Internal show speed | 0 |
| 16 | Dimmer mode | 0, standard |
| 17 | Pan/tilt speed | Intentional per look |
| 18 | Function/reset/sound | 0; never sweep casually |

Group 4 scales only fixture channels 10 and 12 on both Focus units: U1 display channels 090, 092, 108, and 110. It must never scale or rewrite motion, shutters, optics, internal shows, or function/reset.

## Creative closure ledger

| Path | Roots | Live Scene leaves | Required fixture closure |
|---|---:|---:|---|
| Raw Autoloops | 128 Chasers | 1,024 | Physical IDs 0–10; complete 276-pair frame each |
| Priority Looks | 32 roots | 102 | Private IDs 100–110; complete 276-pair frame each |
| Performance | 5 Scenes | 5 | Existing rig plus Wash and Focus pair |
| Color overrides | 9 Scenes | 9 | Sparse color-only coverage across the full rig |
| Total | — | **1,140** | Every live creative leaf classified and validated |

A 276-pair complete frame is:

```text
4 IR-4 × 10 + (1 Wash + 4 tubes) × 40 + 2 Focus × 18 = 276
```

V27 has 2,100 Functions: 1,812 Scenes, 150 Chasers, and 138 Collections. The only new Function IDs are 2175–2184. Seventeen UI-control Scenes remain empty by design. Historical unreachable helper Scenes remain unreachable; they are not counted as live creative coverage and must not be revived by accidental references.

## Exact decoded Focus A/B positions

The source stores each coordinate as a 16-bit value duplicated inside a 32-bit field. QLC+ receives the high byte as coarse and low byte as fine:

```text
coarse = raw16 >> 8
fine   = raw16 & 255
```

`0x7fffffff` in the source means unset. None of the nine positions below uses that sentinel.

| Function | Position | A pan raw `[C/F]` | A tilt raw `[C/F]` | B pan raw `[C/F]` | B tilt raw `[C/F]` |
|---:|---|---:|---:|---:|---:|
| 2175 | Crossed Out Down | 46027 `[179/203]` | 8286 `[32/94]` | 40693 `[158/245]` | 6629 `[25/229]` |
| 2176 | Crossed In Down | 50752 `[198/64]` | 6930 `[27/18]` | 53342 `[208/94]` | 3917 `[15/77]` |
| 2177 | Stage Right | 35663 `[139/79]` | 7231 `[28/63]` | 33682 `[131/146]` | 3917 `[15/77]` |
| 2178 | Stage Left | 20727 `[80/247]` | 65384 `[255/104]` | 23013 `[89/229]` | 63878 `[249/134]` |
| 2179 | Straight Ahead | 39473 `[154/49]` | 2260 `[8/212]` | 47094 `[183/246]` | 1055 `[4/31]` |
| 2180 | Crossed Out Up | 40693 `[158/245]` | 2561 `[10/1]` | 45875 `[179/51]` | 1356 `[5/76]` |
| 2181 | Crossed In Up | 39626 `[154/202]` | 3314 `[12/242]` | 47094 `[183/246]` | 1356 `[5/76]` |
| 2182 | Up | 45417 `[177/105]` | 15066 `[58/218]` | 40997 `[160/37]` | 13107 `[51/51]` |
| 2183 | Down | 50142 `[195/222]` | 8587 `[33/139]` | 49532 `[193/124]` | 6629 `[25/229]` |

The source names the two movement children A and B; it does not prove any physical left/right relationship. V27 maps source A to fixture ID 9/address 081 and source B to fixture ID 10/address 099. Record the installed A/B placement during the bench and do not reinterpret the decoded coordinates by position.

`Disco Ball` is the source collection header that contains the position records. It is not a tenth preset. The ninth preset is `Down`; the eighth is `Up`.

## No-output visual bench

Complete this pass with all physical outputs disabled:

1. Open `IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw`.
2. Confirm QLC+ resolves `American DJ / Focus Spot Two / 18 Channel`. Stop if it reports a missing definition or substitutes a generic fixture.
3. Inspect the fixture list and compare every ID, universe, address, mode, and channel count to the address table.
4. Confirm Universe 1 retains the selected SoundSwitch physical output path and Universe 3 has only the internal priority output on line 4.
5. Open the Engine Monitor. It must contain physical IDs 0–10 exactly once, with unique useful positions, and no IDs 100–110.
6. Trigger BLACKOUT, WHITE, UV, ANNOUNCEMENT, FULL COLOR, all nine color overrides, a loop from each bank, and a still and moving Priority Look. Confirm the Monitor shows all intended fixture classes. In `UV`, confirm both Focus UV shutters/dimmers remain zero; only their visible main sources may illuminate.
7. Trigger position Functions 2175–2183 individually, then Function 2184. Confirm A and B move independently in the Monitor and the sequence order matches the table.
8. Confirm the existing MOVE widget starts Function 2184 and shifted position inputs 164–172 select exactly one position at a time.
9. Confirm Global and Groups 1–4 remain independent. Group 2 must affect only Wash direct emitters; Group 4 must affect only Focus main/UV intensity.

The Monitor is a logic and coverage aid. It cannot prove real beam aim, fixture menu settings, mechanical clearance, or UV safety.

## Controlled physical bench

### 1. Prepare a safe state

- Close SoundSwitch so QLC+ is the only process attempting to own the device.
- Disconnect fog, sparks, lasers, pyrotechnics, and every unrelated hazardous load.
- Clear the full Focus pan/tilt envelopes and install safety cables.
- Exclude people from the beam and UV exposure areas.
- Start with global Blackout active, both Focus shutters closed, all dimmers at zero, and Wash program/strobe controls at zero.
- Enable only one named output path: Micro, Control One DMX 1, or Control One DMX 2.
- Keep physical access to power removal and an immediate QLC+ Stop/Blackout action.

### 2. Prove address ownership one fixture at a time

For each physical fixture, bring up one low-risk visible channel at low intensity and confirm only the intended unit responds. Return it to blackout before moving to the next fixture.

An unexpected second response means an address/mode conflict. Stop immediately and correct the fixture menu or cabling; do not compensate by editing a released workspace live.

### 3. Map the Wash zones

Keep Wash fixture channels 1–4 at zero. At low intensity, activate one direct emitter on one zone at a time across fixture channels 5–40. Record the real left/right or clockwise zone order. The manufacturer channel order does not by itself prove the installed physical orientation.

Then test Group 2 from full to zero. Color values should remain selected while brightness scales; auto programs and strobe must remain inactive.

### 4. Prove Focus motion with light closed

Keep both main and UV shutters closed and both dimmers at zero. Trigger each position Scene 2175–2183 individually.

For every preset:

- confirm fixture A/address 081 and fixture B/address 099 match the intended physical units;
- confirm neither fixture strikes truss, cable, decor, ceiling, or another fixture;
- record whether the named aim lands on the intended stage/floor area; and
- invoke Stop/Blackout and remove power if motion becomes unsafe.

Only after all nine individual positions pass should Function 2184 run continuously. The exact decoded coordinates are authoritative source values, but installed aim is unresolved until this bench passes.

### 5. Prove visible Focus output

At a safe aim and low main dimmer:

1. Open the main shutter without enabling strobe.
2. Check white and each color-wheel family used by V27.
3. Check gobo open, programmed gobos, rotation, and prism.
4. Calibrate motor focus for the actual throw; V27's programmed focus value is a stable starting point, not an autofocus claim.
5. Test Group 4 and confirm it scales main intensity without altering position or optics.
6. Return main shutter and dimmer to zero.

Do not sweep fixture channel 18. Reset and sound-active ranges are deliberately excluded from show programming.

### 6. UV Risk Group 3 gate

The Focus UV source is Risk Group 3. Every released V27 frame—including the `UV` performance Scene—must leave both Focus UV shutters closed and both UV dimmers at zero. The V27 workspace intentionally provides no real Focus UV show look.

Leave Focus UV untested and disabled during V27 qualification. Any later direct-channel UV experiment is outside the released show programming and requires a qualified operator following the manufacturer instructions with controlled access, aim, distance, and duration. Do not use an audience as the test target.

Verify structurally and in the Monitor that both UV shutters and both UV dimmers remain zero through Autoloops, Priority Looks, performance Scenes, overrides, Stop All, and rollback.

### 7. Exercise the complete live show

After every fixture class passes independently:

1. Run all five performance Scenes and verify intentional full-rig blackout/white/UV/announcement/full-color behavior.
2. Run all nine color overrides over moving Autoloops; color should change while movement and intensity continue.
3. Run every raw Autoloop for at least one complete eight-step cycle. Confirm all eleven fixtures participate and no mover hits an unsafe aim.
4. Run all 32 Priority Looks. For each moving Look, observe a complete eight-step cycle. Confirm sole full-frame authority and seamless return to the still-advancing base loop.
5. Exercise manual playback, Auto Bank, Auto All, sequential/random order, all dwell values, and the independent chase-speed multiplier.
6. Test Global and Groups 1–4 at full, intermediate, and zero levels during base and Priority output.
7. Confirm QLC+ Global Blackout, Stop All, loss of playback, and release paths produce the expected safe output.
8. Repeat the named test separately for every output path intended for service. One path's result is not evidence for another.

## Qualification gates

| Gate | Required evidence | Claim allowed after pass |
|---|---|---|
| Structural | Builder and independent validator pass exact packaged artifacts and hashes | Structurally validated |
| Fixture bench | Named device, port, fixture, mode, address, and observed response recorded | Physical-output-tested for only those named combinations |
| Workflow regression | Full performance/override/Autoloop/Priority/intensity/control route observed | Full-rig workflow observed; not yet gig-qualified |
| Fault and soak | Repeated hot-plug, LED recovery, simultaneous required ports, and combined two-hour workload | Combined-soak-qualified for the exact tuple |
| Show readiness | Rigging, aim, UV boundary, fault recovery, audio, OS2L, MIDI, LEDs, DMX, and operator workflow all pass | Gig-qualified |

Do not inherit V26 physical evidence across the changed workspace and V27 intensity plug-in. Record unresolved tests explicitly.

## Manual rollback

1. Stop playback, invoke global Blackout, disable outputs, and close QLC+.
2. Power down or disconnect the Wash and both Focus fixtures.
3. Restore the exact V26 `soundswitch.dll` and `os2l.dll` from the pre-V27 backup.
4. Restore the prior `%USERPROFILE%\QLC+\Fixtures\American-DJ-Focus-Spot-Two.qxf` from the V27 install backup. If no prior file existed, remove only the installed V27 user-fixture copy.
5. Open the preserved V26 workspace and matching Control One profile.
6. Confirm only the original IR-4 and tube rig is patched to physical Universe 1.
7. Confirm private Universe 3 remains internal-only.
8. Enable one selected output and complete the V26 owner check before returning to service.

The V26 addresses never changed, so rollback requires no IR-4 or tube repatch. Keep V27 and its Focus fixture definition as separate files; never rewrite the immutable V26 package.
