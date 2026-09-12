from pathlib import Path
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
assert record['workspaceSha256'] == expected[record['workspace']] == ci['workspaceSha256']
assert ci['v34WorkspaceValidation'] == 'passed-with-corruption-tests'
assert record['soundswitchSha256'] == expected['soundswitch.dll'] == ci['soundswitchSha256']
assert record['os2lSha256'] == expected['os2l.dll']
assert record['inputProfileSha256'] == expected[record['inputProfile']] == ci['inputProfileSha256']
assert record['dllBuildRepositoryCommit'] == ci['repositoryCommit']
assert record['gigQualified'] is False and record['physicalRigObserved'] is False
bindings = json.loads((root/'Evidence/ci-source-hashes.json').read_text())
assert bindings['files']['qlcplus/workspace-tools/'+record['workspace']] == record['workspaceSha256']
assert bindings['files']['qlcplus/input-profiles/'+record['inputProfile']] == record['inputProfileSha256']
ns = {'q':'http://www.qlcplus.org/FixtureDefinition'}
for name, maker, model, mode, count in (('Both-Lighting-IR-4-(BOIR4).qxf', 'Both Lighting', 'IR-4 (BOIR4)', '10 Channel', 10), ('American-DJ-Focus-Spot-Two.qxf', 'American DJ', 'Focus Spot Two', '18 Channel', 18), ('Both-Lighting-BO-TUBE192.qxf', 'Both Lighting', 'BO-TUBE192', '40 Channel', 40), ('Chauvet-Wash-FX-Hex.qxf', 'Chauvet', 'Wash FX Hex', '40 Channel', 40)):
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
