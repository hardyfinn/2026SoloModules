#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Pivot.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 152.4, 128.5
PX = 25.4 / 75.0
g = []
colx = lambda i: L["COL0"] + L["COLDX"] * i


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


def bezel(x, y, glow=None, a=0.0):
    comp("VCVBezel", x, y)
    if glow and a:
        g.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="{glow}" fill-opacity="{a}"/>')


OLD = [8, 9, 3, 9, 6, 5, 2, 7]
NEW = [9, 8, 6, 7, 5, 6, 0, 9]
bezel(L["GUT_X"], L["BTN_Y"], "#7DE2C3", 0.0)
for i in range(8):
    bezel(colx(i), L["BTN_Y"], "#FFC94D", 1.0 if i == 0 else (0.12 if i < 4 else 0.0))
    knob("RoundBlackKnob", colx(i), L["OLD_Y"], OLD[i] / 12)
    knob("RoundBlackKnob", colx(i), L["NEW_Y"], NEW[i] / 12)
for i, v in enumerate([90 / 270, 2 / 10, 5 / 6, 2 / 6, 3 / 7, 0.0, 0.0]):
    knob("RoundSmallBlackKnob", colx(i), L["ROWA_Y"], v)
bezel(colx(7), L["ROWA_Y"])
comp("CKSSThree_2", colx(0) - 3, L["ROWB_Y"]); comp("CKSSThree_0", colx(1) - 3, L["ROWB_Y"]); comp("CKSS_1", colx(2) - 3, L["ROWB_Y"])
for i in range(5):
    comp("PJ301M", colx(3 + i), L["ROWB_Y"])
for i in range(8):
    comp("PJ301M", colx(i), L["ROWC_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen, same layout as PivotDisplay
dx, dy, dw = L["DISP_X"], L["DISP_Y"], L["DISP_W"]
mono = "font-family:'DejaVu Sans Mono',monospace"
AM, MI, SO = "#FFB341", "#7DE2C3", "#B8BCC8"
def t(x, y, s, size, col, anchor="start"):
    g.append(f'<text x="{dx + x}" y="{dy + y}" font-size="{size}" fill="{col}" text-anchor="{anchor}" style="{mono}">{s}</text>')
t(1.6, 6.6, "160.0", 6.0, AM)
t(20.0, 6.6, "BPM  x1.333 of home 120.0", 2.5, SO)
t(dw - 1.6, 6.4, "STEP 1 OF 4", 2.9, AM, "end")
t(1.6, 10.0, "LIVE", 2.1, MI); t(1.6, 13.4, "PIVOT", 2.1, AM)
lx0, lx1, span, Hn = 12.0, dw - 1.6, 8.0, 10.3
xo = lambda h: dx + lx0 + (h - (Hn - span)) / span * (lx1 - lx0)
for b in range(3, 11):
    bar = b % 4 == 0
    g.append(f'<rect x="{xo(b) - 0.25}" y="{dy + (7.9 if bar else 8.7)}" width="0.5" height="{2.4 if bar else 1.6}" fill="{MI}" fill-opacity="{1 if bar else 0.6}"/>')
k = 0
h = 8.0
while h <= Hn:
    if h >= Hn - span:
        bar = k % 4 == 0
        g.append(f'<rect x="{xo(h) - 0.25}" y="{dy + (11.3 if bar else 12.1)}" width="0.5" height="{2.4 if bar else 1.6}" fill="{AM}" fill-opacity="{1 if bar else 0.6}"/>')
    h += 0.75; k += 1
h = 2.3
while h <= 8.0:
    g.append(f'<rect x="{xo(h) - 0.2}" y="{dy + 12.1}" width="0.5" height="1.6" fill="{AM}" fill-opacity="0.6"/>')
    g.append(f'<rect x="{xo(h + 0.0) - 0.2}" y="{dy + 14.0}" width="0.4" height="0.9" fill="{SO}" fill-opacity="0.45"/>') if False else None
    h += 1.0
NAMES = ["1/16T", "1/16Q", "1/16", "1/8T", "1/16.", "1/8Q", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2"]
FR = [(1, 6), (1, 5), (1, 4), (1, 3), (3, 8), (2, 5), (1, 2), (2, 3), (3, 4), (1, 1), (4, 3), (3, 2), (2, 1)]
from math import gcd
cum = 120.0
for i in range(8):
    n = FR[NEW[i]][0] * FR[OLD[i]][1]; d = FR[NEW[i]][1] * FR[OLD[i]][0]; q = gcd(n, d); n //= q; d //= q
    cum *= n / d
    col = AM if i == 0 else (SO if i < 4 else "#5A606E")
    col2 = AM if i == 0 else (MI if i < 4 else "#5A606E")
    x = colx(i) - dx
    t(x, 17.6, f"{NAMES[OLD[i]]}={NAMES[NEW[i]]}", 2.1, col, "middle")
    t(x, 20.7, f"x{n}/{d} {cum:.0f}" if n != d else f"x1 {cum:.0f}", 2.3, col2, "middle")
t(L["GUT_X"] - dx + 0.4, 17.6, "HOME", 2.1, SO, "middle")
t(L["GUT_X"] - dx + 0.4, 20.7, "120", 2.3, SO, "middle")

svg = svg.replace("</svg>", "\n".join(x for x in g if x) + "\n</svg>")
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
