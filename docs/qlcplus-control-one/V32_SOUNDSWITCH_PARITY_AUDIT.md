# V32 SoundSwitch parity audit

Date: 2026-09-11. Audited source: `1f37441f2971af4057a68b7c5a8a7518516ef545`.
This is an independent review of V32, not a review of the separately promised
V33. No V33 source or package was available to this reviewer. This record does
not establish physical output, professional visual quality, or gig qualification.

## Decision

V32 is suitable to distribute as an explicitly limited testing package. It is
not a verified full SoundSwitch replacement. Its deterministic checks and
Windows plug-in CI passed, but review of the actual pinned QLC+ engine exposes
control interactions those checks do not exercise. Read the independent control
and creative audits alongside this matrix. Existing broad claims about STOP,
color-only overrides and optic stability must be interpreted with those findings.

## Coverage and differences

| Capability | Available V32 behavior | Remaining difference or evidence needed |
|---|---|---|
| 128 Autoloops | Four banks of 32; native manual and automatic owners | Fixture observation and musical grading remain open |
| Automatic selection | Bank/All, sequential/fixed shuffled cycle, selected-pad seek | This is the project's explicit workflow; it is not proof of SoundSwitch's selection algorithm |
| Loop timing | Independent 0.25x–4x speed and 1/2/4/8/16-measure selection dwell; OS2L timing | SoundSwitch also exposes 8–128-bar loops, track repeat policies and script transitions; these are different controls |
| Priority Looks | 32 complete-rig still/moving looks; underlying show continues | SoundSwitch Static Looks may include only selected fixtures; excluded fixtures can retain their underlying show |
| Color latches and holds | Normal color press latches, Shift+color holds; all eleven fixtures addressed | Wash/tube color bytes also carry brightness, so their chase dimming is overwritten; see control audit |
| White/Black/UV | Separate hold/toggle widgets | Shared Scene state creates latch/hold interference; BLACK is not an unconditional final blackout |
| STOP | Native StopAll action | Does not clear Scene::Flash sources in the pinned core; do not treat as emergency blackout |
| MOVE/STROBE/positions | Native U4 parameter layer, nine position overrides | Arbitrary-pose takeover, precedence, release, aim and STOP need physical verification |
| Intensity | Global plus four groups | Scripted intensity is reserved, not implemented track-script playback |
| DJ integration | VirtualDJ OS2L is the timing source | No equivalent song autoscripting/imported script engine, deck show blending, scratch/upfader modes or audio-detection fallback is established |
| Control One | MIDI translation and known LEDs, mouse fallback | OLED deferred; not every vendor button/gesture is implemented; reconnect and exact host remain hardware tests |
| Show design | Redesigned 128 loops and 32 Looks, complete eleven-fixture frames | Data completeness is not professional lighting certification; discrete mover channel fading and repeated patterns require review |

The owner's selected gestures take precedence over copying vendor defaults.
SoundSwitch's published Control One quickstart uses Shift+color for position
overrides; this project deliberately provides Shift+performance pads 1–9 for
positions and Shift+color for momentary color holds.

## Prioritized next candidate acceptance

1. Prove a visible, persistent blackout that remains dark when any override is
   pressed. Verify STOP clears every latched and held effect, or clearly separate
   transport stop from the tested emergency blackout.
2. Latch WHITE/BLACK/UV, press and release the ordinary hold of the same effect,
   and confirm the chosen latch contract. Repeat after page changes and reconnect.
3. Preserve each Wash zone/tube cell's current brightness during a color-only
   override, including dark cells and operation over Priority Looks.
4. Make discrete Focus wheel/gobo/prism/function channels snap between valid
   values instead of fading through intervening ranges. Verify on the actual rig.
5. Test selected starts in Bank/All and sequential/shuffled modes with both mouse
   and Control One. From running pad 6, choose pad 3 and observe 3, 4, 5 in
   sequential mode. Test all five speed settings separately from dwell.
6. Verify all fixture classes, known event Looks, tempo changes, release back to
   the advancing show and controller reconnect with the exact delivered tuple.
   Run the intended combined DJ workload before relying on the candidate.

## Official reference sources

- [Autoloops explained](https://support.soundswitch.com/en/support/solutions/articles/69000847100-soundswitch-autoloops-explained)
- [Autoloop changes in SoundSwitch 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9)
- [Static Looks explained](https://support.soundswitch.com/en/support/solutions/articles/69000863339-soundswitch-static-looks-explained)
- [Control One quickstart](https://cdn.inmusicbrands.com/soundswitch/files/User%20Guide%20Control%20One.pdf)
- [Performance mode preferences](https://support.soundswitch.com/en/support/solutions/articles/69000847088-soundswitch-performance-mode-preferences-explained)
- [Loop Auto Strobe](https://support.soundswitch.com/en/support/solutions/articles/69000847399-soundswitch-utilizing-loop-auto-strobe)
- [Audio BPM detection](https://support.soundswitch.com/en/support/solutions/articles/69000858486-soundswitch-bpm-detection-overview)
- [Autoscript troubleshooting](https://support.soundswitch.com/en/support/solutions/articles/69000863270-soundswitch-performance-mode-autoscript-troubleshooting)
- [Official release notes](https://cdn.inmusicbrands.com/soundswitch/files/SoundSwitch%20Release%20Notes.pdf)

The public references do not establish a complete simultaneous-override
precedence table, exact random-selection algorithm or every quantization edge
case. Those behaviors require explicit project contracts and runtime evidence.
