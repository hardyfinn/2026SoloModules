# 15 industrial-machinery knob designs (16-30), same sizes and sheet layout as make_knobs.py
import math, sys
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
import make_knobs as K
from make_knobs import bar, circ, poly, teeth, scallop, INK, CREAM, GREY, MID, BLUE, ORANGE, TEAL, RED, WHITE
OUT=K.OUT
YEL="#f2c230"; OLIVE="#5b5e3c"; BAKE="#2a1d17"; ALU1="#e6e8ec"; ALU2="#8d929b"

def grad(id_, a, b, kind="radial"):
    if kind=="radial":
        return f'<defs><radialGradient id="{id_}" cx="35%" cy="30%" r="80%"><stop offset="0" stop-color="{a}"/><stop offset="1" stop-color="{b}"/></radialGradient></defs>'
    return f'<defs><linearGradient id="{id_}" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="{a}"/><stop offset=".5" stop-color="{b}"/><stop offset="1" stop-color="{a}"/></linearGradient></defs>'
def ticks(n, r0, r1, col, w, skip_every=None, w2=None, r2=None):
    out=""
    for i in range(n):
        a=360*i/n; big = skip_every and i%skip_every==0
        out+=f'<line x1="0" y1="{-r0}" x2="0" y2="{-(r2 if big and r2 else r1)}" stroke="{col}" stroke-width="{w2 if big and w2 else w}" transform="rotate({a})"/>'
    return out
def lobes(n, R, r):   # clamping knob outline: n round lobes joined by concave waists
    pts=[]
    for i in range(721):
        a=math.radians(i/2); rr=R-(R-r)*(0.5-0.5*math.cos(n*a))**1.6
        pts.append((rr*math.sin(a), -rr*math.cos(a)))
    return pts

def d16(s): # Bakelite fluted
    return grad("bk","#5a3f33",BAKE), poly(scallop(16 if s else 24,50,5),"url(#bk)")+circ(36,BAKE)+bar(14 if s else 10,6,46,CREAM)
def d17(s): # instrument knob on silver dial skirt with 0-10 scale (scale turns with knob, index line fixed above in panel)
    sk=grad("al",ALU1,ALU2)
    nums="".join(f'<text x="0" y="-38" transform="rotate({-135+27*i})" font-size="9" font-weight="800" text-anchor="middle" fill="{INK}" font-family="Inter">{i}</text>' for i in range(0,11,2)) if not s else ""
    return sk, circ(50,"url(#al)")+ticks(36,46,50,INK,1.5)+nums+circ(28,INK)+bar(14 if s else 10,4,50,RED,"butt")
def d18(s): # five-point star clamping knob
    return "", poly(lobes(5,50,30),"#c8302a")+circ(16,"#8f1f1b")+bar(14 if s else 10,0,48,WHITE)
def d19(s): # crank: disc with offset spinner handle that is the pointer
    return grad("cr","#6d7078","#2a2b30"), circ(50,"url(#cr)")+circ(46,"none","#111",2)+f'<circle cy="-30" r="{15 if s else 12}" fill="{YEL}" stroke="#111" stroke-width="3"/>'+circ(6,"#111")
def d20(s): # T-bar / tap handle
    w=34 if s else 28
    return "", circ(30,"#3a3c42")+f'<rect x="{-w/2}" y="-52" width="{w}" height="104" rx="{w/2}" fill="{INK}"/>'+bar(10 if s else 7,6,46,YEL)
def d21(s): # valve handwheel
    sp="".join(f'<line x1="0" y1="0" x2="0" y2="-40" stroke="#b5241f" stroke-width="{12 if s else 9}" transform="rotate({a})"/>' for a in (72,144,216,288))
    return "", f'<circle r="42" fill="none" stroke="#b5241f" stroke-width="{16 if s else 13}"/>'+sp+f'<line x1="0" y1="0" x2="0" y2="-48" stroke="{YEL}" stroke-width="{14 if s else 11}" stroke-linecap="round"/>'+circ(12,"#7c1714")
def d22(s): # lathe micrometer dial
    return grad("lt",ALU1,ALU2,"lin"), circ(50,"url(#lt)")+ticks(50 if not s else 20,42,48,INK,1.6,5,2.6,38)+circ(30,"#a9aeb6",INK,1)+bar(14 if s else 9,0,48,INK,"butt")
def d23(s): # safety yellow
    hz="".join(f'<path d="M{-50+14*i},-50 l7,0 l-30,100 l-7,0 Z" fill="{INK}"/>' for i in range(10))
    return "", f'<clipPath id="hzc{s}"><circle r="50"/></clipPath><g clip-path="url(#hzc{s})">{circ(50,YEL)}{hz}</g>'+circ(36,YEL)+bar(16 if s else 13,0,44,INK,"butt")
def d24(s): # cam-switch selector handle (bar with arrow) on grey round base
    w=28 if s else 22
    return "", circ(50,"#9aa0a8")+f'<path d="M{-w/2},44 L{-w/2},-30 L0,-52 L{w/2},-30 L{w/2},44 Z" fill="{INK}" stroke-linejoin="round"/>'+f'<path d="M0,-44 L-7,-30 L7,-30 Z" fill="{WHITE}"/>'
def d25(s): # Swiss instrument: knurled alu ring, black insert, red line
    return grad("sw",ALU1,ALU2), poly(teeth(30 if not s else 18,50,46),"url(#sw)")+circ(34,INK)+bar(12 if s else 8,4,48,RED,"butt")
def d26(s): # phenolic arrow pointer knob
    p=f'<path d="M0,-54 L22,-20 L22,40 Q22,48 14,48 L-14,48 Q-22,48 -22,40 L-22,-20 Z" fill="{INK}"/>'
    return "", circ(40,"#3a3c42")+p+f'<path d="M0,-44 L-9,-26 L9,-26 Z" fill="{WHITE}"/>'+bar(8 if s else 6,-26,10,WHITE).replace('y1="--26"','y1="26"').replace('y2="-10"','y2="-26"')
def d27(s): # diamond-knurl steel
    n=10; hatch=""
    for i in range(-n,n+1):
        hatch+=f'<line x1="{i*10-60}" y1="-60" x2="{i*10+60}" y2="60" stroke="#55585f" stroke-width="2"/><line x1="{i*10+60}" y1="-60" x2="{i*10-60}" y2="60" stroke="#55585f" stroke-width="2"/>'
    return grad("st","#c7cbd2","#7a7f88"), f'<clipPath id="dk{s}"><circle r="50"/></clipPath><g clip-path="url(#dk{s})">{circ(50,"url(#st)")}{hatch}</g>'+circ(30,"#c9cdd4",INK,2)+bar(14 if s else 10,0,48,INK,"butt")
def d28(s): # three-lobe clamp, blue anodised
    return "", poly(lobes(3,50,34),"#2f5fa8")+circ(20,"#1e3f73")+bar(14 if s else 10,0,48,WHITE)
def d29(s): # olive military
    return "", circ(50,OLIVE)+ticks(12,42,48,CREAM,3)+circ(36,"#44462c")+bar(14 if s else 10,4,48,CREAM,"butt")
def d30(s): # riveted plate
    rv="".join(f'<circle cx="{26*math.sin(math.radians(a))}" cy="{-26*math.cos(math.radians(a))}" r="5" fill="#9aa0a8" stroke="#2a2b30" stroke-width="1.5"/>' for a in (45,135,225,315)) if not s else ""
    return "", circ(50,"#3d4046","#1a1b1f",4)+rv+bar(16 if s else 12,0,46,ORANGE,"butt")

DESIGNS=[
 ("Bakelite","fluted brown-black, cream line",d16),
 ("Dial skirt","silver scale skirt, red line",d17),
 ("Star clamp","5-lobe machine-tool grip",d18),
 ("Crank","yellow spinner = pointer",d19),
 ("T-bar","tap handle with yellow stripe",d20),
 ("Handwheel","valve wheel, yellow spoke",d21),
 ("Micrometer","lathe dial, fine graduations",d22),
 ("Hazard","safety yellow, black bar",d23),
 ("Cam switch","arrow handle on grey base",d24),
 ("Instrument","knurled alu, red line",d25),
 ("Arrowhead","phenolic arrow pointer",d26),
 ("Diamond knurl","cross-hatched steel",d27),
 ("Tri clamp","3-lobe blue anodised",d28),
 ("Army","olive drab, cream ticks",d29),
 ("Riveted","bolted plate, orange bar",d30),
]

def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    for gid in ("bk","al","cr","lt","sw","st"):
        st=st.replace(f'id="{gid}"',f'id="{gid}{ident}"'); rot=rot.replace(f"url(#{gid})",f"url(#{gid}{ident})")
    rot=rot.replace('id="hzc','id="hzc'+ident).replace('url(#hzc','url(#hzc'+ident).replace('id="dk','id="dk'+ident).replace('url(#dk','url(#dk'+ident)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')

for i,(name,_,fn) in enumerate(DESIGNS,16):
    for sz,mm in K.SIZES:
        open(f"{OUT}/svg/{i:02d}-{name.lower().replace(' ','-')}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", f"{i}{sz}"))

# reuse the first sheet's layout
K.DESIGNS=DESIGNS; K.knob_svg=knob_svg
for t in ("light","dark"):
    h=K.sheet(t).replace("15 designs","Industrial · designs 16–30")
    import re
    h=re.sub(r'<div class="num">(\d+)</div>', lambda m:f'<div class="num">{int(m.group(1))+15}</div>', h)
    open(f"{OUT}/knobs-industrial-{t}.html","w").write(h)
print("ok")
