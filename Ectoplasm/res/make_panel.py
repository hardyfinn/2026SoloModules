#!/usr/bin/env python3
# Draws res/Ectoplasm.svg (24 HP). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Ectoplasm.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 121.92, 128.5  # 24 HP

BG = "#1C2440"
BG_DARK = "#121830"
TAPBAND = "#3A3226"
MODEBAND = "#1F3A3A"
LINE = "#3A4366"
INK = "#EFE6D2"
SOFT = "#9AA3C8"
AMBER = "#E8A33A"
TEAL = "#5FD0B5"

# ---- LAYOUT (same numbers in src/Ectoplasm.cpp)
DISP_X, DISP_Y, DISP_W, DISP_H = 7.0, 13.5, 107.92, 10.0
BREAK_X, BREAK_Y, RING_R = 60.96, 40.0, 12.8
GRIM_X, AMEN_X, TOP_Y = 17.0, 104.92, 35.0
BTN_Y = 49.5
BTN_X = [11.0, 24.0, 97.92, 110.92]
ROW2_Y = 64.0
ROW2_X = [17.0, 41.0, 80.92, 104.92]
LAYER_X = [15.0, 38.0, 60.96, 83.92, 106.92]
TAP_Y, MODE_Y = 79.6, 94.1
JACK_X0, JACK_DX, JACK_Y1, JACK_Y2 = 8.0, 13.24, 107.5, 119.0
CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3

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
add(f'<rect width="{W}" height="3" fill="{BG_DARK}"/><rect y="{H - 3}" width="{W}" height="3" fill="{BG_DARK}"/>')

# ---- title + display
add(text("Ectoplasm", 6.0, 10.6, 6.0, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("Ectocore firmware by Zack Scholl", W - 6.0, 7.6, SMALL, FONT_MED, SOFT, anchor="end"))
add(text("Infinite Digits · GPL-3", W - 6.0, 10.6, SMALL, FONT_MED, SOFT, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#05070F" stroke="{TEAL}" stroke-opacity="0.6" stroke-width="0.35"/>')

# ---- top block
add(f'<circle cx="{BREAK_X}" cy="{BREAK_Y}" r="{RING_R + 2.4}" fill="{BG_DARK}"/>')
add(text("BREAK", BREAK_X, BREAK_Y + 16.2, CONTROL, fill=AMBER))
add(text("GRIMOIRE", GRIM_X, TOP_Y - 7.5, CONTROL))
add(text("AMEN", AMEN_X, TOP_Y - 7.5, CONTROL))
for x, n, col in zip(BTN_X, ["MODE", "MULT", "TAP", "PLAY"], [TEAL, INK, AMBER, INK]):
    add(text(n, x, BTN_Y - 5.0, CONTROL, fill=col))

# ---- row 2
for x, n in zip(ROW2_X, ["SAMPLE", "BANK", "JUMP", "TEMPO"]):
    add(text(n, x, ROW2_Y - 7.4, CONTROL))

# ---- hidden layers: the hardware's hold-TAP and hold-MODE functions
add(f'<rect x="2.4" y="70.2" width="{W - 4.8}" height="14.4" rx="1.4" fill="{TAPBAND}"/>')
add(f'<rect x="2.4" y="84.6" width="{W - 4.8}" height="14.4" rx="1.4" fill="{MODEBAND}"/>')
add(f'<g transform="rotate(-90 5.0 77.4)">{text("TAP", 5.0, 77.4 + 0.9, 2.4, FONT_BOLD, AMBER)}</g>')
add(f'<g transform="rotate(-90 5.0 91.8)">{text("MODE", 5.0, 91.8 + 0.9, 2.4, FONT_BOLD, TEAL)}</g>')
for x, n in zip(LAYER_X, ["DRIVE", "FILTER", "GATE", "RETRIG", "PITCH"]):
    add(text(n, x, TAP_Y - 5.9, CONTROL))
for x, n in zip(LAYER_X, ["AMIGA", "SATURATE", "SMEAR", "JITTER", "CHAOS"]):
    add(text(n, x, MODE_Y - 5.9, CONTROL))

# ---- jacks
row1 = ["CLOCK", "RESET", "PLAY", "AMEN", "BREAK", "SAMPLE", "BANK", "GRIM", "TEMPO"]
row2 = ["DRIVE", "FILTER", "GATE", "RETRIG", "PITCH"]
for i, n in enumerate(row1):
    add(text(n, JACK_X0 + i * JACK_DX, JACK_Y1 - 4.9, JACK))
for i, n in enumerate(row2):
    add(text(n, JACK_X0 + i * JACK_DX, JACK_Y2 - 4.9, JACK))
ox = JACK_X0 + 5 * JACK_DX
add(f'<rect x="{ox - 6.2}" y="{JACK_Y2 - 7.9}" width="{3 * JACK_DX + 12.4}" height="12.1" rx="1.4" fill="{AMBER}"/>')
for i, n in enumerate(["TRIG", "CLK", "L", "R"]):
    add(text(n, ox + i * JACK_DX, JACK_Y2 - 4.9, JACK, fill=BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Ectoplasm.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Ectoplasm.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG_DARK)
