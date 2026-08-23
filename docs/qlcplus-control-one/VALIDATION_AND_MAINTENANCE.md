# Validation and Maintenance

## Automated checks

- SoundSwitch packet framing for Micro and both Control One DMX ports.
- Plug-in load, interface identity, stable line UIDs, and enumeration.
- Exactly 128 raw Autoloops, four banks of 32, with creative step content unchanged during control-layer refactors.
- Ten Autoplay parents: five scopes times two order policies.
- Autoplay parent duration is common and beat-based.
- Raw Autoloop duration/fade timing is common within each Chaser so the speed multiplier updates a running loop.
- Dwell inputs do not alter chase speed inputs, and Pan/Speed inputs do not alter dwell.
- Same-pad toggle produces an honest stopped/ready state.
- Static Looks are exclusive full-look overlays; color overrides are exclusive color-only overlays.
- OS2L names and controller input channels are unique and stable.
- Generated workspace XML and input profile parse cleanly.

## Physical qualification

1. Start QLC+ with each device already attached.
2. Start QLC+ with the device absent, then attach it.
3. Unplug and reconnect Control One MIDI while a safe loop is selected.
4. Unplug and reconnect each DMX device while output is active.
5. Confirm manual repeat-one, same-pad off, Bank/All Autoplay, Sequential/Random, and live dwell changes.
6. Turn Pan/Speed through every multiplier and confirm the beat phase remains coherent while internal chase rate changes.
7. Apply and release Static Looks over a running Autoloop; confirm the underlying loop continues.
8. Apply color overrides and confirm intensity/movement continue.
9. Test Play/Pause and Stop All separately.
10. Run VirtualDJ audio, OS2L, Control One MIDI, and DMX together for at least two hours before gig qualification.

## Upgrade strategy

- Track an official QLC+ release or pinned commit and keep the custom delta inside the SoundSwitch plug-in whenever possible.
- Build the plug-in against each new QLC+ release; do not copy an old DLL across incompatible Qt/QLC+ builds.
- Maintain a known-good QLC+ folder, workspace, plug-in DLL, input profile, and VirtualDJ mapping as one rollback set.
- Regenerate workspaces from a deterministic generator and compare Function counts, names, and creative Autoloop signatures before installation.
- Install only after QLC+ is closed; preserve QLC+ autosave files and never overwrite the sole recovery copy.

## Publication rules

Reusable documentation and protocol/control code may be published. Exclude machine usernames, absolute paths, hardware serial numbers, private show content, fixture addresses, customer/event names, installation receipts, and personal workflow notes.


