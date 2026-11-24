#!/usr/bin/env python3
# Draws res/PlateDriver.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/PlateDriver.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP

BG = "#25282B"
BG_DARK = "#17191B"
LINE = "#454A4F"
INK = "#ECE8E1"
SOFT = "#A9B0B6"
ACCENT = "#D9A441"

# ---- LAYOUT (same numbers in src/PlateDriver.cpp) ----
COL10 = [13.9 + 19.5 * i for i in range(10)]   # rows 1, 2 and 4
COL11 = [12.6 + 17.8 * i for i in range(11)]   # rows 3 and 5
Y1, Y2, Y3, Y4, Y5 = 26.5, 50.5, 78.0, 98.0, 115.0
DRIVE_X = 23.65
RING_R = 11.0

CONTROL = 3.0   # control label size
JACK = 2.6      # jack label size
SMALL = 2.1
HEAD = 2.4

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

add(text("Plate Driver", 5.0, 10.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("a metal plate held in a feedback loop", W - 5.0, 9.8, 2.4, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def mid(a, b):
    return (a + b) / 2


# row 1
H1 = 15.6
heading("PLATE", 4.5, mid(COL10[6], COL10[7]) - 1.5, H1)
heading("VOICE", mid(COL10[6], COL10[7]) + 1.5, W - 4.5, H1)
for x, n in zip(COL10, ["TYPE", "CAST", "SIZE", "THICK", "CURVE", "STRETCH", "DECAY", "BRIGHT", "AGE", "LEVEL"]):
    add(text(n, x, Y1 - 7.6, CONTROL))
add(text("SHEET", COL10[0] - 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("BOWL", COL10[0] + 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("THIN", COL10[3] - 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("THICK", COL10[3] + 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("FRESH", COL10[8] - 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("TIRED", COL10[8] + 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))

# row 2
H2 = 37.6
heading("LOOP", 4.5, mid(COL10[6], COL10[7]) - 1.5, H2)
heading("TOUCH", mid(COL10[6], COL10[7]) + 1.5, W - 4.5, H2)
add(text("DRIVE", DRIVE_X, Y2 + RING_R + 3.9, CONTROL))
for x, n in zip(COL10[2:6], ["GAIN", "FOCUS", "PITCH", "PHASE"]):
    add(text(n, x, Y2 - 7.6, CONTROL))
add(text("TRACK", COL10[6], Y2 - 7.6, CONTROL))
add(text("PLATE", COL10[6], Y2 - 4.4, SMALL, FONT_BOLD, SOFT))
add(text("FOCUS", COL10[6], Y2 + 7.0, SMALL, FONT_BOLD, SOFT))
for x, n in zip(COL10[7:], ["DRIVER POS", "SPREAD", "GRAB"]):
    add(text(n, x, Y2 - 7.6, CONTROL if n != "DRIVER POS" else 2.6))
add(text("SLOW", COL10[2] - 3.6, Y2 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("HARD", COL10[2] + 3.6, Y2 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("PURE", DRIVE_X - RING_R - 0.6, Y2 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("ROAR", DRIVE_X + RING_R + 0.6, Y2 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="start"))

# row 3
H3 = 67.6
heading("HEAT", 4.5, mid(COL11[5], COL11[6]) - 1.5, H3)
heading("PUMP", mid(COL11[5], COL11[6]) + 1.5, mid(COL11[7], COL11[8]) - 1.5, H3)
heading("EXTRA", mid(COL11[7], COL11[8]) + 1.5, W - 4.5, H3)
for x, n in zip(COL11, ["PICKUP", "AMP", "DRIVER SAT", "RATTLE", "BLEED", "OUT", "PUMP", "RATE", "MALLET", "IN LEVEL", "RELEASE"]):
    add(text(n, x, Y3 - 6.0, CONTROL if len(n) <= 6 else 2.5))

# jacks
add(f'<rect x="2.0" y="{Y4 - 9.4}" width="{W - 4.0}" height="{Y5 - Y4 + 15.4}" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COL10, ["V/OCT", "GATE", "DRIVE", "FOCUS", "PHASE", "SIZE", "THICK", "CURVE", "DRIVER", "SPREAD"]):
    add(text(n, x, Y4 - 5.4, JACK, FONT_BOLD, INK if n in ("V/OCT", "GATE") else SOFT))
for x, n in zip(COL11[:7], ["GRAB", "RATTLE", "PUMP", "RATE", "STRIKE", "VEL", "IN"]):
    add(text(n, x, Y5 - 5.4, JACK, FONT_BOLD, INK if n in ("STRIKE", "VEL", "IN") else SOFT))
ox0 = COL11[7] - 8.0
add(f'<rect x="{ox0}" y="{Y5 - 9.0}" width="{COL11[10] + 8.0 - ox0}" height="13.4" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(COL11[7:], ["ROAR", "ENV", "L", "R"]):
    add(text(n, x, Y5 - 5.4, JACK, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in COL10: c(x, Y1, 4.8, "knob1")
    c(DRIVE_X, Y2, RING_R + 0.6, "drive ring")
    for x in COL10[2:6] + COL10[7:]: c(x, Y2, 4.8, "knob2")
    b.append(("track", COL10[6] - 1.8, Y2 - 3.3, COL10[6] + 1.8, Y2 + 3.3))
    for x in COL11: c(x, Y3, 4.0, "knob3")
    for x in COL10: c(x, Y4, 4.0, "jack4")
    for x in COL11: c(x, Y5, 4.0, "jack5")
    return b


allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in boxes:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "PlateDriver.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "PlateDriver.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
