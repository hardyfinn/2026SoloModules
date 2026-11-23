#!/usr/bin/env python3
# Draws res/Krka.svg and writes src/KrkaLayout.hpp (every position the C++
# needs), so the panel and the code can't drift apart. Rack can't draw SVG
# <text>, so labels become paths.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

BG = "#123A40"
BG_ROW = "#174850"
BG_DARK = "#0C2A2F"
LINE = "#2C6068"
INK = "#F3EBDD"
SOFT = "#A9C9C6"
CORAL = "#FF8A5B"

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5

fonts = {}


def font(f):
    if f not in fonts:
        t = TTFont(f)
        fonts[f] = (t, t.getGlyphSet(), t.getBestCmap(), t["head"].unitsPerEm)
    return fonts[f]


def text_width(s, size, f=FONT_BOLD, spacing=0.04):
    t, gs, cmap, upm = font(f)
    names = [cmap[ord(c)] for c in s]
    return (sum(gs[n].width + spacing * upm for n in names) - spacing * upm) * size / upm


def text(s, x, y, size, f=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    t, gs, cmap, upm = font(f)
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
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


# ---- columns of each channel row: (label, kind). Widths are measured so
# no header label touches its neighbour.
COLS = [("TRIG", "jack"), ("FIRE", "button"), ("BANK", "knob"), ("SHAPE", "knob"), ("REV", "latch"),
        ("TIME", "bigknob"), ("RESP", "knob"), ("WARP", "knob"), ("LEVEL", "knob"), ("GATE", "latch"),
        ("ANALOG", "latch"), ("PHASE", "knob"), ("REPEAT", "knob"), ("DEST", "knob"), ("DEPTH", "knob"),
        ("CV", "jack"), ("OUT", "jack"), ("END", "jack")]
COMP_W = {"jack": 8.4, "button": 7.0, "knob": 8.0, "bigknob": 10.0, "latch": 7.0}
GAP = 1.9
xs = []
x = 10.5
prev_half = 0.0
for i, (lab, kind) in enumerate(COLS):
    size = JACK if kind == "jack" else CONTROL
    w = max(text_width(lab, size), COMP_W[kind])
    if i == 0:
        x += w / 2
    else:
        x += prev_half + GAP + w / 2
    xs.append(round(x, 3))
    prev_half = w / 2
right_edge = xs[-1] + prev_half + 3.0
GX0 = right_edge                     # global block starts here
HP = 5.08
need = GX0 + 36.0
NHP = int(-(-need // HP))
W = round(NHP * HP, 2)
H = 128.5

TITLE_Y = 11.0
DISP_X, DISP_Y, DISP_W, DISP_H = 4.5, 14.0, round(GX0 - 4.5 - 2.5, 3), 25.0
HEAD_Y = 45.4
ROW_Y = [53.0, 70.0, 87.0, 104.0]
LEGEND_Y = 116.0
GX = [round(GX0 + 9.0, 3), round(GX0 + 26.0, 3)]
GCX = round((GX[0] + GX[1]) / 2, 3)
G_ALGO_Y = 26.0
G_ALGOCV_Y = 43.0
G_CLOCK_Y = 62.0
G_PRESET_Y = 84.0
G_BTN_Y = 101.0

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
# row bands
for i, y in enumerate(ROW_Y):
    add(f'<rect x="2.0" y="{y - 7.6}" width="{GX0 - 3.5}" height="15.2" rx="1.6" fill="{BG_ROW}"/>')
    add(text("ABCD"[i], 5.2, y + 2.2, 6.0, FONT_BOLD, CORAL))
# OUT/END highlight
ox = xs[16]
add(f'<rect x="{ox - 5.2}" y="{HEAD_Y - 4.4}" width="{xs[17] - ox + 10.4}" height="{ROW_Y[-1] - HEAD_Y + 12.2}" rx="1.6" fill="{CORAL}" fill-opacity="0.9"/>')
add(f'<line x1="{GX0 - 0.2}" y1="4" x2="{GX0 - 0.2}" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')
add(f'<rect x="{GX0 + 0.3}" y="0" width="{W - GX0 - 0.3}" height="{H}" fill="{BG_DARK}"/>')

# title
add(text("Krka", 5.0, TITLE_Y, 6.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("four function generators · 260 shapes · chains", DISP_X + DISP_W, TITLE_Y - 0.4, 2.7, FONT_MED, CORAL, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" rx="1.0" '
    f'fill="#041417" stroke="{CORAL}" stroke-opacity="0.55" stroke-width="0.35"/>')

# column headers
for (lab, kind), x in zip(COLS, xs):
    jack = kind == "jack"
    fill = BG if lab in ("OUT", "END") else (SOFT if jack else (CORAL if lab in ("TIME", "SHAPE") else INK))
    add(text(lab, x, HEAD_Y, JACK if jack else CONTROL, FONT_BOLD, fill))

# legend under the rows
leg = "DEST:  1 SHAPE   2 REV   3 TIME   4 LEVEL   5 RESP   6 WARP   7 PHASE   8 REPEAT"
add(text(leg, 10.0, LEGEND_Y, SMALL, FONT_MED, SOFT, anchor="start"))
add(text("REPEAT fully right = forever", GX0 - 3.0, LEGEND_Y, SMALL, FONT_MED, SOFT, anchor="end"))
add(text("TIME 0.85 ms – 30 min, or note values when CLOCK is patched", 10.0, LEGEND_Y + 4.0, SMALL, FONT_MED, SOFT, anchor="start"))

# global block
add(text("CHAIN", GCX, 13.0, 4.2, FONT_BOLD, CORAL))
add(text("ALGO", GCX, G_ALGO_Y - 7.6, CONTROL))
add(text("ALGO CV", GCX, G_ALGOCV_Y - 5.6, JACK, FONT_BOLD, SOFT))
add(f'<line x1="{GX0 + 3}" y1="{G_CLOCK_Y - 12.4}" x2="{W - 3}" y2="{G_CLOCK_Y - 12.4}" stroke="{LINE}" stroke-width="0.4"/>')
add(text("CLOCK", GX[0], G_CLOCK_Y - 5.6, JACK, FONT_BOLD, SOFT))
add(text("RESET", GX[1], G_CLOCK_Y - 5.6, JACK, FONT_BOLD, SOFT))
add(f'<line x1="{GX0 + 3}" y1="{G_PRESET_Y - 13.4}" x2="{W - 3}" y2="{G_PRESET_Y - 13.4}" stroke="{LINE}" stroke-width="0.4"/>')
add(text("PRESET", GCX, G_PRESET_Y - 7.6, CONTROL, FONT_BOLD, CORAL))
add(text("1", GCX - 7.0, G_PRESET_Y + 6.4, SMALL, FONT_BOLD, SOFT))
add(text("18", GCX + 7.0, G_PRESET_Y + 6.4, SMALL, FONT_BOLD, SOFT))
add(text("LOAD", GX[0], G_BTN_Y - 5.0, CONTROL))
add(text("SAVE", GX[1], G_BTN_Y - 5.0, CONTROL))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Krka.svg"), "w").write(svg)

# ---- layout header for the C++
L = ["// Generated by res/make_panel.py - do not edit by hand.", "#pragma once", "namespace krka {",
     f"static const float PANEL_W = {W}f;",
     f"static const float COLX[{len(xs)}] = {{{', '.join(f'{v}f' for v in xs)}}};",
     f"static const float ROW_Y[4] = {{{', '.join(f'{v}f' for v in ROW_Y)}}};",
     f"static const float DISP_X = {DISP_X}f, DISP_Y = {DISP_Y}f, DISP_W = {DISP_W}f, DISP_H = {DISP_H}f;",
     f"static const float GX[2] = {{{GX[0]}f, {GX[1]}f}};",
     f"static const float GCX = {GCX}f;",
     f"static const float G_ALGO_Y = {G_ALGO_Y}f, G_ALGOCV_Y = {G_ALGOCV_Y}f, G_CLOCK_Y = {G_CLOCK_Y}f, "
     f"G_PRESET_Y = {G_PRESET_Y}f, G_BTN_Y = {G_BTN_Y}f;",
     "} // namespace krka", ""]
open(os.path.join(HERE, "..", "src", "KrkaLayout.hpp"), "w").write("\n".join(L))
print(f"ok: {NHP} HP, columns end at {right_edge:.1f} mm")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Krka.svg"), x=GCX - 5.0, y=126.0, cap=3.0, bg=BG_DARK)
