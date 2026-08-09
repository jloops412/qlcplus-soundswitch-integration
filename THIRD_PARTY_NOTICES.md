# Third-Party Notices and Protocol Attributions

This project has not selected its final distribution license. Do not distribute a product build until the dependency/license inventory is completed.

## Art-Net

This reference implementation emits Art-Net packets from the public Art-Net 4 specification.

Required product documentation credit under the current official terms:

> Art-Net™ Designed by and Copyright Artistic Licence Engineering Ltd.

A distributing product also requires an OEM Code from Artistic Licence. See [the official Art-Net site](https://art-net.org.uk/).

## Open Fixture Library

Open Fixture Library is the preferred upstream fixture source and is MIT licensed. No OFL fixture dataset is bundled in this checkpoint. Studio recognizes QXF files produced by OFL's QLC+ export plugin and retains OFL provenance in the converted native profile. Preserve per-fixture source metadata and required notices when a fixture corpus is added.

## QLC+

QLC+ is Apache 2.0 licensed. This checkpoint does not copy or link QLC+ or Qt source. It includes an independently implemented, Studio-only compatibility adapter for the documented QLC+ Fixture Definition (`.qxf`) XML contract and an optional standards-based Art-Net bridge to a separately installed QLC+ application. QLC+ remains neither a runtime dependency nor the EmberLights show model. If QLC+ code is later copied or adapted, preserve its license, copyright/attribution notices, NOTICE content if present, and prominent modified-file notices.

## SoundSwitch

SoundSwitch is a product/workflow reference only. No SoundSwitch source, assets, fixture database, or trade dress are included. Future migration research must use user-owned exported/copied artifacts and preserve originals.
