#!/usr/bin/env python3
# Picture of the panel with simple stand-in knobs and jacks at the default settings (no Rack sources needed).
#   python3 tools/preview.py out.png      (needs node + playwright)
import os, sys, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
svg = open(os.path.join(HERE, "..", "res", "LongMetal.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 203.2, 128.5
g = []
def knob(x, y, v, r=4.0):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#1d1e20" stroke="#5a5e63" stroke-width="0.35"/>')
    a = math.radians(-135 + 270 * v - 90)
    g.append(f'<line x1="{x}" y1="{y}" x2="{x + 0.8 * r * math.cos(a):.2f}" y2="{y + 0.8 * r * math.sin(a):.2f}" stroke="#ece8e1" stroke-width="0.6" stroke-linecap="round"/>')
def jack(x, y):
    g.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#9aa0a6"/><circle cx="{x}" cy="{y}" r="1.9" fill="#0b0b0c"/>')
def button(x, y):
    g.append(f'<rect x="{x-3.2}" y="{y-3.2}" width="6.4" height="6.4" rx="0.9" fill="#3a3c40" stroke="#777" stroke-width="0.3"/><rect x="{x-2.2}" y="{y-2.2}" width="4.4" height="4.4" rx="0.6" fill="#d8d8d8"/>')
def switch(x, y):
    g.append(f'<rect x="{x-1.6}" y="{y-3.4}" width="3.2" height="6.8" rx="0.6" fill="#a7abb0"/><rect x="{x-1.2}" y="{y+0.2}" width="2.4" height="2.8" rx="0.4" fill="#2a2a2a"/>')
def pos(p, lo, hi): return (p - lo) / (hi - lo)
C11, C10, C12 = L["COL11"], L["COL10"], L["COL12"]
r1 = [pos(0, -2, 1), 1.0, pos(9, 1, 40), None, 0.51, 0.71, pos(0.31, -1, 1), pos(0.31, -1, 1), 0.5, 0.42, 0.35]
for x, v in zip(C11, r1):
    if v is None: switch(x, L["Y1"])
    else: knob(x, L["Y1"], v)
R2 = L["ROW2_X"]
knob(R2[0], L["Y2"], 0.225, 7.0); knob(R2[1], L["Y2"], pos(0.7, -1, 1), 7.0)
knob(R2[2], L["Y2"], 0.12); knob(R2[3], L["Y2"], pos(30, 4, 120)); button(R2[4], L["Y2"])
for x in R2[5:]: knob(x, L["Y2"], 0)
R3 = L["ROW3_X"]
for x, v in zip(R3, [0.5, 0.49, 0.5, None, 0.8, 0.5, 1.0, 1.0, 0.7]):
    if v is None: button(x, L["Y3"])
    else: knob(x, L["Y3"], v)
for x in C10: jack(x, L["Y4"])
for x in C12: jack(x, L["Y5"])
# a still frame of the display: shimmer far right, train ticks, listener mark
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
for b in range(80):
    t = (b + 0.5) / 80; e = math.exp(-abs(t - 0.82) * 6)
    hh = dh * (0.25 + 0.6 * e)
    g.append(f'<rect x="{dx + 2 + b * (dw - 4) / 80:.2f}" y="{dy + dh / 2 - hh / 2:.2f}" width="{(dw - 4) / 80 + 0.1:.2f}" height="{hh:.2f}" fill="#c9a36b" opacity="{0.15 + 0.75 * e:.2f}"/>')
g.append(f'<line x1="{dx + 2}" y1="{dy + dh / 2}" x2="{dx + dw - 2}" y2="{dy + dh / 2}" stroke="#aab0b6" stroke-width="0.3"/>')
for o in [0, 3, 12, 15, 29, 32, 41, 44]:
    x = dx + 2 + (dw - 4) * (0.82 + o / 319)
    g.append(f'<circle cx="{x:.2f}" cy="{dy + dh / 2}" r="0.55" fill="#ece8e1"/>')
xl = dx + 2 + (dw - 4) * 0.12
g.append(f'<line x1="{xl:.2f}" y1="{dy + 0.6}" x2="{xl:.2f}" y2="{dy + dh - 0.6}" stroke="#8fc8dc" stroke-width="0.5"/>')
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        g.append(f'<circle cx="{x + 2.54}" cy="{y + 2.54}" r="1.6" fill="#b9bdc2"/>')
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
