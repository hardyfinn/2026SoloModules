#!/usr/bin/env python3
# Draws res/Lightning.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Lightning.cpp.
#   pip install fonttools ; python3 res/make_lightning_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 142.24, 128.5  # 28 HP

BG = "#1B1D22"
BAND = "#23262C"
INK = "#ECEAE4"
SOFT = "#9A9890"
AMBER = "#FFB233"
GREEN = "#7FE08A"
IOBG = "#2E2A22"

# ---- LAYOUT (same numbers in src/Lightning.cpp) ----
COL = [13.0, 32.0, 51.0, 70.0, 89.0, 108.0]
C5 = 129.5
DISP_Y, DISP_W, DISP_H = 19.5, 16.5, 13.0
KEEP_DX, SEND_DX, BTN_Y = -4.8, 5.0, 43.0
ROW = [58.0, 73.0, 88.0]
IO_TOP, IO_BOT = 104.0, 118.0

CONTROL = 3.2
JACK = 2.9
R_KNOB, R_JACK, R_BEZEL, R_BTN = 4.8, 4.0, 3.6, 3.05

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

# header
add(text("LIGHTNING", 4.2, 11.0, 5.0, FONT_BOLD, AMBER, anchor="start", spacing=0.12))
add(text("keeps Thunder's hits", 4.4, 15.8, 2.0, FONT_MED, SOFT, anchor="start"))

# slot area band + a frame around each waveform
add(f'<rect x="1.0" y="{DISP_Y - 2.0}" width="{W - 2.0}" height="{ROW[2] + 7.0 - DISP_Y + 2.0}" rx="1.2" fill="{BAND}"/>')
for x in COL:
    add(f'<rect x="{x - DISP_W / 2 - 0.5}" y="{DISP_Y - 0.5}" width="{DISP_W + 1.0}" height="{DISP_H + 1.0}" rx="1.4" fill="#000"/>')
    add(text("KEEP", x + KEEP_DX, BTN_Y - R_BEZEL - 1.4, JACK, fill=AMBER))
    add(text("SEND", x + SEND_DX, BTN_Y - R_BEZEL - 1.4, JACK, fill=GREEN))
    for r, n in zip(ROW, ["TUNE", "DECAY", "LEVEL"]):
        add(text(n, x, r - R_KNOB - 1.4, CONTROL))

# fifth column
add(text("THUNDER", C5, DISP_Y + 4.2, 2.3, FONT_BOLD, SOFT))
add(text("on the left", C5, DISP_Y + 7.4, 2.3, FONT_MED, SOFT))
add(text("or audio IN", C5, DISP_Y + 10.6, 2.3, FONT_MED, SOFT))
add(text("LINK", C5, BTN_Y - 3.6, JACK, fill=GREEN))
add(text("SENS", C5, ROW[0] - R_KNOB - 1.4, CONTROL))
add(text("IN level", C5, ROW[0] + R_KNOB + 3.2, 2.0, FONT_MED, SOFT))
add(text("MIX", C5, ROW[2] - R_KNOB - 1.4, CONTROL))

# IO block
add(f'<rect x="1.5" y="{IO_TOP - 8.6}" width="{W - 3.0}" height="{IO_BOT - IO_TOP + 14.2}" rx="1.4" fill="{IOBG}"/>')
for i, x in enumerate(COL):
    add(text(f"TRIG {i + 1}", x, IO_TOP - R_JACK - 1.4, JACK, fill=INK))
    add(text(f"OUT {i + 1}", x, IO_BOT - R_JACK - 1.4, JACK, fill=AMBER))
add(text("IN", C5, IO_TOP - R_JACK - 1.4, JACK, fill=INK))
add(text("MIX", C5, IO_BOT - R_JACK - 1.4, JACK, fill=AMBER))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Lightning.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Lightning.svg"), x=80.0, y=12.0, cap=3.0, bg=BG)
