from pathlib import Path
import hashlib
import json
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent
expected = {}
for line in (root / 'SHA256SUMS.txt').read_text().splitlines():
    digest, name = line.split('  ', 1)
    if name.startswith('/') or '..' in Path(name).parts or name in expected:
        raise SystemExit('Invalid checksum path: ' + name)
    expected[name] = digest
actual = {str(p.relative_to(root)).replace('\\', '/') for p in root.rglob('*')
          if p.is_file() and p.name != 'SHA256SUMS.txt' and '__pycache__' not in p.parts}
if actual != set(expected):
    raise SystemExit('Package file inventory does not match SHA256SUMS.txt')
for name, digest in expected.items():
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest:
        raise SystemExit('Checksum mismatch: ' + name)
for p in list(root.glob('*.qxw')) + list(root.glob('*.qxi')) + list((root/'Fixtures').glob('*.qxf')):
    ET.parse(p)
record = json.loads((root / 'Evidence/package-evidence.json').read_text())
assert record['packageVersion'] == 'V32-testing' and record['v33Included'] is False
assert record['gigQualified'] is False and record['pinnedWindowsHostLoaded'] is False
assert record['workspaceSha256'] == expected['IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw']
assert record['soundswitchSha256'] == expected['soundswitch.dll']
assert record['os2lSha256'] == expected['os2l.dll']
ci = json.loads((root/'Evidence/soundswitch-build-evidence.json').read_text())
assert ci['soundswitchSha256'] == expected['soundswitch.dll']
ns={'q':'http://www.qlcplus.org/FixtureDefinition'}
for filename, maker, model, mode, channels in [
    ('Both-Lighting-IR-4-(BOIR4).qxf','Both Lighting','IR-4 (BOIR4)','10 Channel',10),
    ('American-DJ-Focus-Spot-Two.qxf','American DJ','Focus Spot Two','18 Channel',18),
    ('Both-Lighting-BO-TUBE192.qxf','Both Lighting','BO-TUBE192','40 Channel',40),
    ('Chauvet-Wash-FX-Hex.qxf','Chauvet','Wash FX Hex','40 Channel',40),
]:
    fixture=ET.parse(root/'Fixtures'/filename).getroot()
    assert fixture.findtext('q:Manufacturer',namespaces=ns)==maker
    assert fixture.findtext('q:Model',namespaces=ns)==model
    matches=[m for m in fixture.findall('q:Mode',ns) if m.attrib['Name']==mode]
    assert len(matches)==1 and len(matches[0].findall('q:Channel',ns))==channels
print('PASS: '+str(len(expected))+' package files match SHA-256, XML parses, all four fixture modes resolve.')
print('V32 testing only. Physical output and gig qualification are pending.')
