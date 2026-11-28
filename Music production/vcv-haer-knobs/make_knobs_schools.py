# 15 knobs after classic design schools (31-45), same sizes and sheet layout as make_knobs.py
import math, sys, re
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
import make_knobs as K
from make_knobs import bar, circ, poly, teeth, scallop, INK, CREAM, GREY, MID, WHITE
OUT=K.OUT
BR="#d8322b"; BY="#f2c12e"; BB="#1f4fa0"   # Bauhaus primaries
RAMS_G="#d9d9d4"; RAMS_O="#f07a1d"; RAMS_GRN="#5aa469"
PINK="#f28ab2"; GOLD="#c9a64b"; TEAK="#9a5b32"

def defs(*g): return "<defs>"+"".join(g)+"</defs>"
def rg(i,a,b,cx="35%",cy="30%",r="80%"): return f'<radialGradient id="{i}" cx="{cx}" cy="{cy}" r="{r}"><stop offset="0" stop-color="{a}"/><stop offset="1" stop-color="{b}"/></radialGradient>'
def lg(i,stops): return f'<linearGradient id="{i}" x1="0" y1="0" x2="1" y2="1">'+"".join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in stops)+'</linearGradient>'
def wedge(r,a0,a1,fill):
    x0,y0=r*math.sin(math.radians(a0)),-r*math.cos(math.radians(a0)); x1,y1=r*math.sin(math.radians(a1)),-r*math.cos(math.radians(a1))
    return f'<path d="M0,0 L{x0:.2f},{y0:.2f} A{r},{r} 0 {1 if a1-a0>180 else 0} 1 {x1:.2f},{y1:.2f} Z" fill="{fill}"/>'

def d31(s): # Bauhaus: red circle, yellow triangle pointer, blue square centre
    return "", circ(50,BR)+f'<path d="M0,-50 L{-15 if s else -12},-14 L{15 if s else 12},-14 Z" fill="{BY}"/>'+f'<rect x="-10" y="-10" width="20" height="20" fill="{BB}"/>'
def d32(s): # Bauhaus half-moon: black disc, yellow half facing the pointer
    return "", circ(50,INK)+wedge(50,-90,90,BY)+bar(14 if s else 10,0,50,INK,"butt")
def d33(s): # Rams radio: light grey cap, fine concentric grooves, single orange line
    gr="".join(f'<circle r="{r}" fill="none" stroke="#c4c4bf" stroke-width="1"/>' for r in range(10,48,5))
    return defs(rg("rm","#f2f2ee",RAMS_G)), circ(50,"url(#rm)","#a9a9a3",2)+gr+bar(12 if s else 8,14,46,RAMS_O,"butt")
def d34(s): # Rams dish: concave white, green dot
    return defs(rg("rd","#c9c9c4","#f7f7f3","62%","68%","70%")), circ(50,"url(#rd)","#9c9c96",2)+f'<circle cy="-33" r="{11 if s else 7}" fill="{RAMS_GRN}"/>'
def d35(s): # Braun calculator: dark cylinder, cream ring, orange cap stripe
    return "", circ(50,"#2d2d2b")+circ(42,"#3b3b38")+bar(14 if s else 10,0,42,RAMS_O,"round")+circ(8,"#2d2d2b")
def d36(s): # Swiss International: black disc, red square at the edge, fine white rim
    q=16 if s else 13
    return "", circ(50,INK)+circ(45,"none",WHITE,1.5)+f'<rect x="{-q/2}" y="-40" width="{q}" height="{q}" fill="#e3261d"/>'
def d37(s): # Starck blob: glossy magenta drop with highlight, the drop tip points
    p='M0,-54 C18,-30 44,-14 44,10 C44,34 24,48 0,48 C-24,48 -44,34 -44,10 C-44,-14 -18,-30 0,-54 Z'
    return defs(rg("sb","#ff7fc8","#a3176a")), f'<path d="{p}" fill="url(#sb)"/>'+'<ellipse cx="-14" cy="0" rx="10" ry="16" fill="#fff" opacity=".35"/>'+bar(12 if s else 8,-6,-38,WHITE).replace('y1="6"','y1="-6"').replace('y2="38"','y2="-38"')
def d38(s): # Starck ghost: translucent clear cap, bar seen inside
    return "", circ(50,"#9fb7c8","#d7e6ef",3).replace('fill="#9fb7c8"','fill="#9fb7c8" fill-opacity=".45"')+circ(40,"none","#ffffff",2).replace('stroke="#ffffff"','stroke="#ffffff" stroke-opacity=".7"')+bar(16 if s else 12,0,44,INK)+circ(6,INK)
def d39(s): # De Stijl: white disc, black grid, red block = pointer
    g=f'<clipPath id="ds"><circle r="50"/></clipPath><g clip-path="url(#ds)">'+circ(50,"#f4f1ea")+f'<rect x="-10" y="-50" width="20" height="36" fill="{BR}"/><rect x="18" y="12" width="40" height="40" fill="{BB}"/><rect x="-60" y="18" width="34" height="40" fill="{BY}"/>'+"".join(f'<line x1="{x}" y1="-60" x2="{x}" y2="60" stroke="{INK}" stroke-width="5"/>' for x in (-12,12))+f'<line x1="-60" y1="-14" x2="60" y2="-14" stroke="{INK}" stroke-width="5"/><line x1="-60" y1="16" x2="60" y2="16" stroke="{INK}" stroke-width="5"/></g>'+circ(50,"none",INK,4)
    return "", g
def d40(s): # Memphis: pink disc, squiggle, yellow triangle pointer
    sq='M-34,14 q8,-10 16,0 t16,0 t16,0 t16,0' if not s else ""
    return "", circ(50,PINK,INK,4)+(f'<path d="{sq}" fill="none" stroke="{INK}" stroke-width="4"/>' if sq else "")+f'<path d="M0,-50 L-14,-18 L14,-18 Z" fill="{BY}" stroke="{INK}" stroke-width="3" stroke-linejoin="round"/>'+f'<circle cx="-22" cy="-14" r="4" fill="{TEAL_}"/>'
TEAL_="#2aa9a6"
def d41(s): # Art Deco: stepped gold rings with sunburst ticks, black pointer
    sun="".join(f'<line x1="0" y1="-22" x2="0" y2="-36" stroke="{GOLD}" stroke-width="2" transform="rotate({a})"/>' for a in range(0,360,15)) if not s else ""
    return defs(rg("dg","#f3dc95","#8a6a22")), circ(50,"url(#dg)")+circ(42,INK)+sun+circ(20,"url(#dg)")+circ(12,INK)+bar(12 if s else 9,22,48,"#f3dc95","butt")
def d42(s): # Scandinavian teak: wood grain, brass dot
    gr="".join(f'<path d="M-60,{y} Q-20,{y+(6 if i%2 else -6)} 0,{y} T60,{y}" fill="none" stroke="#7a4424" stroke-width="2" opacity=".6"/>' for i,y in enumerate(range(-48,52,9)))
    return "", f'<clipPath id="tk"><circle r="50"/></clipPath><g clip-path="url(#tk)">{circ(50,TEAK)}{gr}</g>'+f'<circle cy="-32" r="{12 if s else 8}" fill="#d9b25a" stroke="#6b4a12" stroke-width="2"/>'
def d43(s): # Italian (Olivetti era): soft sculpted orange-red rounded square, sunk groove
    return "", f'<rect x="-46" y="-46" width="92" height="92" rx="34" fill="#e2482f"/>'+f'<rect x="-40" y="-40" width="80" height="80" rx="28" fill="#f05a3c"/>'+bar(14 if s else 10,0,42,"#8f2414")
def d44(s): # Space Age: glossy orange dome, white inset ring, stem pointer
    return defs(rg("sa","#ffb36b","#d4560f")), circ(50,"url(#sa)")+'<ellipse cx="-16" cy="-20" rx="14" ry="9" fill="#fff" opacity=".4"/>'+circ(22,WHITE)+bar(14 if s else 10,0,44,WHITE)+circ(12,"#d4560f")
def d45(s): # Constructivist: black disc, red diagonal wedge pointer, white bar
    return "", circ(50,INK)+wedge(50,-25,25,BR)+circ(26,INK)+bar(12 if s else 8,0,26,WHITE,"butt")

DESIGNS=[
 ("Bauhaus","red circle, yellow triangle",d31),
 ("Half moon","Bauhaus, yellow half points",d32),
 ("Radio","Rams: grooved grey, orange line",d33),
 ("Dish","Rams: concave white, green dot",d34),
 ("Calculator","Braun: dark cap, orange stripe",d35),
 ("Swiss","International: red square",d36),
 ("Blob","Starck: glossy magenta drop",d37),
 ("Ghost","Starck: clear cap, bar inside",d38),
 ("De Stijl","primary blocks, red = pointer",d39),
 ("Memphis","pink, squiggle, yellow tip",d40),
 ("Deco","gold sunburst rings",d41),
 ("Teak","Scandinavian wood, brass dot",d42),
 ("Italian","soft sculpted red-orange",d43),
 ("Space age","glossy orange dome",d44),
 ("Constructivist","black disc, red wedge",d45),
]
IDS=("rm","rd","sb","dg","sa","ds","tk")
def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    for gid in IDS:
        for part in ("st","rot"):
            pass
        st=st.replace(f'id="{gid}"',f'id="{gid}{ident}"'); rot=rot.replace(f'id="{gid}"',f'id="{gid}{ident}"').replace(f"url(#{gid})",f"url(#{gid}{ident})")
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')

for i,(name,_,fn) in enumerate(DESIGNS,31):
    for sz,mm in K.SIZES:
        open(f"{OUT}/svg/{i:02d}-{name.lower().replace(' ','-')}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", f"{i}{sz}"))
K.DESIGNS=DESIGNS; K.knob_svg=knob_svg
for t in ("light","dark"):
    h=K.sheet(t).replace("15 designs","Design schools · designs 31–45")
    h=re.sub(r'<div class="num">(\d+)</div>', lambda m:f'<div class="num">{int(m.group(1))+30}</div>', h)
    open(f"{OUT}/knobs-schools-{t}.html","w").write(h)
print("ok")
