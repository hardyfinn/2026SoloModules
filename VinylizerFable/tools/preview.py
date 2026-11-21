#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "Rack-src", "res", "ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "VinylizerFable.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 254.0, 128.5
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

cx = lambda i: L["COLX0"] + L["COLDX"] * i
KY = [L["K1_Y"], L["K2_Y"], L["K3_Y"]]
knobs = {0: [0.42, 1, None], 1: [0.67, 0.33, 0.15], 2: [0.57, 0.76, 0.5], 3: [0.3, 0, None], 4: [0.5, None, None],
         5: [0.41, 0.5, None], 6: [0.27, 0.5, 0.5], 7: [0.33, 0.5, 0], 8: [0.15, 0.2, 0.2], 9: [0.2, 0.5, None]}
sw3 = {0: 1, 3: 0, 4: 0, 6: 0, 8: 0, 9: 1}
for i in range(10):
    x = cx(i)
    if i in sw3: comp(f"CKSSThree_{sw3[i]}", x - 4, L["SW_Y"])
    for k, v in enumerate(knobs[i]):
        if v is not None: knob("RoundBlackKnob", x, KY[k], v)
    comp("VCVButton_0", x, L["ON_Y"])
    g.append(f'<circle cx="{x}" cy="{L["ON_Y"]}" r="1.6" fill="#ffffff" fill-opacity="0.85"/>')
    comp("PJ301M", x, L["CV_Y"])
comp("CKSS_0", cx(5) - 4, L["SW_Y"]); comp("CKSSThree_2", cx(9) - 4, L["K3_Y"] + 1)
for i, yy in ((1, L["SW_Y"] + 1), (2, L["SW_Y"] + 1)):
    g.append(f'<circle cx="{cx(i)}" cy="{yy}" r="1.5" fill="#3a1010" stroke="#555" stroke-width="0.2"/>')
g.append(f'<circle cx="{cx(5)}" cy="{L["K3_Y"]}" r="1.5" fill="#3a1010" stroke="#555" stroke-width="0.2"/>')
g.append(f'<circle cx="{cx(9) + 6.4}" cy="{L["K3_Y"] - 1.2}" r="1.5" fill="#3a1010" stroke="#555" stroke-width="0.2"/>')
comp("VCVBezel", L["FMT_X"], L["SW2_Y"]); comp("CKSSThree_2", L["DISC_X"], L["SW2_Y"]); comp("CKSS_0", L["SPEED_X"], L["SW2_Y"])
knob("RoundHugeBlackKnob", L["NEEDLE_X"], L["NEEDLE_Y"], 0.35)
comp("VCVBezel", L["PLAY_X"], L["PLAY_Y"])
g.append(f'<circle cx="{L["PLAY_X"]}" cy="{L["PLAY_Y"]}" r="2.4" fill="#40ff60" fill-opacity="0.7"/>')
knob("RoundBlackKnob", L["MIX_X"], L["MIXOUT_Y"], 1.0); knob("RoundBlackKnob", L["OUT_X"], L["MIXOUT_Y"], 0.5)
comp("PJ301M", L["NCV_X"], L["JR1_Y"]); comp("PJ301M", L["PTRIG_X"], L["JR1_Y"])
for x in L["IOX"]: comp("PJ301M", x, L["IO_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
ccx, ccy, R = dx + dh / 2 + 0.7, dy + dh / 2, dh / 2 - 1.0
sc = R / 0.150
g.append(f'<circle cx="{ccx}" cy="{ccy}" r="{0.146 * sc}" fill="#242228"/>')
for k in range(8):
    rr = 0.07 + k * (0.146 - 0.07) / 7
    g.append(f'<circle cx="{ccx}" cy="{ccy}" r="{rr * sc}" fill="none" stroke="#908A9A" stroke-opacity="0.3" stroke-width="0.17"/>')
g.append(f'<circle cx="{ccx}" cy="{ccy}" r="{0.051 * sc}" fill="#D98B4A"/>')
sx, sy = ccx + 0.115 * sc * 0.7071, ccy - 0.115 * sc * 0.7071
g.append(f'<line x1="{dx + dw * 0.47}" y1="{dy + 0.7}" x2="{sx}" y2="{sy}" stroke="#F2EADA" stroke-width="0.34"/>')
g.append(f'<circle cx="{sx}" cy="{sy}" r="0.55" fill="#F2EADA"/>')
mono = "font-family:'DejaVu Sans Mono',monospace"
tx = dx + dh + 1.4
lines = [("LP 33", 3.7, "#D98B4A", 3.8), ("r 115 mm  PLAY", 2.9, "#F2EADA", 7.5), ("groove 0.40 m/s", 2.9, "#F2EADA", 10.5),
         ("HF  -2.1  EXC  0.0", 2.9, "#F2EADA", 13.5), ("track -0.4 deg", 2.9, "#F2EADA", 16.5), ("pitch 0.129 mm", 2.9, "#F2EADA", 19.5),
         ("side cut -2.9 dB", 2.5, "#bdb6aa", 22.6)]
for t, fs, col, yy in lines:
    g.append(f'<text x="{tx}" y="{dy + yy}" font-size="{fs}" fill="{col}" style="{mono}">{t}</text>')

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
