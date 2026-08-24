# Switch from SoundSwitch to QLC+ with Micro or Control One

This guide is for a DJ who wants one lighting application at show time: QLC+.

## What this package replaces

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch plug-in --> Micro or Control One DMX
```

It does not run SoundSwitch in the background, install a bridge, replace Control One firmware, or require the retired EmberLights application.

## Before starting

You need:

- Windows x64;
- SoundSwitch Micro and/or Control One;
- the complete pinned QLC+ `5.3.0 GIT a124abe` installation;
- the V23 release archive; and
- fixtures already set to known DMX modes and addresses.

First confirm the SoundSwitch device appears normally in Windows with its manufacturer software/driver. Then close SoundSwitch. Only one program can own the hardware at a time.

Do not use a generic USB-driver tool on the entire Control One composite device. Its MIDI interface must remain intact.

## Install in ten steps

1. Install the complete pinned QLC+ build in its own folder.
2. Extract the complete V23 archive.
3. Open PowerShell in the extracted folder.
4. Run:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\Test-V23Package.ps1
   ```

5. Close QLC+ and install the plug-in:

   ```powershell
   .\Install-SoundSwitchPlugin.ps1 -QlcRoot 'X:\Path\To\QLCPlus-5.3.0-GIT-a124abe'
   ```

6. Open `IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`.
7. Open QLC+ Input/Output and choose a SoundSwitch output for the desired universe.
8. For Control One, choose the SoundSwitch Control One MIDI input and associate `SoundSwitch-Control-One-Performance.qxi` if required.
9. Switch QLC+ to Operate mode and open the Live Virtual Console page.
10. Run one manual pad before connecting a full lighting rig.

If V21 or V22 already works, skip plug-in installation and open the V23 workspace directly.

## Pick the output

The plug-in presents normal QLC+ output choices:

- SoundSwitch Micro — one DMX output;
- Control One — DMX 1;
- Control One — DMX 2.

Assign the same QLC+ universe to the physical port that feeds the lights. The example workspace uses Universe 1 for the physical show. Universe 3 is a private full-frame Priority Look layer and must not be patched directly to DMX.

## Connect VirtualDJ OS2L

Use direct localhost OS2L when both programs share one laptop. The included `VIRTUALDJ_OS2L_AUTO_RECONNECT.md` gives the exact launch/reconnect setup. QLC+ owns lighting tempo; the plug-in is not involved in OS2L.

## Learn the surface

- `Auto Loop` changes the 32 pads between Autoloops and Priority Looks.
- A normal Bank press selects Medium, Colorful, Slow Dance, or Flashy.
- One Autoloop press latches; the same pad turns it off; a different pad replaces it.
- Auto Bank walks or randomizes one bank.
- Auto All walks Bank 1 through Bank 4 or randomizes all 128 loops.
- Dwell controls how many measures each Autoloop remains selected.
- Speed changes the internal Chaser multiplier and stays independent from dwell.
- A Priority Look becomes sole full-frame output. When released, the underlying Autoloop reappears at its current position.
- A color override changes color parameters only.

The four small indicators beside every onscreen pad are Banks 1–4 from top to bottom. Amber identifies the raw Chaser that is currently running. The tracker under the grid shows the exact loop name.

## Use the included show as-is

The workspace is already patched for:

| Fixtures | Mode | Addresses |
|---|---|---|
| 4 × Both Lighting IR-4 | 10 channel | 1, 11, 21, 31 |
| 4 × Both Lighting BO-TUBE192 | 40 channel | 175, 215, 255, 295 |

Set the physical fixtures to those modes/addresses, connect DMX, and use the show.

## Adapt it to another fixture rig

Treat V23 as a template, not a universal fixture show.

1. Save a new workspace name before editing.
2. Patch the new physical fixtures in QLC+.
3. Build or translate Scenes and Chasers for those fixture IDs.
4. Preserve public control Function IDs and the Virtual Console/Control One logical channels.
5. Preserve the manual owner Collections and Autoplay parent structure.
6. Rebuild the private Priority Layer for the new rig if full-frame Priority Looks are required.
7. Update group-intensity configuration; the current plug-in ranges are specific to the included IR-4/tube patch.
8. Run the validation checklist after every structural change.

Do not simply replace fixture names while leaving Scene fixture IDs untouched. QLC+ Functions target actual internal fixture IDs.

## Safe update rule

The plug-in is built for one exact QLC+ source/runtime tuple. For a new QLC+ release:

1. install it side-by-side;
2. build the plug-in against that exact source;
3. verify the new executable and DLL hashes;
4. test Micro, both Control One ports, MIDI, LEDs, reconnect, OS2L, and DMX; and
5. keep the current known-good QLC+ folder untouched until qualification passes.

Never fix a missing DLL or corrupted text by copying random Qt/FFmpeg files between QLC+ versions.

## Troubleshooting

| Symptom | First check |
|---|---|
| Device does not appear | Close SoundSwitch; reconnect USB; confirm the manufacturer driver/device still appears in Windows |
| Control One DMX works but MIDI does not | Confirm the composite MIDI device remains present and the `.qxi` profile is associated |
| Micro/Control One sends no light | Confirm QLC+ universe output, fixture address/mode, cable direction, and wireless transmitter channel |
| Auto Loop/Priority button does nothing | Confirm V23 is open and there is exactly one SoundSwitch Surface Feedback output |
| Auto Bank/All runs but no pad feedback | Confirm the V23 Live page is open; look at the four-dot rail and native tracker |
| OS2L does not reconnect | Apply the included VirtualDJ keepalive mapping and verify both apps use localhost |
| QLC+ text is corrupted or a runtime DLL is missing | Reinstall one complete coherent QLC+ build; do not mix runtime files |

## Qualification boundary

The package validator proves structure and hashes, not a physical show. Before an event, test the chosen output, controller, fixtures, OS2L, audio workload, reconnect, and emergency Stop behavior. A two-hour combined workload is the gig-qualification target.
