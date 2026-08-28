# Third-Party Notices and Distribution Boundary

The current distributable is the QLC+ SoundSwitch V26 package under
`releases/qlcplus-control-one/v26/`.

## QLC+

QLC+ is licensed under the Apache License 2.0. V26 includes two build-matched
QLC+ plug-ins:

- `soundswitch.dll`, built from this project's focused hardware/workflow source
  plus the QLC+ plug-in interface; and
- `os2l.dll`, built from the pinned QLC+ OS2L source with the focused patch in
  `qlcplus/patches/0001-os2l-use-reported-bpm.patch`.

The current source and package carry Apache 2.0 notices and a complete license
copy. V26 does not redistribute the QLC+ executable, the complete QLC+ source
tree, Qt, FFmpeg, or the QLC+ fixture library. The user supplies a separate,
exactly matched QLC+ installation.

- QLC+ project: <https://www.qlcplus.org/>
- QLC+ source: <https://github.com/mcallegari/qlcplus>
- Pinned source commit: `a124abebe0b5ad6077727c561a5a0e1f3730810c`

## Qt and Windows libraries

The plug-ins dynamically use Qt and Windows system libraries supplied by the
compatible QLC+ installation and Windows. Those runtimes are not bundled here.
V26 was built against Qt 6.8.1 headers and must remain paired with its coherent
QLC+/Qt/compiler runtime.

Qt licensing information: <https://www.qt.io/licensing/>

## SoundSwitch and inMusic

SoundSwitch, Control One, and related names identify compatible user-owned
hardware and an interoperability workflow. This project contains no
SoundSwitch application source, firmware, fixture database, branding assets,
or proprietary project content.

The hardware implementation is independent interoperability work informed by
lawful testing against user-owned devices and public information. This project
is not affiliated with or endorsed by SoundSwitch or inMusic.

## VirtualDJ

VirtualDJ is identified as the OS2L source used by the example workflow. No
VirtualDJ code, assets, or licensed content are distributed. This project is
not affiliated with or endorsed by VirtualDJ or Atomix Productions.

## Both Lighting fixtures

Both Lighting product names identify fixtures in the example workspace. The
repository does not distribute manufacturer manuals or branding assets.
Fixture modes and addresses are represented only as project metadata required
to operate the user's own rig.

## Open Fixture Library

Open Fixture Library is a preferred upstream source for portable fixture
definitions and is MIT licensed. No bulk OFL dataset is included. Preserve
source metadata and required notices if definitions are added later.

Open Fixture Library: <https://open-fixture-library.org/>

## Project licensing scope

The current default-branch source and documentation are distributed under the
root Apache License 2.0 unless a file states otherwise. Release packages include
their own license copy and exact third-party boundary.

