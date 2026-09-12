#!/usr/bin/env python3
"""Package a verified V34 CI binary; never builds or substitutes Windows DLLs."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = 'IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw'
PROFILE = 'SoundSwitch-Control-One-Performance.qxi'
PACKAGE = 'QLCPlus_V34_Windows_Testing_Package'
PINNED_QLC = 'a124abebe0b5ad6077727c561a5a0e1f3730810c'
OS2L_SHA = 'ef611b26fac5d090711af242ef7da880dbf1e1d59d5f22d36b5fb1918bdf6513'
HOST_SHA = '16dfc419bf878ac4802d88684253d12602dbaaab94579e88fd55519a1fb09533'
OLD_DLL_SHA = '44e13e17245682043695ef7386655cd648ea976fd4b50d6e211ff79fc9c1c43e'
STOCK_FIXTURES = {
    'Both-Lighting-BO-TUBE192.qxf': '3bb257593e202e4465998760da2f1f5ee7867cec23023c16d3817e07473e72f5',
    'Chauvet-Wash-FX-Hex.qxf': 'aa0ef16d61a003a85b341a354c47d7d93e2cbab9ec9eeda45f23488ccdfed182',
}
FIXTURES = (
    ('Both-Lighting-IR-4-(BOIR4).qxf', 'Both Lighting', 'IR-4 (BOIR4)', '10 Channel', 10),
    ('American-DJ-Focus-Spot-Two.qxf', 'American DJ', 'Focus Spot Two', '18 Channel', 18),
    ('Both-Lighting-BO-TUBE192.qxf', 'Both Lighting', 'BO-TUBE192', '40 Channel', 40),
    ('Chauvet-Wash-FX-Hex.qxf', 'Chauvet', 'Wash FX Hex', '40 Channel', 40),
)

START_HERE = r'''V34: START HERE

1. Close QLC+. Copy your complete matching installation to C:\QLC+-V34-Test.
   Required: QLC+ 5.3.0 GIT a124abe, Qt 6.8.1, MinGW 13.1.0. Back up BOTH DLLs
   in its Plugins folder, then install the bundled soundswitch.dll and os2l.dll.
2. Back up and install the FOUR Fixtures files in %USERPROFILE%\QLC+\Fixtures
   and the .qxi profile in %USERPROFILE%\QLC+\InputProfiles. These folders are
   shared between installations. Keep your old show and all backups.
3. Connect the Micro/Control One USB before launching the test QLC+ copy.
   Leave the fixture DMX cable unplugged. Open the V34 .qxw. In Inputs/Outputs,
   choose Universe 1 > Output > SoundSwitch Hardware > your EXACT connected
   physical device/port. U1 ships unassigned. Leave U2-U6 intact and Save As.
4. Check Grand Master remains Reduce / Intensity. Connect one LED fixture at
   low Global intensity. Prove top-right BLACKOUT: it stays latched through STOP
   until clicked again. Then test STOP with a loop, color latch and WHITE hold.
   Try mouse STOP and Shift+Play/Pause. Overrides should clear without relighting.
5. Test BLACK then WHITE/color: BLACK must keep output dark. Run a tube/Wash
   chase and change colors: dark cells should stay dark. Release a WHITE hold
   over its latch: the latch should remain. Continue with FIRST_TEST.txt to test
   the full rig, Priority, VirtualDJ timing and Control One recovery.

README.txt has the complete installation, route and rollback instructions.
Record any failure with the exact controls/fixture/address used. This is a
testing candidate; software evidence does not establish physical/gig approval.
'''

README = r'''QLC+ V34 WINDOWS TESTING PACKAGE

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
'''

FIRST_TEST = '''V34 FIRST TEST: RECORD OBSERVATIONS ON THE EXACT HOST AND RIG

1. Connect the actual USB device before QLC+ launch; leave fixture DMX unplugged.
   Confirm QLC+ 5.3.0 GIT a124abe, both plug-ins and all definitions. Assign U1
   to the exact connected hardware/port through Inputs/Outputs and Save As.
   Check U2-U6 routes and default Grand Master Reduce / Intensity. Open Live.
2. Connect one LED fixture on the intended output at low Global intensity.
   Check native BLACKOUT at top right and Global zero with WHITE/color active.
   BLACKOUT is latched: STOP must not cancel it; click BLACKOUT again to release.
3. Start a loop and then a Priority Look. Latch a color, Shift-latch WHITE/UV,
   and hold a second override. Test mouse STOP and Shift+Play/Pause separately.
   Every Flash owner and running Function should release. Late key releases
   must not restart anything. Repeat after browsing another console page.
4. Latch BLACK; press WHITE, UV and a color. Light output must stay dark until
   BLACK is released. Repeat over Priority and STROBE; movement may continue.
5. For WHITE, BLACK and UV: Shift-tap to latch, then press/release the ordinary
   hold. The original latch and corresponding LED must remain. Shift-tap again
   to release. Repeat with mouse holds and with Shift released before the key.
6. Run a clearly patterned tube/Wash chase with dark cells. Latch RED, hold
   BLUE with Shift, then release BLUE. Dark cells must stay dark, the chase
   should keep moving, and the RED latch should return. Repeat over Priority.
7. Add the remaining fixtures by class after confirming mode/address and aim.
   Check Global and Groups 1 IR-4 / 2 Wash / 3 tubes / 4 Focus. Check still and
   moving Priority Looks. Releasing a Look should reveal the advancing loop;
   rapid Look changes should not show a stale previous Look.
8. By mouse and Control One, test every bank: manual latch, physical pad
   release, same-pad off, replacement. During sequential Auto Bank, choose 3
   while 6 plays: it should continue 4, 5, 6. Repeat Auto All and random;
   verify the selected loop starts and random selection continues afterward.
   Change chase speed 0.25x-4x separately from 1/2/4/8/16-measure dwell.
9. With VirtualDJ/audio running through direct OS2L, compare displayed BPM,
   pause/resume playback, seek within a track and change tempo. Observe beat
   timing, controller response and output together. Record any timing jump;
   translator/unit checks cannot establish this physical DJ workflow.
10. Check MOVE and position precedence/release, then STROBE over loops,
    Priority and holds. Watch Focus color/gobo/prism changes for unwanted
    intermediate effects. Inspect sweeps and white/half-time pulses visually.
11. Unplug/replug Control One while a color/WHITE hold is down and a latch is
    established. Holds should release; the established latch should remain.
    Mouse STOP must work with MIDI absent. On reconnect, confirm MIDI, LEDs,
    selected bank/mode and the intended physical output recover without restart.
12. Save/reopen the working show and verify routes persist. Rehearse the exact
    complete DJ/audio/OS2L/controller/USB/DMX setup under sustained load, including
    blackout, STOP and recovery. The project qualification target is two hours.

Record failures with control sequence, fixture/mode/address and observed result.
Review the 128 loops and 32 Priority Looks on the actual fixtures before rating
their appearance. This package does not claim complete SoundSwitch feature
parity, pinned Windows host qualification, physical qualification or gig approval.
'''

RECEIPT = '''V34 INSTALLATION AND PHYSICAL OBSERVATION RECORD
Copy this outside the package before filling it in.
Date/time:
QLC+ version shown and application path:
Original application / test copy / previous show / new working show:
DLL backup paths (both files):
Fixture/profile backup paths; write NEW for files without a prior version:
  Both-Lighting-IR-4-(BOIR4).qxf:
  American-DJ-Focus-Spot-Two.qxf:
  Both-Lighting-BO-TUBE192.qxf:
  Chauvet-Wash-FX-Hex.qxf:
  SoundSwitch-Control-One-Performance.qxi:
Exact hardware output/port and fixture modes/addresses tested:
Native BLACKOUT and STOP observations:
FIRST_TEST step-by-step results and reproduction sequences for failures:
VirtualDJ/audio/OS2L observations:
Combined rehearsal duration and outcome:
Rollback observation:
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(path.read_bytes())


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def source_binding(ci_commit):
    plugin = ROOT / 'qlcplus/plugins/soundswitch'
    suffixes = ('.cpp', '.h', '.json', '.qrc', '.rc', '.def')
    paths = {p.relative_to(ROOT).as_posix() for p in plugin.iterdir() if p.suffix in suffixes}
    committed = git('ls-tree', '-r', '-z', '--name-only', ci_commit, '--',
                    'qlcplus/plugins/soundswitch').decode().split('\0')
    paths.update(name for name in committed if name and Path(name).parent.as_posix() ==
                 'qlcplus/plugins/soundswitch' and Path(name).suffix in suffixes)
    paths = sorted(paths)
    paths += ['qlcplus/plugins/soundswitch/CMakeLists.txt',
              'qlcplus/plugins/soundswitch/standalone/CMakeLists.txt',
              '.github/workflows/soundswitch-plugin.yml',
              'qlcplus/workspace-tools/' + WORKSPACE,
              'qlcplus/input-profiles/' + PROFILE]
    records = {}
    for name in sorted(paths):
        current = (ROOT / name).read_bytes()
        try:
            built = git('cat-file', '--filters', ci_commit + ':' + name)
        except subprocess.CalledProcessError as error:
            raise RuntimeError('CI source unavailable locally; fetch the reported CI commit: ' + ci_commit) from error
        require(current == built, 'Current source differs from CI binary source: ' + name)
        records[name] = digest(current)
    return records


def verify_ci(binary_dir):
    evidence = json.loads((binary_dir / 'build-evidence.json').read_text(encoding='utf-8-sig'))
    require(evidence.get('qlcplusSourceCommit') == PINNED_QLC, 'Wrong QLC+ source target')
    require(evidence.get('qtVersion') == '6.8.1', 'Wrong Qt target')
    require(evidence.get('architecture') == 'windows-x64-mingw', 'Wrong Windows architecture/toolchain')
    require('13.1.0' in evidence.get('compilerVersion', ''), 'Wrong MinGW compiler target')
    require(re.fullmatch(r'[0-9a-f]{40}', evidence.get('repositoryCommit', '')), 'Invalid CI source commit')
    require(str(evidence.get('workflowRun', '')).isdigit(), 'Missing CI workflow run')
    actual = sha(binary_dir / 'soundswitch.dll')
    require(actual == evidence.get('soundswitchSha256'), 'DLL hash does not match CI evidence')
    require(actual != OLD_DLL_SHA, 'Refusing to relabel the old V32 DLL as V34')
    checks = {'protocolTests': 'passed', 'intensityTests': 'passed',
              'autoplaySeekTests': 'passed', 'priorityStateTests': 'passed',
              'midiTranslationTests': 'passed-with-fake-winmm',
              'nativeEffectMaskTests': 'passed', 'pluginLoadSmoke': 'passed-without-hardware'}
    for key, value in checks.items():
        require(evidence.get(key) == value, 'Missing/pending/failed required CI check: ' + key)
    return evidence, source_binding(evidence['repositoryCommit'])


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


def copy_file(source, stage, name):
    require(source.is_file() and not source.is_symlink(), 'Missing or linked package input: ' + str(source))
    target = stage / name
    require(not target.exists(), 'Duplicate package destination: ' + name)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def make_verifier():
    # This is self-contained so the package can be verified after extraction.
    return '''from pathlib import Path
import hashlib, json, xml.etree.ElementTree as ET
root = Path(__file__).resolve().parent
expected = {}
for line in (root/'SHA256SUMS.txt').read_text().splitlines():
    value, name = line.split('  ', 1)
    if name.startswith('/') or '..' in Path(name).parts or name in expected:
        raise SystemExit('Invalid checksum path: '+name)
    expected[name] = value
actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
          and p != root/'SHA256SUMS.txt' and '__pycache__' not in p.parts}
if actual != set(expected): raise SystemExit('Manifest inventory mismatch')
for name, value in expected.items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest() != value:
        raise SystemExit('Checksum mismatch: '+name)
for path in list(root.glob('*.qxw'))+list(root.glob('*.qxi'))+list((root/'Fixtures').glob('*.qxf')):
    ET.parse(path)
record = json.loads((root/'Evidence/package-evidence.json').read_text())
ci = json.loads((root/'Evidence/soundswitch-build-evidence.json').read_text())
assert record['packageVersion'] == 'V34-testing'
assert record['workspaceSha256'] == expected[record['workspace']]
assert record['soundswitchSha256'] == expected['soundswitch.dll'] == ci['soundswitchSha256']
assert record['os2lSha256'] == expected['os2l.dll']
assert record['inputProfileSha256'] == expected[record['inputProfile']]
assert record['dllBuildRepositoryCommit'] == ci['repositoryCommit']
assert record['gigQualified'] is False and record['physicalRigObserved'] is False
bindings = json.loads((root/'Evidence/ci-source-hashes.json').read_text())
assert bindings['files']['qlcplus/workspace-tools/'+record['workspace']] == record['workspaceSha256']
assert bindings['files']['qlcplus/input-profiles/'+record['inputProfile']] == record['inputProfileSha256']
ns = {'q':'http://www.qlcplus.org/FixtureDefinition'}
for name, maker, model, mode, count in ''' + repr(FIXTURES) + ''':
    fixture = ET.parse(root/'Fixtures'/name).getroot()
    assert fixture.findtext('q:Manufacturer',namespaces=ns) == maker
    assert fixture.findtext('q:Model',namespaces=ns) == model
    modes = [m for m in fixture.findall('q:Mode',ns) if m.get('Name') == mode]
    assert len(modes) == 1 and len(modes[0].findall('q:Channel',ns)) == count
profile = ET.parse(root/record['inputProfile']).getroot()
channels = {int(n.get('Number')) for n in profile if n.tag.endswith('}Channel')}
assert {817,818,819} <= channels
print('PASS: complete manifest, component/source bindings, XML and four fixture modes.')
print('V34 testing candidate. Hardware and gig qualification remain pending.')
'''


def build(binary_dir, evidence_dir=None):
    evidence, bound_sources = verify_ci(binary_dir)
    out = ROOT / 'releases/qlcplus-control-one/v34-testing'
    require(not out.exists(), 'Preserve the existing V34 package; choose a new version or archive it explicitly first')
    archive_path = ROOT / (PACKAGE + '.zip')
    checksum_path = ROOT / (PACKAGE + '_SHA256SUMS.txt')
    require(not archive_path.exists() and not checksum_path.exists(), 'V34 ZIP/checksum already exist; refusing silent replacement')
    baseline = ROOT / 'releases/qlcplus-control-one/v32-testing'
    os2l = ROOT / 'releases/qlcplus-control-one/v27/os2l.dll'
    require(sha(os2l) == OS2L_SHA, 'Protected OS2L component changed')
    source_head = git('rev-parse', 'HEAD').decode().strip()
    with tempfile.TemporaryDirectory(prefix='qlc-v34-package-') as temporary:
        stage = Path(temporary) / PACKAGE
        stage.mkdir()
        copy_file(ROOT / 'qlcplus/workspace-tools' / WORKSPACE, stage, WORKSPACE)
        copy_file(binary_dir / 'soundswitch.dll', stage, 'soundswitch.dll')
        copy_file(os2l, stage, 'os2l.dll')
        copy_file(binary_dir / 'build-evidence.json', stage, 'Evidence/soundswitch-build-evidence.json')
        copy_file(ROOT / 'qlcplus/input-profiles' / PROFILE, stage, PROFILE)
        for name, *_ in FIXTURES:
            source = (baseline / 'Fixtures' / name if name in STOCK_FIXTURES
                      else ROOT / 'qlcplus/fixture-definitions' / name)
            if name in STOCK_FIXTURES:
                require(sha(source) == STOCK_FIXTURES[name], 'Pinned stock fixture changed: ' + name)
            copy_file(source, stage, 'Fixtures/' + name)
        copy_file(ROOT / 'qlcplus/fixture-definitions/IR4_DEFINITION_PROVENANCE.md', stage,
                  'Evidence/IR4_DEFINITION_PROVENANCE.md')
        copy_file(ROOT / 'LICENSE', stage, 'Licenses/Integration-LICENSE.txt')
        copy_file(baseline / 'Licenses/QLCPlus-COPYING.txt', stage, 'Licenses/QLCPlus-COPYING.txt')
        for name in ('V32_CONTROL_INDEPENDENT_AUDIT.md', 'V32_CREATIVE_INDEPENDENT_AUDIT.md',
                     'V32_SOUNDSWITCH_PARITY_AUDIT.md'):
            copy_file(ROOT / 'docs/qlcplus-control-one' / name, stage, 'Evidence/Baseline/' + name)
        copy_file(ROOT / 'docs/qlcplus-control-one/V34_TESTING_PACKAGE_RELEASE.md', stage,
                  'Evidence/V34_TESTING_PACKAGE_RELEASE.md')
        local_files = []
        if evidence_dir is not None:
            require(evidence_dir.is_dir(), 'Specified evidence directory is missing')
            for source in sorted(evidence_dir.rglob('*')):
                if source.is_file():
                    require(source.suffix.lower() in ('.txt', '.md', '.json', '.html', '.csv', '.xml', '.log'),
                            'Unexpected evidence file type: ' + str(source))
                    name = 'Evidence/Local/' + source.relative_to(evidence_dir).as_posix()
                    copy_file(source, stage, name)
                    local_files.append(name)
        # Record only commands actually executed successfully for these bytes.
        commands = [
            [sys.executable, 'qlcplus/workspace-tools/Test-V34Workspace.py', '--self-test'],
            [sys.executable, 'qlcplus/workspace-tools/Test-V34CreativeRepairs.py'],
        ]
        logs = []
        for command in commands:
            run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            output = (run.stdout + run.stderr).replace(str(ROOT), '<repository>')
            output = re.sub(r'Ran (\d+) (tests?) in [0-9.]+s',
                            r'Ran \1 \2 (elapsed time omitted for reproducible packaging)', output)
            logs.append('$ ' + ' '.join(['python3', *command[1:]]) + '\n' + output)
            require(run.returncode == 0, 'Structural check failed:\n' + logs[-1])
        regenerated = Path(temporary) / WORKSPACE
        run = subprocess.run([sys.executable, 'qlcplus/workspace-tools/Build-V34Reliability.py',
                              '--output', str(regenerated)], cwd=ROOT, capture_output=True, text=True)
        require(run.returncode == 0, 'V34 deterministic regeneration failed:\n' + run.stdout + run.stderr)
        require(regenerated.read_bytes() == (stage / WORKSPACE).read_bytes(), 'V34 workspace does not match its builder')
        logs.append('PASS: isolated deterministic V34 rebuild is byte-identical to packaged/CI-bound workspace.\n')
        write_text(stage / 'Evidence/structural-validation.txt', '\n'.join(logs))
        write_text(stage / 'START_HERE.txt', START_HERE)
        write_text(stage / 'README.txt', README)
        write_text(stage / 'FIRST_TEST.txt', FIRST_TEST)
        write_text(stage / 'INSTALL_RECEIPT_TEMPLATE.txt', RECEIPT)
        write_text(stage / 'Verify-Package.py', make_verifier())
        write_text(stage / 'Evidence/ci-source-hashes.json', json.dumps({
            'repositoryCommit': evidence['repositoryCommit'],
            'scope': 'Exact Windows plug-in production sources/build files/workflow, workspace and input profile. Test/harness/docs revisions may be newer.',
            'files': bound_sources}, indent=2, sort_keys=True) + '\n')
        write_text(stage / 'Evidence/FIXTURE_PROVENANCE.txt',
                   'IR-4 and Focus: current project definitions, hashes in package-evidence.json.\n'
                   'Tube and Wash: unchanged pinned QLC+ stock definitions bundled in V32.\n'
                   'QLC+ source: ' + PINNED_QLC + '\n'
                   'Stock generic modes are supplied by the matching QLC+ host.\n'
                   'Original author metadata is retained; QLC+ COPYING is included.\n')
        qualification = {
            'packageVersion': 'V34-testing', 'preparedUtc': '2026-09-12',
            'packagingRepositoryCommit': source_head, 'dllBuildRepositoryCommit': evidence['repositoryCommit'],
            'dllBuildWorkflowRun': str(evidence['workflowRun']),
            'dllBuildWorkflowUrl': 'https://github.com/jloops412/qlcplus-soundswitch-integration/actions/runs/' + str(evidence['workflowRun']),
            'requiredQlcSourceCommit': PINNED_QLC, 'requiredQlcUi': '5.3.0 GIT a124abe',
            'requiredQt': '6.8.1', 'requiredCompiler': 'MinGW 13.1.0',
            'requiredArchitecture': 'Windows x64', 'requiredQlcExeSha256': HOST_SHA,
            'workspace': WORKSPACE, 'workspaceSha256': sha(stage / WORKSPACE),
            'inputProfile': PROFILE, 'inputProfileSha256': sha(stage / PROFILE),
            'soundswitchSha256': sha(stage / 'soundswitch.dll'), 'os2lSha256': OS2L_SHA,
            'os2lOrigin': 'Protected V27 package; unchanged matched OS2L binary',
            'fixtureSha256': {name: sha(stage / 'Fixtures' / name) for name, *_ in FIXTURES},
            'structuralEvidence': 'structural-validation.txt: commands executed during packaging; isolated rebuild matches exactly',
            'windowsSoftwareEvidence': 'Original CI build-evidence.json, scoped to its exact repositoryCommit',
            'localEvidenceFiles': local_files,
            'localEvidenceScope': 'Read each included record; inclusion alone is not a passing-test claim',
            'matchedWindowsBinaryBuilt': True, 'pinnedWindowsHostLoaded': False,
            'physicalRigObserved': False, 'gigQualified': False, 'fullSoundSwitchParity': False,
            'privateUniverses': {'3': 'Priority', '4': 'MOVE/STROBE + performance ownership',
                                 '5': 'Color latch', '6': 'Color hold'},
            'remainingChecks': ['Pinned complete Windows host and actual controller/DMX rig',
                                'Physical aim, wheel transitions, visual quality and Priority handoffs',
                                'VirtualDJ timing, pause/seek/tempo changes and sustained audio workload',
                                'Physical MIDI/USB recovery and combined event rehearsal'],
        }
        write_text(stage / 'Evidence/package-evidence.json', json.dumps(qualification, indent=2, sort_keys=True) + '\n')
        payload = sorted(p for p in stage.rglob('*') if p.is_file())
        write_text(stage / 'SHA256SUMS.txt', ''.join(sha(p) + '  ' + p.relative_to(stage).as_posix() + '\n' for p in payload))
        subprocess.run([sys.executable, str(stage / 'Verify-Package.py')], check=True)
        staged_zip = Path(temporary) / archive_path.name
        files = sorted(p for p in stage.rglob('*') if p.is_file())
        with zipfile.ZipFile(staged_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in files:
                info = zipfile.ZipInfo(PACKAGE + '/' + path.relative_to(stage).as_posix(), (2026, 9, 12, 0, 0, 0))
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, path.read_bytes(), compresslevel=9)
        with zipfile.ZipFile(staged_zip) as archive:
            require(archive.testzip() is None, 'ZIP CRC check failed')
            require(len(archive.namelist()) == len(files), 'ZIP inventory mismatch')
            for path in files:
                require(archive.read(PACKAGE + '/' + path.relative_to(stage).as_posix()) == path.read_bytes(),
                        'ZIP/payload bytes differ')
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(stage, out)
        shutil.copyfile(staged_zip, archive_path)
        write_text(checksum_path, sha(archive_path) + '  ' + archive_path.name + '\n')
    print(json.dumps({'zip': str(archive_path), 'bytes': archive_path.stat().st_size,
                      'sha256': sha(archive_path), 'payloadFiles': len(files)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary-dir', required=True, type=Path,
                        help='Downloaded new Windows CI artifact containing soundswitch.dll and build-evidence.json')
    parser.add_argument('--evidence-dir', type=Path,
                        help='Optional actual V34 native/software evidence directory; no pass inferred from inclusion')
    arguments = parser.parse_args()
    build(arguments.binary_dir.resolve(), arguments.evidence_dir.resolve() if arguments.evidence_dir else None)
