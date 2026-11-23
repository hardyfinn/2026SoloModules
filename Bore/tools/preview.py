#!/usr/bin/env python3
# Picture of both panels with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "panel-preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
PX = 25.4 / 75.0
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
L = {}
for block in src.split("# ---- LAYOUT")[1:]:
    body = block.split("\n", 1)[1].split("\nP = []")[0]
    exec(body, L)


def inner(path):
    s = open(path).read()
    return re.sub(r"^.*?<svg[^>]*>", "", s, flags=re.S).replace("</svg>", "")


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


def jack(g, x, y): comp(g, "PJ301M", x, y)


# ---------------- Scanner
g = []
C = L["COLX"]
for x, v in zip(C, [0.67, 0.5, 0.5, 0.3, 0.1]): knob(g, "RoundLargeBlackKnob", x, L["ROW1_Y"], v)
for x, v in zip(C, [0.5, 0.0, 0.15, 0.4, 0.0]): knob(g, "RoundBlackKnob", x, L["ROW2_Y"], v)
for x, v in zip(L["X3"][:6], [0.8, 0.8, 0.8, 0.5, 0.0, 0.3]): knob(g, "RoundSmallBlackKnob", x, L["ROW3_Y"], v)
comp(g, "CKSSThree_0", L["X3"][6], L["ROW3_Y"] - 1.0)
knob(g, "Trimpot", L["X3"][7] + 2.0, L["ROW3_Y"], 0.0)
for i in range(9): jack(g, L["JX0"] + L["JDX"] * i, L["IN_Y"])
for i in range(8): jack(g, L["JX0"] + L["JDX"] * i, L["OUT_Y"])
for y in L["RY"]: jack(g, L["RX"], y)
jack(g, L["RX"], L["IN_Y"]); jack(g, L["RX"], L["OUT_Y"])
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
lane = (dh - 9.0) / 3
cols = ["#6FB8FF", "#FFB84A", "#7DE8A6"]
for a in range(3):
    cy = dy + 4.6 + lane * (a + 0.5)
    pts = []
    for i in range(300):
        t = i / 299
        if a == 0:   # EPI readout bursts
            v = (0.6 if (int(t * 900) % 2) else -0.6) if (t % 0.33) < 0.12 else 0.0
        elif a == 1:  # phase blips
            v = 0.15 if (t % 0.33) < 0.12 and int(t * 900) % 2 else (-0.5 if 0.12 < (t % 0.33) < 0.13 else 0.0)
        else:         # slice select + spoilers
            v = 0.4 if (t % 0.33) < 0.012 else (0.8 if 0.125 < (t % 0.33) < 0.135 else 0.0)
        pts.append(f"{dx + 1.4 + (dw - 2.8) * t:.2f},{cy - v * lane * 0.45:.2f}")
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{cols[a]}" stroke-width="0.3"/>')
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + 1.4}" y="{dy + 3.6}" font-size="2.7" fill="#E6EEF8" style="{mono}">EPI  TR 2.00 s  10 SLICES  523 Hz</text>')
g.append(f'<text x="{dx + dw - 1.4}" y="{dy + 3.6}" font-size="2.7" text-anchor="end" fill="#6FB8FF" style="{mono}">3.0 T</text>')
g.append(f'<text x="{dx + 1.4}" y="{dy + dh - 1.1}" font-size="2.7" fill="#9AA8BA" style="{mono}">X  Y  Z gradients</text>')
scanner = inner(os.path.join(HERE, "..", "res", "Scanner.svg")) + "\n".join(g)

# ---------------- Bore Room
g = []
for x, v in zip(L["BX"], [0.5, 0.45]): knob(g, "RoundLargeBlackKnob", x, L["BR1_Y"], v)
for x, v in zip(L["BX"], [0.35, 0.0]): knob(g, "RoundBlackKnob", x, L["BR2_Y"], v)
for x, v in zip(L["BX"], [0.3, 0.6]): knob(g, "RoundBlackKnob", x, L["BR3_Y"], v)
comp(g, "CKSSThree_0", 30.0, L["EARS_Y"] + 1.0)
for x in L["BJX"]: jack(g, x, L["BCV_Y"]); jack(g, x, L["BIO_Y"])
room = inner(os.path.join(HERE, "..", "res", "BoreRoom.svg")) + "\n".join(g)

W1, W2, H, gap = 182.88, 60.96, 128.5, 6.0
TW = W1 + gap + W2
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{TW}mm" height="{H}mm" viewBox="0 0 {TW} {H}">'
       f'<rect width="{TW}" height="{H}" fill="#2A2D33"/>'
       f'<g>{scanner}</g><g transform="translate({W1 + gap},0)">{room}</g></svg>')
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(TW * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#2A2D33"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
