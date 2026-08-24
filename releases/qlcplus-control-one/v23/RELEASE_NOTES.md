# QLC+ SoundSwitch V23 — Live Console Release Notes

V23 is a UI/control correction and presentation release built deterministically from V22 Unified Pro.

## Corrections

### Active Autoloop feedback

V22 placed 128 raw-Chaser monitor Buttons on only 32 identical geometries. Inactive monitor widgets from later banks could paint over the active raw Chaser, making Auto Bank/Auto All feedback unreliable.

V23 keeps the same 128 read-only monitors outside the playback-owner SoloFrame, but gives every physical pad a four-position live rail—one non-overlapping indicator for each bank. The active raw Chaser therefore remains visible during manual playback, sequential/random Auto Bank, sequential/random Auto All, and pad seek. The native ten-page CueList tracker is enlarged and remains the authoritative full-name/current-step view.

### Autoloop/Priority Looks mouse switch

V22 had two page-specific Buttons bound to the same public Function (`1993`) and logical channel (`811`). Both observed the same Function feedback, allowing one mouse action to be interpreted more than once.

V23 removes only the duplicate widget. One persistent top-level Button remains visible in both modes and retains Function `1993` plus channel `811`.

## UI refresh

- Live page expanded to 1600×900 for the DJ PC's 1920×1080 display.
- Dark performance canvas and consistent typographic hierarchy.
- Large, evenly spaced transport targets with a separate RUN/PAUSE/READY indicator.
- Clear Bank, Auto Bank/All, order, dwell, and mode grouping.
- Control One-aligned 4×8 pad layout with bank-aware live rails.
- Enlarged native current-loop tracker.
- Cleaner color override, speed, state, and intensity rail.
- Existing manual/map pages receive the same dark visual foundation without moving their established mappings.

## Preserved contracts

- 2,090 V22 lighting Functions: byte-equivalent after XML parsing.
- Four IR-4 fixtures at 1/11/21/31 in 10-channel mode.
- Four BO-TUBE192 fixtures at 175/215/255/295 in 40-channel mode.
- Eight matching private Priority Layer fixtures on Universe 3.
- Input/Output map, public Function IDs, logical channels, Priority Look authority, Autoplay owners/parents, dwell, speed, seek, OS2L, and fixture programming.
- Exact V21/V22 `soundswitch.dll` binary and QLC+ compatibility tuple.

No bridge, daemon, QLC+ core fork, firmware, or replacement lighting engine was added.

## Validation status

V23 passes local package, XML, reference, fixture, ownership, monitor, mode-switch, layout, script, hash, and personal-data checks. It is structurally validated and inherits the software-tested V21/V22 plug-in runtime. The UI correction requires the short owner observation in the package README before V23 is treated as the local show baseline.
