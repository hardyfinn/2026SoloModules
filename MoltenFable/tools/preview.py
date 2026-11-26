#!/usr/bin/env python3
# Picture of the panel at the default settings. With Rack's ComponentLibrary folder as the second
# argument it uses Rack's own graphics; without it, plain stand-ins of the right size.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else None
svg = open(os.path.join(HERE, "..", "res", "Molten.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = L["W"] if "W" in L else 223.52, 128.5
PX = 25.4 / 75.0
g = []
SIZES = {"RoundHugeBlackKnob": 9.5, "RoundBlackKnob": 4.8, "RoundSmallBlackKnob": 4.0, "PJ301M": 3.1, "ScrewSilver": 2.4}


def comp(name, x, y, angle=0.0):
    if LIB:
        s = open(os.path.join(LIB, name + ".svg")).read()
        m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
        w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
        data = base64.b64encode(s.encode()).decode()
        g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')
        return
    r = SIZES.get(name, 3.0)
    if name == "PJ301M":
        g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#9aa0a6"/><circle cx="{x}" cy="{y}" r="{r*0.55}" fill="#111"/>')
    elif name == "ScrewSilver":
        g.append(f'<circle cx="{x}" cy="{y}" r="{r*0.7}" fill="#b8bcc0"/>')
    elif name == "CKSS":
        g.append(f'<rect x="{x-1.8}" y="{y-3.3}" width="3.6" height="6.6" rx="0.6" fill="#444"/><rect x="{x-1.4}" y="{y-2.9}" width="2.8" height="2.8" fill="#ddd"/>')
    elif name == "Bezel":
        g.append(f'<circle cx="{x}" cy="{y}" r="4.6" fill="#555"/><circle cx="{x}" cy="{y}" r="3.4" fill="#3a6fb0"/>')
    else:
        a = math.radians(angle - 90)
        g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#1a1a1a" stroke="#3a3a3a" stroke-width="0.4"/>'
                 f'<line x1="{x}" y1="{y}" x2="{x + 0.8*r*math.cos(a)}" y2="{y + 0.8*r*math.sin(a)}" stroke="#eee" stroke-width="0.5"/>')


def knob(base, x, y, v):
    comp(base, x, y, -135 + 270 * v)


A, S, J = L["COLA"], L["COLS"], L["COLJ"]
Y1, Y2, Y3, Y4, Y5 = L["Y1"], L["Y2"], L["Y3"], L["Y4"], L["Y5"]
BX, R = L["BIG_X"], L["RING_R"]
def pos(p, lo, hi): return (p - lo) / (hi - lo)
# zone strip as if half melted
cw = (W - 2 * L["DISP_X0"]) / 16
cols = ["#3c1612", "#5a1a12", "#8e1e0f", "#c0350f", "#e2581a", "#f69a33", "#fbd27a", "#fff3d8", "#fff3d8", "#fbd27a", "#f69a33", "#c0350f", "#8e1e0f", "#5a1a12", "#3c1612", "#2a1a16"]
for k in range(16):
    x = L["DISP_X0"] + k * cw
    g.append(f'<rect x="{x+0.3}" y="{L["DISP_Y"]+0.3}" width="{cw-0.6}" height="{L["DISP_H"]-0.6}" fill="{cols[k]}"/>')
    if 6 <= k <= 9:
        g.append(f'<rect x="{x+0.3}" y="{L["DISP_Y"]+0.3}" width="{cw-0.6}" height="{L["DISP_H"]-0.6}" fill="none" stroke="#aad2ff" stroke-width="0.4"/>')
g.append(f'<circle cx="{BX}" cy="{Y1}" r="{R}" fill="none" stroke="#f69a33" stroke-opacity="0.8" stroke-width="0.75"/>')
g.append(f'<circle cx="{BX}" cy="{Y2}" r="{R}" fill="none" stroke="#f0f0f0" stroke-opacity="0.75" stroke-width="0.75"/>')
knob("RoundHugeBlackKnob", BX, Y1, 0.0)
knob("RoundHugeBlackKnob", BX, Y2, 0.35)
comp("Bezel", A[0], Y1)
for x, v in zip(A[1:], [0.4, 0.3, 0.3, 1.0, 0.45, 0.0, 0.0, 0.3]): knob("RoundBlackKnob", x, Y1, v)
for x, v in zip(A[:3], [pos(60, 20, 600) ** 0.5, 0.5, 0.5]): knob("RoundBlackKnob", x, Y2, v)
comp("CKSS", A[3], Y2)
for x, v in zip(A[4:], [pos(0.31, 0.05, 0.95), 0.0, 0.0, pos(0, -2, 1), 0.625]): knob("RoundBlackKnob", x, Y2, v)
for x, v in zip(S, [0.67, 0.5, 0, 0.3, 0.5, 0.6, 0.25, 0.2, 0.2, 0, 0.5, 0.5, 0.5, 0.3, 0.7]): knob("RoundSmallBlackKnob", x, Y3, v)
for x in J: comp("PJ301M", x, Y4)
for x in J[:5] + J[6:]: comp("PJ301M", x, Y5)
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 1}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
