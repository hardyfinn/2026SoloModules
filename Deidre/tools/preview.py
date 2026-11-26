#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics. The screen is drawn live by the
# module, so the preview pastes in a still of it (screen.png) if one is given.
#   python3 tools/preview.py out.svg [screen.png] [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.svg")
SCREEN = sys.argv[2] if len(sys.argv) > 2 else None
LIB = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Deidre.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("fonts = {}")[0].split("\n", 1)[1], L)
W, H = 284.48, 128.5
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

def bezel(x, y, glow=None, a=0.0):
    comp("VCVBezel", x, y)
    if glow and a:
        g.append(f'<circle cx="{x}" cy="{y}" r="2.7" fill="{glow}" fill-opacity="{a}"/>')

def light(x, y, col, a, r=0.75):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#333"/>')
    if a:
        g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}" fill-opacity="{a}"/>')

if SCREEN:
    data = base64.b64encode(open(SCREEN, "rb").read()).decode()
    g.append(f'<image href="data:image/png;base64,{data}" x="{L["SX"]}" y="{L["SY"]}" width="{L["SW"]}" height="{L["SH"]}" preserveAspectRatio="none"/>')

knob("RoundBigBlackKnob", L["BPM_X"], L["BPM_Y"], 0.35)
bezel(L["RUN_X"], L["RUN_Y"], "#4CAF50", 0.9)
bezel(L["RUN_X"], L["RESET_Y"])
for x in (L["COL_A"], L["COL_B"], L["COL_C"]):
    bezel(x, L["ROW_BTN"]); bezel(x, L["ROW_BANK"])
for x in (L["COL_A"], L["COL_B"], L["COL_C"]):
    comp("PJ301M", x, L["ROW_IN1"]); comp("PJ301M", x, L["ROW_IN2"])
for k in range(7):
    bezel(L["PAGE_X0"] + k * L["PAGE_DX"], L["PAGE_Y"], "#FFFFFF", 0.9 if k == 0 else 0)
# DATA encoder (drawn by the module)
dx, dy = L["DATA_X"], L["DATA_Y"]
g.append(f'<circle cx="{dx}" cy="{dy}" r="5.2" fill="#9C958A"/><circle cx="{dx}" cy="{dy}" r="4.7" fill="#2A2826"/><circle cx="{dx}" cy="{dy}" r="3.2" fill="#3A3734"/>'
         f'<line x1="{dx}" y1="{dy}" x2="{dx + 2.4}" y2="{dy - 1.6}" stroke="#F4EFE6" stroke-width="0.6" stroke-linecap="round"/>')
vals = [[0.6, 0.0, 1.0, 0.0, 0.3], [0.15, 0.4, 0.0, 1.0, 0.0], [0.13, 0.05, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
for r, row in enumerate(vals):
    for c, v in enumerate(row):
        knob("RoundBlackKnob", L["KCX0"] + c * L["KPITCH"], L["BY0"] + r * L["BH"] + 6.6, v)
bezel(L["KCX0"] + 3 * L["KPITCH"], L["BY0"] + 4 * L["BH"] + 6.6)
LANE = ["#E4572E", "#F3A712", "#C9B200", "#4CAF50", "#17BEBB", "#2E86DE", "#8E5CD9", "#D9489B"]
for i in range(8):
    c = L["outC"](i)
    bezel(c + 5, L["OY"] + 9.2, LANE[i], 1.0 if i == 2 else 0.18)
    comp("VCVButton_0", c + 5, L["OY"] + 16.8)
    light(c + 5, L["OY"] + 16.8, "#FF3030", 0.0, 1.3)
    comp("PJ301M", c + 16.5, L["OY"] + 10.5)
    light(c + 21.6, L["OY"] + 5.4, "#FFFFFF", 0.7 if i in (0, 3, 4) else 0.1)
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

out = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
open(OUT, "w").write(out)
print("wrote", OUT)
