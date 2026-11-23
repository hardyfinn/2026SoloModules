#!/usr/bin/env python3
# Picture of both panels side by side, with Rack's own component graphics and
# a sample screen (pi, 16 bits).
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
RES = os.path.join(HERE, "..", "res")
L = {}
src = open(os.path.join(RES, "make_panel.py")).read()
exec(src.split("# ---- LAYOUT: Datamatic")[1].split("fonts = {}")[0], L)
W, H = 101.6, 128.5
GAP = 0.0
PX = 25.4 / 75.0


def panel(name, ox, comps):
    s = open(os.path.join(RES, name + ".svg")).read()
    inner = s.split(">", 1)[1].rsplit("</svg>", 1)[0]
    return f'<g transform="translate({ox} 0)">{inner}{"".join(comps)}</g>'


def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')


def knob(g, base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(g, base + "_bg", x, y)
    comp(g, base, x, y, -135 + 270 * v)


def screws(g, name):
    for x in (5.08, W - 2 * 5.08):
        for y in (0.0, 128.5 - 5.08):
            comp(g, name, x + 2.54, y + 2.54)


# ---- Datamatic
g = []
for x, v in zip(L["KX"], [3 / 7, 0.0, 12 / 60, 0.25]): knob(g, "RoundLargeBlackKnob", x, L["ROW1_Y"], v)
for x, v in zip(L["KX"], [0.3, 0.25, 0.15, 0.45]): knob(g, "RoundBlackKnob", x, L["ROW2_Y"], v)
for i in range(6): comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["JA_Y"])
for i in range(6): comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["JB_Y"])
for i in range(6): comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["JC_Y"])
screws(g, "ScrewSilver")
# sample screen: pi, 16 bits, lanes read in their own orders
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
bits = "".join(format(int(d), "04b") for d in "3141")
Lb = 16
def lb(i): return bits[i % Lb] == "1"
lanes = [
    [lb(j) for j in range(Lb)],
    [lb(Lb - 1 - j) for j in range(Lb)],
    [lb(j * 3 + 1) for j in range(Lb)],
    [lb(j * 5 + 2) and lb(j * 7 + 3) for j in range(Lb)],
]
pos = [5, 5, 11, 11]
pad, textH = 1.2, 4.2
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + pad}" y="{dy + pad + 2.9}" font-size="3.1" fill="#F2F2F2" style="{mono}">PI  W000  L16  x2</text>')
g.append(f'<text x="{dx + dw - pad}" y="{dy + pad + 2.9}" font-size="3.1" text-anchor="end" fill="#9A9A9A" style="{mono}">01001001</text>')
top = dy + pad + textH
rowH = (dh - pad - textH - pad) / 4
cw = (dw - 2 * pad) / Lb
for v in range(4):
    for j in range(Lb):
        b = lanes[v][j]; here = j == pos[v]
        if not b and not here: continue
        c = 255 if (b and here) else (205 if b else 70)
        g.append(f'<rect x="{dx + pad + j * cw + 0.15:.3f}" y="{top + v * rowH + 0.25:.3f}" width="{cw - 0.3:.3f}" height="{rowH - 0.5:.3f}" fill="rgb({c},{c},{c})"/>')
left = panel("Datamatic", 0, g)

# ---- Test Pattern
g = []
vals = [[0.4, 0.0, 1.0, 0.8], [0.5, 0.15, 0.0, 0.45], [0.0, 0.0, 0.7, 0.8], [0.35, 0.3, 1.0, 0.35]]
for x, vs in zip(L["VX"], vals):
    for y, v in zip((L["K1_Y"], L["K2_Y"], L["K3_Y"]), vs[:3]): knob(g, "RoundBlackKnob", x, y, v)
    knob(g, "RoundSmallBlackKnob", x, L["LEV_Y"], vs[3])
    comp(g, "PJ301M", x, L["GATE_Y"]); comp(g, "PJ301M", x, L["OUT_Y"])
    g.append(f'<circle cx="{x + 10.6}" cy="{L["HEAD_Y"] - 1.2}" r="0.9" fill="#bbbbbb" stroke="#333" stroke-width="0.15"/>')
comp(g, "CKSSThreeHorizontal_2", L["VX"][1], L["ZONE_Y"])
for i in range(6): comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["IO_Y"])
screws(g, "ScrewBlack")
right = panel("TestPattern", W + GAP, g)

TW = 2 * W + GAP
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{TW}mm" height="{H}mm" viewBox="0 0 {TW} {H}">'
       f'{left}{right}</svg>')
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(TW * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
