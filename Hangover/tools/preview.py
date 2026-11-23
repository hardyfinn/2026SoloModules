#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Hangover.svg")).read()
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

vals = [[0.0, 2/11, 0.7], [0.7, 0.3, 0.3], [0.0, 0.5, 0.5]]
for r in range(3):
    for c in range(3):
        knob("RoundBlackKnob", L["MX"][c], L["MY"][r], vals[r][c])
knob("RoundLargeBlackKnob", L["DIRT_MX"], L["DIRT_Y"], 0.5)
for i, v in enumerate([0.3, 0.3, 0.2, 0.3, 0.3, 0.3, 0.2, 0.25]):
    knob("RoundBlackKnob", L["DX0"] + L["DDX"] * i, L["DIRT_Y"], v)
comp("VCVButton_0", L["NEW_X"], L["DIRT_Y"])
for i in range(10): comp("PJ301M", L["TRIG_X0"] + L["TRIG_DX"] * i, L["ROWA_Y"])
for i in range(5): comp("PJ301M", L["POLY_X0"] + L["POLY_DX"] * i, L["ROWA_Y"])
for i in range(6): comp("PJ301M", L["CV_X0"] + L["CV_DX"] * i, L["ROWB_Y"])
for x in L["OUT_X"]: comp("PJ301M", x, L["ROWB_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen (a sketch of what the module draws)
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
cx, cy, R = dx + 28, dy + dh / 2, 22.5
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<defs><radialGradient id="sh" cx="0.35" cy="0.32" r="0.8"><stop offset="0" stop-color="#5C4634"/><stop offset="1" stop-color="#261B13"/></radialGradient>'
         f'<radialGradient id="gl"><stop offset="0" stop-color="#FFA854" stop-opacity="0.95"/><stop offset="1" stop-color="#FF8033" stop-opacity="0"/></radialGradient></defs>')
g.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#sh)" stroke="#E39A55" stroke-opacity="0.45" stroke-width="0.35"/>')
notes = ["D3", "A3", "Bb3", "C4", "D4", "E4", "F4", "G4", "A4"]
glow = {0: 0.8, 4: 0.45, 6: 0.2}
for i, nm in enumerate(notes):
    if i == 0: x, y, rr = cx, cy, 6.4
    else:
        k = i - 1; side = 1 if k % 2 == 1 else -1; steps = (k + 1) // 2
        a = math.pi / 2 + side * steps * 2 * math.pi / 8
        x, y = cx + 0.7 * R * math.cos(a), cy + 0.7 * R * math.sin(a); rr = 4.6 * (1 - 0.12 * k / 8)
    g.append(f'<ellipse cx="{x:.2f}" cy="{y:.2f}" rx="{rr}" ry="{rr * (1 if i == 0 else 0.82)}" fill="#140D09" fill-opacity="0.8"/>')
    if i in glow:
        g.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{rr * 1.3}" fill="url(#gl)" opacity="{glow[i]}"/>')
    g.append(f'<ellipse cx="{x:.2f}" cy="{y:.2f}" rx="{rr}" ry="{rr * (1 if i == 0 else 0.82)}" fill="none" stroke="#C9B49A" stroke-opacity="0.35" stroke-width="0.2"/>')
    col = "#1E120A" if glow.get(i, 0) > 0.5 else "#F3E6CF"
    g.append(f'<text x="{x:.2f}" y="{y + 0.9:.2f}" font-size="{2.9 if i == 0 else 2.5}" text-anchor="middle" fill="{col}" style="{mono}">{nm}</text>')
g.append(f'<text x="{dx + 1.4}" y="{dy + dh - 1.4}" font-size="2.3" fill="#E39A55" style="{mono}">gu 76 Hz</text>')
x0 = dx + 55; right = dx + dw - 2
g.append(f'<text x="{x0}" y="{dy + 5.6}" font-size="4" fill="#F3E6CF" style="{mono}">D KURD</text>')
g.append(f'<text x="{x0}" y="{dy + 9.6}" font-size="2.4" fill="#C9B49A" style="{mono}">D3 | A3 Bb3 C4 D4 E4 F4 G4 A4</text>')
g.append(f'<text x="{x0}" y="{dy + 15.4}" font-size="2.5" fill="#C8653A" style="{mono}">DIRT 100%</text>')
for i, (nm, a) in enumerate(zip(["Tuning", "Beat", "Rattle", "Bleed", "Air", "Skin", "Slop", "Room"], [0.3, 0.3, 0.2, 0.3, 0.3, 0.3, 0.2, 0.25])):
    y = dy + 19.6 + 3.55 * i; bx = x0 + 13
    g.append(f'<text x="{x0}" y="{y}" font-size="2.3" fill="#C9B49A" style="{mono}">{nm}</text>')
    g.append(f'<rect x="{bx}" y="{y - 1.3}" width="{right - bx}" height="1.3" fill="#fff" fill-opacity="0.07"/>')
    g.append(f'<rect x="{bx}" y="{y - 1.3}" width="{(right - bx) * a}" height="1.3" fill="#C8653A"/>')
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
