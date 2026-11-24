#!/usr/bin/env python3
# Picture of the Rider panel with Rack's own component graphics and a mock display.
#   python3 tools/preview_rider.py out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "rider.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else "/tmp/Rack-src/res/ComponentLibrary"
svg = open(os.path.join(HERE, "..", "res", "Rider.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_rider_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 152.4, 128.5
PX = 25.4 / 75.0
g = []
col = lambda i: L["COL_X0"] + i * L["COL_DX"]
scol = lambda i: L["SND_X0"] + i * L["SND_DX"]
icol = lambda i: L["IN_X0"] + i * L["IN_DX"]

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
txt("SWING", dx + 2, dy + dh * 0.72, 7.5)
txt("BAR 7/12", dx + 36, dy + dh * 0.45, 4.0)
txt("CAUGHT", dx + 36, dy + dh * 0.84, 4.0, "#8A7A55")
txt("LINK", dx + dw - 2, dy + dh * 0.45, 3.4, "#8A7A55", "end"); txt("SEED 1", dx + dw - 2, dy + dh * 0.84, 3.4, "#8A7A55", "end")
for i, v in enumerate([0, .5, .5, 1, .5, .5, .6, 0, .001]):
    knob("RoundLargeBlackKnob" if i == 1 else "RoundBlackKnob", col(i), L["Y_PLAYER"], v)
comp("VCVButton_0", col(9), L["Y_PLAYER"])
knob("RoundBlackKnob", col(0), L["Y_HANDS"], .4); knob("RoundBlackKnob", col(1), L["Y_HANDS"], .5)
for i in range(5): knob("RoundBlackKnob", col(2 + i), L["Y_HANDS"], .5)
for i in range(3): g.append(f'<circle cx="{col(7 + i)}" cy="{L["Y_HANDS"]}" r="1.5" fill="#F2C25C"/>')
for i, v in enumerate([.5, .5, .5, 1, 0, .5]): knob("RoundBlackKnob", scol(i), L["Y_SOUND"], v)
for i in range(7): comp("PJ301M", icol(i), L["Y_IN1"])
for i in range(10): comp("PJ301M", col(i), L["Y_IN2"])
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
