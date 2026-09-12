#!/usr/bin/env python3
"""Build V34 from the immutable V32 checkpoint; QLC+ remains the only runtime."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import V31RigIntegrity as rig
from V34CreativeRepairs import apply_repairs

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name('IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw')
OUTPUT = SOURCE.with_name('IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw')
SOURCE_SHA = '16c676c531aa9aefa354eb2052e0f643be3c27d9e5fa71953deebf99257478c0'
q = rig.q
ET.register_namespace('', 'http://www.qlcplus.org/Workspace')

def flags(engine, fid, universe, address, channels, name):
    fx = ET.SubElement(engine, q('Fixture'))
    for tag, value in [('Manufacturer','Generic'),('Model','Generic'),('Mode',f'{channels} Channel'),
                       ('ID',fid),('Name',name),('Universe',universe),('Address',address),('Channels',channels)]:
        ET.SubElement(fx,q(tag)).text = str(value)

def build(source=SOURCE, output=OUTPUT):
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA:
        raise RuntimeError('V34 requires the exact protected V32 checkpoint')
    root = ET.fromstring(data)
    engine = root.find(q('Engine'))
    fixtures = rig.id_map(root, 'Fixture')
    functions = rig.id_map(root, 'Function')
    vc = root.find(q('VirtualConsole'))
    widgets = {int(n.get('ID')): n for n in vc.iter() if n.get('ID') and n.tag not in (q('Function'),q('Preset'))}
    io = engine.find(q('InputOutputMap'))
    # Physical devices have serial-dependent UIDs. Old numeric fallback can
    # silently patch a private output when USB is absent or enumeration differs.
    # The user chooses the connected device once in QLC+ and saves its real UID.
    physical = next(n for n in io.findall(q('Universe')) if n.get('ID') == '0')
    for node in physical.findall(q('Output')):
        physical.remove(node)
    controller = next(n for n in io.findall(q('Universe')) if n.get('ID') == '1')
    for node in controller.findall(q('Output')):
        controller.remove(node)
    controller.set('Name','Universe 2 - CONTROL ONE INPUT + FEEDBACK ONLY')
    for universe, offset, line, kind in [(4,300,6,'latch'),(5,400,7,'hold')]:
        u = ET.SubElement(io,q('Universe'),{'ID':str(universe),'Name':f'Universe {universe+1} - INTERNAL COLOR {kind.upper()}'})
        out = ET.SubElement(u,q('Output'),{'Plugin':'SoundSwitch Hardware','Line':str(line),
            'UID':f'soundswitch:color-{kind}-layer','Name':f'SoundSwitch Hardware - Color {kind.title()} Layer'})
        ET.SubElement(out,q('PluginParameters'),{'UniverseChannels':'335'})
        for fid in range(11):
            fx = copy.deepcopy(fixtures[fid])
            fx.find(q('ID')).text = str(offset+fid)
            fx.find(q('Universe')).text = str(universe)
            fx.find(q('Name')).text = f'INTERNAL COLOR {kind.upper()} - {fixtures[fid].findtext(q("Name"))}'
            engine.append(fx)
        flags(engine,offset+11,universe,334,1,f'INTERNAL COLOR {kind.upper()} ACTIVE')
        ids = range(37,46) if kind == 'latch' else range(2185,2194)
        for fid in ids:
            fn = functions[fid]
            original = [copy.deepcopy(v) for v in fn.findall(q('FixtureVal')) if int(v.get('ID')) < 11]
            assert len(original) == 11
            for v in fn.findall(q('FixtureVal')):
                fn.remove(v)
            for v in original:
                v.set('ID',str(offset+int(v.get('ID')))); fn.append(v)
            ET.SubElement(fn,q('FixtureVal'),{'ID':str(offset+11)}).text = '0,255'
    flags(engine,212,3,334,3,'INTERNAL PERFORMANCE FLAGS: WHITE, UV, BLACK')
    for fid, marker in [(0,2),(1,0),(2,1)]:
        fn = functions[fid]
        if fid == 0:
            # BLACK suppresses emitters at final output, while underlying motion,
            # color, loops and priority selection continue to advance normally.
            for v in fn.findall(q('FixtureVal')):
                fn.remove(v)
        ET.SubElement(fn,q('FixtureVal'),{'ID':'212'}).text = f'{marker},255'
        latch = copy.deepcopy(fn)
        latch.set('ID',str(4100+fid)); latch.set('Name',fn.get('Name')+' - INDEPENDENT LATCH')
        engine.append(latch)
    for wid, fid in [(1018,4101),(1019,4100),(1020,4102)]:
        widgets[wid].find(q('Function')).set('ID',str(fid))
    command = ET.SubElement(engine,q('Function'),{'ID':'4103','Type':'Scene','Name':'UI CONTROL - RELEASE OVERRIDES AND STOP'})
    ET.SubElement(command,q('Speed'),{'FadeIn':'0','FadeOut':'0','Duration':'0'})
    stop = widgets[1001]
    native_stop = copy.deepcopy(stop)
    native_stop.set('ID','2000'); native_stop.set('Caption','INTERNAL STOP AFTER OVERRIDE RELEASE')
    for n in native_stop.findall(q('Input')):
        native_stop.remove(n)
    ET.SubElement(native_stop,q('Input'),{'Universe':'1','Channel':'818'})
    native_stop.find(q('WindowState')).attrib.update(X='-10000',Y='-10000',Width='1',Height='1')
    widgets[0].append(native_stop)
    stop.find(q('Action')).text = 'Toggle'
    stop.find(q('Function')).set('ID','4103')
    ET.SubElement(stop,q('Input'),{'Universe':'1','Channel':'819','UpperValue':'0','LowerValue':'0','MonitorValue':'0'})
    # Wait for actual native preRun before issuing StopAll: an immediate button
    # press acknowledgement can outrun MasterTimer's pending start queue.
    barrier = copy.deepcopy(stop)
    barrier.set('ID','2001'); barrier.set('Caption','INTERNAL STOP NATIVE START ACKNOWLEDGEMENT')
    for node in barrier.findall(q('Input')):
        barrier.remove(node)
    ET.SubElement(barrier,q('Input'),{'Universe':'1','Channel':'817','UpperValue':'0','LowerValue':'0','MonitorValue':'255'})
    barrier.find(q('WindowState')).attrib.update(X='-10000',Y='-10000',Width='1',Height='1')
    widgets[0].append(barrier)
    blackout = widgets[1015]
    blackout.set('Caption','BLACKOUT')
    blackout.find(q('WindowState')).attrib.update(X='1420',Y='10',Width='160',Height='50')
    blackout.find(q('Appearance')).find(q('BackgroundColor')).text = '4289995578'
    header = widgets[1000]
    header.set('Caption','CONTROL ONE - V34 FULL-RIG SHOW\nSTOP CLEARS OVERRIDES - COLOR KEEPS CHASE DYNAMICS')
    header.find(q('WindowState')).set('Width','1380')
    # Independent offscreen observers carry raw Chaser feedback; runtime
    # disabling a visual container must not suppress the controller state.
    # Their logical channels are never emitted as input.
    for index in range(128):
        visual = widgets[1414+index]
        observer = copy.deepcopy(visual)
        observer.set('ID',str(2100+index))
        observer.set('Caption',f'INTERNAL RAW LOOP FEEDBACK {index+1}')
        observer.find(q('WindowState')).attrib.update(X='-10000',Y='-10000',Width='1',Height='1')
        for node in visual.findall(q('Input')):
            visual.remove(node)
        widgets[0].append(observer)
    # Pinned QML saves but does not restore Frame Disabled. Those tiny status
    # buttons could therefore launch raw Functions outside their playback owner.
    # Native Labels are inert separators; controller LEDs retain live feedback.
    for wid in list(range(1414,1542)) + list(range(1970,1988)):
        strip = widgets[wid]
        strip.tag = q('Label')
        for node in list(strip):
            if node.tag not in (q('WindowState'),q('Appearance')):
                strip.remove(node)
    # Native VCWidget returns after its FIRST feedback input. Surface feedback
    # must precede legacy OS2L bindings, while both retain input operation.
    for button in vc.iter(q('Button')):
        inputs = button.findall(q('Input'))
        if any(n.get('Universe') == '1' for n in inputs):
            for n in inputs:
                button.remove(n)
            for n in sorted(inputs,key=lambda n:n.get('Universe') != '1'):
                button.append(n)
    stats = apply_repairs(root)
    # Engine fixtures must precede Functions for native loadXML resolution.
    for node in sorted(engine.findall(q('Function')),key=lambda n:int(n.get('ID'))):
        engine.remove(node); engine.append(node)
    ET.indent(root,space='  ')
    serialized = '<?xml version="1.0" encoding="UTF-8"?>\n'+ET.tostring(root,encoding='unicode')+'\n'
    output.write_bytes(serialized.replace('\n','\r\n').encode())
    print(json.dumps({'workspace':str(output),'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'creative':stats},indent=2))
    return root

if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=SOURCE);parser.add_argument('--output',type=Path,default=OUTPUT)
    args=parser.parse_args();build(args.source,args.output)
