QLC+ V32 WINDOWS TESTING PACKAGE
Prepared 2026-09-12 UTC. Read this before enabling physical output.

This contains the exact V32 creative workspace and its matched Windows
SoundSwitch hardware plug-in. V33 is NOT included. This is a testing candidate,
not a gig-qualified release. Automated source/workspace and Windows plug-in
checks passed; loading inside the actual pinned QLC+ host, physical output,
lighting quality and a combined live workload still need operator observation.

KNOWN ISSUES FOUND IN THE CURRENT V32 SOURCE
- STOP stops Functions but may leave Scene Flash overrides, including color,
  WHITE and UV, active. STOP is not an emergency blackout. Validate QLC+'s native
  Grand Master blackout and Global intensity zero with one LED fixture first.
- A WHITE/BLACK/UV hold and latch share a Flash Scene: releasing a hold can
  cancel a previously latched state.
- Wash and tube color overrides can flatten the underlying emitter-based
  brightness/pixel pattern instead of preserving all intensity variation.
- Focus discrete color/gobo/rotation/prism channels have no ExcludeFade flags;
  transitions between looks may sweep through intermediate wheel/macro values.
- Priority handoff frame ordering, MOVE takeover, reconnect and combined
  output still need the exact Windows host and rig checks.
Full SoundSwitch feature parity and professional physical appearance are not
claimed. OLED, Scripted playback and several control functions remain reserved.

REQUIRED EXISTING APPLICATION
Use your complete QLC+ Windows x64 installation identified as:
  QLC+ 5.3.0 GIT a124abe
  QLC+ source a124abebe0b5ad6077727c561a5a0e1f3730810c
  Qt 6.8.1, MinGW 13.1.0
  qlcplus5.exe SHA-256:
  16dfc419bf878ac4802d88684253d12602dbaaab94579e88fd55519a1fb09533
This ZIP does not include QLC+ itself. It is not compatible with arbitrary
QLC+ or Qt versions. Do not mix runtime files from different installations.

INSTALL USING FILE EXPLORER (NO POWERSHELL REQUIRED)
1. Close QLC+ and SoundSwitch. Keep the complete previous show and package.
2. Extract this ZIP into a new folder. Keep this extracted master unchanged.
3. Copy your whole existing QLC+ folder, normally C:\QLC+, to a separate
   C:\QLC+-V32-Test folder. Test using that copy's qlcplus5.exe.
4. In the copied application's Plugins folder, preserve soundswitch.dll and
   os2l.dll in a named backup folder, then copy this package's two DLLs there.
5. In File Explorer enter %USERPROFILE%\QLC+\Fixtures in the address bar.
   Back up any same-name definitions or definitions of the same fixture
   identity. Copy all four files from this package's Fixtures folder there.
   These user definitions are shared between your QLC+ installations; keep
   the backup so they can be restored when rolling back.
6. Open %USERPROFILE%\QLC+\InputProfiles. Back up any same-name profile, then
   copy SoundSwitch-Control-One-Performance.qxi there. Create folders if needed.
7. Fill in a COPY of INSTALL_RECEIPT_TEMPLATE.txt with the backup locations.
8. Keep physical DMX disconnected. Launch C:\QLC+-V32-Test\qlcplus5.exe and open
   IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw from this extracted package.
   Resolve any missing fixture/profile or substituted mode before continuing.
9. Use Save As to create your own working copy in a new show folder. Do not
   overwrite the package master or previous show. Check routing below before
   connecting lights. Follow FIRST_TEST.txt.

INPUT/OUTPUT ROUTING TO CHECK IN QLC+
The exact V32 source workspace retains a historical U2 DMX 2 output. It must
be removed from your testing copy; all eleven physical fixtures are on U1.

Universe 1: OS2L input, line 0, local 127.0.0.1:9996. Physical show output:
            use the actual connected Micro DMX (line 0) or Control One DMX 1
            (line 1). Start with only the one output you will test.
Universe 2: Control One Performance INPUT, line 0,
            UID soundswitch:controlone:midi, profile SoundSwitch Control One
            Performance. Keep Surface FEEDBACK, line 3,
            UID soundswitch:controlone:surface. Remove its physical DMX 2
            output. This universe is the control surface, not the show frame.
Universe 3: Only private Priority output, line 4,
            UID soundswitch:priority-layer, 334 channels. Keep enabled.
Universe 4: Only private Effect output, line 5,
            UID soundswitch:effect-layer. Keep enabled for MOVE/STROBE.

Never send U3 or U4 to physical DMX, Art-Net or sACN. They are internal layers.
If using Control One DMX 2 for this same rig, move its physical output route
to U1, line 2; do not send U2 control values to the fixtures. Test that port
independently before combining routes. Use QLC+ UI output names and UIDs to
distinguish the private routes from similarly named hardware ports.

FIXTURE PATCH (FRONT-PANEL ADDRESSES, ALL PHYSICAL FIXTURES ON U1)
IR-4 1 / 2 / 3 / 4: 10 Channel, addresses 001 / 011 / 021 / 031
Wash FX Hex:         40 Channel, address 041
Focus Spot Two A:    18 Channel, address 081
Focus Spot Two B:    18 Channel, address 099
BO-TUBE192 1/2/3/4:   40 Channel, addresses 175 / 215 / 255 / 295
Addresses 117-174 stay unused. The real Focus UV emitters remain disabled in
the authored show. Confirm A/B identity and mover clearance before movement.

ROLLBACK USING FILE EXPLORER
1. Set your physically verified blackout/intensity-zero control, disconnect
   physical DMX, and close QLC+.
2. Restore the user fixture definitions and input profile recorded in your
   receipt. Remove only files newly introduced by this test, if no prior file
   existed; preserve any later operator edits separately.
3. Open the untouched original QLC+ application folder and previous show.
   If you chose to overwrite DLLs instead of using the separate test copy,
   restore both backed-up DLLs before launching.
4. If returning to V26, disconnect the Wash and Focus fixtures; V26 predates
   those additions. Repeat your one-fixture output and blackout check.

FILES AND EVIDENCE
- SHA256SUMS.txt covers every packaged file except itself.
- Verify-Package.py is an optional Python checksum/XML verification command;
  Python is not required for normal File Explorer installation or QLC+ use.
- Evidence/package-evidence.json records exact source and component hashes.
- Evidence/soundswitch-build-evidence.json is the original CI evidence.
- V32_Creative_Review.html opens offline in a browser for score previews.
  It is an approximate preview, not evidence of calibrated physical lighting.

Do not treat the offline preview or a green automated check as proof that the
actual show rig is ready. Record the physical results in your receipt.
