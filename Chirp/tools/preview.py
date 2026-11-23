#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and the screen drawn as in the module.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, base64, re, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Chirp.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 81.28, 128.5
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(x, y, v):
    comp("RoundBlackKnob_bg", x, y)
    comp("RoundBlackKnob", x, y, -135 + 270 * v)

# default (Kick) knob positions
lg = math.log
vals = [[(-2.4 / 10) + 0.5, 3.2 / 12 + 0.5, lg(0.06 / 0.0005) / lg(4000), (-0.7 + 1) / 2],
        [lg(0.45 / 0.002) / lg(2000), 0.05, 0.0, lg(10 / 0.5) / lg(400)]]
C = L["COLS"]
for r, y in enumerate(L["ROW_Y"]):
    for i, x in enumerate(C):
        knob(x, y, vals[r][i])
comp("VCVButton_0", C[0], L["TRIG_Y"])
comp("CKSS_0", C[3], L["TRIG_Y"])
g.append(f'<circle cx="{L["LIGHT_X"]}" cy="{L["LIGHT_Y"]}" r="0.9" fill="#5fe08a"/>')
for y in L["JY"]:
    for x in C:
        comp("PJ301M", x, y)
# screen: kick glide + decay
sx, sy, sw, sh = L["SCOPE_X"], L["SCOPE_Y"], L["SCOPE_W"], L["SCOPE_H"]
pad = 0.7
sweep, time, curve, decay = 3.2, 0.06, -0.7, 0.45
win = max(time * 1.25, min(decay * 0.7, 4))
kk = 2 ** (curve * 3)
env = [f"{sx + pad} {sy + sh - pad}"]; pit = []
for i in range(81):
    t = win * i / 80; x = sx + pad + (sw - 2 * pad) * i / 80
    e = math.exp(-6.9 * t / decay); env.append(f"{x:.3f} {sy + sh - pad - (sh - 2 * pad) * e:.3f}")
    p = min(1, t / time); v = sweep * (1 - p ** kk)
    pit.append(f"{x:.3f} {sy + sh - pad - (sh - 2 * pad) * v / sweep:.3f}")
env.append(f"{sx + sw - pad} {sy + sh - pad}")
g.append(f'<path d="M{" L".join(env)} Z" fill="rgba(120,200,170,0.2)"/>')
g.append(f'<path d="M{" L".join(pit)}" fill="none" stroke="rgb(140,235,195)" stroke-width="0.45"/>')
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
