#!/usr/bin/env python3
# Draws res/Keel.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Keel.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#141B1F"
BG_DARK = "#0D1215"
LINE = "#2A363C"
INK = "#EFE8D8"
SOFT = "#A3B0B6"
TEAL = "#3CCFB4"     # key + tuning
CORAL = "#FF6B4A"    # pattern
AMBER = "#FFB341"    # meter
VIOLET = "#A497FF"   # undertone

# ---- LAYOUT (same numbers in src/Keel.cpp)
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 13.5, 140.4, 23.0
COL0, COLDX = 18.0, 16.6
GUT_X = 7.4
ROW1_Y, ROW2_Y = 52.0, 76.0
IN_Y, OUT_Y = 98.5, 116.5

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5     # switch positions, units


def colx(i):
    return COL0 + COLDX * i


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

# ---- title + screen
add(text("Keel", 6.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("kick drum tuning", W - 6.0, 10.2, 2.8, FONT_MED, TEAL, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#05080A" stroke="{TEAL}" stroke-opacity="0.5" stroke-width="0.35"/>')


def header(s, c0, c1, y, col):
    xa, xb = colx(c0) - 6.5, colx(c1) + 6.5
    add(text(s, (xa + xb) / 2, y, 2.6, FONT_BOLD, col, spacing=0.12))
    add(f'<line x1="{xa}" y1="{y + 1.1}" x2="{xb}" y2="{y + 1.1}" stroke="{col}" stroke-opacity="0.55" stroke-width="0.35"/>')


def switch_labels(x, y, title, opts, col=INK, title_x=None):
    add(text(title, x if title_x is None else title_x, y - 6.6, CONTROL, fill=col))
    for yy, o in zip([y - 1.4, y + 3.6], opts):
        add(text(o, x + 2.6, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))


# ---- row 1: key + tuning
header("KEY", 0, 2, ROW1_Y - 12.0, TEAL)
header("TUNING", 3, 7, ROW1_Y - 12.0, TEAL)
for i, n in enumerate(["ROOT", "SCALE"]):
    add(text(n, colx(i), ROW1_Y - 6.2, CONTROL))
switch_labels(colx(2) - 4.5, ROW1_Y, "FOLLOW", ["CHORD", "KEY"], title_x=colx(2) + 0.5)
for i, n in enumerate(["OCT", "FINE", "GLIDE", "KICK"]):
    add(text(n, colx(3 + i), ROW1_Y - 6.2, CONTROL))
add(text("LEARN", colx(7), ROW1_Y - 6.2, CONTROL, fill=AMBER))
add(text("own pitch", colx(6), ROW1_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("from meter", colx(7), ROW1_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("cents", colx(4), ROW1_Y + 7.0, SMALL, FONT_MED, SOFT))

add(f'<line x1="4" y1="{ROW1_Y + 9.6}" x2="{W - 4}" y2="{ROW1_Y + 9.6}" stroke="{LINE}" stroke-width="0.4"/>')

# ---- row 2: pattern + undertone
header("PATTERN", 0, 3, ROW2_Y - 12.0, CORAL)
header("UNDERTONE", 4, 7, ROW2_Y - 12.0, VIOLET)
for i, n in enumerate(["STEPS", "MUTATE", "SPREAD"]):
    add(text(n, colx(i), ROW2_Y - 6.2, CONTROL))
add(text("LOCK", colx(3), ROW2_Y - 6.2, CONTROL, fill=CORAL))
for i, n in enumerate(["XOVER", "REPLACE", "DROP", "DRIVE"]):
    add(text(n, colx(4 + i), ROW2_Y - 6.2, CONTROL))
add(text("1-16", colx(0), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("each loop", colx(1), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("1 5 > all", colx(2), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("Hz", colx(4), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("sub", colx(5), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("sigh", colx(6), ROW2_Y + 7.0, SMALL, FONT_MED, SOFT))
add(f'<line x1="{(colx(3) + colx(4)) / 2}" y1="{ROW2_Y - 13}" x2="{(colx(3) + colx(4)) / 2}" y2="{ROW2_Y + 8}" '
    f'stroke="{LINE}" stroke-width="0.4"/>')
add(f'<line x1="{(colx(2) + colx(3)) / 2}" y1="{ROW1_Y - 13}" x2="{(colx(2) + colx(3)) / 2}" y2="{ROW1_Y + 8}" '
    f'stroke="{LINE}" stroke-width="0.4"/>')

# ---- inputs
add(f'<rect x="{colx(0) - 7.0}" y="{IN_Y - 8.6}" width="{COLDX * 7 + 14.0}" height="13.2" rx="1.4" fill="{BG_DARK}"/>')
in_cols = [TEAL, TEAL, TEAL, INK, AMBER, CORAL, CORAL, AMBER]
for i, n in enumerate(["KEY", "CHORD", "BAR", "TRIG", "AUDIO", "MUTATE", "LOCK", "LEARN"]):
    add(text(n, colx(i), IN_Y - 5.0, JACK, fill=in_cols[i]))
add(text("IN", GUT_X - 0.6, IN_Y + 1.0, JACK, fill=SOFT))

# ---- outputs: tuning plate, AUTO switch, undertone level + plate
add(f'<rect x="{colx(0) - 7.4}" y="{OUT_Y - 8.4}" width="{COLDX * 2 + 14.8}" height="13.0" rx="1.4" fill="{TEAL}"/>')
for i, n in enumerate(["V/OCT", "PITCH", "IN TUNE"]):
    add(text(n, colx(i), OUT_Y - 4.9, JACK, fill=BG))
switch_labels(colx(3) - 2.0, OUT_Y, "AUTO", ["ON", "OFF"], AMBER)
add(text("SUB LVL", colx(5), OUT_Y - 6.2, CONTROL))
add(f'<rect x="{colx(6) - 7.4}" y="{OUT_Y - 8.4}" width="{COLDX * 1 + 14.8}" height="13.0" rx="1.4" fill="{VIOLET}"/>')
for i, n in enumerate(["SUB", "MIX"]):
    add(text(n, colx(6 + i), OUT_Y - 4.9, JACK, fill=BG))
add(text("OUT", GUT_X - 0.6, OUT_Y + 1.0, JACK, fill=SOFT))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Keel.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Keel.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
