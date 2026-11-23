#!/usr/bin/env python3
# Draws res/Rimbaud.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Rimbaud.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#ECE6D8"       # coated head
BAND = "#E1D9C8"
INK = "#1D1D1F"
SOFT = "#6B655A"
RED = "#B3261E"
JACKBG = "#24221F"

# ---- LAYOUT (same numbers in src/Rimbaud.cpp) ----
COL = [16.2, 40.6, 65.0, 89.4, 113.8, 138.2]
ROW_DRUM = 31.0
ROW_STROKE = 53.0
ROW_SNARE = 75.0
ROW_CV = 96.0
ROW_IO = 115.0
IOX = [11.0, 26.8, 42.6, 58.4, 74.2, 90.0, 109.0, 124.8, 140.6]
HIT_X, HIT_Y = 138.2, 10.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5     # range marks

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

# ---- title
add(text("Rimbaud", 6.0, 12.2, 6.4, FONT_BOLD, INK, anchor="start", spacing=0.01))
add(text("snare drum · rim · wires", 6.2, 17.0, 2.6, FONT_MED, RED, anchor="start"))
add(text("HIT", HIT_X - 7.0, HIT_Y + 1.1, CONTROL, anchor="end"))


def band(y0, y1, name):
    add(f'<rect x="0" y="{y0}" width="{W}" height="{y1 - y0}" fill="{BAND}"/>')
    # section name, vertical, on the left edge
    add(f'<g transform="rotate(-90 3.6 {(y0 + y1) / 2})">'
        + text(name, 3.6, (y0 + y1) / 2 + 0.95, 2.6, FONT_BOLD, RED) + '</g>')


band(20.0, 41.6, "DRUM")
add(f'<line x1="0" y1="42.3" x2="{W}" y2="42.3" stroke="{BG}" stroke-width="0.8"/>')
band(42.4, 63.6, "STROKE")
band(64.4, 85.6, "SNARES")


def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


def row(y, names, marks):
    for x, n in zip(COL, names):
        if n:
            add(text(n, x, y - 7.4, CONTROL))
    for x, m in zip(COL, marks):
        if m:
            ranges(x, y + 7.6, 2.2, m[0], m[1])


row(ROW_DRUM, ["SIZE", "DEPTH", "TUNE", "RESO", "DAMP", "SHELL"],
    [("10\"", "15\""), ("3\"", "8\""), ("LOW", "HIGH"), ("×0.8", "×2.2"), ("OPEN", "DEAD"), ("WOOD", "METAL")])
row(ROW_STROKE, ["POSITION", "STICK", "BUZZ", "RIM", "HOOP", "XSTICK"],
    [("CTR", "EDGE"), ("TIP", "FELT"), ("OPEN", "PRESS"), ("HEAD", "HOOP"), ("STEEL", "CAST"), ("CLICK", "KNOCK")])
row(ROW_SNARE, ["", "TENSION", "WIRES", "SYMP", "MIC", "LEVEL"],
    [None, ("LOOSE", "TIGHT"), ("12", "42"), None, ("TOP", "BTM"), None])
# snares on/off switch
add(text("SNARES", COL[0], ROW_SNARE - 7.4, CONTROL, fill=RED))
add(text("ON", COL[0] + 3.4, ROW_SNARE - 1.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("OFF", COL[0] + 3.4, ROW_SNARE + 4.0, SMALL, FONT_BOLD, SOFT, anchor="start"))

# ---- CV row
add(f'<rect x="2.0" y="87.0" width="{W - 4.0}" height="16.0" rx="1.4" fill="{BAND}"/>')
for x, n in zip(COL, ["V/OCT", "POS", "STICK", "BUZZ", "TENSION", "DAMP"]):
    add(text(n, x, ROW_CV - 5.2, JACK, FONT_BOLD, SOFT))

# ---- inputs / outputs
for x, n in zip(IOX[:6], ["HIT", "RIM", "XSTICK", "VEL", "SNARES", "EXT"]):
    add(text(n, x, ROW_IO - 5.2, JACK, fill=RED if n in ("HIT", "RIM", "XSTICK") else INK))
ox0, ox1 = IOX[6] - 7.0, IOX[8] + 7.0
add(f'<rect x="{ox0}" y="{ROW_IO - 9.2}" width="{ox1 - ox0}" height="14.6" rx="1.4" fill="{JACKBG}"/>')
for x, n in zip(IOX[6:], ["TOP", "BTM", "MIX"]):
    add(text(n, x, ROW_IO - 5.2, JACK, fill=BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Rimbaud.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Rimbaud.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
