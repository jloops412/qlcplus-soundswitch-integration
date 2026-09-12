QLC+ V34 WINDOWS TESTING PACKAGE

Use START_HERE.txt for the quickest installation and first bench checks.

V34 repairs STOP/override ownership, keeps Wash/tube chase brightness through
color changes, and corrects specific authored lighting defects. Test this exact
package on your rig. Full SoundSwitch parity and gig qualification are not claimed.

REQUIRED EXISTING APPLICATION
Windows x64 QLC+ 5.3.0 GIT a124abe; Qt 6.8.1; MinGW 13.1.0.
This ZIP supplies plug-ins and a show, not the QLC+ application or an installer.
Use the complete matching host; do not mix arbitrary QLC+/Qt runtime versions.
Exact host/source hashes are in Evidence/package-evidence.json.

INSTALL IN FILE EXPLORER
1. Close QLC+. Preserve your previous show and complete application folder.
2. Extract this ZIP. Copy your matching QLC+ installation, normally C:\QLC+,
   to C:\QLC+-V34-Test. Launch only the test copy during this evaluation.
3. Back up soundswitch.dll and os2l.dll from that copy's Plugins folder; copy
   BOTH bundled DLLs into Plugins. Keep the backup together as a matched pair.
4. In File Explorer open %USERPROFILE%\QLC+\Fixtures. Back up existing files
   of the same names/fixture identities, then copy all FOUR bundled .qxf files.
5. Open %USERPROFILE%\QLC+\InputProfiles. Back up the existing profile and copy
   SoundSwitch-Control-One-Performance.qxi. Create these folders if missing.
   User fixture/profile folders are shared by all your QLC+ installations.
6. Connect the Micro or Control One USB BEFORE launching QLC+, with the fixture
   DMX cable still disconnected. Open the test application's qlcplus5.exe and
   IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw. Resolve missing fixture
   definitions, profile warnings or substituted modes before output testing.
7. Physical output is deliberately unassigned in the supplied show. In QLC+,
   open Inputs/Outputs > Universe 1 > Output > SoundSwitch Hardware. Select the
   EXACT connected Control One DMX 1 (or Micro DMX if that is your cable).
   Leave U2 input/feedback and U3-U6 private outputs intact. Save As your own
   working show to retain your device's real UID. This setup is needed once.
8. Keep the package master unchanged. Record the
   backup paths in a copy of INSTALL_RECEIPT_TEMPLATE.txt. Follow FIRST_TEST.txt.

CHECK ROUTING IN QLC+
U1: Physical rig only. OS2L input line 0, 127.0.0.1:9996. Output initially
    unassigned: use the GUI step above to select your actual connected device
    and port by name. Begin with one output; do not select a private layer.
U2: Control One Performance input line 0, UID soundswitch:controlone:midi;
    Surface feedback line 3, UID soundswitch:controlone:surface. No DMX output.
U3: Internal Priority output line 4, UID soundswitch:priority-layer, 334 channels.
U4: Internal Effect output line 5, UID soundswitch:effect-layer. Keep enabled:
    MOVE/STROBE plus WHITE/UV/BLACK ownership flags require this private route.
U5: Internal Color Latch output line 6, UID soundswitch:color-latch-layer,
    335 channels. Keep enabled for the nine color latches.
U6: Internal Color Hold output line 7, UID soundswitch:color-hold-layer,
    335 channels. Keep enabled for Shift-held colors.
Never route U2-U6 to physical DMX, Art-Net or sACN. The private universes carry
internal control and mirror data, not additional physical fixtures.

PHYSICAL PATCH, ALL ON U1
Four IR-4: 10 Channel, addresses 001 / 011 / 021 / 031.
Wash FX Hex: 40 Channel, address 041.
Focus Spot Two A/B: 18 Channel, addresses 081 / 099.
Four BO-TUBE192: 40 Channel, addresses 175 / 215 / 255 / 295.
Addresses 117-174 remain unused. Authored Focus UV emitters remain disabled.

ESSENTIAL CONTROLS
STOP: click STOP or press Shift + Play/Pause. Releases color, performance and
position overrides before requesting native StopAll. Normal Play/Pause remains.
BLACK: a performance intensity gate; later WHITE/UV/color must not relight it.
BLACKOUT: the separate native button at top right. It remains latched after
STOP until clicked again. If a new loop appears dark, check BLACKOUT first.
Keep the native Grand Master at its default Reduce / Intensity setting.
Limit / All Channels changes internal template data and is not this setup.
The thin status strips are now inert separators: pinned QLC+ did not restore
their disabled state, so a click could start a raw Function. Use the main
controls and Control One LEDs; those small strips no longer display live state.
Color pads latch; Shift + color holds. White/Black/UV use ordinary hold and
Shift-tap latch. Releasing a hold should preserve an existing latch.

ROLLBACK
Disconnect physical DMX and close QLC+. Restore the backed-up user fixture
definitions/input profile, then launch the original application and previous
show. If you replaced DLLs in place, restore BOTH backups first. Preserve later
operator edits separately. V26 predates the Wash/Focus additions.

EVIDENCE
SHA256SUMS.txt covers every packaged file except itself. Verify-Package.py is
an optional Python integrity check; normal installation does not need Python
or PowerShell. Evidence/package-evidence.json binds the DLL, workspace, profile
and sources to the reported CI commit. Local evidence is included only when
available. Read its stated scope; missing evidence is pending, never a pass.
Exact Windows host loading, fixture appearance, aim, audio/OS2L load and physical
recovery remain operator checks. A short bench test does not establish a gig.
