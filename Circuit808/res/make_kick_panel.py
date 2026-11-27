#!/usr/bin/env python3
# Draws res/Kick808.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions in mm; the LAYOUT block is shared with src/Kick808.cpp.
#   pip install fonttools ; python3 res/make_kick_panel.py
import os, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 142.24, 128.5  # 28 HP

BG = "#1F2023"
BG_DARK = "#16171A"
LINE = "#3A3B40"
INK = "#ECE6D8"
SOFT = "#9A968C"
RED = "#E0533D"       # 808 step-button red

# ---- LAYOUT (same numbers in src/Kick808.cpp) ----
COL = [11.0, 31.04, 51.08, 71.12, 91.16, 111.2, 131.24]
ROW_STOCK, ROW_ATK, ROW_BODY, ROW_CV, ROW_IO = 30.0, 52.0, 74.0, 96.0, 115.0
# stock 808 knob positions (0..1) for the red dots; same as src/KickEngine.hpp Stock
TUNE_STOCK = math.log(49.5 / 20.0) / math.log(220.0 / 20.0)
DOTS = {
    ROW_STOCK: [TUNE_STOCK, 0.45, 0.5, 0.6, None, 0.0, None],
    ROW_ATK: [0.45, 0.4376, 0.5, 0.5, 0.2, 0.25, 0.0],
    ROW_BODY: [0.33, 0.0, None, None, None, None, None],
}

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
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

# title + the ring of a bridged-T (a fast first cycle, then the 49.5 Hz body)
add(text("808 KICK", 6.0, 11.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
pts = []
for i in range(161):
    t = i / 160.0  # 0..1 = 0..120 ms
    ms = t * 120.0
    ph = 2 * math.pi * (ms / 1000.0) * 49.5 + 2 * math.pi * 0.06 * (1 - math.exp(-ms / 3.0))
    a = math.exp(-ms / 45.0)
    pts.append((37.0 + t * 46.0, 8.6 - 2.6 * a * math.sin(ph * 1.0)))
add('<path d="M' + " L".join(f"{x:.2f} {y:.2f}" for x, y in pts) + f'" stroke="{RED}" stroke-width="0.45" fill="none" stroke-linejoin="round"/>')
add(text("TR-808 bass drum circuit", W - 6.0, 11.2, SMALL, FONT_MED, SOFT, anchor="end"))


def section(name, y, x0=4.0, x1=None):
    hy = y - 12.6
    add(text(name, x0, hy, SMALL, FONT_BOLD, RED, anchor="start", spacing=0.1))
    add(f'<line x1="{x0 + text.width + 2.0:.2f}" y1="{hy - 0.85}" x2="{(x1 or W - 4.0):.2f}" y2="{hy - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')


LBL = 7.8


def dots(y):
    for x, v in zip(COL, DOTS.get(y, [])):
        if v is None:
            continue
        a = (-0.83 + 1.66 * v) * math.pi
        add(f'<circle cx="{x + 6.3 * math.sin(a):.2f}" cy="{y - 6.3 * math.cos(a):.2f}" r="0.62" fill="{RED}"/>')


def marks(x, y, lo, hi):
    add(text(lo, x - 6.4, y + 6.6, 2.0, FONT_MED, SOFT, anchor="end"))
    add(text(hi, x + 6.4, y + 6.6, 2.0, FONT_MED, SOFT, anchor="start"))


section("STOCK 808", ROW_STOCK)
for x, s in zip(COL, ["TUNE", "DECAY", "TONE", "ACCENT", "LEVEL", "AGE", "HIT"]):
    add(text(s, x, ROW_STOCK - LBL, CONTROL))
dots(ROW_STOCK)
marks(COL[0], ROW_STOCK, "20", "220")
marks(COL[1], ROW_STOCK, "", "SUST")
marks(COL[5], ROW_STOCK, "NEW", "OLD")

section("ATTACK", ROW_ATK, x1=COL[4] + 8.0)
section("BODY", ROW_ATK, x0=COL[5] - 8.0)
for x, s in zip(COL, ["SWEEP", "BEND", "STRIKE", "CLICK", "RETRIG", "SIGH", "TENSION"]):
    add(text(s, x, ROW_ATK - LBL, CONTROL))
dots(ROW_ATK)

section("FEEDBACK + OUTPUT", ROW_BODY, x1=COL[1] + 8.0)
for x, s in zip(COL[:2], ["SHELF", "DRIVE"]):
    add(text(s, x, ROW_BODY - LBL, CONTROL))
dots(ROW_BODY)
# signal flow and the dot legend in the free space
fy = ROW_BODY - 4.0
flow = ["TRIG", "SHAPER", "BRIDGED-T", "TONE", "OUT"]
fx = COL[2] - 6.0
add(f'<rect x="{COL[2] - 9.0}" y="{ROW_BODY - 14.2}" width="{W - 4.0 - (COL[2] - 9.0):.2f}" height="19.6" rx="1.4" fill="{BG_DARK}"/>')
xs = [COL[2] - 4.0, COL[3] - 4.5, COL[4] - 1.0, COL[5] + 3.0, COL[6] + 3.0]
for i, s in enumerate(flow):
    add(text(s, xs[i], fy, 2.4, FONT_BOLD, INK))
    if i < len(flow) - 1:
        add(text("→", (xs[i] + xs[i + 1]) / 2 + (1.5 if i == 0 else 0.6), fy, 2.4, FONT_MED, SOFT))
add(text("feedback (DECAY, SHELF)  ·  attack jump + sigh (Q43)", (COL[2] + W - 4.0) / 2 - 4.0, fy + 4.6, 2.1, FONT_MED, SOFT))
add(f'<circle cx="{COL[2] - 5.0}" cy="{fy + 8.5}" r="0.62" fill="{RED}"/>')
add(text("= stock 808 setting   ·   after Werner, Abel & Smith, DAFx-14", COL[2] - 3.6, fy + 9.3, 2.1, FONT_MED, SOFT, anchor="start"))

section("CV IN", ROW_CV)
for x, s in zip(COL, ["V/OCT", "DECAY", "TONE", "SWEEP", "SIGH", "TENSION", "DRIVE"]):
    add(text(s, x, ROW_CV - LBL + 0.6, JACK))

add(f'<line x1="4.0" y1="{ROW_IO - 13.45}" x2="{W - 4.0}" y2="{ROW_IO - 13.45}" stroke="{LINE}" stroke-width="0.3"/>')
for x, s in zip([COL[0], COL[1]], ["TRIG", "ACCENT"]):
    add(text(s, x, ROW_IO - LBL + 0.6, JACK))
add(text("ENV", COL[5], ROW_IO - LBL + 0.6, JACK))
add(f'<rect x="{COL[6] - 8.0}" y="{ROW_IO - 11.0}" width="16.0" height="17.0" rx="1.4" fill="{RED}"/>')
add(text("OUT", COL[6], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))
add(text("ACCENT: gate = accented, 0-5 V = how much", (COL[2] + COL[4]) / 2 + 1.0, ROW_IO + 0.8, 2.1, FONT_MED, SOFT))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(os.path.join(HERE, "Kick808.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Kick808.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
