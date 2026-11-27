#!/usr/bin/env python3
# Picture of the Lightning panel with Rack's own component graphics.
#   python3 tools/preview_lightning.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview-lightning.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Lightning.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_lightning_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 142.24, 128.5
PX = 25.4 / 75.0
g = []


def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')


def knob(x, y, v):
    if os.path.exists(os.path.join(LIB, "RoundBlackKnob_bg.svg")):
        comp("RoundBlackKnob_bg", x, y)
    comp("RoundBlackKnob", x, y, -135 + 270 * v)


vals = [[0.5, 1.0, 0.8], [0.6, 0.7, 0.8], [0.42, 1.0, 0.8], [0.5, 0.4, 0.8], [0.45, 1.0, 0.8], [0.55, 0.6, 0.8]]
for s, x in enumerate(L["COL"]):
    dx, dy, dw, dh = x - L["DISP_W"] / 2, L["DISP_Y"], L["DISP_W"], L["DISP_H"]
    g.append(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="0.7" fill="#0c1a12"/>')
    if s != 3:
        mid, amp = dy + dh * 0.55, dh * 0.38
        d = ""
        for i in range(64):
            t = i / 64.0
            a = math.exp(-t * (3 + 3 * s)) * (0.95 if i > 0 else 0.4)
            xx = dx + 0.35 + (dw - 0.7) * (i + 0.5) / 64
            d += f"M{xx:.2f} {mid - a * amp:.2f}L{xx:.2f} {mid + a * amp:.2f}"
        g.append(f'<path d="{d}" stroke="#66ff77" stroke-width="0.3"/>')
        g.append(f'<text x="{dx + 0.9}" y="{dy + 3.0}" font-family="monospace" font-size="3" fill="#66ff77">{s + 1}</text>')
        g.append(f'<text x="{dx + dw - 0.9}" y="{dy + 3.0}" text-anchor="end" font-family="monospace" font-size="3" fill="#66ff77">T</text>')
    else:
        g.append(f'<text x="{dx + 0.9}" y="{dy + 3.0}" font-family="monospace" font-size="3" fill="#66ff77">{s + 1}</text>')
        g.append(f'<text x="{dx + dw/2}" y="{dy + dh*0.65}" text-anchor="middle" font-family="monospace" font-size="2.5" fill="#66ff77">WAIT</text>')
    comp("VCVBezel", x + L["KEEP_DX"], L["BTN_Y"])
    g.append(f'<circle cx="{x + L["KEEP_DX"]}" cy="{L["BTN_Y"]}" r="2.3" fill="#ffcc00" fill-opacity="{0.35 if s < 3 else 0.9}"/>')
    comp("VCVButton_0", x + L["SEND_DX"], L["BTN_Y"])
    for r, v in zip(L["ROW"], vals[s]): knob(x, r, v)
    comp("PJ301M", x, L["IO_TOP"]); comp("PJ301M", x, L["IO_BOT"])
c5 = L["C5"]
g.append(f'<circle cx="{c5}" cy="{L["BTN_Y"]}" r="1.5" fill="#55dd66"/>')
knob(c5, L["ROW"][0], 0.5); knob(c5, L["ROW"][2], 0.8)
comp("PJ301M", c5, L["IO_TOP"]); comp("PJ301M", c5, L["IO_BOT"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)
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
