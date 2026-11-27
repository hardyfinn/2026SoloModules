#!/usr/bin/env python3
# Picture of the 808 Circuit Bench panel with Rack's component graphics and the live parts
# drawn as the module draws them (knob slots from tools/bench_dump.cpp).
#   g++ -std=c++11 tools/bench_dump.cpp -o /tmp/bench_dump && /tmp/bench_dump [kit] > /tmp/bench.json
#   python3 tools/preview_bench.py /tmp/bench.json out.png [voice code] [kit letter] [kit name]
import os, sys, re, base64, json, subprocess, tempfile, math
HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.expanduser("~/Rack-src/res/ComponentLibrary")
FONTS = os.path.expanduser("~/Rack-src/res/fonts")
BOLD = os.path.join(HERE, "..", "res", "fonts", "DejaVuSans-Bold.ttf")
data = json.load(open(sys.argv[1]))
out = os.path.abspath(sys.argv[2])
want = sys.argv[3] if len(sys.argv) > 3 else "SD"
kitL = sys.argv[4] if len(sys.argv) > 4 else "A"
kitName = sys.argv[5] if len(sys.argv) > 5 else "Stock 808"
V = next(i for i, v in enumerate(data) if v["code"] == want); v = data[V]
W, H = 182.88, 128.5
GX0, GX1, COLS = 8.0, W - 8.0, 8
PIT = (GX1 - GX0) / COLS
ROW_Y, LAB_DY, VAL_DY, HEAD_DY = [29.0, 58.5], 9.4, 13.4, -10.0
BTN_Y, BTN_W, BTN_H = 79.0, 12.0, 5.2
VOICE_X0, VOICE_P, VOICE_Y, TRIG_Y, OUT_Y = 13.0, 10.5, 92.4, 102.4, 115.4
MIX_X = [W - 23.5, W - 10.5]
INK, DIM, AMB, RED, RULE = "#ECE6D8", "#9A968C", "#F3B04A", "#E0533D", "#5A5B60"
PX = 25.4 / 75.0
panel = open(os.path.join(HERE, "..", "res", "Bench808.svg")).read()
g = []
def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    d = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{d}" x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}"/></g>')
    return w / 2
def t(s, x, y, size, fill=INK, anchor="middle", font="Bold", ls=0.0, base="alphabetic"):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" letter-spacing="{ls}" text-anchor="{anchor}" dominant-baseline="{base}" fill="{fill}">{s}</text>')
# title
t(v["code"] + "  " + v["name"], W - 8, 11.3, 4.4, fill=v["colour"], anchor="end")
t("KIT " + kitL + ": " + kitName, W - 8, 15.3, 2.5, fill=DIM, anchor="end")
S = v["slots"]
# stage headings
for row in range(2):
    i, end = row * COLS, min(len(S), (row + 1) * COLS)
    while i < end:
        j = i
        while j + 1 < end and S[j + 1][0] == S[i][0]: j += 1
        xa, xb, hy = GX0 + PIT * (i % COLS) + 1.2, GX0 + PIT * ((j % COLS) + 1) - 1.2, ROW_Y[row] + HEAD_DY
        nm = S[i][0]; raw = len(nm) * 2.05; fit = min(1.0, (xb - xa - 2.4) / raw)
        tw = raw * fit + 2.4; cx = (xa + xb) / 2
        g.append(f'<path d="M{xa} {hy+1.6} V{hy} H{max(xa, cx-tw/2)} M{min(xb, cx+tw/2)} {hy} H{xb} V{hy+1.6}" fill="none" stroke="{RULE}" stroke-width="0.35"/>')
        t(nm, cx, hy + 1.0 * fit, 2.9 * fit, fill=DIM, ls=0.15 * fit)
        i = j + 1
for s, (blk, name, val, k, st) in enumerate(S):
    x, y = GX0 + PIT * ((s % COLS) + 0.5), ROW_Y[s // COLS]
    r = comp("RoundBlackKnob", x, y, -149.4 + 298.8 * k)
    def dot(kk, c):
        a = math.radians(-149.4 + 298.8 * kk - 90); rr = r + 0.9
        g.append(f'<circle cx="{x+rr*math.cos(a):.2f}" cy="{y+rr*math.sin(a):.2f}" r="0.55" fill="{c}"/>')
    dot(st, RED)
    if abs(k - st) > 0.004: dot(k, AMB)   # a loaded factory kit: amber = its value
    t(name, x, y + LAB_DY, 3.2)
    t(val, x, y + VAL_DY, 2.9, fill=AMB, font="ShareTech")
# buttons
def btn(x, y, lab, w=BTN_W, h=BTN_H, fill="#3A3B40", ink=INK, size=3.0, outline=False):
    g.append(f'<rect x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}" rx="1" fill="{fill}"/>')
    if outline: g.append(f'<rect x="{x-w/2-0.55}" y="{y-h/2-0.55}" width="{w+1.1}" height="{h+1.1}" rx="1.4" fill="none" stroke="{INK}" stroke-width="0.4"/>')
    t(lab, x, y + size * 0.36, size, fill=ink)
btn(14, BTN_Y, "STOCK"); btn(28, BTN_Y, "COPY"); btn(42, BTN_Y, "SHIFT"); btn(58, BTN_Y, "KITS")
btn(100, BTN_Y, "RAND", fill="#5A3A1A", ink=AMB)
for i in range(4):
    g.append(f'<circle cx="{67 + i*6}" cy="{BTN_Y-0.2}" r="1.3" fill="{AMB if "ABCD"[i] == kitL else "#3A3020"}"/>')
comp("PJ301M", 113.5, BTN_Y)
def mix(c, a): return c  # voice colours as drawn (unlit = darkened a touch)
for i, d in enumerate(data):
    x = VOICE_X0 + VOICE_P * (i + 0.5)
    w = VOICE_P - 1.6
    g.append(f'<rect x="{x-w/2}" y="{VOICE_Y-2.4}" width="{w}" height="4.8" rx="1" fill="{d["colour"]}" opacity="{1 if i == V else 0.78}"/>')
    if i == V: g.append(f'<rect x="{x-w/2-0.55}" y="{VOICE_Y-2.95}" width="{w+1.1}" height="5.9" rx="1.4" fill="none" stroke="{INK}" stroke-width="0.4"/>')
    t(d["code"], x, VOICE_Y + 1.15, 3.2, fill="#1B1B1B")
    comp("PJ301M", x, TRIG_Y); comp("PJ301M", x, OUT_Y)
for x in MIX_X: comp("PJ301M", x, TRIG_Y); comp("PJ301M", x, OUT_Y)
svg = panel.replace("</svg>", "".join(g) + "</svg>")
SC = 8
svg = re.sub(r'width="[0-9.]+mm" height="[0-9.]+mm"', f'width="{int(W*SC)}" height="{int(H*SC)}"', svg, count=1)
html = f"""<html><head><meta charset="utf-8"><style>@font-face{{font-family:ShareTech;src:url(file://{FONTS}/ShareTechMono-Regular.ttf)}}
@font-face{{font-family:Bold;src:url(file://{os.path.abspath(BOLD)})}}
body{{margin:0;background:#111}}</style></head><body>{svg}</body></html>"""
tmp = tempfile.NamedTemporaryFile("w", suffix=".html", delete=False); tmp.write(html); tmp.close()
js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d}});
await p.goto('file://%s');await p.waitForTimeout(400);await p.screenshot({path:'%s'});await b.close()})()""" % (int(W*SC), int(H*SC), tmp.name, out)
jf = tmp.name + ".js"; open(jf, "w").write(js)
subprocess.run(["node", jf], check=True)
print("wrote", out)
