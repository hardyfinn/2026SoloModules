#!/usr/bin/env python3
# Draws res/BentSheetMetal.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/BentSheetMetal.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP

BG = "#2A2D30"
BG_DARK = "#17191B"
LINE = "#4A5056"
INK = "#ECE8E1"
SOFT = "#A9B0B6"
ACCENT = "#8FB8C9"   # cold steel blue

# ---- LAYOUT (same numbers in src/BentSheetMetal.cpp) ----
COL11 = [12.6 + 17.8 * i for i in range(11)]   # rows 1 and 5
COL10 = [13.9 + 19.5 * i for i in range(10)]   # row 4
Y1, Y2, Y3, Y4, Y5 = 27.0, 52.0, 79.0, 100.0, 116.0
ROW2_X = [21.0, 44.0, 61.0, 78.0, 95.0, 112.0, 139.0, 162.0, 182.0]   # BEND (big), SHAPE, SPOT, HOLD, SNAP, CRACK, BOW (big), SPEED, BOW POS
BIG_R = 7.6
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 72.0, 72.0, 14.0
ROW3_KNOBS = [92.0, 108.0, 124.0]   # HARD, ROLL, IN LEVEL
ROW3_OUTS = [152.0, 168.0, 184.0]   # SNAP, BEND, ENV

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

add(text("Bent Sheet Metal", 5.0, 10.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("bend it, flex it, dip it, bow it", W - 5.0, 9.8, 2.4, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def mid(a, b):
    return (a + b) / 2


def ends(x, y, lo, hi, r=3.6):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


# row 1
H1 = 15.2
heading("SHEET", 4.5, mid(COL11[3], COL11[4]) - 1.5, H1)
heading("WATER", mid(COL11[3], COL11[4]) + 1.5, mid(COL11[5], COL11[6]) - 1.5, H1)
heading("CHARACTER", mid(COL11[5], COL11[6]) + 1.5, W - 4.5, H1)
for x, n in zip(COL11, ["PITCH", "TAPER", "GRIP", "DECAY", "DIP", "DEPTH", "STRETCH", "PICKUP", "OUT", "AGE", "LEVEL"]):
    add(text(n, x, Y1 - 7.6, CONTROL if len(n) <= 6 else 2.7))
ends(COL11[2], Y1 + 7.6, "FREE", "HELD")
ends(COL11[5], Y1 + 7.6, "AIR", "HEAVY")
ends(COL11[9], Y1 + 7.6, "NEW", "OLD")

# row 2
H2 = 39.6
heading("SHAPE", 4.5, mid(ROW2_X[5], ROW2_X[6]) - 3.5, H2)
heading("BOW", mid(ROW2_X[5], ROW2_X[6]) + 1.0, W - 4.5, H2)
add(text("BEND", ROW2_X[0], Y2 + BIG_R + 4.2, CONTROL))
add(text("BOW", ROW2_X[6], Y2 + BIG_R + 4.2, CONTROL))
for x, n in zip(ROW2_X[1:6] + ROW2_X[7:], ["SHAPE", "SPOT", "HOLD", "SNAP", "CRACK", "SPEED", "BOW POS"]):
    add(text(n, x, Y2 - 7.6, CONTROL if len(n) <= 6 else 2.7))
ends(ROW2_X[1], Y2 + 7.6, "ARCH", "S")
ends(ROW2_X[3], Y2 + 7.6, "LOOSE", "FIRM")

# row 3: display strip, HIT, outputs
H3 = 69.0
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="{BG_DARK}" stroke="{LINE}" stroke-width="0.3"/>')
heading("HIT", ROW3_KNOBS[0] - 6.0, ROW3_KNOBS[2] + 5.0, H3)
for x, n in zip(ROW3_KNOBS, ["HARD", "ROLL", "IN LEVEL"]):
    add(text(n, x, Y3 - 6.0, CONTROL if len(n) <= 6 else 2.5))
ox0 = ROW3_OUTS[0] - 8.0
add(f'<rect x="{ox0}" y="{Y3 - 9.0}" width="{ROW3_OUTS[2] + 8.0 - ox0}" height="13.4" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(ROW3_OUTS, ["SNAP", "BEND", "ENV"]):
    add(text(n, x, Y3 - 5.4, JACK, fill=BG_DARK))

# jacks
add(f'<rect x="2.0" y="{Y4 - 9.4}" width="{W - 4.0}" height="{Y5 - Y4 + 15.4}" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COL10, ["V/OCT", "BEND V/OCT", "BEND", "SHAPE", "SPOT", "HOLD", "SNAP", "TAPER", "GRIP", "DIP"]):
    add(text(n, x, Y4 - 5.4, JACK if len(n) <= 6 else 2.2, FONT_BOLD, INK if n in ("V/OCT", "BEND V/OCT") else SOFT))
for x, n in zip(COL11[:9], ["BOW", "SPEED", "BOW POS", "BOW GATE", "STRIKE", "VEL", "ROLL", "STRETCH", "IN"]):
    add(text(n, x, Y5 - 5.4, JACK if len(n) <= 6 else 2.2, FONT_BOLD, INK if n in ("BOW GATE", "STRIKE", "VEL", "IN") else SOFT))
ox0 = COL11[9] - 8.0
add(f'<rect x="{ox0}" y="{Y5 - 9.0}" width="{COL11[10] + 8.0 - ox0}" height="13.4" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(COL11[9:], ["L", "R"]):
    add(text(n, x, Y5 - 5.4, JACK, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in COL11: c(x, Y1, 4.8, "knob1")
    c(ROW2_X[0], Y2, BIG_R, "bend"); c(ROW2_X[6], Y2, BIG_R, "bow")
    for x in ROW2_X[1:6] + ROW2_X[7:]: c(x, Y2, 4.8, "knob2")
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    for x in ROW3_KNOBS: c(x, Y3, 3.9, "knob3")
    for x in ROW3_OUTS: c(x, Y3, 4.0, "out3")
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
open(os.path.join(HERE, "BentSheetMetal.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "BentSheetMetal.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
