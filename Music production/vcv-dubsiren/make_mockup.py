W,H=121.92,128.5
s=[]
def t(x,y,txt,sz=3.2,anchor="middle",fill="#1c1c1c",w="700"):
    s.append(f'<text x="{x}" y="{y}" font-size="{sz}" text-anchor="{anchor}" fill="{fill}" font-family="Orbitron,Arial" font-weight="{w}">{txt}</text>')
def knob(x,y,r,cap,ang=-40):
    import math
    s.append(f'<circle cx="{x}" cy="{y}" r="{r+1.6}" fill="#111"/>')
    for i in range(24):
        a=i*15*math.pi/180
        s.append(f'<line x1="{x+(r+1.2)*math.cos(a)}" y1="{y+(r+1.2)*math.sin(a)}" x2="{x+(r+1.8)*math.cos(a)}" y2="{y+(r+1.8)*math.sin(a)}" stroke="#333" stroke-width="0.6"/>')
    s.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{cap}" stroke="#00000055" stroke-width="0.4"/>')
    a=math.radians(ang-90)
    s.append(f'<line x1="{x+r*0.25*math.cos(a)}" y1="{y+r*0.25*math.sin(a)}" x2="{x+r*0.95*math.cos(a)}" y2="{y+r*0.95*math.sin(a)}" stroke="#fff" stroke-width="0.9" stroke-linecap="round"/>')
def scale(x,y,r,labels,start=-135,end=135):
    import math
    n=len(labels)
    for i,l in enumerate(labels):
        a=math.radians(start+(end-start)*i/(n-1)-90)
        t(x+(r)*math.cos(a),y+(r)*math.sin(a)+1,l,2.9)
def jack(x,y,lab,out=False):
    if out: s.append(f'<rect x="{x-4.6}" y="{y-4.4}" width="9.2" height="12.4" rx="1.2" fill="#1c1c1c"/>')
    s.append(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#c8c8c8" stroke="#555" stroke-width="0.5"/><circle cx="{x}" cy="{y}" r="1.8" fill="#111"/>')
    t(x,y+6.6,lab,2.9,fill="#f4f1e8" if out else "#1c1c1c")
def button(x,y,lab,col="#222"):
    s.append(f'<circle cx="{x}" cy="{y}" r="4" fill="#bbb" stroke="#666" stroke-width="0.5"/><circle cx="{x}" cy="{y}" r="2.8" fill="{col}"/>')
    t(x,y+7.6,lab)
def toggle(x,y,lab,top="",bot=""):
    s.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="#ccc" stroke="#666" stroke-width="0.5"/><rect x="{x-0.9}" y="{y-5.5}" width="1.8" height="5.5" rx="0.9" fill="#ddd" stroke="#555" stroke-width="0.4"/>')
    t(x,y+7.6,lab)
    if top: t(x+5.2,y-3,top,2.9,anchor="start")
    if bot: t(x+5.2,y+2.5,bot,2.9,anchor="start")
def trim(x,y,lab,cap="#e9d44a"):
    knob(x,y,3.0,cap,20); t(x,y+8,lab)
# background brushed aluminium
s.append('<defs><linearGradient id="al" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#c9cbc8"/><stop offset="0.5" stop-color="#b3b5b2"/><stop offset="1" stop-color="#c3c4c1"/></linearGradient></defs>')
s.append(f'<rect width="{W}" height="{H}" fill="url(#al)"/>')
for sx,sy in [(7.5,3),(W-7.5,3),(7.5,H-3),(W-7.5,H-3)]:
    s.append(f'<circle cx="{sx}" cy="{sy}" r="1.6" fill="#8a8a88"/>')
t(W/2,10,"DUB SIREN",6.2)
# row1
knob(32,24,7,"#c8282a",-120); t(32,37.5,"SPEED")
knob(90,24,7,"#c8282a",30); t(90,37.5,"PITCH")
# row2
knob(32,52,6,"#f0dc4c",-20); t(32,64.5,"FEEDBACK")
knob(90,52,6,"#f0dc4c",-45); scale(90,52,11,["1","2","3","4"],-115,115); t(90,64.5,"MODE")
# row3
toggle(12,78,"LATCH"); button(32,78,"SIREN","#c8282a"); button(52,78,"TONE","#222")
knob(90,78,6,"#f0dc4c",45); scale(90,78,11,["1","2","3","OFF"],-115,115); t(90,90.5,"BEAT")
# row4
trim(11,98,"DEPTH"); trim(30,98,"RELEASE"); trim(49,98,"TIME")
toggle(65,98,"ERA","80s","90s")
knob(100,98,6,"#3e8a44",-60); t(100,108,"VOLUME")
y1=114.5
for i,l in enumerate(["V/OCT","SPEED","FDBK","CLOCK","SIREN","TONE","IN"]): jack(9+i*14.2,y1,l)
jack(111.5,y1,"OUT",True)
t(W/2,H-1.2,"Hær",3.0,fill="#1c1c1c",w="900")
svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*7}" height="{H*7}">'+"".join(s)+'</svg>'
open("mock.html","w").write('<html><head><link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@700&family=Climate+Crisis&display=swap" rel="stylesheet"></head><body style="margin:0;background:#2b2d31;padding:20px">'+svg.replace('>Hær<',' style="font-family:Climate Crisis">Hær<')+'</body></html>')
