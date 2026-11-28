import os, sys
sys.path.insert(0, "/home/user/simplekits/res")
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
FB="/usr/share/fonts/opentype/inter/Inter-Bold.otf"; FK="/usr/share/fonts/opentype/inter/Inter-Black.otf"
fonts={}
def text(s,x,y,size,font=FB,fill="#1D1E20",anchor="middle",spacing=0.04):
    if font not in fonts:
        f=TTFont(font); fonts[font]=(f,f.getGlyphSet(),f.getBestCmap(),f["head"].unitsPerEm)
    f,gs,cmap,upm=fonts[font]; sc,tr=size/upm,spacing*upm
    names=[cmap[ord(c)] for c in s]; w=sum(gs[n].width+tr for n in names)-tr
    x0={"middle":x-w*sc/2,"end":x-w*sc}.get(anchor,x); pen,cx=SVGPathPen(gs),0.0
    for n in names:
        gs[n].draw(TransformPen(pen,(sc,0,0,-sc,x0+cx*sc,y))); cx+=gs[n].width+tr
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'
W,H=152.4,128.5   # 30 HP
BG,BAND,INK,SOFT,LINE,TRIM,LI="#D9D9D5","#343538","#1D1E20","#5F6166","#B4B4AE","#E5571E","#F1EFE8"
STRIPE=["#D7322B","#E8792A","#F0C030","#F1EFE8"]
TR=[("BASS DRUM","x...x...x...x..."),("SNARE","................"),("LOW TOM","................"),
    ("MID TOM","................"),("HI TOM","................"),("RIM SHOT","...x.....x....x."),
    ("CLAP","....x.......x..."),("CLOSED HAT","xx.xxx.xxx.xxx.x"),("OPEN HAT","..x...x...x...x."),
    ("CRASH","x..............."),("RIDE","................"),("ACCENT","x...x...x...x...")]
ROW0,P=39.8,7.0; GX0,SW=36.0,5.2; CW,CH=4.4,5.0
MUTE_X=GX0+SW*16+4.0; OX1,OX2=MUTE_X+9.2,MUTE_X+17.0
def sx(s): return GX0+SW*s
def ry(r): return ROW0+P*r+(1.2 if r==11 else 0)
def knob(x,y,r,ang,fill="#AEB0B6"):
    import math; a=math.radians(ang-90)
    return (f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="#55575C" stroke-width="0.45"/>'
            f'<line x1="{x}" y1="{y}" x2="{x+math.cos(a)*r*0.8:.2f}" y2="{y+math.sin(a)*r*0.8:.2f}" stroke="#1D1E20" stroke-width="0.7" stroke-linecap="round"/>')
def jack(x,y): return (f'<circle cx="{x}" cy="{y}" r="4.0" fill="#C9CBD0"/><circle cx="{x}" cy="{y}" r="3.1" fill="#7C7F86"/>'
                       f'<circle cx="{x}" cy="{y}" r="1.9" fill="#0E0E0F"/>')
p=[];a=p.append
a(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
a(f'<rect width="{W}" height="19" fill="{BAND}"/>')
for i,c in enumerate(STRIPE): a(f'<rect x="{i*W/4}" y="19" width="{W/4}" height="1.1" fill="{c}"/>')
a(text("JACK",5.5,12.4,8.0,FK,LI,anchor="start",spacing=0.02))
a(text("house + techno",5.6,16.6,2.4,FB,"#A9AAAE",anchor="start"))
TY=11.6
for x,s,kind in ((45,"CLOCK","j"),(60,"RESET","j"),(77,"PATTERN","k"),(93,"CV","j"),(108,"SWING","k"),(123,"FILL","j")):
    a(text(s,x,5.4,3.2 if s not in("CV","FILL") else 2.9,fill=LI if kind=="k" else "#A9AAAE"))
    a(jack(x,TY) if kind=="j" else knob(x,TY,4.6 if s=="PATTERN" else 3.8, 300 if s=="PATTERN" else 330))
a(text("909 LINK",138.5,7.6,2.9,fill="#A9AAAE"))
a(f'<circle cx="138.5" cy="13.2" r="1.5" fill="{TRIM}" stroke="#1D1E20" stroke-width="0.3"/>')
# display
DX,DY,DW,DH=GX0-2.6,22.0,16*SW,7.0
a(f'<rect x="{DX}" y="{DY}" width="{DW}" height="{DH}" rx="1" fill="#1B2430"/>')
a(text("01 CHICAGO JACK",DX+2.4,DY+4.9,3.2,FB,"#FFB37A",anchor="start"))
a(text("HOUSE · 120 BPM · 16 STEPS",DX+DW-2.2,DY+4.9,2.4,FB,"#8A97A8",anchor="end"))
# step numbers coloured 909-style in groups of 4
for s in range(16):
    c=STRIPE[s//4]
    a(f'<rect x="{sx(s)-CW/2:.2f}" y="30.4" width="{CW}" height="0.9" rx="0.3" fill="{c}"/>')
    a(text(str(s+1),sx(s),34.4,2.4,fill=INK if s%4==0 else SOFT))
# rows
PLAY=4
for r,(name,pat) in enumerate(TR):
    y=ry(r)
    if r==11: a(f'<line x1="3" y1="{y-P/2-0.2:.2f}" x2="{MUTE_X+4}" y2="{y-P/2-0.2:.2f}" stroke="{LINE}" stroke-width="0.3"/>')
    a(text(name,4.0,y+1.15,3.2,anchor="start"))
    for s in range(16):
        on=pat[s]=="x"; fill=("#F0C030" if r==11 else TRIM) if on else "#F7F6F2"
        st="#1D1E20" if s==PLAY else "#9A9A94"; sw=0.55 if s==PLAY else 0.25
        h=CH if r<11 else CH*0.75
        a(f'<rect x="{sx(s)-CW/2:.2f}" y="{y-h/2:.2f}" width="{CW}" height="{h:.2f}" rx="0.5" fill="{fill}" stroke="{st}" stroke-width="{sw}"/>')
    if r<11:
        a(f'<rect x="{MUTE_X-2:.2f}" y="{y-2:.2f}" width="4" height="4" rx="0.6" fill="{"#D7322B" if r==1 else "#C9CBD0"}" stroke="#6B6E75" stroke-width="0.25"/>')
a(text("MUTE",MUTE_X,34.4,2.4,fill=SOFT))
# out block, zig-zag jacks
top=29.0; bot=ry(11)+4.6
a(f'<rect x="{OX1-5.4}" y="{top}" width="{OX2-OX1+10.8}" height="{bot-top:.2f}" rx="1.4" fill="{TRIM}"/>')
a(text("OUT",(OX1+OX2)/2,33.4,2.9,fill=LI))
for r in range(12):
    a(jack(OX1 if r%2==0 else OX2, ry(r)))
a(f'<rect x="0" y="122.4" width="{W}" height="0.6" fill="{TRIM}"/>')
for x,y in((7.5,3),(W-7.5,3),(7.5,125.5),(W-7.5,125.5)): a(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#8A8B90"/>')
svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'+"\n".join(p)+"\n</svg>\n"
open("Jack.svg","w").write(svg)
import haer_logo; haer_logo.add_to_file("Jack.svg",x=W/2,y=126.0,cap=3.0,bg=BG)
