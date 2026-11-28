import math
W=142.24; H=128.5
bg="#1f2340"; ink="#f1e9d6"; acc="#ff9a3c"; acc2="#6fd3ff"
o=[]
def t(x,y,s,size=3.2,anchor="middle",col=ink,w="600",fam="Barlow Condensed"):
    o.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{col}" font-family="{fam}" font-weight="{w}">{s}</text>')
def knob(x,y,r,lab,col=ink,left=None,right=None,bip=False):
    o.append(f'<circle cx="{x}" cy="{y}" r="{r+1.6}" fill="none" stroke="{ink}" stroke-opacity=".25" stroke-width=".3" stroke-dasharray=".4 1.2"/>')
    o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#0d0f1f" stroke="{col}" stroke-width=".6"/>')
    a=math.radians(-90 if bip else -135)
    o.append(f'<line x1="{x}" y1="{y}" x2="{x+r*0.85*math.sin(a+math.pi/2*0+0) if False else x}" y2="{y-r*0.85}" stroke="{col}" stroke-width=".8" stroke-linecap="round"/>')
    t(x,y-r-2.6,lab)
    if left: t(x-r-0.8,y+r+1.8,left,2.4,"end",col=ink)
    if right: t(x+r+0.8,y+r+1.8,right,2.4,"start",col=ink)
def trim(x,y,lab=None):
    o.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#0d0f1f" stroke="{ink}" stroke-width=".4"/><line x1="{x}" y1="{y}" x2="{x}" y2="{y-2.2}" stroke="{ink}" stroke-width=".5"/>')
    if lab: t(x,y-4,lab,2.9)
def jack(x,y,lab,out=False):
    if out: o.append(f'<rect x="{x-5.2}" y="{y-8.2}" width="10.4" height="13.6" rx="1.2" fill="{acc}" fill-opacity=".9"/>')
    o.append(f'<circle cx="{x}" cy="{y}" r="3.9" fill="#bfc3cc" stroke="#555" stroke-width=".3"/><circle cx="{x}" cy="{y}" r="1.8" fill="#111"/>')
    t(x,y-5,lab,2.9,col=("#1f2340" if out else ink),w="700")
def sw(x,y,lab,top,bot):
    o.append(f'<rect x="{x-1.6}" y="{y-3.5}" width="3.2" height="7" rx=".8" fill="#0d0f1f" stroke="{ink}" stroke-width=".4"/><rect x="{x-1.1}" y="{y-3}" width="2.2" height="3" fill="{ink}"/>')
    t(x,y-6.6,lab); t(x+2.6,y-1.6,top,2.4,"start"); t(x+2.6,y+3.4,bot,2.4,"start")
o.append(f'<rect width="{W}" height="{H}" fill="{bg}"/>')
o.append(f'<rect y="0" width="{W}" height="4" fill="#15182e"/><rect y="{H-4.5}" width="{W}" height="4.5" fill="#15182e"/>')
t(W/2,11.2,"JETSTREAM",6.2,w="800")
t(W/2,15.2,"phaser · flanger · through-zero",2.9,col=acc)
# screen
o.append(f'<rect x="8" y="18" width="{W-16}" height="15" rx="1.5" fill="#05060d" stroke="{acc2}" stroke-width=".4"/>')
pts=[]
for i in range(200):
    fx=i/199; x=9+fx*(W-18)
    v=0.5+0.42*math.cos(2*math.pi*(fx*fx*6+0.2))*(0.6+0.4*fx)
    pts.append(f"{x:.2f},{18.8+ (1-v)*11.5:.2f}")
o.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{acc2}" stroke-width=".5"/>')
t(10,31.6,"PHASE 8 STAGES · FB -62% · TZ",2.4,"start",col=acc2)
t(W-10,31.6,"LFO 0.25 Hz  ◐ 90°",2.4,"end",col=acc2)
xs=[16,43.5,71.12,98.7,126.2]
y1=46
knob(xs[0],y1,6.2,"MORPH",acc,"PHASE","FLANGE")
knob(xs[1],y1,6.2,"CENTER")
knob(xs[2],y1,6.2,"DEPTH")
knob(xs[3],y1,6.2,"RATE")
knob(xs[4],y1,6.2,"FEEDBACK",acc,"−","+",bip=True)
y2=66
knob(xs[0],y2,4.6,"STAGES",None,"2","24") if False else knob(xs[0],y2,4.6,"STAGES",ink,"2","24")
knob(xs[1],y2,4.6,"SPREAD",ink,"0°","180°")
knob(xs[2],y2,4.6,"SHAPE",ink,"∿","S&amp;H")
knob(xs[3],y2,4.6,"STEPS",ink,"SMOOTH","16")
knob(xs[4],y2,4.6,"MIX",acc,"−WET","+WET",bip=True)
y3=82.3
knob(xs[0],y3,3.4,"GLIDE")
knob(xs[1],y3,3.4,"ENV",ink,"−","+",bip=True)
knob(xs[2],y3,3.4,"RES")
sw(xs[3]-3,y3+0.5,"TZ","ON","OFF")
sw(xs[4]-3,y3+0.5,"LFO","CLK","FREE")
# CV row
cvx=[9+i*17.75 for i in range(8)]
labs=["MORPH","CENTER","DEPTH","RATE","FDBK","STAGES","SPREAD","MIX"]
for x,l in zip(cvx,labs):
    t(x,90.2,l,2.9); trim(x,93.6); o.append(f'<circle cx="{x}" cy="{102}" r="3.9" fill="#bfc3cc" stroke="#555" stroke-width=".3"/><circle cx="{x}" cy="{102}" r="1.8" fill="#111"/>')
# IO row
io=[("IN L",0),("IN R",0),("CLOCK",0),("RESET",0),("SWEEP",0),("LFO",1),("OUT L",1),("OUT R",1)]
for x,(l,ou) in zip(cvx,io): jack(x,118,l,bool(ou))
# logo
t(W/2,H-0.9,"Hær",3.6,col=ink,fam="Climate Crisis",w="400")
svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*6}" height="{H*6}">'+"".join(o)+'</svg>'
html=f'''<html><head><link href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600;700;800&family=Climate+Crisis&display=block" rel="stylesheet"><style>body{{margin:0;background:#111}}</style></head><body>{svg}</body></html>'''
open("mock.html","w").write(html)
