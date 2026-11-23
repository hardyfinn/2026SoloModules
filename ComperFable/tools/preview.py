#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a mock display.
#   python3 tools/preview.py out.png path/to/Rack/res/ComponentLibrary
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else "/tmp/Rack-src/res/ComponentLibrary"
svg = open(os.path.join(HERE, "..", "res", "Comper.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 182.88, 128.5
PX = 25.4 / 75.0
g = []
col = lambda i: L["COL_X0"] + i * L["COL_DX"]
step = lambda i: L["STEP_X0"] + i * L["STEP_DX"]

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

def txt(s, x, y, size, fill="#F2C25C", anchor="start"):
    g.append(f'<text x="{x}" y="{y}" font-family="monospace" font-size="{size}" fill="{fill}" text-anchor="{anchor}">{s}</text>')

for x, v in zip(L["SMALLK_X"], [3/15, 2/3, 0.55]): knob("RoundSmallBlackKnob", x, L["SMALLK_Y"], v)
chart = [("D", "m7", 2/11, 2/9), ("G", "7", 7/11, 5/9), ("C", "maj7", 0, 0), ("C", "maj7", 0, 0)]
for i in range(16):
    r, q, rv, qv = chart[i % 4]
    knob("Trimpot", step(i), L["Y_ROOT"], rv); knob("Trimpot", step(i), L["Y_QUAL"], qv)
    g.append(f'<circle cx="{step(i)}" cy="{L["Y_STEPLIGHT"]}" r="0.9" fill="{"#F2C25C" if i == 1 else "#3a3220" if i < 4 else "#222"}"/>')
    txt(r + q, step(i), L["Y_STEPNAME"] + 0.4, 2.3, "#F2C25C" if i < 4 else "#5a4f37", "middle")
dx, dy = L["DISP_X"], L["DISP_Y"]
txt("G7", dx + 2, dy + 9.3, 7.5); txt("&gt; Cmaj7", dx + 13.5, dy + 9.3, 4.0, "#8A7A55")
txt("CHART", dx + L["DISP_W"] - 2, dy + 4.0, 3.0, "#8A7A55", "end"); txt("SEED 11", dx + L["DISP_W"] - 2, dy + 8.0, 3.0, "#8A7A55", "end")
txt("F3 A3 B3 E4", dx + 2, dy + L["DISP_H"] - 2, 3.4)
rowa = [("RoundBlackKnob", .25), ("RoundBlackKnob", .25), ("RoundBlackKnob", .47), ("RoundBlackKnob", 1), ("RoundBlackKnob", 0),
        ("RoundBlackKnob", .5), ("RoundBlackKnob", .5), ("RoundBlackKnob", .6), ("RoundLargeBlackKnob", .4), ("Trimpot", .5),
        ("Trimpot", .5), ("Trimpot", .5), ("RoundBlackKnob", 0), ("RoundBlackKnob", .36)]
for i, (k, v) in enumerate(rowa): knob(k, col(i), L["Y_ROWA"], v)
comp("VCVButton_0", col(14), L["Y_ROWA"]); g.append(f'<circle cx="{col(14)}" cy="{L["Y_ROWA"]}" r="1.6" fill="#ffffff" opacity="0.8"/>')
knob("RoundBlackKnob", col(0), L["Y_ROWB"], 1); knob("RoundBlackKnob", col(1), L["Y_ROWB"], 0)
comp("CKSSThree_0", col(2), L["Y_ROWB"])
knob("RoundBlackKnob", col(4), L["Y_ROWB"], .85); knob("RoundBlackKnob", col(5), L["Y_ROWB"], .01)
comp("VCVButton_0", col(6), L["Y_ROWB"]); knob("Trimpot", col(8), L["Y_ROWB"], .5); knob("RoundBlackKnob", col(9), L["Y_ROWB"], 0)
for i in (12, 13): g.append(f'<circle cx="{col(i)}" cy="{L["Y_ROWB"]}" r="1.4" fill="#3a3220"/>')
for i in range(10): comp("PJ301M", col(i), L["Y_IN1"])
for i in range(5): comp("PJ301M", col(i), L["Y_IN2"])
for i in range(5): comp("PJ301M", col(L["OUT_COL0"] + i), L["Y_IN1"])
for i in range(4): comp("PJ301M", col(L["OUT_COL0"] + i), L["Y_IN2"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08): comp("ScrewSilver", x + 2.54, y + 2.54)
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
