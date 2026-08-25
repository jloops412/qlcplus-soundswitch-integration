# V26 release notes

V26 tightens Autoplay visibility and timing while preserving QLC+ as the show engine and stock core application.

## Fixed

- All five dwell choices remain visible on every native dwell state.
- Dwell labels now state measures clearly: `1M`, `2M`, `4M`, `8M`, `16M`.
- Each of the 32 pads has a full-width, four-bank native running-state strip.
- Start Bank and Start All feedback observes all 128 raw Autoloops directly, so the highlighted segment follows actual Chaser execution.
- The previously visible tracker surface remains clipped; no polling process or tracker service was added.
- VirtualDJ OS2L BPM is stabilized by the reported BPM value rather than packet-arrival bursts.
- OS2L timing stops cleanly on disconnect/source silence.

## Preserved

- 2,090 QLC+ Functions and their public IDs;
- all fixtures, addresses, modes, creative content, I/O routing, and Control One bindings;
- ten QLC+ Autoplay parent Chasers and ten native Cue Lists for absolute seek;
- sequential/random Bank and All-Banks modes;
- live dwell changes and the independent 0.25×–4× Chase Speed multiplier;
- Priority Look takeover/release and underlying Autoloop continuation;
- SoundSwitch Micro and both Control One DMX output paths; and
- the stock `qlcplus5.exe`.

## Architecture decision

The running-loop indicator uses stock QLC+ Function Monitoring. QLC+ renders externally started Functions in amber, so V26 enlarges that native signal instead of carrying a custom QLC+ executable only to recolor it. The only new build-matched binary is the focused OS2L plug-in fix.

## Upgrade

With QLC+ closed:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\Test-V26Package.ps1
.\Install-V26.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
```

Then open `IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw` and complete the short owner check in the package README.

V26 is an alpha pending final live observation, physical-fixture confirmation, repeated reconnect testing, simultaneous dual-port qualification, and the combined two-hour DJ workload.
