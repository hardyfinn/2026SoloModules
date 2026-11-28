# Comper 2.3.0 panel mockup: 48 HP, 3.2 mm control labels, 2.9 mm jack labels,
# preset browser in the display, OCTAVE and LENGTH knobs.
import sys, json
ANNOT = len(sys.argv) > 1 and sys.argv[1] == "annot"
W, H = 254.0, 128.5
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

# ---- display with preset browser (drawn as Rack would draw it)
P.append(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')
comps.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
dx, dy = DISP_X, DISP_Y
# preset bar along the top of the screen
P.append(f'<rect x="{dx+1.2}" y="{dy+1.2}" width="{DISP_W-2.4}" height="5.4" rx="0.8" fill="#1C1A14"/>')
P.append(f'<path d="M{dx+3.2} {dy+3.9} l2.6 -1.9 v3.8 z" fill="{AMB}"/>')
P.append(f'<path d="M{dx+DISP_W-3.2} {dy+3.9} l-2.6 -1.9 v3.8 z" fill="{AMB}"/>')
T("PRESET", dx+8.2, dy+5.0, 2.3, DIM, "start", 400, False, 0.08, "M")
T("04 · BOSSA", dx+21.0, dy+5.15, 3.4, AMB, "start", 400, False, 0.03, "M")
T("*", dx+41.6, dy+5.15, 3.4, AMB, "start", 400, False, 0, "M")
T("4 / 12 · TAP NAME FOR LIST", dx+DISP_W-8.0, dy+5.0, 2.3, DIM, "end", 400, False, 0.06, "M")
T("Dm7", dx+2.5, dy+13.6, 7.4, AMB, "start", 400, False, 0, "M")
T("&gt; G7", dx+19.0, dy+13.6, 4.2, DIM, "start", 400, False, 0, "M")
T("CHART · SEED 1 · OCT +1", dx+DISP_W-2.5, dy+10.6, 2.6, DIM, "end", 400, False, 0, "M")
T("F4 A4 C5 E5", dx+DISP_W-2.5, dy+15.6, 3.0, AMB, "end", 400, False, 0, "M")
if ANNOT: new(dx + DISP_W/2, dy + 3.9, None, "preset bar")
for x, n in zip(SMALLK_X, ["STEPS", "BEATS", "KEY"]):
    T(n, x, SMALLK_Y - 5.6, CONTROL); knob(x, SMALLK_Y, 4.0, n)
T("PER STEP", SMALLK_X[1], SMALLK_Y + 7.4, SMALL, SOFT)
T("± 6 ST", SMALLK_X[2], SMALLK_Y + 7.4, SMALL, SOFT)

# ---- chart
T("CHART", 3.2, Y_STEPLIGHT + 1.1, CONTROL, ACCENT, "start", ls=0.06)
T("ROOT", 3.2, Y_ROOT + 1.1, CONTROL, SOFT, "start")
T("QUAL", 3.2, Y_QUAL + 1.1, CONTROL, SOFT, "start")
names = ["Dm7", "G7", "Cmaj7", "Cmaj7"]
for i in range(16):
    x = step(i)
    if i % 4 == 0 and i:
        P.append(f'<line x1="{x-STEP_DX/2:.2f}" y1="{Y_STEPLIGHT-2.6}" x2="{x-STEP_DX/2:.2f}" y2="{Y_STEPNAME+1.0}" stroke="{LINE}" stroke-width="0.3"/>')
    light(x, Y_STEPLIGHT, on=(i == 1))
    T(str(i + 1), x + 2.2, Y_STEPLIGHT + 0.9, 2.4, SOFT, "start", check=False)
    knob(x, Y_ROOT, 3.1, "root"); knob(x, Y_QUAL, 3.1, "qual")
    T(names[i % 4], x, Y_STEPNAME, 3.0, AMB if i < 4 else DIM, "middle", 400, False, 0, "M")
P.append(f'<line x1="3.0" y1="{Y_STEPNAME+2.4}" x2="{W-3.0}" y2="{Y_STEPNAME+2.4}" stroke="{LINE}" stroke-width="0.3"/>')

# ---- knob band
def heading(s, x0, x1, y):
    T(s, x0, y, HEAD, ACCENT, "start", ls=0.1)
    P.append(f'__HEAD__{s}|{x0}|{x1}|{y}')
heading("VOICING", col(0) - 5.0, col(3) + 6.0, Y_HEAD)
heading("RHYTHM", col(4) - 6.0, col(7) + 6.0, Y_HEAD)
heading("HANDS", col(8) - 6.0, col(11) + 6.0, Y_HEAD)
heading("BAND", col(12) - 6.0, col(15) + 4.0, Y_HEAD)
ROWA = ["STYLE", "SPREAD", "REGISTER", "VOICES", "GROOVE", "DENSITY", "SWING", "REPEAT", "LOOSE", "TIMING", "ROLL", "WEIGHT", "LISTEN", "TEMPO", "RUN", ""]
ROWB = ["LEAD", "AUTRE", "ROOTS", "OCTAVE", "PUSH", "SEED", "NEW", "LENGTH", "SLOP", "PEDAL", "", "", "HIT", "SOLO", "", ""]
for i, n in enumerate(ROWA):
    if not n: continue
    r = 5.8 if n == "LOOSE" else (4.0 if n == "RUN" else 3.1 if n in ("TIMING", "ROLL", "WEIGHT") else 4.6)
    T(n, col(i), Y_ROWA - (r + 1.6 if n == "LOOSE" else 6.0 if r > 4 else 5.0), CONTROL)
    if n == "RUN": button(col(i), Y_ROWA, 3.0, n); light(col(i), Y_ROWA, 1.2, True, "runlight")
    else: knob(col(i), Y_ROWA, r, n)
for i, n in enumerate(ROWB):
    if not n: continue
    if n == "ROOTS": T(n, col(i), Y_ROWB - 6.0, CONTROL); switch3(col(i), Y_ROWB, n); continue
    if n in ("HIT", "SOLO"): T(n, col(i), Y_ROWB - 3.0, CONTROL); light(col(i), Y_ROWB, 1.2, n == "HIT", n); continue
    if n == "NEW": T(n, col(i), Y_ROWB - 5.0, CONTROL); button(col(i), Y_ROWB, 3.0, n); continue
    r = 3.1 if n == "SLOP" else 4.6
    T(n, col(i), Y_ROWB - (5.0 if r < 4 else 6.0), CONTROL); knob(col(i), Y_ROWB, r, n)
T("OFF·SOME·ALL", col(2), Y_ROWB + 7.0, 2.0, SOFT, check=False)
T("−3", col(3) - 5.2, Y_ROWB + 6.4, 2.4, SOFT, check=False); T("+3", col(3) + 5.2, Y_ROWB + 6.4, 2.4, SOFT, check=False)
T("SHORT", col(7) - 3.0, Y_ROWB + 7.0, 2.0, SOFT, "end", check=False); T("LEGATO", col(7) + 3.0, Y_ROWB + 7.0, 2.0, SOFT, "start", check=False)
T("TRIMS", col(10) + 7.5, Y_ROWB - 1.0, SMALL, SOFT); T("× LOOSE", col(10) + 7.5, Y_ROWB + 2.2, SMALL, SOFT)
T("BPM", col(13) + 5.4, Y_ROWA + 5.6, 2.2, SOFT, "start", check=False)
if ANNOT:
    new(col(3), Y_ROWB, 6.6, "OCTAVE ±3"); new(col(7), Y_ROWB, 6.6, "LENGTH")

# ---- jacks
top = Y_IN1 - 10.6
P.append(f'<rect x="2.0" y="{top}" width="{W-4.0}" height="{Y_IN2 - top + 4.3}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "RUN", "KEY", "ROOT", "QUAL", "CHORD\nV/OCT", "CHORD\nGATE", "SOLO", "SOLO\nV/OCT"]
IN2 = ["DENSITY", "AUTRE", "LOOSE", "LISTEN", "REGISTER"]
def jlabel(n, x, y, fill):
    l = n.split("\n")
    if len(l) == 1: T(n, x, y - 5.0, JACK, fill)
    else: T(l[0], x, y - 8.0, JACK, fill); T(l[1], x, y - 5.0, JACK, fill)
for i, n in enumerate(IN1): jlabel(n, col(i), Y_IN1, INK); jack(col(i), Y_IN1, n)
for i, n in enumerate(IN2): jlabel(n, col(i), Y_IN2, SOFT); jack(col(i), Y_IN2, n)
T("◂ CV IN", col(4) + 4.6, Y_IN2 + 1.0, SMALL, SOFT, "start")
ox0 = col(OUT_COL0) - 6.6
P.append(f'<rect x="{ox0:.2f}" y="{top+0.6}" width="{W-3.2-ox0:.2f}" height="{Y_IN2 - top + 3.7}" rx="1.4" fill="{ACCENT}"/>')
OUT1 = ["V/OCT", "GATE", "VEL", "PEDAL", "SCALE"]
OUT2 = ["BASS\nV/OCT", "BASS\nGATE", "CHANGE", "EOC", ""]
for i, n in enumerate(OUT1): jlabel(n, col(OUT_COL0 + i), Y_IN1, BG_DARK); jack(col(OUT_COL0 + i), Y_IN1, n)
for i, n in enumerate(OUT2):
    if n: jlabel(n, col(OUT_COL0 + i), Y_IN2, BG_DARK); jack(col(OUT_COL0 + i), Y_IN2, n)
T("Hær", W / 2, 127.7, 3.6, INK, fam="C", weight=400, check=False)

ann = ""
for (x, y, r, tag) in news:
    if r is None:
        ann += f'<rect x="{DISP_X+0.6}" y="{DISP_Y+0.6}" width="{DISP_W-1.2}" height="6.6" rx="1" fill="none" stroke="#4FD1C5" stroke-width="0.6" stroke-dasharray="1.2 0.8"/>'
        ann += f'<text x="{DISP_X+DISP_W/2+8}" y="{DISP_Y-1.0}" font-family="I" font-weight="700" font-size="2.6" fill="#4FD1C5" text-anchor="middle">NEW: preset browser (tap ◂ ▸ or the name)</text>'
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
open(sys.path[0] + ("/annot.html" if ANNOT else "/panel.html"), "w").write(html)
