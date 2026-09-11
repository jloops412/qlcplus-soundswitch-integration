#!/usr/bin/env python3
"""Original full-rig lighting scores. Values are DMX design, not photometry.

The four banks deliberately have different phrase lengths, contrast, and roles.
Fixture IDs come from the checked workspace contract, never fixture names.
Focus aim is an uncalibrated compact envelope near decoded forward presets.
"""
from __future__ import annotations

from dataclasses import dataclass
from math import cos, exp, pi, sin

VERSION = "v32-creative-2026-09-11"
STEPS = 16
# R, G, B, White, Amber, UV; the tube has RGBWA, the Wash has RGBAWUV.
COLORS = {
    "ruby": (255, 0, 24, 0, 0, 0), "red": (255, 0, 0, 0, 0, 0),
    "rose": (255, 12, 62, 0, 0, 0), "coral": (255, 48, 20, 0, 24, 0),
    "amber": (120, 20, 0, 0, 255, 0), "gold": (185, 92, 0, 0, 170, 0),
    "orange": (255, 30, 0, 0, 110, 0), "blush": (150, 16, 32, 90, 35, 0),
    "pink": (255, 0, 94, 0, 0, 0), "hotpink": (255, 0, 145, 0, 0, 0),
    "magenta": (230, 0, 255, 0, 0, 0), "lavender": (90, 24, 180, 40, 0, 0),
    "violet": (100, 0, 255, 0, 0, 0), "purple": (145, 0, 255, 0, 0, 0),
    "uv": (40, 0, 115, 0, 0, 150), "ice": (22, 90, 170, 100, 0, 0),
    "sky": (8, 105, 255, 0, 0, 0), "blue": (0, 12, 255, 0, 0, 0),
    "midnight": (0, 0, 150, 0, 0, 0), "cyan": (0, 225, 255, 0, 0, 0),
    "teal": (0, 195, 115, 0, 0, 0), "green": (0, 255, 12, 0, 0, 0),
    "lime": (90, 255, 0, 0, 30, 0), "warm": (35, 10, 0, 190, 100, 0),
    "ivory": (0, 0, 0, 220, 40, 0), "champagne": (42, 12, 0, 150, 180, 0),
    "white": (0, 0, 0, 255, 0, 0),
}
WHEELS = {
    "ruby":22,"red":22,"rose":82,"coral":124,"amber":124,"gold":67,
    "orange":67,"blush":124,"pink":82,"hotpink":82,"magenta":82,
    "lavender":82,"violet":37,"purple":37,"uv":82,"ice":97,"sky":97,
    "blue":37,"midnight":37,"cyan":97,"teal":112,"green":52,"lime":112,
    "warm":124,"ivory":0,"champagne":124,"white":0,
}

@dataclass(frozen=True)
class Score:
    palette: tuple[str, ...]
    texture: str
    motion: str
    optics: str = "open"

def score(palette, texture, motion="orbit", optics="open"):
    return Score(tuple(palette.split()), texture, motion, optics)

# One intentional score per existing public pad. Ordering and names are retained.
MEDIUM = (
 score("red ruby", "breathe", "drift"), score("blue sky", "tide", "drift"),
 score("gold amber", "breathe", "fan"), score("magenta cyan", "diagonal", "orbit", "g3"),
 score("teal pink", "checker", "fan", "g1"), score("purple gold", "checker", "fan", "g2"),
 score("green blue", "wave", "orbit"), score("blue ice", "pulse", "drift"),
 score("red ruby", "roll", "fan", "g1"), score("blue cyan", "bounce", "orbit"),
 score("pink teal", "bounce", "fan", "g2"), score("warm ice", "halves", "drift"),
 score("amber gold", "inward", "fan"), score("cyan blue", "outward", "fan"),
 score("red green blue", "halves", "fan", "g1"), score("red gold green cyan blue pink", "walk", "orbit"),
 score("gold blue", "pairs", "fan", "g1"), score("purple cyan", "pairs", "fan", "g2"),
 score("amber coral pink", "comet", "orbit", "g1rot"), score("pink cyan lavender", "comet", "fan", "g2prism"),
 score("red gold cyan purple", "corners", "fan", "g1"), score("red gold green cyan blue magenta", "morph", "drift"),
 score("teal cyan", "ripple", "orbit", "g1"), score("magenta pink", "ripple_reverse", "orbit", "g2"),
 score("blue pink white", "accent", "hold"), score("uv purple", "roll", "fan", "g3"),
 score("green lime", "walk", "fan", "g1"), score("orange blue", "halves", "fan", "g2"),
 score("teal cyan gold", "build", "fan"), score("purple blush", "soft_drop", "drift"),
 score("blue cyan", "mirror", "orbit", "g2"), score("pink cyan gold", "phrase", "fan", "g3prism"),
)
COLORFUL = (
 score("red gold green cyan blue pink", "walk", "fan", "g1"),
 score("pink cyan violet", "comet", "orbit", "prism"), score("cyan gold magenta", "bounce", "fan", "prism"),
 score("pink ivory teal", "stripes", "orbit", "g2rot"), score("teal coral gold", "roll", "orbit", "g1"),
 score("lime magenta", "checker", "fan", "g2"), score("red green blue white", "march", "fan"),
 score("teal magenta", "halves", "fan", "g1"), score("red orange gold green cyan violet", "corners", "fan", "g2prism"),
 score("lime gold orange", "syncopated", "fan", "g1"), score("purple pink ruby", "bounce", "orbit", "g2"),
 score("midnight blue teal cyan", "tide", "orbit"), score("ruby red orange gold", "embers", "fan", "g3"),
 score("pink cyan gold lime", "sparkle", "hold", "g2prism"), score("blush lavender ice champagne", "morph", "drift"),
 score("red cyan", "diagonal", "fan", "g1"), score("green magenta", "diagonal_reverse", "fan", "g2"),
 score("blue gold", "pairs", "fan", "g1"), score("red orange gold green cyan blue", "ladder", "fan", "g1"),
 score("red orange gold green cyan blue", "ladder_reverse", "fan", "g2"),
 score("red gold green cyan blue magenta", "comet", "fan"), score("red gold green cyan blue magenta", "comet_reverse", "fan"),
 score("pink white", "sparkle", "hold", "g1prism"), score("teal gold", "sparkle", "hold", "g2"),
 score("magenta cyan lime", "mirror", "orbit", "g3prism"), score("purple gold green", "corners", "fan", "g2"),
 score("hotpink teal", "halves", "drift", "g3"), score("coral gold blue cyan", "morph", "drift"),
 score("lime blue", "ripple", "fan", "g1"), score("violet cyan", "outward", "fan", "g3"),
 score("blue violet pink gold", "build", "fan", "g2"), score("pink cyan gold", "explosion", "hold", "g3prism"),
)
SLOW = (
 score("warm champagne", "breathe", "hold"), score("rose blush", "tide", "drift"),
 score("amber warm", "morph", "drift", "g2soft"), score("amber ice", "halves", "drift"),
 score("midnight ice", "breathe", "hold"), score("coral amber blush", "morph", "drift"),
 score("blush lavender ice", "morph", "drift"), score("amber champagne", "embers", "hold", "g1soft"),
 score("blush gold", "pairs", "drift"), score("lavender blue", "tide", "drift"),
 score("midnight sky", "breathe", "hold"), score("champagne ivory", "breathe", "hold", "g1soft"),
 score("teal ice", "tide", "drift"), score("pink blush", "tide", "drift"),
 score("champagne blush", "outward", "drift"), score("lavender ice", "inward", "drift"),
 score("teal blue", "wave", "drift"), score("coral gold", "morph", "drift"),
 score("purple blush", "breathe", "drift"), score("warm ivory", "breathe", "hold"),
 score("gold champagne", "ripple", "drift", "g1soft"), score("rose blush", "ripple_reverse", "drift", "g1soft"),
 score("blush lavender ice", "outward", "drift"), score("ice lavender blush", "inward", "drift"),
 score("warm lavender ice", "wave", "drift", "g1soft"), score("blue ice", "mirror", "drift", "soft"),
 score("amber champagne", "pairs", "hold", "g1soft"), score("midnight rose", "halves", "hold", "g2soft"),
 score("lavender blush champagne", "build", "drift"), score("rose gold teal sky lavender", "morph", "hold"),
 score("warm rose", "breathe", "hold"), score("champagne ivory", "tide", "hold"),
)
FLASHY = (
 score("white blue", "beat", "hold"), score("red blue", "alternate", "hold"),
 score("pink cyan gold", "pop", "hold"), score("white midnight", "sparkle", "hold", "g1prism"),
 score("uv purple", "syncopated", "hold"), score("gold amber", "beat", "hold", "g1prism"),
 score("blue cyan white", "build_hit", "hold"), score("purple pink white", "drop", "hold", "prism"),
 score("red ruby", "roll_hit", "hold", "g1"), score("blue cyan", "roll_reverse_hit", "hold", "g1"),
 score("red gold green cyan blue magenta", "roll_hit", "hold"), score("white cyan", "outward_hit", "hold"),
 score("white blue", "inward_hit", "hold"), score("white midnight", "checker_hit", "hold"),
 score("white midnight", "halves_hit", "hold"), score("red white blue", "alternate", "hold"),
 score("teal magenta", "pop", "hold", "g1"), score("purple gold", "syncopated", "hold", "g2prism"),
 score("red green blue", "cut", "hold"), score("cyan magenta gold", "cut", "hold", "g3"),
 score("red gold cyan pink", "sparkle", "hold", "g1prism"), score("cyan pink", "bounce_hit", "hold", "g2"),
 score("gold pink", "comet_hit", "hold", "g1prism"), score("gold pink", "comet_reverse_hit", "hold", "g1prism"),
 score("red gold green cyan blue pink", "eighth", "hold"), score("red gold green cyan blue pink", "half", "hold"),
 score("pink cyan gold", "slam", "hold"), score("white pink white blue", "alternate", "hold"),
 score("blue pink white", "build_hit", "hold", "g1prism"), score("gold cyan", "outward_drop", "hold", "g2prism"),
 score("pink blue", "inward_drop", "hold", "g2prism"), score("white gold cyan magenta", "finale", "hold", "g3prism"),
)
BANKS = (MEDIUM, COLORFUL, SLOW, FLASHY)
assert all(len(bank) == 32 for bank in BANKS), [len(bank) for bank in BANKS]
SCORES = {532 + bank*32 + pad: value for bank, entries in enumerate(BANKS) for pad, value in enumerate(entries)}

def clamp(x): return max(0, min(255, round(x)))
def mix(a, b, fraction): return tuple(x*(1-fraction)+y*fraction for x,y in zip(a,b))

def palette_color(s, phase, spatial=0., continuous=False):
    p = (phase + spatial) % len(s.palette)
    a = int(p)
    return mix(COLORS[s.palette[a]], COLORS[s.palette[(a+1)%len(s.palette)]], p-a) if continuous else COLORS[s.palette[a]]

def mask(texture, t, x, index):
    """Spatial rhythm on a normalized rig; 16 equal musical subdivisions."""
    phase = t / STEPS
    travel = (phase*2) % 1
    if texture in {"breathe","pulse"}: return .5+.5*cos(2*pi*(phase*(2 if texture=="pulse" else 1)-.14*x))
    if texture in {"tide","wave","morph"}: return .5+.5*sin(2*pi*(phase-x*.6))
    if texture == "mirror": return .5+.5*sin(2*pi*(phase-abs(x-.5)))
    if texture in {"checker","checker_hit","pairs"}: return float((index//(2 if texture=="pairs" else 1)+t//2)%2==0)
    if texture in {"halves","halves_hit","alternate"}: return float((x<.5)==(t//4%2==0))
    if texture in {"diagonal","diagonal_reverse"}: return float((index+t//2*(1 if texture=="diagonal" else -1))%3==0)
    if texture in {"corners","stripes"}: return float((index+t//2)%4 in (0,3))
    if texture == "build_hit":
        if t < 8: return float(x <= (t+1)/8) * (.2+.8*(t+1)/8)
        return (1,0,.55,0,1,0,1,0)[t-8]
    if texture in {"build","ladder","ladder_reverse"}:
        return float((1-x if texture=="ladder_reverse" else x) <= (t%8+1)/8)
    if texture in {"drop","soft_drop","outward_drop","inward_drop","explosion"}:
        envelope=(.25,.3,.4,.5,.6,.7,.85,.9,0,0,1,1,.6,.4,.25,.15)[t]
        if texture in {"outward_drop","inward_drop","explosion"} and t>=10:
            radius=(t-10)/5
            if texture=="inward_drop": radius=1-radius
            envelope*=exp(-((abs(x-.5)*2-radius)**2)/.09)
        return envelope
    if texture == "embers": return .5+.25*sin(2*pi*(phase+x*2))+.2*sin(2*pi*(phase*3-x))
    if texture == "sparkle": return 1. if (index*7+t*3)%17<3 else .015
    if texture == "accent": return 1. if (index+t)%8==0 else .22
    if texture in {"syncopated","beat","pop","cut","eighth","half","slam","finale"}:
        if texture=="syncopated": return (1,0,0,1,0,0,1,0,1,0,0,1,0,1,0,0)[t]
        if texture=="slam": return float(t%8<4)
        if texture=="half": return float(t%4<2)
        if texture=="finale": return (1,0,1,0,1,0,1,0,1,1,0,0,1,0,1,1)[t]
        if texture=="cut": return float(t%4<3)
        return float(t%2==0)
    if texture=="phrase": return mask(("pairs","roll","outward","accent")[t//4],t,x,index)
    inward = "inward" in texture
    if texture in {"outward","inward","outward_hit","inward_hit"}: x=abs(x-.5)*2; travel=1-travel if inward else travel
    if "reverse" in texture: travel=1-travel
    if "bounce" in texture: travel=1-abs(2*phase-1)
    # Comets and ripples have visible tails; fixture rolls use tight silhouettes.
    distance = abs(x-travel)
    width = .17 if "comet" in texture or "ripple" in texture else .13
    return exp(-distance*distance/(2*width*width))

def timing(fid):
    bank=(fid-532)//32; s=SCORES[fid]
    if bank==0: duration=1000; fade=750 if s.texture not in {"checker","walk","pairs"} else 250
    elif bank==1: duration=500; fade=250 if s.texture not in {"morph","tide"} else 500
    elif bank==2: duration=2000; fade=2000
    else: duration=250 if s.texture not in {"half","slam","build_hit"} else 500; fade=0
    return fade, duration

def motion_position(motion, step, side, scale=1.):
    # No stage-left 65k tilt/wrap path, large source-preset hops, or full circles.
    phase=2*pi*step/STEPS
    center=(40200,2800) if side==0 else (46400,2200)
    if motion=="hold": return center
    width=450 if motion=="drift" else 1400
    height=160 if motion=="drift" else 550
    offset=0 if side==0 else pi
    pan=center[0]+width*scale*sin(phase+offset)
    tilt=center[1]+height*scale*(cos(phase+(offset if motion=="orbit" else 0)))
    return round(pan),round(tilt)

def focus_frame(s, t, side, level, bank, gate=1.):
    pan,tilt=motion_position(s.motion,t,side,.7 if bank==2 else 1.)
    frame=[0]*18
    frame[:4]=[pan>>8,pan&255,tilt>>8,tilt&255]
    # Wheels/optics stay fixed within a phrase; no continuous rainbow wheel spin.
    frame[4]=WHEELS[s.palette[min(side,len(s.palette)-1)]]
    frame[5]=14 if s.optics.startswith("g1") else 23 if s.optics.startswith("g2") else 32 if s.optics.startswith("g3") else 0
    frame[6]=(185 if side==0 else 198) if "rot" in s.optics else 0
    frame[7]=16 if "prism" in s.optics else 0
    # Use dimmer gating with the main shutter fixed open. Real UV remains off.
    frame[8]=8; frame[9]=clamp(level*gate); frame[12]=96 if "soft" in s.optics else 0
    # Native QLC fades own motion speed; a second fixture speed limiter is absent.
    frame[16]=0
    return frame

def raw_frame(fid, t):
    s=SCORES[fid]; bank=(fid-532)//32
    ir_peak,wash_peak,tube_peak,focus_peak=((178,170,215,100),(200,205,235,125),(115,90,130,62),(230,230,255,175))[bank]
    floor=(.12,.045,.70,0.)[bank]
    continuous=bank==2 or s.texture in {"morph","tide","breathe"}
    color_phase=t/STEPS*len(s.palette) if s.texture in {"morph","walk","march","cut","eighth","half","stripes"} else t//8
    frames={}
    def colour(x,index,role):
        # Saturated background; small accent areas on the tubes/Wash supply detail.
        spatial=x*len(s.palette) if s.texture in {"walk","march","stripes","corners","morph","cut"} else (1 if x>.5 else 0)
        c=palette_color(s,color_phase,spatial,continuous)
        if role=="ir" and bank<2: c=palette_color(s,t//8,0 if index%2==0 else 1,False)
        return c
    def gate(x,index,role):
        m=mask(s.texture,t,x,index)
        if bank==2: return floor+(1-floor)*m
        if bank==3:
            if "hit" in s.texture or "comet" in s.texture or "bounce" in s.texture: m*=float(t%2==0)
            return max(0.,m)
        if role=="ir": return .36+.46*mask(s.texture,(t//4)*4,x,index)
        if role=="wash": return .14+.72*m
        return floor+(1-floor)*m
    for i in range(4):
        x=i/3; c=colour(x,i,"ir")
        f=[0]*10; f[0]=clamp(ir_peak*gate(x,i,"ir")); f[1:7]=[clamp(v) for v in c]
        frames[i]=f
    wash=[0]*40
    for zone in range(6):
        c=colour(zone/5,zone,"wash"); amount=wash_peak/255*gate(zone/5,zone,"wash")
        r,g,b,w,a,u=[clamp(v*amount) for v in c]
        wash[4+zone*6:10+zone*6]=[r,g,b,a,w,u]
    frames[4]=wash
    for tube in range(4):
        f=[]
        for pixel in range(8):
            # Sweep across tubes and pixels, with a vertical detail gradient.
            index=tube*8+pixel; x=index/31
            c=colour(x,index,"tube"); amount=tube_peak/255*gate(x,index,"tube")
            c=mix(c,COLORS[s.palette[-1]],.16*(pixel/7))
            r,g,b,w,a,_=[clamp(v*amount) for v in c]
            f.extend([r,g,b,w,a])
        frames[5+tube]=f
    for side in range(2):
        if bank==3: g=gate(float(side),side,"focus")
        elif bank==2: g=.85+.15*sin(2*pi*(t/STEPS+side*.5))
        else: g=.40+.60*mask("halves" if s.texture in {"halves","pairs","checker"} else "breathe",t,float(side),side)
        frames[9+side]=focus_frame(s,t,side,focus_peak,bank,g)
    return frames

PRIORITY = (
 score("ruby red","breathe","hold"), score("red amber","breathe","hold"),
 score("rose blush","breathe","hold"), score("coral champagne","breathe","hold"),
 score("amber warm","breathe","hold"), score("gold champagne","breathe","hold"),
 score("orange amber","breathe","hold"), score("coral amber rose","morph","drift"),
 score("blush champagne","breathe","hold"), score("pink blush","breathe","hold"),
 score("hotpink magenta","breathe","hold"), score("magenta violet","breathe","hold"),
 score("lavender ice","breathe","hold"), score("violet lavender","breathe","hold"),
 score("purple rose","breathe","hold"), score("uv purple","tide","hold"),
 score("ice sky","tide","drift"), score("sky ice","breathe","hold"),
 score("blue sky","breathe","hold"), score("midnight blue","breathe","hold"),
 score("cyan ice","breathe","hold"), score("teal cyan","breathe","hold"),
 score("green lime","breathe","hold"), score("lime gold","breathe","hold"),
 score("amber champagne","embers","hold","g1soft"), score("warm amber","morph","drift","g2soft"),
 score("warm ivory","breathe","hold"), score("champagne ivory","breathe","hold","g1soft"),
 score("amber ice","halves","drift"), score("teal coral gold","morph","drift","g2"),
 score("magenta cyan","pairs","drift","g3"), score("red gold green cyan blue pink","morph","drift","g1"),
)

def priority_frame(fid, step=0, moving=False):
    s=PRIORITY[fid-5]; t=step*2 if moving else 0
    frames={}
    for i in range(4):
        c=COLORS[s.palette[0]] if i in (0,3) else mix(COLORS[s.palette[0]],COLORS[s.palette[-1]],.25)
        f=[0]*10; f[0]=clamp((140 if i in (0,3) else 175)*(.88+.12*sin(t*pi/8+i) if moving else 1))
        f[1:7]=[clamp(v) for v in c]; frames[100+i]=f
    wash=[0]*40
    for zone in range(6):
        c=palette_color(s,t/16*len(s.palette) if moving else 0,zone/5*.8,moving)
        m=.48+.13*cos((zone-2.5)*.8)+(.10*mask(s.texture,t,zone/5,zone) if moving else 0)
        r,g,b,w,a,u=[clamp(v*m) for v in c]; wash[4+zone*6:10+zone*6]=[r,g,b,a,w,u]
    frames[104]=wash
    for tube in range(4):
        f=[]
        for pixel in range(8):
            accent=(pixel/7)**2*.45
            c=mix(COLORS[s.palette[0]],COLORS[s.palette[-1]],accent)
            m=(.4+.32*sin(pi*(pixel+1)/9))*(.87+.13*mask(s.texture,t,(tube*8+pixel)/31,tube*8+pixel) if moving else 1)
            f.extend([clamp(v*m) for v in c[:5]])
        frames[105+tube]=f
    for side in range(2): frames[109+side]=focus_frame(s,t,side,68,2,.90+.1*side)
    return frames
