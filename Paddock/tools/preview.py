#!/usr/bin/env python3
# Pictures of the three panels with Rack's own component graphics, side by side.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "res")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
src = open(os.path.join(RES, "make_panel.py")).read()
L = {}
for block in src.split("# ---- LAYOUT")[1:]:
    body = block.split("\n", 1)[1].split("\n\n")[0]
    exec(body, L)
PX = 25.4 / 75.0

def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(g, base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(g, base + "_bg", x, y)
    comp(g, base, x, y, -135 + 270 * v)

def jack(g, x, y): comp(g, "PJ301M", x, y)

def screws(g, w):
    for x in (5.08, w - 2 * 5.08):
        for y in (0.0, 128.5 - 5.08): comp(g, "ScrewSilver", x + 2.54, y + 2.54)

panels = []
# Swerve
g = []
for x in L["SW_KX"]: knob(g, "RoundLargeBlackKnob", x, L["SW_K1_Y"], 0.45)
knob(g, "RoundBlackKnob", L["SW_KX"][0], L["SW_K2_Y"], 1.0); knob(g, "RoundBlackKnob", L["SW_KX"][1], L["SW_K2_Y"], 0.0)
comp(g, "CKSSThree_2", L["SW_RANGE_X"], L["SW_SW_Y"]); comp(g, "CKSS_1", L["SW_CYCLE_X"], L["SW_SW_Y"])
g.append(f'<circle cx="{L["SW_LIGHT_X"]}" cy="{L["SW_SW_Y"] - 1}" r="1.5" fill="#3f3"/>')
for x, v in zip(L["SW_TX"], [0.5, 1.0, 0.5]): knob(g, "Trimpot", x, L["SW_TRIM_Y"], v); jack(g, x, L["SW_CVJ_Y"])
for x in L["SW_JX"]: jack(g, x, L["SW_IN_Y"]); jack(g, x, L["SW_OUT_Y"])
screws(g, L["SW_W"]); panels.append(("Swerve", L["SW_W"], g))
# Gallop
g = []
for i, y in enumerate(L["GA_ROWS"]):
    g.append(f'<circle cx="{L["GA_NUM_X"]}" cy="{y + 4.4}" r="1.0" fill="{"#3f3" if i % 2 else "#f33"}"/>')
    knob(g, "RoundBlackKnob", L["GA_RATE_X"], y, [0.33, 0.41, 0.37, 0.46][i]); knob(g, "RoundSmallBlackKnob", L["GA_TILT_X"], y, 0.5)
    knob(g, "Trimpot", L["GA_TRIM_X"], y, 1.0)
    for x in (L["GA_CV_X"], L["GA_HI_X"], L["GA_LO_X"], L["GA_TRI_X"], L["GA_SQR_X"]): jack(g, x, y)
comp(g, "CKSSThree_2", L["GA_RANGE_X"] - 2, L["GA_BOT_Y"]); knob(g, "RoundBlackKnob", L["GA_RAIL_X"], L["GA_BOT_Y"], 0.57)
knob(g, "Trimpot", L["GA_FMATT_X"], L["GA_BOT_Y"], 1.0)
for x in (L["GA_FM_X"], L["GA_MIX_X"], L["GA_POLY_X"]): jack(g, x, L["GA_BOT_Y"])
for i in range(4): g.append(f'<circle cx="{66 + 4 * i}" cy="{L["GA_BOT_Y"] - 1}" r="1.0" fill="{"#f33" if i == 2 else "#401010"}"/>')
dx, dy, dw, dh = L["GA_DISP"]
import math
cols = ["#D9A441", "#F2E6CF", "#E06A3B", "#8FC9B8"]
for i in range(4):
    pts = []
    for k in range(200):
        t = k / 199
        v = 0.85 - 0.55 * i + 0.25 * math.asin(math.sin(2 * math.pi * t * (3 + i * 0.7) + i)) / 1.57
        pts.append(f"{dx + dw * t:.2f},{dy + dh / 2 - dh * 0.45 * v:.2f}")
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{cols[i]}" stroke-width="0.3"/>')
screws(g, L["GA_W"]); panels.append(("Gallop", L["GA_W"], g))
# Tumble
g = []
for i, x in enumerate(L["TU_X"]):
    knob(g, "RoundBlackKnob", x, L["TU_RATE_Y"], [0.42, 0.52, 0.36, 0.47, 0.60][i])
    g.append(f'<circle cx="{x + 6.6}" cy="{L["TU_RATE_Y"] - 5.4}" r="1.0" fill="{"#ff3" if i in (1, 3) else "#403a10"}"/>')
    knob(g, "RoundSmallBlackKnob", x, L["TU_TOUCH_Y"], [0.8, 0.5, 0.3, 0.5, 0.65][i])
    jack(g, x, L["TU_TCV_Y"]); jack(g, x, L["TU_GATE_Y"])
    knob(g, "RoundBlackKnob", x, L["TU_KNOB_Y"], [0.5, 0.4, 0.25, 0.4, 0.45][i]); jack(g, x, L["TU_JACK_Y"])
screws(g, L["TU_W"]); panels.append(("Tumble", L["TU_W"], g))

gap = 4.0
total = sum(w for _, w, _ in panels) + gap * (len(panels) + 1)
parts = []
x0 = gap
for name, w, g in panels:
    svg = open(os.path.join(RES, name + ".svg")).read()
    inner = svg.split(">", 1)[1].rsplit("</svg>", 1)[0]
    parts.append(f'<g transform="translate({x0} {gap})">{inner}{"".join(g)}</g>')
    x0 += w + gap
H = 128.5 + 2 * gap
full = f'<svg xmlns="http://www.w3.org/2000/svg" width="{total}mm" height="{H}mm" viewBox="0 0 {total} {H}"><rect width="{total}" height="{H}" fill="#111"/>{"".join(parts)}</svg>'
tmp = OUT + ".svg"; open(tmp, "w").write(full)
pw, ph = int(total * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(500);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
for f in (OUT + ".js", tmp, tmp + ".html"): os.remove(f)
print("wrote", OUT)
