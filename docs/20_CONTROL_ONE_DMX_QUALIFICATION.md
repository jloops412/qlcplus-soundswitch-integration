# SoundSwitch Control One DMX qualification

EmberLights has an independently implemented, isolated Windows transport for
the two DMX output jacks on SoundSwitch Control One. It does not include
SoundSwitch code, firmware, assets, display handling, storage handling, or
MIDI mappings.

## Established host contract

The current SoundSwitch desktop package and public device information establish
the following interoperability contract:

| Field | Value |
|---|---|
| USB identity | `VID_15E4 / PID_0054` |
| USB configuration | `1` |
| USB interface / alternate setting | `0 / 0` |
| Bulk OUT pipe | `0x01`, 64-byte maximum packet |
| Output count | 2, addressed as zero-based ports `0` and `1` |
| DMX frame | 522 bytes: `sTRt`, JLS1 DMX header, port, start code, 512 slots |
| Initialization | Two shared JLS1 controls, then output-mode controls for jack 1 and jack 2 |
| Device latency floor | 10 ms |

The protocol codec, both port selectors, exact slot placement, initialization
sequence, bounded frame interval, device inspection, safe close blackout, and
host failure paths have automated tests. The normal Connections UI remains
locked until the physical procedure below succeeds.

## Booth procedure

1. Close SoundSwitch so only one process owns the device.
2. Disconnect fog, sparks, lasers, motion, and any fixture where full channel
   output is unsafe. Prefer a DMX tester or a simple dimmer fixture.
3. Connect the tester/fixture to Control One jack 1 and jack 2. Note the safe
   test channel for each output.
4. Open **Control One DMX Test** from the EmberLights Start menu. Its first
   screen is help only and sends nothing.
5. From Command Prompt, run:

```bat
"%LOCALAPPDATA%\Programs\EmberLights\Tools\soundswitch_control_one_probe.exe" ^
  --active-test --acknowledge-live-output ^
  --channel-one 1 --channel-two 1 ^
  --report "%USERPROFILE%\Desktop\control-one-dmx-qualification.json"
```

The tool opens only `VID_15E4/PID_0054`, verifies configuration/interface/pipe,
sends the established initialization sequence, drives the selected channel on
jack 1, blackouts both jacks, drives the selected channel on jack 2, and
blackouts both jacks again. It refuses active output without the explicit
acknowledgement flag.

## Passing evidence

A host-side pass requires the expected descriptors, accepted initialization,
accepted writes on both zero-based ports, and accepted bounded blackout writes.
This still does not prove electrical DMX output. Physical qualification also
requires an operator to observe:

- only jack 1 responds during the first stage;
- both jacks return to zero;
- only jack 2 responds during the second stage;
- both jacks remain at zero when the utility exits;
- unplug/replug followed by a second complete run produces the same result.

Retain the generated JSON and add the observed results to the hardware issue.
Until that evidence exists, Control One DMX is implemented and
contract-tested—not physically verified or gig-qualified.

The main Connections page may enable the same transport through the explicit
`Experimental (unqualified) — Jack 1 = U1, Jack 2 = U2` project option. That
option is default-off, requires a safety acknowledgement, and never changes
the evidence tier. Prefer this qualifier first; then exercise the production
Runner route with only safe test fixtures connected.

## Recovery

If open fails, close SoundSwitch, reconnect the Control One, and verify the
official Control One driver is installed. Do not replace the device's driver
during a gig. If any stage drives the wrong jack or blackout fails, disconnect
USB and DMX, disable the experimental Connections option, and attach the JSON
report.
