#!/usr/bin/env python3
# Draws res/Molten.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Molten.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 223.52, 128.5  # 44 HP

BG = "#25282B"
BG_DARK = "#17191B"
LINE = "#454A4F"
INK = "#ECE8E1"
SOFT = "#A9B0B6"
ACCENT = "#D9A441"
HOT = "#E2673A"

# ---- LAYOUT (same numbers in src/Molten.cpp) ----
COLA = [45.0 + 21.2 * i for i in range(9)]     # rows 1 and 2
COLS = [9.0 + 14.68 * i for i in range(15)]    # row 3
COLJ = [9.0 + 18.68 * i for i in range(12)]    # jack rows
BIG_X = 22.0
Y1, Y2, Y3, Y4, Y5 = 35.0, 63.0, 87.0, 101.0, 115.0
RING_R = 11.0
DISP_X0, DISP_Y, DISP_H = 5.0, 12.8, 5.0

CONTROL = 3.0
JACK = 2.6
SMALL = 2.1
HEAD = 2.4

fonts = {}
boxes = []


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


def fit(s, room, size):
    """Shrink a label until it fits in `room` mm."""
    while size > 1.9:
        f = fonts.get(FONT_BOLD)
        if f is None:
            text("X", 0, 0, 1, check=False)
            f = fonts[FONT_BOLD]
        _, gs, cmap, upm = f
        w = sum(gs[cmap[ord(c)]].width + 0.04 * upm for c in s) * size / upm
        if w <= room:
            return size
        size -= 0.1
    return size


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

add(text("Molten", 5.0, 10.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("a singing metal plate over a flame: heat it, melt it, quench it, recast it", W - 5.0, 9.8, 2.4, FONT_MED, ACCENT, anchor="end"))

# the zone strip sits on a dark well; Rack draws the glowing cells on top
add(f'<rect x="{DISP_X0 - 0.6}" y="{DISP_Y - 0.6}" width="{W - 2 * DISP_X0 + 1.2}" height="{DISP_H + 1.2}" rx="0.8" fill="{BG_DARK}"/>')
for k in range(16):
    cw = (W - 2 * DISP_X0) / 16
    add(f'<rect x="{DISP_X0 + k * cw + 0.25:.3f}" y="{DISP_Y + 0.25:.3f}" width="{cw - 0.5:.3f}" height="{DISP_H - 0.5:.3f}" fill="#202326"/>')


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def mid(a, b):
    return (a + b) / 2


# big knobs: HEAT (melting point marked at noon) and DRIVE
add(text("HEAT", BIG_X, Y1 - RING_R - 1.6, CONTROL + 0.4, fill=HOT))
add(f'<circle cx="{BIG_X}" cy="{Y1}" r="{RING_R}" fill="none" stroke="{LINE}" stroke-width="0.5"/>')
add(f'<line x1="{BIG_X}" y1="{Y1 - RING_R - 0.9}" x2="{BIG_X}" y2="{Y1 - RING_R + 1.2}" stroke="{HOT}" stroke-width="0.7"/>')
add(text("MELT", BIG_X + 1.4, Y1 - RING_R + 0.2, 1.7, FONT_BOLD, HOT, anchor="start"))
add(text("COLD", BIG_X - RING_R - 0.6, Y1 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("WHITE", BIG_X + RING_R + 0.6, Y1 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("DRIVE", BIG_X, Y2 - RING_R - 1.6, CONTROL + 0.4))
add(f'<circle cx="{BIG_X}" cy="{Y2}" r="{RING_R}" fill="none" stroke="{LINE}" stroke-width="0.5"/>')
add(text("PURE", BIG_X - RING_R - 0.6, Y2 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("ROAR", BIG_X + RING_R + 0.6, Y2 + RING_R - 0.2, SMALL, FONT_BOLD, SOFT, anchor="start"))

# row 1: HEAT section, then PINGS (METAL)
H1 = 23.4
heading("FIRE", COLA[0] - 9.0, mid(COLA[7], COLA[8]) - 1.5, H1)
heading("METAL", mid(COLA[7], COLA[8]) + 1.5, W - 4.5, H1)
for x, n in zip(COLA, ["QUENCH", "WATER", "MASS", "FLAME", "WIDTH", "LATENT", "SUPERCOOL", "SELF", "PINGS"]):
    add(text(n, x, Y1 - 7.6, fit(n, 19.0, CONTROL)))
add(text("FAST", COLA[2] - 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("SLOW", COLA[2] + 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("SPOT", COLA[4] - 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("ALL", COLA[4] + 3.6, Y1 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("HOLD", COLA[0], Y1 + 7.6, SMALL, FONT_BOLD, SOFT))

# row 2: LOOP, TOUCH, METAL
H2 = 51.4
heading("LOOP", COLA[0] - 9.0, mid(COLA[3], COLA[4]) - 1.5, H2)
heading("TOUCH", mid(COLA[3], COLA[4]) + 1.5, mid(COLA[6], COLA[7]) - 1.5, H2)
heading("METAL", mid(COLA[6], COLA[7]) + 1.5, W - 4.5, H2)
for x, n in zip(COLA, ["GAIN", "FOCUS", "PITCH", "TRACK", "POS", "GRAB", "RELEASE", "TENSION", "LOOSE"]):
    add(text(n, x, Y2 - 7.6, fit(n, 19.0, CONTROL)))
add(text("PLATE", COLA[3], Y2 - 4.4, SMALL, FONT_BOLD, SOFT))
add(text("FOCUS", COLA[3], Y2 + 7.0, SMALL, FONT_BOLD, SOFT))
add(text("SLOW", COLA[0] - 3.6, Y2 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("HARD", COLA[0] + 3.6, Y2 + 7.6, SMALL, FONT_BOLD, SOFT, anchor="start"))

# row 3: METAL, SAT, EXTRA
H3 = 77.6
heading("METAL", 4.5, mid(COLS[6], COLS[7]) - 1.5, H3)
heading("SAT", mid(COLS[6], COLS[7]) + 1.5, mid(COLS[9], COLS[10]) - 1.5, H3)
heading("EXTRA", mid(COLS[9], COLS[10]) + 1.5, W - 4.5, H3)
for x, n in zip(COLS, ["SAG", "CAST", "BODY", "SIZE", "THICK", "CURVE", "DECAY", "PICKUP", "AMP", "OUT", "MALLET", "IN LVL", "BRIGHT", "AGE", "LEVEL"]):
    add(text(n, x, Y3 - 6.0, fit(n, 13.4, 2.7)))

# jacks
add(f'<rect x="2.0" y="{Y4 - 9.0}" width="{W - 4.0}" height="{Y5 - Y4 + 15.0}" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COLJ, ["V/OCT", "GATE", "STRIKE", "VEL", "QUENCH", "IN", "HEAT", "FLAME", "WIDTH", "TENSION", "LOOSE", "DRIVE"]):
    add(text(n, x, Y4 - 5.4, fit(n, 17.0, JACK), FONT_BOLD, INK if n in ("V/OCT", "GATE", "STRIKE", "VEL", "QUENCH", "IN") else SOFT))
for x, n in zip(COLJ[:5], ["FOCUS", "SIZE", "THICK", "POS", "GRAB"]):
    add(text(n, x, Y5 - 5.4, JACK, FONT_BOLD, SOFT))
ox0 = COLJ[6] - 8.0
add(f'<rect x="{ox0}" y="{Y5 - 9.0}" width="{COLJ[11] + 8.0 - ox0}" height="13.4" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(COLJ[6:], ["TEMP", "LIQUID", "ROAR", "ENV", "L", "R"]):
    add(text(n, x, Y5 - 5.4, JACK, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    c(BIG_X, Y1, RING_R + 0.6, "heat ring")
    c(BIG_X, Y2, RING_R + 0.6, "drive ring")
    c(COLA[0], Y1, 4.6, "quench")
    for x in COLA[1:]: c(x, Y1, 4.8, "knob1")
    for x in COLA[:3] + COLA[4:]: c(x, Y2, 4.8, "knob2")
    b.append(("track", COLA[3] - 1.8, Y2 - 3.3, COLA[3] + 1.8, Y2 + 3.3))
    for x in COLS: c(x, Y3, 4.0, "knob3")
    for x in COLJ: c(x, Y4, 4.0, "jack4")
    for x in COLJ[:5] + COLJ[6:]: c(x, Y5, 4.0, "jack5")
    b.append(("display", DISP_X0, DISP_Y, W - DISP_X0, DISP_Y + DISP_H + 0.8))
    return b


allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[0] == "MELT" and b[0] == "heat ring" or b[0] == "MELT" and a[0] == "heat ring":
            continue  # the MELT mark sits on the ring by design
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in boxes:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Molten.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Molten.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
