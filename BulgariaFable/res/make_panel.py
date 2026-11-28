#!/usr/bin/env python3
# Draws res/Bulgaria.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Bulgaria.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 101.6, 128.5  # 20 HP

BG = "#2A2422"
BG_DARK = "#171312"
LINE = "#4E4440"
INK = "#F1EADF"
SOFT = "#B5A79C"
ACCENT = "#E8B04A"   # embroidery gold
RED = "#D9534A"      # embroidery red

# ---- LAYOUT (same numbers in src/Bulgaria.cpp) ----
K1, K2, K3, K4 = 23.0, 39.0, 57.0, 83.5
ROW1_X = [13.0, 38.0, 63.5, 88.5]        # ROOT OCT MODE TUNE
ROW2_X = [13.0, 33.0]                    # VOICES SPREAD
LIGHT_X = [47.0 + 6.6 * i for i in range(8)]
RUB_X = 16.0
SW_X = [36.0, 52.0, 85.0]                # UNIT, WHEN, CADENCE
CHANCE_X = 70.0
ROW4_X = [13.0, 33.0, 53.0, 85.0]        # SLIDE SHAKE WHOOP LOOSE
TRIG_Y = 94.5
J1, J2 = 106.5, 118.5
JACK_X = [9.0 + 13.9 * i for i in range(7)]
BIG_R = 7.6
KNOB_R = 5.0

CONTROL = 2.6
JACK = 2.1
SMALL = 1.6
HEAD = 2.0

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
# a thin embroidered border: red and gold stitches along the top rail
for i in range(int(W / 3.0)):
    x = 1.5 + i * 3.0
    add(f'<path d="M{x:.2f} 2.2 l1.2 1.2 l1.2 -1.2" fill="none" stroke="{RED if i % 2 else ACCENT}" stroke-width="0.35"/>')

add(text("BULGARIA", 4.5, 10.0, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.06))
add(text("village harmony", W - 4.5, 9.6, 2.2, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.14))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.7:.2f}" x2="{x1:.2f}" y2="{y - 0.7:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def around(x, y, labels, r=7.0, size=SMALL, fill=SOFT):
    n = len(labels)
    for i, s in enumerate(labels):
        th = -0.83 * math.pi + (1.66 * math.pi * i / (n - 1) if n > 1 else 0)
        add(text(s, x + r * math.sin(th), y - r * math.cos(th) + size * 0.36, size, FONT_BOLD, fill, check=False))


def side(x, y, lo, hi, r=5.9):
    add(text(lo, x - r, y + 4.0, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y + 4.0, SMALL, FONT_BOLD, SOFT, anchor="start"))


def ends(x, y, lo, hi, r=3.2):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


# row 1: DRONE
heading("DRONE", 4.5, W - 4.5, K1 - 9.5)
for x, n in zip(ROW1_X, ["ROOT", "OCT", "MODE", "TUNE"]):
    add(text(n, x, K1 - 6.2, CONTROL))
side(ROW1_X[0], K1, "C", "B")
side(ROW1_X[1], K1, "-2", "+2")
side(ROW1_X[2], K1, "SHO", "MAJ")
side(ROW1_X[3], K1, "EQ", "VIL")

# row 2: CHOIR
heading("CHOIR", 4.5, W - 4.5, K2 - 9.5)
add(text("VOICES", ROW2_X[0], K2 - 6.2, CONTROL))
add(text("SPREAD", ROW2_X[1], K2 - 6.2, CONTROL))
side(ROW2_X[0], K2, "1", "8")
side(ROW2_X[1], K2, "OPEN", "CLOSE")
for i, x in enumerate(LIGHT_X):
    add(text(str(i + 1), x, K2 - 3.0, 2.0, FONT_BOLD, SOFT))
add(text("SOUNDING", LIGHT_X[0] - 1.6, K2 + 5.2, SMALL, FONT_BOLD, "#7BC67B", anchor="start"))
add(text("RUBBING", LIGHT_X[-1] + 1.6, K2 + 5.2, SMALL, FONT_BOLD, RED, anchor="end"))

# row 3: RUB
heading("RUB", 4.5, W - 4.5, K3 - 11.0)
add(text("CENTS 25-250", RUB_X, K3 + 11.0, SMALL, FONT_BOLD, SOFT))
add(text("HZ 1-80", RUB_X, K3 + 13.2, SMALL, FONT_BOLD, SOFT))
for x, n, top, bot in zip(SW_X, ["UNIT", "WHEN", "CADENCE"], ["HZ", "CHANCE", "RESOLVE"], ["CENTS", "RULE", "TIGHT"]):
    add(text(n, x, K3 - 6.2, CONTROL))
    add(text(top, x + 2.6, K3 - 1.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(bot, x + 2.6, K3 + 3.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("CHANCE", CHANCE_X, K3 - 6.2, CONTROL))

# row 4: ORNAMENT + LOOSE
heading("ORNAMENT", 4.5, 66.0, K4 - 9.5)
heading("SINGERS", 74.0, W - 4.5, K4 - 9.5)
for x, n in zip(ROW4_X, ["SLIDE", "SHAKE", "WHOOP", "LOOSE"]):
    add(text(n, x, K4 - 6.2, CONTROL))
add(text("TRIG", ROW4_X[2] + 6.0, TRIG_Y + 0.7, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("CV", ROW4_X[3] + 5.0, TRIG_Y + 0.7, SMALL, FONT_BOLD, SOFT, anchor="start"))

# jacks
add(f'<rect x="2.0" y="{J1 - 8.4}" width="{W - 4.0}" height="{J2 - J1 + 13.0}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["V/OCT", "GATE", "DRONE", "PHRASE", "RUB", "TUNE", "VOICES"]
for i, (x, n) in enumerate(zip(JACK_X, IN1)):
    add(text(n, x, J1 - 5.3, JACK if len(n) <= 5 else 1.8, FONT_BOLD, INK if i < 4 else SOFT))
ox0, ox1 = JACK_X[0] - 6.4, JACK_X[-1] + 6.4
add(f'<rect x="{ox0:.2f}" y="{J2 - 8.0}" width="{ox1 - ox0:.2f}" height="12.2" rx="1.4" fill="{ACCENT}"/>')
OUT = ["LEAD", "L GATE", "DRONE", "D GATE", "POLY", "P GATE", "PUSH"]
for x, n in zip(JACK_X, OUT):
    add(text(n, x, J2 - 5.0, JACK if len(n) <= 5 else 1.8, FONT_BOLD, BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in ROW1_X: c(x, K1, KNOB_R, "knob1")
    for x in ROW2_X: c(x, K2, KNOB_R, "knob2")
    for x in LIGHT_X: c(x, K2, 1.4, "light")
    c(RUB_X, K3, BIG_R, "rub")
    c(CHANCE_X, K3, KNOB_R, "chance")
    for x in SW_X: b.append(("switch", x - 1.8, K3 - 3.4, x + 1.8, K3 + 3.4))
    for x in ROW4_X: c(x, K4, KNOB_R, "knob4")
    for x in ROW4_X[:3] + [ROW4_X[3]]: c(x, TRIG_Y, 4.0, "trig")
    for x in JACK_X: c(x, J1, 4.0, "jack1")
    for x in JACK_X: c(x, J2, 4.0, "jack2")
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
open(os.path.join(HERE, "Bulgaria.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Bulgaria.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
