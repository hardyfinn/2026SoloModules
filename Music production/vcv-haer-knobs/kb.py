# Small knob-drawing toolkit used by make_knobs_categories.py.
# Units: knob radius 50, viewBox -60..60, pointer drawn at 12 o'clock. s=True means trimpot (thicker marks).
import math, itertools
_c=itertools.count()
def uid(p="g"): return f"{p}{next(_c)}"
def pol(r,a): return r*math.sin(math.radians(a)), -r*math.cos(math.radians(a))
def F(fill):
    """fill may be a colour or a gradient tuple ('r'|'l', [(offset,colour),...], cx, cy) -> (defs, fill)"""
    if isinstance(fill,str): return "", fill
    kind,stops=fill[0],fill[1]; i=uid()
    st="".join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in stops)
    if kind=="r":
        cx,cy=(fill[2],fill[3]) if len(fill)>2 else (35,30)
        return f'<defs><radialGradient id="{i}" cx="{cx}%" cy="{cy}%" r="80%">{st}</radialGradient></defs>', f"url(#{i})"
    x2,y2=(fill[2],fill[3]) if len(fill)>2 else (1,1)
    return f'<defs><linearGradient id="{i}" x1="0" y1="0" x2="{x2}" y2="{y2}">{st}</linearGradient></defs>', f"url(#{i})"
def R(a,b,cx=35,cy=30): return ("r",[(0,a),(1,b)],cx,cy)
def LG(*stops,x2=1,y2=1): return ("l",[(i/(len(stops)-1),c) for i,c in enumerate(stops)],x2,y2)
def _sh(tag,fill,stroke=None,sw=0,op=1):
    d,f=F(fill); s=f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    o=f' opacity="{op}"' if op!=1 else ""
    return d+tag.replace("FILL",f'fill="{f}"{s}{o}')
def C(r,fill,stroke=None,sw=0,op=1,cx=0,cy=0): return _sh(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r}" FILL/>',fill,stroke,sw,op)
def POLY(pts,fill,stroke=None,sw=0,op=1): return _sh(f'<polygon points="{" ".join(f"{x:.2f},{y:.2f}" for x,y in pts)}" FILL stroke-linejoin="round"/>',fill,stroke,sw,op)
def PATH(d,fill,stroke=None,sw=0,op=1): return _sh(f'<path d="{d}" FILL stroke-linejoin="round" stroke-linecap="round"/>',fill,stroke,sw,op)
def TEETH(n,ro,ri,fill,**k): return POLY([pol(ro if i%2==0 else ri,180*i/n) for i in range(2*n)],fill,**k)
def SCAL(n,r,d,fill,**k): return POLY([pol(r-d*(0.5-0.5*math.cos(n*math.radians(a))),a) for a in range(0,360,2)],fill,**k)
def LOBES(n,R_,r,fill,**k): return POLY([pol(R_-(R_-r)*(0.5-0.5*math.cos(n*math.radians(a/2)))**1.6,a/2) for a in range(0,720,2)],fill,**k)
def NGON(n,r,fill,rot=0,**k): return POLY([pol(r,rot+360*i/n) for i in range(n)],fill,**k)
def STAR(n,ro,ri,fill,**k): return TEETH(n,ro,ri,fill,**k)
def RR(h,rx,fill,**k): return _sh(f'<rect x="{-h}" y="{-h}" width="{2*h}" height="{2*h}" rx="{rx}" FILL/>',fill,**k)
def RECT(x,y,w,h,fill,rx=0,op=1,rot=0): return _sh(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" transform="rotate({rot})" FILL/>',fill,op=op)
def RING(r,w,col,op=1,dash=None): 
    da=f' stroke-dasharray="{dash}"' if dash else ""
    return f'<circle r="{r}" fill="none" stroke="{col}" stroke-width="{w}" opacity="{op}"{da}/>'
def ARC(r,a0,a1,w,col,cap="butt",op=1):
    x0,y0=pol(r,a0); x1,y1=pol(r,a1)
    return f'<path d="M{x0:.2f},{y0:.2f} A{r},{r} 0 {1 if a1-a0>180 else 0} 1 {x1:.2f},{y1:.2f}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="{cap}" opacity="{op}"/>'
def WEDGE(r,a0,a1,fill,op=1):
    x0,y0=pol(r,a0); x1,y1=pol(r,a1)
    return PATH(f"M0,0 L{x0:.2f},{y0:.2f} A{r},{r} 0 {1 if a1-a0>180 else 0} 1 {x1:.2f},{y1:.2f} Z",fill,op=op)
def LINE(x1,y1,x2,y2,col,w,cap="round",op=1): return f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{col}" stroke-width="{w}" stroke-linecap="{cap}" opacity="{op}"/>'
def TICKS(n,r0,r1,col,w,a0=0,a1=360,op=1):
    span=a1-a0; k=n if span>=360 else n-1
    return "".join(LINE(*pol(r0,a0+span*i/max(k,1)),*pol(r1,a0+span*i/max(k,1)),col,w,"butt",op) for i in range(n))
def DOTS(n,r,size,col,a0=0,a1=360,op=1):
    span=a1-a0; k=n if span>=360 else n-1
    return "".join(C(size,col,op=op,cx=pol(r,a0+span*i/max(k,1))[0],cy=pol(r,a0+span*i/max(k,1))[1]) for i in range(n))
def TXT(t,size,col,y=0,font="CC",weight=400,x=0,rot=0,anchor="middle"):
    return f'<text x="{x}" y="{y}" font-family="{font}" font-weight="{weight}" font-size="{size}" fill="{col}" text-anchor="{anchor}" dominant-baseline="central" transform="rotate({rot})">{t}</text>'
def CLIP(body,r=50):
    i=uid("c"); return f'<clipPath id="{i}"><circle r="{r}"/></clipPath><g clip-path="url(#{i})">{body}</g>'
# pointers (thicker on trimpots)
def BAR(s,col,r0=0,r1=44,w=10,cap="round",op=1): return LINE(0,-r0,0,-r1,col,w*(1.45 if s else 1),cap,op)
def GBAR(s,col,r0=0,r1=44,w=8):  # glowing bar: soft halo then core
    return BAR(s,col,r0,r1,w*3.2,"round",.15)+BAR(s,col,r0,r1,w*2,"round",.3)+BAR(s,"#ffffff",r0,r1,w*0.55,"round",.9)+BAR(s,col,r0,r1,w,"round",.6)
def DOT(s,col,r=32,size=8,**k): return C(size*(1.4 if s else 1),col,cx=0,cy=-r,**k)
def TRI(s,col,tip=50,base=18,half=11,**k): h=half*(1.35 if s else 1); return POLY([(0,-tip),(-h,-base),(h,-base)],col,**k)
def KNOB(st,rot=""): return (st,rot)
