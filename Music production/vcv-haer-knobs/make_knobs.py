# Generates 15 Haer knob designs as SVG (large 14 mm, medium 10 mm, trimpot 6 mm)
# and an HTML contact sheet (rendered to PNG with Playwright).
import math, os
OUT = "/mnt/project-files/vcv-haer-knobs"
INK="#1c1c20"; CREAM="#ECE8E0"; GREY="#b9bcc4"; MID="#6b6e76"
BLUE="#3d7fd6"; ORANGE="#e8833a"; TEAL="#2aa9a6"; RED="#d9483b"; WHITE="#ffffff"
SIZES=[("large",14.0),("medium",10.0),("trimpot",6.0)]

def bar(w, r0, r1, col, cap="round"):   # pointer bar pointing up (0 deg = 12 o'clock)
    return f'<line x1="0" y1="{-r0}" x2="0" y2="{-r1}" stroke="{col}" stroke-width="{w}" stroke-linecap="{cap}"/>'
def circ(r, fill, stroke=None, sw=0):
    s = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ''
    return f'<circle r="{r}" fill="{fill}"{s}/>'
def poly(pts, fill, extra=""):
    return f'<polygon points="{" ".join(f"{x:.2f},{y:.2f}" for x,y in pts)}" fill="{fill}" {extra}/>'
def teeth(n, ro, ri):
    pts=[]
    for i in range(n*2):
        a=math.pi*i/n; r=ro if i%2==0 else ri
        pts.append((r*math.sin(a), -r*math.cos(a)))
    return pts
def scallop(n, r, depth):
    pts=[]
    for i in range(361):
        a=math.radians(i); rr=r-depth*(0.5-0.5*math.cos(n*a))
        pts.append((rr*math.sin(a), -rr*math.cos(a)))
    return pts

# each design: (name, note, fn(small) -> (static_svg, rotating_svg))
def d1(s):  return "", circ(50,INK)+bar(16 if s else 13,8,44,CREAM)
def d2(s):  return "", circ(46,CREAM,INK,8)+bar(16 if s else 13,6,40,INK)
def d3(s):  return "", circ(50,BLUE)+bar(16 if s else 13,8,44,WHITE)
def d4(s):  # skirted: wide dark skirt with tick marks, raised cap, pointer runs across both
    ticks="".join(f'<line x1="0" y1="-44" x2="0" y2="-49" stroke="{MID}" stroke-width="3" transform="rotate({a})"/>' for a in range(0,360,30))
    return "", circ(50,INK)+ticks+circ(32,"#34353b")+bar(16 if s else 12,10,48,CREAM, "butt")
def d5(s):  return "", poly(teeth(18 if s else 24,50,45),INK)+circ(40,"#2c2d33")+bar(16 if s else 12,8,38,WHITE)
def d6(s):  return "", circ(50,INK)+circ(13 if s else 10,ORANGE) if False else ("", circ(50,INK)+f'<circle cy="-30" r="{14 if s else 10}" fill="{CREAM}"/>')[1]
def d7(s):  # notch: pointer is a cut-out wedge showing an accent under-layer
    w=24 if s else 18
    return "", circ(50,INK)+f'<path d="M0,0 L{-w/2},-52 L{w/2},-52 Z" fill="{ORANGE}"/>'+circ(20,INK)
def d8(s):  # open ring: thick C-ring whose gap is the pointer, with a centre dot
    r=40; g=math.radians(34); sw=18 if s else 15
    x=r*math.sin(g); y=-r*math.cos(g)
    return "", circ(50,"#2c2d33")+f'<path d="M{x:.2f},{y:.2f} A{r},{r} 0 1 1 {-x:.2f},{y:.2f}" fill="none" stroke="{CREAM}" stroke-width="{sw}" stroke-linecap="butt"/>'+f'<line x1="0" y1="-30" x2="0" y2="-52" stroke="{ORANGE}" stroke-width="{12 if s else 9}"/>'
def d9(s):  # chicken-head: whole shape points
    p=f'<path d="M0,-52 C14,-52 30,-12 30,10 C30,36 16,48 0,48 C-16,48 -30,36 -30,10 C-30,-12 -14,-52 0,-52 Z" fill="{INK}"/>'
    return f'<circle r="44" fill="none" stroke="{MID}" stroke-width="2" stroke-dasharray="2 6" opacity="0.0"/>', p+bar(12 if s else 9,0,-42,CREAM).replace('y1="0" ','y1="-8" ').replace('y2="--42"','y2="-44"')
def d10(s): return "", circ(50,INK)+circ(30,TEAL)+bar(16 if s else 12,0,46,CREAM)
def d11(s): # domed, shaded (shading stays still, pointer rotates)
    g=('<defs><radialGradient id="dome" cx="38%" cy="32%" r="75%"><stop offset="0" stop-color="#5a5c64"/>'
       '<stop offset="0.6" stop-color="#26272c"/><stop offset="1" stop-color="#0e0e11"/></radialGradient></defs>')
    return g+circ(50,"url(#dome)")+circ(49,"none","#000",2), bar(14 if s else 10,10,42,WHITE)
def d12(s): # squircle, echoes the blocky Climate Crisis letterforms
    return "", f'<rect x="-46" y="-46" width="92" height="92" rx="26" fill="{INK}"/>'+bar(16 if s else 13,0,40,CREAM,"butt")
def d13(s): # hex
    pts=[(48*math.sin(math.radians(a)), -48*math.cos(math.radians(a))) for a in range(0,360,60)]
    return "", poly(pts,"#2c2d33",f'stroke="{INK}" stroke-width="6" stroke-linejoin="round"')+bar(16 if s else 12,6,40,ORANGE)
def d14(s): # tab: pointer pokes out past the edge as a coloured flag
    tab=f'<rect x="-{11 if s else 8}" y="-58" width="{22 if s else 16}" height="30" rx="4" fill="{BLUE}"/>'
    return "", tab+circ(44,INK)+bar(14 if s else 10,6,36,CREAM)
def d15(s): # scalloped cream cap with ink pointer + orange tip
    return "", poly(scallop(7,50,6),GREY)+circ(34,CREAM)+bar(14 if s else 10,0,44,INK,"butt")+bar(14 if s else 10,32,44,RED,"butt")

DESIGNS=[
 ("Slab","ink disc, chunky cream bar",d1),
 ("Cream cap","cream face, ink rim + bar",d2),
 ("Colour cap","trim colour, white bar",d3),
 ("Skirted","ticked skirt, bar across both",d4),
 ("Knurled","gear edge, white bar",d5),
 ("Dot","ink disc, big cream dot",d6),
 ("Notch","wedge cut shows orange",d7),
 ("Open ring","gap in the ring = pointer",d8),
 ("Chicken head","the whole shape points",d9),
 ("Teal core","two-tone, bar through both",d10),
 ("Dome","shaded, VCV-style",d11),
 ("Block","squircle, after the Hær font",d12),
 ("Hex","hex nut, orange bar",d13),
 ("Tab","blue flag sticks out",d14),
 ("Flower","scalloped grey, red tip",d15),
]
# fix d9 pointer (simpler explicit version)
def d9(s):
    p=f'<path d="M0,-52 C14,-52 30,-12 30,10 C30,36 16,48 0,48 C-16,48 -30,36 -30,10 C-30,-12 -14,-52 0,-52 Z" fill="{INK}"/>'
    return "", p+bar(12 if s else 9,0,-40,CREAM).replace('y2="--40"','y2="-40"')
DESIGNS[8]=("Chicken head","the whole shape points",d9)

def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    st = st.replace('id="dome"', f'id="dome{ident}"').replace("url(#dome)", f"url(#dome{ident})")
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')

# individual files (pointer at 12 o'clock, ready for rollout later)
for i,(name,_,fn) in enumerate(DESIGNS,1):
    for sz,mm in SIZES:
        open(f"{OUT}/svg/{i:02d}-{name.lower().replace(' ','-')}-{sz}.svg","w").write(
            knob_svg(fn, mm*120/100, 0, sz=="trimpot", f"{i}{sz}"))

PX=7.0  # px per mm on sheet (x2 device scale)
def sheet(theme):
    bg, fg, sub, rule = (CREAM, INK, "#6b6a66", BLUE) if theme=="light" else ("#232328", "#ECE8E0", "#9a9aa2", ORANGE)
    cells=[]
    for i,(name,note,fn) in enumerate(DESIGNS,1):
        ks=[]
        for (sz,mm),ang in zip(SIZES,(-135,30,135)):
            w=mm*1.2*PX
            ks.append(f'<div class="k"><div class="lab">{ {"large":"CUTOFF","medium":"LEVEL","trimpot":"CV"}[sz] }</div>'
                      f'<div style="width:{w}px;height:{w}px">{knob_svg(fn,mm,ang,sz=="trimpot",f"{theme}{i}{sz}").replace(f'width="{mm}mm" height="{mm}mm"','width="100%" height="100%"')}</div></div>')
        cells.append(f'<div class="cell"><div class="num">{i}</div><div class="nm">{name}</div><div class="nt">{note}</div><div class="row">{"".join(ks)}</div></div>')
    return f'''<!doctype html><html><head><meta charset="utf-8"><style>
@font-face{{font-family:CC;src:url(file:///mnt/project-files/vcv-haer-logo/fonts/ClimateCrisis.ttf)}}
body{{margin:0;background:{bg};color:{fg};font-family:Inter,sans-serif;width:1500px}}
.hd{{display:flex;align-items:baseline;gap:22px;padding:28px 36px 14px;border-bottom:4px solid {rule}}}
.hd h1{{font-family:CC;font-size:54px;margin:0;font-weight:400}} .hd span{{font-size:20px;color:{sub};font-weight:600}}
.grid{{display:grid;grid-template-columns:repeat(5,1fr);gap:0;padding:10px 20px 26px}}
.cell{{padding:16px 10px 12px;border-bottom:1px solid {sub}44;position:relative}}
.num{{font-family:Inter;font-weight:900;font-size:40px;line-height:1}} .nm{{font-weight:800;font-size:19px;margin-top:4px}}
.nt{{font-size:14px;color:{sub};font-weight:500;height:20px}}
.row{{display:flex;align-items:flex-end;gap:14px;margin-top:10px}}
.k{{display:flex;flex-direction:column;align-items:center}}
.lab{{font-weight:800;font-size:{3.2*PX*0.75:.0f}px;letter-spacing:.5px;margin-bottom:3px}}
</style></head><body><div class="hd"><h1>Hær</h1><span style="font-size:34px;font-weight:800;color:inherit">knobs</span><span>15 designs · large 14 mm · medium 10 mm · trimpot 6 mm · {"light panel" if theme=="light" else "dark panel"} · labels at 3.2 mm</span></div>
<div class="grid">{"".join(cells)}</div></body></html>'''
for t in ("light","dark"):
    open(f"{OUT}/knobs-{t}.html","w").write(sheet(t))
print("ok")
