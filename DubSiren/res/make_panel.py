#!/usr/bin/env python3
# Draws res/DubSiren.svg, the knob SVGs, and writes src/Layout.hpp
# (every position the C++ needs). Rack can't draw SVG <text>, so labels are paths.
#   pip install fonttools ; python3 res/make_panel.py
# Look: brushed aluminium box with red / yellow / green knob caps, like the
# hardware siren it is modelled on.
import os, sys, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_BLACK = os.environ.get("PANEL_FONT_BLACK", "/usr/share/fonts/opentype/inter/Inter-Black.otf")
BG = "#C4C6C3"; INK = "#1B1B1B"; SOFT = "#3A3C3B"; STRIP = "#232425"; CREAM = "#F1EEE4"
RED = "#C8282A"; YELLOW = "#EFD84A"; GREEN = "#3E8A44"
CONTROL, JACK, SMALL = 3.2, 2.9, 2.7
H = 128.5
W = 121.92   # 24 HP
fonts = {}

def text(s, x, y, size, f=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    if f not in fonts:
        t = TTFont(f); fonts[f] = (t.getGlyphSet(), t.getBestCmap(), t["head"].unitsPerEm)
    gs, cmap, upm = fonts[f]
    sc = size / upm; tr = spacing * upm
    names = [cmap[ord(c)] for c in s]
    w = sum(gs[n].width + tr for n in names) - tr
    x0 = {"middle": x - w * sc / 2, "end": x - w * sc}.get(anchor, x)
    pen = SVGPathPen(gs); cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (sc, 0, 0, -sc, x0 + cx * sc, y))); cx += gs[n].width + tr
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

# ------------------------------------------------------------ knob SVGs
def knob_svg(name, d, cap):
    r = d / 2
    p = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{d}mm" height="{d}mm" viewBox="0 0 {d} {d}">']
    p.append(f'<circle cx="{r}" cy="{r}" r="{r}" fill="#121212"/>')
    # knurled skirt
    n = 28 if d > 10 else 18
    for i in range(n):
        a = 2 * math.pi * i / n
        x1, y1 = r + (r - 0.05) * math.cos(a), r + (r - 0.05) * math.sin(a)
        x2, y2 = r + (r - 0.9) * math.cos(a), r + (r - 0.9) * math.sin(a)
        p.append(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke="#3A3A3A" stroke-width="{0.32 if d > 10 else 0.22}"/>')
    rc = r * 0.74
    p.append(f'<circle cx="{r}" cy="{r}" r="{rc:.3f}" fill="{cap}" stroke="#000000" stroke-opacity="0.35" stroke-width="0.25"/>')
    p.append(f'<circle cx="{r - rc * 0.25:.3f}" cy="{r - rc * 0.3:.3f}" r="{rc * 0.5:.3f}" fill="#FFFFFF" fill-opacity="0.12"/>')
    w = 0.9 if d > 10 else 0.7
    p.append(f'<rect x="{r - w / 2:.3f}" y="{r - rc * 0.95:.3f}" width="{w}" height="{rc * 0.7:.3f}" rx="{w / 2}" fill="#FFFFFF"/>')
    p.append('</svg>\n')
    open(os.path.join(HERE, name + ".svg"), "w").write("\n".join(p))

KNOBS = {"knob-red": (15.0, RED), "knob-yellow": (12.0, YELLOW), "knob-green": (12.0, GREEN), "knob-trim": (7.0, YELLOW)}
for k, (d, c) in KNOBS.items():
    knob_svg(k, d, c)

# ------------------------------------------------------------ panel
p = []
p.append(f'<defs><linearGradient id="al" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="{W}" y2="{H}">'
         f'<stop offset="0" stop-color="#D2D4D1"/><stop offset="0.5" stop-color="#B9BBB8"/><stop offset="1" stop-color="#CACCC9"/></linearGradient></defs>')
p.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
p.append(f'<rect width="{W}" height="{H}" fill="url(#al)"/>')
# fine brushed lines
for i in range(0, 60):
    y = 2.0 + i * 2.1
    op = 0.05 + 0.04 * ((i * 7) % 5) / 4
    p.append(f'<rect x="0" y="{y:.2f}" width="{W}" height="0.18" fill="#FFFFFF" fill-opacity="{op:.3f}"/>')
p.append(text("DUB SIREN", 5.0, 11.2, 6.4, FONT_BLACK, INK, anchor="start", spacing=0.03))
p.append(text("sound system siren", W - 5.0, 10.8, 2.7, FONT_BOLD, SOFT, anchor="end"))

layout = []
def put(kind, lab, x, y):
    layout.append(f"static const float {kind}_{lab}_X = {x}f, {kind}_{lab}_Y = {y}f;")

def knob(lab, key, x, y, kind, scale=None):
    d = KNOBS[kind][0]
    gap = {15.0: 2.6, 12.0: 2.6, 7.0: 2.2}[d]
    p.append(text(lab, x, y - d / 2 - gap, CONTROL))
    if scale:
        n = len(scale); R = d / 2 + 2.9
        for i, s in enumerate(scale):
            a = math.radians(-150 + 300 * i / (n - 1))
            sx, sy = x + R * math.sin(a), y - R * math.cos(a) + SMALL * 0.36
            anchor = "middle"
            if abs(math.sin(a)) > 0.4: anchor = "start" if math.sin(a) > 0 else "end"
            if anchor == "start": sx -= 0.9
            if anchor == "end": sx += 0.9
            p.append(text(s, sx, sy, SMALL, FONT_BOLD, SOFT, anchor=anchor))
    put("K", key, x, y)

def switch(lab, key, x, y, up, down):
    p.append(text(lab, x, y - 6.6, CONTROL))
    p.append(text(up, x + 3.0, y - 1.3, SMALL, FONT_BOLD, SOFT, anchor="start"))
    p.append(text(down, x + 3.0, y + 3.7, SMALL, FONT_BOLD, SOFT, anchor="start"))
    put("S", key, x, y)

def button(lab, key, x, y):
    p.append(text(lab, x, y - 6.2, CONTROL))
    put("B", key, x, y)

def led(lab, key, x, y):
    p.append(text(lab, x, y - 3.6, JACK, FONT_BOLD, SOFT))
    put("L", key, x, y)

# row 1
knob("SPEED", "SPEED", 30.0, 29.0, "knob-red")
knob("PITCH", "PITCH", 92.0, 29.0, "knob-red", ["1", "2", "3", "4"])
led("ACTIVE", "ACTIVE", 61.0, 29.0)
# row 2
knob("FEEDBACK", "FEEDBACK", 30.0, 55.0, "knob-yellow")
knob("MODE", "MODE", 92.0, 55.0, "knob-yellow", ["1", "2", "3", "4"])
# row 3
switch("TRIG", "TRIG", 12.0, 80.0, "ON", "OFF")
button("SIREN", "SIREN", 33.0, 80.0)
button("TONE", "TONE", 52.0, 80.0)
knob("BEAT", "BEAT", 92.0, 80.0, "knob-yellow", ["1", "2", "3", "OFF"])
# row 4
knob("DEPTH", "DEPTH", 11.0, 102.0, "knob-trim")
knob("RELEASE", "RELEASE", 29.0, 102.0, "knob-trim")
knob("TIME", "TIME", 47.0, 102.0, "knob-trim")
switch("ERA", "ERA", 63.0, 102.0, "80s", "90s")
knob("SIREN VOL", "VOLUME", 97.0, 102.0, "knob-green")

# jack strip
JY = 118.2
p.append(f'<rect x="2.5" y="{JY - 8.7}" width="{W - 5}" height="13.6" rx="1.4" fill="{STRIP}"/>')
jacks = ["V/OCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN"]
keys = ["VOCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN"]
step = (W - 2 * 8.0) / 8
for i, (lab, key) in enumerate(zip(jacks, keys)):
    x = round(8.0 + i * step, 3)
    p.append(text(lab, x, JY - 5.0, JACK, FONT_BOLD, CREAM))
    put("I", key, x, JY)
ox = round(8.0 + 8 * step, 3)
p.append(f'<rect x="{ox - 5.6}" y="{JY - 8.0}" width="11.2" height="12.2" rx="1.2" fill="{RED}"/>')
p.append(text("OUT", ox, JY - 5.0, JACK, FONT_BOLD, CREAM))
put("O", "OUT", ox, JY)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p) + "\n</svg>\n")
path = os.path.join(HERE, "DubSiren.svg")
open(path, "w").write(svg)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=BG)

open(os.path.join(HERE, "..", "src", "Layout.hpp"), "w").write(
    "// Generated by res/make_panel.py - do not edit by hand.\n#pragma once\nnamespace layout {\n" + "\n".join(layout) + "\n}\n")
print("ok")
