#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, json, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Krka.svg")).read()
lay = open(os.path.join(HERE, "..", "src", "KrkaLayout.hpp")).read()
def arr(name):
    return [float(v.rstrip("f")) for v in re.search(name + r"\[\d+\] = \{([^}]*)\}", lay).group(1).split(",")]
def val(name):
    return float(re.search(r"\b" + name + r" = ([0-9.]+)f", lay).group(1))
COLX, ROWY, GX = arr("COLX"), arr("ROW_Y"), arr("GX")
W = val("PANEL_W"); H = 128.5
GCX = val("GCX")
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

def lit(x, y, col, r=1.6):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}"/>')

vals = [[0.04, 0.2, 0, 0.55, 0.5, 0.5, 1.0, 0, 0, 1.0, 0.0, 0.3, 0.5],
        [0.0, 0.1, 0, 0.5, 0.65, 0.4, 0.8, 1, 0, 0.75, 0.03, 0.3, 0.7],
        [0.48, 0.0, 1, 0.62, 0.4, 0.6, 0.9, 0, 1, 0.5, 0.0, 0.6, 0.3],
        [0.2, 0.0, 0, 0.7, 0.5, 0.5, 0.6, 0, 0, 1.0, 1.0, 0.85, 0.65]]
for r, y in enumerate(ROWY):
    v = vals[r]
    comp("PJ301M", COLX[0], y)
    comp("VCVBezel", COLX[1], y)
    if r in (0, 3): lit(COLX[1], y, "#ffffff", 2.2)
    knob("RoundSmallBlackKnob", COLX[2], y, v[0]); knob("RoundSmallBlackKnob", COLX[3], y, v[1])
    comp("VCVBezel", COLX[4], y)
    if v[2]: lit(COLX[4], y, "#ffffff")
    knob("RoundBlackKnob", COLX[5], y, v[3])
    for i, k in zip((6, 7, 8), (4, 5, 6)): knob("RoundSmallBlackKnob", COLX[i], y, v[k])
    comp("VCVBezel", COLX[9], y)
    if v[7]: lit(COLX[9], y, "#ffd23a")
    comp("VCVBezel", COLX[10], y)
    if v[8]: lit(COLX[10], y, "#3ae07a")
    knob("RoundSmallBlackKnob", COLX[11], y, v[9]); knob("RoundSmallBlackKnob", COLX[12], y, v[10])
    knob("RoundSmallBlackKnob", COLX[13], y, v[11]); knob("Trimpot", COLX[14], y, v[12])
    for i in (15, 16, 17): comp("PJ301M", COLX[i], y)
knob("RoundBlackKnob", GCX, val("G_ALGO_Y"), 0.73)
comp("PJ301M", GCX, val("G_ALGOCV_Y"))
comp("PJ301M", GX[0], val("G_CLOCK_Y")); comp("PJ301M", GX[1], val("G_CLOCK_Y"))
knob("RoundBlackKnob", GCX, val("G_PRESET_Y"), 0.12)
comp("VCVBezel", GX[0], val("G_BTN_Y")); lit(GX[0], val("G_BTN_Y"), "#3ae07a", 2.2)
comp("VCVBezel", GX[1], val("G_BTN_Y"))
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen
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
dx, dy, dw, dh = val("DISP_X"), val("DISP_Y"), val("DISP_W"), val("DISP_H")
mono = "font-family:'DejaVu Sans Mono',monospace"
tiles = [(12, "B2 Perc 2", "1/4  x2", 0.35, "CV>TIME"), (130, "N0 Pluck 0", "1/4.  GATE", 0.62, "CV>RESP"),
         (120, "M0 Swell 0", "2 BARS  REV  AN", 0.2, "CV>WARP"), (220, "W0 Wave 2", "1 BAR  LOOP", 0.8, "CV>SHAPE")]
pad = 1.4 * 75 / 25.4 * PX
statusH = 4.2
tw = (dw - 1.4) / 4
for c, (si, nm, tt, ph, dest) in enumerate(tiles):
    x0 = dx + 1.4 + c * tw; x1 = x0 + tw - 1.4
    cy0, cy1 = dy + 4.6, dy + dh - statusH - 4.4
    g.append(f'<rect x="{x0}" y="{dy + 0.7}" width="{x1 - x0}" height="{dh - statusH - 0.7}" rx="1" fill="#ffffff" fill-opacity="0.03"/>')
    pts = []
    sp = shapes[si]["points"]
    rev = "REV" in tt
    for i in range(97):
        x = i / 96
        v = ev(sp, 1 - x if rev else x)
        pts.append(f"{x0 + 1 + (x1 - x0 - 2) * x:.2f},{cy1 - (cy1 - cy0) * v:.2f}")
    g.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="#FF8A5B" stroke-width="0.45" stroke-linejoin="round"/>')
    v = ev(sp, 1 - ph if rev else ph)
    px = x0 + 1 + (x1 - x0 - 2) * ph
    g.append(f'<line x1="{px}" y1="{cy0}" x2="{px}" y2="{cy1}" stroke="#F3EBDD" stroke-opacity="0.25" stroke-width="0.3"/>')
    g.append(f'<circle cx="{px}" cy="{cy1 - (cy1 - cy0) * v}" r="0.75" fill="#F3EBDD"/>')
    g.append(f'<text x="{x0 + 1}" y="{dy + 3.4}" font-size="2.9" fill="#FF8A5B" style="{mono}">{"ABCD"[c]}</text>')
    g.append(f'<text x="{x0 + 4}" y="{dy + 3.4}" font-size="2.9" fill="#F3EBDD" style="{mono}">{nm}</text>')
    g.append(f'<text x="{x1 - 1}" y="{dy + 3.4}" font-size="2.6" text-anchor="end" fill="#78CDC8" fill-opacity="0.6" style="{mono}">{dest}</text>')
    g.append(f'<text x="{x0 + 1}" y="{dy + dh - statusH - 1.2}" font-size="2.6" fill="#78CDC8" style="{mono}">{tt}</text>')
g.append(f'<text x="{dx + 2.4}" y="{dy + dh - 1.0}" font-size="2.9" fill="#FF8A5B" style="{mono}">CHAIN 12  LOOP A&gt;B&gt;C&gt;D</text>')
g.append(f'<text x="{dx + dw - 2.4}" y="{dy + dh - 1.0}" font-size="2.9" text-anchor="end" fill="#F3EBDD" style="{mono}">CLOCK 124.0 BPM   PRESET 1 (loaded)</text>')

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
