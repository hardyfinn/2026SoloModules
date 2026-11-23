#!/usr/bin/env python3
# Draws res/Pivot.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Pivot.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#1B1E24"
BG_DARK = "#121418"
LINE = "#343945"
INK = "#F1E9D6"
SOFT = "#A9AEBB"
AMBER = "#FFB341"
MINT = "#7DE2C3"

# ---- LAYOUT (same numbers in src/Pivot.cpp)
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 13.5, 140.4, 22.0
GUT_X = 9.5
COL0, COLDX = 24.0, 16.6
BTN_Y, OLD_Y, NEW_Y = 45.5, 57.0, 70.0
ROWA_Y, ROWB_Y, ROWC_Y = 85.5, 104.0, 118.5

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5     # switch positions, units


def colx(i):
    return COL0 + COLDX * i


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

# ---- title + screen
add(text("Pivot", 6.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("metric modulation", W - 6.0, 10.2, 2.8, FONT_MED, AMBER, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#06070A" stroke="{AMBER}" stroke-opacity="0.55" stroke-width="0.35"/>')

# ---- chain: HOME, then steps 1-8, each OLD = NEW
add(text("HOME", GUT_X, BTN_Y - 5.4, CONTROL, fill=MINT))
for i in range(8):
    add(text(str(i + 1), colx(i), BTN_Y - 5.4, CONTROL, fill=AMBER))
add(text("OLD", GUT_X, OLD_Y + 1.15, CONTROL))
add(text("NEW", GUT_X, NEW_Y + 1.15, CONTROL))
for i in range(8):
    add(text("=", colx(i), (OLD_Y + NEW_Y) / 2 + 1.2, 3.4, FONT_BOLD, SOFT))
add(text("=", GUT_X, (OLD_Y + NEW_Y) / 2 + 1.2, 3.4, FONT_BOLD, SOFT))
# a thin rail joining the step buttons, like a chain
add(f'<line x1="{GUT_X}" y1="{BTN_Y}" x2="{colx(7)}" y2="{BTN_Y}" stroke="{LINE}" stroke-width="0.6"/>')

add(f'<line x1="4" y1="76.3" x2="{W - 4}" y2="76.3" stroke="{LINE}" stroke-width="0.4"/>')

# ---- row A
for i, n in enumerate(["BPM", "BAR", "IN", "OUT", "LENGTH", "AUTO", "GLIDE", "NEXT"]):
    add(text(n, colx(i), ROWA_Y - 6.0, CONTROL, fill=AMBER if n == "NEXT" else INK))
add(text("beats", colx(1), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))
add(text("ppqn", colx(2), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))
add(text("ppqn", colx(3), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))
add(text("steps", colx(4), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))
add(text("bars", colx(5), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))
add(text("no clock", colx(0), ROWA_Y + 6.6, SMALL, FONT_MED, SOFT))


# ---- row B: switches
def switch_labels(x, y, title, opts, three=False):
    add(text(title, x + 1.4, y - (8.0 if three else 6.6), CONTROL))
    ys = [y - 3.0, y + 1.0, y + 5.0] if three else [y - 1.4, y + 3.6]
    for yy, o in zip(ys, opts):
        add(text(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))


switch_labels(colx(0) - 3.0, ROWB_Y, "QUANT", ["BAR", "BEAT", "NOW"], three=True)
switch_labels(colx(1) - 3.0, ROWB_Y, "AT END", ["HOLD", "LOOP", "HOME"], three=True)
switch_labels(colx(2) - 3.0, ROWB_Y, "LAND", ["ON", "OFF"])

# inputs
add(f'<rect x="{colx(3) - 7.4}" y="{ROWB_Y - 9.0}" width="{COLDX * 4 + 14.8}" height="14.0" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["CLOCK", "RESET", "NEXT", "HOME", "SELECT"]):
    add(text(n, colx(3 + i), ROWB_Y - 5.2, JACK, fill=SOFT))

# ---- row C: outputs on an amber plate
add(f'<rect x="{colx(0) - 7.6}" y="{ROWC_Y - 8.6}" width="{COLDX * 7 + 15.2}" height="13.4" rx="1.4" fill="{AMBER}"/>')
for i, n in enumerate(["CLOCK", "BEAT", "BAR", "RESET", "PIVOT", "GHOST", "RATIO", "AWAY"]):
    add(text(n, colx(i), ROWC_Y - 5.0, JACK, fill=BG))
add(text("OUT", GUT_X, ROWC_Y + 1.0, JACK, FONT_BOLD, AMBER))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Pivot.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Pivot.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
