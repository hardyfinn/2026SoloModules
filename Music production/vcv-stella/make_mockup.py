import random, re, math
random.seed(7)
W,H=182.88,128.5
BG="#151b30"; INK="#efe6d2"; DIM="#9aa3bd"; PLATE="#0d1122"
VC=["#ffb347","#5fd3e6","#ff7fb0","#b6e05a"]
o=[]
o.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*6}" height="{H*6}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans" font-weight="bold">')
o.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
o.append(f'<rect y="0" width="{W}" height="2" fill="{PLATE}"/><rect y="{H-2}" width="{W}" height="2" fill="{PLATE}"/>')
for x in (7.5,W-7.5):
  for y in (3,H-3): o.append(f'<circle cx="{x}" cy="{y}" r="1.5" fill="#3a4466"/>')
def T(x,y,s,size=3.2,fill=INK,anchor="middle",w="bold"):
  o.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{w}">{s}</text>')
def knob(x,y,r=4.6,col=INK):
  o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#2b3352" stroke="#4a5580" stroke-width="0.5"/>')
  o.append(f'<circle cx="{x}" cy="{y}" r="{r*0.78}" fill="#39436a"/>')
  a=math.radians(-130); o.append(f'<line x1="{x}" y1="{y}" x2="{x+r*0.8*math.sin(a)}" y2="{y-r*0.8*math.cos(a)}" stroke="{col}" stroke-width="0.8" stroke-linecap="round"/>')
def jack(x,y,out=False):
  if out: o.append(f'<rect x="{x-4.6}" y="{y-4.6}" width="9.2" height="9.2" rx="1.2" fill="{INK}"/>')
  o.append(f'<circle cx="{x}" cy="{y}" r="3.9" fill="#c9ccd6" stroke="#6b7088" stroke-width="0.4"/>')
  o.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#8d92a6"/><circle cx="{x}" cy="{y}" r="1.6" fill="#0a0a0a"/>')
# title
T(5,9.2,"STELLA",6,INK,"start"); T(38.5,9.2,"SAMPLE MAP · 4 VOICES",2.9,DIM,"start")
# display
dx,dy,ds=5,12.5,78
o.append(f'<rect x="{dx-0.8}" y="{dy-0.8}" width="{ds+1.6}" height="{ds+1.6}" rx="1.5" fill="#05070f" stroke="#3a4466" stroke-width="0.4"/>')
o.append(f'<clipPath id="c"><rect x="{dx}" y="{dy}" width="{ds}" height="{ds}"/></clipPath><g clip-path="url(#c)">')
for g in range(1,4):
  p=dx+ds*g/4; q=dy+ds*g/4
  o.append(f'<line x1="{p}" y1="{dy}" x2="{p}" y2="{dy+ds}" stroke="#141a30" stroke-width="0.25"/><line x1="{dx}" y1="{q}" x2="{dx+ds}" y2="{q}" stroke="#141a30" stroke-width="0.25"/>')
clusters=[(0.2,0.75,0.09,40,"#ffd9a0"),(0.78,0.2,0.1,45,"#d8f4ff"),(0.55,0.55,0.12,40,"#ffffff"),(0.25,0.3,0.08,25,"#ffc8de"),(0.8,0.7,0.08,30,"#e4ffc0"),(0.5,0.85,0.07,18,"#ffffff")]
pts=[]
for cx,cy,sd,n,c in clusters:
  for i in range(n):
    x=min(0.97,max(0.03,random.gauss(cx,sd))); y=min(0.95,max(0.08,random.gauss(cy,sd)))
    pts.append((x,y,c))
for x,y,c in pts:
  o.append(f'<circle cx="{dx+x*ds}" cy="{dy+y*ds}" r="{random.choice([0.35,0.45,0.55])}" fill="{c}" opacity="{random.uniform(0.45,0.95):.2f}"/>')
voices=[(0.22,0.72,0.11),(0.76,0.24,0.08),(0.55,0.52,0.16),(0.78,0.72,0.06)]
for i,(vx,vy,r) in enumerate(voices):
  X=dx+vx*ds; Y=dy+vy*ds; c=VC[i]
  o.append(f'<circle cx="{X}" cy="{Y}" r="{r*ds}" fill="{c}" fill-opacity="0.07" stroke="{c}" stroke-width="0.35" stroke-dasharray="1 0.8"/>')
  o.append(f'<line x1="{X-2.2}" y1="{Y}" x2="{X+2.2}" y2="{Y}" stroke="{c}" stroke-width="0.4"/><line x1="{X}" y1="{Y-2.2}" x2="{X}" y2="{Y+2.2}" stroke="{c}" stroke-width="0.4"/>')
  near=min(pts,key=lambda p:(p[0]-vx-0.03)**2+(p[1]-vy+0.02)**2)
  o.append(f'<circle cx="{dx+near[0]*ds}" cy="{dy+near[1]*ds}" r="1.1" fill="none" stroke="{c}" stroke-width="0.4"/>')
  T(X+2.6,Y-1.4,str(i+1),2.4,c,"start")
o.append('</g>')
o.append(f'<rect x="{dx}" y="{dy}" width="{ds}" height="5" fill="#05070f" opacity="0.85"/>')
T(dx+1.5,dy+3.6,"techno-kit · 198 samples",2.4,"#cfd6ea","start","normal"); T(dx+ds-1.5,dy+3.6,"FIXED",2.4,"#ffb347","end")
T(dx+ds-1.5,dy+ds-1.4,"BRIGHT →",2.2,"#7f8aa8","end","normal")
o.append(f'<text x="{dx+2.6}" y="{dy+ds-1.5}" font-size="2.2" fill="#7f8aa8" font-weight="normal" transform="rotate(-90 {dx+2.6} {dy+ds-1.5})">LONG →</text>')
# global row
ly,cy=98.5,106
T(10,ly,"AXIS"); o.append(f'<rect x="8.4" y="{cy-4}" width="3.2" height="8" rx="0.8" fill="#2b3352" stroke="#4a5580" stroke-width="0.4"/><rect x="8.8" y="{cy+0.2}" width="2.4" height="3.4" rx="0.5" fill="{INK}"/>')
T(15.2,cy-1.6,"AUTO",2.9,DIM,"start","normal"); T(15.2,cy+3.6,"FIXED",2.9,INK,"start")
T(29,ly,"LOAD"); o.append(f'<circle cx="29" cy="{cy}" r="3.4" fill="#39436a" stroke="#4a5580" stroke-width="0.5"/><circle cx="29" cy="{cy}" r="2.3" fill="{INK}" opacity="0.85"/>')
T(41.5,ly,"DRIFT"); knob(41.5,cy)
T(52.5,ly,"CV",2.9); jack(52.5,cy)
T(65.5,ly,"GRAVITY"); knob(65.5,cy)
T(78,ly,"MIX",2.9); jack(78,cy,True)
o.append(f'<text x="78" y="{cy+0.1}" font-size="0" />')
# voice grid
cols=[111,129,147,165]
rows=[("TRIG",19,"j"),("X",31,"k"),("X CV",42.5,"j"),("Y",54,"k"),("Y CV",65.5,"j"),("SPREAD",77,"k"),("CHANCE",89,"k"),("CHANCE CV",100.5,"j"),("OUT",112.5,"o")]
for i,cx in enumerate(cols):
  o.append(f'<circle cx="{cx}" cy="9.6" r="3" fill="{VC[i]}"/>'); T(cx,10.9,str(i+1),3.6,"#151b30")
o.append(f'<rect x="{cols[0]-8}" y="106.6" width="{cols[-1]-cols[0]+16}" height="11.8" rx="1.5" fill="{PLATE}"/>')
for name,y,kind in rows:
  T(101.5,y+1.1,name,3.2 if kind=="k" else 2.9,INK if kind!="j" else DIM,"end")
  o.append(f'<line x1="102.5" y1="{y}" x2="{cols[-1]+8}" y2="{y}" stroke="#262e4a" stroke-width="0.25"/>')
  for i,cx in enumerate(cols):
    if kind=="k": knob(cx,y,4.6,VC[i])
    else: jack(cx,y,kind=="o")
# logo
src=open('/mnt/project-files/vcv-haer-logo/Haer-ClimateCrisis-light.svg').read()
paths=re.findall(r'<path[^>]*d="([^"]+)"',src)
s=3.0/750
o.append(f'<g transform="translate({11.5-20*s},{125.5}) scale({s})" fill="{INK}">'+''.join(f'<path d="{d}"/>' for d in paths)+'</g>')
o.append('</svg>')
open('stella.svg','w').write('\n'.join(o))
