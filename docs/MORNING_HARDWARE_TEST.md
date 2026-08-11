# EmberLights Morning Hardware Test

This test separates the SoundSwitch Micro transport from project, fixture-library, Autoloop, and OS2L behavior. Host-accepted USB writes are not proof of physical DMX; record the transmitter and fixture result.

## Prepare one safe fixture

1. Close SoundSwitch and EmberLights so only the Hardware Test can own the Micro.
2. Connect the Micro to the known-good transmitter or directly to one isolated fixture when practical.
3. Use one Both Lighting IR-4 and confirm its display shows 6-channel mode and DMX address 1.
4. Confirm Master is Off and the transmitter/wireless selection matches the fixture.
5. Disconnect fog, haze, lasers, sparks, strobe effects, and unrelated fixtures.

## Run the bounded raw test

1. Open **EmberLights Hardware Test** from the Start menu.
2. Wait for the descriptor report and enter DMX address `1`.
3. Type `TEST` only after confirming the isolated setup.
4. The shared production Micro session will inspect the exact VID/PID, configuration, alternate setting, and 64-byte bulk OUT pipe; reset and configure the pipe; send the two JLS1 initialization packets; settle for 200 ms; and warm up with 50 blackout frames at 40 Hz.
5. The active pattern sets only DMX channel 1 to 255 for about three seconds, then automatically returns to blackout.
6. Answer whether the fixture visibly produced red.
7. Retain `SoundSwitch-Micro-report.txt` and `SoundSwitch-Micro-active-test.txt` from the Desktop.

## Interpret the result

- Raw channel 1 produces red: the Micro session and selected physical universe path are proven for this state. Continue with the one-fixture Runner comparison.
- USB writes succeed but no red appears: remain in transport/session/electrical diagnosis. Do not adjust Autoloops or the 71-fixture migration as a substitute.
- Open or initialization fails: close competing applications, reconnect the Micro, and retain the exact lifecycle/error report.
- Automatic blackout fails: stop testing and disconnect output; the build is not hardware-safe.

Do not mark the Micro supported or physically qualified until the raw response, blackout, repeat open, and unplug/replug checks are operator-confirmed.
