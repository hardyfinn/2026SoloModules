#!/usr/bin/env python3
# Draws res/Groover.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Groover.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 101.6, 128.5  # 20 HP

BG = "#1E2A2F"
BG_DARK = "#152025"
LINE = "#34474E"
INK = "#EFE8D8"
SOFT = "#9FB3B5"
ORANGE = "#FF9F1C"

# ---- LAYOUT (same numbers in src/Groover.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.5, 91.6, 23.0
KX = [13.0, 47.0, 81.0]
CVX = [25.0, 59.0, 93.0]
ROW1_Y, ROW2_Y = 51.0, 72.0
IO_Y = 91.0
CLK_X, RST_X, LOAD_X = 10.0, 24.0, 41.0
OCLK_X, OACC_X, OLOOP_X = 64.5, 78.5, 92.0
LIN_Y, LOUT_Y = 104.5, 118.0
LANE_X0, LANE_DX = 9.0, 11.94

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

# ---- title + screen
add(text("Groover", 5.0, 11.0, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("Ableton grooves for your drums", W - 5.0, 10.6, 2.6, FONT_MED, ORANGE, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0A1013" stroke="{ORANGE}" stroke-opacity="0.55" stroke-width="0.35"/>')


def ranges(x, y, lo, hi, r=5.6):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


rows = [(ROW1_Y, ["GROOVE", "AMOUNT", "TIMING"], [None, ("0", "130"), ("0", "100")]),
        (ROW2_Y, ["VELOCITY", "RANDOM", "QUANTIZE"], [("−", "+"), ("0", "100"), ("0", "100")])]
for y, names, rg in rows:
    for i in range(3):
        add(text(names[i], (KX[i] + CVX[i]) / 2, y - 8.2, CONTROL, fill=ORANGE if names[i] == "GROOVE" else INK))
        if rg[i]:
            ranges(KX[i], y + 7.4, *rg[i])
    add(f'<line x1="4" y1="{y + 10.4}" x2="{W - 4}" y2="{y + 10.4}" stroke="{LINE}" stroke-width="0.3"/>')

# ---- clock row
add(text("CLOCK", CLK_X, IO_Y - 5.4, JACK))
add(text("RESET", RST_X, IO_Y - 5.4, JACK))
add(text("LOAD", LOAD_X, IO_Y - 5.4, JACK, fill=ORANGE))
ox0, ox1 = OCLK_X - 7.0, W - 2.6
add(f'<rect x="{ox0}" y="{IO_Y - 8.8}" width="{ox1 - ox0}" height="13.6" rx="1.4" fill="{ORANGE}"/>')
add(text("CLOCK", OCLK_X, IO_Y - 5.4, JACK, fill=BG))
add(text("ACCENT", OACC_X, IO_Y - 5.4, JACK, fill=BG))
add(text("LOOP", OLOOP_X, IO_Y - 5.4, JACK, fill=BG))

# ---- lanes
add(f'<rect x="2.6" y="{LIN_Y - 8.2}" width="{W - 5.2}" height="{LOUT_Y - LIN_Y + 12.6}" rx="1.4" fill="{BG_DARK}"/>')
add(f'<rect x="3.4" y="{LOUT_Y - 8.4}" width="{W - 6.8}" height="12.6" rx="1.2" fill="{ORANGE}" fill-opacity="0.9"/>')
for i in range(8):
    x = LANE_X0 + LANE_DX * i
    add(text(f"IN {i + 1}", x, LIN_Y - 5.2, JACK, FONT_BOLD, SOFT))
    add(text(f"OUT {i + 1}", x, LOUT_Y - 5.0, JACK, FONT_BOLD, BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Groover.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Groover.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
