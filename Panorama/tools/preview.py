#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary] [tenor|second|cello]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
KIND = sys.argv[3] if len(sys.argv) > 3 else "tenor"
svg = open(os.path.join(HERE, "..", "res", "Panorama.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 203.2, 128.5
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


vals = [[0.5, 0.25, 0.7, 0.45], [0.35, 0.5, 0.4, 0.0], [0.13, 0.16, 0.0, 0.6]]
for r in range(3):
    for c in range(4):
        knob("RoundBlackKnob", L["KX"][c], L["ROW_Y"][r], vals[r][c])
for x, v in zip(L["ED_X"], [0.53, 0.46, 0.5]):
    knob("RoundSmallBlackKnob", x, L["ED_Y"], v)
comp("VCVButton_0", L["ROLL_BTN_X"], L["ED_Y"])
comp("CKSSThree_2" if KIND == "tenor" else ("CKSSThree_1" if KIND == "second" else "CKSSThree_0"), L["TYPE_X"], L["ED_Y"])
knob("RoundSmallBlackKnob", L["TUNE_X"], L["ED_Y"], 0.5)
for x in L["IN_X"]:
    comp("PJ301M", x, L["IN_Y"])
for r in range(3):
    for c in range(4):
        comp("PJ301M", L["KX"][c], L["CV_Y"][r])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# ---- sample screen (same geometry as PanDisplay in src/Panorama.cpp)
TYPES = {"tenor": (60, 88, 1, [[0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5]], "TENOR", 0.17),
         "second": (54, 85, 2, [[0, 2, 4, 6, 8, 10], [1, 3, 5, 7, 9, 11]], "DOUBLE SECOND", 0.2),
         "cello": (47, 76, 3, [[0, 3, 6, 9], [1, 4, 7, 10], [2, 5, 8, 11]], "TRIPLE CELLO", 0.25)}
lo, hi, npans, order, tname, base = TYPES[KIND]
PC = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"]
notes, rings = [], {}
for m in range(lo, hi + 1):
    pc = m % 12
    for p in range(npans):
        if pc in order[p]:
            pan, slot = p, order[p].index(pc)
    slots = len(order[pan])
    ring = rings.get((pan, slot), 0); rings[(pan, slot)] = ring + 1
    r = ([0.71, 0.38, 0.1, 0.05] if KIND == "cello" else [0.76, 0.47, 0.2, 0.05])[min(ring, 3)]
    ang = 2 * math.pi * (slot + (0.5 if ring >= 2 else 0)) / slots - math.pi / 2
    size = base * (1 if ring == 0 else 0.78 if ring == 1 else 0.58)
    notes.append((m, pan, r * math.cos(ang), r * math.sin(ang), size))
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
hh = dh - 6.0


def geom(p):
    if npans == 1: return dx + dw * 0.5, dy + hh * 0.5 + 0.6, min(dw, hh) * 0.47
    if npans == 2: return dx + dw * (0.26 if p == 0 else 0.74), dy + hh * 0.5 + 0.6, min(dw / 4, hh / 2) * 0.94
    return dx + dw * (0.17 + 0.33 * p), dy + hh * 0.5 + 0.6 + (-2 if p == 1 else 2), min(dw / 6, hh / 2) * 0.95


g.append('<defs><radialGradient id="pg" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#464C54"/><stop offset="1" stop-color="#1A1C20"/></radialGradient></defs>')
for p in range(npans):
    cx, cy, R = geom(p)
    g.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#pg)" stroke="#A8B0B8" stroke-opacity="0.6" stroke-width="0.4"/>')
ring_glow = {74: 0.95, 81: 0.45, 86: 0.35, 69: 0.25, 79: 0.2, 62: 0.3}
if KIND != "tenor": ring_glow = {62: 0.9, 66: 0.6, 69: 0.55, 57: 0.2}
sel = 74 if KIND == "tenor" else 62
mono = "font-family:'DejaVu Sans Mono',monospace"
devs = [2.1, -3.4, 5.0, -1.2, 0.4, 6.3, -4.8, 1.9, -2.2, 3.3, -0.6, 4.4]
for i, (m, pan, px, py, size) in enumerate(notes):
    cx, cy, R = geom(pan)
    x, y, r = cx + px * R, cy + py * R, size * R
    gl = ring_glow.get(m, 0.0)
    col = f"rgb({int(54 + (255 - 54) * gl)},{int(58 + (196 - 58) * gl)},{int(64 + (64 - 64) * gl)})"
    ang = math.degrees(math.atan2(py, px))
    stroke = "#FFC440" if m == sel else "rgba(168,176,184,0.5)"
    g.append(f'<ellipse cx="{x}" cy="{y}" rx="{r * 1.05}" ry="{r * 0.82}" transform="rotate({ang} {x} {y})" fill="{col}" stroke="{stroke}" stroke-width="{0.55 if m == sel else 0.25}"/>')
    fs = max(1.7, min(2.8, r * 0.7))
    g.append(f'<text x="{x}" y="{y + fs * 0.25}" font-size="{fs}" text-anchor="middle" fill="{"#1E180A" if gl > 0.55 else "#F1E9D6"}" style="{mono}">{PC[m % 12]}</text>')
    d = devs[i % 12]
    bw = max(-1, min(1, d / 30)) * r * 0.75
    g.append(f'<rect x="{min(x, x + bw)}" y="{y + fs * 0.48}" width="{abs(bw)}" height="{max(0.35, fs * 0.14)}" fill="{"#FF785A" if d > 0 else "#6EB4FF"}"/>')
selname = f"{PC[sel % 12]}{sel // 12 - 1}"
g.append(f'<text x="{dx + 1.4}" y="{dy + dh - 1.2}" font-size="2.9" fill="#FFC440" style="{mono}">{tname}  {selname}</text>')
g.append(f'<text x="{dx + dw - 1.4}" y="{dy + dh - 1.2}" font-size="2.9" text-anchor="end" fill="#F1E9D6" style="{mono}">FUND +2.1  OCT -3.4  5TH +7.3 c</text>')

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
