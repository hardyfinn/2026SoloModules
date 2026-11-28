import math,random
S=6; bg="#17161a"; ink="#f2efe6"; acc="#ffd23f"; grn="#3ddc84"
def knob(x,y,l,r=5.2):
  return f'<circle cx="{x*S}" cy="{y*S}" r="{r*S}" fill="#2b2a30" stroke="{acc}" stroke-width="2"/><line x1="{x*S}" y1="{y*S}" x2="{x*S}" y2="{(y-r+1)*S}" stroke="{ink}" stroke-width="3"/><text x="{x*S}" y="{(y+r+4.2)*S}" class="c">{l}</text>'
def jack(x,y,l,out=False):
  b=f'<rect x="{(x-4.6)*S}" y="{(y-4.6)*S}" width="{9.2*S}" height="{9.2*S}" rx="8" fill="{acc}"/>' if out else ''
  t=f'<text x="{x*S}" y="{((y+8) if out else (y-6))*S}" class="j">{l}</text>'
  return b+f'<circle cx="{x*S}" cy="{y*S}" r="{3.2*S}" fill="#888"/><circle cx="{x*S}" cy="{y*S}" r="{1.8*S}" fill="#111"/>'+t
def btn(x,y,l,on=False):
  return f'<rect x="{(x-4)*S}" y="{(y-3)*S}" width="{8*S}" height="{6*S}" rx="6" fill="{acc if on else "#2b2a30"}" stroke="{acc}" stroke-width="2"/><text x="{x*S}" y="{(y+7.6)*S}" class="c">{l}</text>'
def head(W,H,title):
  return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*S}" height="{H*S}"><style>text{{font-family:"Helvetica Neue",Arial;font-weight:700}} .c{{font-size:{3.2*S}px;fill:{ink};text-anchor:middle}} .j{{font-size:{2.9*S}px;fill:{ink};text-anchor:middle}} .m{{font-family:monospace;fill:{grn}}}</style>',
  f'<rect width="{W*S}" height="{H*S}" fill="{bg}"/>',
  f'<text x="{5*S}" y="{11*S}" style="font-size:{7*S}px;fill:{ink};font-weight:900;letter-spacing:2px">{title}</text>',
  f'<text x="{(W-5)*S}" y="{11*S}" style="font-family:\'Climate Crisis\';font-size:{5.5*S}px;fill:{acc}" text-anchor="end">Hær</text>']
def note(x,y,t,sz=2.6): return f'<text x="{x*S}" y="{y*S}" style="font-size:{sz*S}px;fill:#9a9a9a">{t}</text>'

# ---------- PLAYER 20 HP ----------
W=101.6;H=128.5;p=head(W,H,"BREAK")
dx,dy,dw,dh=5,15,W-10,30
p.append(f'<rect x="{dx*S}" y="{dy*S}" width="{dw*S}" height="{dh*S}" rx="8" fill="#0b0f0c" stroke="#333"/>')
p.append(f'<text x="{(dx+2.5)*S}" y="{(dy+5)*S}" class="m" style="font-size:{2.8*S}px">amen_bar1.wav  16 SL  SLICE 5 R</text>')
random.seed(3); mid=(dy+18)*S; n=int((dw-5)*S); pts=[]
for i in range(n):
  k=int(i/(n/16)); ph=(i%(n/16))/(n/16)
  a=(1 if k in [0,2,4,7,10,11,12] else .45)*math.exp(-ph*5)*(9*S)*(0.6+0.4*random.random())+S*0.5*random.random()
  x=dx*S+2.5*S+i; pts.append(f'M{x},{mid-a}L{x},{mid+a}')
x0=(dx+2.5)*S+4*n/16
p.append(f'<rect x="{x0}" y="{(dy+7)*S}" width="{n/16}" height="{21*S}" fill="{acc}" opacity=".25"/>')
p.append(f'<path d="{"".join(pts)}" stroke="{grn}" stroke-width="1" opacity=".85"/>')
for k in range(17):
  x=(dx+2.5)*S+k*n/16; p.append(f'<line x1="{x}" y1="{(dy+7)*S}" x2="{x}" y2="{(dy+28)*S}" stroke="{acc}" stroke-width="1" opacity=".55"/>')
for i,l in enumerate(["SLICES","PITCH","CRUNCH"]): p.append(knob(17+i*33.8,54,l))
for i,l in enumerate(["DECAY","START","LEVEL"]): p.append(knob(17+i*33.8,71,l))
p.append(btn(17,86,"LOAD")); p.append(btn(50.8,86,"REPITCH",True)); p.append(note(68,88,"lit = Akai-style"))
for i,l in enumerate(["TRIG","SLICE","REV","STUTTER","V/OCT"]): p.append(jack(12+i*19.4,108,l))
for i,l in enumerate(["L","R"]): p.append(jack(12+i*19.4,120,l,True))
p.append(note(50,119,"sits right of CHOP:")); p.append(note(50,123,"plays with no cables"))
p.append('</svg>'); P1="".join(p)

# ---------- SEQUENCER 30 HP ----------
W=152.4;H=128.5;p=head(W,H,"CHOP")
dx,dy,dw,dh=5,15,W-10,40
p.append(f'<rect x="{dx*S}" y="{dy*S}" width="{dw*S}" height="{dh*S}" rx="8" fill="#0b0f0c" stroke="#333"/>')
p.append(f'<text x="{(dx+2.5)*S}" y="{(dy+5)*S}" class="m" style="font-size:{2.8*S}px">P25 3-3-2 KICK   LEN 32   EDIT: STEP 4  SLICE 1</text>')
bars=["123123125678DEFG","12312312567DDDDD"]; mods={28:"S2",29:"S3",30:"S4",31:"S4"}
cw=(dw-5)*S/16
for b,row in enumerate(bars):
  for k,c in enumerate(row):
    i=b*16+k; x=(dx+2.5)*S+k*cw; y=(dy+10+b*14)*S
    cur=i==9; sel=i==3
    fill=acc if cur else ("#2e4a36" if sel else "#1d2a20")
    p.append(f'<rect x="{x+2}" y="{y}" width="{cw-4}" height="{8*S}" rx="4" fill="{fill}" {"stroke=\"#fff\" stroke-width=\"3\"" if sel else ""}/>')
    p.append(f'<text x="{x+cw/2}" y="{y+5.6*S}" class="m" style="font-size:{3.8*S}px;text-anchor:middle;{"fill:#111" if cur else ""}">{c}</text>')
    if i in mods: p.append(f'<text x="{x+cw/2}" y="{y-0.6*S}" style="font-size:{2.3*S}px;fill:{acc};text-anchor:middle;font-family:monospace">{mods[i]}</text>')
    if k%4==0: p.append(f'<rect x="{x+2}" y="{y+8.6*S}" width="{cw-4}" height="{0.6*S}" fill="#555"/>')
for i,l in enumerate(["PATTERN","LENGTH","MUTATE","STUTTER","REVERSE","SWING"]): p.append(knob(15+i*24.5,67,l))
for i,(l,on) in enumerate([("EDIT",1),("REV",0),("STUT",0),("PITCH",0),("FILL",0),("SAVE",0)]): p.append(btn(15+i*24.5,84,l,on))
p.append(note(5,95.5,"EDIT: click a step, drag up/down for slice; REV / STUT / PITCH change that step. SAVE writes over the preset.",2.3))
for i,l in enumerate(["CLOCK","RESET","FILL","PATTERN","MUTATE"]): p.append(jack(12+i*15,108,l))
for i,l in enumerate(["TRIG","SLICE","REV","STUT","PITCH"]): p.append(jack(12+i*15,120,l,True))
for i,l in enumerate(["KICK","SNARE","HAT"]): p.append(jack(97+i*15,120,l,True))
p.append(note(93,106,"drum triggers for a 909 / 707")); p.append(note(93,110,"layered under the break"))
p.append('</svg>'); P2="".join(p)
for name,svg,w in [("player",P1,101.6),("sequencer",P2,152.4)]:
  open(name+'.html','w').write('<html><head><link href="https://fonts.googleapis.com/css2?family=Climate+Crisis&display=block" rel="stylesheet"></head><body style="margin:0">'+svg+'</body></html>')
