#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Cymbolism.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 203.2, 128.5
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

knob("RoundBigBlackKnob", L["TYPE_X"], L["TYPE_Y"], 1 / 9)
comp("VCVButton_0", L["NEW_X"], L["NEW_Y"])
knob("RoundBigBlackKnob", L["POS_X"], L["BIG_Y"], 0.95)
knob("RoundBigBlackKnob", L["FORCE_X"], L["BIG_Y"], 0.66)
comp("VCVButton_0", L["STRIKE_X"], L["STRIKE_Y"])
comp("VCVButton_0", L["STRIKE_X"], L["CHOKE_Y"])
for i, v in enumerate([0.44, 0.25, 0.47, 0.5, 0.5, 0.44, 0.0, 0.3, 0.5]):
    knob("RoundBlackKnob", L["ROWA_X0"] + L["ROWA_DX"] * i, L["ROWA_Y"], v)
for i, v in enumerate([0.5, 0.5, 0.0, 0.5, 0.0, 0.25, 0.2, 0.5, 0.45, 0.0]):
    knob("RoundBlackKnob", L["ROWB_X0"] + L["ROWB_DX"] * i, L["ROWB_Y"], v)
for i in range(12): comp("PJ301M", L["JACK_X0"] + L["JACK_DX"] * i, L["JACK1_Y"])
for i in range(12): comp("PJ301M", L["JACK_X0"] + L["JACK_DX"] * i, L["JACK2_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen (a sketch of what the module draws)
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
cx, cy, R = dx + 23, dy + dh / 2 + 1.5, 17.5
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append('<defs><radialGradient id="cy" cx="0.38" cy="0.35" r="0.75"><stop offset="0" stop-color="#B08846"/><stop offset="1" stop-color="#462F14"/></radialGradient>'
         '<radialGradient id="bl" cx="0.35" cy="0.35" r="0.7"><stop offset="0" stop-color="#D2AA5F"/><stop offset="1" stop-color="#6E5023"/></radialGradient></defs>')
g.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#cy)"/>')
for i in range(1, 9):
    g.append(f'<circle cx="{cx}" cy="{cy}" r="{R * (0.25 + 0.09 * i):.2f}" fill="none" stroke="#000" stroke-opacity="0.11" stroke-width="0.2"/>')
g.append(f'<circle cx="{cx}" cy="{cy}" r="{R * 0.25:.2f}" fill="url(#bl)"/>')
g.append(f'<circle cx="{cx}" cy="{cy}" r="0.8" fill="#0A0805"/>')
g.append(f'<circle cx="{cx + R * 0.95:.2f}" cy="{cy}" r="1.3" fill="#FFD98A" fill-opacity="0.9"/>')
g.append(f'<rect x="{cx - R * 0.515 - 0.6:.2f}" y="{cy - 1.8:.2f}" width="1.2" height="2.4" fill="#7FA58F"/>')
x0, right = dx + 47, dx + dw - 2
g.append(f'<text x="{x0}" y="{dy + 5.8}" font-size="4.2" fill="#E2C27A" style="{mono}">CRASH</text>')
g.append(f'<text x="{x0}" y="{dy + 10}" font-size="2.6" fill="#F1E7CF" style="{mono}">18.0"  0.85 mm  B20</text>')
g.append(f'<text x="{x0}" y="{dy + 13.8}" font-size="2.6" fill="#B98E52" style="{mono}">SHOULDER  mic BOW</text>')
sy0, sy1 = dy + 17, dy + dh - 6
g.append(f'<rect x="{x0}" y="{sy0}" width="{right - x0}" height="{sy1 - sy0}" fill="#fff" fill-opacity="0.03"/>')
bw = (right - x0) / 64
for b in range(64):
    v = 0.12 + 0.6 * math.exp(-((b - 38) / 16) ** 2) + 0.15 * math.exp(-((b - 12) / 6) ** 2) + 0.05 * math.sin(b * 1.7)
    hgt = max(0.0, min(1.0, v)) * (sy1 - sy0)
    t = b / 64
    col = f"rgb({int(185 + 41 * t)},{int(142 + 52 * t)},{int(82 + 40 * t)})"
    g.append(f'<rect x="{x0 + b * bw + 0.1:.2f}" y="{sy1 - hgt:.2f}" width="{bw - 0.2:.2f}" height="{hgt:.2f}" fill="{col}"/>')
g.append(f'<text x="{x0}" y="{dy + dh - 2.6}" font-size="2" fill="#B7A888" style="{mono}">40 Hz</text>')
g.append(f'<text x="{right}" y="{dy + dh - 2.6}" font-size="2" fill="#B7A888" text-anchor="end" style="{mono}">20 kHz</text>')
g.append(f'<text x="{dx + 1.4}" y="{dy + dh - 1.4}" font-size="2.3" fill="#B7A888" style="{mono}">2215 modes  core 64  cpu 31%</text>')
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
