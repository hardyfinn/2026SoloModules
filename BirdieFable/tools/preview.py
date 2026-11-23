#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Birdie.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 182.88, 128.5
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

# default settings (knob positions as in the module's defaults)
vals = [[0.143, 0.837, 0.309, 0.25, 0.0, 0.0, 0.708, 0.0],
        [0.5, 0.375, 0.25, 0.224, 0.483, 0.734, 0.0, 0.0],
        [0.535, 0.667, 0.6, 0.333, None, 0.934, 0.0, None],
        [0.161, 0.590, 0.333, 0.5, 0.5, 0.5, 0.5, 0.526]]
C = L["COLS"]
for r, y in enumerate(L["ROW_Y"]):
    for i, x in enumerate(C):
        v = vals[r][i]
        if v is not None: knob(x, y, v)
comp("CKSS_1", C[4] - 2.0, L["ROW_Y"][2])
comp("VCVBezel", C[7], L["ROW_Y"][2])
RX, RY = L["RX"], L["RY"]
comp("VCVButton_0", RX[0], RY[0]); comp("VCVButton_0", RX[1], RY[0]); comp("VCVButton_0", RX[0], RY[1])
comp("CKSS_0", RX[1] - 2.0, RY[1]); comp("CKSS_0", RX[0] - 2.0, RY[2])
g.append(f'<circle cx="{L["LIGHT_X"]}" cy="{L["LIGHT_Y"]}" r="1.5" fill="#ff8a3d"/>')
for i in range(12):
    x = L["JX0"] + L["JDX"] * i
    comp("PJ301M", x, L["JA_Y"])
    if i not in (5, 6): comp("PJ301M", x, L["JB_Y"])
dx, dy = L["DISP_X"], L["DISP_Y"]
g.append(f'<text x="{dx + 1.8}" y="{dy + 5.9}" font-family="monospace" font-size="4.8" fill="#ff8a3d">MOTIF 3/6</text>')
g.append(f'<text x="{dx + L["DISP_W"] - 1.6}" y="{dy + 5.5}" text-anchor="end" font-family="monospace" font-size="2.6" fill="#efe9d2">FREE  SEED 0421</text>')
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
