import math, json
W=11  # stroke weight in units (cap height = 100)

def svg(body, vb, fg, extra=""):
    x0,y0,x1,y1 = vb
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{x0} {y0} {x1-x0} {y1-y0}" '
            f'fill="none" stroke="{fg}" stroke-width="{W}" stroke-linecap="round" stroke-linejoin="round">{body}</svg>')

# ---------- A: BUS ----------
def bus(fg, bg, acc):
    e_end = (137+27*math.cos(math.radians(38)), 72+27*math.sin(math.radians(38)))
    b = f'''
<circle cx="-27" cy="72" r="8" stroke="{acc}"/>
<path d="M-19,72 H164" />
<path d="M0,0 V100 M40,0 V100"/>
<g transform="translate(6,0)"><path d="M64,56 Q71,45 84,45 Q104,45 104,64 V100"/>
<path d="M86,72 Q62,72 62,86 Q62,100 80,100 Q96,100 104,90"/></g>
<path d="M164,72 A27,27 0 1 0 {e_end[0]:.1f},{e_end[1]:.1f}"/>
<path d="M188,46 V100 M188,68 Q190,46 214,46"/>'''
    return svg(b,(-42,-8,224,108),fg)

# ---------- B: KNOT ----------
def arc_pts(cx,cy,r,a0,a1,n=40):
    pts=[(cx+r*math.cos(math.radians(a0+(a1-a0)*i/n)), cy+r*math.sin(math.radians(a0+(a1-a0)*i/n))) for i in range(n+1)]
    return "M"+" L".join(f"{x:.2f},{y:.2f}" for x,y in pts)
def knot(fg,bg,acc):
    # two-storey a whose stem threads THROUGH the e ring: over at the top, under at the bottom
    sw=9.5; G=8.5
    cx,cy,r=130,72,27
    dy=math.sqrt(r*r-(cx-104)**2)          # stem x=104 meets e ring
    ta=math.degrees(math.atan2(-dy,104-cx)); ba=math.degrees(math.atan2(dy,104-cx))
    ga=math.degrees(G/r)
    # e path: angle 0 -> decreasing to -320 (open tail); gap around top crossing (stem is over there)
    t=ta if ta<0 else ta-360
    e1=arc_pts(cx,cy,r,0,t+ga); e2=arc_pts(cx,cy,r,t-ga,-322)
    body=f'''<g stroke-width="{sw}">
<path d="M0,0 V100 M40,0 V100 M0,52 H40"/>
<path d="M62,56 Q69,45 82,45 Q104,45 104,64 V{cy+dy-G:.1f} M104,{cy+dy+G:.1f} V100"/>
<path d="M104,72 H84 Q60,72 60,86 Q60,100 78,100 Q94,100 104,90"/>
<path d="{e1} {e2}"/>
<path d="M{104+G:.1f},72 H{cx+r}"/>
<path d="M182,46 V100 M182,68 Q184,46 208,46"/></g>'''
    return svg(body,(-8,-8,216,108),fg)

# ---------- C: PATCH (one continuous cable, plug at the end) ----------
def patch_path():
    return ("M0,0 V100 V54 H40 V0 V100 "
            "C52,104 62,98 62,86 Q62,72 86,72 H104 V100 V90 "
            "Q96,100 80,100 Q62,100 62,86 Q62,72 86,72 H104 "
            "V64 Q104,45 84,45 Q71,45 64,56 Q71,45 84,45 Q104,45 104,64 V72 H156 "
            "A26,26 0 1 0 150.5,88 "
            "C158,100 176,104 176,92 V46 V66 Q178,46 202,46 H206")
def patch(fg,bg,acc):
    b=f'''<path d="{patch_path()}"/>
<rect x="206" y="40" width="13" height="12" rx="1.5" fill="{acc}" stroke="none"/>
<path d="M219,46 H230" stroke="{acc}" stroke-width="5"/>
<circle cx="232" cy="46" r="3.6" fill="{acc}" stroke="none"/>'''
    return svg(b,(-8,-8,240,108),fg)

# ---------- D: RIBBON (same cable as three-wire ribbon) ----------
def ribbon(fg,bg,acc):
    p=patch_path()
    b=f'''<path d="{p}" stroke-width="15"/>
<path d="{p}" stroke="{bg}" stroke-width="9"/>
<path d="{p}" stroke="{acc}" stroke-width="3"/>'''
    s=svg(b,(-10,-10,220,110),fg)
    return s

# ---------- E: ROLL (piano-roll notes) ----------
BM={
'H':["X....X"]*4+["XXXXXX"]+["X....X"]*4,
'ae':["........."]*3+[".XXX.XXX.","....X...X",".XXXXXXXX","X...X....","X...X...X",".XXX.XXX."],
'r':["....."]*3+["X.XXX","XX...","X....","X....","X....","X...."],
}
def roll(fg,bg,acc):
    cell=11.6; h=7.2; out=[]; x0=0
    for k in ['H','ae','r']:
        rows=BM[k]
        for ri,row in enumerate(rows):
            c=0
            while c<len(row):
                if row[c]=='X':
                    s=c
                    while c<len(row) and row[c]=='X': c+=1
                    out.append(f'<rect x="{x0+s*cell+0.8:.1f}" y="{ri*cell+ (cell-h)/2:.1f}" width="{(c-s)*cell-1.6:.1f}" height="{h}" rx="1.6"/>')
                else: c+=1
        x0+= (len(rows[0])+1)*cell
    width=x0-cell
    ph=6.5*cell  # playhead between ae columns
    body=f'<g fill="{fg}" stroke="none">{"".join(out)}</g>' \
         f'<path d="M{(6+1+4.5)*cell:.1f},-6 V{9*cell+6:.1f}" stroke="{acc}" stroke-width="2.4"/>'
    return svg(body,(-6,-10,width+6,9*cell+10),fg)

CONCEPTS=[("A-bus","Bus",bus),("B-thread","Thread",knot),("C-patch","Patch",patch),("D-ribbon","Ribbon",ribbon),("E-roll","Roll",roll)]
THEMES={"dark":("#ece6d8","#1d2733","#e8833a"),"light":("#1b1b1b","#e9e4d8","#d0541f")}
out={}
for slug,name,fn in CONCEPTS:
    for t,(fg,bg,acc) in THEMES.items():
        s=fn(fg,bg,acc)
        open(f"Haer-{slug}-{t}.svg","w").write(s)
        out[f"{slug}-{t}"]=s
json.dump(out,open("all.json","w"))
