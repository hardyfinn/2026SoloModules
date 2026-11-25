#!/usr/bin/env python3
# Draws res/LongMetal.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/LongMetal.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP

BG = "#2B2C2E"
BG_DARK = "#161718"
LINE = "#4E5257"
INK = "#ECE8E1"
SOFT = "#A9B0B6"
ACCENT = "#C9A36B"   # rail rust-brass

# ---- LAYOUT (same numbers in src/LongMetal.cpp) ----
COL11 = [12.6 + 17.8 * i for i in range(11)]   # row 1
COL10 = [13.9 + 19.5 * i for i in range(10)]   # row 4
COL12 = [10.4 + 16.58 * i for i in range(12)]  # row 5
Y1, Y2, Y3, Y4, Y5 = 39.5, 62.0, 88.0, 104.5, 118.5
DISP_X, DISP_Y, DISP_W, DISP_H = 41.6, 13.0, 156.6, 11.0
ROW2_X = [17.0, 43.0, 63.0, 80.0, 95.0, 117.0, 134.0, 151.0, 168.0]   # LIVE (big), DISTANCE (big), LISTEN, PASS TIME, PASS button, WARP, SWELL, LOOSE, GRIP
ROW3_X = [12.6, 30.4, 48.2, 66.0, 83.8, 101.6, 133.0, 154.0, 175.0]   # TRAIN, SPEED, FLATS, STRIKE button, HARD, IN LEVEL, EVEN, WIDTH, LEVEL
BIG_R = 7.6

CONTROL = 3.0
JACK = 2.6
SMALL = 1.9
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
add(text("Long Metal", 5.0, 15.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("the sound is the journey", 5.0, 21.4, 2.3, FONT_MED, ACCENT, anchor="start"))
# display strip
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="{BG_DARK}" stroke="{LINE}" stroke-width="0.3"/>')


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def mid(a, b):
    return (a + b) / 2


def ends(x, y, lo, hi, r=3.6):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


def lab(n, x, y):
    add(text(n, x, y, CONTROL if len(n) <= 6 else (2.6 if len(n) <= 8 else 2.3)))


# row 1: LINE + CHARACTER
H1 = 28.0
heading("LINE", 4.5, mid(COL11[9], COL11[10]) - 1.5, H1)
heading("CHAR", mid(COL11[9], COL11[10]) + 1.5, W - 4.5, H1)
for x, n in zip(COL11, ["PITCH", "LENGTH", "SPACING", "GAUGE", "STIFF", "SUPPORTS", "END A", "END B", "GROUND", "BRIGHT", "AGE"]):
    lab(n, x, Y1 - 7.0)
add(text("UP: HEAVY", COL11[3], Y1 + 8.2, SMALL, FONT_BOLD, SOFT))
ends(COL11[4], Y1 + 7.6, "ROPE", "RAIL", r=3.2)
ends(COL11[5], Y1 + 7.6, "OPEN", "TIES", r=3.2)
ends(COL11[10], Y1 + 7.6, "NEW", "OLD")

# row 2: LIVE, PLACE, PUSHED
H2 = 51.0
heading("PLACE", mid(ROW2_X[0], ROW2_X[1]) + 1.0, mid(ROW2_X[4], ROW2_X[5]) - 1.5, H2)
heading("PUSHED", mid(ROW2_X[4], ROW2_X[5]) + 1.5, W - 4.5, H2)
add(text("LIVE", ROW2_X[0], Y2 + BIG_R + 4.2, CONTROL))
ends(ROW2_X[0], Y2 + BIG_R + 4.2, "DAMP", "SING", r=5.6)
add(text("DISTANCE", ROW2_X[1], Y2 + BIG_R + 4.2, CONTROL))
for x, n in zip(ROW2_X[2:], ["LISTEN", "PASS TIME", "PASS", "WARP", "SWELL", "LOOSE", "GRIP"]):
    lab(n, x, Y2 - 6.6)
ends(ROW2_X[2], Y2 + 7.6, "A", "B", r=3.2)

# row 3: SOURCES, OUT
H3 = 78.4
heading("SOURCES", 4.5, mid(ROW3_X[5], ROW3_X[6]) - 3.0, H3)
heading("OUT", mid(ROW3_X[5], ROW3_X[6]) + 3.0, W - 4.5, H3)
for x, n in zip(ROW3_X, ["TRAIN", "SPEED", "FLATS", "STRIKE", "HARD", "IN LEVEL", "EVEN", "WIDTH", "LEVEL"]):
    lab(n, x, Y3 - 6.6)

# jacks
add(f'<rect x="2.0" y="{Y4 - 9.2}" width="{W - 4.0}" height="{Y5 - Y4 + 14.6}" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COL10, ["V/OCT", "STIFF", "SUPPORTS", "LIVE", "GROUND", "LISTEN", "DISTANCE", "PASS", "TRAIN", "SPEED"]):
    add(text(n, x, Y4 - 5.4, JACK if len(n) <= 6 else 2.2, FONT_BOLD, INK if n in ("V/OCT", "PASS") else SOFT))
for x, n in zip(COL12[:7], ["STRIKE", "VEL", "WARP", "SWELL", "LOOSE", "GRIP", "IN"]):
    add(text(n, x, Y5 - 5.4, JACK if len(n) <= 6 else 2.2, FONT_BOLD, INK if n in ("STRIKE", "VEL", "IN") else SOFT))
ox0 = COL12[7] - 7.4
add(f'<rect x="{ox0}" y="{Y5 - 8.8}" width="{COL12[11] + 7.4 - ox0}" height="13.0" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(COL12[7:], ["ENV", "CLACK", "PASS", "L", "R"]):
    add(text(n, x, Y5 - 5.4, JACK if len(n) <= 5 else 2.2, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in COL11: c(x, Y1, 4.8, "knob1")
    c(ROW2_X[0], Y2, BIG_R, "live"); c(ROW2_X[1], Y2, BIG_R, "dist")
    for x in ROW2_X[2:]: c(x, Y2, 4.8, "knob2")
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    for x in ROW3_X: c(x, Y3, 4.8, "knob3")
    for x in COL10: c(x, Y4, 4.0, "jack4")
    for x in COL12: c(x, Y5, 4.0, "jack5")
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
open(os.path.join(HERE, "LongMetal.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "LongMetal.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
