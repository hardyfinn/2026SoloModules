#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Cicada.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 101.6, 128.5
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

# Field Cricket (the default)
comp("CKSSThree_1", L["MECH_X"], L["R1_Y"])
knob("RoundLargeBlackKnob", L["R1_X"][0], L["R1_Y"], 0.752)
for x, v in zip(L["R1_X"][1:], [0.885, 0.589]): knob("RoundBlackKnob", x, L["R1_Y"], v)
for x, v in zip(L["R2_X"], [0.0, 0.5, 0.5, 0.4, 0.15]): knob("RoundBlackKnob", x, L["R2_Y"], v)
knob("RoundLargeBlackKnob", L["R3_X"][0], L["R3_Y"], 0.468)
for x, v in zip(L["R3_X"][1:], [0.441, 0.409, 0.0, 1.0]): knob("RoundBlackKnob", x, L["R3_Y"], v)
comp("CKSS_1", L["VSW_X"], L["R3_Y"])
g.append(f'<circle cx="{L["PLIGHT_X"]}" cy="{L["PLIGHT_Y"]}" r="1.5" fill="#f2b33d"/>')
for i in range(8):
    x = L["JX0"] + L["JDX"] * i
    comp("PJ301M", x, L["JA_Y"]); comp("PJ301M", x, L["JB_Y"])
g.append(f'<text x="{L["DISP_X"] + 1.8}" y="{L["DISP_Y"] + 6.6}" font-family="monospace" font-size="5.6" fill="#f2b33d">STEADY</text>')
g.append(f'<text x="{L["DISP_X"] + L["DISP_W"] - 1.6}" y="{L["DISP_Y"] + 5.9}" text-anchor="end" font-family="monospace" font-size="2.6" fill="#efe9d2">FILE 22.5C</text>')
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
