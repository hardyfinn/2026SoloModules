#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "CopyThat.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 71.12, 128.5
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

knob("RoundHugeBlackKnob", L["TUNE_X"], L["TUNE_Y"], 0.3)
knob("Trimpot", L["TATT_X"], L["TUNE_Y"], 1.0)
knob("RoundLargeBlackKnob", L["L_X"], L["ROW2_Y"], 0.25)
knob("RoundLargeBlackKnob", L["R_X"], L["ROW2_Y"], 0.3)
knob("RoundBlackKnob", L["L_X"], L["ROW3_Y"], 1.0)
knob("RoundBlackKnob", L["R_X"], L["ROW3_Y"], 0.0)
for x in L["SW_X"]: comp("CKSS_0", x, L["SW_Y"] + 1.3)
for x in L["CV_X"]: comp("PJ301M", x, L["CV_Y"])
comp("PJ301M", L["CV_X"][0], L["IO_Y"])
for x in L["OUT_X"]: comp("PJ301M", x, L["IO_Y"] + 0.6)
for i in range(5):
    on = i < 4
    g.append(f'<circle cx="{L["METER_X0"] + L["METER_DX"] * i}" cy="{L["METER_Y"]}" r="0.95" fill="{"#38E05A" if on else "#203A26"}" stroke="#111" stroke-width="0.15"/>')
g.append(f'<circle cx="{L["TATT_X"]}" cy="{L["LOCK_Y"]}" r="0.95" fill="#38E05A" stroke="#111" stroke-width="0.15"/>')
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, H - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 6), int(H * 6)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
