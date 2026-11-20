#!/usr/bin/env python3
# Draws res/Heathrow.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Heathrow.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 213.36, 128.5  # 42 HP

BG = "#1F2340"
BG_B = "#262A4E"
BG_DARK = "#151830"
LINE = "#3A3F6A"
INK = "#F1E9D6"
SOFT = "#B9B3C8"
ORANGE = "#FF9A3C"
CYAN = "#6FD3FF"

# ---- LAYOUT (same numbers in src/Heathrow.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 7.0, 15.0, 128.24, 17.0
COLX = [15.12, 43.12, 71.12, 99.12, 127.12]
ROW1_Y = 46.0
ROW2_Y = 67.0
ROW3_Y = 84.5
CV_X0, CV_DX = 8.995, 17.75
TRIM_Y = 97.6
CVJ_Y = 105.4
IO_Y = 118.0
BX = [151.235, 168.985, 186.735, 204.485]
BSW_Y = 25.0
BK1_Y = 44.0
BK2_Y = 61.5
MEM_Y = 79.0
MEMX0, MEMDX = 151.235, 10.65
MEM2_Y = 90.5

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
add(f'<rect x="142.24" y="0" width="{W - 142.24}" height="{H}" fill="{BG_B}"/>')
add(f'<line x1="142.24" y1="4" x2="142.24" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')

# ---- title
add(text("Heathrow", 6.0, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("phaser · flanger · through-zero", 136.24, 11.0, 2.6, FONT_MED, ORANGE, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#05060D" stroke="{CYAN}" stroke-opacity="0.6" stroke-width="0.35"/>')

def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))

# ---- engine A, row 1 (large knobs)
for x, n in zip(COLX, ["MORPH", "CENTER", "DEPTH", "RATE", "FEEDBACK"]):
    add(text(n, x, ROW1_Y - 9.4, CONTROL, fill=ORANGE if n in ("MORPH", "FEEDBACK") else INK))
ranges(COLX[0], ROW1_Y + 8.8, 5.2, "PHASE", "FLANGE")
ranges(COLX[4], ROW1_Y + 8.8, 5.2, "−", "+")
ranges(COLX[3], ROW1_Y + 8.8, 5.2, "SLOW", "FAST")
# row 2
for x, n in zip(COLX, ["STAGES", "SPREAD", "SHAPE", "STEPS", "MIX"]):
    add(text(n, x, ROW2_Y - 7.8, CONTROL, fill=ORANGE if n == "MIX" else INK))
ranges(COLX[0], ROW2_Y + 7.6, 4.6, "2", "24")
ranges(COLX[1], ROW2_Y + 7.6, 4.6, "0°", "180°")
ranges(COLX[2], ROW2_Y + 7.6, 4.6, "SINE", "S&H")
ranges(COLX[3], ROW2_Y + 7.6, 4.6, "OFF", "16")
ranges(COLX[4], ROW2_Y + 7.6, 4.6, "−WET", "+WET")
# row 3
for x, n in zip(COLX[:3], ["GLIDE", "ENV", "RES"]):
    add(text(n, x, ROW3_Y - 6.0, CONTROL))
ranges(COLX[1], ROW3_Y + 1.0, 5.4, "−", "+")

def switch_labels(x, y, title, opts, three=False):
    add(text(title, x, y - (7.4 if three else 6.0), CONTROL))
    ys = [y - 3.0, y + 1.0, y + 5.0] if three else [y - 1.4, y + 3.6]
    for yy, o in zip(ys, opts):
        add(text(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))

switch_labels(COLX[3] - 3.0, ROW3_Y, "TZ", ["ON", "OFF"])
switch_labels(COLX[4] - 3.0, ROW3_Y, "LFO", ["CLK", "FREE"])

# ---- A CV strip
add(f'<rect x="2.6" y="89.6" width="137.0" height="20.4" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["MORPH", "CENTER", "DEPTH", "RATE", "FDBK", "STAGES", "SPREAD", "MIX"]):
    add(text(n, CV_X0 + CV_DX * i, 93.4, JACK, FONT_BOLD, SOFT))
# inputs / outputs
for i, n in enumerate(["IN L", "IN R", "CLOCK", "RESET", "SWEEP"]):
    add(text(n, CV_X0 + CV_DX * i, IO_Y - 5.0, JACK))
ox0 = CV_X0 + CV_DX * 5
add(f'<rect x="{ox0 - 7.6}" y="{IO_Y - 8.6}" width="{CV_DX * 2 + 15.2}" height="13.6" rx="1.4" fill="{ORANGE}"/>')
for i, n in enumerate(["LFO", "OUT L", "OUT R"]):
    add(text(n, ox0 + CV_DX * i, IO_Y - 5.0, JACK, fill=BG))

# ---- engine B
add(text("ENGINE B", 146.4, 11.4, 4.4, FONT_BOLD, INK, anchor="start", spacing=0.03))
add(text("bi-phase", 209.0, 11.0, 2.6, FONT_MED, ORANGE, anchor="end"))
switch_labels(BX[0] - 2.0, BSW_Y, "B", ["ON", "OFF"])
switch_labels(BX[1] - 4.0, BSW_Y, "ROUTE", ["SPLIT", "PARALLEL", "SERIES"], three=True)
switch_labels(BX[2] + 2.0, BSW_Y, "B SWEEP", ["A INV", "A", "OWN"], three=True)
for x, n in zip(BX, ["MORPH", "CENTER", "DEPTH", "RATE"]):
    add(text(n, x, BK1_Y - 7.8, CONTROL))
for x, n in zip(BX[:3], ["FDBK", "STAGES", "MIX"]):
    add(text(n, x, BK2_Y - 7.8, CONTROL))

# ---- memory
add(f'<line x1="145.0" y1="68.4" x2="{W - 3.0}" y2="68.4" stroke="{LINE}" stroke-width="0.4"/>')
add(text("MEMORY", W - 4.0, BK2_Y + 2.0, CONTROL, FONT_BOLD, ORANGE, anchor="end"))
for i in range(6):
    add(text(str(i + 1), MEMX0 + MEMDX * i, MEM_Y - 4.7, CONTROL))
add(text("SAVE", BX[0], MEM2_Y - 5.0, CONTROL))
add(text("TIME", BX[1], MEM2_Y - 5.6, CONTROL))
add(text("SAVE, then tap", BX[2] - 5.0, MEM2_Y - 1.0, SMALL, FONT_MED, SOFT, anchor="start"))
add(text("a slot to store", BX[2] - 5.0, MEM2_Y + 2.6, SMALL, FONT_MED, SOFT, anchor="start"))

# ---- B CV strip
add(f'<rect x="144.6" y="96.6" width="{W - 147.2}" height="26.0" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(BX, ["MORPH", "CENTER", "DEPTH", "RATE"]):
    add(text(n, x, CVJ_Y - 5.0, JACK, FONT_BOLD, SOFT))
for x, n in zip(BX, ["FDBK", "STAGES", "MIX", "SLOT"]):
    add(text(n, x, IO_Y - 5.0, JACK, FONT_BOLD, ORANGE if n == "SLOT" else SOFT))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Heathrow.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Heathrow.svg"), x=71.12, y=126.0, cap=3.0, bg=BG)
