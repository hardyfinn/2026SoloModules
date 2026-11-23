#!/usr/bin/env python3
# Draws res/Lilt.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Lilt.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 182.88, 128.5  # 36 HP

BG = "#2A1F2D"
BG_R = "#33263A"
BG_DARK = "#1C1420"
LINE = "#4A3A52"
INK = "#F1E9D6"
SOFT = "#BFB0C6"
CH = ["#FF6F59", "#FFC145", "#5BE0B3", "#7AB8FF"]  # threshold channel colours (same on screen)

# ---- LAYOUT (same numbers in src/Lilt.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 14.5, 170.88, 19.0
GX = [14.0, 38.0, 62.0, 86.0]
ROW1_Y = 50.0
ROW2_Y = 73.0
SNAP_X, DIR_X = 79.0, 94.0
CV_X0, CV_DX = 9.0, 17.4
TRIM_Y = 92.5
CVJ_Y = 101.0
IO_Y = 117.0
SPLIT_X = 106.0
LX = [115.5, 134.0, 152.5, 171.0]
RATIO_Y = 48.5
LEVEL_Y = 67.0
LFOJ_Y = 79.6

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
add(f'<rect x="{SPLIT_X}" y="36.0" width="{W - SPLIT_X}" height="{H - 36.0}" fill="{BG_R}"/>')
add(f'<line x1="{SPLIT_X}" y1="36.5" x2="{SPLIT_X}" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')

# ---- title + screen
add(text("Lilt", 6.0, 11.0, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("relabi generator · four LFOs that lean on the pulse", W - 6.0, 10.6, 2.6, FONT_MED, CH[1], anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0B070D" stroke="{CH[1]}" stroke-opacity="0.55" stroke-width="0.35"/>')


def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


# ---- global row 1 (large knobs)
for x, n in zip(GX, ["PULSE", "DRIFT", "MUTATE", "GRID"]):
    add(text(n, x, ROW1_Y - 9.4, CONTROL, fill=CH[1] if n in ("DRIFT", "MUTATE") else INK))
ranges(GX[0], ROW1_Y + 9.6, 3.2, "20", "300")
ranges(GX[1], ROW1_Y + 9.6, 3.2, "TIGHT", "LOOSE")
ranges(GX[2], ROW1_Y + 9.6, 3.2, "STILL", "ALIVE")
ranges(GX[3], ROW1_Y + 9.6, 3.2, "FREE", "GRID")
# row 2
for x, n in zip(GX[:3], ["SHAPE", "THRESH", "SPREAD"]):
    add(text(n, x, ROW2_Y - 7.8, CONTROL))
ranges(GX[0], ROW2_Y + 7.6, 4.6, "SINE", "SQR")
ranges(GX[1], ROW2_Y + 7.6, 4.6, "−", "+")
ranges(GX[2], ROW2_Y + 7.6, 4.6, "0", "WIDE")


def switch_labels(x, y, title, opts, three=False):
    add(text(title, x, y - (7.4 if three else 6.0), CONTROL))
    ys = [y - 3.0, y + 1.0, y + 5.0] if three else [y - 1.4, y + 3.6]
    for yy, o in zip(ys, opts):
        add(text(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))


switch_labels(SNAP_X, ROW2_Y, "SNAP", ["ON", "OFF"])
switch_labels(DIR_X, ROW2_Y, "EDGE", ["UP", "BOTH", "DOWN"], three=True)

# ---- CV strip (left)
add(f'<rect x="2.6" y="84.4" width="{SPLIT_X - 5.2}" height="23.6" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["PULSE", "DRIFT", "MUTATE", "THRESH", "SPREAD", "GRID"]):
    add(text(n, CV_X0 + CV_DX * i, 88.4, JACK, FONT_BOLD, SOFT))
# inputs / outputs
for i, n in enumerate(["CLOCK", "RESET", "FREEZE"]):
    add(text(n, CV_X0 + CV_DX * i, IO_Y - 5.0, JACK))
ox0 = CV_X0 + CV_DX * 3
add(f'<rect x="{ox0 - 7.9}" y="{IO_Y - 8.6}" width="{CV_DX * 2 + 15.8}" height="13.6" rx="1.4" fill="{CH[1]}"/>')
for i, n in enumerate(["BEAT", "SUM", "ACC"]):
    add(text(n, ox0 + CV_DX * i, IO_Y - 5.0, JACK, fill=BG))

# ---- LFO columns (right)
for i, x in enumerate(LX):
    add(text(f"LFO {i + 1}", x, 40.6, CONTROL, FONT_BOLD, INK))
    add(text("RATIO", x, RATIO_Y + 7.9, JACK, FONT_BOLD, SOFT))
    add(text("LEVEL", x, LEVEL_Y - 5.6, CONTROL))
    add(text("OUT", x, LFOJ_Y - 5.0, JACK, FONT_BOLD, SOFT))
# trigger / gate block
add(f'<rect x="{SPLIT_X + 2.4}" y="86.0" width="{W - SPLIT_X - 5.0}" height="36.6" rx="1.4" fill="{BG_DARK}"/>')
for i, x in enumerate(LX):
    add(text(f"TRIG {i + 1}", x, CVJ_Y - 5.4, JACK, FONT_BOLD, CH[i]))
    add(text(f"GATE {i + 1}", x, IO_Y - 5.4, JACK, FONT_BOLD, CH[i]))
    add(f'<rect x="{x - 4.4}" y="{CVJ_Y - 9.9}" width="8.8" height="0.7" rx="0.3" fill="{CH[i]}"/>')

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Lilt.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Lilt.svg"), x=53.0, y=126.0, cap=3.0, bg=BG)
