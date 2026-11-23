#!/usr/bin/env python3
# Draws res/Clapfall.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Clapfall.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 91.44, 128.5  # 18 HP

BG = "#2B2024"
BG_DARK = "#1C1518"
LINE = "#4A3940"
INK = "#F4E8DC"
SOFT = "#C2B1AA"
CORAL = "#FF7A5C"

# ---- LAYOUT (same numbers in src/Clapfall.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.5, 46.0, 9.0
PLIGHT_X, PLIGHT_Y = 55.0, 19.0
HIT_X, FREE_X, BTN_Y = 66.0, 81.0, 19.0
COLX = [14.0, 40.0, 66.0]
ROW1_Y = 40.0
ROW2_Y = 58.0
ROW3_Y = 77.0
SW_X = [58.5, 75.5]
JX0, JDX = 9.0, 14.68
JA_Y, JB_Y, JC_Y = 93.0, 105.0, 117.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3     # range / switch position marks

fonts = {}
boxes = []   # (label, x0, y0, x1, y1) for the overlap check


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, check=True):
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
    if check:
        boxes.append((s, x0, y - size * 0.75, x0 + width * scale, y + size * 0.05))
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# ---- title + display + buttons
add(text("Clapfall", 5.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("multi-particle percussion", W - 4.0, 10.2, 2.4, FONT_MED, CORAL, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0B0809" stroke="{CORAL}" stroke-opacity="0.6" stroke-width="0.35"/>')
add(text("HIT", HIT_X, BTN_Y - 5.2, JACK))
add(text("FREE", FREE_X, BTN_Y - 5.2, JACK))

def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))

# ---- knobs
add(text("SURFACE", COLX[0], ROW1_Y - 9.0, CONTROL, fill=CORAL))
add(text("DENSITY", COLX[1], ROW1_Y - 7.6, CONTROL))
add(text("DECAY", COLX[2], ROW1_Y - 7.6, CONTROL))
add(text("LOOSE", COLX[0], ROW2_Y - 7.6, CONTROL))
add(text("TONE", COLX[1], ROW2_Y - 7.6, CONTROL))
add(text("SPREAD", COLX[2], ROW2_Y - 7.6, CONTROL))
ranges(COLX[0], ROW2_Y + 7.4, 4.4, "EXACT", "HUMAN")
add(text("REVERB", COLX[0], ROW3_Y - 7.6, CONTROL))
add(text("SIZE", COLX[1], ROW3_Y - 7.6, CONTROL))
ranges(COLX[1], ROW3_Y + 6.4, 4.4, "ROOM", "HALL")

def switch_labels(x, y, title, opts):
    add(text(title, x, y - 7.6, CONTROL))
    for yy, o in zip([y - 2.6, y + 1.2, y + 5.0], opts):
        add(text(o, x + 3.0, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))

switch_labels(SW_X[0], ROW3_Y, "ENV", ["AMP", "BOTH", "CHANCE"])
switch_labels(SW_X[1], ROW3_Y, "FILTER", ["BAND", "PITCH", "PUNCH"])

# ---- jacks
add(f'<rect x="2.0" y="{JA_Y - 8.3}" width="{W - 4.0}" height="{JB_Y - JA_Y + 12.9}" rx="1.4" fill="{BG_DARK}"/>')
def jx(i): return JX0 + JDX * i
for i, n in enumerate(["SURF", "DENS", "DECAY", "LOOSE", "TONE", "SPREAD"]):
    add(text(n, jx(i), JA_Y - 5.2, JACK, FONT_BOLD, SOFT))
for i, n in enumerate(["REVERB", "SIZE", "V/OCT", "FREE", "ACCENT", "ONE"]):
    add(text(n, jx(i), JB_Y - 5.2, JACK, FONT_BOLD, INK if i >= 2 else SOFT))
add(text("TRIG", jx(0), JC_Y - 5.2, JACK, FONT_BOLD, CORAL))
add(text("5 V = full turn", jx(1) - 3.6, JC_Y + 0.9, SMALL, FONT_MED, SOFT, anchor="start"))
ox = jx(4)
add(f'<rect x="{ox - 7.0}" y="{JC_Y - 8.8}" width="{JDX + 14.0}" height="13.4" rx="1.4" fill="{CORAL}"/>')
add(text("OUT L", jx(4), JC_Y - 5.2, JACK, fill=BG))
add(text("OUT R", jx(5), JC_Y - 5.2, JACK, fill=BG))

# ---- overlap check against each other and against controls
def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    c(COLX[0], ROW1_Y, 6.1, "SURFACE knob")
    for x in COLX[1:]: c(x, ROW1_Y, 4.8, "knob r1")
    for x in COLX: c(x, ROW2_Y, 4.8, "knob r2")
    for x in COLX[:2]: c(x, ROW3_Y, 4.8, "knob r3")
    for x in SW_X: b.append(("switch", x - 2.4, ROW3_Y - 5.2, x + 2.4, ROW3_Y + 5.2))
    for y in (JA_Y, JB_Y): 
        for i in range(6): c(jx(i), y, 4.0, "jack")
    for i in (0, 4, 5): c(jx(i), JC_Y, 4.0, "jack")
    c(HIT_X, BTN_Y, 3.6, "button"); c(FREE_X, BTN_Y, 3.6, "button")
    b.append(("display", DISP_X - 0.6, DISP_Y - 0.6, DISP_X + DISP_W + 0.6, DISP_Y + DISP_H + 0.6))
    return b
allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in boxes:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Clapfall.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Clapfall.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
