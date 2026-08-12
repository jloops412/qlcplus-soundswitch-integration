# EmberLights Limited Beta — OS2L and Installer Test

This preview is for the owner and at most three invited testers. It is unsigned, testing-only software and is not yet the sole lighting controller for a paid event.

## What this pass is testing

- clean install, upgrade, launch, uninstall, and `.emberlights` file association;
- the integrated Live fixture-fidelity/control work and Studio document/migration foundation;
- VirtualDJ OS2L automatic discovery and reconnection without a DMX-pad press;
- the safe direct-IP fallback when DNS-SD is unavailable;
- diagnostics and fail-closed behavior with output disabled or connected only to an isolated test rig.

## Five-minute OS2L check

1. Leave physical DMX output disabled, install EmberLights, and open the supplied V1 template.
2. In Connections, enable OS2L at `127.0.0.1:9996`, save, and start the show.
3. In VirtualDJ Options, set `os2l=Auto`, clear `os2lDirectIp`, then restart VirtualDJ once.
4. Confirm EmberLights reports Discovery **Ready**, then OS2L **Ready**, without pressing a DMX pad.
5. Load and transition across several songs. Record whether OS2L ever returns to **Waiting**, for how long, and whether it recovers by itself.
6. Restart VirtualDJ while EmberLights remains running and confirm it reconnects without a pad press.

If step 4 or 6 fails, select **Copy VirtualDJ Setup** in Connections and apply the copied direct-IP values and Keyboard ONINIT action. Repeat the test. Do not substitute `blackout off`; the supplied `EmberLights Keepalive` action is deliberately inert.

## What to send back

- Windows version and VirtualDJ build;
- which path worked: automatic discovery, direct-IP fallback, or neither;
- the exact launch order and song-transition moment if it disconnected;
- EmberLights Diagnostics copied immediately after the failure;
- installer/upgrade/uninstall result and any Windows warning or dialog screenshot;
- no private music, DJ database, or SoundSwitch source files unless deliberately authorized.

Stop testing and return to the known backup lighting path if output behaves unexpectedly. A successful synthetic or OS2L test does not yet qualify this preview for unassisted live use.
