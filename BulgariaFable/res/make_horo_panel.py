#!/usr/bin/env python3
# Draws res/Horo.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Horo.cpp.
#   pip install fonttools ; python3 res/make_horo_panel.py
import os, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 50.8, 128.5  # 10 HP

BG = "#2A2422"
BG_DARK = "#171312"
LINE = "#4E4440"
INK = "#F1EADF"
SOFT = "#B5A79C"
ACCENT = "#E8B04A"   # embroidery gold
RED = "#D9534A"      # embroidery red

# ---- LAYOUT (same numbers in src/Horo.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 3.0, 14.0, 44.8, 24.0
METRE_X, METRE_Y = 16.0, 53.0
RBTN_X, RBTN_Y = 39.0, 53.0
KROW_Y = 72.0
KROW_X = [9.5, 25.4, 41.3]
IN_Y = 90.0
IN_X = [7.4, 19.4, 31.4, 43.4]
OUT_Y = [104.5, 117.0]
OUT_X = [9.0, 25.4, 41.8]
BIG_R = 7.6
KNOB_R = 5.0

CONTROL = 2.4
JACK = 2.0
SMALL = 1.5
HEAD = 2.0

fonts = {}
boxes = []

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
for i in range(int(W / 3.0)):
    x = 1.5 + i * 3.0
    add(f'<path d="M{x:.2f} 2.2 l1.2 1.2 l1.2 -1.2" fill="none" stroke="{RED if i % 2 else ACCENT}" stroke-width="0.35"/>')
add(text("HORO", 4.0, 10.0, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.06))
add(text("uneven metres", W - 3.5, 9.6, 2.0, FONT_MED, ACCENT, anchor="end"))
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#120E0D" stroke="{LINE}" stroke-width="0.3"/>')

add(text("METRE", METRE_X, METRE_Y - 9.6, CONTROL))
add(text("RESET", RBTN_X, METRE_Y - 5.0, CONTROL))
for x, n in zip(KROW_X, ["BARS", "LILT", "ACCENT"]):
    add(text(n, x, KROW_Y - 6.4, CONTROL))
add(text("1", KROW_X[0] - 4.2, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))
add(text("16", KROW_X[0] + 4.2, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))
add(text("EVEN", KROW_X[1] - 4.4, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))
add(text("LONG", KROW_X[1] + 4.4, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))
add(text("0", KROW_X[2] - 4.2, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))
add(text("FULL", KROW_X[2] + 4.2, KROW_Y + 7.2, SMALL, FONT_BOLD, SOFT))

add(f'<rect x="2.0" y="{IN_Y - 7.6}" width="{W - 4.0}" height="12.0" rx="1.4" fill="{BG_DARK}"/>')
for i, (x, n) in enumerate(zip(IN_X, ["CLOCK", "RESET", "METRE", "LILT"])):
    add(text(n, x, IN_Y - 4.8, JACK if len(n) <= 5 else 1.8, FONT_BOLD, INK if i < 2 else SOFT))
add(f'<rect x="2.0" y="{OUT_Y[0] - 8.0}" width="{W - 4.0}" height="{OUT_Y[1] - OUT_Y[0] + 12.4}" rx="1.4" fill="{ACCENT}"/>')
for r, names in enumerate([["PULSE", "BEAT", "LONG"], ["BAR", "PHRASE", "ACCENT"]]):
    for x, n in zip(OUT_X, names):
        add(text(n, x, OUT_Y[r] - 4.9, JACK if len(n) <= 5 else 1.8, FONT_BOLD, BG_DARK))


def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    c(METRE_X, METRE_Y, BIG_R, "metre")
    c(RBTN_X, RBTN_Y, 3.0, "button")
    for x in KROW_X: c(x, KROW_Y, KNOB_R, "knob")
    for x in IN_X: c(x, IN_Y, 4.0, "in")
    for y in OUT_Y:
        for x in OUT_X: c(x, y, 4.0, "out")
    c(OUT_X[1] + 5.4, OUT_Y[0] - 4.6, 0.8, "light")
    c(OUT_X[0] + 5.4, OUT_Y[1] - 4.6, 0.8, "light")
    c(OUT_X[1] + 6.4, OUT_Y[1] - 4.6, 0.8, "light")
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
open(os.path.join(HERE, "Horo.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Horo.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
