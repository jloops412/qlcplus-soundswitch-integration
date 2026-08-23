# V20 UI/UX Release Candidate

## Artifacts

- Workspace: `IR4-TUBES-CONTROL-ONE-V20-UIUX-PORTABLE.qxw`
- Input profile: `SoundSwitch-Control-One-Performance.qxi`
- Windows plug-in: `soundswitch.dll`
- Source: `qlcplus/plugins/soundswitch/`

The `.qxw` file is the QLC+ project/workspace. The repository copy is portable: the original Micro hardware serial was removed from its saved output UID. Select the desired physical output once after loading it.

## What changed from V19

- Added a clickable four-bank bar.
- Added explicit 1/2/4/8/16-measure dwell controls.
- Added a compact now-playing/status area and clearer Bank/All/order state.
- Made bank and dwell controls usable with a mouse as well as Control One.
- Corrected the Virtual Console widget-ID allocator to ignore nested Function-reference IDs.
- Preserved every one of the 1,897 V19 Functions byte-for-byte; V20 adds nine inert UI-state Scenes only.

No creative Autoloop, manual Collection, Autoplay parent, Priority Look, fixture, or core Control One behavior was intentionally changed in the V20 UI pass.

## Structural audit

- XML parses successfully.
- 16 fixtures: eight physical and eight private Priority Layer duplicates.
- 1,906 Functions: 1,619 Scenes, 149 Chasers, and 138 Collections.
- All Function IDs are unique.
- All 1,756 Chaser/Collection references resolve.
- All Scene fixture references resolve.
- 392 actual Virtual Console widget IDs are unique.
- 128 raw Autoloops and 128 manual one-child owner Collections are present.
- No Windows paths, usernames, email addresses, tokens, or secrets are present.
- The published workspace has no hardware serial.

## Known status

Verified during the preceding V19/control-layer work:

- SoundSwitch Micro DMX output;
- Control One DMX 1 output;
- Control One MIDI input and page/bank/pad workflow;
- Control One LED feedback;
- manual Autoloop latch and same-pad off across all four banks;
- Bank/All Autoplay, sequential/random policies, and pad seek;
- full-frame Priority Look takeover and release back to the advancing loop;
- color-only overrides;
- Group 1 IR-4 and Group 3 tube intensity;
- VirtualDJ OS2L connection and beat timing;
- reconnect logic in the plug-in.

Still pending before V20 can be called gig-qualified:

- physical V20 UI regression test;
- Control One DMX 2 and both DMX ports simultaneously;
- sustained unplug/replug testing;
- a two-hour combined VirtualDJ/audio/OS2L/MIDI/DMX soak;
- rebuild against one exact supported QLC+ release/commit.
