#!/usr/bin/env python3
# Draws res/Perun.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Perun.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#1B1D22"
BAND = "#23262C"
INK = "#ECEAE4"
SOFT = "#9A9890"
AMBER = "#FFB233"
IOBG = "#2E2A22"

# ---- LAYOUT (same numbers in src/Perun.cpp) ----
COL = [11.4, 29.9, 48.4, 66.9, 85.4, 103.9, 122.4, 140.9]
ROW = [30.0, 55.0, 80.0, 105.0]
JACK_DY = 10.0
IOX = [71.0, 93.5, 116.0, 138.5]
IO_TOP, IO_BOT = 103.0, 117.0
MEMX = [111.8, 119.0, 126.2, 133.4]
MEMY = [8.0, 15.5]
STORE_X, STORE_Y = 143.4, 11.5
SCREEN = (31.0, 3.0, 71.0, 16.5)

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

# ---- header
add(text("PERUN", 4.6, 11.0, 6.2, FONT_BOLD, AMBER, anchor="start", spacing=0.12))
add(text("thunder drum voice", 4.8, 16.2, 2.4, FONT_MED, SOFT, anchor="start"))
sx, sy, sw, sh = SCREEN
add(f'<rect x="{sx - 0.6}" y="{sy - 0.6}" width="{sw + 1.2}" height="{sh + 1.2}" rx="1.2" fill="#000"/>')
add(text("1-4", MEMX[0] - 4.4, MEMY[0] + 0.9, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("5-8", MEMX[0] - 4.4, MEMY[1] + 0.9, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("STORE", STORE_X, STORE_Y + 7.6, SMALL, FONT_BOLD, AMBER))

# ---- sections: a darker band per row, name written up the left edge
SECTIONS = ["SOURCE", "BODY + FILTER", "DRIVE + FX", "MOVE"]
for r, name in enumerate(SECTIONS):
    y0 = ROW[r] - 9.6
    y1 = ROW[r] + 14.6
    if r < 3:
        add(f'<rect x="1.0" y="{y0}" width="{W - 2.0}" height="{y1 - y0}" rx="1.2" fill="{BAND}"/>')
    yc = (y0 + y1) / 2 if r < 3 else ROW[3] + 2.0
    add(f'<g transform="rotate(-90 3.3 {yc})">' + text(name, 3.3, yc + 0.85, 2.3, FONT_BOLD, AMBER, spacing=0.08) + '</g>')

LABELS = [
    ["ALGO A", "ALGO B", "MORPH", "MODE", "TUNE", "BEND", "SWEEP", "CLICK"],
    ["DECAY", "P1", "P2", "NOISE", "CUTOFF", "RES", "ENV", "VCF"],
    ["DRIVE", "TONE", "SEND", "TIME", "FDBK", "ACCENT", "LEVEL", None],
    ["WEAR", "MUTATE", "LOCK", None, None, None, None, None],
]
for r, row in enumerate(LABELS):
    for c, name in enumerate(row):
        if name:
            add(text(name, COL[c], ROW[r] - 6.6, CONTROL))

# switch positions (CKSSThree: value 0 = lever down)
def switch_marks(x, y, top, mid, bot):
    add(text(top, x + 3.2, y - 2.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(mid, x + 3.2, y + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(bot, x + 3.2, y + 4.2, SMALL, FONT_BOLD, SOFT, anchor="start"))

switch_marks(COL[3], ROW[0], "C", "B", "A")
switch_marks(COL[7], ROW[1], "LP", "BP", "HP")

# small hints under a few knobs (between knob and jack would collide; put them beside)
add(text("V/OCT", COL[4] + 5.2, ROW[0] + JACK_DY + 0.8, 2.0, FONT_BOLD, SOFT, anchor="start"))
add(text("VCA", COL[6] + 5.2, ROW[2] + JACK_DY + 0.8, 2.0, FONT_BOLD, SOFT, anchor="start"))
add(text("GATE", COL[2] + 5.2, ROW[3] + JACK_DY + 0.8, 2.0, FONT_BOLD, SOFT, anchor="start"))
# empty slot in row 3: a reminder
add(text("DECAY", COL[7], ROW[2] - 1.6, 2.2, FONT_BOLD, SOFT))
add(text("fully up", COL[7], ROW[2] + 1.6, 2.2, FONT_MED, SOFT))
add(text("= drone", COL[7], ROW[2] + 4.8, 2.2, FONT_MED, SOFT))

# ---- trigger + output block
bx0, bx1 = IOX[0] - 8.0, W - 2.0
add(f'<rect x="{bx0}" y="{IO_TOP - 9.0}" width="{bx1 - bx0}" height="{IO_BOT - IO_TOP + 14.0}" rx="1.4" fill="{IOBG}"/>')
for x, n in zip(IOX, ["TRIG", "ACC", "CHOKE", "PATCH"]):
    add(text(n, x, IO_TOP - 5.3, JACK, fill=AMBER if n == "TRIG" else INK))
add(text("HIT", IOX[0], IO_BOT - 5.3, JACK, fill=INK))
for x, n in zip(IOX[1:], ["ENV", "SEND", "OUT"]):
    add(text(n, x, IO_BOT - 5.3, JACK, fill=AMBER))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Perun.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Perun.svg"), x=30.0, y=126.0, cap=3.0, bg=BG)
