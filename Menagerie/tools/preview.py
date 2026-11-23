#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "panel-preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
PX = 25.4 / 75.0
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
L = {}
for block in src.split("# ---- LAYOUT")[1:]:
    body = block.split("\n", 1)[1].split("\nP = []")[0]
    exec(body, L)


def inner(path):
    s = open(path).read()
    return re.sub(r"^.*?<svg[^>]*>", "", s, flags=re.S).replace("</svg>", "")


def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')


def knob(g, base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(g, base + "_bg", x, y)
    comp(g, base, x, y, -135 + 270 * v)


def jack(g, x, y): comp(g, "PJ301M", x, y)


# ---------------- Menagerie
g = []
knob(g, "RoundLargeBlackKnob", L["XA"][0], L["ROWA_Y"], 0.5)
knob(g, "RoundBlackKnob", L["XA"][1], L["ROWA_Y"], 0.5)
knob(g, "RoundLargeBlackKnob", L["XA"][2], L["ROWA_Y"], 1 / 15)
for x, v in zip(L["XB"], [0.45, 0.6, 0.3]): knob(g, "RoundLargeBlackKnob", x, L["ROWB_Y"], v)
for x in L["XB"]: knob(g, "Trimpot", x, L["ROWC_Y"], 0.5)
for x in L["JX"]: jack(g, x, L["IN1_Y"]); jack(g, x, L["IN2_Y"])
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + 2.3}" y="{dy + 6.4}" font-size="5.0" fill="#7FE0A0" style="{mono}">02 HERD</text>')
cw = (dw - 4.6) / 3
for i, (h, v) in enumerate(zip(["spread", "pull", "stray"], [0.45, 0.6, 0.3])):
    x = dx + 2.3 + i * cw
    g.append(f'<text x="{x}" y="{dy + 11.2}" font-size="3.1" fill="#4A7A5C" style="{mono}">{h}</text>')
    g.append(f'<rect x="{x}" y="{dy + 12.8}" width="{cw - 2.6}" height="1.0" fill="#223328"/>')
    g.append(f'<rect x="{x}" y="{dy + 12.8}" width="{(cw - 2.6) * v}" height="1.0" fill="#7FE0A0"/>')
panel = inner(os.path.join(HERE, "..", "res", "Menagerie.svg")) + "\n".join(g)

TW, H = 91.44, 128.5
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{TW}mm" height="{H}mm" viewBox="0 0 {TW} {H}">'
       f'<g>{panel}</g></svg>')
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(TW * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#2A2D33"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
