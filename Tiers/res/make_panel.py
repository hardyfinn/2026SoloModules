#!/usr/bin/env python3
# Draws res/Tiers.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Tiers.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#17202B"
BG_DARK = "#10161E"
LINE = "#2E3A48"
INK = "#F1E9D6"
SOFT = "#A9B3C1"
CORAL = "#FF6B4A"
# one colour per metric level: 1/2, 1/4, 1/8, 1/16, 1/32 (same in the screen)
LEVEL_COL = ["#FF6B4A", "#FFB341", "#E8DC5A", "#7DE2C3", "#78A9FF"]

# ---- LAYOUT (same numbers in src/Tiers.cpp)
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 13.5, 140.4, 23.0
COL0, COLDX = 18.0, 16.6
GUT_X = 7.4
KNOB_Y, CV_Y = 51.5, 63.5
ROW3_Y = 82.5
IN_Y, OUT_Y = 100.5, 117.5

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
add(text("Tiers", 6.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("metric gate patterns", W - 6.0, 10.2, 2.8, FONT_MED, CORAL, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#06080B" stroke="{CORAL}" stroke-opacity="0.5" stroke-width="0.35"/>')

# ---- weights: how often each metric level plays
add(text("HOW OFTEN EACH LEVEL PLAYS", (colx(0) + colx(4)) / 2, KNOB_Y - 11.0, 2.6, FONT_MED, SOFT))
for i, n in enumerate(["1/2", "1/4", "1/8", "1/16", "1/32"]):
    add(text(n, colx(i), KNOB_Y - 6.2, CONTROL, fill=LEVEL_COL[i]))
for i, n in enumerate(["HITS", "ACCENT", "ROTATE"]):
    add(text(n, colx(5 + i), KNOB_Y - 6.2, CONTROL))
add(text("count", colx(5), KNOB_Y + 7.0, SMALL, FONT_MED, SOFT))
add(f'<line x1="{(colx(4) + colx(5)) / 2}" y1="{KNOB_Y - 12}" x2="{(colx(4) + colx(5)) / 2}" y2="{CV_Y - 5}" '
    f'stroke="{LINE}" stroke-width="0.4"/>')
# CV row
add(f'<rect x="{colx(0) - 7.0}" y="{CV_Y - 4.6}" width="{COLDX * 7 + 14.0}" height="9.2" rx="1.4" fill="{BG_DARK}"/>')
add(text("CV", GUT_X - 0.6, CV_Y + 1.0, JACK, fill=SOFT))

add(f'<line x1="4" y1="71.8" x2="{W - 4}" y2="71.8" stroke="{LINE}" stroke-width="0.4"/>')

# ---- row 3: actions + switches
for i, n in enumerate(["GEN", "MUTATE", "LOCK", "BACK", "PRESET", "LOAD"]):
    add(text(n, colx(i), ROW3_Y - 6.4, CONTROL, fill=CORAL if n in ("GEN", "LOAD") else INK))


def switch_labels(x, y, title, opts):
    add(text(title, x, y - 6.6, CONTROL))
    for yy, o in zip([y - 1.4, y + 3.6], opts):
        add(text(o, x + 2.6, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))


switch_labels(colx(6) - 2.0, ROW3_Y, "MODE", ["CHANCE", "COUNT"])
switch_labels(colx(7) - 2.0, ROW3_Y, "STEPS", ["32", "16"])

# ---- inputs
add(f'<rect x="{colx(0) - 7.0}" y="{IN_Y - 8.6}" width="{COLDX * 7 + 14.0}" height="13.2" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["CLOCK", "RESET", "GEN", "MUTATE", "LOCK", "BACK", "PRESET", "LOAD"]):
    add(text(n, colx(i), IN_Y - 5.0, JACK, fill=SOFT))
add(text("IN", GUT_X - 0.6, IN_Y + 1.0, JACK, fill=SOFT))

# ---- row 5: swing + grid, then outputs on a coral plate
add(text("SWING", colx(0), OUT_Y - 6.2, CONTROL))
add(text("50-75%", colx(0), OUT_Y + 7.0, SMALL, FONT_MED, SOFT))
add(text("STEP", colx(1) - 2.0, OUT_Y - 6.6, CONTROL))
for yy, o in zip([OUT_Y - 1.4, OUT_Y + 3.6], ["1/32", "1/16"]):
    add(text(o, colx(1) + 0.6, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(f'<rect x="{colx(2) - 7.4}" y="{OUT_Y - 8.4}" width="{COLDX * 5 + 14.8}" height="13.0" rx="1.4" fill="{CORAL}"/>')
for i, n in enumerate(["GATE", "TRIG", "INV", "ACC", "LEVEL", "EOC"]):
    add(text(n, colx(2 + i), OUT_Y - 4.9, JACK, fill=BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Tiers.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Tiers.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
