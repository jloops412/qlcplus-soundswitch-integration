#!/usr/bin/env python3
"""Offline comparison of programmed colours and phrases from actual workspaces."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import V31RigIntegrity as rig
import V32CreativeProgram as program

ROOT=Path(__file__).resolve().parents[2]; q=rig.q

def rgb(c):
    r,g,b,w,a,u=c
    return [min(255,round(v)) for v in (r+w+.95*a+.3*u,g+w+.4*a,b+w+.65*u)]

def picture(values):
    offset=100 if 100 in values else 0
    ir=[]; tubes=[]; wash=[]; focus=[]; positions=[]
    for i in range(4):
        f=values[offset+i]; ir.append(rgb([f[c]*f[0]/255 for c in range(1,7)]))
    f=values[offset+4]
    for i in range(6):
        p=4+i*6; wash.append(rgb([f[p],f[p+1],f[p+2],f[p+4],f[p+3],f[p+5]]))
    for i in range(4):
        f=values[offset+5+i]; tubes.append([rgb([f[p],f[p+1],f[p+2],f[p+3],f[p+4],0]) for p in range(0,40,5)])
    wheel=[(14,(255,255,255)),(29,(255,0,0)),(44,(0,0,255)),(59,(0,255,0)),(74,(255,230,0)),(89,(255,35,140)),(104,(80,190,255)),(119,(125,255,125)),(127,(255,220,125))]
    for i in range(2):
        f=values[offset+9+i]
        c=next((c for maximum,c in wheel if f[4]<=maximum),(160,160,160))
        focus.append([round(v*f[9]/255) if 8<=f[8]<=15 else 0 for v in c])
        positions.append([f[0]*256+f[1],f[2]*256+f[3]])
    return {'ir':ir,'wash':wash,'tubes':tubes,'focus':focus,'positions':positions}

def read(path):
    root=ET.parse(path).getroot(); f=rig.id_map(root,'Function'); fx=rig.id_map(root,'Fixture')
    result={}
    for fid in list(range(532,660))+list(range(5,37)):
        fn=f[fid]; steps=fn.findall(q('Step')); leaves=[int(s.text) for s in steps] if steps else [fid]
        speed=fn.find(q('Speed'))
        result[fid]={'frames':[picture(rig.parse_values(f[leaf],fx)) for leaf in leaves],
                     'duration':int(speed.get('Duration'))/1000,'fade':int(speed.get('FadeIn'))/1000,'name':fn.get('Name')}
    return result

def export(out):
    source=ROOT/'qlcplus/workspace-tools/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V31-RELIABILITY.qxw'
    candidate=source.with_name('IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw')
    before,after=read(source),read(candidate); catalog=[]
    for fid,item in after.items():
        raw=fid>=532; s=program.SCORES[fid] if raw else program.PRIORITY[fid-5]
        catalog.append({'id':fid,'name':item['name'],'bank':('Medium','Colorful','Slow Dance','Flashy')[(fid-532)//32] if raw else 'Priority Looks',
                        'pad':(fid-532)%32+1 if raw else fid-4,'palette':s.palette,'texture':s.texture,'motion':s.motion,'optics':s.optics,
                        'steps':len(item['frames']),'step_beats':item['duration'],'phrase_measures':len(item['frames'])*item['duration']/4 if len(item['frames'])>1 else None})
    data={'before':before,'after':after,'catalog':catalog,'sha256':hashlib.sha256(candidate.read_bytes()).hexdigest()}
    template=(Path(__file__).parent/'V32ReviewTemplate.html').read_text()
    out.mkdir(parents=True,exist_ok=True)
    (out/'V32_Creative_Review.html').write_text(template.replace('/*SCORE_DATA*/{}',json.dumps(data,separators=(',',':'))))
    (out/'V32_CREATIVE_CATALOG.json').write_text(json.dumps(catalog,indent=2)+'\n')
    lines=['# V32 creative catalog','', 'All 128 Autoloop and 32 Priority pad identities are retained. Values describe programming, not measured output or physical aim.','',
           '| Bank | Pad | Existing look | Palette | Texture | Motion | Optics | Phrase at 1x |','|---|---:|---|---|---|---|---|---|']
    for row in catalog:
        phrase=f'{row["phrase_measures"]:g} measures' if row['phrase_measures'] else 'Still'
        lines.append(f'| {row["bank"]} | {row["pad"]} | {row["name"]} | {", ".join(row["palette"])} | {row["texture"]} | {row["motion"]} | {row["optics"]} | {phrase} |')
    (out/'V32_CREATIVE_CATALOG.md').write_text('\n'.join(lines)+'\n')
    print('Exported 160 before/after scores to '+str(out))

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--output',type=Path,default=ROOT/'docs/qlcplus-control-one/v32-review')
    export(p.parse_args().output)
