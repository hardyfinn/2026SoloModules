#!/usr/bin/env python3
# Picture of the two expander panels with Rack's component graphics.
#   python3 tools/preview_expanders.py out.png [ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "expanders.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
PX = 25.4 / 75.0

def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(g, x, y, v):
    comp(g, "RoundBlackKnob_bg", x, y); comp(g, "RoundBlackKnob", x, y, -135 + 270 * v)

def led(g, x, y, col, r=1.1):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}" stroke="#555" stroke-width="0.2"/>')

def common(g, W, sel):
    knob(g, 12, 30, sel / 4.0 * 0.5)
    for i in range(5): led(g, 25, 22 + 4 * i, "#ffd84a" if i == sel else "#3a3210")
    led(g, W - 5.5, 9, "#40ff60")
    comp(g, "ScrewSilver", 5.08 + 2.54, 2.54); comp(g, "ScrewSilver", W - 10.16 + 2.54, 128.5 - 2.54)

def master():
    W = 60.96; g = []; X = [11.0, 30.48, 49.96]
    common(g, W, 1)
    for x, v in zip(X, [0.3, 0.0, 0.5]): knob(g, x, 54, v)
    for x, v in zip(X, [0.3, 0.5, 0.0]): knob(g, x, 72, v)
    led(g, X[2] + 6.5, 67, "#3a1010")
    comp(g, "CKSS_1", X[0] - 4, 91); comp(g, "CKSSThree_0", X[1] - 5, 91); comp(g, "CKSS_0", X[2] - 4, 91)
    comp(g, "VCVButton_0", X[0], 116); led(g, X[0], 116, "#ffffff", 1.6)
    comp(g, "PJ301M", X[1], 116); comp(g, "PJ301M", X[2], 116)
    return "VinylMaster.svg", W, g

def pressing():
    W = 71.12; g = []; X = [12.0, 35.56, 59.12]; JX = [9.0, 22.28, 35.56, 48.84, 62.12]
    common(g, W, 1)
    for x, v in zip(X, [0.65, 0.5, 0.8]): knob(g, x, 54, v)
    for x, v in zip(X, [0.05, 0.2, 0.0]): knob(g, x, 72, v)
    led(g, X[1] + 6.5, 49, "#3a1010"); led(g, X[1] + 6.5, 67, "#3a1010")
    knob(g, X[0], 90, 0.0); comp(g, "VCVBezel", X[1], 90); comp(g, "VCVBezel", X[2], 90)
    for x in JX: comp(g, "PJ301M", x, 105)
    comp(g, "VCVButton_0", JX[0], 118); led(g, JX[0], 118, "#ffffff", 1.6)
    comp(g, "PJ301M", JX[3], 118); comp(g, "PJ301M", JX[4], 118)
    return "VinylPressing.svg", W, g

imgs = []; total = 0
for fn, W, g in (master(), pressing()):
    svg = open(os.path.join(HERE, "..", "res", fn)).read().replace("</svg>", "\n".join(g) + "\n</svg>")
    imgs.append((base64.b64encode(svg.encode()).decode(), W)); total += W
S = 7
html = '<html><body style="margin:0;background:#111;display:flex;gap:14px">' + "".join(
    f'<img src="data:image/svg+xml;base64,{d}" style="width:{int(W*S)}px;height:{int(128.5*S)}px">' for d, W in imgs) + "</body></html>"
tmp = OUT + ".html"; open(tmp, "w").write(html)
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {int(total*S)+14}, height: {int(128.5*S)}}}, deviceScaleFactor: 1}});
await p.goto('file://{tmp}'); await p.waitForTimeout(500);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp)
print("wrote", OUT)
