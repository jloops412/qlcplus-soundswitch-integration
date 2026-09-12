# V34 Windows testing package

V34 is a reliability testing candidate built from the protected V32 creative
checkpoint. It preserves the physical patch, 128 Autoloops and 32 Priority
Looks while addressing the output and control defects found in the independent
V32 audits. Full SoundSwitch parity and gig qualification are not claimed.

**Packaging/evidence status: pending.** This document describes the prepared
V34 changes and packaging contract. It does not establish a downloadable build
or a passed hardware test. The release coordinator must update this paragraph
with the actual CI run, available validation records and download/checksum links
after verification. The ZIP hash belongs in its companion checksum file, outside
the archive's own payload.

## Changes to test

- Mouse/OS2L STOP and Control One Shift + Play/Pause share a release path. It
  returns to Live, releases color, performance and position Flash owners, then
  requests native StopAll. Late held-key releases must not restart an override.
- WHITE, BLACK and UV hold/latch controls use separate native Scene ownership.
  Releasing either form should preserve the other and its shared hardware LED.
- BLACK is a final intensity gate. Later WHITE, UV or color controls should not
  relight the rig while BLACK remains held or latched.
- Private color latch/hold layers preserve the active show's tube/Wash emitter
  brightness patterns while replacing hue. Dark chase cells should remain dark.
- Focus discrete channels receive native fade exclusions. Specific sweep,
  palette and pulse programming defects receive bounded creative corrections.
- Live-loop feedback uses independent feedback observers. Runtime disabling
  blocks feedback, while the pinned Frame XML loader ignores saved Disabled
  state; this does not establish that every older monitor was blocked on load.

The tiny status strips are now inert native Labels: the pinned QML loader
does not restore the saved frame-disabled flag, making monitor Buttons capable
of accidental raw Function starts. This deliberately removes their live borders;
the main controls remain and Control One LEDs receive native state feedback
through enabled offscreen observers.

The native **BLACKOUT** button is visible at top right. It is a separate latch:
STOP leaves BLACKOUT enabled until the operator clicks BLACKOUT again. Normal
Play/Pause remains unshifted. A successful STOP or blackout bench test does not
establish the full rehearsal or fault-recovery outcome.

## Installation and internal routes

Begin with the one-page `START_HERE.txt` for the quickest setup and five bench
checks. Use the package's `README.txt` for File Explorer installation, matched DLL
backup, four fixture definitions, the updated input profile and rollback.
This package requires an existing complete Windows x64 QLC+ installation:
`5.3.0 GIT a124abe`, source `a124abebe0b5ad6077727c561a5a0e1f3730810c`,
Qt `6.8.1`, MinGW `13.1.0`. It is not a complete host installer and must not be
mixed with an arbitrary QLC+/Qt build.

The supplied workspace deliberately leaves U1 physical output unassigned.
Connect the actual Micro or Control One USB before launching QLC+. In
Inputs/Outputs, select Universe 1, Output, SoundSwitch Hardware, then the exact
connected device/port by name. Leave U2 and U3-U6 routes intact. Save As a working
show to retain the real device UID. Keep the native Grand Master at its default
Reduce / Intensity setting; Limit / All Channels changes internal template data.

All eleven physical fixtures retain their U1 addresses: IR-4 at
001/011/021/031, Wash FX Hex at 041, Focus Spot Two at 081/099, and tubes at
175/215/255/295. U2 is input/feedback only. Keep these private outputs enabled:

| Universe | Private function | SoundSwitch output line |
|---|---|---|
| U3 | Priority frame | 4 |
| U4 | MOVE/STROBE and WHITE/UV/BLACK flags | 5 |
| U5 | Color latch frame | 6 |
| U6 | Color hold frame | 7 |

U2-U6 must never be routed to physical DMX, Art-Net or sACN. The separate color
universes contain fixture mirrors and an active marker. U4 carries the native
performance ownership flags used by the final output operations.

## Package production and evidence

After downloading the new matching Windows CI artifact, run:

```text
python3 qlcplus/workspace-tools/Build-V34TestingPackage.py --binary-dir PATH_TO_NEW_CI_ARTIFACT --evidence-dir PATH_TO_ACTUAL_V34_EVIDENCE
```

`--evidence-dir` is optional. The builder verifies the DLL hash, reported CI
checks and exact compatibility. It compares the current production plug-in
sources/build configuration/workflow, workspace and input profile against the
CI commit's Git objects. Later test, harness and documentation changes do not
automatically change the DLL's production-source identity.

The builder runs the V34 structural and creative checks, regenerates the show
in isolation and requires byte equality. It produces a ZIP with fixed timestamps,
a checksum file, a complete internal manifest and an optional standalone package
verifier. Existing V34 output is not silently overwritten. Protected V26-V32
packages remain untouched. Original CI evidence and available native evidence
retain their stated scope; missing evidence is pending and is never converted
into a passing result. Baseline V32 audits are labelled historical context.

The decisive remaining observations are the complete pinned Windows host,
physical controller/DMX behavior, actual fixture appearance/aim, rapid Priority
handoffs, VirtualDJ BPM/pause/seek/tempo changes, hot-plug recovery and the combined
audio/OS2L/show workload. `FIRST_TEST.txt` gives the operator the exact sequences.
