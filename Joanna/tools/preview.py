#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Joanna.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 172.72, 128.5
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


C = L["COL"]
for x, v in zip(C, [0.5, 0.4, 0.5, 0.5, 0.5, 0.4]): knob(x, L["ROW_PIANO"], v)
for x, v in zip(C[:5], [0.5, 0.0, 0.3, 0.3, 0.5]): knob(x, L["ROW_LIFE"], v)
comp("VCVBezel", C[5], L["ROW_LIFE"])
comp("CKSSThree_1", C[0], L["ROW_HOUSE"])
for x, v in zip(C[1:], [0.3, 0.35, 0.35, 0.4, 0.5]): knob(x, L["ROW_HOUSE"], v)
for i, x in enumerate(C[:3]):
    comp("VCVButton_0", x, L["ROW_PEDAL"])
    if i == 0:
        g.append(f'<circle cx="{x}" cy="{L["ROW_PEDAL"]}" r="1.6" fill="#fffbe6" fill-opacity="0.9"/>')
for x in C[3:]: comp("PJ301M", x, L["ROW_PEDAL"])
for x in L["IOX"]: comp("PJ301M", x, L["ROW_IO"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)
px, py, pw_, ph_ = L["PLATE_X"], L["PLATE_Y"], L["PLATE_W"], L["PLATE_H"]
g.append(f'<text x="{px + pw_ / 2}" y="{py + ph_ / 2 + 1.5}" font-family="DejaVu Sans, sans-serif" font-size="4.4" text-anchor="middle" fill="#4a2416">No. 4127</text>')

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
