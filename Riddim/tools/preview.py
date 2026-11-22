#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [riddim_index] [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
RI = int(sys.argv[2]) if len(sys.argv) > 2 else 0
LIB = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Riddim.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
pat = open(os.path.join(HERE, "make_patterns.py")).read().split("assert len(R)")[0]
P = {}
exec(pat.replace("os.path", "os.path"), P)
rest = open(os.path.join(HERE, "make_patterns.py")).read().split("assert len(R) == 24")[1].split("out = [")[0]
exec(rest, P)
W, H = 152.4, 128.5
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

C, R1, R2 = L["COLX"], L["ROW1_Y"], L["ROW2_Y"]
knob("RoundLargeBlackKnob", C[0], R1, RI / 23.0)
for x, v in zip(C[1:], [0.0, 0.5, 0.33, 0.5, 0.67]): knob("RoundBlackKnob", x, R1, v)
for x, v in zip(C[:4], [0.25, 0.0, 0.0, 0.1]): knob("RoundBlackKnob", x, R2, v)
comp("CKSSThree_1", C[4] - 3, R2)
comp("VCVBezel", C[5], R2)
g.append(f'<circle cx="{C[5]}" cy="{R2}" r="2.3" fill="#ff3b30" fill-opacity="0.25"/>')
for i in range(10):
    cx = L["VX"][i % 5]; y = L["VA_Y"] if i < 5 else L["VB_Y"]
    knob("Trimpot", cx + L["VKNOB_DX"], y, 0.8)
    comp("PJ301M", cx + L["VJACK_DX"], y)
for i in range(7): comp("PJ301M", L["IO_X0"] + L["IO_DX"] * i, L["IO_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, H - 5.08): comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen
dx, dy = L["DISP_X"], L["DISP_Y"]
name, year, prod, mach, kit, conf, A, B = P["R"][RI]
grid = P["grid"](A)
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + 1.6}" y="{dy + 4.6}" font-size="3.4" fill="#FFC83C" style="{mono}">{RI+1:02d} {name}</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + 9.2}" font-size="3.0" fill="#3CC85A" style="{mono}">{year}  {prod}</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + 13.8}" font-size="3.0" fill="{"#FFC83C" if conf else "#EB463C"}" style="{mono}">{mach}</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + 18.4}" font-size="3.0" fill="#787D6E" style="{mono}">KIT {["CASIO","ROLAND 808","ROLAND PCM","LINN","SIMMONS","SAMPLER","WORKSTATION","LIVE"][kit]}   A</text>')
gx, gy, cw, rh = dx + 64.0, dy + 1.2, 4.5, 1.85
cur = 6
for t in range(10):
    for s in range(16):
        lv = grid[t][s]; cx = gx + s * cw + cw / 2; cy = gy + t * rh + rh / 2
        now = s == cur
        if lv:
            col = "#ffffff" if now else {3: "#EB463C", 2: "#FFC83C", 1: "#A08232"}[lv]
            g.append(f'<rect x="{cx - cw*0.34:.2f}" y="{cy - rh*0.36:.2f}" width="{cw*0.68:.2f}" height="{rh*0.72:.2f}" fill="{col}"/>')
        else:
            g.append(f'<rect x="{cx - cw*0.12:.2f}" y="{cy - rh*0.12:.2f}" width="{cw*0.24:.2f}" height="{rh*0.24:.2f}" fill="#787D6E" fill-opacity="{0.7 if now else 0.35}"/>')
svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 8), int(H * 8)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 1}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
