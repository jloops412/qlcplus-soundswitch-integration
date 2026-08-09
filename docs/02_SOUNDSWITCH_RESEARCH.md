# SoundSwitch and Competitor Research

## Research status labels

- **Verified** — supported by official documentation, public protocol, source code, or an owned-file/hardware probe.
- **Probable** — consistent evidence but not yet verified on Joshua's setup.
- **Unknown** — must not be promised.

## SoundSwitch model we must preserve

### Verified

- Track-specific scripts, Autoloops, Static Looks, live overrides, Master/Group/Fixture control tracks, Position Cues, Attribute Cues, movement effects, and fader-aware performance behavior.
- Static Looks are sparse: excluded fixtures continue lower-layer behavior; included zero-intensity fixtures remain off.
- In SoundSwitch 2.9, a manually selected Autoloop begins immediately, runs for its duration, and then returns to the scripted track; infinite and track-duration repeat options are also available.
- Static Looks behave as enabled scene overlays. Public SoundSwitch documentation explains their sparse fixture behavior but does not specify a beat/bar-quantized release or fade contract.
- SoundSwitch separates project saves from song lightshow saves, creating a migration/recovery requirement we should improve.
- Direct DJ integration behavior is deeper than BPM alone and can include tempo, playhead, loop, seeking, reverse, and fader behavior.
- Control One is officially usable as a general MIDI controller, while third-party OLED/feedback support is not guaranteed and third-party use of its DMX interface is not officially supported.
- `.ssproj` packages can contain venues, fixtures, Autoloops, Static Looks, positions, attributes, and optionally lighting files; song work also involves audio-associated data.

### Design implications

- The core is a per-property priority resolver, not a flat scene launcher.
- Master intent must be portable across rigs; group/fixture adaptations specialize it.
- Track association should combine internal ID, DJ-library identity, fingerprint/hash, path, metadata, duration, and manual confirmation rather than depend on one audio tag.
- Import must be non-destructive and loss-preserving.

## VirtualDJ and OS2L

### Verified baseline

OS2L uses short JSON messages over a network connection:

- `beat`: BPM, beat position, change flag, optional strength;
- `btn`: named button and on/off state;
- `cmd`: numeric command with a percentage parameter;
- `feedback`: app-to-DJ-software state feedback.

VirtualDJ supports beat/phase synchronization, `os2l_button`, `os2l_cmd`, and same-PC or LAN operation.

### Extended path

VDJScript subscriptions and other VirtualDJ interfaces may expose exact deck/mixer state. This remains a dedicated spike because update rate, lifecycle, and some messages are not fully documented. A native VirtualDJ plugin is a last resort, not the starting assumption.

## QLC+ decision

QLC+ 5.2.2 is a mature Apache-2.0 C++/Qt console with an engine and broad protocol/device plugins. Its current engine links Qt Core, GUI, Multimedia and additional UI/script infrastructure. Its scene/chaser/show model is general-purpose rather than song/transport/event-first.

Accepted use:

- source and test reference;
- optional local/LAN Art-Net compatibility bridge to QLC+-supported USB hardware;
- possible selective adaptation of isolated components after license/dependency review;
- interoperability target and fixture/protocol validation tool.

Rejected default:

- full application fork;
- mandatory QLC+/Qt dependency in Runner;
- using QLC+'s project model as our canonical format;
- forcing song scripts into a generic scene/chaser model.

## Wolfmix role

Wolfmix is a secondary inspiration for tactile Color/Move/Beam engines, group performance controls, palettes, presets, strobe/blinder/fog actions, and immediate parameter editing. These ideas enter only when they strengthen the SoundSwitch-first workflow.

## Primary sources

- SoundSwitch support documentation and release notes.
- [SoundSwitch Autoloop Improvements in 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9).
- [SoundSwitch Static Looks Explained](https://support.soundswitch.com/en/support/solutions/articles/69000863339-soundswitch-static-looks-explained).
- [OS2L protocol](https://os2l.org/).
- [VirtualDJ OS2L documentation](https://virtualdj.com/wiki/os2l.html).
- [Control One FAQ](https://support.inmusicstore.com/en/support/solutions/articles/69000847095-soundswitch-control-one-frequently-asked-questions).
- [QLC+ source](https://github.com/mcallegari/qlcplus) and [QLC+ project site](https://www.qlcplus.org/).
- [Open Fixture Library](https://open-fixture-library.org/about).
- [Art-Net 4 specification](https://art-net.org.uk/downloads/art-net.pdf).

## Legal/ethical boundary

- Implement documented behavior and open protocols.
- Inspect and migrate only user-owned/exported SoundSwitch artifacts.
- Do not extract SoundSwitch source, production fixture databases, trade dress, or proprietary assets.
- Keep undocumented Control One investigation isolated and obtain legal review before distributing derived support.
- Preserve Apache/MIT/other required notices for any reused code or data.
- Art-Net distribution requires the protocol attribution and an OEM code under the current official terms.
