#!/usr/bin/env python3
# Draws res/Chirp.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Chirp.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 81.28, 128.5  # 16 HP

BG = "#EFEBE1"
PLATE = "#DDD6C6"
LINE = "#B8AE98"
INK = "#1E2328"
SOFT = "#5C6168"
ACCENT = "#2E7A64"   # leaf green
OUTBG = "#243038"

# ---- LAYOUT (same numbers in src/Chirp.cpp) ----
COLS = [11.0, 30.8, 50.5, 70.3]
ROW_Y = [30.0, 51.0]
HEAD_Y = [17.4, 38.4]
TRIG_Y = 67.5
LIGHT_X, LIGHT_Y = 17.6, 63.0
SCOPE_X, SCOPE_Y, SCOPE_W, SCOPE_H = 22.0, 60.6, 37.0, 12.4
JY = [85.0, 99.0, 113.0]

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

# ---- title: the word sits on its own little chirp curve
add(text("Chirp", 4.6, 10.6, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("one sweep, many animals", W - 4.0, 10.2, 2.3, FONT_MED, ACCENT, anchor="end"))
add(f'<path d="M4.6 12.6 C 9 12.6, 12 13.8, 20.5 13.8" fill="none" stroke="{ACCENT}" stroke-width="0.45" stroke-linecap="round"/>')


def heading(s, y, x0=4.0, x1=W - 4.0):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    a = boxes[-1][3] + 1.6
    add(f'<line x1="{a:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def label(s, x, y, col=INK):
    add(text(s, x, y - 7.6, CONTROL, fill=col))


heading("SWEEP", HEAD_Y[0])
for i, n in enumerate(["PITCH", "SWEEP", "TIME", "CURVE"]):
    label(n, COLS[i], ROW_Y[0], ACCENT if n in ("PITCH", "SWEEP") else INK)
heading("SOUND + TRAIN", HEAD_Y[1])
for i, n in enumerate(["DECAY", "TONE", "REPEAT", "RATE"]):
    label(n, COLS[i], ROW_Y[1])

# trigger button, contour screen, loop switch
add(text("TRIG", COLS[0], TRIG_Y - 7.6, CONTROL))
add(f'<rect x="{SCOPE_X - 0.5}" y="{SCOPE_Y - 0.5}" width="{SCOPE_W + 1.0}" height="{SCOPE_H + 1.0}" rx="1.0" '
    f'fill="#151A1E" stroke="{ACCENT}" stroke-opacity="0.7" stroke-width="0.35"/>')
add(text("LOOP", COLS[3], TRIG_Y - 7.6, CONTROL))
add(text("ON", COLS[3] + 3.0, TRIG_Y - 1.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("OFF", COLS[3] + 3.0, TRIG_Y + 3.4, SMALL, FONT_BOLD, SOFT, anchor="start"))

# ---- jacks
add(f'<rect x="2.0" y="{JY[0] - 8.4}" width="{W - 4.0}" height="{JY[2] - JY[0] + 13.2}" rx="1.4" fill="{PLATE}"/>')
for i, n in enumerate(["V/OCT", "SWEEP", "TIME", "CURVE"]):
    add(text(n, COLS[i], JY[0] - 5.2, JACK, FONT_BOLD, SOFT))
for i, n in enumerate(["DECAY", "TONE", "REPEAT", "RATE"]):
    add(text(n, COLS[i], JY[1] - 5.2, JACK, FONT_BOLD, SOFT))
add(text("TRIG", COLS[0], JY[2] - 5.2, JACK, FONT_BOLD, INK))
ox0 = COLS[1] - 7.4
add(f'<rect x="{ox0}" y="{JY[2] - 8.6}" width="{COLS[3] + 7.4 - ox0}" height="13.0" rx="1.4" fill="{OUTBG}"/>')
for i, n in enumerate(["ENV", "PITCH", "OUT"]):
    add(text(n, COLS[1 + i], JY[2] - 5.2, JACK, fill=ACCENT if n == "OUT" else BG))


# ---- overlap check
def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for r, y in enumerate(ROW_Y):
        for i, x in enumerate(COLS):
            c(x, y, 4.8, "knob %d,%d" % (r, i))
    c(COLS[0], TRIG_Y, 3.2, "TRIG btn")
    c(LIGHT_X, LIGHT_Y, 1.2, "light")
    b.append(("LOOP switch", COLS[3] - 2.0, TRIG_Y - 3.6, COLS[3] + 2.0, TRIG_Y + 3.6))
    b.append(("scope", SCOPE_X - 0.5, SCOPE_Y - 0.5, SCOPE_X + SCOPE_W + 0.5, SCOPE_Y + SCOPE_H + 0.5))
    for y in JY:
        for x in COLS:
            c(x, y, 4.0, "jack")
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
open(os.path.join(HERE, "Chirp.svg"), "w").write(svg)
print("ok")

sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Chirp.svg"), x=W / 2, y=125.6, cap=3.0, bg=BG)
