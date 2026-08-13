# EmberLights Windows installer 0.1.0-preview.85.2

This is an installer-only repair build from source commit
`172641a3d3e3243c25f643241c99128c3ed4ae17` and source tree
`a38745fa15c40cd38ce4716f233178dd88e739a4`.

## Upgrade and uninstall behavior

- Installs per user under `%LocalAppData%\Programs\EmberLights`.
- Detects and silently removes earlier EmberLights installers registered by the
  current NSIS installer, the legacy NSIS installer, or the earlier Inno Setup
  installer before installing this build.
- Registers both normal and quiet uninstall commands for this build.
- Its uninstaller removes the installed application, packaged tools,
  shortcuts, and current or stale EmberLights uninstall records.
- Projects and settings stored outside the application install directory are
  preserved.

## Verification performed

- Windows package and installer unit tests: 17/17 passed.
- Windows x64 application and tools built with static C/C++ runtime closure.
- Payload-manifest creation and verification passed for all 18 product files.
- NSIS archive integrity and extraction test passed.
- All 19 files in the extracted installer payload matched the verified staging
  directory byte for byte (18 product files plus the payload manifest).
- An independent second NSIS build was byte-identical to the published setup
  executable.

## Checksums

- `EmberLights-0.1.0-preview.85.2-Setup.exe`
  SHA-256: `c1b0ff364b7dc7bbd84bb741d235d5ec77f7df66603854c60970ecfb09fb9e1a`
- `EmberLights-Windows-payload-manifest.json`
  SHA-256: `fd55fe90351f2d06198254895f0cc3355b5327a0233490882c5e477070871678`

## Test boundary

This preview is unsigned, so Windows SmartScreen may warn before launch. The
package, extraction, runtime dependency closure, upgrade logic, and uninstall
logic were verified off-Windows. Installation over each historical installer
and uninstall through Windows Installed Apps still require the native Windows
lifecycle test this build was produced for.
