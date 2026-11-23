#!/usr/bin/env python3
# Draws res/Cicada.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Cicada.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 101.6, 128.5  # 20 HP

BG = "#1E2A1F"
BG_DARK = "#121A13"
LINE = "#3B4C3A"
INK = "#EFE9D2"
SOFT = "#AEB89E"
AMBER = "#F2B33D"

# ---- LAYOUT (same numbers in src/Cicada.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.0, 52.0, 9.0
PLIGHT_X, PLIGHT_Y = 61.5, 18.5
R1_Y = 40.0
MECH_X = 11.0
R1_X = [36.0, 62.0, 88.0]           # PITCH (large), PURE, STROKE
R2_Y = 62.0
R2_X = [12.0, 31.4, 50.8, 70.2, 89.6]   # BODY, OPEN, TEMP, LOOSE, DISTANCE
R3_Y = 84.0
R3_X = [12.0, 30.0, 46.0, 62.0, 78.0]   # RATE (large), CHIRP, GAP, SONG, LENGTH
VSW_X = 94.0
JX0, JDX = 8.6, 12.06
JA_Y, JB_Y = 101.5, 115.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3     # range / switch position marks
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

# ---- title, display
add(text("Cicada", 5.0, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("singing insects", W - 4.0, 10.2, 2.4, FONT_MED, AMBER, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0A0F0A" stroke="{AMBER}" stroke-opacity="0.6" stroke-width="0.35"/>')
add(text("PULSE", PLIGHT_X + 3.0, PLIGHT_Y + 0.9, SMALL, FONT_BOLD, SOFT, anchor="start"))


def heading(s, y):
    t = text(s, 4.0, y, HEAD, FONT_BOLD, AMBER, anchor="start", spacing=0.12)
    add(t)
    x1 = boxes[-1][3] + 1.6
    add(f'<line x1="{x1:.2f}" y1="{y - 0.8:.2f}" x2="{W - 4.0}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def label(s, x, y, large=False):
    add(text(s, x, y - (8.4 if large else 7.6), CONTROL))


def ranges(x, y, r, lo, hi, dy=6.6, col=SOFT):
    add(text(lo, x - r, y + dy, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y + dy, SMALL, FONT_BOLD, col, anchor="start"))


# ---- row 1: mechanism
heading("MECHANISM", 28.2)
add(text("MECH", MECH_X, R1_Y - 7.6, CONTROL))
for yy, o in zip([R1_Y - 2.6, R1_Y + 1.2, R1_Y + 5.0], ["TYMBAL", "FILE", "WING"]):
    add(text(o, MECH_X + 3.0, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))
label("PITCH", R1_X[0], R1_Y, large=True)
label("PURE", R1_X[1], R1_Y)
ranges(R1_X[1], R1_Y, 4.4, "RASP", "TONE")
label("STROKE", R1_X[2], R1_Y)

# ---- row 2: body and air
heading("BODY AND AIR", 50.2)
for x, n in zip(R2_X, ["BODY", "OPEN", "TEMP", "LOOSE", "DISTANCE"]):
    label(n, x, R2_Y)
ranges(R2_X[2], R2_Y, 4.4, "10°", "35°")
ranges(R2_X[4], R2_Y, 4.4, "NEAR", "FAR")

# ---- row 3: song
heading("SONG", 71.6)
label("RATE", R3_X[0], R3_Y, large=True)
for x, n in zip(R3_X[1:], ["CHIRP", "GAP", "SONG", "LENGTH"]):
    label(n, x, R3_Y)
ranges(R3_X[1], R3_Y, 4.4, "1", "TRILL")
ranges(R3_X[4], R3_Y, 4.4, "", "HOLD")
add(text("V/OCT", VSW_X, R3_Y - 7.6, CONTROL))
add(text("PITCH", VSW_X - 3.0, R3_Y - 1.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("RATE", VSW_X - 3.0, R3_Y + 3.4, SMALL, FONT_BOLD, SOFT, anchor="end"))

# ---- jacks
add(f'<rect x="2.0" y="{JA_Y - 8.6}" width="{W - 4.0}" height="{JB_Y - JA_Y + 13.6}" rx="1.4" fill="{BG_DARK}"/>')
def jx(i): return JX0 + JDX * i
for i, n in enumerate(["PITCH", "PURE", "STROKE", "OPEN", "RATE", "CHIRP", "TEMP", "V/OCT"]):
    add(text(n, jx(i), JA_Y - 5.2, JACK, FONT_BOLD, INK if i == 7 else SOFT))
for i, n in enumerate(["GATE", "TRIG", "CLOCK"]):
    add(text(n, jx(i), JB_Y - 5.2, JACK, FONT_BOLD, INK))
ox0 = jx(3) - 6.0
add(f'<rect x="{ox0}" y="{JB_Y - 8.8}" width="{jx(7) + 6.0 - ox0}" height="13.4" rx="1.4" fill="{AMBER}"/>')
for i, n in enumerate(["PULSE", "CHIRP", "ENV", "L", "R"]):
    add(text(n, jx(3 + i), JB_Y - 5.2, JACK, fill=BG))


# ---- overlap check against each other and against controls
def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    c(R1_X[0], R1_Y, 6.1, "PITCH knob"); c(R1_X[1], R1_Y, 4.8, "PURE"); c(R1_X[2], R1_Y, 4.8, "STROKE")
    b.append(("MECH switch", MECH_X - 2.4, R1_Y - 5.2, MECH_X + 2.4, R1_Y + 5.2))
    for x in R2_X: c(x, R2_Y, 4.8, "knob r2")
    c(R3_X[0], R3_Y, 6.1, "RATE knob")
    for x in R3_X[1:]: c(x, R3_Y, 4.8, "knob r3")
    b.append(("V/OCT switch", VSW_X - 2.0, R3_Y - 3.6, VSW_X + 2.0, R3_Y + 3.6))
    for y in (JA_Y, JB_Y):
        for i in range(8): c(jx(i), y, 4.0, "jack")
    c(PLIGHT_X, PLIGHT_Y, 1.6, "light")
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
open(os.path.join(HERE, "Cicada.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Cicada.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
