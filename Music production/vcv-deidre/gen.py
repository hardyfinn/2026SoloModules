# Mockup generator: expanded clocked-modulation module ("Interval", working name)
import math
HP=56; W=HP*5.08; H=128.5
O=[]  # svg elements
def a(s): O.append(s)
INK="#1f1d1a"; PANEL="#ece6da"; MUTED="#6b655c"
LANE=["#e4572e","#f3a712","#c9b200","#4caf50","#17bebb","#2e86de","#8e5cd9","#d9489b"]
BANDS=[("TIME","#f6d9c9"),("WAVE","#f8e7bd"),("EUCLID","#d6ead2"),("LOOP","#cfe6ea"),("LOGIC / PITCH","#ddd6f0")]
def text(x,y,s,size=3.2,anchor="middle",fill=INK,weight=600,font="Inter",extra=""):
    a(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{fill}" {extra}>{s}</text>')
def knob(x,y,r=4.6,ang=-30,stepped=False,cv=None):
    if stepped:
        for i in range(11):
            t=math.radians(-225+27*i)
            a(f'<line x1="{x+(r+0.6)*math.cos(t):.2f}" y1="{y+(r+0.6)*math.sin(t):.2f}" x2="{x+(r+1.5)*math.cos(t):.2f}" y2="{y+(r+1.5)*math.sin(t):.2f}" stroke="{INK}" stroke-width="0.35"/>')
    a(f'<circle cx="{x}" cy="{y}" r="{r+0.5}" fill="#9c958a"/>')
    a(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#2a2826"/>')
    a(f'<circle cx="{x}" cy="{y}" r="{r*0.72}" fill="#3a3734"/>')
    t=math.radians(ang-90)
    a(f'<line x1="{x+r*0.2*math.cos(t):.2f}" y1="{y+r*0.2*math.sin(t):.2f}" x2="{x+r*0.95*math.cos(t):.2f}" y2="{y+r*0.95*math.sin(t):.2f}" stroke="#f4efe6" stroke-width="0.8" stroke-linecap="round"/>')
    if cv is not None:
        a(f'<circle cx="{x+r+0.4}" cy="{y-r-0.2}" r="1.05" fill="{["#e4572e","#2e86de","#4caf50","#8e5cd9"][cv]}" stroke="{PANEL}" stroke-width="0.35"/>')
def jack(x,y):
    a(f'<circle cx="{x}" cy="{y}" r="4.1" fill="#b9b3a8"/><circle cx="{x}" cy="{y}" r="3.2" fill="#d8d3ca" stroke="#8d877c" stroke-width="0.3"/><circle cx="{x}" cy="{y}" r="1.85" fill="#111"/>')
def button(x,y,s=6,fill="#3a3734",lit=None,label=None,lc="#f4efe6",lsize=3.2):
    a(f'<rect x="{x-s/2}" y="{y-s/2}" width="{s}" height="{s}" rx="1.2" fill="#9c958a"/>')
    a(f'<rect x="{x-s/2+0.5}" y="{y-s/2+0.5}" width="{s-1}" height="{s-1}" rx="0.9" fill="{lit or fill}"/>')
    if label: text(x,y+lsize*0.36,label,size=lsize,fill=lc,weight=800)
def led(x,y,c,on=True):
    a(f'<circle cx="{x}" cy="{y}" r="1.1" fill="{c if on else "#5a5650"}" stroke="#2a2826" stroke-width="0.25"/>')

# ---------- panel
a(f'<rect width="{W}" height="{H}" fill="{PANEL}"/>')
a(f'<rect width="{W}" height="9.5" fill="{INK}"/><rect y="{H-6}" width="{W}" height="6" fill="{INK}"/>')
text(6,6.6,"DEIDRE",size=4.6,anchor="start",fill="#f4efe6",weight=800,extra='letter-spacing="0.6"')
text(29,6.6,"Deidre&#8217;s Latest Workout · 8-output clocked modulation",size=3.0,anchor="start",fill="#bdb6aa",weight=500)
text(W-6,6.6,"mockup",size=2.6,anchor="end",fill="#8d877c",weight=500)
for sx in (2.5,W-7.5):
    for sy in (3,H-3):
        a(f'<circle cx="{sx+2.5}" cy="{sy}" r="1.3" fill="#55514b"/>')
text(W/2,H-1.45,"Hær",size=4.2,fill="#f4efe6",weight=400,font="Climate Crisis")

# ---------- left column: CLOCK + BANK + INPUTS
a(f'<rect x="3" y="12" width="41" height="109.5" rx="2" fill="#e2dbcd"/>')
text(5,16.2,"CLOCK",size=2.9,anchor="start",fill=MUTED,weight=800)
knob(15,27,r=7,ang=20); text(15,40.5,"BPM")
button(35,22,s=7,lit="#4caf50",label="▶"); text(35,30.5,"RUN")
button(35,37,s=6); 
text(35,44.2,"RESET")
button(9,50,s=6); text(9,57.2,"TAP")
button(22,50,s=6); text(22,57.2,"RESEED")
button(35,50,s=6); text(35,57.2,"COPY")
text(5,63.5,"BANK",size=2.9,anchor="start",fill=MUTED,weight=800)
button(9,68,s=6,label="◀"); button(22,68,s=6,label="▶"); button(35,68,s=6,lit="#c0392b",label="●",lsize=2.6)
text(15.5,75.2,"BANK"); text(35,75.2,"SAVE")
text(5,80.5,"INPUTS",size=2.9,anchor="start",fill=MUTED,weight=800)
# input row
# jacks in two rows of 3 continued below band
CVC=["#e4572e","#2e86de","#4caf50","#8e5cd9"]
for i,(lab,x,y,c) in enumerate([("CLK",9,88,None),("RUN",22,88,None),("CV 1",35,88,0),("CV 2",9,107,1),("CV 3",22,107,2),("CV 4",35,107,3)]):
    jack(x,y); text(x,y+9.3,lab)
    if c is not None: a(f'<circle cx="{x+4.3}" cy="{y-4.1}" r="1.05" fill="{CVC[c]}"/>')

# ---------- screen
SX,SY,SW,SH=48,12,120,72
a(f'<rect x="{SX-1.5}" y="{SY-1.5}" width="{SW+3}" height="{SH+3}" rx="2.5" fill="#2a2826"/>')
a(f'<rect x="{SX}" y="{SY}" width="{SW}" height="{SH}" rx="1" fill="#0e1116"/>')
F="JetBrains Mono, DejaVu Sans Mono, monospace"
def st(x,y,s,size=2.4,fill="#d8dee9",anchor="start",weight=500):
    text(x,y,s,size=size,anchor=anchor,fill=fill,weight=weight,font=F)
# header
a(f'<rect x="{SX}" y="{SY}" width="{SW}" height="6" fill="#1b2028"/>')
st(SX+2,SY+4.2,"BANK 07  TECHNO-A",fill="#9fb3c8")
st(SX+56,SY+4.2,"124.0 BPM",fill="#f4efe6",weight=700,size=2.8)
st(SX+80,SY+4.2,"▶ RUN  INT",fill="#7fd08a")
st(SX+SW-2,SY+4.2,"BAR 17.3",fill="#9fb3c8",anchor="end")
# lanes
import random; random.seed(3)
lanes=[("x4","PULSE",50),("/2","SMOOTH EXP",30),("x2","SMOOTH PULSE",40),("x1","TRIANGLE",60),("/4","SINE",50),("x3","RATCHET x2",50),("/8","SMOOTH RAND",50),("/16","RAND S&H",50)]
LX,LY,LW,LH=SX+2,SY+8,70,6.6
def wave(kind,x0,y0,w,h,cycles,col,mask=None):
    pts=[]
    N=int(w*4)
    rnd=[random.random() for _ in range(64)]
    for i in range(N+1):
        x=x0+w*i/N; ph=(i/N*cycles)%1; step=int(i/N*cycles)
        on = mask is None or mask[step%len(mask)]
        if kind in("PULSE","SMOOTH PULSE","RATCHET x2"):
            v=1 if ph<0.5 else 0
            if kind=="RATCHET x2": v=1 if (ph*4)%1<0.5 and ph<0.5 else 0
        elif kind=="SMOOTH EXP": v=math.exp(-ph*4)
        elif kind=="TRIANGLE": v=1-abs(ph*2-1)
        elif kind=="SINE": v=0.5+0.5*math.sin(ph*2*math.pi)
        elif kind=="SMOOTH RAND":
            p=i/N*cycles; k=int(p); f=p-k; f=f*f*(3-2*f); v=rnd[k%64]*(1-f)+rnd[(k+1)%64]*f
        else: v=rnd[step%64]
        if not on: v=0
        pts.append(f"{x:.2f},{y0+h-v*h:.2f}")
    a(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{col}" stroke-width="0.4" stroke-linejoin="round"/>')
masks={0:[1,0,1,1,0,1,0,1],2:[1,0,0,1,0,0,1,0],5:[1,1,0,1]}
cyc=[16,2,8,4,1,12,0.5*2,0.25*4]
for i,(mod,shape,wid) in enumerate(lanes):
    y=LY+i*LH
    sel=(i==2)
    if sel: a(f'<rect x="{SX+0.6}" y="{y-0.4}" width="{LW+2.4}" height="{LH-0.4}" fill="#26303d" stroke="{LANE[i]}" stroke-width="0.35"/>')
    a(f'<rect x="{LX}" y="{y+0.6}" width="3.6" height="4.4" rx="0.5" fill="{LANE[i]}"/>')
    st(LX+1.8,y+3.85,str(i+1),fill="#0e1116",anchor="middle",weight=800,size=2.6)
    st(LX+5,y+3.85,mod,fill="#f4efe6",weight=700,size=2.6)
    muted=(i==6)
    wave(shape,LX+13,y+0.8,40,4.2,[16,2,8,4,1,12,4,4][i],"#555c66" if muted else LANE[i],masks.get(i))
    st(LX+55,y+3.6,"MUTE" if muted else shape.replace("SMOOTH ","SM."),fill="#8d96a3",size=2.1)
# detail panel (selected output 3)
DX=SX+76; DY=SY+8
a(f'<rect x="{DX}" y="{DY}" width="{SW-78}" height="53" rx="1" fill="#151a21" stroke="{LANE[2]}" stroke-width="0.35"/>')
st(DX+2,DY+4.5,"OUT 3",fill=LANE[2],weight=800,size=3)
st(DX+40,DY+4.5,"x2 · SM.PULSE",fill="#f4efe6",anchor="end",size=2.3)
# euclid ring 8 steps 3 hits
cx,cy,r=DX+11,DY+19,8
pat=[1,0,0,1,0,0,1,0]
for k in range(8):
    t=math.radians(-90+45*k)
    px,py=cx+r*math.cos(t),cy+r*math.sin(t)
    a(f'<circle cx="{px:.2f}" cy="{py:.2f}" r="1.5" fill="{LANE[2] if pat[k] else "none"}" stroke="{LANE[2]}" stroke-width="0.35"/>')
a(f'<circle cx="{cx+ (r)*math.cos(math.radians(-90+45*3)):.2f}" cy="{cy+r*math.sin(math.radians(-90+45*3)):.2f}" r="2.3" fill="none" stroke="#fff" stroke-width="0.3"/>')
st(cx,cy+1,"3/8",anchor="middle",fill="#f4efe6",weight=700,size=2.6)
st(cx,DY+31,"EUCLID",anchor="middle",fill="#8d96a3",size=2)
params=[("WIDTH","40%"),("LEVEL","100%"),("OFFSET","0%"),("PHASE","0°"),("PROB","85%"),("FLEX","SWING 12%"),("CROSS","AND ← 1"),("QUANT","—"),("LOOP","4 b · 1/3")]
for k,(n,v) in enumerate(params):
    yy=DY+9+k*4.6
    st(DX+22,yy,n,fill="#8d96a3",size=2.1)
    st(DX+40,yy,v,fill="#f4efe6" if n!="PROB" else "#f3a712",anchor="end",size=2.2,weight=700)
# CV badge on PROB
a(f'<circle cx="{DX+40.6}" cy="{DY+9+4*4.6-0.8}" r="0.8" fill="{CVC[1]}"/>')
# mini scope
a(f'<rect x="{DX+2}" y="{DY+36}" width="17" height="14" fill="#0e1116" stroke="#2c3440" stroke-width="0.25"/>')
wave("PULSE",DX+3,DY+38,15,10,4,LANE[2],[1,0,0,1])
st(DX+10.5,DY+52.3,"SCOPE",anchor="middle",fill="#8d96a3",size=1.9)
# tab bar
TABS=["HOME","WAVE","EUCLID","LOOP","LOGIC","CV","BANK"]
TY=SY+SH-6
a(f'<rect x="{SX}" y="{TY}" width="{SW}" height="6" fill="#1b2028"/>')
PITCH=14.4; TX0=SX+8
for k,t in enumerate(TABS):
    x=TX0+k*PITCH
    if k==0: a(f'<rect x="{x-6.6}" y="{TY+0.8}" width="13.2" height="4.4" rx="0.8" fill="#f4efe6"/>')
    st(x,TY+4,t,anchor="middle",fill="#0e1116" if k==0 else "#9fb3c8",size=2.3,weight=700)
# page buttons under screen
for k,t in enumerate(TABS):
    x=TX0+k*PITCH
    button(x,90,s=6,lit="#f4efe6" if k==0 else None)
# encoder at right of page buttons
knob(SX+SW-8,90,r=5.5,ang=60)
text(SX+SW-8,99.6,"DATA",size=3.2)
text(TX0+3*PITCH,97.8,"PAGES",size=2.9,fill=MUTED,weight=800)

# ---------- per-output control bank (acts on the selected output)
BX0=172; BX1=W-3; BY0=12; BH=17.2
ROWS=[[("RATE",1,None,40),("PHASE",0,None,-120),("PROB",0,1,90),("FLEX",1,None,-60),("AMOUNT",0,None,-100)],
      [("SHAPE",1,None,70),("WIDTH",0,None,-20),("SLEW",0,None,-140),("LEVEL",0,0,140),("OFFSET",0,None,-140)],
      [("STEPS",1,None,-40),("TRIGS",1,None,-100),("PAD",1,None,-140),("RAND",0,None,-140),("SHIFT",1,None,-120)],
      [("LOOP",1,None,-60),("NAP",1,None,-120),("WAKE",1,None,-90),("SHIFT",1,None,-140),None],
      [("CROSS",1,None,80),("SOURCE",1,None,-90),("SCALE",1,None,-140),("INVERT","btn",None,0),None]]
PITCH=20.3; CX0=BX0+8+PITCH/2+0.5
for r,(row,(bname,bcol)) in enumerate(zip(ROWS,BANDS)):
    y0=BY0+r*BH
    a(f'<rect x="{BX0}" y="{y0+0.4}" width="{BX1-BX0}" height="{BH-0.8}" rx="2" fill="{bcol}"/>')
    a(f'<rect x="{BX0}" y="{y0+0.4}" width="6.5" height="{BH-0.8}" rx="2" fill="{INK}" fill-opacity="0.85"/>')
    nm=bname.split(" / ")[0] if r==4 else bname
    nm="LOGIC" if r==4 else nm
    text(BX0+3.25,y0+BH/2,nm,size=2.9,fill="#f4efe6",weight=800,extra=f'transform="rotate(-90 {BX0+3.25} {y0+BH/2})" dominant-baseline="middle"')
    for c,item in enumerate(row):
        if not item: continue
        lab,kind,cv,ang=item
        x=CX0+c*PITCH; ky=y0+6.6
        if kind=="btn":
            button(x,ky,s=6.5,lit="#3a3734"); text(x,y0+15.1,lab)
        else:
            knob(x,ky,r=4.4,ang=ang,stepped=bool(kind),cv=cv); text(x,y0+15.1,lab)
# note in empty slot
text(CX0+4*PITCH,BY0+3*BH+7.2,"knobs follow",size=2.2,fill=MUTED,weight=600)
text(CX0+4*PITCH,BY0+3*BH+10.0,"selected out",size=2.2,fill=MUTED,weight=600)
text(CX0+4*PITCH,BY0+4*BH+7.2,"● = CV",size=2.2,fill=MUTED,weight=600)
text(CX0+4*PITCH,BY0+4*BH+10.0,"assigned",size=2.2,fill=MUTED,weight=600)

# ---------- outputs row
OX0=48; OX1=W-3; OY=101
a(f'<rect x="{OX0-1.5}" y="{OY}" width="{OX1-OX0+1.5}" height="20.5" rx="2" fill="#e2dbcd"/>')
text(OX0+1,OY+4.3,"OUTPUTS",size=2.9,anchor="start",fill=MUTED,weight=800)
text(OX0+16.5,OY+10.2,"SELECT",size=2.9,anchor="end",fill=MUTED,weight=800)
text(OX0+16.5,OY+17.4,"MUTE",size=2.9,anchor="end",fill=MUTED,weight=800)
CP=(OX1-OX0-19)/8
for i in range(8):
    c=OX0+19+i*CP
    bx=c+5
    button(bx,OY+9.2,s=6.4,lit=LANE[i] if i==2 else None,fill="#3a3734",label=str(i+1),lc="#0e1116" if i==2 else LANE[i],lsize=3.4)
    button(bx,OY+16.8,s=4.6,lit="#c0392b" if i==6 else None)
    jack(c+16.5,OY+10.5)
    a(f'<rect x="{c+13}" y="{OY+16.3}" width="7" height="1.0" rx="0.5" fill="{LANE[i]}"/>')
    led(c+21.6,OY+5.4,LANE[i],on=(i not in (6,)))

svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W:.2f} {H}" width="{W*8:.0f}" height="{H*8:.0f}">'+"".join(O)+'</svg>'
open("panel.svg","w").write(svg)
html=f'''<html><head><style>@font-face{{font-family:"Climate Crisis";src:url(ClimateCrisis.ttf)}}body{{margin:0;background:#888}}</style></head><body>{svg}</body></html>'''
open("panel.html","w").write(html)
