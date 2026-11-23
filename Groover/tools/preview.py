#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Groover.svg")).read()
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

for (y, vals) in ((L["ROW1_Y"], [0.69, 0.77, 1.0]), (L["ROW2_Y"], [1.0, 0.1, 0.0])):
    for i in range(3):
        knob("RoundBlackKnob", L["KX"][i], y, vals[i]); comp("PJ301M", L["CVX"][i], y)
for x in (L["CLK_X"], L["RST_X"], L["OCLK_X"], L["OACC_X"], L["OLOOP_X"]): comp("PJ301M", x, L["IO_Y"])
comp("VCVButton_0", L["LOAD_X"], L["IO_Y"])
for i in range(8):
    x = L["LANE_X0"] + L["LANE_DX"] * i
    comp("PJ301M", x, L["LIN_Y"]); comp("PJ301M", x, L["LOUT_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen: the "Wonky" groove, step 5 playing
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
off = [0.0, 0.24, -0.06, 0.31, 0.03, 0.19, -0.11, 0.27, 0.0, 0.22, -0.04, 0.34, 0.06, 0.15, -0.09, 0.29]
vv = [112, 64, 88, 58, 104, 70, 96, 52, 110, 60, 84, 66, 100, 74, 92, 57]
pad = 4 * PX; top = dy + 6.2; bot = dy + dh - 2.2; mid = top + (bot - top) * 0.42
cw = (dw - 2 * pad) / 16
xo = lambda s: dx + pad + cw * (s + 0.5)
for s in range(16):
    op = 0.43 if s % 16 == 0 else (0.24 if s % 4 == 0 else 0.09)
    g.append(f'<line x1="{xo(s)}" y1="{top}" x2="{xo(s)}" y2="{bot}" stroke="#9FB3B5" stroke-opacity="{op}" stroke-width="0.2"/>')
for s in range(16):
    now = s == 5
    a = vv[s] / 112
    bh = (bot - mid - 1.4) * a
    g.append(f'<rect x="{xo(s) - cw*0.28}" y="{bot - bh}" width="{cw*0.56}" height="{bh}" fill="{"#FF9F1C" if now else "#9FB3B5"}" fill-opacity="{0.78 if now else 0.35}"/>')
    r = min(cw * 0.3, 3.2 * PX)
    g.append(f'<circle cx="{xo(s)}" cy="{mid}" r="{r}" fill="none" stroke="#9FB3B5" stroke-opacity="0.35" stroke-width="0.2"/>')
    col = "#FF9F1C" if now else ("#FFBE6E" if abs(off[s]) > 0.01 else "#EFE8D8")
    g.append(f'<circle cx="{xo(s + off[s])}" cy="{mid}" r="{r}" fill="{col}"/>')
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + pad}" y="{dy + 4.4}" font-size="3.0" fill="#EFE8D8" style="{mono}">10/15 Wonky</text>')
g.append(f'<text x="{dx + dw - pad}" y="{dy + 4.4}" font-size="3.0" text-anchor="end" fill="#9FB3B5" style="{mono}">1/16 4b 124 BPM</text>')

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
