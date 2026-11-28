# Comper 2.3.0 panel mockup: 48 HP, 3.2 mm control labels, 2.9 mm jack labels,
# preset browser in the display, OCTAVE and LENGTH knobs.
import sys, json
ANNOT = len(sys.argv) > 1 and sys.argv[1] == "annot"
W, H = 193.04, 128.5
BG, BG_DARK, LINE, INK, SOFT, ACCENT = "#23262B", "#16181C", "#3D424A", "#EFE9DD", "#A8A397", "#D9A441"
AMB, DIM = "#F2C25C", "#8A7A55"
CONTROL, JACK, SMALL, HEAD = 3.2, 2.9, 2.4, 3.2
COL_X0, COL_DX = 9.0, 15.73
STEP_X0, STEP_DX = 23.0, 14.87
Y_STEPLIGHT, Y_ROOT, Y_QUAL, Y_STEPNAME = 34.0, 40.0, 47.6, 54.0
Y_HEAD, Y_ROWA, Y_ROWB, Y_IN1, Y_IN2 = 59.4, 71.0, 85.6, 104.8, 120.0
DISP_X, DISP_Y, DISP_W, DISP_H = 4.0, 12.6, 182.0, 18.4
SMALLK_X, SMALLK_Y = [201.0, 223.0, 245.0], 23.0
OUT_COL0 = 11
def col(i): return COL_X0 + i * COL_DX
def step(i): return STEP_X0 + i * STEP_DX
P = []; labels = []; comps = []; news = []
def T(s, x, y, size, fill=INK, anchor="middle", weight=700, check=True, ls=0.02, fam="I"):
    P.append(f'<text class="t" data-c="{1 if check else 0}" x="{x:.2f}" y="{y:.2f}" font-family="{fam}" font-weight="{weight}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" letter-spacing="{size*ls:.3f}">{s}</text>')
def knob(x, y, r, name, snap=False):
    comps.append((name, x - r, y - r, x + r, y + r))
    P.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#0F1114" stroke="#55555C" stroke-width="{r*0.12:.2f}"/>')
    P.append(f'<circle cx="{x}" cy="{y}" r="{r*0.72:.2f}" fill="#2E3137"/>')
    P.append(f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - r*0.8:.2f}" stroke="{INK}" stroke-width="{max(0.35, r*0.1):.2f}" stroke-linecap="round"/>')
def jack(x, y, name, dark=False):
    r = 4.0; comps.append((name, x - r, y - r, x + r, y + r))
    P.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="#B9BCC2" stroke="#7D8087" stroke-width="0.4"/><circle cx="{x}" cy="{y}" r="2.3" fill="#3A3C40"/><circle cx="{x}" cy="{y}" r="1.4" fill="#0A0A0A"/>')
def light(x, y, r=1.1, on=False, name="light"):
    comps.append((name, x - r, y - r, x + r, y + r))
    P.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{AMB if on else "#3A3324"}"/>')
def button(x, y, r, name):
    comps.append((name, x - r, y - r, x + r, y + r))
    P.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#C9C4B8" stroke="#77736B" stroke-width="0.3"/>')
def switch3(x, y, name):
    comps.append((name, x - 2.0, y - 4.6, x + 2.0, y + 4.6))
    P.append(f'<rect x="{x-1.8}" y="{y-4.4}" width="3.6" height="8.8" rx="0.8" fill="#0F1114" stroke="#55555C" stroke-width="0.3"/><rect x="{x-1.3}" y="{y-1.2}" width="2.6" height="2.4" rx="0.4" fill="#C9C4B8"/>')
def new(x, y, r, tag):
    news.append((x, y, r, tag))


P.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
T("Comper", 5.0, 9.4, 5.4, INK, "start")
T("jazz piano comping player · notes out", W - 5.0, 9.0, 2.6, ACCENT, "end", 500)
DX, DY, DW, DH = 4.0, 12.2, W - 8.0, 26.0
P.append(f'<rect x="{DX}" y="{DY}" width="{DW}" height="{DH}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')
comps.append(("display", DX, DY, DX + DW, DY + DH))
P.append(f'<rect x="{DX+1.2}" y="{DY+1.2}" width="{DW-2.4}" height="5.4" rx="0.8" fill="#1C1A14"/>')
P.append(f'<path d="M{DX+3.2} {DY+3.9} l2.6 -1.9 v3.8 z" fill="{AMB}"/>')
P.append(f'<path d="M{DX+DW-3.2} {DY+3.9} l-2.6 -1.9 v3.8 z" fill="{AMB}"/>')
T("PRESET", DX+8.2, DY+5.0, 2.3, DIM, "start", 400, False, 0.08, "M")
T("04 · BOSSA *", DX+21.0, DY+5.15, 3.4, AMB, "start", 400, False, 0.03, "M")
T("4 / 12 · TAP NAME FOR LIST", DX+DW-8.0, DY+5.0, 2.3, DIM, "end", 400, False, 0.06, "M")
T("Dm7", DX+2.5, DY+13.6, 6.6, AMB, "start", 400, False, 0, "M")
T("&gt; G7", DX+17.5, DY+13.6, 3.8, DIM, "start", 400, False, 0, "M")
T("F4 A4 C5 E5", DX+DW/2+6, DY+13.4, 3.2, AMB, "middle", 400, False, 0, "M")
T("CHART · SEED 1 · OCT +1", DX+DW-2.5, DY+13.4, 2.6, DIM, "end", 400, False, 0, "M")
# chart strip: 16 tappable cells
cw = (DW - 2.4) / 16; cy = DY + 16.8
chart = ["Dm7","G7","Cmaj7","Cmaj7","Dm7b5","G7alt","Cm6","Cm6"] + ["--"]*8
for i in range(16):
    x = DX + 1.2 + i * cw
    sel = i == 5; play = i == 1; used = i < 8
    fill = "#3A2F14" if sel else "#17181B"
    P.append(f'<rect x="{x+0.25:.2f}" y="{cy}" width="{cw-0.5:.2f}" height="7.8" rx="0.6" fill="{fill}" stroke="{AMB if sel else ("#5A4B28" if play else "#262830")}" stroke-width="{0.45 if sel or play else 0.25}"/>')
    T(str(i+1), x+0.9, cy+2.1, 1.6, DIM, "start", 400, False, 0, "M")
    if play: P.append(f'<circle cx="{x+cw-1.4:.2f}" cy="{cy+1.4}" r="0.6" fill="{AMB}"/>')
    T(chart[i], x+cw/2, cy+6.2, 2.9 if len(chart[i])<6 else 2.5, AMB if used else "#4A4232", "middle", 400, False, 0, "M")
Y_CH = 0
# knob blocks: 11 columns x 3 rows
KX0, KDX = 9.6, (W - 19.2) / 12
def kc(i): return KX0 + i * KDX
Y_H = 45.0; RY = [55.6, 70.6, 85.4]
def heading(s, x0, x1, y):
    T(s, x0, y, HEAD, ACCENT, "start", ls=0.1); P.append(f'__HEAD__{s}|{x0}|{x1}|{y}')
heading("CHART", kc(0)-5.0, kc(1)+5.4, Y_H)
heading("VOICING", kc(2)-5.4, kc(4)+5.4, Y_H); heading("RHYTHM", kc(5)-5.4, kc(7)+5.4, Y_H)
heading("HANDS", kc(8)-5.4, kc(10)+5.4, Y_H); heading("BAND", kc(11)-5.4, kc(12)+5.0, Y_H)
grid = {(0,0):"STEPS",(0,1):"BEATS",(0,2):"KEY",(1,0):"ROOT",(1,1):"QUAL",
 
 (2,0):"STYLE",(3,0):"SPREAD",(4,0):"VOICES",(2,1):"LEAD",(3,1):"REGISTER",(4,1):"AUTRE",(2,2):"ROOTS",(3,2):"OCTAVE",
 (5,0):"GROOVE",(6,0):"SWING",(7,0):"DENSITY",(5,1):"REPEAT",(6,1):"PUSH",(7,1):"LENGTH",(5,2):"SEED",(6,2):"NEW",
 (8,0):"LOOSE",(9,0):"TIMING",(10,0):"ROLL",(8,1):"PEDAL",(9,1):"WEIGHT",(10,1):"SLOP",
 (11,0):"LISTEN",(12,0):"TEMPO",(11,1):"RUN",(11,2):"HIT",(12,2):"SOLO"}
for (c,r),n in grid.items():
    x, y = kc(c), RY[r]
    if n == "ROOTS": T(n, x, y-6.0, CONTROL); switch3(x, y, n); T("OFF·SOME·ALL", x, y+6.2, 1.9, SOFT, check=False); continue
    if n in ("HIT","SOLO"): T(n, x, y-3.0, CONTROL); light(x, y, 1.2, n=="HIT", n); continue
    if n in ("NEW","RUN"):
        T(n, x, y-5.0, CONTROL); button(x, y, 3.0, n)
        if n == "RUN": light(x, y, 1.2, True, "runlight")
        continue
    r_ = 5.0 if n=="LOOSE" else 4.0 if n in ("STEPS","BEATS","KEY","ROOT","QUAL") else 3.1 if n in ("TIMING","ROLL","WEIGHT","SLOP") else 4.6
    T(n, x, y-(r_+1.6 if n=="LOOSE" else 6.0 if r_>4 else 5.0), CONTROL); knob(x, y, r_, n)
T("−3", kc(3)-5.4, RY[2]+5.6, 2.2, SOFT, check=False); T("+3", kc(3)+5.4, RY[2]+5.6, 2.2, SOFT, check=False)
T("SHORT", kc(7)-2.6, RY[1]+6.6, 1.9, SOFT, "end", check=False); T("LEGATO", kc(7)+2.6, RY[1]+6.6, 1.9, SOFT, "start", check=False)
T("TRIMS × LOOSE", kc(9)+KDX/2, RY[2]-0.4, SMALL, SOFT)
T("BPM", kc(12), RY[0]+7.2, 2.0, SOFT, "middle", check=False)
T("± 6 ST", kc(0), RY[2]+6.6, 2.0, SOFT, check=False)
T("EDIT", kc(1), RY[2]-1.2, CONTROL, ACCENT); T("STEP 6", kc(1), RY[2]+2.4, SMALL, SOFT)
if ANNOT:
    news.append((None,None,None,"disp")); news.append((kc(3), RY[2], 6.4, "OCTAVE")); news.append((kc(7), RY[1], 6.4, "LENGTH"))
    news.append(("edit", 0, 0, 0))
# jacks: 13 columns, inputs 0-7, outputs 8-12
JX0, JDX = 8.4, (W - 17.6) / 12
def jc(i): return JX0 + i * JDX
Y_IN1, Y_IN2 = 104.8, 119.2
top = Y_IN1 - 10.2
P.append(f'<rect x="2.0" y="{top}" width="{W-4.0}" height="{Y_IN2 - top + 3.9}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "RUN", "KEY", "ROOT", "QUAL", "CHORD\nV/OCT", "CHORD\nGATE"]
IN2 = ["SOLO", "SOLO\nV/OCT", "DENSITY", "AUTRE", "LOOSE", "LISTEN", "REGISTER"]
def jlabel(n, x, y, fill):
    l = n.split("\n")
    if len(l) == 1: T(n, x, y - 5.0, JACK, fill)
    else: T(l[0], x, y - 7.7, JACK, fill); T(l[1], x, y - 4.8, JACK, fill)
for i, n in enumerate(IN1): jlabel(n, jc(i), Y_IN1, INK); jack(jc(i), Y_IN1, n)
for i, n in enumerate(IN2): jlabel(n, jc(i), Y_IN2, INK if i < 2 else SOFT); jack(jc(i), Y_IN2, n)
ox0 = jc(8) - 6.6
P.append(f'<rect x="{ox0:.2f}" y="{top+0.6}" width="{W-3.2-ox0:.2f}" height="{Y_IN2 - top + 2.7}" rx="1.4" fill="{ACCENT}"/>')
OUT1 = ["V/OCT", "GATE", "VEL", "PEDAL", "SCALE"]
OUT2 = ["BASS\nV/OCT", "BASS\nGATE", "CHANGE", "EOC", ""]
for i, n in enumerate(OUT1): jlabel(n, jc(8 + i), Y_IN1, BG_DARK); jack(jc(8 + i), Y_IN1, n)
for i, n in enumerate(OUT2):
    if n: jlabel(n, jc(8 + i), Y_IN2, BG_DARK); jack(jc(8 + i), Y_IN2, n)
T("Hær", W / 2, 126.6, 3.2, INK, fam="C", weight=400, check=False)
ann = ""
for (x, y, r, tag) in news:
    if tag == "disp":
        ann += f'<rect x="{DX+0.4}" y="{DY+0.4}" width="{DW-0.8}" height="{DH-0.8}" rx="1" fill="none" stroke="#4FD1C5" stroke-width="0.6" stroke-dasharray="1.2 0.8"/>'
        ann += f'<text x="{W/2+14}" y="{DY-0.9}" font-family="I" font-weight="700" font-size="2.4" fill="#4FD1C5" text-anchor="middle">NEW: preset bar on top, chord chart strip at the bottom</text>'
    elif x == "edit":
        ann += f'<rect x="{kc(1)-7}" y="{RY[0]-10}" width="14" height="{RY[2]-RY[0]+16}" rx="1" fill="none" stroke="#4FD1C5" stroke-width="0.6" stroke-dasharray="1.2 0.8"/>'
    else:
        ann += f'<circle cx="{x}" cy="{y}" r="{r}" fill="none" stroke="#4FD1C5" stroke-width="0.6" stroke-dasharray="1.2 0.8"/>'
        ann += f'<text x="{x}" y="{y+r+2.6:.2f}" font-family="I" font-weight="700" font-size="2.4" fill="#4FD1C5" text-anchor="middle">NEW</text>'
body = "\n".join(p for p in P if not p.startswith("__HEAD__"))
heads = [p[8:].split("|") for p in P if p.startswith("__HEAD__")]
html = f'''<!doctype html><html><head><style>
@font-face{{font-family:I;src:url(file:///usr/share/fonts/opentype/inter/Inter-Bold.otf);font-weight:700}}
@font-face{{font-family:I;src:url(file:///usr/share/fonts/opentype/inter/Inter-Medium.otf);font-weight:500}}
@font-face{{font-family:M;src:url(file://{sys.path[0]}/ShareTechMono.ttf)}}
@font-face{{font-family:C;src:url(file://{sys.path[0]}/ClimateCrisis.ttf)}}
body{{margin:0;background:#111}}</style></head><body>
<svg id="p" xmlns="http://www.w3.org/2000/svg" width="{W*6:.0f}" height="{H*6:.0f}" viewBox="0 0 {W} {H}">{body}<g id="hl"></g>{ann}</svg>
<script>
const heads={json.dumps(heads)}, comps={json.dumps(comps)}, W={W};
window.check=()=>{{const svg=document.getElementById('p');const bad=[];const L=[];
for(const t of svg.querySelectorAll('text.t')){{const b=t.getBBox();if(t.dataset.c==='1')L.push([t.textContent,b.x,b.y+b.height*0.18,b.x+b.width,b.y+b.height*0.95]);if(b.x<0.8||b.x+b.width>W-0.8)bad.push([t.textContent,'edge']);}}
for(const h of heads){{const t=[...svg.querySelectorAll('text.t')].find(e=>e.textContent===h[0]);const b=t.getBBox();
const l=document.createElementNS('http://www.w3.org/2000/svg','line');l.setAttribute('x1',b.x+b.width+1.4);l.setAttribute('x2',h[2]);l.setAttribute('y1',h[3]-1.1);l.setAttribute('y2',h[3]-1.1);l.setAttribute('stroke','{LINE}');l.setAttribute('stroke-width','0.3');document.getElementById('hl').appendChild(l);}}
const A=L.concat(comps);for(let i=0;i<A.length;i++)for(let j=i+1;j<A.length;j++){{const a=A[i],b=A[j];if(a[1]<b[3]&&b[1]<a[3]&&a[2]<b[4]&&b[2]<a[4])bad.push([a[0],b[0]]);}}return bad;}};
</script></body></html>'''
open(sys.path[0] + ("/annot2.html" if ANNOT else "/panel2.html"), "w").write(html)
