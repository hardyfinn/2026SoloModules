#!/usr/bin/env python3
# Picture of the 909 Circuit Bench panel with Rack's component graphics and the screen as
# the module draws it (layout and stock values from tools/bench_dump.cpp).
#   g++ -std=c++11 tools/bench_dump.cpp -o /tmp/bench_dump && /tmp/bench_dump > /tmp/bench.json
#   python3 tools/preview_bench.py /tmp/bench.json out.png [voice code] [block index]
import os, sys, re, base64, json, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
LIB = os.path.expanduser("~/Rack-src/res/ComponentLibrary")
FONTS = os.path.expanduser("~/Rack-src/res/fonts")
data = json.load(open(sys.argv[1]))
out = os.path.abspath(sys.argv[2])
want = sys.argv[3] if len(sys.argv) > 3 else "BD"
V = next(i for i, v in enumerate(data) if v["code"] == want)
W, H = 243.84, 128.5
SCR = (8.0, 20.0, 227.84, 47.0)
PITCH = (W - 44.0) / len(data)
EDIT_X, SEL_X, KNOB_Y = [28.0, 50.0, 72.0, 94.0], 128.0, 84.5
VOICE_Y, TRIG_Y, OUT_Y, MIX_X = 99.0, 106.4, 118.4, [W - 26.0, W - 14.0]
COLP, BLKW, ROWH = (SCR[2] - 8.0) / 5.0, 37.0, 3.3
PX = 25.4 / 75.0
INK, DIM, AMB, RED, PINK = "#ECE6D8", "#8C8A84", "#F3B04A", "#E5631F", "#262626"
panel = open(os.path.join(HERE, "..", "res", "Bench909.svg")).read()
g = []
def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    d = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{d}" x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}"/></g>')
def t(s, x, y, size, fill=INK, anchor="start", font="ShareTech", weight=400):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{fill}">{s}</text>')
v = data[V]
blocks = v["blocks"]
sb = int(sys.argv[4]) if len(sys.argv) > 4 else next((i for i, b in enumerate(blocks) if any(p[0] == "TUNE" for p in b["p"])), 1)
sx, sy = SCR[0], SCR[1]
def bx(b): return sx + 4.0 + b["col"] * COLP
def by(b): return sy + (9.0 if b["row"] == 0 else 28.0 if b["row"] == 1 else 18.0)
def bh(b): return 4.8 + ROWH * len(b["p"])
t(v["code"] + "  " + v["name"], sx + 3, sy + 5.8, 4.2, fill=v["colour"])
t("tap a block, drag a value   |   KIT A", sx + SCR[2] - 3, sy + 5.6, 2.6, fill=DIM, anchor="end")
for a, b2 in v["arrows"]:
    A, B = blocks[a], blocks[b2]
    if A["col"] == B["col"]:
        x = bx(A) + BLKW / 2
        g.append(f'<path d="M{x} {by(A)+bh(A)} V{by(B)-0.4}" stroke="{DIM}" stroke-width="0.3"/><path d="M{x} {by(B)} l-0.8 -1.4 h1.6z" fill="{DIM}"/>')
        continue
    y0, y1, x0, x1 = by(A) + 2.6, by(B) + 2.6, bx(A) + BLKW, bx(B)
    mx = (x0 + x1) / 2
    g.append(f'<path d="M{x0} {y0} H{mx} V{y1} H{x1-0.4}" fill="none" stroke="{DIM}" stroke-width="0.3"/><path d="M{x1} {y1} l-1.4 -0.8 v1.6z" fill="{DIM}"/>')
for i, b in enumerate(blocks):
    sel = i == sb
    g.append(f'<rect x="{bx(b)}" y="{by(b)}" width="{BLKW}" height="{bh(b)}" rx="0.8" fill="{"#2A3A31" if sel else "#151D19"}" stroke="{AMB if sel else "#4A5A52"}" stroke-width="{0.5 if sel else 0.3}"/>')
    t(b["name"], bx(b) + 1.8, by(b) + 3.5, 2.6, fill=AMB if sel else INK)
    for j, (nm, val) in enumerate(b["p"]):
        yy = by(b) + 7.1 + j * ROWH
        t((f"{j+1} " if sel else "") + nm, bx(b) + 1.8, yy, 2.25, fill=INK if sel else DIM)
        t(val, bx(b) + BLKW - 4.4, yy, 2.25, fill=AMB, anchor="end")
        g.append(f'<circle cx="{bx(b)+BLKW-2.3}" cy="{yy-0.75}" r="0.6" fill="{RED}"/>')
selp = blocks[sb]["p"]
for j in range(4):
    comp("RoundBlackKnob_bg", EDIT_X[j], KNOB_Y) if os.path.exists(os.path.join(LIB, "RoundBlackKnob_bg.svg")) else None
    comp("RoundBlackKnob", EDIT_X[j], KNOB_Y, -149.4 + 298.8 * 0.5)
    t(f"{j+1} " + (selp[j][0] if j < len(selp) else "-"), EDIT_X[j], 77.3, 3.4, fill=PINK, anchor="middle", font="DejaVu Sans")
if os.path.exists(os.path.join(LIB, "RoundLargeBlackKnob_bg.svg")): comp("RoundLargeBlackKnob_bg", SEL_X, KNOB_Y)
comp("RoundLargeBlackKnob", SEL_X, KNOB_Y, -149.4 + 298.8 * (sb + 0.5) / len(blocks))
for (x, y, lab) in [(148, 81, "STOCK"), (148, 87.5, "COPY"), (164, 81, "KITS"), (164, 87.5, "SHIFT")]:
    g.append(f'<rect x="{x-5.5}" y="{y-2.3}" width="11" height="4.6" rx="1" fill="#3A3B40"/>')
    t(lab, x, y + 0.9, 2.5, anchor="middle", font="DejaVu Sans", weight=700)
for i, d in enumerate(data):
    x = 8.0 + PITCH * (i + 0.5)
    w = PITCH - 2.4
    g.append(f'<rect x="{x-w/2}" y="{VOICE_Y-2.3}" width="{w}" height="4.6" rx="1" fill="{d["colour"]}" stroke="#9C9A93" stroke-width="0.25"/>')
    if i == V: g.append(f'<rect x="{x-w/2-0.55}" y="{VOICE_Y-2.85}" width="{w+1.1}" height="5.7" rx="1.4" fill="none" stroke="{PINK}" stroke-width="0.4"/>')
    t(d["code"], x, VOICE_Y + 1.1, 3.2, fill="#1B1B1B", anchor="middle", font="DejaVu Sans", weight=700)
    comp("PJ301M", x, TRIG_Y); comp("PJ301M", x, OUT_Y)
for x in MIX_X: comp("PJ301M", x, 104.6); comp("PJ301M", x, OUT_Y)
svg = panel.replace("</svg>", "".join(g) + "</svg>")
svg = re.sub(r'width="[0-9.]+mm" height="[0-9.]+mm"', f'width="{int(W*8)}" height="{int(H*8)}"', svg, count=1)
html = f"""<html><head><style>@font-face{{font-family:ShareTech;src:url(file://{FONTS}/ShareTechMono-Regular.ttf)}}
body{{margin:0;background:#111}}</style></head><body>{svg}</body></html>"""
tmp = tempfile.NamedTemporaryFile("w", suffix=".html", delete=False); tmp.write(html); tmp.close()
js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d}});
await p.goto('file://%s');await p.waitForTimeout(300);await p.screenshot({path:'%s'});await b.close()})()""" % (int(W*8), int(H*8), tmp.name, out)
jf = tmp.name + ".js"; open(jf, "w").write(js)
subprocess.run(["node", jf], check=True)
print("wrote", out)
