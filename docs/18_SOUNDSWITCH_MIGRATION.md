# SoundSwitch migration evidence workflow

EmberLights does not modify a SoundSwitch project or audio file. This testing build can inspect an exported project read-only and create a lossless migration source bundle before any semantic conversion is attempted.

## What is verified

SoundSwitch documents that a project export carries project settings such as venues, fixtures, Autoloops, Static Looks, positions, and attributes. It also documents that track lightshows are saved separately against audio metadata, and that scripted audio must be copied when moving systems:

- [Move a SoundSwitch project to another computer](https://support.soundswitch.com/en/support/solutions/articles/69000860388-soundswitch-how-do-i-move-my-soundswitch-project-to-a-different-computer-)
- [Saving projects and light shows](https://support.soundswitch.com/en/support/solutions/articles/69000853039-soundswitch-saving-projects-and-light-shows)

Independent public format research suggests that recent exports may contain a `.ssproj` manifest, `SoundSwitchVenues.bin`, Autoloop databases, `SoundSwitchTrackMap.bin`, `.ssfile` scripts, and `recordable/*.dat`. Some observed `.ssfile` payloads begin `AA AA 09 55`. Those details are treated as version-specific evidence, not a stable specification. The general importer recognizes and reports them without guessing. The current-2026 adapter described below accepts only the exact reviewed five-artifact identity and imports one evidence-backed Autoloop slice; the broader V1 fallback remains an output-disabled approximation.

## Installed workflow

In EmberLights, use **File → Import SoundSwitch 2026 Project (Output-Disabled)** for the exact reviewed first slice, **File → Inspect SoundSwitch Project** to create a JSON inventory, **File → Compare SoundSwitch Exports** after one controlled change, or **File → Create SoundSwitch Migration Bundle** to make a verified copy. The comparison reports added, removed, and modified payloads with SHA-256 values and bounded changed-byte ranges; it does not retain or export source payload bytes.

The bundle contains:

- `payload/`: every regular source file at the same relative path;
- `inventory.json`: kind classification, byte size, SHA-256, recognized header flag, and warnings/errors.

The destination must not already exist. Every copied payload is re-hashed before the bundle is published. Symbolic links and files that change during inspection are rejected.

The installed command-line tool supports the same workflow:

```powershell
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" inspect "D:\ExportedProject.ssproj" --report "$env:USERPROFILE\Desktop\soundswitch-inspection.json"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" compare "D:\Export-before" "D:\Export-after" --report "$env:USERPROFILE\Desktop\soundswitch-comparison.json"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" bundle "D:\ExportedProject.ssproj" "$env:USERPROFILE\Desktop\MyShow-EmberLights-migration"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" convert-2026-red-smooth "D:\2026.ssproj" "$env:USERPROFILE\Desktop\EmberLights-2026-Red-Smooth.emberlights"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" convert-v1 "D:\2026.ssproj" "$env:USERPROFILE\Desktop\EmberLights-2026-V1.emberlights"
& "$env:LOCALAPPDATA\Programs\EmberLights\Tools\emberlights_migrate.exe" verify-source-binding "$env:USERPROFILE\Desktop\EmberLights-2026-V1.emberlights" "D:\2026.ssproj" --report "$env:USERPROFILE\Desktop\EmberLights-migration-review.json"
```

`verify-source-binding` now produces an operator-facing review summary as well
as the byte-identity audit. It reports project validation and output-disable
state, then separates Fixture Profiles, Patch, Static Looks, Autoloops,
Scripted Tracks, scripted audio, and MIDI mappings into one of these explicit
states:

- `approximated` — the narrow pilot converter created useful replacement data,
  but the source semantics were not decoded;
- `sourceEvidenceOnly` — source artifacts were inventoried by hash but have no
  qualified project representation;
- `projectDataUnqualified` — project objects exist without per-object evidence
  that they came from SoundSwitch;
- `missingDependency` — the evidence needed to recover that area is absent;
- `notImported` — neither a supported source artifact nor a project object was
  found.

The review can say `readyForManualReview` only when the project validates, all
DMX output paths are disabled, and the Venue/Autoloop hashes match the source
claim. This means it is safe to inspect the candidate; it does **not** mean the
content was imported exactly or that output is ready to enable. Stable action
codes let the Studio UI present the same repair checklist without inventing a
second migration policy.

## Current-2026 exact first Autoloop slice

`convert-2026-red-smooth` and the File-menu import first require the exact reviewed current project identity: project marker, Venue database, primary and extended Autoloop catalogs, and `SSAutoLoop1.ssfile`. Every artifact is size-bounded and SHA-256 checked. The adapter uses the authored placement arrays and the exact current-project Venue target map; it never derives choreography from the loop name or reuses target IDs from another project.

The first accepted slice is:

- bank/slot: `Medium / 1`;
- source identity: `Red - Smooth Pulse`;
- length: 8 bars;
- representation: Autoloops V2 semantic source/program;
- evidence: exact catalog placement plus byte-range-backed A/B timeline records;
- status: RGB/intensity timelines are `DeterministicallyTranslated`; unclaimed packed bytes are `PreservedOpaque`; missing uplight-local color is `MissingDependency` / `soundswitch.missing_color_source`;
- output: disabled by construction.

The separate destination project uses the current test rig and does not create the obsolete duplicate generic uplights:

| Fixtures | EmberLights representation | Staged universe/address |
| --- | --- | --- |
| 4 Both Lighting BO-IR4 spotlights | Manufacturer-manual 10-channel snapshot; source uplight intensity reconciles semantically | U1 001, 011, 021, 031 |
| 4 BO-TUBE192/360 tubes | Sixteen RGB cell projections inside each 80-channel physical block; unclaimed functions remain unsupported | U1 041, 121, 201, 281 |

The exact-slice project contains one imported Autoloop at Medium slot 1 and 18 clearly labeled original EmberLights review Static Looks. Those Looks are not claimed SoundSwitch imports. The source directory is never modified; save/reopen and idempotent re-import are generation/digest checked through `StudioDocumentService`.

## Broader V1 fallback

`convert-v1` accepts the recognized SoundSwitch 2.10.x color-rig shape and creates the same current IR-4/tube destination patch, 18 original Static Looks, and the original 128-placement EmberLights starter Autoloop pack. Source Autoloop names are retained only as evidence; no choreography is fabricated from names. This fallback is useful for safe project/fixture/UI testing but is not source-fidelity evidence.

The Windows package also installs `Templates\EmberLights-2026-V1-Template.emberlights`. It remains output-disabled and must be reviewed against the exact physical modes and addresses before any adapter is enabled.

## What is still needed for broad semantic conversion

Provide a verified baseline bundle from the SoundSwitch version you use, ideally containing a small known venue with one fixture, one Static Look, one Autoloop, and one deliberately simple scripted track. Then make **one** documented change, export again, and create a comparison report. Repeat per feature. This isolates candidate fields without risking your original project. Copied audio is needed only for validating track identity/relinking; keep your originals and share only material you are authorized to use.

The supplied current-2026 corpus proves only the first Red Smooth vertical slice. EmberLights makes no broad claim for the other 111 Autoloops, Static Looks, movement/effects, other fixture families, scripted tracks, or other SoundSwitch versions until each evidence class is decoded and regression-tested. The current bundle remains the loss-preserving input for that expansion.
