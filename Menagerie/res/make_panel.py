#!/usr/bin/env python3
# Draws res/Menagerie.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT numbers are shared with src/Menagerie.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import haer_logo

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
H = 128.5

# warm cream with a deep green
BG = "#EEEADF"
STRIP = "#DDD7C8"
LINE = "#C2BAA6"
INK = "#1E2621"
SOFT = "#6A6F63"
GREEN = "#2E6B48"
WHITE = "#FFFFFF"

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


# =========================================================== MENAGERIE (18 HP)
W = 91.44
# ---- LAYOUT (same numbers in src/Menagerie.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 15.5, 81.44, 17.0
ROWA_Y = 47.0
XA = [18.0, 45.72, 73.44]
ROWB_Y = 70.0
XB = [16.0, 45.72, 75.44]
ROWC_Y = 85.5
JX = [10.5, 28.1, 45.72, 63.3, 81.0]
IN1_Y = 102.0
IN2_Y = 116.5

P = []
add = P.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Menagerie", 5.0, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("16 engines", W - 5.0, 11.0, 2.6, FONT_MED, GREEN, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#121813" stroke="{GREEN}" stroke-width="0.4"/>')


def ranges(x, y, r, lo, hi, col=SOFT):
    if lo: add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    if hi: add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


# row A: FREQ, DECAY, MODEL
for x, n in zip(XA, ["FREQ", "DECAY", "MODEL"]):
    add(text(n, x, ROWA_Y - 9.4, CONTROL, fill=GREEN if n == "MODEL" else INK))
ranges(XA[0], ROWA_Y + 8.8, 5.4, "-4", "+4")
ranges(XA[1], ROWA_Y + 7.6, 4.4, "SHORT", "LONG")
ranges(XA[2], ROWA_Y + 8.8, 5.4, "1", "16")
# row B: the three macros
for x, n in zip(XB, ["SHAPE", "TEXTURE", "LIFE"]):
    add(text(n, x, ROWB_Y - 9.4, CONTROL, fill=GREEN if n == "LIFE" else INK))
# row C: attenuverters for the macro CVs
for x in XB:
    add(text("−", x - 5.2, ROWC_Y + 1.0, SMALL, FONT_BOLD, SOFT))
    add(text("+", x + 5.2, ROWC_Y + 1.0, SMALL, FONT_BOLD, SOFT))
add(text("CV", (XB[0] + XB[1]) / 2, ROWC_Y + 1.0, SMALL, FONT_BOLD, SOFT))
add(text("CV", (XB[1] + XB[2]) / 2, ROWC_Y + 1.0, SMALL, FONT_BOLD, SOFT))

# jack strips: inputs (two rows) and the two outputs in the right column
add(f'<rect x="2.2" y="{IN1_Y - 9.0}" width="{JX[3] + 8.2 - 2.2}" height="13.4" rx="1.4" fill="{STRIP}"/>')
add(f'<rect x="2.2" y="{IN2_Y - 9.0}" width="{JX[3] + 8.2 - 2.2}" height="13.4" rx="1.4" fill="{STRIP}"/>')
add(f'<rect x="{JX[4] - 8.0}" y="{IN1_Y - 9.0}" width="16.0" height="{IN2_Y - IN1_Y + 13.4}" rx="1.4" fill="{GREEN}"/>')
for x, n in zip(JX[:4], ["V/OCT", "TRIG", "LEVEL", "MODEL"]):
    add(text(n, x, IN1_Y - 5.4, JACK, FONT_BOLD, INK))
for x, n in zip(JX[:4], ["SHAPE", "TEXTURE", "LIFE", "DECAY"]):
    add(text(n, x, IN2_Y - 5.4, JACK, FONT_BOLD, INK))
add(text("AUX", JX[4], IN1_Y - 5.4, JACK, FONT_BOLD, WHITE))
add(text("MAIN", JX[4], IN2_Y - 5.4, JACK, FONT_BOLD, WHITE))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in P if p) + "\n</svg>\n")
path = os.path.join(HERE, "Menagerie.svg")
open(path, "w").write(svg)
# Haer maker logo
haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=BG)
print("ok")
