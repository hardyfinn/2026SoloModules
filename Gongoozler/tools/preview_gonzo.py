#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Gonzo.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel_gonzo.py")).read()
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

C = L["COL"]
for x, v in zip(C, [0.0, 0.55, 0.45, 0.67, 0.5, 1.0, 0.13, 0.0]): knob(x, L["ROW_GONG"], v)
for x, v in zip(C[:7], [0.4, 0.6, 0.0, 0.55, 0.67, 0.5, 0.18]): knob(x, L["ROW_STRIKE"], v)
comp("VCVBezel", C[7], L["ROW_STRIKE"])
g.append(f'<circle cx="{C[7]}" cy="{L["ROW_STRIKE"]}" r="2.3" fill="#ffd84a" fill-opacity="0.8"/>')
for x, v in zip(C[:7], [0.85, 0.52, 0.5, 0.5, 0.0, 0.7, 1.0]): knob(x, L["ROW_CASC"], v)
comp("CKSS_0", C[7], L["ROW_CASC"])
for x in C:
    comp("PJ301M", x, L["ROW_CV"]); comp("PJ301M", x, L["ROW_IO"])
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
