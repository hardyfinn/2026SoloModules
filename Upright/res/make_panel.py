#!/usr/bin/env python3
# Draws res/Upright.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT numbers are shared with src/Upright.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 60.96, 128.5  # 12 HP

BG = "#F1ECE0"       # ivory key
INK = "#161412"      # lacquer black
SOFT = "#6B655B"
BAND = "#1D1A18"
FELT = "#8E2A2A"     # damper felt red

# ---- LAYOUT (same numbers in src/Upright.cpp) ----
CX = [16.0, 45.0]
ROW1, ROW2 = 39.0, 62.0
IO1, IO2 = 88.0, 108.0
IOX = [11.0, 30.5, 50.0]
READY_X, READY_Y = 52.5, 10.6

CONTROL = 3.2
JACK = 2.9
SMALL = 2.5

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    x0 = {"middle": x - width * scale / 2, "end": x - width * scale}.get(anchor, x)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y)))
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Upright", 5.0, 13.0, 6.4, FONT_BOLD, INK, anchor="start", spacing=0.01))
add(text("sampled upright piano", 5.2, 17.8, 2.5, FONT_MED, SOFT, anchor="start"))
add(text("READY", READY_X, READY_Y + 4.6, SMALL, FONT_BOLD, SOFT))
# a strip of keys under the title
for i in range(14):
    add(f'<rect x="{3.0 + i * 3.97}" y="20.6" width="3.6" height="4.2" rx="0.3" fill="#FFFFFF" stroke="{SOFT}" stroke-width="0.15"/>')
for i in [0, 1, 3, 4, 5, 7, 8, 10, 11, 12]:
    add(f'<rect x="{3.0 + i * 3.97 + 2.5}" y="20.6" width="2.3" height="2.6" fill="{INK}"/>')


def knob(x, y, name, lo, hi):
    add(text(name, x, y - 8.2, CONTROL))
    if lo: add(text(lo, x - 4.2, y + 8.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
    if hi: add(text(hi, x + 4.2, y + 8.6, SMALL, FONT_BOLD, SOFT, anchor="start"))


knob(CX[0], ROW1, "TONE", "DARK", "BRIGHT")
knob(CX[1], ROW1, "RELEASE", "DRY", "LONG")
add(text("PEDAL", CX[0], ROW2 - 8.2, CONTROL, fill=FELT))
add(text("sustain", CX[0], ROW2 + 8.6, SMALL, FONT_BOLD, SOFT))
knob(CX[1], ROW2, "VOLUME", None, None)

# ---- jacks
add(f'<rect x="2.0" y="{IO1 - 9.6}" width="{W - 4.0}" height="36.4" rx="1.4" fill="#E4DDCD"/>')
for x, n in zip(IOX, ["V/OCT", "GATE", "VEL"]):
    add(text(n, x, IO1 - 5.4, JACK))
add(text("SUS", IOX[0], IO2 - 5.4, JACK, fill=FELT))
ox0, ox1 = IOX[1] - 7.6, IOX[2] + 7.6
add(f'<rect x="{ox0}" y="{IO2 - 9.4}" width="{ox1 - ox0}" height="14.6" rx="1.4" fill="{BAND}"/>')
for x, n in zip(IOX[1:], ["L", "R"]):
    add(text(n, x, IO2 - 5.4, JACK, fill=BG))
add(text("poly 16", IOX[1], IO1 + 7.4, SMALL, FONT_BOLD, SOFT))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Upright.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Upright.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
