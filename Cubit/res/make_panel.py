#!/usr/bin/env python3
# Draws res/Cubit.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Cubit.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "Cubit"
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

HP = 26                       # widen HP rather than cram
W, H = HP * 5.08, 128.5

BG = "#1F2534"        # deep blue-grey, like a solder-masked board
BG_DARK = "#141925"
INK = "#ECE6D6"
SOFT = "#9AA4B8"
ACCENT = "#7BE3A2"    # the digits' green

# ---- LAYOUT (same numbers in src/Cubit.cpp) ----
ROW1_Y = 27.0
FREQ_X, FINE_X, RANGE_X, EXPO_X, LIN_X, DRIFT_X = 13.0, 30.0, 43.0, 57.0, 70.0, 83.0
MUTATE_X, MUTBTN_X, LOCK_X = 98.0, 112.0, 123.0
DIG_Y = 48.0
DIG_X = [33.0, 66.0, 99.0]
DIG_W, DIG_H = 15.0, 17.0
TC_Y = 65.5
TC_DX = 6.5
RESET_X, MUTIN_X = 12.0, 120.0
BEND_Y = 84.0
BEND_X = [16.0, 36.0, 56.0, 76.0, 96.0, 116.0]
IN_Y = 101.0
IN_X = [10.0 + 14.0 * i for i in range(9)]
OUT_Y = 115.0
OUT_X = [10.0 + 16.0 * i for i in range(8)]

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)

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
add(f'<rect x="0" y="0" width="{W}" height="2.2" fill="{BG_DARK}"/>')
add(text("CUBIT", W / 2, 10.6, 4.6, FONT_BOLD, INK, spacing=0.18))
add(text("8-BIT WAVETABLE VCO", W / 2, 15.0, 2.4, FONT_MED, SOFT, spacing=0.1))

# row 1: clock and mutation
add(text("FREQ", FREQ_X, ROW1_Y - 9.6, CONTROL))
for x, lab in [(FINE_X, "FINE"), (EXPO_X, "EXPO"), (LIN_X, "LIN"), (DRIFT_X, "DRIFT"), (MUTATE_X, "MUTATE")]:
    add(text(lab, x, ROW1_Y - 7.6, CONTROL))
add(text("RANGE", RANGE_X, ROW1_Y - 7.6, CONTROL))
add(text("HI", RANGE_X + 2.6, ROW1_Y - 1.6, 2.2, FONT_MED, SOFT, anchor="start"))
add(text("LO", RANGE_X + 2.6, ROW1_Y + 3.6, 2.2, FONT_MED, SOFT, anchor="start"))
add(text("MUT", MUTBTN_X, ROW1_Y - 7.6, CONTROL))
add(text("LOCK", LOCK_X, ROW1_Y - 7.6, CONTROL))
add(f'<rect x="{MUTATE_X - 8.5}" y="{ROW1_Y - 12.4}" width="0.35" height="18.0" fill="{SOFT}"/>')

# centre: the cube
add(f'<rect x="3.0" y="34.0" width="{W - 6.0}" height="40.0" rx="1.6" fill="{BG_DARK}"/>')
for x, lab in zip(DIG_X, "ABC"):
    add(text(lab, x, DIG_Y - DIG_H / 2 - 2.0, 4.0, FONT_BOLD, ACCENT))
    add(f'<rect x="{x - DIG_W / 2 - 0.8}" y="{DIG_Y - DIG_H / 2 - 0.8}" width="{DIG_W + 1.6}" height="{DIG_H + 1.6}" '
        f'rx="1.8" fill="none" stroke="{SOFT}" stroke-width="0.35"/>')
    add(text("TRIG", x - TC_DX, TC_Y - 5.6, JACK, fill=SOFT))
    add(text("CV", x + TC_DX, TC_Y - 5.6, JACK, fill=SOFT))
add(text("RESET", RESET_X, TC_Y - 5.6, JACK, fill=SOFT))
add(text("A TO 0", RESET_X, TC_Y + 6.8, 2.0, FONT_MED, SOFT))
add(text("MUT", MUTIN_X, TC_Y - 5.6, JACK, fill=SOFT))
add(text("WAVE = C·64 + B·8 + A", W / 2, TC_Y + 6.8, 2.0, FONT_MED, SOFT))

# bends
for x, lab in zip(BEND_X, ["SPLICE", "WITH", "BIT", "STUCK", "RES", "DEPTH"]):
    add(text(lab, x, BEND_Y - 7.6, CONTROL))
for dy, lab in [(-3.3, "HI"), (0.0, "OFF"), (3.3, "LO")]:
    add(text(lab, BEND_X[3] + 2.6, BEND_Y + dy + 0.8, 2.2, FONT_MED, SOFT, anchor="start"))
for x, lo, hi in [(BEND_X[0], "OFF", "1/64"), (BEND_X[1], "A1", "C4"), (BEND_X[2], "0", "7"),
                  (BEND_X[4], "64", "4"), (BEND_X[5], "8", "1")]:
    add(text(lo, x - 4.6, BEND_Y + 7.4, 2.0, FONT_MED, SOFT, anchor="end"))
    add(text(hi, x + 4.6, BEND_Y + 7.4, 2.0, FONT_MED, SOFT, anchor="start"))

# inputs
for x, lab in zip(IN_X, ["V/OCT", "EXPO", "LIN", "SYNC", "CLK IN", "VCA", "SPLICE", "RES", "DEPTH"]):
    add(text(lab, x, IN_Y - 5.6, JACK))

# outputs
add(f'<rect x="2.5" y="{OUT_Y - 9.6}" width="{W - 5.0}" height="14.6" rx="1.4" fill="{BG_DARK}"/>')
for x, lab in zip(OUT_X, ["OUT", "UNI", "SUB", "TAP 1", "TAP 2", "TAP 3", "TAP 4", "CLK"]):
    add(text(lab, x, OUT_Y - 5.6, JACK, fill=ACCENT if lab == "OUT" else INK))

out = os.path.join(HERE, f"{NAME}.svg")
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(out, "w").write(svg)

# Haer maker logo on the bottom rail (required on every panel)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=W / 2, y=125.9, cap=2.6, bg=BG)
print("ok", out)
