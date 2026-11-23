#!/usr/bin/env python3
# Draws res/Hangover.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Hangover.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 213.36, 128.5  # 42 HP

BG = "#33251B"       # nitrided steel, warm bronze-brown
BG_DIRT = "#24180F"
BG_IO = "#2B1F16"
LINE = "#5A4331"
INK = "#F3E6CF"
SOFT = "#C9B49A"
COPPER = "#E39A55"
RUST = "#C8653A"

# ---- LAYOUT (same numbers in src/Hangover.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 14.5, 104.0, 50.0
MX = [130.0, 160.0, 190.0]
MY = [26.0, 43.0, 60.0]
DIRT_Y = 86.0
DIRT_MX = 19.0
DX0, DDX = 40.0, 19.5
NEW_X = 199.0
ROWA_Y = 106.0
ROWB_Y = 119.0
TRIG_X0, TRIG_DX = 10.0, 12.2
POLY_X0, POLY_DX = 140.0, 15.5
CV_X0, CV_DX = 10.0, 16.0
OUT_X = [186.5, 202.0]

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
# a faint hammered-steel texture: soft dimples
import random
rnd = random.Random(7)
for _ in range(70):
    x, y = rnd.uniform(4, W - 4), rnd.uniform(4, 66)
    if DISP_X - 2 < x < DISP_X + DISP_W + 2 and DISP_Y - 2 < y < DISP_Y + DISP_H + 2:
        continue
    add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rnd.uniform(1.5, 4.0):.1f}" fill="#000" fill-opacity="0.06"/>')

# ---- title + screen
add(text("Hangover", 6.0, 11.0, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("handpan · with the dirt left in", W - 6.0, 10.6, 2.8, FONT_MED, COPPER, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.2" fill="#0E0906" stroke="{COPPER}" stroke-opacity="0.55" stroke-width="0.35"/>')


def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


# ---- main knobs 3 x 3
names = [["SCALE", "KEY", "DECAY"], ["FORCE", "TOUCH", "STRIKE"], ["MUTE", "GU", "ROLL"]]
marks = [[None, None, ("SHORT", "LONG")], [("SOFT", "HARD"), ("PAD", "SLAP"), ("MID", "EDGE")],
         [("OPEN", "DAMP"), ("0", "BOOM"), ("6", "24/s")]]
for r in range(3):
    for c in range(3):
        add(text(names[r][c], MX[c], MY[r] - 7.6, CONTROL, fill=COPPER if names[r][c] == "GU" else INK))
        if marks[r][c]:
            ranges(MX[c], MY[r] + 4.8, 5.8, *marks[r][c])

# ---- dirt strip
add(f'<rect x="2.6" y="68.0" width="{W - 5.2}" height="28.6" rx="1.6" fill="{BG_DIRT}"/>')
add(f'<rect x="2.6" y="68.0" width="{W - 5.2}" height="0.7" fill="{RUST}"/>')
add(text("DIRT", DIRT_MX, DIRT_Y - 9.0, CONTROL + 0.4, FONT_BOLD, RUST))
ranges(DIRT_MX, DIRT_Y + 8.6, 5.6, "CLEAN", "FILTHY", SOFT)
for i, n in enumerate(["TUNING", "BEAT", "RATTLE", "BLEED", "AIR", "SKIN", "SLOP", "ROOM"]):
    add(text(n, DX0 + DDX * i, DIRT_Y - 7.4, CONTROL, FONT_BOLD, INK))
add(text("NEW", NEW_X, DIRT_Y - 7.4, CONTROL, FONT_BOLD, RUST))
add(text("HANG", NEW_X, DIRT_Y + 8.0, SMALL, FONT_BOLD, SOFT))

# ---- jacks
add(f'<rect x="2.6" y="98.6" width="{TRIG_X0 + TRIG_DX * 9 + 3.4}" height="12.8" rx="1.4" fill="{BG_IO}"/>')
for i, n in enumerate(["DING", "1", "2", "3", "4", "5", "6", "7", "8", "GU"]):
    add(text(n, TRIG_X0 + TRIG_DX * i, ROWA_Y - 5.2, JACK, FONT_BOLD, COPPER if n in ("DING", "GU") else INK))
add(f'<rect x="{POLY_X0 - 7.4}" y="98.6" width="{POLY_DX * 4 + 14.8}" height="12.8" rx="1.4" fill="{BG_IO}"/>')
for i, n in enumerate(["GATE", "V/OCT", "VEL", "MUTE", "ROLL"]):
    add(text(n, POLY_X0 + POLY_DX * i, ROWA_Y - 5.2, JACK, FONT_BOLD, INK))
# CV row
add(f'<rect x="2.6" y="111.8" width="{CV_DX * 5 + 14.8}" height="12.8" rx="1.4" fill="{BG_IO}"/>')
for i, n in enumerate(["DECAY", "TOUCH", "STRIKE", "MUTE", "GU", "DIRT"]):
    add(text(n, CV_X0 + CV_DX * i, ROWB_Y - 5.2, JACK, FONT_BOLD, SOFT))
add(text("CV IN", CV_X0 + CV_DX * 6 - 3.0, ROWB_Y + 1.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("GATE + V/OCT (poly) strike", POLY_X0 - 7.0, ROWB_Y - 2.0, SMALL, FONT_MED, SOFT, anchor="start"))
add(text("the nearest field (try Peal)", POLY_X0 - 7.0, ROWB_Y + 1.6, SMALL, FONT_MED, SOFT, anchor="start"))
# outputs
add(f'<rect x="{OUT_X[0] - 7.0}" y="111.8" width="{OUT_X[1] - OUT_X[0] + 14.0}" height="12.8" rx="1.4" fill="{COPPER}"/>')
for x, n in zip(OUT_X, ["L", "R"]):
    add(text(n, x, ROWB_Y - 5.2, JACK, FONT_BOLD, BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Hangover.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Hangover.svg"), x=150.0, y=126.0, cap=3.0, bg=BG)
