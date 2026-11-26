#!/usr/bin/env python3
# Picture of the Soloist panel with Rack's own component graphics and a mock display.
#   python3 tools/preview_walker.py out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "soloist.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else "/tmp/Rack-src/res/ComponentLibrary"
svg = open(os.path.join(HERE, "..", "res", "Soloist.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_soloist_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 152.4, 128.5
PX = 25.4 / 75.0
g = []
col = lambda i: L["COL_X0"] + i * L["COL_DX"]

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

def txt(s, x, y, size, fill="#F2C25C", anchor="start"):
    g.append(f'<text x="{x}" y="{y}" font-family="monospace" font-size="{size}" fill="{fill}" text-anchor="{anchor}">{s}</text>')

dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
txt("TENOR SAX", dx + 2, dy + dh * 0.45, 5.0)
txt("SOLO 2 OF 2", dx + 2, dy + dh * 0.84, 3.7, "#8A7A55")
import random; random.seed(3)
for k in range(22):
    x = dx + dw * 0.42 + k * (dw * 0.36 / 22); y = dy + dh * (0.8 - 0.6 * (0.5 + 0.4 * __import__("math").sin(k * 0.6) * random.random()))
    g.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.55" fill="#F2C25C"/>')
ex = dx + dw * 0.80; ew = dw * 0.06
g.append(f'<rect x="{ex}" y="{dy + dh * 0.62}" width="{ew}" height="{dh * 0.16}" fill="#2A261C"/>')
g.append(f'<rect x="{ex}" y="{dy + dh * 0.62}" width="{ew * 0.7}" height="{dh * 0.16}" fill="#F2C25C"/>')
txt("ENERGY", ex, dy + dh * 0.45, 3.0, "#8A7A55")
txt("LEADER", dx + dw - 2, dy + dh * 0.45, 3.0, "#8A7A55", "end"); txt("SEED 1", dx + dw - 2, dy + dh * 0.84, 3.0, "#8A7A55", "end")
for i, v in enumerate([0, 1 / 7, 2 / 3, 1, 0, .001]): knob("RoundBlackKnob", col(i), L["Y_FORM"], v)
comp("VCVButton_0", col(6), L["Y_FORM"])
for i, v in enumerate([.5, .4, .4, .1, .5, .5, (1 - .5) / 1.1]):
    knob("RoundLargeBlackKnob" if i == 3 else "RoundBlackKnob", col(i), L["Y_LINE"], v)
knob("RoundLargeBlackKnob", col(0), L["Y_HORN"], 0)
for i, v in enumerate([.5, .5, .35, .5]): knob("RoundBlackKnob", col(i + 1), L["Y_HORN"], v)
g.append(f'<circle cx="{col(6)}" cy="{L["Y_HORN"]}" r="1.5" fill="#F2C25C"/>')
g.append(f'<circle cx="{col(7)}" cy="{L["Y_HORN"]}" r="1.5" fill="#F2C25C"/>')
for i in range(8): comp("PJ301M", col(i), L["Y_IN1"]); comp("PJ301M", col(i), L["Y_IN2"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08): comp("ScrewSilver", x + 2.54, y + 2.54)
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
