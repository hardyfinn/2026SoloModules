#!/usr/bin/env python3
# Draws res/Thunder.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Thunder.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 132.08, 128.5  # 26 HP

BG = "#1B1D22"
BAND = "#23262C"
INK = "#ECEAE4"
SOFT = "#9A9890"
AMBER = "#FFB233"
IOBG = "#2E2A22"

# ---- LAYOUT (same numbers in src/Thunder.cpp) ----
COL = [12.0, 29.8, 47.6, 65.4, 83.2, 101.0, 118.8]
ROW = [31.0, 50.5, 70.0, 89.5]
TRIM_DX, LEARN_DX, LEARN_DY = -2.5, 5.0, 4.0
IO_TOP, IO_BOT = 106.0, 118.6
MEMX = [96.5, 103.5, 110.5, 117.5]
MEMY = [9.5, 16.8]
STORE_X, STORE_Y = 125.6, 12.8
SCREEN = (27.5, 3.0, 62.5, 16.5)

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3
# half heights of the Rack parts, so labels sit just above them
R_KNOB, R_TRIM, R_JACK, R_BEZEL, R_SWITCH = 4.8, 3.0, 4.0, 3.6, 4.8

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

# ---- header
add(text("THUNDER", 4.2, 11.0, 5.0, FONT_BOLD, AMBER, anchor="start", spacing=0.12))
add(text("drum + synth voice", 4.4, 16.0, 2.0, FONT_MED, SOFT, anchor="start"))
sx, sy, sw, sh = SCREEN
add(f'<rect x="{sx - 0.6}" y="{sy - 0.6}" width="{sw + 1.2}" height="{sh + 1.2}" rx="1.2" fill="#000"/>')
add(text("STORE", STORE_X, STORE_Y + 7.4, SMALL, FONT_BOLD, AMBER))

# ---- four bands, name written up the left edge
SECTIONS = ["SOURCE", "BODY", "FILTER", "FX + MOD"]
for r, name in enumerate(SECTIONS):
    y0, y1 = ROW[r] - 9.0, ROW[r] + 7.5
    add(f'<rect x="1.0" y="{y0}" width="{W - 2.0}" height="{y1 - y0}" rx="1.2" fill="{BAND}"/>')
    yc = (y0 + y1) / 2
    add(f'<g transform="rotate(-90 3.2 {yc})">' + text(name, 3.2, yc + 0.85, 2.3, FONT_BOLD, AMBER, spacing=0.08) + '</g>')

LABELS = [
    [("ALGO A", R_KNOB), ("ALGO B", R_KNOB), ("MORPH", R_KNOB), ("MODE", R_SWITCH), ("TUNE", R_KNOB), ("BEND", R_KNOB), ("SWEEP", R_KNOB)],
    [("DECAY", R_KNOB), ("P1", R_KNOB), ("P2", R_KNOB), ("CLICK", R_KNOB), ("NOISE", R_KNOB), ("WEAR", R_KNOB), ("MUTATE", R_KNOB)],
    [("CUTOFF", R_KNOB), ("RES", R_KNOB), ("ENV", R_KNOB), ("VCF", R_SWITCH), ("DRIVE", R_KNOB), ("TONE", R_KNOB), ("LOCK", R_BEZEL)],
    [("SEND", R_KNOB), ("TIME", R_KNOB), ("FDBK", R_KNOB), ("ACCENT", R_KNOB), ("LEVEL", R_KNOB), ("CV A", R_TRIM), ("CV B", R_TRIM)],
]
for r, row in enumerate(LABELS):
    for c, (name, rad) in enumerate(row):
        x = COL[c] + (TRIM_DX if rad == R_TRIM else 0.0)
        add(text(name, x, ROW[r] - rad - 1.4, CONTROL))

# LEARN buttons beside the CV trims
for c in (5, 6):
    add(text("LEARN", COL[c] + LEARN_DX, ROW[3] + LEARN_DY - 3.7, 1.9, FONT_BOLD, SOFT))


# switch positions (CKSSThree: value 0 = lever down)
def switch_marks(x, y, marks):
    for k, mk in enumerate(marks):
        add(text(mk, x + 3.1, y - 2.6 + 3.4 * k, 2.2, FONT_BOLD, SOFT, anchor="start"))


switch_marks(COL[3], ROW[0], ("C", "B", "A"))
switch_marks(COL[3], ROW[2], ("LP", "BP", "HP"))

# ---- trigger + output block
add(f'<rect x="1.5" y="97.6" width="{W - 3.0}" height="25.8" rx="1.4" fill="{IOBG}"/>')
for x, n in zip(COL, ["TRIG", "V/OCT", "ACC", "CHOKE", "PATCH", "CV A", "CV B"]):
    add(text(n, x, IO_TOP - R_JACK - 1.4, JACK, fill=AMBER if n == "TRIG" else INK))
for x, n, rad, col in zip(COL[2:], ["LOCK", "HIT", "ENV", "SEND", "OUT"], [R_JACK, R_BEZEL, R_JACK, R_JACK, R_JACK], [INK, INK, AMBER, AMBER, AMBER]):
    add(text(n, x, IO_BOT - rad - 1.4, JACK, fill=col))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Thunder.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Thunder.svg"), x=20.0, y=118.0, cap=3.0, bg=BG)
