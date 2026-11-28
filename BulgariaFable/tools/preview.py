#!/usr/bin/env python3
# Picture of the panel with simple stand-in knobs, switches, lights and jacks (no Rack graphics needed).
#   python3 tools/preview.py docs/panel.png
import os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "panel.png")
svg = open(os.path.join(HERE, "..", "res", "Bulgaria.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 101.6, 128.5
g = []
def knob(x, y, r):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#0d0e10" stroke="#5a5f66" stroke-width="0.4"/>')
    g.append(f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - r * 0.85}" stroke="#f2f2f2" stroke-width="0.6"/>')
def jack(x, y):
    g.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#b8bcc2"/><circle cx="{x}" cy="{y}" r="1.8" fill="#111"/>')
for x in L["ROW1_X"]: knob(x, L["K1"], 4.6)
for x in L["ROW2_X"]: knob(x, L["K2"], 4.6)
for i, x in enumerate(L["LIGHT_X"]): g.append(f'<circle cx="{x}" cy="{L["K2"]}" r="1.3" fill="{"#d9534a" if i in (1, 4) else ("#7bc67b" if i < 5 else "#333")}"/>')
knob(L["RUB_X"], L["K3"], L["BIG_R"] - 0.4)
knob(L["CHANCE_X"], L["K3"], 4.6)
for x in L["SW_X"]: g.append(f'<rect x="{x - 1.6}" y="{L["K3"] - 3.2}" width="3.2" height="6.4" rx="0.6" fill="#c9ccd1"/>')
for x in L["ROW4_X"]: knob(x, L["K4"], 4.6); jack(x, L["TRIG_Y"])
for x in L["JACK_X"]: jack(x, L["J1"]); jack(x, L["J2"])
svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 96 / 25.4 * 1.5) + 1, int(H * 96 / 25.4 * 1.5) + 1
svg2 = open(tmp).read().replace(f'width="{W}mm" height="{H}mm"', f'width="{pw}px" height="{ph}px"')
open(tmp, "w").write(svg2)
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}'); await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(tmp + ".js", "w").write(js)
subprocess.run(["node", tmp + ".js"], check=True)
os.remove(tmp); os.remove(tmp + ".js")
print("ok")
