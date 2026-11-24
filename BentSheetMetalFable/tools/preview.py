#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics, at the default settings.
#   python3 tools/preview.py out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "BentSheetMetal.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 203.2, 128.5
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

C10, C11 = L["COL10"], L["COL11"]
Y1, Y2, Y3, Y4, Y5 = L["Y1"], L["Y2"], L["Y3"], L["Y4"], L["Y5"]
R2 = L["ROW2_X"]
def pos(p, lo, hi): return (p - lo) / (hi - lo)
# defaults: PITCH TAPER GRIP DECAY DIP DEPTH STRETCH PICKUP OUT AGE LEVEL
for x, v in zip(C11, [0.5, 0.7, 0.7, pos(1, 0.25, 4), 0, 0.75, 1 / 3, 0.2, 0, 0.3, 0.7]): knob("RoundBlackKnob", x, Y1, v)
knob("RoundBigBlackKnob", R2[0], Y2, pos(0.45, -1, 1))
knob("RoundBigBlackKnob", R2[6], Y2, 0.27)
for x, v in zip(R2[1:6] + R2[7:], [1, 0.5, 0.9, 0, 0.35, 0.44, 0.5]): knob("RoundBlackKnob", x, Y2, v)
for x, v in zip(L["ROW3_KNOBS"], [0.5, 0, 0.5]): knob("RoundSmallBlackKnob", x, Y3, v)
for x in L["ROW3_OUTS"]: comp("PJ301M", x, Y3)
# a still frame of the display: an S-bent blade, bow mark in the middle
import math
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
pts = []
for i in range(49):
    t = i / 48; y = dy + dh / 2 - 0.32 * dh * math.sin(2 * math.pi * (t - 0.5)) * (1 - 0.3 * t)
    pts.append(f"{dx + 3 + t * (dw - 6):.2f},{y:.2f}")
g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="#d8dde2" stroke-width="0.6"/>')
bx = dx + 3 + 0.5 * (dw - 6)
g.append(f'<line x1="{bx}" y1="{dy + 1.5}" x2="{bx}" y2="{dy + dh - 1.5}" stroke="#d9a441" stroke-width="0.5"/>')
for x in C10: comp("PJ301M", x, Y4)
for x in C11: comp("PJ301M", x, Y5)
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
