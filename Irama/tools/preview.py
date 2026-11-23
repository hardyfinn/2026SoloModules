#!/usr/bin/env python3
# Pictures of both panels with Rack's own component graphics (and a sample screen).
#   python3 tools/preview.py outdir [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
for blk in src.split("# ---- LAYOUT")[1:]:
    exec(blk.split("\n", 1)[1].split("\nparts = []")[0], L)
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

# ---- Irama
g = []; screws(g, L["W"] if False else 203.2)
C = L["COLX"]
comp(g, "CKSS_0", C[0], L["ROW1_Y"])
for x, v in zip(C[1:], [0.0, 0.4, 0.5, 0.25, 0.75]): knob(g, "RoundBlackKnob", x, L["ROW1_Y"], v)
for x, v in zip(C[:4], [0.3, 0.33, 0.0, 0.2]): knob(g, "RoundBlackKnob", x, L["ROW2_Y"], v)
comp(g, "CKSS_0", C[4], L["ROW2_Y"]); comp(g, "CKSS_0", C[5], L["ROW2_Y"])
for i in range(4): comp(g, "VCVBezel", C[i], L["ROW3_Y"])
comp(g, "MediumLight", C[5], L["ROW3_Y"])
for i in range(8):
    comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["IN_Y"]); comp(g, "PJ301M", L["JX0"] + L["JDX"] * i, L["COL_Y"])
for r in range(7):
    comp(g, "PJ301M", L["OUT_VX"], L["OUT_Y0"] + L["OUT_DY"] * r); comp(g, "PJ301M", L["OUT_GX"], L["OUT_Y0"] + L["OUT_DY"] * r)
x0, y0 = L["DISP_X"], L["DISP_Y"]
screen(g, x0, y0, 0, 0, [("LANCARAN  SLENDRO MANYURA", "#E8B040", 2.9, "start", 1.7, 3.6), ("IRAMA TANGGUNG", "#F3E6C8", 2.9, "end", 123.3, 3.6)])
bal = ["3 5 3 2", "6 5 3 2", "5 6 1 6", "3 2 1 6"]
for i, b in enumerate(bal):
    g.append(f'<text x="{x0 + 8 + i * 31}" y="{y0 + 13.5}" font-family="monospace" font-size="4.2" fill="#F3E6C8" xml:space="preserve">{b}</text>')
g.append(f'<circle cx="{x0 + 8 + 3 * 31 + 13.2}" cy="{y0 + 12.1}" r="2.6" fill="none" stroke="#E8B040" stroke-width="0.3"/>')
render("Irama.svg", 203.2, g, os.path.join(OUT, "irama-preview.png"))

# ---- Gatra
g = []; screws(g, 121.92)
C = L["G_COLX"]
knob(g, "RoundBlackKnob", C[0], L["G_ROW1_Y"], 0.33); knob(g, "RoundLargeBlackKnob", C[1], L["G_ROW1_Y"], 0.5)
knob(g, "RoundBlackKnob", C[2], L["G_ROW1_Y"], 0.85); knob(g, "RoundBlackKnob", C[3], L["G_ROW1_Y"], 0.3)
knob(g, "RoundBlackKnob", C[0], L["G_ROW2_Y"], 0.2); knob(g, "RoundBlackKnob", C[1], L["G_ROW2_Y"], 0.1)
comp(g, "CKSS_0", L["G_SWX"][0], L["G_ROW2_Y"]); comp(g, "CKSSThree_1", L["G_SWX"][1], L["G_ROW2_Y"]); comp(g, "CKSS_1", L["G_SWX"][2], L["G_ROW2_Y"])
comp(g, "VCVBezel", C[0], L["G_ROW3_Y"]); comp(g, "VCVBezel", C[1], L["G_ROW3_Y"]); comp(g, "MediumLight", C[3], L["G_ROW3_Y"])
for i in range(8): comp(g, "PJ301M", L["G_JX0"] + L["G_JDX"] * i, L["G_IN_Y"])
for i in range(5):
    comp(g, "PJ301M", L["G_OX0"] + L["G_ODX"] * i, L["G_OA_Y"]); comp(g, "PJ301M", L["G_OX0"] + L["G_ODX"] * i, L["G_OB_Y"])
x0, y0 = L["G_DISP_X"], L["G_DISP_Y"]
screen(g, x0, y0, 0, 0, [("16 BEATS  TELU", "#E8B040", 2.8, "start", 1.7, 3.5), ("GONG +2/16", "#F3E6C8", 2.8, "end", 108.2, 3.5)])
for i in range(16):
    x = x0 + 1.7 + (i + 0.5) * 106.5 / 16; y = y0 + 11.3
    r = 1.2 if i == 15 else 0.8 if i % 4 == 3 else 0.55 if i in (5, 9, 13) else 0.3
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{"#E8B040" if i == 15 else "#F3E6C8"}"/>')
render("Gatra.svg", 121.92, g, os.path.join(OUT, "gatra-preview.png"))
print("ok")
