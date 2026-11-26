#!/usr/bin/env python3
# Draws res/Deidre.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm and match src/Layout.hpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_SEMI = os.environ.get("PANEL_FONT_SEMI", "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 284.48, 128.5  # 56 HP
INK = "#1F1D1A"; PANEL = "#ECE6DA"; BOX = "#E2DBCD"; MUTED = "#6B655C"; CREAM = "#F4EFE6"
LANE = ["#E4572E", "#F3A712", "#C9B200", "#4CAF50", "#17BEBB", "#2E86DE", "#8E5CD9", "#D9489B"]
CVC = ["#E4572E", "#2E86DE", "#4CAF50", "#8E5CD9"]
BANDS = [("TIME", "#F6D9C9"), ("WAVE", "#F8E7BD"), ("EUCLID", "#D6EAD2"), ("LOOP", "#CFE6EA"), ("LOGIC", "#DDD6F0")]
ROWS = [["RATE", "PHASE", "PROB", "FLEX", "AMOUNT"], ["SHAPE", "WIDTH", "SLEW", "LEVEL", "OFFSET"],
        ["STEPS", "TRIGS", "PAD", "RAND", "SHIFT"], ["LOOP", "NAP", "WAKE", "SHIFT"], ["CROSS", "SOURCE", "SCALE", "INVERT"]]
CONTROL, JACK, HEAD = 3.2, 2.9, 2.9

# ---- LAYOUT (same as src/Layout.hpp)
BPM_X, BPM_Y = 15.0, 27.0
RUN_X, RUN_Y, RESET_Y = 35.0, 22.0, 37.0
COL_A, COL_B, COL_C = 9.0, 22.0, 35.0
ROW_BTN, ROW_BANK, ROW_IN1, ROW_IN2 = 50.0, 68.0, 88.0, 107.0
SX, SY, SW, SH = 48.0, 12.0, 120.0, 72.0
PAGE_X0, PAGE_DX, PAGE_Y = 56.0, 14.4, 90.0
DATA_X, DATA_Y = 160.0, 91.0
BX0, BY0, BH, KPITCH, KCX0 = 172.0, 12.0, 17.2, 20.3, 190.65
OX0, OY, OCP = 48.0, 101.0, 26.81
outC = lambda i: 67.0 + i * OCP

fonts = {}
def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.02, rotate=None):
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
    tr = f' transform="rotate({rotate[0]} {rotate[1]} {rotate[2]})"' if rotate else ""
    return f'<path fill="{fill}" d="{pen.getCommands()}"{tr}/>'

P = []
add = P.append
add(f'<rect width="{W}" height="{H}" fill="{PANEL}"/>')
add(f'<rect width="{W}" height="9.5" fill="{INK}"/>')
add(f'<rect y="{H - 6}" width="{W}" height="6" fill="{INK}"/>')
add(text("DEIDRE", 13, 6.7, 4.8, FONT_BOLD, CREAM, "start", 0.12))
add(text("Deidre’s Latest Workout", 37, 6.6, 3.2, FONT_SEMI, "#D8D0C2", "start"))
add(text("8-output clocked modulation", W - 14, 6.6, 2.9, FONT_MED, "#8D877C", "end"))

# ---- left column
add(f'<rect x="3" y="12" width="41" height="109.5" rx="2" fill="{BOX}"/>')
add(text("CLOCK", 5, 16.2, HEAD, FONT_BOLD, MUTED, "start", 0.06))
add(text("BPM", BPM_X, 40.5, CONTROL))
add(text("RUN", RUN_X, 30.6, CONTROL))
add(text("RESET", RUN_X, 45.2, CONTROL))
for x, n in ((COL_A, "TAP"), (COL_B, "RESEED"), (COL_C, "COPY")):
    add(text(n, x, ROW_BTN + 8.0, CONTROL))
add(text("BANK", 5, 63.2, HEAD, FONT_BOLD, MUTED, "start", 0.06))
add(text("◀", COL_A, ROW_BANK + 8.0, CONTROL)); add(text("▶", COL_B, ROW_BANK + 8.0, CONTROL))
add(text("SAVE", COL_C, ROW_BANK + 8.0, CONTROL))
add(text("INPUTS", 5, 80.8, HEAD, FONT_BOLD, MUTED, "start", 0.06))
ins = [("CLK", COL_A, ROW_IN1, None), ("RUN", COL_B, ROW_IN1, None), ("CV 1", COL_C, ROW_IN1, 0),
       ("CV 2", COL_A, ROW_IN2, 1), ("CV 3", COL_B, ROW_IN2, 2), ("CV 4", COL_C, ROW_IN2, 3)]
for n, x, y, c in ins:
    add(text(n, x, y + 9.4, JACK))
    if c is not None:
        add(f'<circle cx="{x + 4.6}" cy="{y - 4.3}" r="1.05" fill="{CVC[c]}"/>')

# ---- screen bezel (the screen itself is drawn by the module)
add(f'<rect x="{SX - 1.5}" y="{SY - 1.5}" width="{SW + 3}" height="{SH + 3}" rx="2.5" fill="#2A2826"/>')
add(f'<rect x="{SX}" y="{SY}" width="{SW}" height="{SH}" rx="1" fill="#0E1116"/>')
add(text("PAGES", PAGE_X0 + 3 * PAGE_DX, 98.4, HEAD, FONT_BOLD, MUTED, "middle", 0.06))
add(f'<line x1="{PAGE_X0}" y1="96.0" x2="{PAGE_X0 + 2 * PAGE_DX - 2}" y2="96.0" stroke="{MUTED}" stroke-width="0.3"/>')
add(f'<line x1="{PAGE_X0 + 4 * PAGE_DX + 2}" y1="96.0" x2="{PAGE_X0 + 6 * PAGE_DX}" y2="96.0" stroke="{MUTED}" stroke-width="0.3"/>')
add(text("DATA", DATA_X, 99.8, CONTROL))

# ---- knob bank
for r, (bname, bcol) in enumerate(BANDS):
    y0 = BY0 + r * BH
    add(f'<rect x="{BX0}" y="{y0 + 0.4}" width="{W - 3 - BX0}" height="{BH - 0.8}" rx="2" fill="{bcol}"/>')
    add(f'<rect x="{BX0}" y="{y0 + 0.4}" width="6.5" height="{BH - 0.8}" rx="2" fill="{INK}" fill-opacity="0.85"/>')
    cy = y0 + BH / 2
    add(text(bname, BX0 + 4.3, cy, 2.9, FONT_BOLD, CREAM, "middle", 0.06, rotate=(-90, BX0 + 4.3, cy)))
    for c, n in enumerate(ROWS[r]):
        add(text(n, KCX0 + c * KPITCH, y0 + 15.1, CONTROL))
add(text("knobs follow", KCX0 + 4 * KPITCH, BY0 + 3 * BH + 7.2, 2.3, FONT_SEMI, MUTED))
add(text("the selected", KCX0 + 4 * KPITCH, BY0 + 3 * BH + 10.0, 2.3, FONT_SEMI, MUTED))
add(text("output", KCX0 + 4 * KPITCH, BY0 + 3 * BH + 12.8, 2.3, FONT_SEMI, MUTED))
add(f'<circle cx="{KCX0 + 4 * KPITCH - 6.2}" cy="{BY0 + 4 * BH + 6.4}" r="1.05" fill="{CVC[1]}"/>')
add(text("= CV on", KCX0 + 4 * KPITCH + 1.2, BY0 + 4 * BH + 7.2, 2.3, FONT_SEMI, MUTED))
add(text("right-click", KCX0 + 4 * KPITCH, BY0 + 4 * BH + 10.0, 2.3, FONT_SEMI, MUTED))
add(text("a knob", KCX0 + 4 * KPITCH, BY0 + 4 * BH + 12.8, 2.3, FONT_SEMI, MUTED))

# ---- outputs
add(f'<rect x="{OX0 - 1.5}" y="{OY}" width="{W - 3 - OX0 + 1.5}" height="20.5" rx="2" fill="{BOX}"/>')
add(text("OUTPUTS", OX0 + 1, OY + 4.3, HEAD, FONT_BOLD, MUTED, "start", 0.06))
add(text("SELECT", OX0 + 16.5, OY + 10.3, HEAD, FONT_BOLD, MUTED, "end", 0.04))
add(text("MUTE", OX0 + 16.5, OY + 17.9, HEAD, FONT_BOLD, MUTED, "end", 0.04))
for i in range(8):
    c = outC(i)
    add(f'<rect x="{c + 12.6}" y="{OY + 15.7}" width="7.8" height="0.9" rx="0.45" fill="{LANE[i]}"/>')
    add(text(str(i + 1), c + 16.5, OY + 19.9, JACK, FONT_BOLD, INK))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(P) + "\n</svg>\n")
open(os.path.join(HERE, "Deidre.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Deidre.svg"), x=W / 2, y=126.0, cap=3.0, bg=INK)
