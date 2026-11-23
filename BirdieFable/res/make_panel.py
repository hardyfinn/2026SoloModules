#!/usr/bin/env python3
# Draws res/Birdie.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Birdie.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 182.88, 128.5  # 36 HP

BG = "#EEE7D5"
BG_B = "#E2D9C2"
PLATE = "#D6CBB0"
LINE = "#B9AD92"
INK = "#1F2430"
SOFT = "#5E6470"
ACCENT = "#C4501A"   # robin orange
OUTBG = "#2A3140"

# ---- LAYOUT (same numbers in src/Birdie.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 32.0, 4.4, 72.0, 8.2
LIGHT_X, LIGHT_Y = 108.5, 8.5
COLS = [10.6, 28.8, 47.0, 65.2, 83.4, 101.6, 119.8, 138.0]
ROW_Y = [30.0, 48.5, 67.0, 85.5]
HEAD_Y = [19.2, 37.7, 56.2, 74.7]
RX = [157.4, 173.4]
RY = [30.0, 48.5, 67.0]
JX0, JDX = 7.62, 15.24
JA_Y, JB_Y = 103.0, 116.5

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3     # switch position marks
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
add(f'<rect x="146.0" y="15.0" width="{W - 146.0}" height="{HEAD_Y[3] - 3.2 - 15.0}" fill="{BG_B}"/>')

# ---- title, display
add(text("Birdie", 5.0, 10.8, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("one bird, by physical model", W - 4.0, 10.2, 2.4, FONT_MED, ACCENT, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#14171F" stroke="{ACCENT}" stroke-opacity="0.7" stroke-width="0.35"/>')


def heading(s, y, x0=4.0, x1=144.0, subs=()):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    segs = [boxes[-1][3] + 1.6]
    for t, sx in subs:
        add(text(t, sx, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
        segs += [sx - 1.6, boxes[-1][3] + 1.6]
    segs.append(x1)
    for a, b in zip(segs[0::2], segs[1::2]):
        add(f'<line x1="{a:.2f}" y1="{y - 0.8:.2f}" x2="{b:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


def label(s, x, y, col=INK):
    add(text(s, x, y - 7.6, CONTROL, fill=col))


def switch(s, x, y, top, bottom):
    add(text(s, x, y - 7.6, CONTROL))
    add(text(top, x + 3.0, y - 1.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(bottom, x + 3.0, y + 3.4, SMALL, FONT_BOLD, SOFT, anchor="start"))


heading("VOICE", HEAD_Y[0], subs=[("2ND VOICE", COLS[5] - 6.0)])
for i, n in enumerate(["SIZE", "TENSION", "PRESSURE", "BREATH", "ROUGH", "MIX", "INTERVAL", "SLIDE"]):
    label(n, COLS[i], ROW_Y[0], ACCENT if n in ("SIZE", "TENSION") else INK)
heading("NOTE", HEAD_Y[1], subs=[("BUZZ", COLS[5] - 6.0)])
for i, n in enumerate(["LENGTH", "SLIDE", "SHAPE", "ATTACK", "RELEASE", "RATE", "DEPTH", "CHOP"]):
    label(n, COLS[i], ROW_Y[1])
heading("BODY", HEAD_Y[2], subs=[("STRETCH", COLS[6] - 6.0)])
for i, n in enumerate(["TRACHEA", "TUBE", "THROAT", "TUNE", None, "BEAK", "TIME", "HOLD"]):
    if n: label(n, COLS[i], ROW_Y[2])
switch("FOLLOW", COLS[4] - 2.0, ROW_Y[2], "ON", "OFF")
heading("PHRASE", HEAD_Y[3], x1=W - 4.0)
for i, n in enumerate(["NOTES", "RATE", "PATTERN", "VARY", "DRIFT", "ACCEL", "SWELL", "GAP"]):
    label(n, COLS[i], ROW_Y[3])

# ---- right column: play
heading("PLAY", HEAD_Y[0], x0=149.0, x1=W - 4.0)
label("NOTE", RX[0], RY[0], ACCENT)
label("PHRASE", RX[1], RY[0], ACCENT)
label("NEW", RX[0], RY[1])
add(text("seed", RX[0], RY[1] + 6.4, SMALL, FONT_MED, SOFT))
switch("AUTO", RX[1] - 2.0, RY[1], "ON", "OFF")
switch("CLOCK", RX[0] - 2.0, RY[2], "LOCK", "FREE")

# ---- jacks
add(f'<rect x="2.0" y="{JA_Y - 8.6}" width="{W - 4.0}" height="{JB_Y - JA_Y + 13.4}" rx="1.4" fill="{PLATE}"/>')
def jx(i): return JX0 + JDX * i
for i, n in enumerate(["SIZE", "TENSION", "PRESS", "ROUGH", "MIX 2", "LENGTH", "SLIDE", "BUZZ", "BEAK", "TIME", "RATE", "VARY"]):
    add(text(n, jx(i), JA_Y - 5.2, JACK, FONT_BOLD, SOFT))
for i, n in enumerate(["NOTE", "PHRASE", "NEW", "CLOCK", "HOLD"]):
    add(text(n, jx(i), JB_Y - 5.2, JACK, FONT_BOLD, INK))
ox0 = jx(7) - 6.6
add(f'<rect x="{ox0}" y="{JB_Y - 8.8}" width="{jx(11) + 6.6 - ox0}" height="13.4" rx="1.4" fill="{OUTBG}"/>')
for i, n in enumerate(["END", "GATE", "PRESS", "PITCH", "OUT"]):
    add(text(n, jx(7 + i), JB_Y - 5.2, JACK, fill=ACCENT if n == "OUT" else BG))


# ---- overlap check against each other and against controls
def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for r, y in enumerate(ROW_Y):
        for i, x in enumerate(COLS):
            if r == 2 and i == 4:
                b.append(("FOLLOW switch", x - 2.0 - 2.0, y - 3.6, x - 2.0 + 2.0, y + 3.6))
            elif r == 2 and i == 7:
                c(x, y, 3.2, "HOLD")
            else:
                c(x, y, 4.8, "knob %d,%d" % (r, i))
    c(RX[0], RY[0], 3.2, "NOTE btn"); c(RX[1], RY[0], 3.2, "PHRASE btn"); c(RX[0], RY[1], 3.2, "NEW btn")
    b.append(("AUTO switch", RX[1] - 4.0, RY[1] - 3.6, RX[1], RY[1] + 3.6))
    b.append(("CLOCK switch", RX[0] - 4.0, RY[2] - 3.6, RX[0], RY[2] + 3.6))
    for y in (JA_Y, JB_Y):
        for i in range(12):
            if y == JB_Y and i in (5, 6): continue
            c(jx(i), y, 4.0, "jack")
    c(LIGHT_X, LIGHT_Y, 1.6, "light")
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
open(os.path.join(HERE, "Birdie.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Birdie.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
