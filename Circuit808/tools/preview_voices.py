#!/usr/bin/env python3
# Pictures of every voice panel with Rack's own component graphics, knobs at stock.
#   python3 tools/preview_voices.py outdir [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "res"))
from voices import VOICES
OUTDIR = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
COL = [11.0, 31.04, 51.08, 71.12, 91.16, 111.2, 131.24]
ROWS = [30.0, 52.0, 74.0]
W, H = 142.24, 128.5
PX = 25.4 / 75.0

for v in VOICES:
    svg = open(os.path.join(HERE, "..", "res", v["slug"] + ".svg")).read()
    g = []

    def comp(name, x, y, angle=0.0):
        s = open(os.path.join(LIB, name + ".svg")).read()
        m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
        w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
        data = base64.b64encode(s.encode()).decode()
        g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

    for k in v["knobs"]:
        x, y = COL[k[9]], ROWS[k[8]]
        kind = k[10] if len(k) > 10 else "knob"
        if kind == "sw3":
            comp("CKSSThree_%d" % int(k[3]), x, y)
        elif kind == "sw2":
            comp("CKSS_%d" % int(k[3]), x, y)
        else:
            if os.path.exists(os.path.join(LIB, "RoundBlackKnob_bg.svg")):
                comp("RoundBlackKnob_bg", x, y)
            comp("RoundBlackKnob", x, y, -149.4 + 298.8 * k[3])
    comp("VCVBezel", COL[6], ROWS[0])
    g.append(f'<circle cx="{COL[6]}" cy="{ROWS[0]}" r="2.3" fill="#ff3b30" fill-opacity="0.7"/>')
    for i in range(len(v["cv"])): comp("PJ301M", COL[i], 96.0)
    for i in (0, 1, 5, 6): comp("PJ301M", COL[i], 115.0)
    for x in (5.08, W - 2 * 5.08):
        for y in (0.0, 128.5 - 5.08):
            comp("ScrewSilver", x + 2.54, y + 2.54)
    out = os.path.join(OUTDIR, v["slug"] + "-panel.png")
    svg2 = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
    tmp = out + ".svg"; open(tmp, "w").write(svg2)
    pw, ph = int(W * 5), int(H * 5)
    open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
    js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{out}'}}); await b.close(); }})();"""
    open(out + ".js", "w").write(js)
    subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
    os.remove(out + ".js"); os.remove(tmp); os.remove(tmp + ".html")
    print("wrote", out)
