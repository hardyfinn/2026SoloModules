#!/usr/bin/env python3
# Draws res/Sawstack.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Sawstack.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 111.76, 128.5  # 22 HP

BG = "#2A2623"
BG_DARK = "#1A1715"
LINE = "#4A433D"
INK = "#F1E8DA"
SOFT = "#B9AE9F"
ACCENT = "#E8733A"

# ---- LAYOUT (same numbers in src/Sawstack.cpp) ----
COLS = [11.5, 29.25, 47.0, 64.75, 82.5, 100.25]
Y_BIG = 27.5       # FREQUENCY SPREAD TIMBRE RICHNESS AMPLITUDE OUTPUT
Y_ATT = 41.5       # attenuverters for the five CV jacks below
Y_CV = 53.0        # FREQ SPREAD TIMBRE RICHNESS AMP inputs
Y_MED = 74.0       # RISE 1 FALL 1 RISE 2 FALL 2 AGE DRIVE
Y_FATT = 85.5      # FALL 1 and FALL 2 attenuverters (cols 2 and 4)
Y_IN = 99.0        # V/OCT GATE 1 FALL 1 GATE 2 FALL 2
Y_OUT = 114.0      # ENV 1 END 1 ENV 2 END 2 L R
LIGHT_DX, LIGHT_DY = 6.6, -5.2   # envelope lights, from the RISE knobs

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3
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

add(text("Sawstack", 5.0, 10.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("five-oscillator swarm voice", W - 4.5, 9.8, 2.4, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


heading("STACK", 4.5, 73.0, 15.4)
heading("VCA", 76.0, W - 4.5, 15.4)
for x, n in zip(COLS, ["FREQUENCY", "SPREAD", "TIMBRE", "RICHNESS", "AMP", "OUTPUT"]):
    add(text(n, x, Y_BIG - 8.6, 2.95))
for x, lo, hi in [(COLS[2], "SINE", "SAW")]:
    add(text(lo, x - 3.4, Y_BIG + 9.0, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + 3.4, Y_BIG + 9.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
for x, n in zip(COLS[:5], ["FREQ", "SPREAD", "TIMBRE", "RICH", "AMP"]):
    add(text(n, x, Y_CV - 5.4, JACK, FONT_BOLD, SOFT))
add(text("CV", COLS[5], Y_ATT + 1.0, SMALL, FONT_BOLD, SOFT))
add(text("ATTENUVERTERS", COLS[5], Y_ATT + 4.0, 1.9, FONT_BOLD, SOFT, spacing=0.06))
add(text("ENV 1 → RICH", COLS[5], Y_CV - 1.4, 1.9, FONT_MED, SOFT, spacing=0.02))
add(text("ENV 2 → AMP", COLS[5], Y_CV + 1.6, 1.9, FONT_MED, SOFT, spacing=0.02))

heading("ENVELOPE 1", 4.5, 38.0, 61.4)
heading("ENVELOPE 2", 40.4, 73.0, 61.4)
heading("CHARACTER", 76.0, W - 4.5, 61.4)
for x, n in zip(COLS, ["RISE", "FALL", "RISE", "FALL", "AGE", "DRIVE"]):
    add(text(n, x, Y_MED - 7.6, CONTROL))
add(text("FRESH", COLS[4] - 4.6, Y_MED + 6.9, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("TIRED", COLS[4] + 4.6, Y_MED + 6.9, SMALL, FONT_BOLD, SOFT, anchor="start"))

add(f'<rect x="2.0" y="{Y_IN - 8.6}" width="{W - 4.0}" height="{Y_OUT - Y_IN + 13.8}" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COLS[:5], ["V/OCT", "GATE 1", "FALL 1", "GATE 2", "FALL 2"]):
    add(text(n, x, Y_IN - 5.4, JACK, FONT_BOLD, INK if n.startswith(("V/", "GATE")) else SOFT))
add(text("GATE 2", COLS[5], Y_IN - 1.2, 1.9, FONT_MED, SOFT, spacing=0.02))
add(text("FOLLOWS", COLS[5], Y_IN + 1.6, 1.9, FONT_MED, SOFT, spacing=0.02))
add(text("GATE 1", COLS[5], Y_IN + 4.4, 1.9, FONT_MED, SOFT, spacing=0.02))
ox0 = 3.2
add(f'<rect x="{ox0}" y="{Y_OUT - 9.0}" width="{W - 2 * ox0}" height="13.6" rx="1.4" fill="{ACCENT}"/>')
for x, n in zip(COLS, ["ENV 1", "END 1", "ENV 2", "END 2", "L", "R"]):
    add(text(n, x, Y_OUT - 5.4, JACK, fill=BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in COLS: c(x, Y_BIG, 6.4, "big knob")
    for x in COLS[:5]: c(x, Y_ATT, 3.1, "trimpot"); c(x, Y_CV, 4.0, "cv jack")
    for x in COLS: c(x, Y_MED, 4.8, "knob")
    for x in (COLS[1], COLS[3]): c(x, Y_FATT, 3.1, "fall trimpot")
    for x in COLS[:5]: c(x, Y_IN, 4.0, "in jack")
    for x in COLS: c(x, Y_OUT, 4.0, "out jack")
    for x in (COLS[0], COLS[2]): c(x + LIGHT_DX, Y_MED + LIGHT_DY, 1.2, "light")
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
open(os.path.join(HERE, "Sawstack.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Sawstack.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
