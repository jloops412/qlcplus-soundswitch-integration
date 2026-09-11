#!/usr/bin/env python3
"""Deterministic second pass, preserving V31 and all existing public IDs."""
from __future__ import annotations
import argparse
import copy
import hashlib
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import V32CreativeProgram as program
import V31RigIntegrity as rig

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'qlcplus/workspace-tools/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V31-RELIABILITY.qxw'
OUTPUT=SOURCE.with_name('IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw')
SOURCE_SHA='c6e03f865cfeda3fb5c578221ade3ef6883d87c033072a41bf5f1511f7f13651'
q=rig.q
ET.register_namespace('', 'http://www.qlcplus.org/Workspace')

def set_values(fn, frames):
    for value in fn.findall(q('FixtureVal')): fn.remove(value)
    for fid, values in sorted(frames.items()):
        pairs=values.items() if isinstance(values,dict) else enumerate(values)
        ET.SubElement(fn,q('FixtureVal'),{'ID':str(fid)}).text=','.join(str(v) for pair in pairs for v in pair)

def scene(engine,fid,name,frames):
    node=ET.SubElement(engine,q('Function'),{'ID':str(fid),'Type':'Scene','Name':name})
    ET.SubElement(node,q('Speed'),{'FadeIn':'0','FadeOut':'0','Duration':'0'})
    set_values(node,frames)
    return node

def set_timing(fn,fade,duration,steps=None):
    fn.find(q('Speed')).attrib.update(FadeIn=str(fade),FadeOut='0',Duration=str(duration))
    fn.find(q('SpeedModes')).attrib.update(FadeIn='Common',FadeOut='Common',Duration='Common')
    if steps is not None:
        for step in fn.findall(q('Step')): fn.remove(step)
        for index,fid in enumerate(steps):
            ET.SubElement(fn,q('Step'),{'Number':str(index),'FadeIn':str(fade),'Hold':str(duration-fade),'FadeOut':'0'}).text=str(fid)
    else:
        for step in fn.findall(q('Step')):
            step.attrib={'Number':step.get('Number'),'FadeIn':str(fade),'Hold':str(duration-fade),'FadeOut':'0'}

def rewrite_speed_data(root,functions):
    # Preserve every existing widget, input channel and rectangle. Reassign only
    # the hidden speed dial timing tables which necessarily follow the new score.
    dials=[n for n in root.find(q('VirtualConsole')).iter(q('SpeedDial')) if n.get('Caption','').startswith('V30 AUTOLOOP SPEED')]
    groups={}
    for fid in range(532,660):
        for attribute in ('Duration','FadeIn'):
            value=int(functions[fid].find(q('Speed')).get(attribute))
            groups.setdefault((attribute,value),[]).append(fid)
    assert len(groups)<=len(dials)
    for index,dial in enumerate(dials):
        for node in list(dial):
            if node.tag in (q('Function'),q('Preset')): dial.remove(node)
        if index>=len(groups):
            dial.set('Caption','V32 UNUSED SPEED TABLE'); continue
        (attribute,base),ids=sorted(groups.items())[index]
        values=(base*4,base*2,base,base//2,base//4)
        dial.set('Caption',f'V32 AUTOLOOP SPEED {attribute} {base}')
        dial.find(q('Time')).text=str(base)
        dial.find(q('AbsoluteValue')).attrib.update(Minimum=str(min(values)),Maximum=str(max(values)))
        for fid in ids:
            flags={x:'6' if x==attribute else '0' for x in ('FadeIn','FadeOut','Duration')}
            ET.SubElement(dial,q('Function'),flags).text=str(fid)
        for i,value in enumerate(values):
            preset=ET.SubElement(dial,q('Preset'),{'ID':str(i)})
            ET.SubElement(preset,q('Name')).text=('0.25x','0.5x','1x','2x','4x')[i]
            ET.SubElement(preset,q('Value')).text=str(value)
            ET.SubElement(preset,q('Input'),{'Universe':'1','Channel':str(470+i)})

def add_effect_layer(root,functions):
    engine=root.find(q('Engine')); fixtures=rig.id_map(root,'Fixture')
    for old,new in ((9,209),(10,210)):
        fx=copy.deepcopy(fixtures[old]); fx.find(q('ID')).text=str(new); fx.find(q('Universe')).text='3'
        fx.find(q('Name')).text=f'INTERNAL MOVE Focus {"A" if old==9 else "B"}'
        engine.append(fx)
    flags=ET.SubElement(engine,q('Fixture'))
    for tag,value in (('Manufacturer','Generic'),('Model','Generic'),('Mode','3 Channel'),('ID','211'),
                      ('Name','INTERNAL EFFECT FLAGS: MOVE, STROBE, GATE'),('Universe','3'),('Address','0'),('Channels','3')):
        ET.SubElement(flags,q(tag)).text=value
    io=engine.find(q('InputOutputMap'))
    universe=next(u for u in io.findall(q('Universe')) if u.get('ID')=='3')
    if len(universe): raise RuntimeError('Reserved Universe 4 is already routed')
    universe.set('Name','Universe 4 - INTERNAL MOVE + STROBE')
    ET.SubElement(universe,q('Output'),{'Plugin':'SoundSwitch Hardware','Line':'5','UID':'soundswitch:effect-layer','Name':'SoundSwitch Hardware - Native Effect Layer'})
    # A position Flash takes the same native ForceLTP precedence in this layer.
    for fid in range(2175,2184):
        fn=functions[fid]
        for original,mirror in ((9,209),(10,210)):
            value=copy.deepcopy(next(v for v in fn.findall(q('FixtureVal')) if v.get('ID')==str(original)))
            value.set('ID',str(mirror)); fn.append(value)
    # Strobe is intensity multiplication. It never turns on a dark fixture,
    # changes a colour/aim, opens a UV shutter, or uses an independent clock.
    scene(engine,2400,'V32 STROBE - OPEN GATE',{211:{1:255,2:255}})
    scene(engine,2401,'V32 STROBE - CLOSED GATE',{211:{1:255,2:0}})
    set_timing(functions[808],0,500,[2400,2401])
    functions[808].set('Name','PERFORMANCE - NATIVE BEAT STROBE GATE')
    move_ids=[]
    for t in range(16):
        frame={211:{0:255}}
        for side in range(2):
            pan,tilt=program.motion_position('orbit',t,side)
            frame[209+side]={0:pan>>8,1:pan&255,2:tilt>>8,3:tilt&255,16:0}
        fid=2402+t; scene(engine,fid,f'V32 MOVE - SWEEP {t+1:02}',frame); move_ids.append(fid)
    fn=functions[2184]; set_timing(fn,2000,2000,move_ids)
    fn.set('Name','MOVEMENT - NATIVE FOCUS A/B SWEEP')
    # Seed private pan/tilt immediately on step 1. Later steps fade, including
    # the long return to step 1's adjacent pose. No fade from private DMX zero.
    fn.find(q('SpeedModes')).set('FadeIn','PerStep')
    fn.findall(q('Step'))[0].attrib.update(FadeIn='0',Hold='2000')
    for n in root.find(q('VirtualConsole')).iter():
        if n.get('ID')=='1587' and n.tag==q('Button'):
            n.set('Caption','MOVE AUTO - SMOOTH FOCUS SWEEP')
        if n.get('ID')=='1000' and n.tag==q('Label'):
            n.set('Caption','CONTROL ONE - V32 FULL-RIG SHOW\nBEAT PHRASES - FOUR ENERGY BANKS - FULL-RIG PRIORITY')

def build(source=SOURCE,output=OUTPUT):
    data=source.read_bytes()
    if hashlib.sha256(data).hexdigest()!=SOURCE_SHA: raise RuntimeError('V32 requires the exact protected V31 checkpoint')
    root=ET.fromstring(data); engine=root.find(q('Engine')); functions=rig.id_map(root,'Function')
    for fid in range(532,660):
        fn=functions[fid]; old=[int(s.text) for s in fn.findall(q('Step'))]; assert len(old)==8
        ids=old+list(range(3000+(fid-532)*8,3008+(fid-532)*8))
        for t,scene_id in enumerate(ids):
            frame=program.raw_frame(fid,t)
            if t<8: set_values(functions[scene_id],frame)
            else: scene(engine,scene_id,f'V32 {fn.get("Name")} - STEP {t+1:02}',frame)
        fade,duration=program.timing(fid); set_timing(fn,fade,duration,ids)
    for fid in range(5,37):
        fn=functions[fid]
        if fn.get('Type')=='Scene': set_values(fn,program.priority_frame(fid))
        else:
            for t,step in enumerate(fn.findall(q('Step'))): set_values(functions[int(step.text)],program.priority_frame(fid,t,True))
            set_timing(fn,4000,4000)
    rewrite_speed_data(root,functions)
    add_effect_layer(root,functions)
    # Keep Engine element ordering accepted by QLC+ and deterministic.
    for node in sorted(engine.findall(q('Function')),key=lambda x:int(x.get('ID'))): engine.remove(node); engine.append(node)
    ET.indent(root,space='  ')
    text='<?xml version="1.0" encoding="UTF-8"?>\n'+ET.tostring(root,encoding='unicode')+'\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(text.replace('\n','\r\n').encode())
    print(f'Built {output.name}: {hashlib.sha256(output.read_bytes()).hexdigest()}')
    return root

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--source',type=Path,default=SOURCE); parser.add_argument('--output',type=Path,default=OUTPUT)
    args=parser.parse_args(); build(args.source,args.output)
