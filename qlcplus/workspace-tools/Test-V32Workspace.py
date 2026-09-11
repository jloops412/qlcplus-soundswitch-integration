#!/usr/bin/env python3
"""Independent preservation, timing, fixture, and native effects contract checks."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET
import V31RigIntegrity as rig
from importlib.util import spec_from_file_location, module_from_spec

ROOT=Path(__file__).resolve().parents[2]
WS=ROOT/'qlcplus/workspace-tools'
SOURCE=WS/'IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V31-RELIABILITY.qxw'
CANDIDATE=WS/'IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw'
SOURCE_SHA='c6e03f865cfeda3fb5c578221ade3ef6883d87c033072a41bf5f1511f7f13651'
q=rig.q; require=rig.require; semantic=rig.semantic
RAW=set(range(532,660)); PRI=set(range(5,37)); POSITION=set(range(2175,2184))
NEW_SCENES=set(range(2400,2418))|set(range(3000,4024))

def widgets(root):
    nodes=[n for n in root.find(q('VirtualConsole')).iter() if n.get('ID') is not None and n.find(q('WindowState')) is not None]
    result={int(n.get('ID')):n for n in nodes}
    require(len(result)==len(nodes),'duplicate widget IDs')
    return result

def only_values(node):
    n=copy.deepcopy(node)
    for c in n.findall(q('FixtureVal')): n.remove(c)
    return semantic(n)

def validate(before,after):
    old=rig.id_map(before,'Function'); new=rig.id_map(after,'Function')
    original_leaves=rig.scene_leaves(old,RAW|PRI)
    require(set(new)==set(old)|NEW_SCENES,'lost or unexpected Function IDs')
    changed=RAW|PRI|original_leaves|POSITION|{808,2184}
    for fid,fn in old.items():
        require(new[fid].get('Type')==fn.get('Type'),f'public Function type changed: {fid}')
        if fid not in changed: require(semantic(fn)==semantic(new[fid]),f'control/dormant Function changed: {fid}')
        if fid in original_leaves: require(only_values(fn)==only_values(new[fid]),f'Scene non-payload properties changed: {fid}')
        if fid in RAW|PRI: require(fn.get('Name')==new[fid].get('Name'),f'public pad theme changed: {fid}')
    bfx=rig.id_map(before,'Fixture'); afx=rig.id_map(after,'Fixture')
    require(set(afx)==set(bfx)|{209,210,211},'unexpected fixture IDs')
    for fid,fx in bfx.items(): require(semantic(fx)==semantic(afx[fid]),f'original fixture changed: {fid}')
    occupied={}
    for fid,fx in afx.items():
        uni=int(fx.findtext(q('Universe'))); start=int(fx.findtext(q('Address'))); count=int(fx.findtext(q('Channels')))
        require(start>=0 and start+count<=512,f'invalid fixture patch {fid}')
        span=set(range(start,start+count)); used=occupied.setdefault(uni,set())
        require(not used&span,f'overlapping fixture {fid}'); used.update(span)
    for base,mirror in ((9,209),(10,210)):
        for tag in ('Manufacturer','Model','Mode','Address','Channels'):
            require(afx[mirror].findtext(q(tag))==afx[base].findtext(q(tag)),f'MOVE mirror {tag} differs')
        require(afx[mirror].findtext(q('Universe'))=='3','MOVE mirror must stay internal')
    require(tuple(afx[211].findtext(q(x)) for x in ('Manufacturer','Model','Universe','Address','Channels'))==('Generic','Generic','3','0','3'),'native HTP markers invalid')
    bio=before.find(q('Engine')).find(q('InputOutputMap')); aio=after.find(q('Engine')).find(q('InputOutputMap'))
    copy_io=copy.deepcopy(aio)
    require(len({u.get('ID') for u in aio.findall(q('Universe'))})==len(aio.findall(q('Universe'))),'duplicate universe IDs')
    extra=next((n for n in copy_io.findall(q('Universe')) if n.get('ID')=='3'),None)
    require(extra is not None and len(extra)==1,'private effects route missing or exposes input/feedback')
    out=extra[0]
    require(out.tag==q('Output') and out.get('Plugin')=='SoundSwitch Hardware' and out.get('UID')=='soundswitch:effect-layer','effects layer routed outside private plug-in')
    idx=list(copy_io).index(extra); copy_io.remove(extra)
    copy_io.insert(idx,copy.deepcopy(next(u for u in bio.findall(q('Universe')) if u.get('ID')=='3')))
    require(semantic(copy_io)==semantic(bio),'existing routing changed')
    # All existing console objects remain. Only the hidden speed tables and one
    # explanatory MOVE/header captions differ; no input, owner, latch or monitor rewiring.
    bw,aw=widgets(before),widgets(after); require(set(bw)==set(aw),'widget set changed')
    speed_ids={wid for wid,w in bw.items() if w.tag==q('SpeedDial') and w.get('Caption','').startswith('V30 AUTOLOOP SPEED')}
    stripped=copy.deepcopy(after.find(q('VirtualConsole')))
    parents={c:p for p in stripped.iter() for c in p}
    for n in list(stripped.iter()):
        if n.get('ID') is None or n.find(q('WindowState')) is None: continue
        wid=int(n.get('ID'))
        if wid in speed_ids:
            a,b=copy.deepcopy(aw[wid]),copy.deepcopy(bw[wid])
            for w in (a,b):
                w.attrib.pop('Caption',None)
                for child in list(w):
                    if child.tag in {q('Function'),q('Preset'),q('Time'),q('AbsoluteValue')}: w.remove(child)
            require(semantic(a)==semantic(b),f'speed widget geometry/input changed: {wid}')
            parent=parents[n]; idx=list(parent).index(n); parent.remove(n); parent.insert(idx,copy.deepcopy(bw[wid]))
        elif wid in {1000,1587}: n.set('Caption',bw[wid].get('Caption'))
    require(semantic(stripped)==semantic(before.find(q('VirtualConsole'))),'console/control layer changed outside permitted timing/caption data')
    speed_coverage={fid:{'FadeIn':0,'Duration':0} for fid in RAW}
    for wid in speed_ids:
        w=aw[wid]; refs=w.findall(q('Function')); presets=w.findall(q('Preset'))
        if not refs: require(not presets,'unused speed table still dispatches'); continue
        require(len(presets)==5,'missing speed multipliers')
        for ref in refs:
            fid=int(ref.text); require(fid in RAW,'raw speed affects another owner')
            attr='FadeIn' if ref.get('FadeIn')=='6' else 'Duration'
            require(ref.attrib=={x:'6' if x==attr else '0' for x in ('FadeIn','FadeOut','Duration')},'speed attribute mask wrong')
            speed_coverage[fid][attr]+=1
            base=int(new[fid].find(q('Speed')).get(attr)); expected=(base*4,base*2,base,base//2,base//4)
            for i,(preset,value) in enumerate(zip(presets,expected)):
                inp=preset.find(q('Input'))
                require(int(preset.findtext(q('Value')))==value and inp.attrib=={'Universe':'1','Channel':str(470+i)},f'speed mismatch {fid}/{attr}')
    require(all(v=={'FadeIn':1,'Duration':1} for v in speed_coverage.values()),'raw speed coverage not exact')
    raw_leaves=rig.scene_leaves(new,RAW); priority_leaves=rig.scene_leaves(new,PRI)
    require(len(raw_leaves)==2048 and len(priority_leaves)==102,'incorrect live creative closure')
    for fid in PRI:
        require([s.text for s in new[fid].findall(q('Step'))]==[s.text for s in old[fid].findall(q('Step'))],f'Priority component identity/order changed: {fid}')
    values={fid:rig.parse_values(fn,afx) for fid,fn in new.items() if fn.get('Type')=='Scene'}
    # Every reference/value, including dormant Functions, is valid.
    for fid,fn in new.items():
        for step in fn.findall(q('Step')): require(int(step.text) in new,f'broken Function ref {fid}')
    for leaves,expected in ((raw_leaves,set(range(11))),(priority_leaves,set(range(100,111)))):
        for fid in leaves:
            require(set(values[fid])==expected,f'incomplete rig in Scene {fid}')
            for fx,frame in values[fid].items(): require(set(frame)==set(range(int(afx[fx].findtext(q('Channels'))))),f'incomplete frame {fid}/{fx}')
    for fid,frame in values.items():
        for fx,vals in frame.items():
            if fx in {9,10,109,110,209,210}:
                require(all(vals.get(ch,0)==0 for ch in (10,11,13,14,15,17)),f'Focus UV/program/reset enabled {fid}/{fx}')
            if fx in {4,104}: require(all(vals.get(ch,0)==0 for ch in range(4)),f'Wash macro/strobe enabled {fid}')
            if fx in {0,1,2,3,100,101,102,103}: require(all(vals.get(ch,0)==0 for ch in (7,8,9)),f'IR4 internal program enabled {fid}')
    fingerprints=[]; min_contrast=[]
    for fid in sorted(RAW):
        fn=new[fid]; steps=fn.findall(q('Step')); require(len(steps)==16,'raw phrase must have 16 subdivisions')
        require([int(s.text) for s in steps[:8]]==[int(s.text) for s in old[fid].findall(q('Step'))],'original Scene IDs were replaced')
        require(fn.find(q('Tempo')).text=='Beats' and fn.find(q('SpeedModes')).attrib=={'FadeIn':'Common','FadeOut':'Common','Duration':'Common'},'raw timing bypasses speed dial')
        speed=fn.find(q('Speed')); duration=int(speed.get('Duration')); fade=int(speed.get('FadeIn'))
        require(duration in (250,500,1000,2000) and 0<=fade<=duration and speed.get('FadeOut')=='0','non-musical raw timing')
        require(all(int(s.get('FadeIn'))==fade and int(s.get('Hold'))==duration-fade and s.get('FadeOut')=='0' for s in steps),'step timing data disagrees')
        frames=[values[int(s.text)] for s in steps]
        fingerprint=json.dumps(frames,sort_keys=True); fingerprints.append(fingerprint)
        require(len({json.dumps(f,sort_keys=True) for f in frames})>=4,f'loop has too little programmed variation: {fid}')
        for fx in (9,10):
            for ch in (4,5,6,7,12): require(len({f[fx][ch] for f in frames})==1,f'visible optic/wheel indexing within loop {fid}')
            positions=[(f[fx][0]*256+f[fx][1],f[fx][2]*256+f[fx][3]) for f in frames]
            for a,b in zip(positions,positions[1:]+positions[:1]): require(abs(a[0]-b[0])<=600 and abs(a[1]-b[1])<=250,f'mover discontinuity {fid}')
            require(all(1000<=p[1]<=3500 for p in positions),f'large mover tilt {fid}')
        if 596<=fid<628:
            require(duration==2000 and fade==2000,'slow dance cannot have hard rhythmic cuts')
            require(min(f[x][0] for f in frames for x in range(4))>=75,'slow bank blacks out its room bed')
        if fid>=628: require(fade==0,'flashy rhythm must use clean dimmer gates')
    require(len(set(fingerprints))==128,'duplicate raw score')
    for fid in POSITION:
        old_vals=rig.parse_values(old[fid],bfx); v=values[fid]
        require({fx:data for fx,data in v.items() if fx not in {209,210}}==old_vals,'existing calibrated position payload changed')
        require(v[209]==v[9] and v[210]==v[10],'position latch does not override MOVE layer')
    require([int(s.text) for s in new[808].findall(q('Step'))]==[2400,2401],'strobe no longer uses native gate')
    require(values[2400]=={211:{1:255,2:255}} and values[2401]=={211:{1:255,2:0}},'strobe must only own flags and brightness gate')
    require(new[808].find(q('Tempo')).text=='Beats' and new[808].find(q('Speed')).attrib=={'FadeIn':'0','FadeOut':'0','Duration':'500'},'strobe lost native beat timing')
    for fid in range(2402,2418):
        require(set(values[fid])=={209,210,211} and values[fid][211]=={0:255},'MOVE has wrong ownership')
        require(all(set(values[fid][fx])=={0,1,2,3,16} for fx in (209,210)),'MOVE owns non-position channels')
    return {'physical_fixtures':11,'priority_fixtures':11,'effect_fixtures':3,'functions':len(new),'widgets':len(aw),
            'raw_loops':128,'raw_scene_leaves':2048,'priority_looks':32,'priority_scene_leaves':102,
            'protected_native_owners':138,'unchanged_source_functions':len(set(old)-changed),'speed_presets_checked':128*2*5}

def main():
    p=argparse.ArgumentParser(); p.add_argument('--candidate',type=Path,default=CANDIDATE); p.add_argument('--self-test',action='store_true'); p.add_argument('--json',type=Path)
    args=p.parse_args(); require(hashlib.sha256(SOURCE.read_bytes()).hexdigest()==SOURCE_SHA,'V31 source changed')
    before=ET.parse(SOURCE).getroot(); after=ET.parse(args.candidate).getroot(); result=validate(before,after)
    spec=spec_from_file_location('v32builder',WS/'Build-V32Creative.py'); builder=module_from_spec(spec); spec.loader.exec_module(builder)
    with tempfile.TemporaryDirectory() as tmp:
        rebuilt=Path(tmp)/'first.qxw'; second=Path(tmp)/'second.qxw'
        builder.build(output=rebuilt); builder.build(output=second)
        require(rebuilt.read_bytes()==second.read_bytes()==args.candidate.read_bytes(),'checked-in V32 is not deterministic')
    if args.self_test:
        def set_scene(r): rig.id_map(r,'Function')[2401].find(q('FixtureVal')).text='1,255,2,255'
        def set_patch(r): rig.id_map(r,'Fixture')[0].find(q('Address')).text='5'
        def set_owner(r): rig.id_map(r,'Function')[788].find(q('Step')).text='533'
        def set_speed(r): rig.id_map(r,'Function')[532].find(q('SpeedModes')).set('Duration','PerStep')
        def set_layer(r): next(u for u in r.find(q('Engine')).find(q('InputOutputMap')).findall(q('Universe')) if u.get('ID')=='3')[0].set('Plugin','ArtNet')
        def set_control(r): widgets(r)[1004].find(q('Input')).set('Channel','999')
        def set_uv(r):
            f=rig.id_map(r,'Function')[3000]; n=next(n for n in f.findall(q('FixtureVal')) if n.get('ID')=='9'); n.text=n.text.replace('10,0,11,0','10,8,11,255')
        def set_latch(r): rig.id_map(r,'Function')[2175].remove(next(v for v in rig.id_map(r,'Function')[2175].findall(q('FixtureVal')) if v.get('ID')=='209'))
        for mutate in (set_scene,set_patch,set_owner,set_speed,set_layer,set_control,set_uv,set_latch):
            corrupt=copy.deepcopy(after); mutate(corrupt)
            try: validate(before,corrupt)
            except (RuntimeError,KeyError): continue
            raise RuntimeError('corruption was not rejected: '+mutate.__name__)
        result['corruption_cases_rejected']=8
    result['workspace_sha256']=hashlib.sha256(args.candidate.read_bytes()).hexdigest()
    if args.json: args.json.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: V32 structural preservation and creative programming contracts\n'+json.dumps(result,indent=2))

if __name__=='__main__': main()
