import math,random
S=6; bg="#16171b"; ink="#f2efe6"; acc="#ff7a1a"; dim="#9a9a9a"
COL={"K":"#ff4d4d","S":"#ffd23f","H":"#46d3ff","O":"#b98cff"}
W=182.88;H=128.5
def knob(x,y,l,r=5.2,c=acc):
  return f'<circle cx="{x*S}" cy="{y*S}" r="{r*S}" fill="#2b2a30" stroke="{c}" stroke-width="2.5"/><line x1="{x*S}" y1="{y*S}" x2="{(x+ (r-1)*0.6)*S}" y2="{(y-(r-1)*0.8)*S}" stroke="{ink}" stroke-width="3"/><text x="{x*S}" y="{(y+r+4.2)*S}" class="c">{l}</text>'
def jack(x,y,l,out=False,c=None,lab_above=True):
  b=f'<rect x="{(x-4.6)*S}" y="{(y-4.6)*S}" width="{9.2*S}" height="{9.2*S}" rx="8" fill="{c or acc}"/>' if out else ''
  ty=(y-6.2) if lab_above else (y+8.3)
  return b+f'<circle cx="{x*S}" cy="{y*S}" r="{3.2*S}" fill="#888"/><circle cx="{x*S}" cy="{y*S}" r="{1.8*S}" fill="#111"/><text x="{x*S}" y="{ty*S}" class="j">{l}</text>'
def btn(x,y,l,on=False):
  return f'<rect x="{(x-4.5)*S}" y="{(y-3)*S}" width="{9*S}" height="{6*S}" rx="6" fill="{acc if on else "#2b2a30"}" stroke="{acc}" stroke-width="2"/><text x="{x*S}" y="{(y+7.6)*S}" class="c">{l}</text>'
def note(x,y,t,sz=2.4,anchor="start"): return f'<text x="{x*S}" y="{y*S}" style="font-size:{sz*S}px;fill:{dim};font-weight:400;text-anchor:{anchor}">{t}</text>'
p=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*S}" height="{H*S}"><style>@import url("https://fonts.googleapis.com/css2?family=Climate+Crisis");text{{font-family:"Helvetica Neue",Arial;font-weight:700}} .c{{font-size:{3.2*S}px;fill:{ink};text-anchor:middle}} .j{{font-size:{2.9*S}px;fill:{ink};text-anchor:middle}} .m{{font-family:monospace;font-weight:700}}</style>',
f'<rect width="{W*S}" height="{H*S}" fill="{bg}"/>',
f'<rect width="{W*S}" height="{2*S}" fill="{acc}"/>',
f'<text x="{5*S}" y="{11*S}" style="font-size:{7*S}px;fill:{ink};font-weight:900;letter-spacing:3px">BREAKER</text>',
note(52,10.6,"break analyser + drum pattern generator",2.6)]

# ---- waveform display
dx,dy,dw,dh=5,14,W-10,24
p.append(f'<rect x="{dx*S}" y="{dy*S}" width="{dw*S}" height="{dh*S}" rx="8" fill="#0b0c0f" stroke="#333"/>')
p.append(f'<text x="{(dx+2.5)*S}" y="{(dy+4.4)*S}" class="m" style="font-size:{2.8*S}px;fill:{ink}">my_break.wav   2 BARS   171.4 BPM</text>')
cnt=f'<tspan fill="{COL["K"]}">K 6</tspan>  <tspan fill="{COL["S"]}">S 9</tspan>  <tspan fill="{COL["H"]}">H 4</tspan>  <tspan fill="{COL["O"]}">O 2</tspan>'
p.append(f'<text x="{(dx+dw-2.5)*S}" y="{(dy+4.4)*S}" class="m" style="font-size:{2.8*S}px;text-anchor:end">{cnt}</text>')
random.seed(7); n=int((dw-5)*S); mid=(dy+15)*S; x0=(dx+2.5)*S
hits=[(0,"K"),(2,"H"),(4,"S"),(6,"H"),(7,"S"),(9,"S"),(10,"K"),(11,"K"),(12,"S"),(15,"S"),(16,"K"),(18,"H"),(20,"S"),(22,"O"),(23,"S"),(25,"S"),(26,"K"),(27,"K"),(28,"S"),(30,"O"),(31,"H")]
amp={"K":1,"S":.85,"H":.35,"O":.55}; dec={"K":4,"S":6,"H":14,"O":8}
env=[0.03]*n
for st,t in hits:
  s=int(st*n/32)
  for i in range(s,min(n,s+int(n/8))):
    ph=(i-s)/(n/32); env[i]=max(env[i],amp[t]*math.exp(-ph*dec[t]/4))
pts=[]
for i in range(n):
  a=env[i]*(7.5*S)*(0.55+0.45*random.random()); x=x0+i; pts.append(f'M{x},{mid-a}L{x},{mid+a}')
p.append(f'<path d="{"".join(pts)}" stroke="#6f7a72" stroke-width="1"/>')
for st,t in hits:
  x=x0+st*n/32
  p.append(f'<line x1="{x}" y1="{(dy+6.2)*S}" x2="{x}" y2="{(dy+22.5)*S}" stroke="{COL[t]}" stroke-width="2.5"/>')
  p.append(f'<rect x="{x-2.1*S}" y="{(dy+5.8)*S}" width="{4.2*S}" height="{3.3*S}" rx="3" fill="{COL[t]}"/><text x="{x}" y="{(dy+8.4)*S}" class="m" style="font-size:{2.5*S}px;fill:#111;text-anchor:middle">{t}</text>')
xs=x0+22*n/32
p.append(f'<rect x="{xs-2.7*S}" y="{(dy+5.2)*S}" width="{5.4*S}" height="{17.8*S}" rx="4" fill="none" stroke="#fff" stroke-width="2.5" stroke-dasharray="6 4"/>')
p.append(f'<text x="{xs+3.2*S}" y="{(dy+21.8)*S}" class="m" style="font-size:{2.2*S}px;fill:#fff">click a hit: K S H O</text>')
# ---- pattern grid
gx,gy,gw=5,40,W-10
p.append(f'<rect x="{gx*S}" y="{gy*S}" width="{gw*S}" height="{24.5*S}" rx="8" fill="#0b0c0f" stroke="#333"/>')
p.append(f'<text x="{(gx+2.5)*S}" y="{(gy+4.2)*S}" class="m" style="font-size:{2.7*S}px;fill:{ink}">JUNGLE   BAR 1/2   MUTATE 34%</text>')
p.append(f'<text x="{(gx+gw-2.5)*S}" y="{(gy+4.2)*S}" class="m" style="font-size:{2.7*S}px;fill:{acc};text-anchor:end">LOCKED: K S</text>')
lanes={"K":"1000000000100000","S":"0000100101001001","H":"1010101010111010","O":"0000000000000100"}
ghost={"S":[7,9,15]}; locked=["K","S"]; mutated={"H":[9,10],"O":[13]}
cw=(gw-14)*S/16; play=6
for li,(t,row) in enumerate(lanes.items()):
  y=(gy+6.2+li*4.5)*S
  p.append(f'<text x="{(gx+3.2)*S}" y="{y+3.1*S}" class="m" style="font-size:{3*S}px;fill:{COL[t]}">{t}</text>')
  if t in locked: p.append(f'<rect x="{(gx+7)*S}" y="{y+0.8*S}" width="{2*S}" height="{2*S}" rx="2" fill="{acc}"/>')
  for k,c in enumerate(row):
    x=(gx+11)*S+k*cw; on=c=="1"; g=k in ghost.get(t,[]); mu=k in mutated.get(t,[])
    fill=COL[t] if on else ("#22252b" if k%4 else "#30343d")
    p.append(f'<rect x="{x+3}" y="{y}" width="{cw-6}" height="{3.6*S}" rx="4" fill="{fill}" opacity="{".45" if g else "1"}"/>')
    if mu and on: p.append(f'<rect x="{x+3}" y="{y}" width="{cw-6}" height="{3.6*S}" rx="4" fill="none" stroke="#fff" stroke-width="2" stroke-dasharray="4 3"/>')
xp=(gx+11)*S+play*cw
p.append(f'<rect x="{xp+1}" y="{(gy+5.6)*S}" width="{cw-2}" height="{18.4*S}" rx="4" fill="none" stroke="#fff" stroke-width="2.5"/>')
# ---- knobs
cols=[16,46,76,106,136,166]
p.append(note(16-5.2,70.2,"",2))
for x,l in zip(cols,["STYLE","DENSITY","MUTATE","GHOSTS","FILLS","SWING"]): p.append(knob(x,71.5,l))
for x,l in zip(cols,["SENS","BARS","PITCH","DECAY","VARIETY","LEVEL"]): p.append(knob(x,86.5,l,4.4,"#8a8f99"))
# ---- jacks
ins=[("CLOCK","RESET","NEW","LOCK"),("MUTATE","STYLE","DENS","V/OCT")]
for r,row in enumerate(ins):
  for i,l in enumerate(row): p.append(jack(10+i*14,106+r*14,l))
def btn2(x,y,l,on=False):
  return f'<rect x="{(x-4.5)*S}" y="{(y-3)*S}" width="{9*S}" height="{6*S}" rx="6" fill="{acc if on else "#2b2a30"}" stroke="{acc}" stroke-width="2"/><text x="{x*S}" y="{(y-4.6)*S}" class="c">{l}</text>'
p.append(btn2(68,106,"NEW")); p.append(btn2(68,120,"LOCK",True)); p.append(btn2(81,106,"LOAD")); p.append(btn2(81,120,"RUN",True))
p.append(f'<rect x="{88.5*S}" y="{97*S}" width="{91*S}" height="{30*S}" rx="10" fill="#24201c" stroke="{acc}" stroke-width="1.5"/>')
outs=[("KICK","K"),("SNARE","S"),("HAT","H"),("OTHER","O")]
for i,(l,t) in enumerate(outs):
  x=95+i*14.2
  p.append(jack(x,106,l,True,COL[t])); p.append(jack(x,120,t+" GATE",True,COL[t]))
p.append(jack(155.5,106,"L",True)); p.append(jack(170,106,"R",True))
p.append(jack(155.5,120,"ACCENT",True,"#cccccc")); p.append(jack(170,120,"CLK",True,"#cccccc"))
p.append(f'<text x="{(W-5)*S}" y="{11*S}" style="font-family:\'Climate Crisis\';font-size:{5.5*S}px;fill:{acc}" text-anchor="end">H\u00e6r</text>')
p.append('</svg>')
open('breaker-panel.svg','w').write("".join(p))
