#!/usr/bin/env python3
# Pictures of both panels with Rack's own component graphics (and a sample screen).
#   python3 tools/preview.py outdir [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
L = {}
src = open(os.path.join(HERE, "..", "res", "make_instrument_panels.py")).read()
for blk in src.split("# ---- LAYOUT")[1:]:
    exec(blk.split("\n", 1)[1].split("\nparts, add")[0], L)
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

def screen(g, x, y, w, h, lines):
    for i, (t, col, size, anchor, xx, yy) in enumerate(lines):
        g.append(f'<text x="{x + xx}" y="{y + yy}" font-family="monospace" font-size="{size}" fill="{col}" text-anchor="{anchor}">{t}</text>')

def render(svgname, W, g, png):
    svg = open(os.path.join(HERE, "..", "res", svgname)).read()
    body = svg.split(">", 1)[1].rsplit("</svg>", 1)[0]
    html = (f'<html><body style="margin:0;background:#111"><svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{W * 6}" height="{128.5 * 6}" viewBox="0 0 {W} 128.5">{body}{"".join(g)}</svg></body></html>')
    p = os.path.join(OUT, svgname + ".html"); open(p, "w").write(html)
    js = ("const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const pg=await b.newPage({viewport:{width:%d,height:%d}});"
          "await pg.goto('file://%s');await pg.screenshot({path:'%s'});await b.close();})();" % (int(W * 6), int(128.5 * 6), p, png))
    subprocess.run(["node", "-e", js], check=True); os.remove(p)

def screws(g, W):
    for x in (7.5, W - 7.5):
        for y in (3.0, 125.5): comp(g, "ScrewSilver", x, y)


# ---- Ageng
g = []; screws(g, 91.44)
for i, v in enumerate([0.5, 0.25, 0.5, 0.4, 0.27, 0.35, 0.5, 0.5, 0.3]):
    knob(g, "RoundLargeBlackKnob" if i == 0 else "RoundBlackKnob", L["A_COLX"][i % 3], L["A_ROW_Y"][i // 3], v)
comp(g, "CKSS_0", L["A_COLX"][0], L["A_ROW4_Y"])
knob(g, "RoundSmallBlackKnob", L["A_COLX"][1], L["A_ROW4_Y"], 1.0); knob(g, "RoundSmallBlackKnob", L["A_COLX"][2], L["A_ROW4_Y"], 0.8)
for i in range(4): comp(g, "PJ301M", L["A_IN_X0"] + L["A_IN_DX"] * i, L["A_IN_Y"])
for i in range(6): comp(g, "PJ301M", L["A_BOT_X0"] + L["A_BOT_DX"] * i, L["A_BOT_Y"])
render("Ageng.svg", 91.44, g, os.path.join(OUT, "ageng-preview.png"))
# ---- Gangsa
g = []; screws(g, 101.6)
for i, v in enumerate([0.54, 0.4, 0.7, 0.75, 0.26, 0.6, 0.5, 0.3, 0.6]):
    knob(g, "RoundLargeBlackKnob" if i in (0, 3) else "RoundBlackKnob", L["G2_COLX"][i % 3], L["G2_ROW_Y"][i // 3], v)
comp(g, "CKSS_0", L["G2_COLX"][0], L["G2_ROW4_Y"]); comp(g, "MediumLight", L["G2_COLX"][1], L["G2_ROW4_Y"]); comp(g, "MediumLight", L["G2_COLX"][2], L["G2_ROW4_Y"])
for i in range(5): comp(g, "PJ301M", L["G2_IN_X0"] + L["G2_IN_DX"] * i, L["G2_IN_Y"])
for i in range(6): comp(g, "PJ301M", L["G2_BOT_X0"] + L["G2_BOT_DX"] * i, L["G2_BOT_Y"])
render("Gangsa.svg", 101.6, g, os.path.join(OUT, "gangsa-preview.png"))
# ---- Reyong
g = []; screws(g, 111.76)
for i, v in enumerate([None, 0.2, 0.5, 0.27, 0.5, 0.0, 0.5, 0.3, None, 0.3, 0.6, 0.45]):
    x, y = L["R_COLX"][i % 4], L["R_ROW_Y"][i // 4]
    if v is None: comp(g, "CKSS_0", x, y)
    else: knob(g, "RoundBlackKnob", x, y, v)
for i in range(7): comp(g, "PJ301M", L["R_IN_X0"] + L["R_IN_DX"] * i, L["R_IN_Y"])
for i in range(6): comp(g, "PJ301M", L["R_BOT_X0"] + L["R_BOT_DX"] * i, L["R_BOT_Y"])
render("Reyong.svg", 111.76, g, os.path.join(OUT, "reyong-preview.png"))
print("ok")
