# QLC+ SoundSwitch V24 — Runtime Feedback Release Notes

V24 repairs the shared Virtual Console/Control One feedback route and makes current Autoloop state visible without changing creative programming.

## Root cause and correction

V23 declared both Surface Feedback and Priority Look feedback on QLC+ Universe 2. QLC+ supports one feedback destination per universe, so the later declaration silently replaced the first. Mouse Bank, mode, dwell, transport, order, and speed commands could therefore miss the plug-in.

V24 keeps one Surface Feedback patch. The plug-in now consumes Priority Look ownership channels `600–631` on that same line while the separate private Priority output continues to buffer the full overlay DMX frame.

Empty command Scenes emit a positive start followed by zero. UI commands `800–816` now act only on the positive edge, preventing mode, bank, dwell, transport, order, and speed actions from dispatching twice.

The hardware-independent Surface line stays open while Control One is unplugged. The periodic MIDI/LED reconnect remains independent, so mouse operation does not depend on a live hardware feedback handle.

## Runtime indicators

V23's large disabled monitor frame sat behind opaque bank frames. V24 replaces it with 32 visible read-only frames, one per physical pad. Each frame contains four non-overlapping raw-Chaser monitors for Banks 1–4.

The owner model remains native QLC+:

- manual repeat-one and all Autoplay variants stay in one SoloFrame;
- Start Bank and Start All remain latched owners;
- the ten native CueLists remain the full-name/current-step tracker; and
- the live indicators observe raw Chasers without starting or stopping anything.

## Preserved contracts

- All 2,090 V23 lighting Functions are unchanged.
- Four IR-4 fixtures remain at 1/11/21/31 in 10-channel mode.
- Four BO-TUBE192 fixtures remain at 175/215/255/295 in 40-channel mode.
- Eight private Priority Layer fixtures remain on Universe 3.
- All public Function IDs, logical channels, Autoloops, Priority Looks, dwell, speed, seek, OS2L, and fixture programming are preserved.
- QLC+ remains the only lighting application; no bridge, daemon, firmware, or custom engine was added.

## Software validation

- Protocol tests passed.
- Plug-in ABI and hardware-independent Surface/Priority smoke tests passed.
- Smoke coverage includes both mode directions, command-edge gating, Bank selection, and 2× speed selection.
- Package validation covers hashes, XML/references/IDs, fixture patch, one unified feedback line, all 128 speed-controlled raw loops, all ten dwell-controlled parents, 32 live frames, and 128 unique monitors.
- An isolated QLC+ runtime test—with no OS2L or physical DMX route—confirmed Bank selection, two-way mode switching, speed selection, latched Start Bank/All, parent advancement, and the live indicator following the active raw Chaser.

Physical fixture confirmation and the existing soak/fault gates remain separate from this software release.
