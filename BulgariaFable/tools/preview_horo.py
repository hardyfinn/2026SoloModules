#!/usr/bin/env python3
# Picture of the Horo panel with simple stand-in knobs, jacks and a mock display (no Rack graphics needed).
#   python3 tools/preview_horo.py docs/horo-panel.png
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "horo-panel.png")
svg = open(os.path.join(HERE, "..", "res", "Horo.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_horo_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 50.8, 128.5
g = []
def knob(x, y, r):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#0d0e10" stroke="#5a5f66" stroke-width="0.4"/>')
    g.append(f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - r * 0.85}" stroke="#f2f2f2" stroke-width="0.6"/>')
def jack(x, y):
    g.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#b8bcc2"/><circle cx="{x}" cy="{y}" r="1.8" fill="#111"/>')
knob(L["METRE_X"], L["METRE_Y"], L["BIG_R"] - 0.4)
g.append(f'<circle cx="{L["RBTN_X"]}" cy="{L["RBTN_Y"]}" r="2.6" fill="#c9ccd1" stroke="#777" stroke-width="0.3"/>')
for x in L["KROW_X"]: knob(x, L["KROW_Y"], 4.6)
for x in L["IN_X"]: jack(x, L["IN_Y"])
for y in L["OUT_Y"]:
    for x in L["OUT_X"]: jack(x, y)
dx, dy = L["DISP_X"], L["DISP_Y"]
g.append(f'<text x="{dx + 1.6}" y="{dy + 5.2}" font-family="monospace" font-size="4" fill="#E8B04A">RUCHENITSA 7</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + 9.6}" font-family="monospace" font-size="3.4" fill="#F1EADF">2 2 3</text>')
g.append(f'<text x="{dx + 1.6}" y="{dy + 22.6}" font-family="monospace" font-size="2.6" fill="#78685C">BAR 3/4</text>')
groups = [2, 2, 3]; n = 7; unit = (L["DISP_W"] - 3.6) / (n + 0.9 * 2); x = dx + 1.8 + unit / 2; k = 0
for gi, gl in enumerate(groups):
    for j in range(gl):
        col = "#D9534A" if k == 4 else ("#826232" if gi == 2 else "#463C36")
        r = min(0.85, unit * 0.32) * (1.25 if gl == 3 else 1)
        g.append(f'<circle cx="{x:.2f}" cy="{dy + 15.0}" r="{r:.2f}" fill="{col}"/>'); x += unit; k += 1
    x += 0.9 * unit
svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"
pw, ph = int(W * 96 / 25.4 * 1.5) + 1, int(H * 96 / 25.4 * 1.5) + 1
open(tmp, "w").write(svg.replace(f'width="{W}mm" height="{H}mm"', f'width="{pw}px" height="{ph}px"'))
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}'); await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(tmp + ".js", "w").write(js)
subprocess.run(["node", tmp + ".js"], check=True)
os.remove(tmp); os.remove(tmp + ".js")
print("ok")
