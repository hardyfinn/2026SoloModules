#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary] [pattern] [title]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
PAT = sys.argv[3] if len(sys.argv) > 3 else "X-xxX-xxX-x-X-xr--x-X-xxX-xrX-xx"
TITLE = sys.argv[4] if len(sys.argv) > 4 else "TECHNO  Rumble kick  (mutated)"
svg = open(os.path.join(HERE, "..", "res", "Tiers.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 152.4, 128.5
PX = 25.4 / 75.0
LEVEL_COL = ["#FF6B4A", "#FFB341", "#E8DC5A", "#7DE2C3", "#78A9FF"]
g = []
colx = lambda i: L["COL0"] + L["COLDX"] * i


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


def bezel(x, y, glow=None, a=0.0):
    comp("VCVBezel", x, y)
    if glow and a:
        g.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="{glow}" fill-opacity="{a}"/>')


# weights, HITS, ACCENT, ROTATE
for i, v in enumerate([1.0, 0.75, 0.5, 0.25, 0.12, 9 / 32, 0.5, 0.0]):
    knob("RoundBlackKnob", colx(i), L["KNOB_Y"], v)
for i in range(8):
    comp("PJ301M", colx(i), L["CV_Y"])
bezel(colx(0), L["ROW3_Y"], "#FF6B4A", 0.0)
knob("RoundBlackKnob", colx(1), L["ROW3_Y"], 0.35)
bezel(colx(2), L["ROW3_Y"], "#FFB341", 0.0)
bezel(colx(3), L["ROW3_Y"])
knob("RoundBlackKnob", colx(4), L["ROW3_Y"], 11 / 40)
bezel(colx(5), L["ROW3_Y"], "#FF6B4A", 0.0)
comp("CKSS_1", colx(6) - 2.0, L["ROW3_Y"]); comp("CKSS_1", colx(7) - 2.0, L["ROW3_Y"])
for i in range(8):
    comp("PJ301M", colx(i), L["IN_Y"])
knob("RoundSmallBlackKnob", colx(0), L["OUT_Y"], 0.3)
comp("CKSS_0", colx(1) - 2.0, L["OUT_Y"])
for i in range(6):
    comp("PJ301M", colx(2 + i), L["OUT_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# ---- sample screen, same layout as TiersDisplay
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
mono = "font-family:'DejaVu Sans Mono',monospace;font-weight:bold"
CO, CR, SO, DIM = "#FF6B4A", "#F1E9D6", "#B8BCC8", "#2A3240"


def t(x, y, s, size, col, anchor="start"):
    g.append(f'<text x="{dx + x}" y="{dy + y}" font-size="{size}" fill="{col}" text-anchor="{anchor}" style="{mono}">{s}</text>')


def level16(i):
    p = i % 16
    return 0 if p % 8 == 0 else 1 if p % 4 == 0 else 2 if p % 2 == 0 else 3


n = len(PAT)
hits = sum(c != "-" for c in PAT)
t(1.6, 3.9, f"CHANCE  {n} STEPS  1/16  {hits} HITS", 2.5, SO)
t(dw - 1.6, 3.9, TITLE, 2.5, CO, "end")
rows = 1 if n == 16 else 2
gx0, gy0, gw, gh = 1.6, 5.6, dw - 3.2, dh - 7.0
cw = gw / 16
rh = gh / rows
play = 6
for k in range(n):
    r, c = divmod(k, 16)
    x = dx + gx0 + c * cw + 0.3
    y = dy + gy0 + r * rh + 0.3
    w = cw - 0.6
    h = rh - 0.6
    lv = level16(k)
    g.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="0.5" fill="{DIM}" fill-opacity="0.55"/>')
    g.append(f'<rect x="{x}" y="{y + h - 0.9}" width="{w}" height="0.9" fill="{LEVEL_COL[lv]}" fill-opacity="0.9"/>')
    ch = PAT[k]
    if ch != "-":
        col = CO if ch in "XR" else CR
        hh = (h - 1.4) if ch in "XR" else (h - 1.4) * 0.62
        yy = y + h - 1.2 - hh
        if ch in "rR":
            g.append(f'<rect x="{x + 0.5}" y="{yy}" width="{w / 2 - 0.8}" height="{hh}" rx="0.3" fill="{col}"/>')
            g.append(f'<rect x="{x + w / 2 + 0.3}" y="{yy}" width="{w / 2 - 0.8}" height="{hh}" rx="0.3" fill="{col}"/>')
        else:
            g.append(f'<rect x="{x + 0.5}" y="{yy}" width="{w - 1.0}" height="{hh}" rx="0.3" fill="{col}"/>')
    if k == play:
        g.append(f'<rect x="{x - 0.15}" y="{y - 0.15}" width="{w + 0.3}" height="{h + 0.3}" rx="0.6" fill="none" stroke="#FFFFFF" stroke-width="0.35"/>')
    if c % 4 == 0:
        t(gx0 + c * cw + 0.9, gy0 + r * rh + 2.4, str(c // 4 + 1 + (4 if r else 0)), 1.9, SO)

svg = svg.replace("</svg>", "\n".join(x for x in g if x) + "\n</svg>")
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
