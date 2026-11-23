#!/usr/bin/env python3
# Contact sheet of all 260 shapes (same maths as KrkaDSP.hpp): python3 tools/shapesheet.py docs/shapes-sheet.png
import json, math, os, sys, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "shapes.png")
shapes = json.load(open(os.path.join(HERE, "..", "res", "shapes.json")))

def bend(u, k):
    if k == 2.0: return 0.5 - 0.5 * math.cos(math.pi * u)
    return u ** (2.0 ** (3.0 * k))

def ev(pts, t):
    for i in range(1, len(pts)):
        if t <= pts[i][0] or i == len(pts) - 1:
            a, b = pts[i - 1], pts[i]
            if b[0] - a[0] <= 1e-7: return b[1]
            u = min(1, max(0, (t - a[0]) / (b[0] - a[0])))
            return a[1] + (b[1] - a[1]) * bend(u, b[2])
    return pts[-1][1]

cw, ch, cols = 72, 30, 10
rows = math.ceil(len(shapes) / 10)
g = []
for idx, s in enumerate(shapes):
    b, n = divmod(idx, 10)
    col, row = n, b
    x0, y0 = col * cw + 4, row * (ch + 12) + 6
    pts = []
    N = 300
    for i in range(N + 1):
        t = i / N
        v = ev(s["points"], t)
        pts.append(f"{x0 + t * (cw - 8):.1f},{y0 + (1 - v) * (ch - 6):.1f}")
    g.append(f'<rect x="{x0-2}" y="{y0-3}" width="{cw-4}" height="{ch}" fill="#0b2a30"/>')
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="#ff9a6a" stroke-width="1"/>')
    g.append(f'<text x="{x0}" y="{y0 + ch + 6}" font-size="7" fill="#ddd" font-family="sans-serif">{s["bank"]}{n} {s["name"].replace("&","&amp;")}</text>')
W, H = cols * cw + 8, 26 * (ch + 12) + 10
svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><rect width="100%" height="100%" fill="#12343b"/>' + "".join(g) + "</svg>"
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
js = f"""const {{chromium}} = require('playwright');(async()=>{{const b=await chromium.launch();const p=await b.newPage({{viewport:{{width:{W},height:{H}}},deviceScaleFactor:2}});await p.goto('file://{tmp}');await p.screenshot({{path:'{OUT}'}});await b.close();}})();"""
open(OUT + ".js", "w").write(js); subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp"); os.remove(OUT + ".js"); os.remove(tmp)
print("wrote", OUT)
