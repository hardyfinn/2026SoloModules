#!/usr/bin/env python3
# Draws res/Gonzo.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Gonzo.cpp (the numbers in the LAYOUT block are the same in both).
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 182.88, 128.5  # 36 HP

BG = "#16141C"
BG_DARK = "#17140F"
LINE = "#4A4036"
INK = "#F0E6D2"
SOFT = "#A39682"
BRASS = "#D2A04E"
VIOLET = "#B48CF0"

# ---- LAYOUT (same numbers in src/Gonzo.cpp) ----
COL = [11.5 + 22.8 * i for i in range(8)]
ROW_GONG, ROW_STRIKE, ROW_CASC, ROW_CV, ROW_IO = 30.0, 53.0, 75.0, 96.0, 114.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4

fonts = {}
def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    """SVG path for string s; (x, y) is the baseline point in mm."""
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
    text.width = width * scale
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

add(text("GONZO", 6.0, 11.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
add(text("the Gongoozler engine, past real gongs", 6.0 + text.width + 4.0, 11.2, SMALL, FONT_MED, SOFT, anchor="start"))
# a gong flying apart: the disc and three rings bursting outwards
cx, cy = W - 32.0, 9.6
add(f'<circle cx="{cx}" cy="{cy}" r="3.0" fill="none" stroke="{BRASS}" stroke-width="0.6"/>')
add(f'<circle cx="{cx}" cy="{cy}" r="0.9" fill="{BRASS}"/>')
for rr, op in ((4.6, 0.8), (6.0, 0.5), (7.4, 0.25)):
    add(f'<path d="M{cx - rr*0.7:.2f} {cy - rr*0.7:.2f} A{rr} {rr} 0 0 1 {cx + rr*0.7:.2f} {cy - rr*0.7:.2f}" fill="none" stroke="{VIOLET}" stroke-opacity="{op}" stroke-width="0.4"/>')


def section(name, y):
    hy = y - 12.6
    add(text(name, 4.0, hy, SMALL, FONT_BOLD, BRASS, anchor="start", spacing=0.1))
    add(f'<line x1="{4.0 + text.width + 2.0:.2f}" y1="{hy - 0.85}" x2="{W - 4.0}" y2="{hy - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')


def hints(x, y, left, right, col=SOFT):
    add(text(left, x - 5.3, y + 5.0, 2.1, FONT_MED, col, anchor="end"))
    add(text(right, x + 5.3, y + 5.0, 2.1, FONT_MED, col, anchor="start"))


def mark(x, y, v, lo, hi):
    """Violet tick at knob value v (range lo..hi): where the real-gong setting sits."""
    import math
    a = math.radians(-135 + 270 * (v - lo) / (hi - lo) - 90)
    r0, r1 = 5.6, 7.0
    add(f'<line x1="{x + r0*math.cos(a):.2f}" y1="{y + r0*math.sin(a):.2f}" x2="{x + r1*math.cos(a):.2f}" y2="{y + r1*math.sin(a):.2f}" stroke="{VIOLET}" stroke-width="0.55" stroke-linecap="round"/>')


LBL = 7.8

section("GONG", ROW_GONG)
hy = ROW_GONG - 12.6
legend = "TYPE  1 TAM-TAM · 2 WIND · 3 OPERA · 4 BOSSED     violet tick = real gong"
text(legend, 0, 0, SMALL, FONT_MED, SOFT)
lw = text.width
add(f'<rect x="{W - 4.0 - lw - 2.0:.2f}" y="{hy - 2.6}" width="{lw + 2.0:.2f}" height="3.4" fill="{BG}"/>')
add(text(legend, W - 4.0, hy, SMALL, FONT_MED, SOFT, anchor="end"))
for x, s in zip(COL, ["TYPE", "SIZE", "THICK", "LAW", "ORDER", "DENSITY", "BEAT", "CAST"]):
    add(text(s, x, ROW_GONG - LBL, CONTROL))
hints(COL[0], ROW_GONG, "1", "4")
hints(COL[1], ROW_GONG, "2cm", "20m")
hints(COL[3], ROW_GONG, "sqz", "str")
hints(COL[4], ROW_GONG, "harm", "rand")
hints(COL[5], ROW_GONG, "few", "all")
hints(COL[7], ROW_GONG, "1", "16")
mark(COL[1], ROW_GONG, 0.5511, 0, 1)
mark(COL[3], ROW_GONG, 0, -2, 1)
mark(COL[4], ROW_GONG, 0, -1, 1)
mark(COL[6], ROW_GONG, 1, 0, 3)

section("STRIKE & RING", ROW_STRIKE)
for x, s in zip(COL, ["POSITION", "MALLET", "BOW", "DECAY", "TILT", "GLIDE", "ROAR", "HIT"]):
    add(text(s, x, ROW_STRIKE - LBL, CONTROL))
hints(COL[0], ROW_STRIKE, "ctr", "edge")
hints(COL[1], ROW_STRIKE, "click", "shove")
hints(COL[3], ROW_STRIKE, "blip", "frz")
hints(COL[4], ROW_STRIKE, "lows", "highs")
hints(COL[5], ROW_STRIKE, "-36", "+36")
mark(COL[1], ROW_STRIKE, 0, -0.5, 1.5); mark(COL[1], ROW_STRIKE, 1, -0.5, 1.5)
mark(COL[3], ROW_STRIKE, 0, -0.4, 1.5); mark(COL[3], ROW_STRIKE, 1, -0.4, 1.5)
mark(COL[4], ROW_STRIKE, 0, -1, 1)
mark(COL[6], ROW_STRIKE, 1, 0, 2)

section("CASCADE  ·  TUNE  ·  OUTPUT", ROW_CASC)
for x, s in zip(COL, ["BLOOM", "BUILD", "THRESH", "TUNE", "MUTE", "WIDTH", "LEVEL", "PITCH"]):
    add(text(s, x, ROW_CASC - LBL, CONTROL))
hints(COL[0], ROW_CASC, "sink", "climb")
hints(COL[1], ROW_CASC, "fast", "slow")
hints(COL[2], ROW_CASC, "easy", "never")
add(text("V/OCT", COL[7] - 3.6, ROW_CASC - 2.2, 2.1, FONT_MED, SOFT, anchor="end"))
add(text("SIZE", COL[7] - 3.6, ROW_CASC + 4.0, 2.1, FONT_MED, SOFT, anchor="end"))
mark(COL[0], ROW_CASC, 0, -1, 1)
mark(COL[1], ROW_CASC, 0, -0.7, 1.6); mark(COL[1], ROW_CASC, 1, -0.7, 1.6)

section("CV IN", ROW_CV)
for x, s in zip(COL, ["V/OCT", "SIZE", "LAW", "ORDER", "POSITION", "MALLET", "BOW", "DECAY"]):
    add(text(s, x, ROW_CV - LBL + 0.6, JACK))

add(f'<line x1="4.0" y1="{ROW_IO - 11.4 - 0.85}" x2="{W - 4.0}" y2="{ROW_IO - 11.4 - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')
for x, s in zip(COL[:6], ["BLOOM", "GLIDE", "TRIG", "VEL", "HAND", "EXCITE"]):
    add(text(s, x, ROW_IO - LBL + 0.6, JACK))
add(f'<rect x="{COL[6] - 8.0}" y="{ROW_IO - 10.6}" width="{COL[7] - COL[6] + 16.0}" height="16.4" rx="1.4" fill="{BRASS}"/>')
add(text("OUT L", COL[6], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))
add(text("OUT R", COL[7], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(os.path.join(HERE, "Gonzo.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Gonzo.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
