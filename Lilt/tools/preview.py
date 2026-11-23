#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math, cmath
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Lilt.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 182.88, 128.5
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)


G = L["GX"]
for x, v in zip(G, [0.37, 0.2, 0.3, 0.0]): knob("RoundLargeBlackKnob", x, L["ROW1_Y"], v)
for x, v in zip(G[:3], [0.0, 0.5, 0.5]): knob("RoundBlackKnob", x, L["ROW2_Y"], v)
comp("CKSS_1", L["SNAP_X"], L["ROW2_Y"]); comp("CKSSThree_2", L["DIR_X"], L["ROW2_Y"])
for i in range(6):
    x = L["CV_X0"] + L["CV_DX"] * i
    knob("Trimpot", x, L["TRIM_Y"], 1.0); comp("PJ301M", x, L["CVJ_Y"]); comp("PJ301M", x, L["IO_Y"])
for i, x in enumerate(L["LX"]):
    knob("RoundBlackKnob", x, L["RATIO_Y"], [0.5, 0.465, 0.6, 0.37][i])
    knob("RoundSmallBlackKnob", x, L["LEVEL_Y"], [1.0, 0.9, 0.8, 0.75][i])
    for y in (L["LFOJ_Y"], L["CVJ_Y"], L["IO_Y"]): comp("PJ301M", x, y)
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
pad, top, bot = 1.4, dy + 1.4, dy + dh - 4.6
N = 360
def yv(v): return top + (bot - top) * (1 - (max(-1.25, min(1.25, v)) + 1.25) / 2.5)
def xi(i): return dx + pad + (dw - 2 * pad) * i / (N - 1)
r = [1.0, 0.75, 1.5, 1/3]; lv = [1.0, 0.8, 0.6, 0.5]; dr = [0.31, -0.57, 0.79, -0.23]
thr = [-0.33, -0.11, 0.11, 0.33]; CH = ["#FF6F59", "#FFC145", "#5BE0B3", "#7AB8FF"]
for b in range(8):
    x = xi(b * N / 8); g.append(f'<line x1="{x}" y1="{top}" x2="{x}" y2="{bot}" stroke="#FFC145" stroke-opacity="0.18" stroke-width="0.2"/>')
for k in range(4):
    g.append(f'<line x1="{dx+pad}" y1="{yv(thr[k])}" x2="{dx+dw-pad}" y2="{yv(thr[k])}" stroke="{CH[k]}" stroke-opacity="0.35" stroke-width="0.25"/>')
S = []
for j in range(4):
    pts = []
    for i in range(N):
        t = i * 8 / N; val = math.sin(2 * math.pi * r[j] * (1 + 0.15 * 0.2 * dr[j]) * t)
        pts.append(f"{xi(i):.2f},{top + (bot - top) * (0.5 - 0.45 * val):.2f}")
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="#BFB0C6" stroke-opacity="0.16" stroke-width="0.22"/>')
for i in range(N):
    t = i * 8 / N
    S.append(sum(lv[j] * math.sin(2 * math.pi * r[j] * (1 + 0.15 * 0.2 * dr[j]) * t) for j in range(4)) / sum(lv))
g.append('<polyline points="' + " ".join(f"{xi(i):.2f},{yv(S[i]):.2f}" for i in range(N)) + '" fill="none" stroke="#F1E9D6" stroke-width="0.45" stroke-linejoin="round"/>')
for k in range(4):
    for i in range(1, N):
        if S[i - 1] <= thr[k] < S[i]:
            g.append(f'<circle cx="{xi(i):.2f}" cy="{yv(thr[k]):.2f}" r="0.55" fill="{CH[k]}"/>')
mono = "font-family:'DejaVu Sans Mono',monospace"
g.append(f'<text x="{dx + 1.4}" y="{dy + dh - 1.3}" font-size="2.7" fill="#F1E9D6" style="{mono}">124 BPM  DRIFT 20%  MUT 30%  GRID 0%</text>')
g.append(f'<text x="{dx + dw - 1.4}" y="{dy + dh - 1.3}" font-size="2.7" text-anchor="end" fill="#FFC145" style="{mono}">1.01  0.737  1.52  0.332</text>')
g.append(f'<circle cx="{G[0] + 7.5}" cy="{L["ROW1_Y"] - 7}" r="0.9" fill="#FFD84A"/>')
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
