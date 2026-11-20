#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math, cmath
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Heathrow.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 213.36, 128.5
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

C = L["COLX"]
for x, v in zip(C, [0.15, 0.5, 0.65, 0.3, 0.2]): knob("RoundLargeBlackKnob", x, L["ROW1_Y"], v)
for x, v in zip(C, [0.27, 0.25, 0.0, 0.0, 0.75]): knob("RoundBlackKnob", x, L["ROW2_Y"], v)
for x, v in zip(C[:3], [0.0, 0.5, 0.2]): knob("RoundSmallBlackKnob", x, L["ROW3_Y"], v)
comp("CKSS_0", C[3] - 3, L["ROW3_Y"]); comp("CKSS_0", C[4] - 3, L["ROW3_Y"])
for i in range(8):
    x = L["CV_X0"] + L["CV_DX"] * i
    knob("Trimpot", x, L["TRIM_Y"], 1.0)
    comp("PJ301M", x, L["CVJ_Y"]); comp("PJ301M", x, L["IO_Y"])
BX = L["BX"]
comp("CKSS_1", BX[0] - 2, L["BSW_Y"]); comp("CKSSThree_0", BX[1] - 4, L["BSW_Y"]); comp("CKSSThree_2", BX[2] + 2, L["BSW_Y"])
for x, v in zip(BX, [0.0, 0.4, 0.6, 0.4]): knob("RoundBlackKnob", x, L["BK1_Y"], v)
for x, v in zip(BX[:3], [0.7, 0.18, 0.75]): knob("RoundBlackKnob", x, L["BK2_Y"], v)
for i in range(6):
    x = L["MEMX0"] + L["MEMDX"] * i
    comp("VCVBezel", x, L["MEM_Y"])
    a = [0.2, 1.0, 0.2, 0.0, 0.0, 0.0][i]
    if a: g.append(f'<circle cx="{x}" cy="{L["MEM_Y"]}" r="2.3" fill="#ffffff" fill-opacity="{a}"/>')
comp("VCVBezel", BX[0], L["MEM2_Y"]); knob("RoundSmallBlackKnob", BX[1], L["MEM2_Y"], 0.45)
for x in BX: comp("PJ301M", x, L["CVJ_Y"]); comp("PJ301M", x, L["IO_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen (approximate curves, same maths as the module)
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
pad, top, bot = 1.6, dy + 1.6, dy + dh - 4.6
def resp(hz, s, sections, fb, flanger=False):
    sr = 48000.0; wn = 2 * math.pi * hz / sr; z1 = cmath.exp(-1j * wn)
    if flanger:
        d = 15 * 0.01 ** s * 0.001 * sr; A = cmath.exp(-1j * wn * d)
    else:
        fc = 40 * 400 ** s; gg = math.tan(math.pi * fc / sr); sp = (1 - z1) / (1 + z1) / gg
        A = ((sp * sp - 2 * sp + 1) / (sp * sp + 2 * sp + 1)) ** sections
    Hh = A / (1 - fb * A * z1)
    return 20 * math.log10(abs(0.5 + 0.5 * Hh) + 1e-5)
def curve(s, sections, fb, col, wid, fl=False):
    pts = []
    for i in range(160):
        f = i / 159; hz = 20 * 1000 ** f
        db = max(-36, min(12, resp(hz, s, sections, fb, fl)))
        pts.append(f"{dx + pad + (dw - 2 * pad) * f:.2f},{top + (bot - top) * (12 - db) / 48:.2f}")
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{col}" stroke-width="{wid}" stroke-linejoin="round"/>')
curve(0.4, 3, 0.6, "#FF9A3C", 0.35, True)
curve(0.55, 4, -0.6, "#6FD3FF", 0.45)
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + 1.6}" y="{dy + dh - 1.3}" font-size="2.7" fill="#6FD3FF" style="{mono}">PHASER 8 STG FB -60%  B SERIES</text>')
g.append(f'<text x="{dx + dw - 1.6}" y="{dy + dh - 1.3}" font-size="2.7" text-anchor="end" fill="#FF9A3C" style="{mono}">sine 0.67 Hz 45°</text>')

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
