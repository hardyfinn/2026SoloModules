#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Pond.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 111.76, 128.5
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

for x, v in zip(L["R1_X"][:4], [0.0, 1 / 15, 0.25, 7 / 15]): knob("RoundBlackKnob", x, L["R1_Y"], v)
for x, col in zip(L["R1_X"][4:], ["#e6503c", "#8cc63f"]):
    comp("VCVBezel", x, L["R1_Y"])
    g.append(f'<circle cx="{x}" cy="{L["R1_Y"]}" r="2.6" fill="{col}" fill-opacity="0.35"/>')
for x, v in zip(L["R2_X"], [0.5, 0.5, 0.5, 0.5, 0.5, 0.4]): knob("RoundBlackKnob", x, L["R2_Y"], v)
for i, (x, v) in enumerate(zip(L["R3_X"], [0.6, 0.7, 0.4, 0.85, 0.53])):
    knob("RoundLargeBlackKnob" if i in (0, 3) else "RoundBlackKnob", x, L["R3_Y"], v)
for i in range(9):
    x = L["JX0"] + L["JDX"] * i
    comp("PJ301M", x, L["JA_Y"]); comp("PJ301M", x, L["JB_Y"])
# screen
dx, dy = L["DISP_X"], L["DISP_Y"]
g.append(f'<text x="{dx + 1.8}" y="{dy + 4.8}" font-family="monospace" font-size="4.0" fill="#8cc63f">A SPRING PEEPER</text>')
g.append(f'<text x="{dx + 1.8}" y="{dy + 9.8}" font-family="monospace" font-size="4.0" fill="#f2b33d">B AMERICAN TOAD 25%</text>')
g.append(f'<text x="{dx + 1.8}" y="{dy + 14.7}" font-family="monospace" font-size="3.2" fill="#eaf0e0">3/8 CALLING  20C</text>')
mw = 36.0; mx = dx + L["DISP_W"] - mw - 1.2; my = dy + 1.2; mh = L["DISP_H"] - 2.4
g.append(f'<rect x="{mx}" y="{my}" width="{mw}" height="{mh}" rx="1.5" fill="#0c1e22"/>')
import math
for i in range(8):
    fx = (math.fmod(0.5 + i * 0.618, 1.0) * 2 - 1); fy = math.fmod(0.3 + i * 0.37, 1.0)
    lv = [0.9, 0, 0, 0.6, 0, 0, 0.8, 0][i]
    col = "#f2b33d" if i in (2, 5) else "#8cc63f"
    px = mx + 2 + (fx + 1) * 0.5 * (mw - 4); py = my + mh - 2 - fy * (mh - 4)
    g.append(f'<circle cx="{px:.2f}" cy="{py:.2f}" r="{0.7 + 0.9 * lv:.2f}" fill="{col}" fill-opacity="{0.25 + 0.75 * lv:.2f}"/>')
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
