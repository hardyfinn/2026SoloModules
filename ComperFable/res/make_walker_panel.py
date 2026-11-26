#!/usr/bin/env python3
# Draws res/Walker.svg (36 HP since 2.4.0: full-size labels, OCTAVE, LENGTH, VARY). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Walker.cpp.
#   pip install fonttools ; python3 res/make_walker_panel.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 182.88, 128.5  # 36 HP

BG = "#23262B"
BG_DARK = "#16181C"
LINE = "#3D424A"
INK = "#EFE9DD"
SOFT = "#A8A397"
ACCENT = "#D9A441"

# ---- LAYOUT (same numbers in src/Walker.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 4.0, 12.5, 174.88, 13.0
COL_X0, COL_DX = 11.0, 20.11       # 9 columns: PLAYER, HANDS and the jacks
SND_X0, SND_DX = 11.0, 17.875      # 10 columns: SOUND
Y_PLAYER = 42.0
Y_HANDS = 66.0
Y_SOUND = 89.0
Y_IN1 = 106.0
Y_IN2 = 118.0
OUT_COL0 = 7                       # outputs in columns 7 and 8

CONTROL = 3.2     # project standard (2026-11-10)
JACK = 2.9
SMALL = 2.2       # captions under knobs
HEAD = 2.9

def col(i): return COL_X0 + i * COL_DX
def scol(i): return SND_X0 + i * SND_DX

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
add(text("Walker", 5.0, 9.4, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("upright bass player · sits next to Comper", W - 5.0, 9.0, 2.9, FONT_MED, ACCENT, anchor="end"))

def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

# display window
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')

# PLAYER
heading("PLAYER", 3.2, W - 3.2, 30.6)
PLAYER = ["FEEL", "ACTIVITY", "REGISTER", "ROAM", "OUT", "REPEAT", "SEED", "NEW", "OCTAVE"]
for i, n in enumerate(PLAYER):
    add(text(n, col(i), Y_PLAYER - (5.6 if n == "NEW" else 6.6), CONTROL))
add(text("0 = FOLLOW", col(0), Y_PLAYER + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("LINES OUT", col(4), Y_PLAYER + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("0 · +1 · +2", col(8), Y_PLAYER + 8.0, SMALL, FONT_BOLD, SOFT))

# HANDS
heading("HANDS", 3.2, W - 3.2, 53.4)
HANDS = ["LOOSE", "AHEAD", "TIMING", "WEIGHT", "SLOP", "PITCH", "LENGTH", "VARY"]
for i, n in enumerate(HANDS):
    add(text(n, col(i), Y_HANDS - (7.9 if n == "LOOSE" else 6.6 if n in ("AHEAD", "LENGTH", "VARY") else 5.2), CONTROL))
add(text("TRIMS × LOOSE", (col(2) + col(5)) / 2, Y_HANDS + 6.4, SMALL, FONT_BOLD, SOFT))
add(text("BEHIND · AHEAD", col(1), Y_HANDS + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("SHORT · LONG", col(6), Y_HANDS + 8.0, SMALL, FONT_BOLD, SOFT))
add(text("SAME · MIXED", col(7), Y_HANDS + 8.0, SMALL, FONT_BOLD, SOFT))
# lights, stacked in the last column
add(text("PLUCK", col(8), Y_HANDS - 5.6, CONTROL))
add(text("LINK", col(8), Y_HANDS + 2.4, CONTROL))

# SOUND
heading("SOUND", 3.2, W - 3.2, 76.6)
SOUND = ["ATTACK", "PLUCK", "BRIGHT", "SUSTAIN", "GROWL", "BODY", "PICKUP", "THUMP", "AGE", "LEVEL"]
for i, n in enumerate(SOUND):
    add(text(n, scol(i), Y_SOUND - (7.7 if n == "ATTACK" else 6.6), CONTROL))
add(text("MIC · PICKUP", scol(6), Y_SOUND + 7.6, SMALL, FONT_BOLD, SOFT))
add(text("NEAR · FAR", scol(1), Y_SOUND + 7.6, SMALL, FONT_BOLD, SOFT))

# jacks
add(f'<rect x="2.0" y="{Y_IN1 - 8.4}" width="{W - 4.0}" height="{Y_IN2 - Y_IN1 + 13.0}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "ROOT", "QUAL", "CHORD", "LAY OUT", ""]
IN2 = ["V/OCT IN", "GATE IN", "VEL IN", "ACTIVITY", "REGISTER", "LOOSE", "ATTACK"]
def jlabel(n, x, y, fill):
    add(text(n, x, y - 4.9, JACK, FONT_BOLD, fill))
for i, n in enumerate(IN1):
    if n: jlabel(n, col(i), Y_IN1, INK)
for i, n in enumerate(IN2): jlabel(n, col(i), Y_IN2, SOFT if i >= 3 else INK)
ox0 = col(OUT_COL0) - 7.0
add(f'<rect x="{ox0:.2f}" y="{Y_IN1 - 8.0}" width="{W - 3.2 - ox0:.2f}" height="{Y_IN2 - Y_IN1 + 12.2}" rx="1.4" fill="{ACCENT}"/>')
OUT1 = ["OUT", "V/OCT"]
OUT2 = ["GATE", "VEL"]
for i, n in enumerate(OUT1): jlabel(n, col(OUT_COL0 + i), Y_IN1, BG_DARK)
for i, n in enumerate(OUT2): jlabel(n, col(OUT_COL0 + i), Y_IN2, BG_DARK)

def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for i, n in enumerate(PLAYER):
        c(col(i), Y_PLAYER, 3.0 if n == "NEW" else 4.6, n)
    for i, n in enumerate(HANDS):
        c(col(i), Y_HANDS, 6.4 if n == "LOOSE" else 4.6 if n in ("AHEAD", "LENGTH", "VARY") else 3.1, n)
    c(col(8), Y_HANDS - 2.2, 1.4, "pluck light"); c(col(8), Y_HANDS + 5.4, 1.4, "link light")
    for i, n in enumerate(SOUND):
        c(scol(i), Y_SOUND, 6.4 if n == "ATTACK" else 4.6, n)
    for i, n in enumerate(IN1):
        if n: c(col(i), Y_IN1, 4.0, "in")
    for i in range(7): c(col(i), Y_IN2, 4.0, "in2")
    for i in range(2):
        c(col(OUT_COL0 + i), Y_IN1, 4.0, "out"); c(col(OUT_COL0 + i), Y_IN2, 4.0, "out2")
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
open(os.path.join(HERE, "Walker.svg"), "w").write(svg)
print("ok")

import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Walker.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
