# SoundSwitch migration evidence workflow

EmberLights does not modify a SoundSwitch project or audio file. This testing build can inspect an exported project read-only and create a lossless migration source bundle before any semantic conversion is attempted.

## What is verified

SoundSwitch documents that a project export carries project settings such as venues, fixtures, Autoloops, Static Looks, positions, and attributes. It also documents that track lightshows are saved separately against audio metadata, and that scripted audio must be copied when moving systems:

- [Move a SoundSwitch project to another computer](https://support.soundswitch.com/en/support/solutions/articles/69000860388-soundswitch-how-do-i-move-my-soundswitch-project-to-a-different-computer-)
- [Saving projects and light shows](https://support.soundswitch.com/en/support/solutions/articles/69000853039-soundswitch-saving-projects-and-light-shows)

Independent public format research suggests that recent exports may contain a `.ssproj` manifest, `SoundSwitchVenues.bin`, Autoloop databases, `SoundSwitchTrackMap.bin`, `.ssfile` scripts, and `recordable/*.dat`. Some observed `.ssfile` payloads begin `AA AA 09 55`. Those details are treated as version-specific evidence, not a stable specification; EmberLights currently recognizes and reports them but does not guess at their semantics.

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
```

## What is needed for semantic conversion

Provide a verified baseline bundle from the SoundSwitch version you use, ideally containing a small known venue with one fixture, one Static Look, one Autoloop, and one deliberately simple scripted track. Then make **one** documented change, export again, and create a comparison report. Repeat per feature. This isolates candidate fields without risking your original project. Copied audio is needed only for validating track identity/relinking; keep your originals and share only material you are authorized to use.

Until that corpus exists, EmberLights makes no claim that SoundSwitch binary cues are semantically imported. The current bundle is the loss-preserving input needed to build and regression-test that decoder safely.
