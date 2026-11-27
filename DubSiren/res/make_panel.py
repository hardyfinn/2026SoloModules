#!/usr/bin/env python3
# Draws res/DubSiren.svg, the knob SVGs, and writes src/Layout.hpp
# (every position the C++ needs). Rack can't draw SVG <text>, so labels are paths.
#   pip install fonttools ; python3 res/make_panel.py
# Look (2.2.0): the Jamaican flag (black, green and gold saltire) across the
# whole panel, labels on dark plates, gold and white knob caps, laid out
# symmetrically about the centre line.
import os, sys, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_TITLE = os.path.join(HERE, "..", "tools", "fonts", "RubikMonoOne-Regular.ttf")   # SIL OFL 1.1
BLACK, GREEN, GOLD = "#111111", "#009B3A", "#FED100"
WHITE = "#FFFFFF"; PLATE = "#111111"; STRIP = "#0A0A0A"
CONTROL, JACK, SMALL = 3.2, 2.9, 2.7
H = 128.5
W = 132.08   # 26 HP
CX = W / 2
fonts = {}

def measure(s, size, f=FONT_BOLD, spacing=0.04):
    if f not in fonts:
        t = TTFont(f); fonts[f] = (t.getGlyphSet(), t.getBestCmap(), t["head"].unitsPerEm)
    gs, cmap, upm = fonts[f]
    names = [cmap[ord(c)] for c in s]
    return (sum(gs[n].width + spacing * upm for n in names) - spacing * upm) * size / upm

def text(s, x, y, size, f=FONT_BOLD, fill=WHITE, anchor="middle", spacing=0.04):
    measure(s, size, f, spacing)
    gs, cmap, upm = fonts[f]
    sc = size / upm; tr = spacing * upm
    names = [cmap[ord(c)] for c in s]
    w = sum(gs[n].width + tr for n in names) - tr
    x0 = {"middle": x - w * sc / 2, "end": x - w * sc}.get(anchor, x)
    pen = SVGPathPen(gs); cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (sc, 0, 0, -sc, x0 + cx * sc, y))); cx += gs[n].width + tr
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

def plated(s, x, y, size, fill=WHITE, anchor="middle", f=FONT_BOLD):
    """Label on a dark rounded plate so it reads on any part of the flag."""
    w = measure(s, size, f)
    x0 = {"middle": x - w / 2, "end": x - w}.get(anchor, x)
    pad = 0.9
    r = f'<rect x="{x0 - pad:.3f}" y="{y - size * 0.86:.3f}" width="{w + 2 * pad:.3f}" height="{size * 1.16:.3f}" rx="{size * 0.3:.3f}" fill="{PLATE}" fill-opacity="0.88"/>'
    return r + text(s, x, y, size, f, fill, anchor)

# ------------------------------------------------------------ knob SVGs
def knob_svg(name, d, cap, pointer):
    r = d / 2
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{d}mm" height="{d}mm" viewBox="0 0 {d} {d}">']
    p.append(f'<circle cx="{r}" cy="{r}" r="{r}" fill="#121212"/>')
    n = 32 if d > 10 else 20
    for i in range(n):
        a = 2 * math.pi * i / n
        x1, y1 = r + (r - 0.05) * math.cos(a), r + (r - 0.05) * math.sin(a)
        x2, y2 = r + (r - 1.0) * math.cos(a), r + (r - 1.0) * math.sin(a)
        p.append(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke="#3A3A3A" stroke-width="{0.34 if d > 10 else 0.24}"/>')
    rc = r * 0.74
    p.append(f'<circle cx="{r}" cy="{r}" r="{rc:.3f}" fill="{cap}" stroke="#000000" stroke-opacity="0.35" stroke-width="0.25"/>')
    p.append(f'<circle cx="{r - rc * 0.25:.3f}" cy="{r - rc * 0.3:.3f}" r="{rc * 0.5:.3f}" fill="#FFFFFF" fill-opacity="0.14"/>')
    w = 1.0 if d > 10 else 0.75
    p.append(f'<rect x="{r - w / 2:.3f}" y="{r - rc * 0.95:.3f}" width="{w}" height="{rc * 0.7:.3f}" rx="{w / 2}" fill="{pointer}"/>')
    p.append('</svg>\n')
    open(os.path.join(HERE, name + ".svg"), "w").write("\n".join(p))

KNOBS = {"knob-gold-big": (19.0, GOLD, BLACK), "knob-gold": (16.0, GOLD, BLACK),
         "knob-white": (16.0, WHITE, BLACK), "knob-trim": (8.5, GOLD, BLACK)}
for k, (d, c, ptr) in KNOBS.items():
    knob_svg(k, d, c, ptr)
for old in ("knob-red", "knob-yellow", "knob-green"):
    if os.path.exists(os.path.join(HERE, old + ".svg")): os.remove(os.path.join(HERE, old + ".svg"))

# ------------------------------------------------------------ panel: the flag
p = []
p.append(f'<rect width="{W}" height="{H}" fill="{GREEN}"/>')
p.append(f'<path d="M0 0L{CX:.3f} {H / 2}L0 {H}Z" fill="{BLACK}"/>')
p.append(f'<path d="M{W} 0L{CX:.3f} {H / 2}L{W} {H}Z" fill="{BLACK}"/>')
# gold saltire as two filled bands (the flag's proportion: about 1/6 of the height)
bw = 15.0
L = math.hypot(W, H); ux, uy = W / L, H / L; nx, ny = -uy * bw / 2, ux * bw / 2
def band(x0, y0, x1, y1):
    L = math.hypot(x1 - x0, y1 - y0); ux, uy = (x1 - x0) / L, (y1 - y0) / L
    nx, ny = -uy * bw / 2, ux * bw / 2
    ex, ey = ux * 20, uy * 20
    pts = [(x0 - ex + nx, y0 - ey + ny), (x1 + ex + nx, y1 + ey + ny), (x1 + ex - nx, y1 + ey - ny), (x0 - ex - nx, y0 - ey - ny)]
    return '<path d="M' + "L".join(f"{a:.3f} {b:.3f}" for a, b in pts) + f'Z" fill="{GOLD}"/>'
p.append(band(0, 0, W, H))
p.append(band(W, 0, 0, H))

# title centred on a black bar
tw = measure("DUB SIREN", 6.2, FONT_TITLE, 0.02)
p.append(f'<rect x="{CX - tw / 2 - 3:.3f}" y="4.4" width="{tw + 6:.3f}" height="9.2" rx="1.6" fill="{BLACK}" stroke="{GOLD}" stroke-width="0.5"/>')
p.append(text("DUB SIREN", CX, 11.6, 6.2, FONT_TITLE, WHITE, spacing=0.02))

layout = []
def put(kind, lab, x, y):
    layout.append(f"static const float {kind}_{lab}_X = {x:.3f}f, {kind}_{lab}_Y = {y:.3f}f;")

def knob(lab, key, x, y, kind, scale=None):
    d = KNOBS[kind][0]
    gap = 2.6 if d > 10 else 2.2
    p.append(plated(lab, x, y - d / 2 - gap, CONTROL))
    if scale:
        n = len(scale); R = d / 2 + 3.0
        for i, s in enumerate(scale):
            a = math.radians(-150 + 300 * i / (n - 1))
            sx, sy = x + R * math.sin(a), y - R * math.cos(a) + SMALL * 0.36
            anchor = "middle"
            if abs(math.sin(a)) > 0.4: anchor = "start" if math.sin(a) > 0 else "end"
            if anchor == "start": sx -= 0.9
            if anchor == "end": sx += 0.9
            p.append(plated(s, sx, sy, SMALL, GOLD, anchor))
    put("K", key, x, y)

def switch(lab, key, x, y, up, down):
    p.append(plated(lab, x, y - 6.6, CONTROL))
    p.append(plated(up, x + 3.0, y - 1.3, SMALL, GOLD, "start"))
    p.append(plated(down, x + 3.0, y + 3.7, SMALL, GOLD, "start"))
    put("S", key, x, y)

def button(lab, key, x, y):
    p.append(plated(lab, x, y - 6.4, CONTROL))
    put("B", key, x, y)

def led(lab, key, x, y):
    p.append(plated(lab, x, y - 4.0, JACK, GOLD))
    put("L", key, x, y)

# Columns mirror about the centre line
C3 = [CX - 44.0, CX, CX + 44.0]                     # three knob columns
C4 = [CX - 44.0, CX - 14.667, CX + 14.667, CX + 44.0]  # four evenly spaced
C5 = [CX - 44.0, CX - 22.0, CX, CX + 22.0, CX + 44.0]

# ---- the box's own controls
knob("SPEED", "SPEED", C3[0], 30.0, "knob-gold-big")
knob("FEEDBACK", "FEEDBACK", C3[1], 30.0, "knob-gold")
knob("PITCH", "PITCH", C3[2], 30.0, "knob-gold-big", ["1", "2", "3", "4"])
knob("MODE", "MODE", C3[0], 59.0, "knob-white", ["1", "2", "3", "4"])
knob("BEAT", "BEAT", C3[1], 59.0, "knob-white", ["1", "2", "3", "OFF"])
knob("SIREN VOL", "VOLUME", C3[2], 59.0, "knob-gold")
button("SIREN", "SIREN", C4[0], 83.0)
switch("TRIG", "TRIG", C4[1] - 1.5, 83.0, "ON", "OFF")
led("ACTIVE", "ACTIVE", C4[2], 83.0)
button("TONE", "TONE", C4[3], 83.0)
p.append(f'<rect x="4" y="90.6" width="{W - 8}" height="0.6" fill="{GOLD}"/>')

# ---- extras row, lined up under the knob columns
switch("EXTRAS", "EXTRAS", C5[0] - 1.5, 101.0, "ON", "OFF")
knob("DEPTH", "DEPTH", C5[1], 101.5, "knob-trim")
knob("RELEASE", "RELEASE", C5[2], 101.5, "knob-trim")
knob("TIME", "TIME", C5[3], 101.5, "knob-trim")
switch("ERA", "ERA", C5[4] - 1.5, 101.0, "80s", "90s")

# ---- all nine jacks in one strip, evenly spaced and centred
JY = 119.0
p.append(f'<rect x="2.5" y="{JY - 8.5}" width="{W - 5}" height="13.5" rx="1.4" fill="{STRIP}"/>')
names = ["V/OCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN", "OUT"]
keys = ["VOCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN", "OUT"]
step = 14.25
for i, (lab, key) in enumerate(zip(names, keys)):
    x = CX + (i - 4) * step
    if key == "OUT":
        p.append(f'<rect x="{x - 5.6:.3f}" y="{JY - 7.7}" width="11.2" height="11.9" rx="1.2" fill="{GREEN}"/>')
        p.append(text(lab, x, JY - 5.0, JACK, FONT_BOLD, WHITE))
        put("O", key, x, JY)
    else:
        p.append(text(lab, x, JY - 5.0, JACK, FONT_BOLD, WHITE if i % 2 == 0 else GOLD))
        put("I", key, x, JY)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p) + "\n</svg>\n")
path = os.path.join(HERE, "DubSiren.svg")
open(path, "w").write(svg)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(path, x=CX, y=126.0, cap=3.0, bg=GREEN, ink=WHITE)

open(os.path.join(HERE, "..", "src", "Layout.hpp"), "w").write(
    "// Generated by res/make_panel.py - do not edit by hand.\n#pragma once\nnamespace layout {\n" + "\n".join(layout) + "\n}\n")
print("ok")
