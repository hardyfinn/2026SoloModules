#!/usr/bin/env python3
# Draws res/Template.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Template.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "Crate"
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

HP = 16                      # widen HP rather than cram
W, H = HP * 5.08, 128.5

BG = "#2A1F2D"
BG_DARK = "#1C1420"
INK = "#F1E9D6"
SOFT = "#BFB0C6"
ACCENT = "#FFC145"

# ---- LAYOUT (same numbers in src/Crate.cpp) ----
CX = W / 2
COL = [10.0, 25.3, 40.64, 56.0, 71.3]
ROW1_Y, ROW2_Y, ROW3_Y = 28.0, 50.0, 72.0
ROW3_X = [8.5, 21.4, 34.3, 47.2, 60.1, 73.0]
JACK_A_Y, JACK_B_Y = 96.0, 113.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)

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
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("CRATE", CX, 11.0, 4.6, FONT_BOLD, INK))
add(text("12 BIT  26 KHZ", CX, 15.2, 2.4, FONT_MED, SOFT))

# row 1
for x, lab, big in zip(COL, ["TUNE", "DECAY", "START", "END", "LEVEL"], [1, 1, 0, 0, 0]):
    add(text(lab, x, ROW1_Y - (9.0 if big else 7.5), CONTROL))
# row 2
add(text("GAIN", COL[0], ROW2_Y - 7.5, CONTROL))
add(text("CLIP", COL[0] + 6.6, ROW2_Y + 8.6, 2.2, FONT_MED, SOFT))
add(text("SPEED", COL[1], ROW2_Y - 7.5, CONTROL))
for dy, lab in [(-3.3, "78"), (0.0, "45"), (3.3, "33")]:
    add(text(lab, COL[1] + 2.2, ROW2_Y + dy + 1.0, JACK, FONT_MED, SOFT, anchor="start"))
add(text("CHANNEL", COL[2], ROW2_Y - 10.0, CONTROL))
for ang, lab in [(-135, "1·2"), (-45, "3·4"), (45, "5·6"), (135, "7·8")]:
    import math
    r = 7.6
    x = COL[2] + r * math.sin(math.radians(ang))
    y = ROW2_Y - r * math.cos(math.radians(ang)) + 1.0
    add(text(lab, x, y, JACK, FONT_MED, SOFT))
add(text("AGE", COL[3], ROW2_Y - 7.5, CONTROL, fill=ACCENT))
add(text("MUTATE", COL[4], ROW2_Y - 7.5, CONTROL))
# row 3
for x, lab in zip(ROW3_X, ["REC", "THRU", "PLAY", "LOOP", "MUT", "LOCK"]):
    add(text(lab, x, ROW3_Y - 6.5, CONTROL))

# jacks
add(f'<rect x="2.5" y="{JACK_A_Y - 9.5}" width="{W - 5.0}" height="{JACK_B_Y - JACK_A_Y + 15.0}" rx="1.4" fill="{BG_DARK}"/>')
for x, lab in zip(COL, ["TRIG", "V/OCT", "VEL", "DECAY", "START"]):
    add(text(lab, x, JACK_A_Y - 5.6, JACK))
for x, lab in zip(COL[:3], ["IN", "REC", "MUT"]):
    add(text(lab, x, JACK_B_Y - 5.6, JACK))
for x, lab in zip(COL[3:], ["END", "OUT"]):
    add(f'<rect x="{x - 6.5}" y="{JACK_B_Y - 9.0}" width="13.0" height="14.0" rx="1.4" fill="{ACCENT}"/>')
    add(text(lab, x, JACK_B_Y - 5.4, JACK, fill=BG))

out = os.path.join(HERE, f"{NAME}.svg")
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(out, "w").write(svg)

# Haer maker logo on the bottom rail (required on every panel)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=CX, y=126.0, cap=3.0, bg=BG)
print("ok", out)
