# V32 Windows testing package

This prerelease packages the recovered V32 Creative workspace with its matched
Windows SoundSwitch hardware plug-in for owner testing. **It is V32, not V33,
and is not gig-qualified.** Full SoundSwitch feature parity and professional
visual quality have not been established on the physical rig.

Download `QLCPlus_V32_Windows_Testing_Package.zip`, extract it, and read the
included `README.md` before installation. The companion
`QLCPlus_V32_Windows_Testing_Package_SHA256SUMS.txt` verifies the ZIP; the internal
`SHA256SUMS.txt` verifies each payload file. Use a separate copy of the compatible
QLC+ installation and preserve the existing installation and show for rollback.
This package does not include the complete QLC+ host or its runtime dependencies.

## Included work

- All 128 redesigned 16-step Autoloops and all 32 redesigned Priority Looks.
- V31 reliability/control work, including independent chase-speed and Autoplay
  dwell controls, selected-loop seeking, and physical/private override coverage.
- V32 native MOVE and STROBE layers, with the existing fixture patch and
  public control identities preserved.
- The matched Windows SoundSwitch DLL, compatible OS2L DLL, Control One input
  profile, required custom fixture definitions, checksums, and build evidence.

The physical patch remains four IR-4 fixtures at U1 001/011/021/031, Wash FX Hex
40-channel at 041, Focus Spot Two 18-channel pair at 081/099, and BO-TUBE192
40-channel fixtures at 175/215/255/295. U3 Priority and U4 effects are internal
plug-in routes and must remain disconnected from physical/network output.

## Exact compatibility and evidence

The SoundSwitch plug-in targets Windows x64, QLC+ source
`a124abebe0b5ad6077727c561a5a0e1f3730810c` (UI `5.3.0 GIT a124abe`), Qt 6.8.1,
and MinGW 13.1.0. Do not treat it as compatible with an arbitrary QLC+ version.
The package records the recovered CI binary's source and SHA-256 evidence;
packaging does not rebuild or silently replace that binary.

The release workflow verifies the entire payload, ZIP hash, and exact
ZIP-to-payload equality, then runs the unchanged V31 and V32 preservation and
corruption-rejection checks. Recovered Windows CI evidence records deterministic
software tests and a plug-in load smoke check without hardware. A load in the
exact complete pinned QLC+ host, physical output, visual review, fault recovery,
and combined DJ workload remain separate pending checks.

## Confirmed issues to test before any show use

- **STOP is not a verified blackout for Flash overrides.** Native StopAll stops
  running Functions; `Scene::Flash` uses separate DMX sources. White, UV, or
  color Flash overrides may remain lit after STOP. Do not rely on STOP as the
  only way to extinguish the rig. Verify the QLC+ Blackout control separately.
- WHITE/BLACK/UV hold and latch controls share Scene IDs. Releasing a hold can
  clear a latched override instead of restoring the preceding latch.
- Color-only overrides can replace emitter brightness on fixtures whose color
  channels also determine intensity, so the underlying loop's intensity shape
  is not reliably preserved across every fixture class.
- Common scene fades can interpolate discrete Focus channels, including color
  and gobo selections. Intermediate values may produce unintended transitions.

The software checks currently do not establish that these issues are repaired.
They must remain visible in testing reports and any later release comparison.

## Testing focus

Check the exact host identity, fixture definitions, input profile, and output
patch first. Then observe STOP/Blackout and every override release, sequential
and random Autoplay seeks, speed versus dwell, Priority handoffs, MOVE/STROBE,
group intensity, OS2L timing, and Control One reconnect/LED restoration. Review
each Autoloop and Priority Look on the actual fixtures before rating its design
quality. Preserve a working rollback while evaluating this candidate.

This versioned package and prerelease are a testing checkpoint. Existing V26
and V27 packages remain unchanged. Later fixes require a new version and its own
evidence; they must not overwrite these release assets.
