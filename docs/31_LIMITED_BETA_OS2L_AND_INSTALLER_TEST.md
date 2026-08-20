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
2. In Connections, enable OS2L at `127.0.0.1:9996` and save, but leave the show stopped. Confirm OS2L reports **Waiting**: listener/discovery lifetime must not depend on Start Show.
3. In VirtualDJ Options, set `os2l=Auto`, clear `os2lDirectIp`, then restart VirtualDJ once.
4. Confirm EmberLights reports Discovery **Ready**, then OS2L **Ready**, without starting the show or pressing a DMX pad.
5. Start the show, then stop and start it again. The OS2L listener must stay available and the existing client or a clean reconnect must remain usable.
6. Load and transition across several songs. Record whether the TCP client actually disconnects or only beat traffic stops, how long that state lasts, and whether it recovers by itself.
7. Restart VirtualDJ while EmberLights remains open and confirm it reconnects without a pad press.

If step 4 or 7 fails, select **Copy VirtualDJ Setup** in Connections and apply the copied direct-IP values and Keyboard ONINIT action. Repeat the test. Do not substitute `blackout off`; the supplied `EmberLights Keepalive` action is deliberately inert.

## One bounded raw-capture run

Close EmberLights first so only the capture listener owns port `9996`. In PowerShell, run:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\os2l_capture.exe" 2>&1 |
  Tee-Object "$env:USERPROFILE\Desktop\EmberLights-os2l-capture.txt"
```

Exercise these mappings once each, then load, play, stop, and replace one song:

```vdjscript
os2l_button 'blackout'
os2l_button 'blackout' on
os2l_button 'blackout' off
os2l_button 'EmberLights Keepalive' off
```

Press `Ctrl+C` after the final transition. The capture records ordered sequence numbers, Unix-millisecond timestamps, connection events, parsed summaries, and the exact bounded raw JSON. Review the text before sharing it.

## Five-minute SoundSwitch import check

1. Keep every physical output disabled. Select **File → Import SoundSwitch 2026 Project (Output-Disabled)** and choose the extracted root of the exact current `2026.ssproj` source.
2. The exact adapter should identify `Medium / slot 1 / Red - Smooth Pulse / 8 bars` and propose a separate project with 68 semantic fixture rows, 18 original review Static Looks, and exactly one imported semantic Autoloop. Decline the save if the adapter falls back to the broader V1 approximation unexpectedly.
3. Save the candidate under a new `.emberlights` filename. Confirm Patch contains four IR-4 destinations at U1 `001/011/021/031`, tube cell banks beginning at `041/121/201/281`, and no `uplight-*` destination fixtures.
4. In Autoloops, confirm Medium slot 1 is `Red - Smooth Pulse`. Save, close, and reopen the project; the same one-loop source should persist.
5. Treat missing IR-4 color as an expected explicit limitation: this slice retains the source intensity timeline but invents no red color when the source target has no local color records.

The installed CLI exposes the same reviewed path and writes a JSON evidence report:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" `
  convert-2026-red-smooth "D:\2026.ssproj" `
  "$env:USERPROFILE\Desktop\EmberLights-Red-Smooth.emberlights"
```

## What to send back

- Windows version and VirtualDJ build;
- which path worked: automatic discovery, direct-IP fallback, or neither;
- the exact launch order and song-transition moment if it disconnected;
- EmberLights Diagnostics copied immediately after the failure;
- installer/upgrade/uninstall result and any Windows warning or dialog screenshot;
- no private music, DJ database, or SoundSwitch source files unless deliberately authorized.

Stop testing and return to the known backup lighting path if output behaves unexpectedly. A successful synthetic or OS2L test does not yet qualify this preview for unassisted live use.
