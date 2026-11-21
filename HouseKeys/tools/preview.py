#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample display.
#   python3 tools/preview.py out.png path/to/Rack/res/ComponentLibrary
# (the component SVGs are in github.com/VCVRack/Rack, res/ComponentLibrary)
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "panel-preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "HouseKeys.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec("LAYOUT = dict(" + src.split("LAYOUT = dict(")[1].split("\n)\n")[0] + "\n)", L)
L = L["LAYOUT"]
W, H = 132.08, 128.5
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

knob("RoundHugeBlackKnob", L["SOUND_X"], L["SOUND_Y"], 0.0)
comp("VCVButton_0", L["PREV_X"], L["PN_Y"]); comp("VCVButton_0", L["NEXT_X"], L["PN_Y"])
for i in range(5):
    x = L["ENG_X0"] + L["ENG_DX"] * i
    comp("VCVBezel", x, L["ENG_Y"])
    if i == 0: g.append(f'<circle cx="{x}" cy="{L["ENG_Y"]}" r="2.3" fill="#ffd23a" fill-opacity="0.9"/>')
P, S, M1, M2 = L["COL_PLAY"], L["COL_SHAPE"], L["COL_M1"], L["COL_M2"]
R = [L["ROW1"], L["ROW2"], L["ROW3"], L["ROW4"]]
for y, v in zip(R, [0.5, 0.0, 0.5, 0.0]): knob("RoundBlackKnob", P, y, v)
for y, v in zip(R[:3], [0.5, 0.5, 0.5]): knob("RoundBlackKnob", S, y, v)
knob("RoundBlackKnob", M1, R[0], 0.0); knob("RoundBlackKnob", M2, R[0], 0.0)
knob("RoundBlackKnob", M1, R[1], 0.5); knob("RoundBlackKnob", M2, R[1], 0.0)
knob("RoundBlackKnob", (M1 + M2) / 2, R[2], 0.5)
for i in range(10):
    comp("PJ301M", L["JACK_X0"] + L["JACK_DX"] * i, L["JACK_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)
dx, dy, dh = L["DISP_X"], L["DISP_Y"], L["DISP_H"]
g.append(f'<text x="{dx + 1.6}" y="{dy + 6.2}" font-family="Share Tech Mono, DejaVu Sans Mono, monospace" font-size="4.0" fill="#FFB04A">001 Solid Bass</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + dh - 2.2}" font-family="Share Tech Mono, DejaVu Sans Mono, monospace" font-size="3.0" fill="#9A8660">4-OPERATOR FM</text>')

out = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"
open(tmp, "w").write(out)
html = f'<html><body style="margin:0;background:#888"><img src="file://{tmp}" style="width:{W*6}px;height:{H*6}px"></body></html>'
open(OUT + ".html", "w").write(html)
js = f"""
const {{ chromium }} = require('playwright');
(async () => {{
  const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {int(W*6)}, height: {int(H*6)}}}}});
  await p.goto('file://{OUT}.html'); await p.waitForTimeout(500);
  await p.screenshot({{path: '{OUT}'}}); await b.close();
}})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True)
for f in (tmp, OUT + ".html", OUT + ".js"): os.remove(f)
print("wrote", OUT)
