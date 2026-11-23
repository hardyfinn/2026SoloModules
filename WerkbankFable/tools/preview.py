#!/usr/bin/env python3
# Picture of a panel with Rack's own component graphics, placed from src/layout.hpp.
#   python3 tools/preview.py Zap out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess, random
HERE = os.path.dirname(os.path.abspath(__file__))
NAME = sys.argv[1]
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else NAME + ".png")
LIB = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
RES = os.path.join(HERE, "..", "res")
svg = open(os.path.join(RES, NAME + ".svg")).read()
W = float(re.search(r'width="([0-9.]+)mm"', svg).group(1))
H = 128.5
PX = 25.4 / 75.0
rnd = random.Random(3)

# read this module's namespace from layout.hpp
lay = open(os.path.join(HERE, "..", "src", "layout.hpp")).read()
block = lay.split("namespace %s {" % NAME)[1].split("\n}")[0]
items = re.findall(r"static const P (\w+) = \{([-0-9.]+)f, ([-0-9.]+)f\};\s+// (\w+)", block)
g = []


def comp(name, x, y, angle=0.0, lib=LIB):
    s = open(os.path.join(lib, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')
    return w, h


def knob(base, x, y):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * rnd.random())


KN = {"big": "RoundLargeBlackKnob", "knob": "RoundBlackKnob", "small": "RoundSmallBlackKnob", "trim": "Trimpot"}
for cid, xs, ys, kind in items:
    x, y = float(xs), float(ys)
    if kind in KN: knob(KN[kind], x, y)
    elif kind in ("jack", "out"): comp("PJ301M", x, y)
    elif kind == "button": comp("VCVButton_0", x, y)
    elif kind == "bezel": comp("VCVBezel", x, y)
    elif kind == "pad": comp("Pad_0", x, y, lib=RES)
    elif kind == "sw2": comp("CKSS_0", x, y)
    elif kind == "sw3": comp("CKSSThree_1", x, y)
    elif kind == "slider":
        comp("VCVSlider", x, y)
        comp("VCVSliderHandle", x, y + (rnd.random() - 0.5) * 19.0)
    elif kind in ("light", "mlight"):
        g.append(f'<circle cx="{x}" cy="{y}" r="{1.0 if kind == "light" else 1.5}" fill="#e8733a"/>')
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
