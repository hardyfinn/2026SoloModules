# Builds a shortlist sheet (HTML pages) from chosen knob numbers across all generators.
import sys, os
sys.path.insert(0, "/mnt/project-files/vcv-haer-knobs")
PICK=[int(x) for x in sys.argv[1].split(",")]
TITLE=sys.argv[3] if len(sys.argv)>3 else "knob shortlist"
OUTD=sys.argv[2]
import make_knobs as K
REG={}
base=list(K.DESIGNS); base_svg=K.knob_svg
for i,d in enumerate(base,1): REG[i]=(d,base_svg)
import make_knobs_industrial as I
for i,d in enumerate(I.DESIGNS,16): REG[i]=(d,I.knob_svg)
import make_knobs_schools as S1
for i,d in enumerate(S1.DESIGNS,31): REG[i]=(d,S1.knob_svg)
import make_knobs_schools2 as S2
for i,d in enumerate(S2.DESIGNS,46): REG[i]=(d,S2.knob_svg)
import make_knobs_categories as C
n=61; CAT={}
for title,slug,ds in C.CATS:
    for d in ds: REG[n]=(d,C.knob_svg); CAT[n]=title; n+=1
import make_knobs_japan as J
for i,d in enumerate(J.TRAD+J.MODERN,211): REG[i]=(d,J.knob_svg)
import make_knobs_custom as CU
for i,d in enumerate(CU.DESIGNS,241): REG[i]=(d,CU.knob_svg)
def cat(n):
    for lo,t in ((1,"Basics"),(16,"Industrial"),(31,"Design schools"),(46,"Design schools II"),(211,"Japan: traditional"),(226,"Japan: modern")):
        pass
    if n<=15: return "Basics"
    if n<=30: return "Industrial"
    if n<=45: return "Design schools"
    if n<=60: return "Design schools II"
    if n<=210: return CAT[n]
    if n>240: return "From your image"
    return "Japan: traditional" if n<=225 else "Japan: modern"
PX=8.5
def strip(n,theme,tag):
    (name,note,fn),ks=REG[n]
    out=[]
    for (sz,mm),ang in zip(K.SIZES,(-135,30,135)):
        w=mm*1.2*PX
        svg=ks(fn,mm,ang,sz=="trimpot",f"{tag}{theme}{n}{sz}").replace(f'width="{mm}mm" height="{mm}mm"','width="100%" height="100%"')
        out.append(f'<div class="k"><div style="width:{w:.0f}px;height:{w:.0f}px">{svg}</div></div>')
    return f'<div class="strip {theme}">{"".join(out)}</div>'
cells=[]
for n in PICK:
    (name,note,_),_=REG[n]
    cells.append(f'<div class="cell"><div class="hd"><span class="num">{n}</span><span class="nm">{name}</span><span class="ct">{cat(n)}</span></div><div class="nt">{note}</div>{strip(n,"light","a")}{strip(n,"dark","b")}</div>')
PER=16
pages=[cells[i:i+PER] for i in range(0,len(cells),PER)]
os.makedirs(OUTD,exist_ok=True)
for pi,pc in enumerate(pages,1):
    html=f'''<!doctype html><html><head><meta charset="utf-8"><style>
@font-face{{font-family:CC;src:url(file:///mnt/project-files/vcv-haer-logo/fonts/ClimateCrisis.ttf)}}
body{{margin:0;background:#f6f4ef;color:#1c1c20;font-family:Inter,sans-serif;width:1500px}}
.top{{display:flex;align-items:baseline;gap:20px;padding:26px 34px 12px;border-bottom:4px solid #3d7fd6}}
.top h1{{font-family:CC;font-weight:400;font-size:46px;margin:0}} .top b{{font-size:30px;font-weight:900}} .top span{{color:#77746d;font-size:18px;font-weight:600}}
.grid{{display:grid;grid-template-columns:repeat(4,1fr);gap:16px;padding:18px 26px 26px}}
.cell{{background:#fff;border-radius:10px;padding:12px 12px 12px;box-shadow:0 1px 0 #0001}}
.hd{{display:flex;align-items:baseline;gap:8px}} .num{{font-weight:900;font-size:30px}} .nm{{font-weight:800;font-size:17px}} .ct{{margin-left:auto;font-size:12px;color:#8a877f;font-weight:600}}
.nt{{font-size:13px;color:#77746d;margin:0 0 8px}}
.strip{{display:flex;align-items:center;justify-content:space-around;height:150px;border-radius:6px}}
.light{{background:#ECE8E0}} .dark{{background:#232328;margin-top:6px}}
</style></head><body><div class="top"><h1>Hær</h1><b>{TITLE}</b><span>{len(PICK)} knobs · page {pi} of {len(pages)} · large 14 mm, medium 10 mm, trimpot 6 mm · light and dark panel</span></div>
<div class="grid">{"".join(pc)}</div></body></html>'''
    open(f"{OUTD}/shortlist-{pi}.html","w").write(html)
print(len(pages))
