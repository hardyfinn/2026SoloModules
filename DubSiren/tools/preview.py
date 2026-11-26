#!/usr/bin/env python3
# Picture of the panel with its knobs and Rack's own jacks/switches.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "res")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
L = {}
for line in open(os.path.join(HERE, "..", "src", "Layout.hpp")):
    m = re.findall(r"(\w+) = ([0-9.]+)f", line)
    for k, v in m: L[k] = float(v)
svg = open(os.path.join(RES, "DubSiren.svg")).read()
W, H = 121.92, 128.5
PX = 25.4 / 75.0
g = []
def img(path, x, y, angle=0.0):
    s = open(path).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')
def knob(name, key, v):
    img(os.path.join(RES, name + ".svg"), L[f"K_{key}_X"], L[f"K_{key}_Y"], -150 + 300 * v)
knob("knob-red", "SPEED", 0.5); knob("knob-red", "PITCH", 1/3)
knob("knob-yellow", "FEEDBACK", 0.45); knob("knob-yellow", "MODE", 1.0); knob("knob-yellow", "BEAT", 0.0)
knob("knob-green", "VOLUME", 0.8)
for k, v in (("DEPTH", 0.67), ("RELEASE", 0.56), ("TIME", 0.65)): knob("knob-trim", k, v)
img(os.path.join(LIB, "CKSS_1.svg"), L["S_TRIG_X"], L["S_TRIG_Y"]); img(os.path.join(LIB, "CKSS_1.svg"), L["S_ERA_X"], L["S_ERA_Y"])
for k in ("SIREN", "TONE"): img(os.path.join(LIB, "VCVBezel.svg"), L[f"B_{k}_X"], L[f"B_{k}_Y"])
g.append(f'<circle cx="{L["L_ACTIVE_X"]}" cy="{L["L_ACTIVE_Y"]}" r="2.2" fill="#E8302A" stroke="#5a1010" stroke-width="0.3"/>')
for k in ("VOCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN"): img(os.path.join(LIB, "PJ301M.svg"), L[f"I_{k}_X"], L[f"I_{k}_Y"])
img(os.path.join(LIB, "PJ301M.svg"), L["O_OUT_X"], L["O_OUT_Y"])
for x, y in ((7.5, 3), (W - 7.5, 3), (7.5, H - 3), (W - 7.5, H - 3)): img(os.path.join(LIB, "ScrewSilver.svg"), x, y)
full = svg.replace("</svg>", "".join(g) + "</svg>").replace(f'width="{W}mm" height="{H}mm"', f'width="{W*8}" height="{H*8}"')
html = os.path.join(HERE, "_preview.html")
open(html, "w").write(f'<html><body style="margin:0;background:#2b2d31">{full}</body></html>')
js = f"const {{chromium}}=require('playwright');(async()=>{{const b=await chromium.launch();const p=await b.newPage({{viewport:{{width:{int(W*8)},height:{int(H*8)}}}}});await p.goto('file://{html}');await p.waitForTimeout(300);await p.screenshot({{path:'{OUT}'}});await b.close();}})();"
subprocess.run(["node", "-e", js], check=True)
os.remove(html)
print("wrote", OUT)
