# SoundSwitch migration evidence workflow

EmberLights does not modify a SoundSwitch project or audio file. This testing build can inspect an exported project read-only and create a lossless migration source bundle before any semantic conversion is attempted.

## What is verified

SoundSwitch documents that a project export carries project settings such as venues, fixtures, Autoloops, Static Looks, positions, and attributes. It also documents that track lightshows are saved separately against audio metadata, and that scripted audio must be copied when moving systems:

- [Move a SoundSwitch project to another computer](https://support.soundswitch.com/en/support/solutions/articles/69000860388-soundswitch-how-do-i-move-my-soundswitch-project-to-a-different-computer-)
- [Saving projects and light shows](https://support.soundswitch.com/en/support/solutions/articles/69000853039-soundswitch-saving-projects-and-light-shows)

Independent public format research suggests that recent exports may contain a `.ssproj` manifest, `SoundSwitchVenues.bin`, Autoloop databases, `SoundSwitchTrackMap.bin`, `.ssfile` scripts, and `recordable/*.dat`. Some observed `.ssfile` payloads begin `AA AA 09 55`. Those details are treated as version-specific evidence, not a stable specification. The general importer recognizes and reports them without guessing; the narrow V1 converter described below is qualified only against the supplied SoundSwitch 2.10.x color rig.

## Installed workflow

In EmberLights, use **File → Inspect SoundSwitch Project** to create a JSON inventory, **File → Compare SoundSwitch Exports** after one controlled change, or **File → Create SoundSwitch Migration Bundle** to make a verified copy. The comparison reports added, removed, and modified payloads with SHA-256 values and bounded changed-byte ranges; it does not retain or export source payload bytes.

The bundle contains:

- `payload/`: every regular source file at the same relative path;
- `inventory.json`: kind classification, byte size, SHA-256, recognized header flag, and warnings/errors.

The destination must not already exist. Every copied payload is re-hashed before the bundle is published. Symbolic links and files that change during inspection are rejected.

The installed command-line tool supports the same workflow:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" inspect "D:\ExportedProject.ssproj" --report "$env:USERPROFILE\Desktop\soundswitch-inspection.json"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" compare "D:\Export-before" "D:\Export-after" --report "$env:USERPROFILE\Desktop\soundswitch-comparison.json"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" bundle "D:\ExportedProject.ssproj" "$env:USERPROFILE\Desktop\MyShow-EmberLights-migration"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" convert-v1 "D:\2026.ssproj" "$env:USERPROFILE\Desktop\EmberLights-2026-V1.emberlights"
```

## Qualified 2026 color-rig V1 conversion

`convert-v1` reads only `.ssproj`, `SoundSwitchVenues.bin`, and `SoundSwitchAutoLoops.bin`. It verifies the SoundSwitch 2.10.x manifest and the four recognized fixture models, hashes the two binary inputs, and emits both a checksummed EmberLights project and a JSON migration report.

The staged native patch is deliberately non-overlapping and output-disabled:

| Fixtures | EmberLights representation | Staged universe/address |
| --- | --- | --- |
| 4 Both Lighting BO-S601 uplights | Mode 2, 10 channels each | U1 1, 11, 21, 31 |
| 4 Both Lighting 360 tubes | 16 RGB cells per tube, 48 channels each | U1 41, 89, 137, 185 |
| 1 CHAUVET DJ Wash FX HEX | Mode 1, 11 channels | U1 233 |
| 2 Both Lighting BO-IR4 spotlights | Mode 1, 10 channels each | U1 244, 254 |

The project contains 18 native Static Looks and 32 native beat-driven Autoloops. Active SoundSwitch Autoloop names are retained, but their patterns are purpose-built semantic equivalents derived from those names; the binary cues are not represented as exact decoded timelines. Movers, GigBars, PartyBars, cold sparks, purchased track shows, and other opaque payloads remain in the original archive. Confirm every physical DMX mode and address in **Patch** before enabling Art-Net, sACN, or a USB-DMX COM port.

The Windows package also installs `Templates\EmberLights-2026-V1-Template.emberlights`, which has the same safe patch and content without source hashes.

## What is still needed for broad semantic conversion

Provide a verified baseline bundle from the SoundSwitch version you use, ideally containing a small known venue with one fixture, one Static Look, one Autoloop, and one deliberately simple scripted track. Then make **one** documented change, export again, and create a comparison report. Repeat per feature. This isolates candidate fields without risking your original project. Copied audio is needed only for validating track identity/relinking; keep your originals and share only material you are authorized to use.

Until that corpus exists, EmberLights makes no broad claim that SoundSwitch binary cues are semantically imported. The current bundle remains the loss-preserving input needed to build and regression-test that decoder safely.
