# Custom knobs from Haer's reference images (241+). Uses kb.py helpers.
import sys
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
from kb import *
INK="#111113"; RED="#ef3b2c"; CREAM="#ECE8E0"
def sphere(r=33,top="#f4f4f6"): return C(r,LG(top,"#8a898e","#141315",x2=0,y2=1),INK,0.8)
def ringbar_bar(s,col=INK): w=15*(1.3 if s else 1); return RECT(-w/2,-50,w,51,col)
def d241(s): # the image as a knob: ring, shaded ball, bar to the rim, red dot opposite; all turns
    return C(50,"#fbfbfb")+RING(48.5,3.2,INK)+sphere(), ringbar_bar(s)+C(5.5*(1.3 if s else 1),RED,cy=43)
def d242(s): # ring + red dot fixed (dot marks the stop at 6 o'clock); ball + bar turn
    return C(50,"#fbfbfb")+RING(48.5,3.2,INK)+C(5.5*(1.3 if s else 1),RED,cy=43)+sphere(), ringbar_bar(s)
def d243(s): # dark-panel version: dark face, cream ring and bar
    return C(50,"#1b1b1f")+RING(48.5,3.2,CREAM)+C(5.5*(1.3 if s else 1),RED,cy=43)+sphere(33,"#c9c9ce"), ringbar_bar(s,CREAM)
def d244(s): # no ring: just the shaded ball and bar, red dot rides the bar end
    return sphere(40), ringbar_bar(s)+C(6*(1.3 if s else 1),RED,cy=-44)
DESIGNS=[
 ("Ring and bar","from your image, red dot opposite",d241),
 ("Fixed ring","ring and red stop stay put",d242),
 ("Ring and bar dark","cream ring, for dark panels",d243),
 ("Ball and bar","no ring, red dot on the bar",d244),
]
def knob_svg(fn, mm, angle, small, ident):
    st, rot = fn(small)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{mm}mm" height="{mm}mm" viewBox="-60 -60 120 120">'
            f'{st}<g transform="rotate({angle})">{rot}</g></svg>')
if __name__=="__main__":
    import make_knobs as K, re
    for i,(name,_,fn) in enumerate(DESIGNS,241):
        sl=re.sub(r'[^a-z0-9]+','-',name.lower()).strip('-')
        for sz,mm in K.SIZES:
            open(f"{K.OUT}/svg/{i:03d}-{sl}-{sz}.svg","w").write(knob_svg(fn, mm*1.2, 0, sz=="trimpot", ""))
    print("ok")
