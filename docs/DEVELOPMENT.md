# Development Guide

This guide is for contributors working on the active QLC+ workspace and the
minimal SoundSwitch hardware integration.

## Architecture boundary

QLC+ is the only lighting runtime. Normal show behavior belongs in QLC+
fixtures, Scenes, Chasers, Collections, SpeedDials, input profiles, and Virtual
Console controls.

Custom C++ belongs only in the SoundSwitch plug-in when QLC+ cannot provide the
required hardware transport, Control One translation/feedback/reconnect, or
full-frame Priority behavior cleanly. V27's Group 1–4 implementation also
belongs there because it scales selected channel ranges after Priority-frame
selection; it must remain deterministic and covered by the intensity tests.

Do not add a background bridge, replacement engine, separate UI application,
firmware, or runtime tracker.

## Repository checkout

```powershell
git clone "https://github.com/<your-account>/<your-fork>.git" qlcplus-soundswitch-integration
cd qlcplus-soundswitch-integration
```

Run the protected V26 package validator before changing anything:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
releases/qlcplus-control-one/v26/Test-V26Package.ps1
```

This establishes that the immutable V27 generation source and rollback match
their recorded workspace, profile, plug-in, OS2L, and package hashes. If the
V27 candidate package is present, validate it independently as well:

```powershell
releases/qlcplus-control-one/v27/Test-V27Package.ps1
```

Do not weaken either validator or rewrite V26 to make a candidate pass.

## Workspace-only development

Workspace and documentation changes do not require a QLC+ source build.

### Protected V26 provenance

The V26 workspace is deterministically generated from the reviewed V25 source:

```powershell
qlcplus/workspace-tools/Build-V26AutoplayClarity.ps1 `
  -SourceWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw `
  -OutputWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw
```

Validate a candidate:

```powershell
qlcplus/workspace-tools/Test-V26Workspace.ps1 `
  -SourceWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw `
  -CandidateWorkspace qlcplus/workspace-tools/IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw
```

The builder intentionally rejects an unexpected V25 input hash. Never weaken
that guard to make an unrelated source file pass.

The published V26 workspace is the exact protected input to V27. Its required
SHA-256 is
`ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570`.
Retain the V25-to-V26 commands above as provenance, but do not regenerate or
replace the released V26 file during V27 work.

### Build and validate V27

The current full-rig builder reads the immutable V26 release by default and
loads the reviewed creative program from `V27FullRigCreative.py`:

```powershell
python qlcplus/workspace-tools/Build-V27FullRig.py --force
python qlcplus/workspace-tools/Test-V27Workspace.py
```

The builder refuses a V26 source with the wrong hash, an incomplete creative
provider, incomplete fixture frames, changed public control identities, unsafe
fixture-channel values, or missing additive movement controls. The independent
validator compares the generated workspace to V26 and checks fixture patching,
Function/reference/widget integrity, all 1,140 live creative leaves, exact
Focus positions, full-rig visual-bench membership, and safe channel boundaries.
Neither tool proves physical output or visual quality.

During V27 candidate assembly only, copy the validated generated workspace into
the candidate release directory, validate that exact copy and bundled Focus
definition, then run the package validator:

```powershell
Copy-Item -Force `
  qlcplus/workspace-tools/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw `
  releases/qlcplus-control-one/v27/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw

python qlcplus/workspace-tools/Test-V27Workspace.py `
  --candidate-workspace releases/qlcplus-control-one/v27/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw `
  --focus-definition releases/qlcplus-control-one/v27/American-DJ-Focus-Spot-Two.qxf

releases/qlcplus-control-one/v27/Test-V27Package.ps1
```

Once promoted, V27 becomes immutable. Further show changes require another
versioned package rather than replacement of a released workspace or checksum.

### Workspace change rules

- Back up the candidate before editing in QLC+.
- Preserve public Function IDs and logical channels.
- Resolve Scene fixture IDs from the workspace, not names or DMX addresses.
- For creative-only changes, prove the Virtual Console and control bindings are
  unchanged.
- For UI-only changes, prove the Engine and creative Functions are unchanged.
- Keep the private Priority layer on Universe 3 disconnected from physical DMX.
- Preserve V26's IR-4 addresses 001/011/021/031 and tube addresses
  175/215/255/295. V27 fills only the free 041–116 range and leaves 117–174
  free.
- Keep real Focus UV disabled in every released frame: UV shutter zero and UV
  dimmer zero for both physical and private fixtures.
- Create a new versioned package for a promoted change. Do not silently replace
  V26 or a promoted V27 file or hash.

## Plug-in development

The plug-in snapshot in this repository is not a complete standalone QLC+
source tree. It must be integrated into the exact upstream source.

### Pinned source tuple

| Component | Required value |
|---|---|
| QLC+ source | `a124abebe0b5ad6077727c561a5a0e1f3730810c` |
| QLC+ UI identity | `5.3.0 GIT a124abe` |
| Qt headers used by V26/V27 | `6.8.1` |
| Target | Windows x64 |

The current binary is build-matched. A different Qt version, compiler, QLC+
commit, or runtime directory creates a new compatibility tuple and requires
new qualification.

### Prepare upstream QLC+

Use separate sibling directories for this repository and upstream QLC+:

```powershell
git clone https://github.com/mcallegari/qlcplus.git qlcplus-upstream
cd qlcplus-upstream
git checkout a124abebe0b5ad6077727c561a5a0e1f3730810c
```

Copy the focused SoundSwitch plug-in into upstream's `plugins` directory:

```powershell
Copy-Item -Recurse -Force `
  ..\qlcplus-soundswitch-integration\qlcplus\plugins\soundswitch `
  .\plugins\soundswitch
```

In upstream `plugins/CMakeLists.txt`, add the following inside the existing
desktop plug-in block:

```cmake
if(WIN32)
    add_subdirectory(soundswitch)
endif()
```

Apply the focused OS2L correction only if that target is part of the change:

```powershell
git apply --unidiff-zero `
  ..\qlcplus-soundswitch-integration\qlcplus\patches\0001-os2l-use-reported-bpm.patch
```

### Configure and build

Install the dependencies required by the official QLC+ build guide, including
Qt 6.8.1, CMake, and a Windows compiler compatible with the target QLC+ build.
Use the complete Qt/compiler runtime as one coherent tuple.

Example CMake flow:

```powershell
cmake -S . -B build -G Ninja `
  -Dqmlui=ON `
  -DCMAKE_PREFIX_PATH='C:\Qt\6.8.1\mingw_64'

cmake --build build --target `
  soundswitch `
  soundswitch_protocol_tests `
  soundswitch_intensity_tests `
  ss_smoke `
  os2l
```

Adjust the generator and Qt path to the documented compiler kit being
qualified. Do not mix MSVC and MinGW artifacts or copy missing runtime DLLs
from another QLC+ installation.

Locate the three SoundSwitch test executables in the generated build tree:

```powershell
Get-ChildItem -Recurse build -Filter 'soundswitch_protocol_tests.exe'
Get-ChildItem -Recurse build -Filter 'soundswitch_intensity_tests.exe'
Get-ChildItem -Recurse build -Filter 'soundswitch_plugin_smoke_tests.exe'
```

Execute the paths returned by those commands and require all three to exit
zero. The plug-in-load smoke test also requires the candidate plug-in path and
the expected ABI/metadata arguments printed by its usage message. Record an
exact pinned-host load separately; a CI smoke pass is not that evidence.

### Test an unreleased DLL safely

1. Use an isolated copy of the exact QLC+ installation.
2. Close SoundSwitch and QLC+.
3. Back up the existing plug-in and record both SHA-256 values.
4. Copy the candidate DLL only into the isolated installation.
5. Start with no physical output, then a low-risk tester or LED fixture.
6. Record the QLC+ executable, source, Qt/compiler, DLL, device, port, fixture,
   mode, address, and result.
7. Restore blackout and the known-good plug-in after the test.

Never overwrite the current show installation with an unqualified build.

## Release/package changes

A new release package must include:

- a new immutable version directory;
- the exact workspace and input profile;
- every required custom fixture definition, including the V27 Focus Spot Two
  `.qxf`;
- build-matched plug-in binaries;
- source/build compatibility values;
- SHA-256 sums;
- deterministic validation;
- install and rollback scripts;
- release notes, patch/bench instructions, and source provenance; and
- an honest statement of structural, software, physical, and gig evidence.

For V27, the installer must back up same-name user fixture/profile files, and
rollback must restore those backups or remove only the files introduced by the
V27 receipt. Preserve the complete V26 directory as both exact source and
rollback. Do not edit an existing released workspace, fixture definition,
binary, script, or checksum in place.

## Pull-request checklist

- The change has one bounded purpose.
- Product direction remains QLC+ plus the minimal plug-in.
- Relevant deterministic tests pass.
- Public Function IDs and logical channels are preserved or explicitly
  migrated.
- The V26 workspace still has its protected SHA-256 and every existing IR-4 and
  tube address is unchanged.
- Every V27 Autoloop, Priority leaf, performance Scene, and override has the
  required full-rig coverage, and real Focus UV remains disabled throughout.
- No private projects, serials, personal paths, credentials, or vendor assets
  are included.
- Hardware claims name the exact device, port, fixture, mode, and address.
- Release files and hashes change only for a new versioned release.
- Documentation and limitations match the actual evidence.

See [CONTRIBUTING.md](../CONTRIBUTING.md) for the community workflow and
[VALIDATION_AND_MAINTENANCE.md](qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md)
for the full physical regression route.
