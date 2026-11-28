#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Keel.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 152.4, 128.5
PX = 25.4 / 75.0
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


R1, R2, IN, OUTY = L["ROW1_Y"], L["ROW2_Y"], L["IN_Y"], L["OUT_Y"]
# row 1: ROOT (A), SCALE (minor), FOLLOW (chord, up), OCT, FINE, GLIDE, KICK 49.5, LEARN
knob("RoundBlackKnob", colx(0), R1, 9 / 11)
knob("RoundBlackKnob", colx(1), R1, 0.0)
comp("CKSS_1", colx(2) - 4.5, R1)
knob("RoundBlackKnob", colx(3), R1, 0.5)
knob("RoundBlackKnob", colx(4), R1, 0.5)
knob("RoundBlackKnob", colx(5), R1, 0.1)
import math
knob("RoundBlackKnob", colx(6), R1, (math.log2(49.5) - math.log2(20)) / (math.log2(200) - math.log2(20)))
bezel(colx(7), R1, "#FFB341", 0.0)
# row 2: STEPS 4, MUTATE, SPREAD, LOCK | XOVER, REPLACE, DROP, DRIVE
knob("RoundBlackKnob", colx(0), R2, 3 / 15)
knob("RoundBlackKnob", colx(1), R2, 0.35)
knob("RoundBlackKnob", colx(2), R2, 0.5)
bezel(colx(3), R2, "#FF4D5E", 0.0)
knob("RoundBlackKnob", colx(4), R2, (math.log2(120) - math.log2(60)) / (math.log2(300) - math.log2(60)))
knob("RoundBlackKnob", colx(5), R2, 1.0)
knob("RoundBlackKnob", colx(6), R2, 0.25)
knob("RoundBlackKnob", colx(7), R2, 0.2)
for i in range(8):
    comp("PJ301M", colx(i), IN)
for i in (0, 1, 2, 6, 7):
    comp("PJ301M", colx(i), OUTY)
comp("CKSS_1", colx(3) - 2.0, OUTY)
knob("RoundSmallBlackKnob", colx(5), OUTY + 1.0, 0.5)
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# ---- sample screen, same layout as KeelDisplay
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
mono = "font-family:'DejaVu Sans Mono',monospace;font-weight:bold"
TEAL, CREAM, CORAL, AMBER, GREEN, SOFT = "#3CCFB4", "#EFE8D8", "#FF6B4A", "#FFB341", "#7DE27F", "#A3B0B6"


def t(x, y, s, size, col, anchor="start"):
    g.append(f'<text x="{dx + x}" y="{dy + y}" font-size="{size}" fill="{col}" text-anchor="{anchor}" style="{mono}">{s}</text>')


t(1.6, 3.9, "KEY A MINOR  CHORD F", 2.5, TEAL)
t(dw - 1.6, 3.9, "KICK &gt; F1  43.7 Hz  V/OCT -0.18", 2.5, CREAM, "end")
t(1.6, 7.9, "HEARD 43.6 Hz  F1 -3 c", 2.5, AMBER)
t(dw - 1.6, 7.9, "IN TUNE  AUTO -14 c", 2.5, GREEN, "end")
pat = ["1", "5", "1", "8"]
hgt = {"1": 0, "5": 0.7, "4": 0.55, "8": 1.0, "3": 0.4, "7": 0.85}
gx0, gy0 = 1.6, 10.2
cw = (dw - 2 * gx0) / 16
for k in range(16):
    x = dx + gx0 + k * cw + 0.25
    w = cw - 0.5
    y = dy + gy0
    h = dh - gy0 - 1.3
    on = k < len(pat)
    g.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="0.5" fill="#263238" fill-opacity="{0.63 if on else 0.2}"/>')
    lab = pat[k] if on else "1"
    if on and hgt[lab]:
        hh = (h - 1.2) * hgt[lab]
        g.append(f'<rect x="{x + 0.4}" y="{y + h - 0.6 - hh}" width="{w - 0.8}" height="{hh}" fill="{CORAL}" fill-opacity="0.27"/>')
    g.append(f'<text x="{x + w / 2}" y="{y + h / 2 + 1.2}" font-size="3.4" fill="{CREAM if on else SOFT}" '
             f'fill-opacity="{1 if on else 0.25}" text-anchor="middle" style="{mono}">{lab}</text>')
    if k == 2:
        g.append(f'<rect x="{x - 0.15}" y="{y - 0.15}" width="{w + 0.3}" height="{h + 0.3}" rx="0.6" fill="none" stroke="#FFFFFF" stroke-width="0.35"/>')

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
