#!/usr/bin/env python3
# Draws res/CopyThat.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/CopyThat.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_XB = os.environ.get("PANEL_FONT_XB", "/usr/share/fonts/opentype/inter/Inter-ExtraBold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 71.12, 128.5  # 14 HP

BG = "#E9E2D0"
BG_LOW = "#D6CCB4"
INK = "#1B1B1B"
RED = "#7A2D1D"
SOFT = "#5E5648"
CREAM = "#F2E9D8"

# ---- LAYOUT (same numbers in src/CopyThat.cpp) ----
TUNE_X, TUNE_Y = 35.56, 29.6
TATT_X = 61.0
METER_Y, METER_X0, METER_DX = 42.0, 26.06, 4.75
LOCK_Y = 36.0
ROW2_Y, L_X, R_X = 54.0, 17.0, 54.12
ROW3_Y = 73.6
SW_Y, SW_X = 87.4, [12.0, 33.0, 54.0]
CV_Y, IO_Y = 102.5, 115.5
BAND_Y = 95.2
CV_X = [10.0, 27.0, 44.0, 61.0]
OUT_X = [29.0, 44.0, 59.0]

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4

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
add(f'<rect x="0" y="{BAND_Y}" width="{W}" height="{H - BAND_Y}" fill="{BG_LOW}"/>')

# title
add(text("COPY THAT", W / 2, 10.4, 5.4, FONT_XB, INK, spacing=0.03))
add(text("FM LINK · LOSS OF SIGNAL", W / 2, 14.8, 2.5, FONT_MED, RED))

# TUNE and its CV attenuverter
add(text("TUNE", TUNE_X, TUNE_Y - 10.6, CONTROL))
add(text("T CV", TATT_X, TUNE_Y - 5.4, CONTROL))
add(text("−", TATT_X - 4.6, TUNE_Y + 1.0, SMALL, fill=SOFT))
add(text("+", TATT_X + 4.6, TUNE_Y + 1.0, SMALL, fill=SOFT))
# signal meter
add(text("SIGNAL", TUNE_X, METER_Y + 4.6, SMALL, FONT_BOLD, SOFT))
add(text("LOCK", TATT_X, LOCK_Y + 4.4, SMALL, FONT_BOLD, SOFT))

# row 2 / 3
add(text("SHIFT", L_X, ROW2_Y - 8.8, CONTROL))
add(text("DRIVE", R_X, ROW2_Y - 8.8, CONTROL))
add(text("FINE", L_X - 7.8, ROW2_Y + 8.8, SMALL - 0.4, FONT_BOLD, SOFT, anchor="start"))
add(text("LOST", L_X + 7.8, ROW2_Y + 8.8, SMALL - 0.4, FONT_BOLD, SOFT, anchor="end"))
add(text("CLEAN", R_X - 7.8, ROW2_Y + 8.8, SMALL - 0.4, FONT_BOLD, SOFT, anchor="start"))
add(text("DIRTY", R_X + 7.8, ROW2_Y + 8.8, SMALL - 0.4, FONT_BOLD, SOFT, anchor="end"))
add(text("WET", L_X, ROW3_Y - 7.6, CONTROL))
add(text("DRY", R_X, ROW3_Y - 7.6, CONTROL))
add(text("wet is out", W / 2, ROW3_Y - 0.6, SMALL, FONT_MED, RED))
add(text("of phase", W / 2, ROW3_Y + 2.6, SMALL, FONT_MED, RED))

# switches (CKSS: lever up = value 1)
def switch(x, title, up, down):
    add(text(title, x, SW_Y - 5.4, CONTROL))
    add(text(up, x + 2.6, SW_Y - 1.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(down, x + 2.6, SW_Y + 3.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
switch(SW_X[0], "RANGE", "2", "1")
switch(SW_X[1], "VOICE", "ON", "OFF")
switch(SW_X[2], "PHASE", "0°", "180°")

# CV row
for x, n in zip(CV_X, ["TUNE", "SHIFT", "DRIVE", "FADE"]):
    add(text(n, x, CV_Y - 5.2, JACK))
add(text("IN", CV_X[0], IO_Y - 5.2, JACK))
add(f'<rect x="{OUT_X[0] - 6.8}" y="{IO_Y - 9.0}" width="{OUT_X[2] - OUT_X[0] + 13.6}" height="14.6" rx="1.2" fill="{INK}"/>')
for x, n in zip(OUT_X, ["OUT", "SIGNAL", "LOCK"]):
    add(text(n, x, IO_Y - 5.2, JACK, fill=CREAM))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
out = os.path.join(HERE, "CopyThat.svg")
open(out, "w").write(svg)

# --- Haer maker logo (res/haer_logo.py): top left, beside TUNE (the bottom rail holds jacks)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=10.0, y=TUNE_Y + 0.5, cap=3.0, bg=BG)
print("ok")
