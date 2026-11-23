#!/usr/bin/env python3
# Draws res/Panorama.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Panorama.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP

BG = "#1D2A33"
BG_R = "#22323D"
BG_DARK = "#151F26"
LINE = "#3A4C58"
INK = "#F1E9D6"
SOFT = "#A9BAC4"
GOLD = "#FFC440"
CORAL = "#FF7A5A"

# ---- LAYOUT (same numbers in src/Panorama.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 15.0, 100.0, 66.0
KX = [120.0, 143.0, 166.0, 189.0]
ROW_Y = [27.0, 49.0, 71.0]
ED_Y = 94.0
ED_X = [14.0, 31.0, 48.0]
ROLL_BTN_X, TYPE_X, TUNE_X = 64.0, 79.0, 98.0
IN_Y = 116.5
IN_X = [12.0, 30.5, 49.0, 67.5, 86.0]
CV_Y = [91.0, 104.5, 118.0]

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
SPLIT_X = 108.0
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<rect x="{SPLIT_X}" y="15.0" width="{W - SPLIT_X}" height="{H - 15.0}" fill="{BG_R}"/>')
add(f'<line x1="{SPLIT_X}" y1="15.5" x2="{SPLIT_X}" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')

# ---- title + screen
add(text("Panorama", 6.0, 11.0, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("steel pan · tenor, double second, triple cello", W - 6.0, 10.6, 2.6, FONT_MED, GOLD, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0C1216" stroke="{GOLD}" stroke-opacity="0.5" stroke-width="0.35"/>')


def marks(x, y, lo, hi, col=SOFT):
    add(text(lo, x - 1.1, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + 1.1, y, SMALL, FONT_BOLD, col, anchor="start"))


# ---- right: 12 knobs
NAMES = [["STICK", "POSITION", "FORCE", "DECAY"],
         ["NEIGHBOURS", "BLOOM", "GLIDE", "DAMP"],
         ["DETUNE", "PARTIALS", "ROLL", "WIDTH"]]
RANGES = [[("SOFT", "HARD"), ("MID", "EDGE"), ("0", "MAX"), ("SHORT", "LONG")],
          [("0", "MAX"), ("0", "MAX"), ("FLAT", "SHARP"), ("0", "MAX")],
          [("0", "30c"), ("0", "50c"), ("OFF", "FAST"), ("MONO", "WIDE")]]
GOLDEN = {"NEIGHBOURS", "BLOOM", "DETUNE", "PARTIALS"}
for r in range(3):
    for c in range(4):
        n = NAMES[r][c]
        add(text(n, KX[c], ROW_Y[r] - 7.4, CONTROL, fill=GOLD if n in GOLDEN else INK))
        marks(KX[c], ROW_Y[r] + 9.0, *RANGES[r][c])
# tuning group bracket (DETUNE + PARTIALS)
add(f'<rect x="{KX[0] - 9.5}" y="{ROW_Y[2] + 10.4}" width="{KX[1] - KX[0] + 19.0}" height="0.6" rx="0.3" fill="{GOLD}" fill-opacity="0.7"/>')

# ---- CV jacks (right, under the knobs)
add(f'<rect x="{SPLIT_X + 2.4}" y="83.8" width="{W - SPLIT_X - 4.8}" height="40.4" rx="1.4" fill="{BG_DARK}"/>')
CVN = [["STICK", "POSITION", "DECAY", "GLIDE"],
       ["NEIGHBOURS", "BLOOM", "DAMP", "ROLL SPD"],
       ["DETUNE", "PARTIALS", None, None]]
for r in range(3):
    for c in range(4):
        if CVN[r][c]:
            add(text(CVN[r][c], KX[c], CV_Y[r] - 5.4, JACK, FONT_BOLD, SOFT))
# outputs
ox0, ox1 = KX[2] - 8.6, KX[3] + 8.6
add(f'<rect x="{ox0}" y="{CV_Y[2] - 9.0}" width="{ox1 - ox0}" height="14.0" rx="1.4" fill="{GOLD}"/>')
add(text("OUT L", KX[2], CV_Y[2] - 5.4, JACK, fill=BG))
add(text("R", KX[3], CV_Y[2] - 5.4, JACK, fill=BG))

# ---- left: tuning editor strip
add(f'<rect x="3.4" y="84.6" width="{SPLIT_X - 6.8}" height="16.4" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(ED_X, ["FUND", "OCT", "5TH"]):
    add(text(n, x, ED_Y - 5.3, CONTROL, fill=GOLD))
add(text("tune the note picked on the screen", ED_X[1], ED_Y + 6.2, SMALL, FONT_MED, SOFT))
add(text("RE-ROLL", ROLL_BTN_X, ED_Y - 5.3, CONTROL))
add(text("PAN", TYPE_X, ED_Y - 5.3, CONTROL))
for yy, o in zip([ED_Y - 2.4, ED_Y + 1.0, ED_Y + 4.4], ["TENOR", "2ND", "CELLO"]):
    add(text(o, TYPE_X + 2.6, yy, 2.3, FONT_BOLD, SOFT, anchor="start"))
add(text("TUNE", TUNE_X, ED_Y - 5.3, CONTROL))
add(text("±100c", TUNE_X, ED_Y + 6.2, SMALL, FONT_MED, SOFT))

# ---- left: main inputs
for x, n in zip(IN_X, ["V/OCT", "GATE", "VEL", "MUTE", "ROLL"]):
    add(text(n, x, IN_Y - 5.6, JACK, fill=CORAL if n in ("V/OCT", "GATE") else INK))
add(text("POLY", (IN_X[0] + IN_X[1]) / 2, IN_Y + 7.0, SMALL, FONT_BOLD, CORAL))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Panorama.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Panorama.svg"), x=100.0, y=126.0, cap=3.0, bg=BG)
