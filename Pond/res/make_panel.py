#!/usr/bin/env python3
# Draws res/Pond.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Pond.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 111.76, 128.5  # 22 HP

BG = "#152430"
BG_DARK = "#0D1820"
LINE = "#2C4250"
INK = "#EAF0E0"
SOFT = "#9FB6B2"
GREEN = "#8CC63F"
RED = "#E6503C"

# ---- LAYOUT (same numbers in src/Pond.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.0, 101.76, 17.0
R1_Y = 46.0
R1_X = [12.0, 29.0, 46.0, 63.0, 82.0, 99.5]     # SPECIES A, B, MIX, FROGS, HUSH, CALL
R2_Y = 66.0
R2_X = [12.0, 29.5, 47.0, 64.5, 82.0, 99.5]     # PITCH PULSE LENGTH ROUGH VARIETY SPACE
R3_Y = 87.5
R3_X = [13.0, 32.0, 50.0, 70.0, 97.0]           # TEMP(L) BUSY BOUTS TALK(L) RETURN
JX0, JDX = 8.4, 11.9
JA_Y, JB_Y = 104.0, 117.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3     # range marks
HEAD = 2.4      # section headings

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

# ---- ripples behind the title (decoration)
for i, r in enumerate([3.0, 5.2, 7.4]):
    add(f'<ellipse cx="{W - 36}" cy="7.6" rx="{r * 1.9:.2f}" ry="{r * 0.55:.2f}" fill="none" stroke="{LINE}" stroke-width="0.35"/>')

# ---- title, display
add(text("Pond", 5.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("frog and toad chorus", 22.5, 10.2, 2.4, FONT_MED, GREEN, anchor="start"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#081014" stroke="{GREEN}" stroke-opacity="0.6" stroke-width="0.35"/>')


def heading(s, y):
    add(text(s, 4.0, y, HEAD, FONT_BOLD, GREEN, anchor="start", spacing=0.12))
    x1 = boxes[-1][3] + 1.6
    add(f'<line x1="{x1:.2f}" y1="{y - 0.8:.2f}" x2="{W - 4.0}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def label(s, x, y, large=False, fill=INK):
    add(text(s, x, y - (8.4 if large else 7.6), CONTROL, fill=fill))


def ranges(x, y, r, lo, hi, dy=6.6, col=SOFT):
    if lo: add(text(lo, x - r, y + dy, SMALL, FONT_BOLD, col, anchor="end"))
    if hi: add(text(hi, x + r, y + dy, SMALL, FONT_BOLD, col, anchor="start"))


# ---- row 1: who is in the pond
heading("FROGS", 35.2)
for x, n in zip(R1_X[:4], ["SPECIES A", "B", "MIX", "FROGS"]):
    label(n, x, R1_Y)
ranges(R1_X[2], R1_Y, 4.4, "A", "B")
ranges(R1_X[3], R1_Y, 4.4, "1", "16")
label("HUSH", R1_X[4], R1_Y, fill=RED)
label("CALL", R1_X[5], R1_Y, fill=GREEN)

# ---- row 2: voice
heading("VOICE", 55.2)
for x, n in zip(R2_X, ["PITCH", "PULSE", "LENGTH", "ROUGH", "VARIETY", "SPACE"]):
    label(n, x, R2_Y)
ranges(R2_X[5], R2_Y, 4.4, "NEAR", "FAR")

# ---- row 3: the pond's behaviour
heading("CHORUS", 75.4)
label("TEMP", R3_X[0], R3_Y, large=True)
label("BUSY", R3_X[1], R3_Y)
label("BOUTS", R3_X[2], R3_Y)
label("TALK", R3_X[3], R3_Y, large=True)
label("RETURN", R3_X[4], R3_Y)
ranges(R3_X[0], R3_Y, 6.4, "5°", "30°", dy=7.6)
ranges(R3_X[3], R3_Y, 6.4, "SYNC", "ANSWER", dy=7.6)
ranges(R3_X[4], R3_Y, 4.4, "2s", "60s")

# ---- jacks
add(f'<rect x="2.0" y="{JA_Y - 8.6}" width="{W - 4.0}" height="{JB_Y - JA_Y + 13.6}" rx="1.4" fill="{BG_DARK}"/>')
def jx(i): return JX0 + JDX * i
for i, n in enumerate(["V/OCT", "PULSE", "LEN", "TEMP", "BUSY", "BOUTS", "TALK", "MIX", "SPACE"]):
    add(text(n, jx(i), JA_Y - 5.2, JACK, FONT_BOLD, INK if i == 0 else SOFT))
for i, n in enumerate(["CLOCK", "CALL", "HUSH"]):
    add(text(n, jx(i), JB_Y - 5.2, JACK, FONT_BOLD, RED if n == "HUSH" else INK))
ox0 = jx(3) - 5.8
add(f'<rect x="{ox0}" y="{JB_Y - 8.8}" width="{jx(8) + 5.8 - ox0}" height="13.4" rx="1.4" fill="{GREEN}"/>')
for i, n in enumerate(["TRIG", "GATE", "PITCH", "ENV", "L", "R"]):
    add(text(n, jx(3 + i), JB_Y - 5.2, JACK, fill=BG_DARK))


# ---- overlap check against each other and against controls
def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in R1_X[:4]: c(x, R1_Y, 4.8, "knob r1")
    for x in R1_X[4:]: c(x, R1_Y, 4.6, "button")
    for x in R2_X: c(x, R2_Y, 4.8, "knob r2")
    for i, x in enumerate(R3_X): c(x, R3_Y, 6.1 if i in (0, 3) else 4.8, "knob r3")
    for y in (JA_Y, JB_Y):
        for i in range(9): c(jx(i), y, 4.0, "jack")
    b.append(("display", DISP_X - 0.6, DISP_Y - 0.6, DISP_X + DISP_W + 0.6, DISP_Y + DISP_H + 0.6))
    return b


allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in allb:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Pond.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Pond.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
