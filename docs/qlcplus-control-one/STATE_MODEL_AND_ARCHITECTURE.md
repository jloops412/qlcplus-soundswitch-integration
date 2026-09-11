# State Model and Architecture

## Authoritative state

QLC+ Function running state is authoritative. The plug-in retains only selection preferences needed to translate the Control One surface and restore feedback after reconnect.

Keep these state dimensions independent:

| Dimension | Values |
|---|---|
| Playback owner | none, manual repeat-one, Auto Bank, Auto All |
| Transport | stopped/ready, running, paused |
| Bank | 1, 2, 3, 4 |
| Order | sequential, random |
| Dwell | 1, 2, 4, 8, 16 measures |
| Chase speed | 0.25x, 0.5x, 1x, 2x, 4x |
| Pad role | Autoloops, Priority Looks |
| Full-frame overlay | none or one Priority Look |
| Parameter overlays | one color latch plus independent momentary color holds |
| Intensity target | Global, Groups 1–4, Scripted |

Dwell is selection timing. Chase speed is the internal rate of the selected loop. Pan and Tilt are reserved for movement overrides and must not be repurposed as dwell controls.

## Playback ownership that works

The 128 creative Autoloops are ordinary QLC+ Chasers. Each manual pad starts a one-child Collection that owns one raw Chaser. Ten additional Collection owners start the five Autoplay scopes—Banks 1–4 and All—under sequential or random policy.

All 138 manual/automatic Collection buttons live in one QLC+ SoloFrame. Raw Chasers do not have buttons in that outer exclusivity frame. This gives native latch, same-pad off, replacement, and Manual/Autoplay exclusion.

Autoplay parents are long-running native QLC+ Chasers. Five shared native SpeedDials change parent step duration for 1/2/4/8/16-measure dwell without restarting the active parent. Separate SpeedDials scale the raw Chasers for the 0.25x–4x chase multiplier.

During Autoplay, a pad emits an absolute seek value on logical channel 632. That moves the active 32-step or 128-step parent to the requested loop while preserving the parent owner and its dwell/order state.

V31 displays the active loop through 32 disabled full-width monitor strips below the pads. Each strip contains four read-only Buttons, one raw Chaser per bank, and remains outside the owner SoloFrame. V31 adds dedicated synthetic feedback channels 1100–1227 to these existing monitors. The input translator never emits these observation channels as playback commands; the disabled strip parents and direct raw Function targets remain unchanged. QLC+ applies its native amber Monitoring state whether the raw Chaser was started manually or by an Autoplay parent, without introducing another playback owner or recreating the one-second flash failure.

The ten existing native Cue Lists remain bound to Autoplay parents `788–797` and retain absolute seek on logical channel `632`, but their helper frame is clipped to `1×1`. There is no visible/polling tracker: the raw Function monitors are the authoritative moving readout. No QLC+ core change or new logical channel is required.

The dwell SpeedDial remains the source of truth for parent step duration. Presets `4000/8000/16000/32000/64000` are beat-counted durations, not wall-clock milliseconds in this context: 4/8/16/32/64 beats correspond to 1/2/4/8/16 four-beat measures. QLC+ Multipage children must retain one five-button copy on each dwell page; removing those copies makes the values disappear on pages 2–5.

### QLC+ allows one feedback destination per universe

V23 declared both Surface Feedback and Priority feedback on Universe 2. QLC+ retained only the later patch, so mouse channels `800–816` could miss the plug-in. V24 leaves one Surface patch. The plug-in consumes Priority Look ownership channels `600–631` on that same route while the separate private Priority output still buffers overlay DMX.

Empty command Scenes start and stop immediately, producing a positive value followed by zero. Bank, dwell, transport, order, mode, and speed commands act only on the positive edge. Processing both edges causes exactly the double-toggle behavior V24 removes.

## Failure modes discovered

### Hidden handoff Scene caused the one-second flash

A Collection initially launched both a raw Chaser and a hidden handoff Scene. Because the handoff Scene also participated in the same SoloFrame ownership tree, QLC+ correctly treated it as a competing Function and stopped the Collection that launched it. The visible result was a loop that flashed for about one second and stopped.

The fix was to remove hidden handoff Scenes from playback ownership and use one-child Collection wrappers only.

### Banks 2–4 failed because input pages were encoded twice

Nested frame pages already provide their bank offset. Child pad inputs must stay on the low logical range 0–31. Encoding the bank again in each child input meant later pages triggered only part of the expected command path. Normalizing every bank page to child inputs 0–31 restored all four banks.

### Full-frame priority cannot be modeled as a normal sparse overlay

QLC+ HTP/LTP merging is suitable for sparse parameter overrides, but a SoundSwitch-style static/moving Look must be sole output authority. V20 duplicates the rig on private Universe 3. Priority Scene/Chaser output is buffered by the plug-in and replaces the complete physical Universe 1 frame only while a Priority Look feedback channel 600–631 is active. The base universe continues advancing.

### Widget IDs must exclude nested Function references

Virtual Console widgets have IDs, and child `<Function>` references also have an `ID` attribute—including the sentinel `4294967295`. Treating every `ID` in the Virtual Console as a widget ID caused allocator overflow and collisions. V20 allocates against actual widget element types only.

### UI gibberish was a runtime/build problem

The non-English-looking menu text was character corruption, not localization. The alpha installation had a mismatched/incomplete Windows runtime dependency set. A complete official QLC+ installation fixed it. The durable rule is to keep the QLC+ executable, Qt libraries, plug-ins, and FFmpeg/runtime DLLs from one coherent build.

## Hardware/plugin boundary

The plug-in implements:

- SoundSwitch Micro (`VID 15E4`, `PID 0053`) WinUSB DMX output;
- Control One (`VID 15E4`, `PID 0054`) DMX ports 1 and 2;
- Control One WinMM MIDI input/output translation and LED feedback;
- periodic two-second rescan and hot-plug recovery;
- WinMM handle validation by device ID, close-callback invalidation, and one-shot LED-write reconnect/retry;
- logical-state/known-LED restoration after a replacement MIDI handle opens;
- stable advertised output lines within a plug-in instance;
- newest-frame-wins USB output behavior;
- private Priority Look frame selection.

The source also contains explicit rig-specific intensity spans for IR-4 masters, Wash zone emitters, BO-TUBE192 emitters, and Focus dimmers. Non-intensity channels are excluded. Move these spans into validated configuration before a general-purpose release.

## OS2L compatibility boundary

The pinned stock QLC+ OS2L plug-in ignores the `bpm` value in a beat message and emits a beat for each message received. Since QLC+ calculates displayed tempo from those emitted intervals, queued/bursty delivery can turn a stable song into an impossible high reading.

V26 carries one focused, build-matched `os2l.dll` correction. It reads reported BPM, schedules precise native QLC+ beat events, treats subsequent packets as tempo/keepalive updates, and stops on disconnect/source silence. It is still a QLC+ plug-in—not a bridge or second runtime. The source delta is preserved under `qlcplus/patches/` for future side-by-side upgrades.

V26 keeps one persistent Virtual Console Button targeting Function `1993` and channel `811`. Its plug-in feedback handler is positive-edge gated and shares one Surface line with Priority ownership. The Surface output is hardware-independent; Control One MIDI/LED feedback may reconnect without taking mouse commands offline.

## Version boundary

The plug-in source branch began at QLC+ 5.2.2. V26's two DLLs are compiled against the exact DJ-PC core source commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`, identified by the UI as `5.3.0 GIT a124abe`. The installer verifies the unchanged stock `qlcplus5.exe` and both packaged plug-in hashes. No ABI promise is made across arbitrary QLC+/Qt builds: a future update must be installed side-by-side, rebuilt against its exact source commit, and qualified before production switches.

## V31 control and buffer corrections

Mouse pad, color, and Focus-position command Scenes route through the existing
unified Surface feedback and MIDI translation path. Hidden native controls own
the actual QLC Functions; the original Collection owner hierarchy remains
authoritative. A UI click and a controller press now take the same decision
path instead of bypassing Autoplay seek or latch state.

Transient held inputs retain their original output channel until release or
disconnect. QLC feedback updates latch and owner state before reconnect LED
restoration. Test-only WinMM fakes exercise the actual translation class; they
are excluded from the production DLL.

Private Priority buffers now own a deep copy of QLC's incoming frame. QLC can
send a non-owning view into a mutable universe buffer, so a shallow QByteArray
assignment was not a stable cross-thread snapshot. The plug-in mutex protects
state selection, and the owned copy protects its bytes.

Handoff clears the cached frame on owner changes. It does not correlate frame
generations across independently scheduled universes, so an in-flight older
private frame cannot be identified from the DMX bytes alone. Do not describe
this as complete stale-frame rejection. Observe look-to-look handoffs in the
exact host and rig before promotion.

The raw-Chaser observation channels update only minimal LED feedback state.
They never infer or change the playback owner. Bank buttons retain the browsed
bank; Autoloop pads show the actual current pad number, including Auto All
across banks. Priority mode retains its own selected-Look feedback.

Known transient release first selects the native Live page through existing
channel 510, then emits the matching release. QLC routes inputs only to the
selected top-level page, so this ordering prevents page navigation from leaving
a live hold behind. Releasing a transient after browsing another page returns
the operator to Live. Cross-page Focus-position clones observe the same native
Scene::flashing state, so position latch release remains shared.

## V32 native effect layer

V32 uses the formerly empty U4 for native QLC+ MOVE/STROBE programming. Focus
mirrors 209/210 and three HTP control bytes in fixture 211 emit to the
`soundswitch:effect-layer` virtual output. The effect values and ownership
markers arrive in one owned frame. The plug-in copies only pan/tilt/speed for
MOVE, multiplies designated intensity channels by the native STROBE gate,
then applies existing Global/group levels. It does not add a clock, scheduler
or Function owner. Position Flash Scenes add their exact values to the native
MOVE layer so they retain precedence. Removing the private route clears the
snapshot. U4 must never feed a physical/network output. Full implementation,
software evidence and physical limits are in `V32_CREATIVE_PASS.md`.
