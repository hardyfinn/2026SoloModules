import re,math
logo=open('/mnt/project-files/vcv-haer-logo/Haer-ClimateCrisis-light.svg').read()
d=re.search(r' d="([^"]+)"',logo).group(1)
W,H=111.76,128.5
K=W/101.6
BG="#1c2440"; INK="#efe6d2"; ACC="#e8a33a"; DIM="#9aa3c8"
o=[]
def t(x,y,s,size=3.2,anchor="middle",fill=INK,weight=700,ls=0.15):
    o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" text-anchor="{anchor}" fill="{fill}" font-weight="{weight}" letter-spacing="{ls}" font-family="DejaVu Sans, sans-serif">{s}</text>')
def knob(x,y,r,label,ang=-40,col="#d9d2c0"):
    o.append(f'<circle cx="{x}" cy="{y}" r="{r+0.8}" fill="#11162a"/><circle cx="{x}" cy="{y}" r="{r}" fill="{col}" stroke="#555" stroke-width="0.3"/>')
    a=math.radians(ang-90)
    o.append(f'<line x1="{x+math.cos(a)*r*0.2:.2f}" y1="{y+math.sin(a)*r*0.2:.2f}" x2="{x+math.cos(a)*r*0.92:.2f}" y2="{y+math.sin(a)*r*0.92:.2f}" stroke="#111" stroke-width="{max(0.7,r*0.14):.2f}" stroke-linecap="round"/>')
    if label: t(x,y-r-2.4,label)
def led(x,y,on=False,c=ACC,r=0.9):
    o.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{c if on else "#3a4366"}"/>')
def jack(x,y,label,out=False):
    t(x,y-4.9,label,2.9,fill=(BG if out else INK))
    o.append(f'<circle cx="{x:.2f}" cy="{y}" r="3.1" fill="#b9b9b9"/><circle cx="{x:.2f}" cy="{y}" r="2.2" fill="#222"/><circle cx="{x:.2f}" cy="{y}" r="1.4" fill="#000"/>')
def button(x,y,label,lit=False):
    t(x,y-5.2,label)
    o.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="#11162a"/><circle cx="{x}" cy="{y}" r="3.0" fill="{ACC if lit else "#cfc8b6"}"/>')
o.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
o.append(f'<rect width="{W}" height="3" fill="#141a30"/><rect y="{H-3}" width="{W}" height="3" fill="#141a30"/>')
for sx in (7.5,W-7.5):
    for sy in (1.5,H-1.5): o.append(f'<circle cx="{sx}" cy="{sy}" r="1.1" fill="#888"/>')
t(W/2,10.0,"ECTOPLASM",6.2,ls=0.9)
t(W/2,14.0,"engine: Ectocore firmware by Zack Scholl (GPL-3)",2.3,fill=DIM,weight=400,ls=0.05)
o.append(f'<rect x="8" y="16" width="{W-16}" height="7.4" rx="1.2" fill="#0b0f1d" stroke="#3a4366" stroke-width="0.3"/>')
o.append(f'<text x="{W/2}" y="21.1" text-anchor="middle" font-size="3.5" fill="#7fe0c4" font-family="DejaVu Sans Mono, monospace">B01 S03 · 172.0 BPM · RUNE 4 · x1</text>')
bx,by=W/2,40
cols=["#e8a33a","#e86a3a","#d94c7a","#7fe0c4"]
for i in range(16):
    a=math.radians(-135+i*270/15-90)
    led(bx+math.cos(a)*13.2,by+math.sin(a)*13.2,on=i<10,c=cols[i%4],r=1.0)
knob(bx,by,9.5,None,30)
t(bx,54.2,"BREAK")
gx,gy=16.5,38; knob(gx,gy,6.2,"GRIMOIRE",-20)
for i in range(7): led(gx-9+i*3,gy+9.4,on=i==3,r=0.8)
t(gx,gy+13.2,"rune 1–7",2.3,fill=DIM,weight=400)
ax,ay=W-16.5,38; knob(ax,ay,6.2,"AMEN",60)
t(ax,ay+10.6,"straight → random",2.3,fill=DIM,weight=400)
for x,l,a in zip([x*K for x in [13.5,38,63.6,88.1]],["SAMPLE","BANK","JUMP","TEMPO"],[-90,-120,0,10]): knob(x,64.5,5.0,l,a)
o.append(f'<rect x="5" y="71.6" width="{W-10}" height="13.4" rx="1.5" fill="none" stroke="{DIM}" stroke-width="0.3" stroke-dasharray="1 0.8"/>')
o.append(f'<rect x="{W/2-21}" y="70.4" width="42" height="2.6" fill="{BG}"/>')
t(W/2,72.4,"hidden on the hardware (hold TAP)",2.1,fill=DIM,weight=400,ls=0.02)
for x,l,a in zip([19*K,W/2,82.6*K],["DRIVE","DJ FILTER","GATE"],[20,0,-60]): knob(x,80.6,3.4,l,a,col="#efe6d2")
for x,l,lit in zip([13.5*K,32*K,W/2,88.1*K],["TAP","PLAY","MODE","MULT"],[True,False,False,False]): button(x,94,l,lit)
for i,k in enumerate("KSTR"):
    lx=W/2+6.4+(i%2)*6.4; ly=92.4+(i//2)*3.4
    led(lx,ly,on=i==0,r=0.8); t(lx+2.2,ly+0.8,k,2.1,fill=DIM,anchor="middle")
t(70.6,94.4,"",2)
xs=[8.4+i*13.57 for i in range(8)]
for x,l in zip(xs,["CLOCK","RESET","AMEN","BREAK","SAMPLE","BANK","GRIM","PLAY"]): jack(x,107.6,l)
o.append(f'<rect x="{xs[4]-5.6}" y="111.8" width="{xs[7]-xs[4]+11.2}" height="11.6" rx="1.3" fill="{ACC}"/>')
for i,(x,l) in enumerate(zip(xs,["DRIVE","FILTER","GATE","TEMPO","TRIG","CLK","L","R"])): jack(x,119.4,l,out=i>=4)
cap=2.6; s=cap/750; lw=3070*s
o.append(f'<g transform="translate({W/2-lw/2:.2f},{H-0.25:.2f}) scale({s:.5f})"><path fill="{INK}" d="{d}"/></g>')
svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*6}" height="{H*6}">{"".join(o)}</svg>'
open('panel.svg','w').write(svg)
html=f'<html><body style="margin:0;background:#888;display:flex;gap:30px;padding:24px;align-items:flex-start">{svg}</body></html>'
open('panel.html','w').write(html)
