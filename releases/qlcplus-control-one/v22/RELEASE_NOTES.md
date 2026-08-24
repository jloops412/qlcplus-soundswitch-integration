# QLC+ SoundSwitch V22 — Unified Pro Creative Merge

V22 resolves the split between the V21 Control One/UI/reliability branch and the later `All Banks Variety Pro` creative branch.

## Merge result

- Host: V21 reliability/control workspace.
- Creative donor: All Banks Variety Pro, not its later autosave.
- Replaced raw Autoloops: 22 of 128.
- Imported supporting Scene steps: 176.
- Donor Function-ID collisions remapped: 17.
- Static/Priority Look roots changed by the donor: 0.
- Existing V21 host Functions intentionally changed: exactly the 22 raw Chasers. Their one-child manual owner Collections already carried the final names and remain byte-for-byte unchanged.
- Existing fixture patch, I/O map, Priority Look Functions, Autoplay parents, public IDs, logical channels, and plug-in binary: unchanged.

The donor autosave was excluded because it contains 32 additional save-state XML differences that are not part of the authored Variety Pro delta. The named Variety Pro workspace is the deterministic creative source.

## Updated Autoloops

The creative donor replaces these pads:

- Colorful: Tropical Run, Festival Six, Citrus Chase, Berry Chase, Ocean Chase, Fire Chase, Green Magenta Cross, Blue Gold Cross, Reverse Color Ladder, Teal Gold Sparkle, Mardi Gras, Miami Neon, and Electric Violet.
- Flashy: UV Punch, Gold Hit, Drop Chase, Red Fixture Hits, Blue Fixture Hits, Purple Gold Punch, CMY Quick Cut, Center-Out Drop, and Outside-In Drop.

Each replacement remains an eight-step native QLC+ Chaser and retains its public raw Function ID, so manual pads, Auto Bank, Auto All, seek, dwell, speed, OS2L, and Control One mappings continue to target the same contract.

## Active-loop visibility

V22 adds one read-only Virtual Console outline monitor for every raw Autoloop. The monitor layer is outside the playback-owner SoloFrame and sits behind the real pad surface. It cannot take ownership or stop Autoplay. Because it observes the actual raw Chaser state, the outline follows:

- a manually latched loop;
- sequential or random Auto Bank;
- sequential or random Auto All;
- pad seek while Autoplay stays active.

The existing native Now Playing Cue List remains the authoritative text readout for the current Bank/All parent and Function name.

## Compatibility

- QLC+ UI: `5.3.0 GIT a124abe`
- QLC+ source: `a124abebe0b5ad6077727c561a5a0e1f3730810c`
- Required `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`
- Plug-in SHA-256: `AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D`
- Platform: Windows x64 with the matching MinGW/Qt QLC+ build

The V22 plug-in is binary-identical to V21. This release does not add a bridge, daemon, firmware replacement, custom lighting engine, or QLC+ core modification.

## Validation completed

- Workspace and profile XML parsed.
- Function IDs and actual Virtual Console widget IDs are unique.
- All Function and fixture references resolve.
- The physical and private fixture patches match the documented addresses and modes.
- All 128 raw Autoloops, 128 manual owners, ten Autoplay controls, and ten parent Chasers exist.
- The 22 Variety Pro roots each resolve to eight imported Variety Pro Scene steps.
- The active-loop outline covers all 128 raw Chasers exactly once and remains outside the owner SoloFrame.
- V21 fixtures, I/O, Priority Looks, owner structure, and public bindings are preserved.
- Package scripts parse and all distributed files are hash-pinned.

## Qualification boundary

V22 is structurally validated. The unchanged V21 runtime/control path is software-tested, and the preceding baseline has physical evidence for the Micro, each Control One DMX port independently, Control One MIDI, OS2L, and core live behavior. The creative replacements and advancing outline require the short owner test in the package README before V22 replaces V21 at an event.

This is independent community interoperability work and is not affiliated with or endorsed by SoundSwitch, inMusic, or QLC+.
