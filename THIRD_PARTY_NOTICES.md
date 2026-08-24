# Third-Party Notices and Distribution Boundary

The current distributable in this repository is the QLC+ SoundSwitch V22 package under `releases/qlcplus-control-one/v22/`. Historical standalone EmberLights code and installers remain archived development provenance and are not part of the V22 runtime or release.

## QLC+

QLC+ is licensed under Apache License 2.0. The V22 package reuses the V21 `soundswitch.dll`, built as a QLC+ I/O plugin against exact QLC+ source commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`, and compiles the QLC+ plugin-interface implementation used by the matched core.

The V22 package includes a complete copy of Apache License 2.0 as `LICENSE-APACHE-2.0.txt`. The custom SoundSwitch plugin source files also carry Apache 2.0 notices. No QLC+ executable, Qt runtime, fixture library, or full QLC+ source tree is distributed in the V22 archive; the user supplies the separately installed, exactly matched QLC+ core.

QLC+ project: <https://www.qlcplus.org/>

QLC+ source: <https://github.com/mcallegari/qlcplus>

## Qt and Windows libraries

The plugin dynamically uses the Qt runtime and Windows system libraries supplied by the compatible QLC+ installation and Windows. Those runtimes are not bundled in the V22 archive. The compatibility tuple in the V22 README records the headers and installed runtime against which this binary was qualified.

Qt licensing information: <https://www.qt.io/licensing/>

## SoundSwitch and inMusic

SoundSwitch, Control One, and related product names are used only to identify compatible user-owned hardware and the performer workflow being recreated. This project contains no SoundSwitch application source, firmware, fixture database, branding assets, or proprietary project content.

The hardware protocol implementation is independent interoperability work informed by testing against user-owned devices and prior project findings. This project is not affiliated with or endorsed by SoundSwitch or inMusic.

## Both Lighting fixtures

Both Lighting product names identify the fixtures patched in the example workspace. The project does not distribute the manufacturer’s manuals or branding assets. Fixture modes and addresses are represented only as user-created QLC+ project/fixture metadata required to operate the user-owned rig.

## Open Fixture Library

Open Fixture Library is a preferred upstream source for portable fixture definitions and is MIT licensed. No bulk OFL dataset is included in V22. Preserve source metadata and required notices if OFL definitions are added later.

Open Fixture Library: <https://open-fixture-library.org/>

## Art-Net

The V22 QLC+ SoundSwitch package does not require or bundle the retired standalone project’s Art-Net implementation. Historical source may still reference Art-Net and is outside the V22 release boundary.

Art-Net™ is designed by and copyright Artistic Licence Engineering Ltd. See <https://art-net.org.uk/> for its current specification and attribution terms.

## Project licensing scope

The V22 package’s plugin source and binary are distributed under Apache License 2.0 as stated in their source headers and packaged license. Do not infer that every historical file elsewhere in this repository has been relicensed; archived components retain their own file-level notices and dependency obligations.
