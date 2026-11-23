#!/usr/bin/env python3
# Draws res/Joanna.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Joanna.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 172.72, 128.5  # 34 HP

BG = "#EFE8D6"       # old ivory
BAND = "#E2D8C1"
INK = "#1E1B18"
SOFT = "#6E6457"
WOOD = "#5A2D1C"     # walnut
JACKBG = "#2A211C"
BRASS = "#C9A961"

# ---- LAYOUT (same numbers in src/Joanna.cpp) ----
COL = [16.0, 44.0, 72.0, 100.0, 128.0, 156.0]
ROW_PIANO = 32.0
ROW_LIFE = 54.0
ROW_HOUSE = 76.0
ROW_PEDAL = 96.0
ROW_IO = 115.0
IOX = [10.0, 27.0, 44.0, 61.0, 78.0, 95.0, 122.0, 140.0, 158.0]
PLATE_X, PLATE_Y, PLATE_W, PLATE_H = 128.0, 6.2, 36.0, 9.0

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

# ---- title, and the maker's plate where the serial number shows
add(text("Joanna", 6.0, 12.4, 6.6, FONT_BOLD, INK, anchor="start", spacing=0.01))
add(text("upright piano · the room it lives in", 6.2, 17.2, 2.6, FONT_MED, WOOD, anchor="start"))
add(f'<rect x="{PLATE_X}" y="{PLATE_Y}" width="{PLATE_W}" height="{PLATE_H}" rx="1.2" fill="{BRASS}"/>')
add(f'<rect x="{PLATE_X + 0.7}" y="{PLATE_Y + 0.7}" width="{PLATE_W - 1.4}" height="{PLATE_H - 1.4}" rx="0.8" fill="none" stroke="{WOOD}" stroke-width="0.25"/>')
add(text("PIANO", PLATE_X - 1.6, PLATE_Y + 5.6, SMALL, FONT_BOLD, SOFT, anchor="end"))


def band(y0, y1, name):
    add(f'<rect x="0" y="{y0}" width="{W}" height="{y1 - y0}" fill="{BAND}"/>')
    add(f'<g transform="rotate(-90 3.6 {(y0 + y1) / 2})">'
        + text(name, 3.6, (y0 + y1) / 2 + 0.95, 2.6, FONT_BOLD, WOOD) + '</g>')


band(21.0, 42.6, "PIANO")
band(43.4, 64.6, "LIFE")
band(65.4, 86.6, "HOUSE")


def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


def row(y, names, marks, cols=COL):
    for x, n in zip(cols, names):
        if n:
            add(text(n, x, y - 7.4, CONTROL))
    for x, m in zip(cols, marks):
        if m:
            ranges(x, y + 7.6, 2.2, m[0], m[1])


row(ROW_PIANO, ["SIZE", "HAMMER", "TOUCH", "SUSTAIN", "SYMP", "NOISE"],
    [("SPINET", "TALL"), ("SOFT", "TACK"), ("HEAVY", "LIGHT"), ("SHORT", "LONG"), None, ("QUIET", "LOUD")])
row(ROW_LIFE, ["TUNING", "UNISON", "WEAR", "AGE", "FINE", "ANOTHER"],
    [("TODAY", "20 YRS"), ("0", "25c"), ("NEW", "PLAYED"), ("NEW", "OLD"), ("-50c", "+50c"), None])
row(ROW_HOUSE, ["LID", "WALL", "ROOM", "FURNISH", "DISTANCE", "LEVEL"],
    [None, ("3cm", "60cm"), ("BOX", "HALL"), ("SOFT", "BARE"), ("IN", "NEXT"), None])
# lid switch positions (up = front panel off, middle = top open, down = shut)
add(text("FRONT", COL[0] + 3.6, ROW_HOUSE - 3.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("OPEN", COL[0] + 3.6, ROW_HOUSE + 0.9, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("SHUT", COL[0] + 3.6, ROW_HOUSE + 4.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("new piano", COL[5], ROW_LIFE + 7.6, SMALL, FONT_BOLD, SOFT))

# ---- pedals (left) and CV for the knobs (right)
add(f'<rect x="2.0" y="88.2" width="{W - 4.0}" height="15.4" rx="1.4" fill="{BAND}"/>')
for x, n in zip(COL[:3], ["SUSTAIN", "SOFT", "FELT"]):
    add(text(n, x, ROW_PEDAL - 4.6, CONTROL, fill=WOOD))
for x, n in zip(COL[3:], ["HAMMER", "TUNING", "DIST"]):
    add(text(n, x, ROW_PEDAL - 4.6, JACK, FONT_BOLD, SOFT))
add(text("PEDALS", (COL[0] + COL[2]) / 2, ROW_PEDAL + 6.2, SMALL, FONT_BOLD, SOFT))
add(text("CV ±5V", (COL[3] + COL[5]) / 2, ROW_PEDAL + 6.2, SMALL, FONT_BOLD, SOFT))

# ---- inputs / outputs
for x, n in zip(IOX[:6], ["V/OCT", "GATE", "VEL", "SUS", "SOFT", "EXT"]):
    add(text(n, x, ROW_IO - 5.2, JACK, fill=WOOD if n in ("V/OCT", "GATE", "VEL") else INK))
ox0, ox1 = IOX[6] - 8.0, IOX[8] + 8.0
add(f'<rect x="{ox0}" y="{ROW_IO - 9.2}" width="{ox1 - ox0}" height="14.6" rx="1.4" fill="{JACKBG}"/>')
for x, n in zip(IOX[6:], ["L", "R", "DRY"]):
    add(text(n, x, ROW_IO - 5.2, JACK, fill=BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Joanna.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Joanna.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
