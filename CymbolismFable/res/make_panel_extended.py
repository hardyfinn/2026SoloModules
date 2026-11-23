#!/usr/bin/env python3
# Draws res/CymbolismExtended.svg (60 HP). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/CymbolismExtended.cpp.
#   pip install fonttools ; python3 res/make_panel_extended.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 304.8, 128.5  # 60 HP

BG = "#2A2418"        # dark patinated bronze (as Cymbolism)
BG_ROW = "#221D13"
BG_IO = "#1C1810"
LINE = "#4E4330"
GOLD = "#E2C27A"
CREAM = "#F1E7CF"
BRONZE = "#B98E52"
PATINA = "#7FA58F"    # verdigris: the UNREAL half
BG_UNREAL = "#1F2A24"
SOFT = "#B7A888"

# ---- LAYOUT (same numbers in src/CymbolismExtended.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 13.0, 104.0, 38.0
TYPE_X, TYPE_Y = 122.0, 24.0
TYPEB_X, TYPEB_Y = 122.0, 44.0
NEW_X, NEW_Y = 135.0, 44.0
POS_X, FORCE_X, BIG_Y, BIGCV_Y = 150.0, 174.0, 24.0, 44.0
BTN_X, STRIKE_Y, CHOKE_Y, FREEZE_Y = 193.0, 19.0, 31.5, 44.0
IO_X0, IO_DX, IO1_Y, IO2_Y = 210.0, 12.2, 24.0, 44.0
ROW_X0, ROW_DX = 14.0, 22.0
ROW1_Y, ROW2_Y, ROW3_Y, CV_DY = 63.0, 85.5, 108.0, 10.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=CREAM, anchor="middle", spacing=0.04):
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
for i in range(34):
    r = 40 + i * 6.0
    add(f'<circle cx="{W + 10}" cy="-30" r="{r:.1f}" fill="none" stroke="#000" stroke-opacity="0.07" stroke-width="0.6"/>')

# ---- title + screen
add(text("Cymbolism Extended", 5.0, 9.8, 5.0, FONT_BOLD, GOLD, anchor="start", spacing=0.02))
add(text("the same cymbal · the laws of physics on knobs", W - 5.0, 9.6, 2.7, FONT_MED, PATINA, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.2" fill="#0D0B07" stroke="{GOLD}" stroke-opacity="0.5" stroke-width="0.35"/>')

# ---- top: type, type B, new, the big knobs with their CV, the buttons
add(text("TYPE", TYPE_X, TYPE_Y - 9.0, CONTROL, fill=GOLD))
add(text("TYPE B", TYPEB_X, TYPEB_Y - 5.6, CONTROL, fill=PATINA))
add(text("NEW", NEW_X, NEW_Y - 5.6, CONTROL, fill=GOLD))
add(text("POSITION", POS_X, BIG_Y - 9.0, CONTROL, fill=BRONZE))
add(text("FORCE", FORCE_X, BIG_Y - 9.0, CONTROL, fill=BRONZE))
add(text("BELL", POS_X - 6.4, BIG_Y + 9.4, SMALL, fill=SOFT, anchor="middle"))
add(text("EDGE", POS_X + 6.4, BIG_Y + 9.4, SMALL, fill=SOFT, anchor="middle"))
add(text("SOFT", FORCE_X - 6.4, BIG_Y + 9.4, SMALL, fill=SOFT, anchor="middle"))
add(text("HARD", FORCE_X + 6.4, BIG_Y + 9.4, SMALL, fill=SOFT, anchor="middle"))
add(text("CV", POS_X - 6.6, BIGCV_Y + 1.0, JACK, fill=BRONZE, anchor="end"))
add(text("CV", FORCE_X - 6.6, BIGCV_Y + 1.0, JACK, fill=BRONZE, anchor="end"))
add(text("STRIKE", BTN_X, STRIKE_Y - 4.6, CONTROL, fill=BRONZE))
add(text("CHOKE", BTN_X, CHOKE_Y - 4.6, CONTROL, fill=BRONZE))
add(text("FREEZE", BTN_X, FREEZE_Y - 4.6, CONTROL, fill=PATINA))

# ---- gates, audio in and the outputs (top right)
add(f'<rect x="{IO_X0 - 7.0}" y="{IO1_Y - 9.8}" width="{W - IO_X0 + 4.4}" height="{IO2_Y - IO1_Y + 15.6}" rx="1.4" fill="{BG_IO}"/>')
io1 = ["STRIKE", "VEL", "CHOKE", "ROLL", "HOLD", "FREEZE", "EXCITE", "PUMP"]
cols1 = [BRONZE, BRONZE, BRONZE, BRONZE, PATINA, PATINA, BRONZE, PATINA]
for i, n in enumerate(io1):
    add(text(n, IO_X0 + IO_DX * i, IO1_Y - 5.4, JACK if len(n) < 6 else 2.6, FONT_BOLD, cols1[i]))
io2 = ["V/OCT", "CHORD"]
for i, n in enumerate(io2):
    add(text(n, IO_X0 + IO_DX * i, IO2_Y - 5.4, JACK, FONT_BOLD, PATINA))
add(text("poly", IO_X0 + IO_DX * 1, IO2_Y + 7.2, SMALL, FONT_BOLD, SOFT))
outs = ["L", "R", "CORE", "WASH", "ENV"]
ox0 = IO_X0 + IO_DX * 3
add(f'<rect x="{ox0 - 6.0}" y="{IO2_Y - 9.0}" width="{IO_DX * 4 + 12.0}" height="14.6" rx="1.4" fill="{GOLD}"/>')
for i, n in enumerate(outs):
    add(text(n, ox0 + IO_DX * i, IO2_Y - 5.4, JACK, FONT_BOLD, BG))

# ---- the three knob rows; the jack under a knob is its CV
def row(y, names, fills, ground, accent, title):
    top = y - 9.6
    add(f'<rect x="2.6" y="{top}" width="{W - 5.2}" height="{CV_DY + 14.2}" rx="1.4" fill="{ground}"/>')
    add(f'<rect x="2.6" y="{top}" width="{W - 5.2}" height="0.5" fill="{accent}" fill-opacity="0.7"/>')
    # the row's name runs up the right edge, clear of the last knob
    cy = top + (CV_DY + 14.2) / 2
    add(f'<g transform="rotate(-90 {W - 4.6} {cy})">' + text(title, W - 4.6, cy + 0.8, SMALL, FONT_BOLD, accent) + '</g>')
    for i, n in enumerate(names):
        if not n:
            continue
        x = ROW_X0 + ROW_DX * i
        add(text(n, x, y - 5.8, CONTROL if len(n) < 9 else 2.9, fill=fills[i]))


row1 = ["SIZE", "WEIGHT", "TAPER", "PROFILE", "BELL", "FLANGE", "HOLE", "ALLOY", "HAMMER", "TENSION"]
row(ROW1_Y, row1, [GOLD] * 10, BG_ROW, GOLD, "CYMBAL")
row2 = ["MORPH", "WARP", "PIVOT", "TUNE", "SCALE", "ROOT", "TILT", "SUSTAIN", "SWELL", "COLOUR", "FLOW", "LOAD", "LOAD POS"]
row(ROW2_Y, row2, [CREAM] * 13, BG_UNREAL, PATINA, "UNREAL")
row3 = ["DECAY", "BLOOM", "DAMP", "MOUNT", "SIZZLE", "MIC", "PICKUP", "WIDTH", "PLAY", "HARDNESS", "RATE", "BOUNCE", "EXC"]
row(ROW3_Y, row3, [CREAM] * 8 + [BRONZE] * 5, BG_ROW, BRONZE, "SOUND · PLAY")

# knobs without a CV jack get a small mark where the jack would be
for (y, idx) in [(ROW1_Y, 5), (ROW1_Y, 6), (ROW1_Y, 7), (ROW2_Y, 4)]:
    x = ROW_X0 + ROW_DX * idx
    add(f'<circle cx="{x}" cy="{y + CV_DY}" r="0.6" fill="{LINE}"/>')

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "CymbolismExtended.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "CymbolismExtended.svg"), x=W / 2, y=125.6, cap=2.6, bg=BG)
