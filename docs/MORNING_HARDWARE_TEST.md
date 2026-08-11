# EmberLights Morning Hardware Test

This test separates the SoundSwitch Micro transport from Autoloop and OS2L behavior, then proves that the exact compiled fixture/Runner frame matches the raw reference. Host-accepted USB writes are not proof of physical DMX; record the transmitter and fixture result.

## Prepare one safe fixture

1. Close SoundSwitch and EmberLights so only the Hardware Test can own the Micro.
2. Connect the Micro to the known-good transmitter or directly to one isolated fixture when practical.
3. Use one Both Lighting IR-4 and confirm its display shows 6-channel mode and DMX address `001`.
4. Confirm Master is Off and the transmitter/wireless selection matches the fixture.
5. Disconnect fog, haze, lasers, sparks, strobe effects, and unrelated fixtures.

## Run the bounded raw test

1. Open **EmberLights Hardware Test** from the Start menu.
2. Wait for the descriptor report. The test is deliberately fixed to universe 1, address `001`; it does not ask for another address.
3. Type `TEST` only after confirming the isolated setup.
4. The shared production Micro session will inspect the exact VID/PID, configuration, alternate setting, and 64-byte bulk OUT pipe; reset and configure the pipe; send the two JLS1 initialization packets; settle for 200 ms; and warm up with 50 blackout frames at 40 Hz.
5. Before any active output, the frame inspector compiles an exact IR-4 6-channel semantic project through the production compiler and renderer. It compares all 512 DMX slots and all 522 native JLS1 packet bytes against the raw reference and refuses transmission on any mismatch.
6. Stage 1 sends the raw reference—CH1 Red `255`; CH2 Green, CH3 Blue, CH4 White, CH5 Amber, and CH6 UV all `0`—for about three seconds, then blackouts.
7. Confirm that Stage 1 visibly produced red and then blacked out.
8. The tool closes and reopens the Micro, repeats initialization/warm-up, and sends the compiled Runner frame for about three seconds, then blackouts.
9. Confirm that Stage 2 visibly matched the same red and then blacked out.
10. Retain `SoundSwitch-Micro-report.txt` and `SoundSwitch-Micro-active-test.txt` from the Desktop.

## Interpret the result

- Both stages produce identical red and blackout: raw Micro output, a clean repeat open, and the exact compiled fixture/Runner path are proven for this state.
- USB writes succeed but no red appears: remain in transport/session/electrical diagnosis. Do not adjust Autoloops or the 71-fixture migration as a substitute.
- Raw red works but the Runner stage fails: preserve the report; the exact failing boundary is reopen, Runner-frame write, or physical frame response.
- Open or initialization fails: close competing applications, reconnect the Micro, and retain the exact lifecycle/error report.
- Automatic blackout fails: stop testing and disconnect output; the build is not hardware-safe.

The six-channel ordering comes from the manufacturer IR-4 manual: Red, Green, Blue, White, Amber, then UV (called Purple in the channel table). Do not mark the Micro supported or physically qualified until the raw response, compiled Runner response, blackout, repeat open, and unplug/replug checks are operator-confirmed.
