#!/usr/bin/env python3
# Pictures of every voice panel with Rack's own component graphics, knobs at stock.
#   python3 tools/preview_voices.py outdir [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "res"))
from voices import VOICES
OUTDIR = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
W, H = 81.28, 128.5
BIG_X = [W / 6 + i * W / 3 for i in range(3)]
SMALL_X = [W / 8 + i * W / 4 for i in range(4)]
ROW_Y = [27.0, 44.5, 63.0, 76.5, 90.0]
JACK_Y = 112.0
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
        big = k[8] < 2
        x, y = (BIG_X if big else SMALL_X)[k[9]], ROW_Y[k[8]]
        kind = k[10] if len(k) > 10 else "knob"
        if kind == "sw3":
            comp("CKSSThree_%d" % int(k[3]), x, y)
        elif kind == "sw2":
            comp("CKSS_%d" % int(k[3]), x, y)
        else:
            nm = "RoundBlackKnob" if big else "RoundSmallBlackKnob"
            if os.path.exists(os.path.join(LIB, nm + "_bg.svg")):
                comp(nm + "_bg", x, y)
            comp(nm, x, y, -149.4 + 298.8 * k[3])
    comp("VCVBezel", BIG_X[2], ROW_Y[1])
    g.append(f'<circle cx="{BIG_X[2]}" cy="{ROW_Y[1]}" r="2.3" fill="#ff3b30" fill-opacity="0.7"/>')
    comp("VCVButton_0", SMALL_X[2], ROW_Y[v["rand_row"]]); comp("VCVButton_0", SMALL_X[3], ROW_Y[v["rand_row"]])
    n = 5 + (1 if v.get("extra_trig") else 0) + len(v["cv"])
    for i in range(n): comp("PJ301M", 7.5 + i * (W - 15.0) / (n - 1), JACK_Y)
    for x in (5.08, W - 2 * 5.08):
        for y in (0.0, 128.5 - 5.08):
            comp("ScrewSilver", x + 2.54, y + 2.54)
    out = os.path.join(OUTDIR, v["slug"] + "-panel.png")
    svg2 = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
    tmp = out + ".svg"; open(tmp, "w").write(svg2)
    pw, ph = int(W * 5), int(H * 5)
    open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#888"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
    js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{out}'}}); await b.close(); }})();"""
    open(out + ".js", "w").write(js)
    subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
    os.remove(out + ".js"); os.remove(tmp); os.remove(tmp + ".html")
    print("wrote", out)
