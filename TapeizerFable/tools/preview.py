#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics, at the default settings.
#   python3 tools/preview.py out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess, json, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Tapeizer.svg")).read()
L = json.load(open(os.path.join(HERE, "layout.json")))
W, H = L["W"], L["H"]
PX = 25.4 / 75.0
g = []
DEF = dict(STRENGTH=0.4, AGE=0.2, MIX=1.0, OUTPUT=0.5, TRIM=0.0, LOW=0.5, LOW_FREQ=0.08, HIGH=0.5, HIGH_FREQ=1.0, CAL=0.5,
           MULTI_MACHINE=0.0, TAPE=0.5, BOUNCE=0.0, NR_TYPE=1.0, REC=0.5, BIAS=0.5, WOW=0.5, FLUTTER=0.5, DROPOUT=0.5, HISS=0.5,
           AZIMUTH=0.5, WEAR=0.0, THRESH=0.67, RATIO=0.13, MASTER_MACHINE=0.0, MASTER_REC=0.5, MASTER_TAPE=0.5)
ON = dict(DESK_ON=1, MULTI_ON=1, BUS_ON=0, MASTER_ON=1, NR_ENC=1, NR_DEC=1, MASTER_NR=1)

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(base + "_bg", x, y)
    comp(base, x, y, -150 + 300 * v)

for name, (kind, x, y, label) in L["comps"].items():
    if kind in ("knob", "snap"): knob("RoundBlackKnob", x, y, DEF.get(name, 0.5))
    elif kind == "big": knob("RoundLargeBlackKnob", x, y, DEF.get(name, 0.5))
    elif kind in ("jack", "out"): comp("PJ301M", x, y)
    elif kind == "bezel":
        comp("VCVBezel", x, y)
        if ON.get(name): g.append(f'<circle cx="{x}" cy="{y}" r="2.4" fill="#d9a441" opacity="0.9"/>')
    elif kind == "light":
        col = "#3c3" if name[3:] in ("3", "4") else "#333"
        g.append(f'<circle cx="{x}" cy="{y}" r="1.0" fill="{col}"/>')
    elif kind == "display":
        txt = "4TR  II  2:1" if name == "MULTI_DISP" else "DECK  II  B"
        g.append(f'<text x="{x}" y="{y + 1.2}" font-family="monospace" font-size="3.6" fill="#f2c060" text-anchor="middle">{txt}</text>')
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, H - 5.08):
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
