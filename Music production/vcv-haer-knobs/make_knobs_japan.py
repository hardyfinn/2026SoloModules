# 30 Japanese-inspired Haer knobs (211-240): traditional crafts/patterns + modern Japan. Uses kb.py helpers.
import sys, re, math
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
import make_knobs as K
from kb import *
OUT=K.OUT
INK="#1c1c20"; CREAM="#ECE8E0"; WHITE="#ffffff"
AI="#1d2f5e"; SHU="#d9452b"; GOLD="#d4af37"; SAKURA="#f6c1cf"; MATCHA="#7f9c4a"; WASHI="#f3eee2"
def SPECK(n,col,size=1.6,seed=1,r=46,op=1):
    out=""; x=seed
    for i in range(n):
        x=(x*9301+49297)%233280; a=x/233280*360; x=(x*9301+49297)%233280; rr=math.sqrt(x/233280)*r
        px,py=pol(rr,a); out+=C(size,col,cx=px,cy=py,op=op)
    return out
def K_(*parts, st=""): return lambda s: (st, "".join(p(s) if callable(p) else p for p in parts))
def seigaiha(bg, line):
    out=C(60,bg)
    for row,y in enumerate(range(-64,70,8)):
        off=8 if row%2 else 0
        for x in range(-72+off,80,16):
            out+=C(11,bg,cx=x,cy=y)+"".join(f'<circle cx="{x}" cy="{y}" r="{r}" fill="none" stroke="{line}" stroke-width="1.4"/>' for r in (11,7.5,4))
    return CLIP(out)
def shippo(bg,line):
    out=C(60,bg)+"".join(f'<circle cx="{x}" cy="{y}" r="10" fill="none" stroke="{line}" stroke-width="1.6"/>' for x in range(-60,61,10) for y in range(-60,61,10) if (x//10+y//10)%2==0)
    return CLIP(out)
def asanoha(bg,line):
    out=C(60,bg); h=10*math.sqrt(3)
    for j,y in enumerate([k*h for k in range(-4,5)]):
        for x in range(-70,71,20):
            xx=x+(10 if j%2 else 0)
            out+="".join(LINE(xx,y,*(lambda p:(xx+p[0],y+p[1]))(pol(11.5,a)),line,1.2) for a in range(30,360,60))
            out+="".join(LINE(xx,y,*(lambda p:(xx+p[0],y+p[1]))(pol(20,a)),line,0.6) for a in range(0,360,60))
    return CLIP(out)
def maple(r=46):
    rs=[1,.55,.9,.5,.75,.45,.6,.45,.75,.5,.9,.55]
    pts=[pol(r*rs[i%12],i*30) for i in range(12)]
    return pts
def tomoe(r,a,col):
    x,y=pol(r,a); tx,ty=pol(r+14,a+70)
    return C(10,col,cx=x,cy=y)+PATH(f"M{x+pol(10,a+90)[0]:.1f},{y+pol(10,a+90)[1]:.1f} Q{pol(r+12,a+30)[0]:.1f},{pol(r+12,a+30)[1]:.1f} {tx:.1f},{ty:.1f} Q{pol(r,a+40)[0]:.1f},{pol(r,a+40)[1]:.1f} {x+pol(10,a-90)[0]:.1f},{y+pol(10,a-90)[1]:.1f} Z",col)

TRAD=[
 ("Hanko","red seal on paper",K_(C(50,WASHI,"#cfc6b2",2),SPECK(30,"#d8cfbb",1,21),lambda s:RECT(-11 if s else -9,-44,22 if s else 18,22 if s else 18,SHU,2),lambda s:f'<rect x="{-7 if s else -5.5}" y="{-40 if s else -40.5}" width="{14 if s else 11}" height="{14 if s else 11}" fill="none" stroke="#f3eee2" stroke-width="1.5"/>')),
 ("Brush stroke","tapered ink stroke",K_(C(50,WASHI,"#cfc6b2",2),lambda s:PATH(f"M0,-46 C{8 if s else 6},-30 {10 if s else 7},-10 {4},10 L-4,10 C{-10 if s else -7},-10 {-8 if s else -6},-30 0,-46 Z",INK))),
 ("Tomoe","three commas, one red",K_(C(50,"#2a2420"),tomoe(22,0,SHU),tomoe(22,120,CREAM),tomoe(22,240,CREAM))),
 ("Seigaiha","wave scales, gold bar",K_(seigaiha(AI,"#dfe6f2"),RING(49,3,AI),lambda s:BAR(s,GOLD,0,46,10,"butt"))),
 ("Asanoha","hemp-leaf star",K_(asanoha("#5b2333","#f0d9b5"),RING(49,3,"#5b2333"),C(16,"#5b2333"),lambda s:BAR(s,"#f0d9b5",0,46,9))),
 ("Shippō","linked circles",K_(shippo("#1f5e5b","#e8cf86"),RING(49,3,"#1f5e5b"),lambda s:BAR(s,SHU,0,46,9))),
 ("Kintsugi","gold-mended crack",K_(C(50,R("#3a3634","#141212")),PATH("M-44,22 L-26,8 L-12,16 L-4,-2","none",GOLD,2.5),PATH("M20,30 L30,14 L46,10","none",GOLD,2),lambda s:PATH("M0,-48 L-3,-34 L3,-24 L-2,-10 L0,0","none",GOLD,6 if s else 4.5))),
 ("Momiji","red maple leaf points",K_(C(50,"#e9e2cf"),POLY(maple(46),SHU),LINE(0,10,0,24,"#8a2a1a",3))),
 ("Sakura","five petals, white bar",K_(C(50,"#8a2d4a"),SCAL(5,40,12,SAKURA),C(7,"#f7e27a"),lambda s:BAR(s,WHITE,14,48,7))),
 ("Tatami","woven rush, dark border",K_(C(50,"#c9c08a"),CLIP("".join(LINE(-60,y,60,y,"#b3a970",1.4) for y in range(-50,52,4))),RING(46,7,"#2f3b2a"),lambda s:BAR(s,INK,0,40,9,"butt"))),
 ("Vermilion","shrine lacquer, black band",K_(C(50,"#d4401f"),RING(36,8,INK),lambda s:BAR(s,GOLD,0,48,8,"butt"))),
 ("Tea bowl","drippy glaze",K_(C(50,"#d9a98a"),PATH("M-50,-6 C-40,6 -32,-8 -22,8 C-14,20 -6,0 4,10 C14,22 22,2 32,12 C40,20 46,4 50,-6 A50,50 0 0 0 -50,-6 Z","#7a8f8a"),lambda s:BAR(s,WASHI,0,44,8))),
 ("Karesansui","raked sand, stone",K_(C(50,"#e6e1d3"),*[RING(r,1.4,"#b9b2a0") for r in range(8,50,6)],lambda s:PATH("M-12,-34 C-10,-46 10,-48 14,-36 C16,-26 -10,-22 -12,-34 Z","#4a4740"))),
 ("Bamboo","green section, leaf",K_(C(50,"#7fa24a"),RING(38,5,"#5d7e33"),C(30,"#e9e2b8"),lambda s:PATH("M0,-48 C8,-38 8,-24 0,-14 C-8,-24 -8,-38 0,-48 Z","#3f5a20"))),
 ("Chōchin","paper lantern",K_(C(50,"#d93a2a"),CLIP("".join(LINE(-60,y,60,y,"#7a1a12",2) for y in range(-44,46,9))),CLIP(RECT(-50,-50,100,8,INK)+RECT(-50,42,100,8,INK)),lambda s:BAR(s,WASHI,-2,40,8))),
]
MODERN=[
 ("80s hi-fi","fine knurl, blue LED",K_(TEETH(60,50,48,LG("#f4f5f7","#9aa0a8","#e9ebee")),C(40,LG("#e9ebee","#a3a8b0","#f4f5f7")),lambda s:DOT(s,"#3aa0ff",32,8 if s else 5),lambda s:DOT(s,"#3aa0ff",32,12,op=.3))),
 ("City pop","sunset gradient",K_(C(50,LG("#ff8fb1","#ffb36b","#7a5cc8",x2=0,y2=1)),CLIP("".join(RECT(-60,y,120,3,"#7a5cc8",op=.6) for y in (14,22,30,38))),lambda s:BAR(s,WHITE,0,44,8))),
 ("Mecha","panel-lined armour",K_(NGON(8,50,"#f2f2f2",rot=22.5,stroke="#2a2a2a",sw=3),POLY([(0,-46),(16,-18),(-16,-18)],"#2b55c7"),RECT(-18,-10,36,24,"#d9302a",3),RECT(-8,18,16,10,"#f2c12e"),LINE(-34,-20,-20,30,"#2a2a2a",1.5),LINE(34,-20,20,30,"#2a2a2a",1.5))),
 ("Bullet train","white nose, blue line",K_(C(50,"#f4f6f8","#c9ced6",2),PATH("M0,-50 C16,-30 22,0 22,40 L-22,40 C-22,0 -16,-30 0,-50 Z","#ffffff","#c9ced6",1.5),lambda s:BAR(s,"#1f4fb0",-20,46,7,"butt"))),
 ("Tokyo neon","pink and cyan tubes",K_(C(50,"#120a1c"),RING(42,4,"#ff4fc3",.9),RING(42,10,"#ff4fc3",.2),lambda s:GBAR(s,"#4ff0ff",0,34,6))),
 ("Mochi","soft blob, tiny face",K_(PATH("M0,-48 C30,-48 50,-26 50,4 C50,34 28,48 0,48 C-28,48 -50,34 -50,4 C-50,-26 -30,-48 0,-48 Z","#fbf7f2","#e5d9cf",2),C(3.5,INK,cx=-10,cy=-22),C(3.5,INK,cx=10,cy=-22),C(6,"#f6b2c4",cx=-20,cy=-14,op=.8),C(6,"#f6b2c4",cx=20,cy=-14,op=.8),PATH("M-4,-16 Q0,-12 4,-16","none",INK,2))),
 ("Pachinko","chrome ball pointer",K_(C(50,"#e83a7a"),RING(40,2,"#ffd84a",dash="3 4"),lambda s:C(14 if s else 11,R("#ffffff","#7a7f88",30,25),cx=0,cy=-30))),
 ("Capsule toy","half red, half clear",K_(WEDGE(50,-90,90,"#e8402a"),WEDGE(50,90,270,"#cfe8f5",.7),LINE(-50,0,50,0,"#f4f4f4",5),lambda s:DOT(s,WHITE,32,7))),
 ("Vending button","lit ring",K_(C(50,"#c9ced6"),C(38,"#2a2c33"),RING(30,4,"#ff3a3a",.9),lambda s:BAR(s,WHITE,0,26,8))),
 ("Station sign","green band, arrow",K_(C(50,WHITE,"#2a2a2a",2),CLIP(RECT(-50,30,100,20,"#2aa84a")),lambda s:TRI(s,INK,46,22,11),lambda s:BAR(s,INK,0,24,6,"butt"))),
 ("Speed lines","manga burst",K_(C(50,WHITE,INK,3),*[LINE(*pol(18,a),*pol(48,a),INK,1.4 if a%20 else 2.4) for a in range(0,360,10)],lambda s:BAR(s,"#e8402a",0,46,9))),
 ("Personal stereo","blue metallic, orange key",K_(C(50,LG("#7fa6d9","#2f5a9a","#8fb2e0")),RING(44,2,"#d0dbea",.6),lambda s:RECT(-8 if s else -6,-46,16 if s else 12,22,"#ff8a2a",3))),
 ("D-pad","one arm lit",K_(C(50,"#2a2b30"),RECT(-11,-36,22,72,"#45474e",3),RECT(-36,-11,72,22,"#45474e",3),lambda s:RECT(-11,-36,22,24,"#ff3a3a",3),C(5,"#2a2b30"))),
 ("Origami","folded crane facets",K_(C(50,"#d8e6ef"),POLY([(0,-48),(16,0),(0,10)],"#e8402a"),POLY([(0,-48),(-16,0),(0,10)],"#b52b1f"),POLY([(-16,0),(-40,14),(0,10)],"#c93625"),POLY([(16,0),(40,14),(0,10)],"#e04a35"))),
 ("Ramen","naruto swirl, chopsticks",K_(C(50,"#c9302a"),C(42,"#f1d9a8"),C(12,WHITE,cx=-14,cy=14),PATH("M-14,14 m-7,0 a7,7 0 1 1 7,7 a4,4 0 1 1 -4,-4","none","#f06292",2),lambda s:LINE(-4,10,-4,-48,"#6b4a2b",5 if s else 4),lambda s:LINE(4,10,4,-48,"#6b4a2b",5 if s else 4))),
]
def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')
K.knob_svg=knob_svg
n0=211
for title,slug,designs in (("Japan: traditional","japan-trad",TRAD),("Japan: modern","japan-modern",MODERN)):
    assert len(designs)==15
    for i,(name,_,fn) in enumerate(designs,n0):
        sl=re.sub(r'[^a-z0-9]+','-',name.lower().replace('ō','o')).strip('-')
        for sz,mm in K.SIZES:
            open(f"{OUT}/svg/{i:03d}-{sl}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", ""))
    K.DESIGNS=designs
    for t in ("light","dark"):
        h=K.sheet(t).replace("15 designs",f"{title} · designs {n0}–{n0+14}")
        h=re.sub(r'<div class="num">(\d+)</div>', lambda m:f'<div class="num">{int(m.group(1))+n0-1}</div>', h)
        open(f"{OUT}/sheets/knobs-{n0:03d}-{slug}-{t}.html","w").write(h)
    n0+=15
print("ok",n0)
