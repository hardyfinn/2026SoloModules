#!/usr/bin/env python3
# Picture of the three panels side by side with Rack's component graphics:
#   python3 tools/preview.py panel-preview.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
lay = open(os.path.join(HERE, "..", "src", "Layout.hpp")).read()
PX = 25.4 / 75.0
H = 128.5
mono = "font-family:'DejaVu Sans Mono',monospace"
SCREENS = {
    "sprout": [("FIBONACCI", "#9BD770", 2.8, 3.6, "start"), ("A>AB B>A", "#F2ECD946", 2.5, 3.6, "end"),
               ("GEN 4  8 per cycle  grows /2", "#F2ECD9", 2.6, -1.2, "start")],
    "graft": [("DETROIT", "#9BD770", 2.6, 3.4, "start"), ("GEN 5", "#F2ECD9", 2.6, 3.4, "end")],
    "bloom": [("HOMECOMING", "#9BD770", 2.6, 3.4, "start"), ("F DORIAN", "#F2ECD9", 2.6, 3.4, "end"),
              ("ii  Gm", "#F2ECD9", 5.0, 10.2, "start"), ("i IV ii v ii v i", "#FFAA50", 2.6, 15.4, "start")],
}
def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')
parts, x0 = [], 0.0
for name in ("Sprout", "Graft", "Bloom"):
    svg = open(os.path.join(HERE, "..", "res", name + ".svg")).read()
    W = float(re.search(r'viewBox="0 0 ([0-9.]+)', svg).group(1))
    inner = svg.split(">", 1)[1].rsplit("</svg>", 1)[0]
    block = lay.split(f"namespace {name.lower()} {{")[1].split("}")[0]
    g = []
    for kind, lab, x, y in re.findall(r"([KSBIO])_(\w+)_X = ([0-9.]+)f, \w+ = ([0-9.]+)f", block):
        x, y = float(x), float(y)
        if kind == "K":
            big = y < 60
            base = "RoundBlackKnob" if big else "RoundSmallBlackKnob"
            comp(g, base + "_bg", x, y); comp(g, base, x, y, -135 + 270 * 0.35)
        elif kind == "S": comp(g, "CKSS_0", x, y)
        elif kind == "B": comp(g, "VCVBezel", x, y)
        else: comp(g, "PJ301M", x, y)
    dx, dy = [float(v) for v in re.search(r"DISP_X = ([0-9.]+)f, DISP_Y = ([0-9.]+)f", block).groups()]
    dw, dh = [float(v) for v in re.search(r"DISP_W = ([0-9.]+)f, DISP_H = ([0-9.]+)f", block).groups()]
    for t, col, size, yy, anc in SCREENS[name.lower()]:
        op = ""
        if len(col) == 9: op = f' fill-opacity="{int(col[7:], 16) / 255:.2f}"'; col = col[:7]
        X = dx + 1.4 if anc == "start" else dx + dw - 1.4
        Y = dy + yy if yy > 0 else dy + dh + yy
        g.append(f'<text x="{X}" y="{Y}" font-size="{size}" text-anchor="{anc}" fill="{col}"{op} style="{mono}">{t}</text>')
    if name == "Sprout":
        word = "ABAABABA"
        for i, c in enumerate(word):
            a = dx + 1.4 + (dw - 2.8) * i / 8; top = dy + 6 + (dh - 11.4) * (0 if c == "A" else 0.45)
            g.append(f'<rect x="{a + 0.2}" y="{top}" width="{(dw - 2.8) / 8 - 0.4}" height="{dy + dh - 5.4 - top}" fill="{"#9BD770" if c == "A" else "#F2ECD9"}" fill-opacity="{1 if i == 3 else 0.35}"/>')
    if name == "Graft":
        pats = ["x...x...x..xx...", "....x.......x..x", "..x...x.x.x...x.", "...x..x....x..x."]
        for l, pat in enumerate(pats):
            for s, c in enumerate(pat):
                if c == "x":
                    cw = (dw - 2.4 - 3.4) / 16; rh = (dh - 4.6 - 1.2) / 4
                    g.append(f'<rect x="{dx + 1.2 + 3.4 + s * cw + 0.2}" y="{dy + 4.6 + l * rh + 0.3}" width="{cw - 0.4}" height="{rh - 0.6}" fill="{"#FFAA50" if (l, s) in ((0, 11), (1, 15), (2, 8)) else "#9BD770"}"/>')
    for x in (5.08, W - 2 * 5.08):
        for y in (0.0, H - 5.08):
            comp(g, "ScrewSilver", x + 2.54, y + 2.54)
    parts.append(f'<g transform="translate({x0} 0)">{inner}{"".join(g)}</g>')
    x0 += W + 4
TW = x0 - 4
svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{TW}mm" height="{H}mm" viewBox="0 0 {TW} {H}">' + "".join(parts) + "</svg>"
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(TW * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');(async()=>{{const b=await chromium.launch();const p=await b.newPage({{viewport:{{width:{pw},height:{ph}}},deviceScaleFactor:2}});await p.goto('file://{tmp}.html');await p.waitForTimeout(400);await p.screenshot({{path:'{OUT}'}});await b.close();}})();"""
open(OUT + ".js", "w").write(js); subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
