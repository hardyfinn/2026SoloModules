#!/usr/bin/env python3
# Draws res/Soloist.svg (30 HP). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Soloist.cpp.
#   pip install fonttools ; python3 res/make_soloist_panel.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#23262B"
BG_DARK = "#16181C"
LINE = "#3D424A"
INK = "#EFE9DD"
SOFT = "#A8A397"
ACCENT = "#D9A441"

# ---- LAYOUT (same numbers in src/Soloist.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 4.0, 12.5, 144.4, 13.0
COL_X0, COL_DX = 10.5, 18.77       # 8 columns
Y_FORM = 42.0
Y_LINE = 66.0
Y_HORN = 89.0
Y_IN1 = 106.0
Y_IN2 = 118.0

CONTROL = 3.2     # project standard (2026-11-10)
JACK = 2.9
SMALL = 2.2       # captions under knobs
HEAD = 2.9

def col(i): return COL_X0 + i * COL_DX

fonts = {}
boxes = []


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, check=True):
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
    if check:
        boxes.append((s, x0, y - size * 0.75, x0 + width * scale, y + size * 0.05))
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Soloist", 5.0, 9.4, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("horn player · the fourth in the band", W - 5.0, 9.0, 2.9, FONT_MED, ACCENT, anchor="end"))

def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')

# FORM
heading("FORM", 3.2, W - 3.2, 30.6)
FORM = ["FORM", "CHORUSES", "TRADE", "WITH", "SECTION", "SEED", "NEW", ""]
for i, n in enumerate(FORM):
    if n: add(text(n, col(i), Y_FORM - (5.6 if n == "NEW" else 6.6), CONTROL))
add(text("TUNE·SOLO·CUE", col(0), Y_FORM + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("OFF·2·4·8 BARS", col(2), Y_FORM + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("DR·PNO·BOTH", col(3), Y_FORM + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("HORNS·TURNS", col(4), Y_FORM + 8.0, SMALL, FONT_BOLD, SOFT))

# LINE
heading("LINE", 3.2, W - 3.2, 53.4)
LINE_ = ["DENSITY", "SPACE", "MOTIF", "OUTSIDE", "ENERGY", "LOOSE", "LENGTH", ""]
for i, n in enumerate(LINE_):
    if n: add(text(n, col(i), Y_LINE - (7.9 if n == "OUTSIDE" else 6.6), CONTROL))
add(text("INSIDE · FREE", col(3), Y_LINE + 9.2, SMALL, FONT_BOLD, SOFT))
add(text("SHORT · LONG", col(6), Y_LINE + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("TIGHT · LAZY", col(5), Y_LINE + 8.0, SMALL, FONT_BOLD, SOFT))

# HORN
heading("HORN", 3.2, W - 3.2, 76.6)
HORN = ["HORN", "REGISTER", "OCTAVE", "ROOM", "LEVEL", "", "", ""]
for i, n in enumerate(HORN):
    if n: add(text(n, col(i), Y_HORN - (7.9 if n == "HORN" else 6.6), CONTROL))
add(text("V/OCT ONLY", col(2), Y_HORN + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("NOTE", col(6), Y_HORN - 5.6, CONTROL))
add(text("LINK", col(7), Y_HORN - 5.6, CONTROL))

# jacks
add(f'<rect x="2.0" y="{Y_IN1 - 8.4}" width="{W - 4.0}" height="{Y_IN2 - Y_IN1 + 13.0}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "CHORD", "PLAY", "TRADE", "ENERGY", "DENSITY", "OUTSIDE"]
OUT = ["L", "R", "V/OCT", "GATE", "VEL", "BREATH", "PHRASE", "FORM"]
def jlabel(n, x, y, fill):
    add(text(n, x, y - 4.9, JACK, FONT_BOLD, fill))
for i, n in enumerate(IN1): jlabel(n, col(i), Y_IN1, SOFT if i >= 5 else INK)
add(f'<rect x="3.2" y="{Y_IN2 - 8.0}" width="{W - 6.4:.2f}" height="11.6" rx="1.4" fill="{ACCENT}"/>')
for i, n in enumerate(OUT): jlabel(n, col(i), Y_IN2, BG_DARK)

def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for i, n in enumerate(FORM):
        if n: c(col(i), Y_FORM, 3.0 if n == "NEW" else 4.6, n)
    for i, n in enumerate(LINE_):
        if n: c(col(i), Y_LINE, 6.4 if n == "OUTSIDE" else 4.6, n)
    for i, n in enumerate(HORN):
        if n: c(col(i), Y_HORN, 6.4 if n == "HORN" else 4.6, n)
    c(col(6), Y_HORN, 1.4, "note light"); c(col(7), Y_HORN, 1.4, "link light")
    for i in range(8):
        c(col(i), Y_IN1, 4.0, "in"); c(col(i), Y_IN2, 4.0, "out")
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    return b

allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in allb:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Soloist.svg"), "w").write(svg)
print("ok")

import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Soloist.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
