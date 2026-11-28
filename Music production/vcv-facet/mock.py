import math, sys
BG,PANEL2,LINE,INK,SOFT,AMB,RED="#232428","#2C2D32","#44454B","#ECE6D8","#9A968C","#F3B04A","#E0533D"
TONE={"A":"#E0533D","B":"#F3B04A","C":"#4FB3A9","D":"#7B8CDE"}
g=[]
def t(s,x,y,size,fill=INK,anchor="middle",font="Inter",w=700,ls=0):
    s=s.replace("&","&amp;"); g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{w}" letter-spacing="{ls}" text-anchor="{anchor}" fill="{fill}">{s}</text>')
def rect(x,y,w,h,fill,rx=1,stroke=None,sw=0.35):
    st=f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ''
    g.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"{st}/>')
def knob(x,y,k,name=None,r=4.2,lab=8.2,col="#111"):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r+0.6}" fill="#0c0c0d"/><circle cx="{x}" cy="{y}" r="{r}" fill="#2a2a2e" stroke="#555" stroke-width="0.3"/>')
    a=math.radians(-150+300*k-90); g.append(f'<line x1="{x}" y1="{y}" x2="{x+r*0.9*math.cos(a):.2f}" y2="{y+r*0.9*math.sin(a):.2f}" stroke="{INK}" stroke-width="0.7" stroke-linecap="round"/>')
    if name: t(name,x,y+lab,3.2)
def slider(x,y0,h,k,name,cap=INK):
    g.append(f'<rect x="{x-0.9}" y="{y0}" width="1.8" height="{h}" rx="0.9" fill="#0c0c0d"/>')
    for i in range(6): g.append(f'<line x1="{x-3.2}" y1="{y0+i*h/5:.2f}" x2="{x-2}" y2="{y0+i*h/5:.2f}" stroke="{LINE}" stroke-width="0.3"/>')
    cy=y0+h*(1-k); rect(x-3,cy-2.2,6,4.4,cap,0.8); g.append(f'<line x1="{x-3}" y1="{cy}" x2="{x+3}" y2="{cy}" stroke="#111" stroke-width="0.4"/>')
    t(name,x,y0+h+5.2,3.2)
def jack(x,y,name,out=False):
    if out: rect(x-4.3,y-5.2,8.6,12.6,"#4A4B50",1.2)
    g.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#9a9a9a"/><circle cx="{x}" cy="{y}" r="2.2" fill="#555"/><circle cx="{x}" cy="{y}" r="1.4" fill="#111"/>')
    t(name,x,y+6.4 if not out else y+6.0,2.9,fill=INK if not out else INK)
def btn(x,y,lab,w=9,h=5,fill="#3A3B40",ink=INK,size=3.0,sel=False):
    rect(x-w/2,y-h/2,w,h,fill,1)
    if sel: rect(x-w/2-0.6,y-h/2-0.6,w+1.2,h+1.2,"none",1.4,INK,0.45)
    t(lab,x,y+size*0.36,size,fill=ink)
def led(x,y,on,col=AMB): g.append(f'<circle cx="{x}" cy="{y}" r="1.1" fill="{col if on else "#3A3020"}"/>')
def sect(name,x0,x1,y,col=AMB):
    t(name,x0,y,3.0,fill=col,anchor="start",ls=0.35); w=len(name)*2.35+2
    g.append(f'<line x1="{x0+w}" y1="{y-1}" x2="{x1}" y2="{y-1}" stroke="{LINE}" stroke-width="0.35"/>')
def logo(x,y,fill=INK): t("Hær",x,y,3.4,fill=fill,font="Climate Crisis",w=400,anchor="end")
def screen(x,y,w,h,lines):
    rect(x,y,w,h,"#17150F",1.5,"#55503F",0.4)
    for i,(s,c,sz) in enumerate(lines): t(s,x+2.5,y+5+i*(h-6)/max(1,len(lines)-1) if len(lines)>1 else y+h/2,sz,fill=c,anchor="start",font="Share Tech Mono",w=400)
def svg(W,H):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*6}" height="{H*6}" viewBox="0 0 {W} {H}"><rect width="{W}" height="{H}" fill="{BG}"/>'+"".join(g)+'</svg>'

def optionA():
    W=284.48; H=128.5
    rect(0,0,W,11,"#18191C",0); t("FACET",8,7.6,5.2,anchor="start",ls=1.2); t("4-TONE DIGITAL LAYER SYNTH",33,7.4,2.9,fill=SOFT,anchor="start",w=600,ls=0.4)
    t("working name",W-8,7.4,2.6,fill=SOFT,anchor="end",w=500)
    # tones
    sect("TONES",6,62,17)
    for i,c in enumerate("ABCD"):
        x=12+i*14; btn(x,24,c,w=11,h=6,fill=TONE[c],ink="#1B1B1B",size=3.6,sel=(c=="A"))
        led(x,31,c!="D",TONE[c])
    t("EDIT",12,37.5,2.6,fill=SOFT); t("on/off LEDs: hold + tap",40,37.5,2.6,fill=SOFT,w=500)
    btn(19,44,"LINK ALL",w=20,h=5); btn(47,44,"SOLO TONE",w=22,h=5)
    # screen
    screen(68,14,92,34,[("TONE A  ·  WG",AMB,3.4),("WAVE 041 GLASS SPECTRUM",INK,3.6),("COARSE +12  FINE -03  RND 05",INK,3.0),("ENV  T1 02  T2 40  T3 55  T4 70",SOFT,3.0),("PAGE 2/9  <  >   [ENTER]",SOFT,2.8)])
    btn(74,52.5,"<",w=8,h=4.6); btn(84,52.5,">",w=8,h=4.6); knob(100,53.5,0.4,None,r=3.6); t("DATA",108,54.8,2.9,fill=SOFT,anchor="start")
    btn(140,52.5,"PAGE",w=14,h=4.6)
    # patch / random
    sect("PATCH  ·  MOVEMENT",166,W-6,17)
    for i in range(8):
        x=172+i*8.6; btn(x,24,str(i+1),w=7,h=5.5,sel=(i==2))
    btn(172+0.5,34,"SAVE",w=10,h=5)
    knob(197,36,0.3,"RAND AMT",r=4.2,lab=9)
    btn(214,34,"RAND",w=12,h=6,fill=RED,ink="#1B1B1B")
    knob(236,36,0.55,"MUTATE",r=4.2,lab=9)
    btn(256,30,"LOCK",w=12,h=6,fill=AMB,ink="#1B1B1B"); led(256,37,True,AMB)
    t("+ every hidden param",214,41.5,2.5,fill=SOFT,w=500)
    btn(273,42,"SEL",w=9,h=4.6); t("what MUTATE moves",256,48.5,2.5,fill=SOFT,w=500)
    # sliders row: edits selected tone
    y0=64; hh=26
    sect("WG",6,52,60,TONE["A"]); knob(14,72,0.4,"WAVE",r=5.2,lab=10); slider(31,y0,hh,0.5,"PITCH"); slider(45,y0,hh,0.5,"FINE")
    sect("TVF",56,118,60,TONE["A"]); btn(66,66,"LPF",w=10,h=4.4,sel=True); btn(66,72,"BPF",w=10,h=4.4); btn(66,78,"HPF",w=10,h=4.4)
    slider(80,y0,hh,0.62,"CUTOFF"); slider(94,y0,hh,0.3,"RESO"); slider(108,y0,hh,0.65,"ENV")
    sect("TVA",122,152,60,TONE["A"]); slider(130,y0,hh,0.75,"LEVEL"); slider(144,y0,hh,0.5,"VELO")
    sect("ENVELOPE",156,236,60,TONE["A"])
    for i,n in enumerate(["PITCH","TVF","TVA"]): btn(165,66+i*6,n,w=13,h=4.4,sel=(n=="TVF"))
    for i,(n,k) in enumerate([("T1",0.1),("T2",0.4),("T3",0.5),("SUS",0.6),("T4",0.7)]): slider(180+i*12.5,y0,hh,k,n)
    sect("LFO",240,W-6,60,TONE["A"]); btn(250,66,"1",w=6,h=4.4,sel=True); btn(258,66,"2",w=6,h=4.4)
    knob(254,78,0.35,"RATE",r=4,lab=8.6); knob(270,70,0.2,"PITCH",r=3.4,lab=7.4); knob(270,85,0.0,None,r=3.4); t("TVF/TVA",270,92.5,2.6,fill=SOFT,w=600)
    # FX row
    sect("EFFECTS  (patch)",6,W-6,103)
    fx=[("DIST",0.3,True),("PHASER",0.5,False),("SPECTRUM",0.5,False),("ENHANCE",0.4,True),("CHORUS",0.55,True),("DELAY",0.35,True),("REVERB",0.5,True)]
    for i,(n,k,on) in enumerate(fx):
        x=14+i*19; btn(x-1,108.5,n,w=15.5,h=4.6,size=2.5,fill="#3A3B40" if on else "#2C2D32",ink=INK if on else SOFT); led(x+8.6,108.5,on); knob(x,116.8,k,None,r=3.4)
    t("A-order",14+3.5*19,124.5,2.6,fill=SOFT,w=500)
    # IO
    xs=[150,161,172,183,194,205,216,227,238]
    for x,n in zip(xs,["V/OCT","GATE","VEL","PRESS","MOD","BEND","CV 1","CV 2","LOCK"]): jack(x,113,n)
    jack(254,113,"L",True); jack(266,113,"R",True); t("OUT",260,105.8,2.6,fill=INK)
    logo(W-6,126.2); t("Hær", 0,0,0)
    g.pop()
    return svg(W,H)

def optionB():
    W=203.2; H=128.5
    rect(0,0,W,11,"#18191C",0); t("FACET",8,7.6,5.2,anchor="start",ls=1.2); t("STRIPS",34,7.4,2.9,fill=SOFT,anchor="start",ls=0.4)
    for i,c in enumerate("ABCD"):
        x0=5+i*28; rect(x0,14,25,86,PANEL2,1.5); btn(x0+12.5,19.5,"TONE "+c,w=20,h=5.4,fill=TONE[c],ink="#1B1B1B",size=3.0)
        for j,(n,k) in enumerate([("WAVE",0.3+i*0.15),("PITCH",0.5),("CUTOFF",0.6-i*0.1),("RESO",0.2+i*0.1),("ATTACK",0.1+i*0.15),("LEVEL",0.7)]):
            knob(x0+12.5,29+j*10.9,k,None,r=3.3); t(n,x0+12.5,29+j*10.9+6.0,2.9)
        btn(x0+12.5,96,"ON",w=12,h=4.2,sel=(c!="D"))
    screen(120,14,78,26,[("PATCH 03  GLASS CATHEDRAL",AMB,3.2),("A: 041 GLASS SPECTRUM",INK,3.0),("B: 007 BREATH CHIFF",INK,3.0),("TAP TONE + PAGE = deep edit",SOFT,2.7)])
    sect("MACROS",120,198,46)
    for j,(n,k) in enumerate([("BRIGHT",0.6),("MOTION",0.4),("SPACE",0.5),("GRIT",0.2)]): knob(130+j*19,56,k,n,r=4.2,lab=8.6)
    sect("MOVEMENT",120,198,73); knob(130,83,0.3,"RAND AMT",r=3.8,lab=8.2); btn(149,81,"RAND",w=12,h=5.5,fill=RED,ink="#1B1B1B")
    knob(168,83,0.55,"MUTATE",r=3.8,lab=8.2); btn(187,81,"LOCK",w=12,h=5.5,fill=AMB,ink="#1B1B1B")
    xs=[12,24,36,48,60,72,84,96,108]
    for x,n in zip(xs,["V/OCT","GATE","VEL","PRESS","MOD","BEND","CV 1","CV 2","LOCK"]): jack(x,114,n)
    for i in range(8): btn(126+i*8.2,104,str(i+1),w=7,h=5,sel=(i==2))
    jack(176,113,"L",True); jack(188,113,"R",True)
    logo(W-6,126.2)
    return svg(W,H)
which=sys.argv[1]
open(f"opt{which}.svg","w").write(optionA() if which=="A" else optionB())
