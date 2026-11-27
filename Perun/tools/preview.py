#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Perun.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 132.08, 128.5
PX = 25.4 / 75.0
g = []


def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')


def knob(x, y, v, name="RoundBlackKnob"):
    if os.path.exists(os.path.join(LIB, name + "_bg.svg")):
        comp(name + "_bg", x, y)
    comp(name, x, y, -135 + 270 * v)


C, R = L["COL"], L["ROW"]
vals = [[0.0, 0.18, 0.15, "sw0", 0.19, 0.45, 0.3],
        [0.56, 0.35, 0.5, 0.35, 0.0, 0.1, 0.0],
        [1.0, 0.1, 0.0, "sw2", 0.35, 0.7, "latch"],
        [0.0, 0.45, 0.3, 0.5, 0.8, "trim", "trim"]]
for r in range(4):
    for c in range(7):
        v = vals[r][c]
        if v == "sw0": comp("CKSSThree_0", C[c], R[r])
        elif v == "sw2": comp("CKSSThree_2", C[c], R[r])
        elif v == "latch": comp("VCVBezel", C[c], R[r])
        elif v == "trim": knob(C[c] + L["TRIM_DX"], R[r], 0.5 + 0.3 * (c - 5), "Trimpot")
        else: knob(C[c], R[r], v)
for c in (5, 6): comp("VCVButton_0", C[c] + L["LEARN_DX"], R[3] + L["LEARN_DY"])
for x in C: comp("PJ301M", x, L["IO_TOP"])
comp("PJ301M", C[2], L["IO_BOT"]); comp("VCVBezel", C[3], L["IO_BOT"])
for x in C[4:]: comp("PJ301M", x, L["IO_BOT"])
for i in range(8):
    x, y = L["MEMX"][i % 4], L["MEMY"][i // 4]
    comp("VCVBezel", x, y)
    if i == 0: g.append(f'<circle cx="{x}" cy="{y}" r="2.3" fill="#ffcc00" fill-opacity="0.8"/>')
comp("VCVBezel", L["STORE_X"], L["STORE_Y"])
sx, sy, sw, sh = L["SCREEN"]
g.append(f'<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="1" fill="#0c1a12" stroke="#33363a" stroke-width="0.3"/>')
for i, t in enumerate(["A 01 FOLD BD &gt; 15% &gt; B 03 RESONANT BD", "MODE A sine | P1 fold 35 | P2 sym 50", "MEM 1 Thunder Kick | A&gt;CUTOFF"]):
    g.append(f'<text x="{sx + 1.6}" y="{sy + 4.4 + i * 5.1}" font-family="monospace" font-size="2.6" fill="#66ff77">{t}</text>')
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
