#!/usr/bin/env python3
# Draws res/LiveWire.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/LiveWire.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP

BG = "#24262B"
BG_DARK = "#141518"
LINE = "#454A52"
INK = "#EEEAE2"
SOFT = "#A3AAB3"
ACCENT = "#B9A4FF"   # arc violet
HOT = "#FFD66B"

# ---- LAYOUT (same numbers in src/LiveWire.cpp) ----
Y1, Y2, Y3, Y4, Y5 = 30.0, 55.0, 77.0, 100.0, 117.0
ROW1_X = [15.0, 33.0, 48.0, 66.0, 84.0, 104.0, 122.0, 142.0, 160.0, 175.0, 190.0]   # VOLTS CURRENT GAP TUNE are big
ROW1_BIG = [0, 3, 5, 7]
ROW2_X = [12.0 + 19.9 * i for i in range(10)]
ROW3_KNOB_X = [11.0, 27.0, 43.0, 59.0]          # FEEDBACK DISTANCE AGE IN
ROW3_SW_X = [75.0, 89.0]                       # TRACK, V/OCT target
ROW3_BTN_X = [104.0, 117.0, 130.0]             # FIRE TOUCH SURGE
ROW3_LIGHT_X = [142.0, 151.0]                  # ARC CHARGE
ROW3_OUT_X = [163.0, 177.5, 192.0]             # ARC FIELD OUT knobs
JACK_X = [10.5 + 16.6 * i for i in range(12)]
BIG_R = 7.6

CONTROL = 2.8
JACK = 2.4
SMALL = 2.0
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


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

add(text("Live Wire", 5.0, 10.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("one circuit: source, spark gap, tuned coil", W - 5.0, 9.8, 2.4, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def mid(a, b):
    return (a + b) / 2


def ends(x, y, lo, hi, r=3.4):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


# row 1: SOURCE, GAP, TANK
H1 = 15.0
heading("SOURCE", 4.5, mid(ROW1_X[4], ROW1_X[5]) - 1.5, H1)
heading("GAP", mid(ROW1_X[4], ROW1_X[5]) + 1.5, mid(ROW1_X[6], ROW1_X[7]) - 1.5, H1)
heading("TANK", mid(ROW1_X[6], ROW1_X[7]) + 1.5, W - 4.5, H1)
for i, (x, n) in enumerate(zip(ROW1_X, ["VOLTS", "MAINS", "FREQ", "CURRENT", "CHOKE", "GAP", "DRAW", "TUNE", "RING", "LOAD", "IRON"])):
    big = i in ROW1_BIG
    add(text(n, x, Y1 - (BIG_R + 1.6 if big else 7.0), CONTROL))
ends(ROW1_X[1], Y1 + 7.4, "DC", "AC")

# row 2: ARC, AIR, EXTRAS
H2 = 44.2
heading("ARC", 4.5, mid(ROW2_X[5], ROW2_X[6]) - 1.5, H2)
heading("AIR", mid(ROW2_X[5], ROW2_X[6]) + 1.5, mid(ROW2_X[7], ROW2_X[8]) - 1.5, H2)
heading("FIELD", mid(ROW2_X[7], ROW2_X[8]) + 1.5, W - 4.5, H2)
for x, n in zip(ROW2_X, ["HEAT", "COOL", "QUENCH", "MEMORY", "RESTRIKE", "JITTER", "WIND", "CLIMB", "CORONA", "HUM"]):
    add(text(n, x, Y2 - 7.0, CONTROL if len(n) <= 7 else 2.5))

# row 3
for x, n in zip(ROW3_KNOB_X, ["FEEDBACK", "DISTANCE", "AGE", "IN"]):
    add(text(n, x, Y3 - 7.0, CONTROL if len(n) <= 6 else 2.3))
add(text("TRACK", ROW3_SW_X[0], Y3 - 7.0, CONTROL))
add(text("V/OCT", ROW3_SW_X[1], Y3 - 7.0, CONTROL))
add(text("TANK", ROW3_SW_X[1] + 3.2, Y3 - 2.3, 1.8, FONT_BOLD, SOFT, anchor="start"))
add(text("BOTH", ROW3_SW_X[1] + 3.2, Y3 + 0.7, 1.8, FONT_BOLD, SOFT, anchor="start"))
add(text("MAINS", ROW3_SW_X[1] + 3.2, Y3 + 3.7, 1.8, FONT_BOLD, SOFT, anchor="start"))
add(text("ON", ROW3_SW_X[0] + 3.0, Y3 - 1.6, 1.8, FONT_BOLD, SOFT, anchor="start"))
add(text("OFF", ROW3_SW_X[0] + 3.0, Y3 + 3.2, 1.8, FONT_BOLD, SOFT, anchor="start"))
for x, n in zip(ROW3_BTN_X, ["FIRE", "TOUCH", "SURGE"]):
    add(text(n, x, Y3 - 7.0, CONTROL, fill=HOT))
for x, n in zip(ROW3_LIGHT_X, ["ARC", "CHARGE"]):
    add(text(n, x, Y3 - 3.0, 2.0, FONT_BOLD, SOFT))
heading("MIX", ROW3_OUT_X[0] - 5.0, W - 4.5, Y3 - 10.6)
for x, n in zip(ROW3_OUT_X, ["ARC", "FIELD", "OUT"]):
    add(text(n, x, Y3 - 7.0, CONTROL))

# jacks
add(f'<rect x="2.0" y="{Y4 - 9.0}" width="{W - 4.0}" height="{Y5 - Y4 + 15.0}" rx="1.4" fill="{BG_DARK}"/>')
ROW4 = ["V/OCT", "TRIG", "KEY", "TOUCH", "SURGE", "AUDIO", "VOLTS", "CURRENT", "GAP", "HEAT", "COOL", "WIND"]
ROW5 = ["CLIMB", "RING", "LOAD", "IRON", "CORONA", "FEEDBK", "MAINS", "AGE"]
for x, n in zip(JACK_X, ROW4):
    add(text(n, x, Y4 - 5.4, JACK if len(n) <= 6 else 2.1, FONT_BOLD, INK if n in ROW4[:6] else SOFT))
for x, n in zip(JACK_X, ROW5):
    add(text(n, x, Y5 - 5.4, JACK if len(n) <= 6 else 2.1, FONT_BOLD, SOFT))
ox0 = JACK_X[8] - 7.6
add(f'<rect x="{ox0}" y="{Y5 - 8.8}" width="{JACK_X[11] + 7.6 - ox0}" height="13.0" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(JACK_X[8:], ["ARC", "FIELD", "L", "R"]):
    add(text(n, x, Y5 - 5.4, JACK, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for i, x in enumerate(ROW1_X): c(x, Y1, BIG_R if i in ROW1_BIG else 4.8, "knob1")
    for x in ROW2_X: c(x, Y2, 4.8, "knob2")
    for x in ROW3_KNOB_X + ROW3_OUT_X: c(x, Y3, 4.8, "knob3")
    for x in ROW3_SW_X: b.append(("switch", x - 2.0, Y3 - 4.2, x + 2.0, Y3 + 4.2))
    for x in ROW3_BTN_X: c(x, Y3, 3.2, "button")
    for x in ROW3_LIGHT_X: c(x, Y3 + 1.0, 1.2, "light")
    for x in JACK_X: c(x, Y4, 4.0, "jack4")
    for x in JACK_X: c(x, Y5, 4.0, "jack5")
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
open(os.path.join(HERE, "LiveWire.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "LiveWire.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
