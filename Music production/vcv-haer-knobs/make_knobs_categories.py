# Ten more categories of Haer knobs, 15 each (designs 61-210). Uses kb.py helpers.
import sys, re, math
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
import make_knobs as K
from kb import *
from kb_logo import LOGO_D
def LOGO(col,w=72,y=0):
    sc=w/3070
    return f'<g transform="translate(0,{y}) scale({sc:.5f}) translate(-1555,360)">'+''.join(f'<path d="{d}" fill="{col}"/>' for d in LOGO_D)+'</g>'
OUT=K.OUT
INK="#1c1c20"; CREAM="#ECE8E0"; WHITE="#ffffff"; RED="#d9483b"; ORANGE="#e8833a"; YEL="#f2c230"; BLUE="#3d7fd6"
SILVER=LG("#f4f5f7","#a3a8b0","#e9ebee","#7d828b"); CHROME=R("#ffffff","#6b7078",35,28); GOLDG=LG("#f3dc95","#b48a35","#f0d58a","#8a6a22")
def K_(*parts, st=""): return lambda s: (st if isinstance(st,str) else st(s), "".join(p(s) if callable(p) else p for p in parts))
def PIX(step,fill,r=50,cond=None):
    out=""; n=int(r//step)+1
    for i in range(-n,n):
        for j in range(-n,n):
            cx,cy=(i+.5)*step,(j+.5)*step
            if cx*cx+cy*cy<=r*r and (cond is None or cond(cx,cy)): out+=f'<rect x="{i*step}" y="{j*step}" width="{step}" height="{step}" fill="{fill}"/>'
    return out
def SPECK(n,col,size=1.6,seed=1,r=46,op=1):
    out=""; x=seed
    for i in range(n):
        x=(x*9301+49297)%233280; a=x/233280*360; x=(x*9301+49297)%233280; rr=math.sqrt(x/233280)*r
        px,py=pol(rr,a); out+=C(size,col,cx=px,cy=py,op=op)
    return out

CATS=[]
# ---------------- 61-75 Vintage hi-fi and tape decks ----------------
CATS.append(("Vintage hi-fi","hifi",[
 ("Silver skirt","brushed skirt, black cap",K_(C(50,SILVER),TICKS(31,44,49,INK,1.5,-150,150),C(30,INK),lambda s:BAR(s,WHITE,4,48,7,"butt"))),
 ("VU amber","black cap, amber dot",K_(C(50,R("#3a3a3e","#111114")),RING(42,3,"#f2a33a",.5),lambda s:DOT(s,"#ffb547",34,8),lambda s:DOT(s,"#ffb547",34,15,op=.25))),
 ("Chrome dome","engraved black line",K_(C(50,CHROME),RING(49,2,"#555"),lambda s:BAR(s,INK,8,44,6,"butt"))),
 ("Knurled silver","black top, silver line",K_(TEETH(36,50,46,SILVER),C(36,INK),lambda s:BAR(s,"#d6d9de",4,34,7,"butt"))),
 ("Receiver","black, silver ring + dot",K_(C(50,INK),RING(40,3,"#c9ccd2"),lambda s:DOT(s,"#e4e6ea",28,7))),
 ("Transport","flat black, orange line",K_(C(50,"#222226"),TICKS(11,44,49,WHITE,2,-135,135),lambda s:BAR(s,ORANGE,0,40,8,"butt"))),
 ("Spun alu","concentric grooves, red dot",K_(C(50,SILVER),*[RING(r,1,"#ffffff",.6) for r in range(8,48,6)],lambda s:DOT(s,RED,36,7))),
 ("Champagne","gold receiver knob",K_(C(50,GOLDG),RING(48,2,"#6b5016"),lambda s:BAR(s,INK,10,44,6,"butt"))),
 ("Two-tier","chrome skirt, black cap",K_(C(50,CHROME),TICKS(21,43,49,INK,1.5,-135,135),C(32,INK),lambda s:BAR(s,WHITE,0,48,6,"butt"))),
 ("Reel","tape spool, red hole",K_(C(50,"#2b2c30"),C(9,"#bfc3c9"),*[C(11,"#bfc3c9",cx=pol(28,a)[0],cy=pol(28,a)[1]) for a in (120,240)],lambda s:C(13 if s else 11,RED,cx=0,cy=-28))),
 ("Valve amp","black, gold ticks + bar",K_(C(50,INK),TICKS(11,42,48,"#c9a64b",2.5,-135,135),lambda s:BAR(s,"#e2c26b",0,38,8))),
 ("Fluted silver","red stripe",K_(SCAL(24,50,4,SILVER),C(38,"#d9dce1"),lambda s:BAR(s,RED,0,48,7,"butt"))),
 ("Dual concentric","silver ring, black core",K_(C(50,SILVER),lambda s:BAR(s,INK,36,50,8,"butt"),C(30,INK),lambda s:BAR(s,WHITE,0,28,7,"butt"))),
 ("Cassette","spindle hub, white notch",K_(C(50,"#3a3c42"),C(26,WHITE),TEETH(6,22,16,"#3a3c42"),lambda s:BAR(s,WHITE,30,48,9,"butt"))),
 ("Broadcast","grey body, blue cap",K_(C(50,"#8d9199"),C(36,"#2f6fc4"),lambda s:BAR(s,WHITE,0,46,8,"butt"))),
]))
# ---------------- 76-90 Synth heritage ----------------
CATS.append(("Synth heritage","synth",[
 ("Red cap","black body, red cap",K_(C(50,INK),C(28,"#d23a2c"),lambda s:BAR(s,WHITE,0,48,6,"butt"))),
 ("Yellow cap","black body, yellow cap",K_(C(50,INK),C(28,YEL),lambda s:BAR(s,WHITE,0,48,6,"butt"))),
 ("Soft touch","matte rubber, raised ring",K_(C(50,"#3a3b40"),RING(34,4,"#4a4b52"),lambda s:BAR(s,WHITE,6,44,8))),
 ("Silver insert","black, silver top",K_(C(50,INK),C(34,SILVER),lambda s:BAR(s,INK,0,48,6,"butt"))),
 ("Numbered skirt","0-10 skirt ticks",K_(C(50,INK),TICKS(11,40,49,WHITE,2,-150,150),C(30,"#2a2a2e"),lambda s:BAR(s,WHITE,0,40,7,"butt"))),
 ("80s grey","flat grey, white dot",K_(C(50,"#6e7178"),RING(48,2,"#55585e"),lambda s:DOT(s,WHITE,34,7))),
 ("70s cream","cream body, orange stripe",K_(C(50,"#efe6d2","#b9ad94",3),lambda s:BAR(s,ORANGE,0,46,10,"butt"))),
 ("Walnut ring","wood ring, black core",K_(C(50,"#6d4325"),C(38,INK),lambda s:BAR(s,CREAM,0,48,7,"butt"))),
 ("Fader cap","slider cap as pointer",K_(C(50,"#2b2c30"),lambda s:RECT(-9 if s else -7,-48,18 if s else 14,40,"#c9ccd2",3),lambda s:BAR(s,INK,10,46,2.5,"butt"))),
 ("LED encoder","lit dot in LED ring",K_(C(50,"#18181b"),DOTS(15,44,2.6,"#5b2a20",-135,135),C(36,"#2a2b30"),lambda s:DOT(s,"#ff5a3c",44,4),lambda s:DOT(s,"#ff5a3c",44,9,op=.35),lambda s:BAR(s,WHITE,6,32,6))),
 ("Trimmer","blue trimmer, white arm",K_(C(50,"#2459b8"),lambda s:RECT(-38,-5,76,10,"#173d82"),lambda s:BAR(s,WHITE,0,44,10,"butt"))),
 ("Fin filter","big finned, orange line",K_(SCAL(8,50,7,INK),C(30,"#2a2b30"),lambda s:BAR(s,ORANGE,0,46,8,"butt"))),
 ("Patch colours","segmented cap",K_(*[WEDGE(50,a,a+60,c) for a,c in zip(range(0,360,60),["#e8402a","#f2c12e","#5aa469","#2f7fd6","#8c4bd1","#f28c28"])],C(32,INK),lambda s:BAR(s,WHITE,0,48,7,"butt"))),
 ("Translucent","orange glass cap",K_(C(50,"#f28c28",op=.55),RING(48,3,"#f28c28"),C(18,"#2a1608"),lambda s:BAR(s,WHITE,0,44,7))),
 ("Drum cap","square orange cap",K_(RR(44,12,"#e8602a"),lambda s:BAR(s,INK,0,40,8,"butt"))),
]))
# ---------------- 91-105 Aviation & cockpit ----------------
LUM="#b8f5a0"
CATS.append(("Aviation & cockpit","aviation",[
 ("Luminous","black, glow-green line",K_(C(50,INK),DOTS(12,44,2,LUM),lambda s:GBAR(s,LUM,6,40,6))),
 ("Olive panel","olive, white arrow",K_(C(50,"#4f5233"),TICKS(24,44,49,WHITE,1.5),lambda s:TRI(s,WHITE,46,20,10),lambda s:BAR(s,WHITE,0,22,5,"butt"))),
 ("Night lit","red backlit line",K_(C(50,"#121214"),lambda s:GBAR(s,"#ff4a3a",6,44,6))),
 ("Altimeter","needle with arrow tip",K_(C(50,INK),TICKS(36,44,48,WHITE,1.5),TICKS(12,40,48,WHITE,3),lambda s:BAR(s,WHITE,-8,30,4,"butt"),lambda s:TRI(s,WHITE,44,28,7),C(5,"#777"))),
 ("Throttle","fluted grey, yellow stripe",K_(SCAL(14,50,5,"#7b7f86"),lambda s:BAR(s,YEL,0,48,10,"butt"))),
 ("Guarded","knurled black, red guard",K_(RING(48,5,"#c0392b"),TEETH(28,42,38,INK),lambda s:BAR(s,WHITE,0,38,8))),
 ("Dual tuner","stacked grey and black",K_(C(50,"#5a5d63"),lambda s:BAR(s,WHITE,36,50,6,"butt"),TEETH(20,34,31,INK),lambda s:BAR(s,WHITE,0,30,6,"butt"))),
 ("Glow cap","luminous cap, black line",K_(C(50,R("#effbe7","#bfe7b0")),lambda s:BAR(s,INK,0,44,9,"butt"))),
 ("Compass","rose, red north",K_(C(50,"#20252b"),STAR(8,42,14,"#c9ccd2"),lambda s:TRI(s,RED,48,0,12 if s else 9),C(6,INK))),
 ("Horizon","sky/ground split",K_(WEDGE(50,-90,90,"#3a7bd5"),WEDGE(50,90,270,"#8a5a2b"),LINE(-50,0,50,0,WHITE,3),lambda s:BAR(s,WHITE,0,46,7,"butt"))),
 ("Bezel","screws, white needle",K_(C(50,"#6b6f76"),*[C(4,"#2a2b30",cx=pol(44,a)[0],cy=pol(44,a)[1]) for a in (45,135,225,315)],C(36,INK),lambda s:BAR(s,WHITE,0,34,6),lambda s:TRI(s,WHITE,36,22,6))),
 ("T-handle","lum stripe",K_(C(32,"#3a3c42"),lambda s:RECT(-14 if s else -12,-52,28 if s else 24,104,INK,12),lambda s:BAR(s,LUM,4,44,6,"butt"))),
 ("Pull knob","red centre, white ring",K_(C(50,INK),RING(32,5,WHITE),C(14,RED),lambda s:BAR(s,WHITE,36,48,8,"butt"))),
 ("Radar","sweep = pointer",K_(C(50,"#0d2a1a"),RING(32,1,"#3ddc84",.5),RING(16,1,"#3ddc84",.5),WEDGE(48,-40,0,"#3ddc84",.35),lambda s:BAR(s,"#7dffb0",0,48,4,"butt"))),
 ("Trim wheel","ridged, white mark",K_(TEETH(40,50,46,INK),C(44,"#26272b"),lambda s:BAR(s,WHITE,18,50,9,"butt"))),
]))
# ---------------- 106-120 Nature and materials ----------------
def VEINS(col,w=1.5,op=.6): return "".join(PATH(d,"none",col,w,op) for d in ("M-50,-20 C-20,-30 0,0 50,-10","M-40,30 C-10,10 10,40 50,20","M-10,-50 C0,-20 -20,10 10,50"))
CATS.append(("Nature & materials","materials",[
 ("Marble","white veins, gold line",K_(C(50,"#f2f0ec"),CLIP(VEINS("#9a9a9a")),RING(49,1.5,"#bdbab3"),lambda s:BAR(s,"#c9a64b",4,44,7,"butt"))),
 ("Slate","layered, chalk line",K_(C(50,"#3b4148"),CLIP("".join(LINE(-60,y,60,y+6,"#4b525a",3) for y in range(-50,60,9))),lambda s:BAR(s,"#f1efe8",0,44,9))),
 ("Cork","speckled tan",K_(C(50,"#c79a62"),SPECK(70,"#8a5e30",1.8,3),lambda s:DOT(s,"#3a2412",32,8))),
 ("Bone","soft ivory",K_(C(50,R("#fbf7ec","#d8ceb5")),lambda s:BAR(s,INK,6,44,7))),
 ("Amber resin","inclusions, dark bar",K_(C(50,R("#ffcf6e","#c0701a"),op=.9),SPECK(14,"#6b3a10",2.2,7,40,.6),lambda s:BAR(s,"#3a1d06",0,44,8))),
 ("Pebble","river stone",K_(PATH("M0,-50 C30,-50 50,-26 48,4 C46,34 26,50 0,50 C-30,50 -50,30 -48,0 C-46,-30 -28,-50 0,-50 Z",R("#a4a7a6","#5e6262")),lambda s:DOT(s,WHITE,32,8))),
 ("Granite","pink grey speckle",K_(C(50,"#b9a8a3"),SPECK(90,"#3c3434",1.5,5),SPECK(40,"#f1e6e0",1.5,9),lambda s:BAR(s,INK,0,44,9,"butt"))),
 ("End grain","tree rings",K_(C(50,"#c48a52"),*[RING(r,1.5,"#8a5a2e",.7) for r in (8,15,21,28,34,40,46)],lambda s:BAR(s,"#3a2412",0,48,7,"butt"))),
 ("Verdigris","patina, copper bar",K_(C(50,"#5fa58f"),SPECK(30,"#3d7f6c",3,4,44,.6),lambda s:BAR(s,"#c26b35",0,46,10,"butt"))),
 ("Obsidian","glossy black",K_(C(50,R("#4a4a52","#050506",30,25)),PATH("M-30,-30 Q-10,-46 14,-42","none","#ffffff",3,.35),lambda s:BAR(s,WHITE,8,44,6))),
 ("Jade","green, gold dot",K_(C(50,R("#8fd1a5","#2e7a51")),lambda s:DOT(s,"#e4c25a",32,8))),
 ("Sandstone","banded",K_(CLIP("".join(RECT(-60,y,120,8,c) for y,c in zip(range(-56,60,8),["#d98e55","#e9a66b","#c97b44","#eab57f","#d48b52"]*4))),lambda s:BAR(s,"#3a2412",0,44,9,"butt"))),
 ("Terracotta","matte clay, cream line",K_(C(50,"#c3653f"),RING(46,2,"#a3502f"),lambda s:BAR(s,CREAM,0,44,9))),
 ("Lichen","moss spots",K_(C(50,"#5e6b45"),SPECK(18,"#b3c27a",4,2,42,.8),lambda s:BAR(s,CREAM,0,44,9))),
 ("Quartz","pale facets",K_(NGON(6,50,"#cfe6f2",rot=0),*[POLY([(0,0),pol(50,a),pol(50,a+60)],c) for a,c in zip(range(0,360,60),["#e7f4fa","#bcdcec","#d6ecf6","#a9d0e4","#e1f1f8","#c3e0ee"])],lambda s:BAR(s,"#1d3d55",0,44,8,"butt"))),
]))
# ---------------- 121-135 Toys and candy ----------------
CATS.append(("Toys & candy","toys",[
 ("Brick stud","red, stud = pointer",K_(C(50,"#d22f27"),lambda s:C(15 if s else 12,"#e94a40","#a8221c",2,cx=0,cy=-26))),
 ("Jelly bean","glossy pink bean",K_(PATH("M0,-54 C26,-54 34,-20 30,10 C26,40 12,50 0,50 C-12,50 -26,40 -30,10 C-34,-20 -26,-54 0,-54 Z",R("#ff9ccc","#c4337a")),PATH("M-16,-30 Q-8,-44 4,-44","none","#fff",5,.6))),
 ("Wood block","natural, red dot",K_(RR(44,10,"#e3c38f"),RING(40,0,"#000",0),lambda s:DOT(s,"#d22f27",28,9))),
 ("Gumball","glossy blue",K_(C(50,R("#7fb5ff","#1d4fa8")),C(10,"#ffffff",op=.5,cx=-18,cy=-18),lambda s:BAR(s,WHITE,6,44,8))),
 ("Lollipop","swirl, black dot",K_(C(50,WHITE),*[ARC(r,a,a+180,7,"#e8402a") for r,a in ((42,0),(28,180),(14,0))],lambda s:DOT(s,INK,46,6))),
 ("Pastel","mint, lilac line",K_(C(50,"#a8e6cf"),lambda s:BAR(s,"#9a7fd1",0,44,11))),
 ("Candy coat","yellow, white dot",K_(C(50,R("#ffe56b","#e0a800")),lambda s:DOT(s,WHITE,30,9))),
 ("Allsort","layered square",K_(RR(44,6,INK),lambda s:RECT(-44,-12,88,24,"#f7a8c8"),lambda s:RECT(-44,-4,88,8,WHITE),lambda s:BAR(s,YEL,14,42,10,"butt"))),
 ("Bubble","clear with sheen",K_(C(50,"#d8f1ff",op=.55),RING(49,2,"#9bd3f0"),ARC(38,-60,20,5,"#ff9ad5",op=.7),ARC(38,200,260,5,"#9affd8",op=.7),lambda s:BAR(s,"#2b5f80",0,40,8))),
 ("Play dough","lumpy orange",K_(SCAL(5,50,7,"#f28c28"),lambda s:BAR(s,"#7b3fbf",0,40,12))),
 ("Duck","yellow, orange beak",K_(lambda s:TRI(s,"#f28c28",56,36,14),C(46,"#ffd84a"),C(5,INK,cx=12,cy=-18),C(5,INK,cx=-12,cy=-18))),
 ("Doughnut","pink icing, sprinkles",K_(C(50,"#b9824f"),SCAL(9,46,5,"#f7a8c8"),*[LINE(*pol(r,a),*pol(r,a+8),c,3) for r,a,c in ((34,40,"#3d7fd6"),(30,110,YEL),(36,170,"#5aa469"),(32,230,WHITE),(30,300,"#3d7fd6"))],C(14,"#ECE8E0"),lambda s:BAR(s,"#5a3418",16,44,7))),
 ("Glass marble","blue swirl",K_(C(50,R("#d6ecff","#5fa0dc")),PATH("M-30,10 C-20,-30 20,-30 30,10","none","#1d5fb0",8,.7),lambda s:BAR(s,WHITE,0,44,7))),
 ("Toy gear","yellow gear, blue bar",K_(TEETH(10,50,40,"#ffd84a"),C(14,RED),lambda s:BAR(s,"#2f6fc4",16,44,10,"butt"))),
 ("Stack rings","rainbow rings",K_(*[C(r,c) for r,c in zip((50,40,30,20),("#e8402a","#f2c12e","#5aa469","#2f7fd6"))],lambda s:BAR(s,INK,0,48,8,"butt"))),
]))
# ---------------- 136-150 Typography ----------------
CATS.append(("Typography","type",[
 ("Wordmark","Hær logo reads level at noon",K_(C(50,INK),LOGO(CREAM,70,0),lambda s:BAR(s,ORANGE,30,49,10,"butt"))),
 ("H rim","Hær H round the rim",K_(C(50,"#2a2b30"),TXT("H",18,CREAM,-36,rot=0),TXT("H",18,CREAM,-36,rot=120),TXT("H",18,CREAM,-36,rot=240),lambda s:BAR(s,ORANGE,0,24,8))),
 ("Numeral dial","0-10 printed",K_(C(50,CREAM,INK,3),*[TXT(str(n),11,INK,-38,"Inter",800,rot=-135+27*n) for n in range(0,11,2)],C(22,INK),lambda s:BAR(s,RED,0,46,6,"butt"))),
 ("Bang","! points outward",K_(C(50,YEL),TXT("!",70,INK,-6,"Inter",900))),
 ("Arrow","↑ glyph",K_(C(50,"#2f6fc4"),TXT("↑",72,WHITE,-6,"Inter",900))),
 ("Serif I","the letter is the bar",K_(C(50,"#f4f1ea","#1c1c20",3),TXT("I",80,INK,-10,"DejaVu Serif",700))),
 ("Ampersand","& with red dot",K_(C(50,INK),TXT("&amp;",46,CREAM,6,"DejaVu Serif",700),lambda s:DOT(s,RED,42,6))),
 ("Hash","# grid, orange dot",K_(C(50,"#3a3c42"),TXT("#",60,"#9aa0a8",4,"Inter",900),lambda s:DOT(s,ORANGE,40,8))),
 ("Plus","one arm coloured",K_(C(50,INK),lambda s:RECT(-6,-34,12,68,CREAM),lambda s:RECT(-34,-6,68,12,CREAM),lambda s:BAR(s,RED,8,40,12,"butt"))),
 ("Q tail","the tail points",K_(C(50,INK),TXT("Q",72,CREAM,-4,"Inter",900,rot=225))),
 ("Inverted T","stem to the edge",K_(C(50,"#2a2b30"),TXT("T",76,CREAM,-6,"Inter",900,rot=180))),
 ("Braille","dot cluster pointer",K_(C(50,"#f4f1ea","#bdb6a6",2),*[C(4.5,INK,cx=x,cy=y) for x,y in ((-6,-42),(6,-42),(-6,-30),(6,-30),(-6,-18))])),
 ("Typewriter","chrome key, H",K_(C(50,CHROME),C(40,INK),TXT("H",40,CREAM,2,"CC"),lambda s:DOT(s,RED,46,5))),
 ("Stencil","broken stencil bar",K_(C(50,"#6b6f3a"),lambda s:RECT(-6 if not s else -8,-46,12 if not s else 16,18,CREAM),lambda s:RECT(-6 if not s else -8,-24,12 if not s else 16,18,CREAM))),
 ("Asterisk","one long arm",K_(C(50,"#e8402a"),*[LINE(0,0,*pol(22,a),WHITE,7) for a in (60,120,180,240,300)],lambda s:BAR(s,WHITE,0,46,8))),
]))
# ---------------- 151-165 Retro computing and pixel ----------------
G="#39ff6a"; AMB="#ffb000"
def pbar(s,col,step=6): return "".join(f'<rect x="{-step if s else -step/2-1}" y="{-48+i*step}" width="{2*step if s else step+2}" height="{step}" fill="{col}"/>' for i in range(6))
CATS.append(("Retro computing","pixel",[
 ("8-bit","pixel disc, pixel bar",K_(PIX(6,"#3a3c42"),lambda s:pbar(s,WHITE))),
 ("Phosphor","green outline pixels",K_(PIX(6,"#0a0f0a"),PIX(6,G,50,lambda x,y:x*x+y*y>40*40),lambda s:pbar(s,G))),
 ("Amber CRT","amber glow line",K_(C(50,"#1d1308"),lambda s:GBAR(s,AMB,4,44,6))),
 ("Beige 90s","beige plastic, grey line",K_(C(50,"#d9d1bd","#a99f86",3),SPECK(40,"#c6bda7",1,4),lambda s:BAR(s,"#6b6658",6,42,8))),
 ("Floppy","shutter = pointer",K_(RR(44,4,"#1d2a55"),lambda s:RECT(-12 if s else -10,-44,24 if s else 20,30,"#c9ccd2"),RECT(-26,10,52,28,WHITE,2))),
 ("Cursor","arrow pointer",K_(C(50,WHITE,"#888",2),POLY([(0,-46),(16,-6),(5,-9),(10,8),(4,10),(-1,-6),(-10,2)],INK))),
 ("Bevel","grey bevel square",K_(RR(44,0,"#c0c0c0"),PATH("M-44,44 L-44,-44 L44,-44","none",WHITE,5),PATH("M44,-44 L44,44 L-44,44","none","#808080",5),lambda s:BAR(s,INK,0,36,8,"butt"))),
 ("Prompt","> points up",K_(C(50,"#0b0f0b"),TXT("&gt;",58,G,-14,"JetBrains Mono",800,rot=-90))),
 ("Joystick","red ball offset",K_(C(50,INK),C(30,"#2a2b30"),lambda s:C(16 if s else 13,R("#ff8a7a","#b81d12"),cx=0,cy=-28))),
 ("LCD","segment bar",K_(C(50,"#9fae8f","#5f6b54",3),lambda s:POLY([(0,-46),(6 if not s else 9,-40),(6 if not s else 9,-12),(0,-6),(-6 if not s else -9,-12),(-6 if not s else -9,-40)],"#222a1c"))),
 ("Dither","checker dither",K_(PIX(5,"#2a2b30"),PIX(5,"#6b6f78",50,lambda x,y:(int((x+60)//5)+int((y+60)//5))%2==0 and y>0),lambda s:pbar(s,YEL,6))),
 ("Isometric","three-tone hex",K_(POLY([pol(50,a) for a in (300,0,60,0)][:3]+[(0,0)],"#9aa0a8"),POLY([pol(50,a) for a in (60,120,180)]+[(0,0)],"#5a5e66"),POLY([pol(50,a) for a in (180,240,300)]+[(0,0)],"#3a3d43"),lambda s:BAR(s,ORANGE,6,44,8,"butt"))),
 ("Vector","wireframe green",K_(C(50,"#050805"),RING(46,1.5,G),RING(30,1.5,G,.5),NGON(6,30,"none",stroke=G,sw=1),lambda s:BAR(s,G,0,46,3 if not s else 4,"butt"))),
 ("Keycap","home-bump bar",K_(RR(44,10,"#9a958a"),RR(36,8,"#f6f3ec"),lambda s:BAR(s,"#6b6658",16,34,7))),
 ("Power LED","beige, green LED",K_(C(50,"#d9d1bd","#a99f86",3),lambda s:DOT(s,G,32,12,op=.3),lambda s:DOT(s,G,32,6))),
]))
# ---------------- 166-180 Sci-fi interfaces ----------------
CY="#33e6ff"; MAG="#ff3dbb"
CATS.append(("Sci-fi interfaces","scifi",[
 ("Neon ring","cyan glow bar",K_(C(50,"#07131a"),RING(44,3,CY,.6),lambda s:GBAR(s,CY,0,40,6))),
 ("Halo gap","magenta arc, gap points",K_(C(50,"#14061a"),ARC(40,24,336,10,MAG,op=.3),ARC(40,24,336,4,MAG),lambda s:BAR(s,WHITE,30,48,5,"butt"))),
 ("HUD","segmented arcs",K_(C(50,"#0c1216"),RING(44,3,"#7fd1ff",.8,"10 6"),RING(32,2,"#7fd1ff",.5,"4 4"),lambda s:TRI(s,ORANGE,48,24,9))),
 ("Hologram","blue grid",K_(C(50,"#3a8dff",op=.25),CLIP("".join(LINE(x,-50,x,50,"#7ab8ff",1,op=.6)+LINE(-50,x,50,x,"#7ab8ff",1,op=.6) for x in range(-48,50,8))),RING(49,2,"#7ab8ff"),lambda s:BAR(s,WHITE,0,44,6))),
 ("Reactor","glowing rings",K_(C(50,"#081018"),*[RING(r,3,CY,o) for r,o in ((44,.3),(34,.5),(24,.7))],C(10,CY),lambda s:BAR(s,WHITE,12,46,6))),
 ("Visor","red slit glow",K_(C(50,R("#3a3a40","#050507")),lambda s:GBAR(s,"#ff2a2a",10,40,5))),
 ("Scanner","one bright dot",K_(C(50,"#06140c"),DOTS(24,40,2.5,"#2a6b45"),lambda s:DOT(s,"#5dff9a",40,10,op=.3),lambda s:DOT(s,"#5dff9a",40,5))),
 ("Plasma","purple core",K_(C(50,R("#e3a6ff","#2a0640",50,50)),lambda s:BAR(s,WHITE,0,46,7))),
 ("Hex shield","teal hex grid",K_(C(50,"#0a2a2e"),CLIP("".join(NGON(6,7,"none",stroke="#2fd1c0",sw=1).replace("<polygon",f'<polygon transform="translate({x+(7 if (y//12)%2 else 0)},{y})"') for x in range(-56,60,14) for y in range(-60,61,12))),lambda s:BAR(s,YEL,0,46,8))),
 ("Orbit","moon = pointer",K_(C(50,"#0b0d1c"),RING(34,1,"#8890c8"),C(18,R("#7aa0ff","#2a3ca0")),lambda s:C(9 if s else 7,"#f4f1ea",cx=0,cy=-34))),
 ("Warp","streaks, white bar",K_(C(50,"#05060f"),*[LINE(*pol(14,a),*pol(46,a),"#8aa8ff",1.5,op=.6) for a in range(0,360,20)],lambda s:BAR(s,WHITE,0,46,8))),
 ("Data ring","waveform ticks",K_(C(50,"#081218"),*[LINE(*pol(36,a),*pol(36+6+6*math.sin(a/9),a),CY,2,op=.8) for a in range(0,360,10)],lambda s:BAR(s,RED,0,46,7))),
 ("Iris","aperture blades",K_(*[POLY([(0,0),pol(50,a),pol(50,a+60),pol(14,a+80)],c) for a,c in zip(range(0,360,60),["#2a2c33","#3a3c44"]*3)],C(10,"#0a0a0c"),lambda s:DOT(s,RED,40,6))),
 ("Laser","thin red beam",K_(C(50,INK),lambda s:BAR(s,"#ff1a1a",-44,46,8,"round",.2),lambda s:BAR(s,"#ff5a5a",0,46,3 if not s else 4))),
 ("Chrome glow","silver, blue ring",K_(C(50,CHROME),RING(38,3,"#4aa8ff",.8),lambda s:GBAR(s,"#4aa8ff",6,34,5))),
]))
# ---------------- 181-195 World crafts ----------------
CATS.append(("World crafts","crafts",[
 ("Lacquer","red-black urushi",K_(C(50,R("#c0281e","#3a0805",30,20)),RING(48,2,"#1a0402"),lambda s:BAR(s,"#e4c25a",6,44,6))),
 ("Gold dust","black lacquer, gold flecks",K_(C(50,"#0e0c0c"),SPECK(60,"#d9b25a",1.2,11,46,.8),lambda s:BAR(s,"#e8c66b",0,44,8))),
 ("Zellige","8-point star tile",K_(C(50,"#f4f1ea"),STAR(8,46,30,"#1f5fa0"),STAR(8,26,16,"#2e8a5a"),lambda s:BAR(s,WHITE,30,48,7,"butt"))),
 ("Woven","kente-like blocks",K_(CLIP("".join(RECT(x,y,12,12,["#e3b22b","#2e7d32","#c62828","#111"][(x//12+y//12)%4]) for x in range(-60,60,12) for y in range(-60,60,12))),C(20,"#111"),lambda s:BAR(s,"#f4f1ea",0,46,8))),
 ("Rose paint","blue, red rose",K_(C(50,"#1f4f8a"),SCAL(6,24,8,"#d23a2c"),C(7,"#f2c12e"),lambda s:BAR(s,WHITE,28,48,7))),
 ("Shibori","indigo, dotted rings",K_(C(50,"#1c2f5a"),RING(40,3,"#e9eef7",.8,"2 5"),RING(26,3,"#e9eef7",.6,"2 5"),lambda s:BAR(s,WHITE,0,46,8))),
 ("Talavera","white, blue petals",K_(C(50,"#f7f4ec"),SCAL(8,44,12,"#2a4fa0"),C(18,"#f7f4ec"),C(9,YEL),lambda s:BAR(s,"#e8602a",30,48,8))),
 ("Turquoise","glaze, cobalt line",K_(C(50,R("#7fe0d6","#1f8f8a")),lambda s:BAR(s,"#1e3a8a",0,46,9))),
 ("Delft","white, blue rings",K_(C(50,"#f7f7f4"),RING(44,3,"#2a4fa0"),RING(30,1.5,"#2a4fa0"),lambda s:BAR(s,"#2a4fa0",0,46,8))),
 ("Batik","wax crackle",K_(C(50,"#7a4a26"),*[PATH(d,"none","#e8c99a",1.2,.7) for d in ("M-40,-20 L-10,-5 L5,-30 L30,-10","M-30,30 L0,15 L20,35 L40,20","M-45,5 L-20,10")],lambda s:BAR(s,"#f4e2c0",0,46,8))),
 ("Kilim","red/cream diamonds",K_(C(50,"#b52d2d"),*[POLY([(x,-12),(x+10,0),(x,12),(x-10,0)],"#f4e2c0") for x in (-30,-10,10,30)],lambda s:BAR(s,INK,0,46,8,"butt"))),
 ("Ikat","blurred stripes",K_(CLIP("".join(RECT(x,-60,10,120,c,op=.85) for x,c in zip(range(-60,60,10),["#7b2d6b","#e6a23c","#1f4f8a"]*4))),lambda s:BAR(s,WHITE,0,46,9))),
 ("Sashiko","indigo, stitched lines",K_(C(50,"#16244a"),*[RING(r,1.8,"#f4f1ea",.9,"4 3") for r in (44,34,24)],lambda s:BAR(s,"#f4f1ea",0,46,7))),
 ("Raku","crackle glaze, copper",K_(C(50,"#e9e4d8"),*[PATH(d,"none","#222",1.2) for d in ("M-46,-10 L-20,0 L-6,-24 L20,-18 L44,-30","M-30,34 L-4,20 L18,38","M10,-2 L30,12 L48,4")],lambda s:BAR(s,"#b5652f",0,46,9))),
 ("Cloisonné","turquoise, gold wire",K_(C(50,"#2aa9a6","#d4af37",3),SCAL(5,30,10,"#e8402a",stroke="#d4af37",sw=2),C(8,"#2b4fa0","#d4af37",2),lambda s:BAR(s,"#d4af37",34,48,8,"butt"))),
]))
# ---------------- 196-210 Techno and club ----------------
CATS.append(("Techno & club","club",[
 ("Detroit chrome","chrome, black bar",K_(C(50,CHROME),lambda s:BAR(s,INK,0,44,8,"butt"))),
 ("Silver acid","small silver cap",K_(C(50,"#3a3b40"),C(36,SILVER),lambda s:BAR(s,INK,0,48,5,"butt"))),
 ("Smiley","the smile shows the angle",K_(C(50,"#ffd93a",INK,3),C(5,INK,cx=-15,cy=-10),C(5,INK,cx=15,cy=-10),PATH("M-22,12 Q0,34 22,12","none",INK,5))),
 ("Vinyl","grooves, red label",K_(C(50,"#111"),*[RING(r,.8,"#3a3a3a") for r in range(24,50,3)],C(20,"#d9302a"),C(3,"#ECE8E0"),lambda s:BAR(s,WHITE,22,48,4,"butt"))),
 ("Strobe","flash wedges",K_(C(50,INK),*[WEDGE(50,a-6,a+6,WHITE,.5) for a in range(45,360,45)],lambda s:WEDGE(50,-10 if s else -7,10 if s else 7,"#ff3a2a"))),
 ("Warehouse","corrugated steel",K_(CLIP("".join(RECT(x,-60,5,120,"#8d929b" if (x//5)%2 else "#6b7078") for x in range(-60,60,5))),RING(49,2,"#4a4e55"),lambda s:BAR(s,ORANGE,0,46,10,"butt"))),
 ("Laser fan","one bright beam",K_(C(50,"#050805"),*[LINE(0,0,*pol(48,a),"#39ff6a",1.5,op=.35) for a in range(-60,61,15)],lambda s:BAR(s,"#9dffb6",0,48,5))),
 ("Accent cap","grey, orange top",K_(C(50,"#8d9199"),C(30,"#f07a1d"),lambda s:BAR(s,WHITE,0,48,6,"butt"))),
 ("Acid green","neon, black bar",K_(C(50,"#b6ff2e"),lambda s:BAR(s,INK,0,44,10))),
 ("White label","stamp ring, red dot",K_(C(50,"#f6f5f1"),RING(32,2,INK,.6,"3 3"),C(3,INK),lambda s:DOT(s,"#d9302a",42,6))),
 ("Haze","smoky grey",K_(C(50,R("#cfd2d8","#3a3d44",50,50)),lambda s:BAR(s,WHITE,0,44,8))),
 ("Concrete room","red strip light",K_(C(50,"#5d5f61"),SPECK(30,"#46484a",1.6,13),lambda s:GBAR(s,"#ff2a2a",8,44,5))),
 ("Midnight blue","deep blue, silver line",K_(C(50,R("#2a4f9e","#0b1a40")),lambda s:BAR(s,"#d6dbe4",0,46,7,"butt"))),
 ("EQ colours","DJ-mixer colour ring",K_(C(50,"#2a2b30"),RING(45,5,"#d9302a"),C(34,"#3a3c42"),lambda s:BAR(s,WHITE,0,44,7,"butt"))),
 ("Platter","strobe dots, red dot",K_(C(50,SILVER),DOTS(36,46,1.8,"#3a3d44"),DOTS(30,40,1.6,"#3a3d44"),C(30,"#2a2b30"),lambda s:DOT(s,"#ff3a2a",22,6))),
]))

def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')
K.knob_svg=knob_svg
n0=61
for title,slug,designs in CATS:
    assert len(designs)==15,(title,len(designs))
    for i,(name,_,fn) in enumerate(designs,n0):
        sl=re.sub(r'[^a-z0-9]+','-',name.lower().replace('æ','ae').replace('é','e')).strip('-')
        for sz,mm in K.SIZES:
            open(f"{OUT}/svg/{i:03d}-{sl}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", ""))
    K.DESIGNS=designs
    for t in ("light","dark"):
        h=K.sheet(t).replace("15 designs",f"{title} · designs {n0}–{n0+14}")
        h=re.sub(r'<div class="num">(\d+)</div>', lambda m:f'<div class="num">{int(m.group(1))+n0-1}</div>', h)
        h=h.replace("@font-face{","@font-face{font-display:block;",1)
        open(f"{OUT}/sheets/knobs-{n0:03d}-{slug}-{t}.html","w").write(h)
    n0+=15
print("ok", n0)
