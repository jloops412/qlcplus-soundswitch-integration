# Third-Party Notices and Protocol Attributions

This project has not selected its final distribution license. Do not distribute a product build until the dependency/license inventory is completed.

## Art-Net

This reference implementation emits Art-Net packets from the public Art-Net 4 specification.

Required product documentation credit under the current official terms:

> Art-Net™ Designed by and Copyright Artistic Licence Engineering Ltd.

A distributing product also requires an OEM Code from Artistic Licence. See [the official Art-Net site](https://art-net.org.uk/).

## Open Fixture Library

Open Fixture Library is the preferred upstream fixture source and is MIT licensed. No OFL fixture dataset is bundled in this checkpoint. Studio can search OFL's official API and download one selected QLC+ export; the project retains the OFL key/source URLs, MIT attribution, exact downloaded QXF SHA-256, adapter version, and unreviewed status with the converted immutable native snapshot. Studio also recognizes local QXF files produced by OFL's QLC+ export plugin. Preserve per-fixture source metadata and required notices when a fixture corpus is added.

## QLC+

QLC+ is Apache 2.0 licensed. This checkpoint does not copy or link QLC+ or Qt source. It includes an independently implemented, Studio-only compatibility adapter for the documented QLC+ Fixture Definition (`.qxf`) XML contract and an optional standards-based Art-Net bridge to a separately installed QLC+ application. QLC+ remains neither a runtime dependency nor the EmberLights show model. If QLC+ code is later copied or adapted, preserve its license, copyright/attribution notices, NOTICE content if present, and prominent modified-file notices.

## Microsoft Fluent iconography

The Windows preview draws documented Segoe Fluent Icons glyphs from the operating system and falls back to the Windows-supplied Segoe MDL2 Assets font when Fluent is unavailable. EmberLights does not redistribute either font. The separate [Microsoft Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) repository is MIT licensed and is a candidate for a future curated vector asset pack; no repository SVG or font asset is bundled in this checkpoint.

## SoundSwitch

SoundSwitch is a product/workflow reference only. No SoundSwitch source, assets, fixture database, or trade dress are included. Future migration research must use user-owned exported/copied artifacts and preserve originals.

## Slint

The Windows replacement-shell beta uses the pinned Slint 1.17.1 C++ runtime under the Slint Royalty-free License. The application visibly identifies Slint in its footer. The installer includes the complete upstream `SLINT_LICENSE.md` and `SLINT_THIRDPARTY.md` files next to the runtime dependency; those bundled files are authoritative for the redistributed binary and its dependencies.

The Slint Windows binary depends on the Microsoft Visual C++ 2015–2022 x64 runtime. EmberLights packages the required `msvcp140.dll`, `vcruntime140.dll`, and `vcruntime140_1.dll` app-locally from Microsoft's signed x64 redistributable so a per-user install does not silently depend on a machine-wide runtime. The Preview 106 build input is Microsoft's 14.44.35211 redistributable (`vc_redist.x64.exe`, SHA-256 `cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b`).
