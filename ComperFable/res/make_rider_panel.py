#!/usr/bin/env python3
# Draws res/Rider.svg (30 HP). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Rider.cpp.
#   pip install fonttools ; python3 res/make_rider_panel.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#23262B"
BG_DARK = "#16181C"
LINE = "#3D424A"
INK = "#EFE9DD"
SOFT = "#A8A397"
ACCENT = "#D9A441"

# ---- LAYOUT (same numbers in src/Rider.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 4.0, 12.5, 144.4, 13.0
COL_X0, COL_DX = 9.0, 14.9         # 10 columns: PLAYER, HANDS + KIT, the jacks
SND_X0, SND_DX = 14.0, 24.88       # 6 columns: the kit's sound
IN_X0, IN_DX = 9.0, 22.35          # 7 columns: the inputs
Y_PLAYER = 41.5
Y_HANDS = 62.0
Y_SOUND = 83.0
Y_IN1 = 101.0
Y_IN2 = 114.5

CONTROL = 2.2
JACK = 2.15
SMALL = 1.9
HEAD = 2.4

def col(i): return COL_X0 + i * COL_DX
def scol(i): return SND_X0 + i * SND_DX
def icol(i): return IN_X0 + i * IN_DX

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
add(text("Rider", 5.0, 9.4, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("jazz drummer · sits next to Comper and Walker", W - 5.0, 9.0, 2.4, FONT_MED, ACCENT, anchor="end"))

def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')

# PLAYER
heading("PLAYER", 3.2, W - 3.2, 29.8)
PLAYER = ["FEEL", "ENERGY", "COMP", "LISTEN", "FILLS", "FEATHER", "REPEAT", "BAND", "SEED", "NEW"]
for i, n in enumerate(PLAYER):
    add(text(n, col(i), Y_PLAYER - (8.4 if n == "ENERGY" else 6.2), CONTROL if len(n) < 7 else SMALL))
add(text("0 = FOLLOW", col(0), Y_PLAYER + 6.2, 1.5, FONT_BOLD, SOFT))
add(text("LEADS THE BAND", col(7), Y_PLAYER + 6.2, 1.5, FONT_BOLD, SOFT))
add(text("BASS DRUM", col(5), Y_PLAYER + 6.2, 1.5, FONT_BOLD, SOFT))

# HANDS and KIT levels
heading("HANDS", 3.2, col(1) + 5.6, 51.2)
heading("KIT", col(2) - 4.6, col(6) + 5.0, 51.2)
HANDS = ["LOOSE", "AHEAD"]
KITL = ["RIDE", "HATS", "SNARE", "KICK", "TOMS"]
add(text("LOOSE", col(0), Y_HANDS - 6.2, CONTROL))
add(text("AHEAD", col(1), Y_HANDS - 6.2, CONTROL))
add(text("BEHIND · ON TOP", col(1), Y_HANDS + 6.4, 1.4, FONT_BOLD, SOFT))
add(text("MACHINE · DRUNK", col(0), Y_HANDS + 6.4, 1.4, FONT_BOLD, SOFT))
for i, n in enumerate(KITL): add(text(n, col(2 + i), Y_HANDS - 6.2, CONTROL))
add(text("LEVELS", (col(2) + col(6)) / 2, Y_HANDS + 6.4, 1.6, FONT_BOLD, SOFT))
LIGHTS = ["HIT", "LINK", "FILL"]
for i, n in enumerate(LIGHTS): add(text(n, col(7 + i), Y_HANDS - 3.0, CONTROL))
add(text("COMPER", col(8), Y_HANDS + 4.4, 1.6, FONT_BOLD, SOFT))

# KIT sound
SOUND = ["TUNE", "DECAY", "BRIGHT", "WIDTH", "AGE", "LEVEL"]
add(f'<line x1="3.2" y1="{71.2 - 0.8:.2f}" x2="{W - 3.2:.2f}" y2="{71.2 - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')
for i, n in enumerate(SOUND): add(text(n, scol(i), Y_SOUND - 6.2, CONTROL))
add(text("DRUMS", scol(0), Y_SOUND + 6.6, 1.5, FONT_BOLD, SOFT))
add(text("CYMBALS", scol(1), Y_SOUND + 6.6, 1.5, FONT_BOLD, SOFT))
add(text("MONO · WIDE", scol(3), Y_SOUND + 6.6, 1.5, FONT_BOLD, SOFT))
add(text("OLD KIT", scol(4), Y_SOUND + 6.6, 1.5, FONT_BOLD, SOFT))

# jacks
add(f'<rect x="2.0" y="{Y_IN1 - 9.2}" width="{W - 4.0}" height="{Y_IN2 - Y_IN1 + 13.8}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "ENERGY", "COMP", "FILLS", "FILL", "LAY OUT"]
OUT = ["LEFT", "RIGHT", "KICK", "SNARE", "HAT", "RIDE", "CRASH", "TOM", "VEL", "ENERGY"]
def jlabel(n, x, y, fill):
    add(text(n, x, y - 5.2, JACK if len(n) < 6 else SMALL, FONT_BOLD, fill))
add(f'<rect x="3.2" y="{Y_IN2 - 8.6:.2f}" width="{W - 6.4:.2f}" height="12.6" rx="1.4" fill="{ACCENT}"/>')
for i, n in enumerate(IN1): jlabel(n, icol(i), Y_IN1, SOFT if 2 <= i <= 4 else INK)
for i, n in enumerate(OUT): jlabel(n, col(i), Y_IN2, BG_DARK)

def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for i, n in enumerate(PLAYER): c(col(i), Y_PLAYER, 3.0 if n == "NEW" else 6.4 if n == "ENERGY" else 4.6, n)
    c(col(0), Y_HANDS, 4.6, "LOOSE"); c(col(1), Y_HANDS, 4.6, "AHEAD")
    for i in range(5): c(col(2 + i), Y_HANDS, 4.6, KITL[i])
    for i in range(3): c(col(7 + i), Y_HANDS, 1.4, "light")
    for i, n in enumerate(SOUND): c(scol(i), Y_SOUND, 4.6, n)
    for i in range(7): c(icol(i), Y_IN1, 4.0, "in")
    for i in range(10): c(col(i), Y_IN2, 4.0, "out")
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    return b

allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in allb:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Rider.svg"), "w").write(svg)
print("ok")

import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Rider.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
