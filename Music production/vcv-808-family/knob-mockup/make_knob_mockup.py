#!/usr/bin/env python3
# Mockup: 808 Circuit Bench with the screen replaced by 16 parameter knobs that follow the
# selected voice. Labels/values/group names are drawn live by the module (big, readable).
#   python3 make_knob_mockup.py bench2.json out.png CODE
import os, sys, re, base64, json, subprocess, tempfile, math
LIB = os.path.expanduser("~/Rack-src/res/ComponentLibrary")
data = json.load(open(sys.argv[1])); out = os.path.abspath(sys.argv[2]); want = sys.argv[3]
V = next(i for i, v in enumerate(data) if v["code"] == want); v = data[V]
W, H = 182.88, 128.5        # 36 HP (was 48)
BG, LINE, INK, SOFT, ORA, AMB, RED = "#1F2023", "#3A3B40", "#ECE6D8", "#9A968C", "#F08A24", "#F3B04A", "#E0533D"
PX = 25.4 / 75.0
g = []
def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    d = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{d}" x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}"/></g>')
def t(s, x, y, size, fill=INK, anchor="middle", font="Inter", weight=700, ls=0.0):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{weight}" letter-spacing="{ls}" text-anchor="{anchor}" fill="{fill}">{s}</text>')
def section(name, x0, x1, yy):
    t(name, x0, yy, 3.0, fill=ORA, anchor="start", ls=0.35)
    w = len(name) * 2.35 + 2.5
    g.append(f'<line x1="{x0+w}" y1="{yy-1}" x2="{x1}" y2="{yy-1}" stroke="{LINE}" stroke-width="0.35"/>')

g.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
t("808 CIRCUIT BENCH", 8, 11.5, 5.4, anchor="start", ls=0.4)
# live voice name (module draws it in the voice colour)
t(v["code"] + "  " + v["name"], W - 8, 11.3, 4.4, fill=v["colour"], anchor="end", font="Inter", weight=800)

# ---- 16 knobs, 2 rows of 8, in signal order; block names over each run of knobs ----
blocks = sorted(v["blocks"], key=lambda b: (b["col"], b["row"]))
slots = [(b["name"], p) for b in blocks for p in b["p"]]
COLS, X0, X1 = 8, 8.0, W - 8.0
PIT = (X1 - X0) / COLS
ROWS_Y = [29.0, 58.5]
for r in range(2):
    ky = ROWS_Y[r]
    row = slots[r*COLS:(r+1)*COLS]
    # group headings
    i = 0
    while i < len(row):
        j = i
        while j + 1 < len(row) and row[j+1][0] == row[i][0]: j += 1
        xa, xb = X0 + PIT*i + 1.2, X0 + PIT*(j+1) - 1.2
        hy = ky - 10.2
        g.append(f'<path d="M{xa} {hy+1.6} V{hy} H{xb} V{hy+1.6}" fill="none" stroke="#5A5B60" stroke-width="0.35"/>')
        nm = row[i][0]
        tw = len(nm) * 1.95 + 2.4
        g.append(f'<rect x="{(xa+xb)/2-tw/2}" y="{hy-1.8}" width="{tw}" height="3.6" fill="{BG}"/>')
        t(nm, (xa+xb)/2, hy+1.05, 2.9, fill=SOFT, weight=600, ls=0.15)
        i = j + 1
    for c in range(COLS):
        x = X0 + PIT*(c+0.5)
        if r*COLS + c >= len(slots):
            comp("RoundBlackKnob", x, ky, -149.4 + 298.8*0.5)
            g.append(f'<circle cx="{x}" cy="{ky}" r="4.9" fill="{BG}" opacity="0.65"/>')
            t("-", x, ky+9.4, 3.2, fill="#55565A")
            continue
        _, (name, val, k) = slots[r*COLS + c]
        ang = -149.4 + 298.8*k
        comp("RoundBlackKnob", x, ky, ang)
        a = math.radians(ang - 90); rr = 6.1
        g.append(f'<circle cx="{x+rr*math.cos(a):.2f}" cy="{ky+rr*math.sin(a):.2f}" r="0.55" fill="{RED}"/>')
        t(name, x, ky+9.4, 3.2)
        t(val, x, ky+13.4, 2.9, fill=AMB, font="ShareTech", weight=400)

# ---- edit buttons ----
BY = 80.0
def btn(x, lab, w=13.0, fill="#3A3B40", ink=INK):
    g.append(f'<rect x="{x-w/2}" y="{BY-2.6}" width="{w}" height="5.2" rx="1" fill="{fill}"/>')
    t(lab, x, BY+1.1, 3.0, fill=ink)
btn(16, "STOCK"); btn(32, "COPY"); btn(48, "SHIFT")
btn(66, "KITS")
for i, L in enumerate("ABCD"):
    x = 77 + i*6.0
    g.append(f'<circle cx="{x}" cy="{BY-0.2}" r="1.3" fill="{AMB if i == 0 else "#3A3020"}"/>')
    t(L, x, BY+5.6, 2.6, fill=SOFT, weight=600)
t("SHIFT + voice = hit   |   SHIFT + STOCK = whole voice   |   SHIFT + KITS = stock kit", W-8, BY+1.0, 2.3, fill=SOFT, anchor="end", weight=500)

# ---- voices + mix ----
MIXW = 26.0
VP = (W - 8.0 - 4.0 - MIXW - 4.0) / 13.0
VOICE_Y, TRIG_Y, OUT_Y = 92.6, 102.0, 115.2
section("VOICES", 4.0, W - MIXW - 8.0, 87.6)
section("MIX", W - MIXW - 4.0, W - 4.0, 87.6)
t("TRIG", 4.0 + 0.0, TRIG_Y - 5.6, 2.3, fill=SOFT, anchor="start", weight=500)
for i, d in enumerate(data):
    x = 4.0 + VP*(i+0.5) + 0.0
    x = 8.0 + VP*(i+0.5) - 4.0 + 0.0
    w = VP - 1.6
    g.append(f'<rect x="{x-w/2}" y="{VOICE_Y-2.4}" width="{w}" height="4.8" rx="1" fill="{d["colour"]}" opacity="{1 if i == V else 0.8}"/>')
    if i == V: g.append(f'<rect x="{x-w/2-0.55}" y="{VOICE_Y-2.95}" width="{w+1.1}" height="5.9" rx="1.4" fill="none" stroke="{INK}" stroke-width="0.45"/>')
    t(d["code"], x, VOICE_Y+1.15, 3.2, fill="#1B1B1B")
    g.append(f'<rect x="{x-4.2}" y="{OUT_Y-5.4}" width="8.4" height="10.4" rx="1.2" fill="#4A4B50"/>')
    comp("PJ301M", x, TRIG_Y); comp("PJ301M", x, OUT_Y)
MX = [W - MIXW - 4.0 + 7.0, W - 4.0 - 6.0]
for x, lab in zip(MX, ["ACC", "POLY"]):
    t(lab, x, VOICE_Y+1.1, 2.9); comp("PJ301M", x, TRIG_Y)
for x, lab in zip(MX, ["L", "R"]):
    g.append(f'<rect x="{x-4.6}" y="{OUT_Y-8.6}" width="9.2" height="13.6" rx="1.2" fill="{ORA}"/>')
    t(lab, x, OUT_Y-5.0, 2.6, fill="#1B1B1B"); comp("PJ301M", x, OUT_Y)
t("OUT", 4.0, OUT_Y - 6.2, 2.3, fill=SOFT, anchor="start", weight=500)
t("Hær", W/2, 125.6, 2.8, weight=800, font="Climate Crisis")

S = 8
svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(W*S)}" height="{int(H*S)}" viewBox="0 0 {W} {H}">' + "".join(g) + "</svg>"
F = "/usr/share/fonts/opentype/inter"
html = f"""<html><head><style>
@font-face{{font-family:Inter;font-weight:500;src:url(file://{F}/Inter-Medium.otf)}}
@font-face{{font-family:Inter;font-weight:600;src:url(file://{F}/Inter-SemiBold.otf)}}
@font-face{{font-family:Inter;font-weight:700;src:url(file://{F}/Inter-Bold.otf)}}
@font-face{{font-family:Inter;font-weight:800;src:url(file://{F}/Inter-ExtraBold.otf)}}
@font-face{{font-family:ShareTech;src:url(file://{os.path.expanduser('~/Rack-src/res/fonts')}/ShareTechMono-Regular.ttf)}}
body{{margin:0;background:#111}}</style></head><body>{svg}</body></html>"""
tmp = tempfile.NamedTemporaryFile("w", suffix=".html", delete=False); tmp.write(html); tmp.close()
js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d}});
await p.goto('file://%s');await p.waitForTimeout(400);await p.screenshot({path:'%s'});await b.close()})()""" % (int(W*S), int(H*S), tmp.name, out)
jf = tmp.name + ".js"; open(jf, "w").write(js)
subprocess.run(["node", jf], check=True); print("wrote", out)
