# 15 more knobs after classic design schools (46-60), same sizes and sheet layout as make_knobs.py
import math, sys, re
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
import make_knobs as K
from make_knobs import bar, circ, poly, scallop, INK, CREAM, WHITE
from make_knobs_schools import defs, rg, lg, wedge
OUT=K.OUT
def clip(i, body): return f'<clipPath id="{i}"><circle r="50"/></clipPath><g clip-path="url(#{i})">{body}</g>'
def pol(r,a): return r*math.sin(math.radians(a)), -r*math.cos(math.radians(a))

def d46(s): # Art Nouveau: sage disc, gold whiplash curve ending in the pointer
    w=f'<path d="M0,-46 C-6,-26 22,-18 14,0 C6,18 -24,10 -22,30" fill="none" stroke="#c9a64b" stroke-width="{10 if s else 6}" stroke-linecap="round"/>'
    return "", circ(50,"#6f8f6a","#3f5a3c",4)+(w if not s else "")+bar(14 if s else 9,20 if not s else 0,46,"#f1dc93")
def d47(s): # Arts & Crafts: hammered copper, dark leaf pointer
    dents="".join(f'<circle cx="{pol(r,a)[0]:.1f}" cy="{pol(r,a)[1]:.1f}" r="7" fill="#fff" opacity=".12"/>' for r in (14,30,42) for a in range(0,360,int(360/(4+r//6)))) if not s else ""
    leaf='M0,-48 C12,-36 12,-18 0,-6 C-12,-18 -12,-36 0,-48 Z'
    return defs(rg("cu","#e7a06b","#8d4a22")), circ(50,"url(#cu)")+dents+f'<path d="{leaf}" fill="#2f3a22"/>'
def d48(s): # Vienna Secession: black & white checker ring, white pointer square
    n=16 if not s else 10; ring=""
    for i in range(n): ring+=wedge(50,360*i/n,360*(i+1)/n, INK if i%2 else WHITE)
    return "", ring+circ(38,INK)+f'<rect x="-7" y="-36" width="14" height="22" fill="{WHITE}"/>'+circ(50,"none",INK,3)
def d49(s): # Suprematism: off-white disc, tilted black square, red bar pointer
    return "", circ(50,"#f1ece1",INK,3)+'<rect x="-14" y="-4" width="26" height="26" fill="#1c1c20" transform="rotate(12)"/>'+f'<rect x="{-6 if s else -4.5}" y="-46" width="{12 if s else 9}" height="40" fill="#d32a22"/>'
def d50(s): # Futurism: speed lines streaming behind a red pointer
    sp="".join(f'<line x1="{x}" y1="-30" x2="{x*0.5}" y2="40" stroke="#ccc" stroke-width="3"/>' for x in (-30,-18,18,30)) if not s else ""
    return "", circ(50,"#2a2b31")+sp+f'<path d="M0,-50 L{-12 if s else -9},10 L{12 if s else 9},10 Z" fill="#e8402a"/>'
def d51(s): # Op Art: black/white rings with a broken sector showing direction
    rings="".join(circ(r, INK if i%2==0 else WHITE) for i,r in enumerate(range(50,0,-8 if not s else -12)))
    return "", rings+wedge(52,-12,12,"#e8402a")+circ(10,INK)
def d52(s): # Pop Art: Ben-Day dots yellow, black outline, red pointer
    dots="".join(f'<circle cx="{x}" cy="{y}" r="3" fill="#e8402a" opacity=".55"/>' for x in range(-48,50,10) for y in range(-48,50,10) if x*x+y*y<2200) if not s else ""
    return "", circ(50,"#ffd93a")+dots+circ(48,"none",INK,5)+bar(16 if s else 12,0,42,INK)+circ(10,INK)
def d53(s): # Zen enso: rice paper disc, brushed ink circle open at the top = pointer
    r=34; g=24
    x,y=pol(r,g)
    return "", circ(50,"#efe8d8","#cfc6b2",2)+f'<path d="M{x:.1f},{y:.1f} A{r},{r} 0 1 1 {-x:.1f},{y:.1f}" fill="none" stroke="{INK}" stroke-width="{16 if s else 11}" stroke-linecap="round"/>'+f'<circle cy="-40" r="{8 if s else 5}" fill="#c0392b"/>'
def d54(s): # Mid-century Eames: walnut rim, white face, black ball pointer
    return "", circ(50,"#5c3a22")+circ(40,"#f4f1ea")+f'<circle cy="-24" r="{13 if s else 9}" fill="{INK}"/>'
def d55(s): # Postmodern: salmon/teal split cap, fluted column rim
    return "", poly(scallop(20 if not s else 12,50,4),"#5ec2b7")+circ(40,"#f4a28c")+wedge(40,-30,30,"#f6e3a1")+bar(12 if s else 8,0,40,"#2b2b2b","butt")
def d56(s): # Streamline Moderne: chrome with three speed bands, black pointer
    bands="".join(f'<rect x="-60" y="{y}" width="120" height="4" fill="#3b3d42"/>' for y in (10,20,30)) if not s else ""
    return defs(lg("ch",[(0,"#fafafa"),(0.45,"#9aa0a8"),(0.55,"#eaecef"),(1,"#6b7078")])), clip(f"sm{s}",circ(50,"url(#ch)")+bands)+circ(50,"none","#555",2)+bar(14 if s else 10,0,46,INK,"butt")
def d57(s): # Brutalism: rough concrete octagon, deep slot
    pts=[pol(50 if i%2 else 47,a) for i,a in enumerate(range(0,360,22))]
    spk="".join(f'<circle cx="{(i*37)%80-40}" cy="{(i*53)%80-40}" r="1.6" fill="#6d6d68"/>' for i in range(28)) if not s else ""
    return "", poly(pts,"#a7a59e","stroke=\"#6d6d68\" stroke-width=\"2\"")+spk+f'<rect x="{-8 if s else -6}" y="-46" width="{16 if s else 12}" height="56" rx="2" fill="#2b2b28"/>'
def d58(s): # Psychedelic: rainbow swirl, white teardrop pointer
    cols=["#e8402a","#f28c28","#f2c12e","#5aa469","#2f7fd6","#8c4bd1"]
    sw="".join(f'<path d="M0,0 Q{pol(40,i*60+30)[0]:.1f},{pol(40,i*60+30)[1]:.1f} {pol(56,i*60+60)[0]:.1f},{pol(56,i*60+60)[1]:.1f} L{pol(56,i*60)[0]:.1f},{pol(56,i*60)[1]:.1f} Z" fill="{c}"/>' for i,c in enumerate(cols))
    return "", clip(f"ps{s}",sw)+circ(50,"none",INK,3)+f'<path d="M0,-46 C8,-30 8,-18 0,-12 C-8,-18 -8,-30 0,-46 Z" fill="{WHITE}" stroke="{INK}" stroke-width="2"/>'
def d59(s): # Googie / atomic: teal disc, boomerang pointer, starburst dot
    star="".join(f'<line x1="0" y1="0" x2="{pol(9,a)[0]:.1f}" y2="{pol(9,a)[1]:.1f}" stroke="#f2c12e" stroke-width="2"/>' for a in range(0,360,45)) if not s else ""
    boom='M0,-48 Q8,-26 22,-14 Q6,-16 0,-4 Q-6,-16 -22,-14 Q-8,-26 0,-48 Z'
    return "", circ(50,"#2e8f8a")+f'<path d="{boom}" fill="#f4f1ea"/>'+f'<g transform="translate(18,22)">{star}</g>'
def d60(s): # Shaker: plain maple peg, small dark walnut dot
    return defs(rg("mp","#f0d9a8","#c99a55")), circ(50,"url(#mp)")+circ(36,"none","#b5874a",2)+f'<circle cy="-38" r="{9 if s else 6}" fill="#4a2c18"/>'

DESIGNS=[
 ("Nouveau","sage, gold whiplash line",d46),
 ("Arts & Crafts","hammered copper, leaf",d47),
 ("Secession","Vienna checker ring",d48),
 ("Suprematist","black square, red bar",d49),
 ("Futurist","speed lines, red arrow",d50),
 ("Op Art","rings, red sector",d51),
 ("Pop Art","Ben-Day dots, black bar",d52),
 ("Ensō","Zen brush circle, red seal",d53),
 ("Mid-century","walnut rim, black ball",d54),
 ("Postmodern","salmon/teal, fluted",d55),
 ("Streamline","chrome speed bands",d56),
 ("Brutalist","concrete, deep slot",d57),
 ("Psychedelic","rainbow swirl, white drop",d58),
 ("Googie","atomic boomerang",d59),
 ("Shaker","plain maple, walnut dot",d60),
]
IDS=("cu","ch","mp")
def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    for gid in IDS:
        st=st.replace(f'id="{gid}"',f'id="{gid}{ident}"'); rot=rot.replace(f"url(#{gid})",f"url(#{gid}{ident})")
    rot=re.sub(r'(id="|url\(#)(sm|ps)(True|False)', lambda m: m.group(1)+m.group(2)+m.group(3)+ident, rot)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')
for i,(name,_,fn) in enumerate(DESIGNS,46):
    slug=re.sub(r'[^a-z]+','-',name.lower().replace('ō','o')).strip('-')
    for sz,mm in K.SIZES:
        open(f"{OUT}/svg/{i:02d}-{slug}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", f"{i}{sz}"))
K.DESIGNS=DESIGNS; K.knob_svg=knob_svg
for t in ("light","dark"):
    h=K.sheet(t).replace("15 designs","Design schools II · designs 46–60")
    h=re.sub(r'<div class="num">(\d+)</div>', lambda m:f'<div class="num">{int(m.group(1))+45}</div>', h)
    open(f"{OUT}/knobs-schools2-{t}.html","w").write(h)
print("ok")
