#!/usr/bin/env python3
# Draws res/Greybox.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Greybox.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "Greybox"
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

HP = 22                       # widen HP rather than cram
W, H = HP * 5.08, 128.5

BG = "#C9C7BF"        # warm rack grey
BG_DARK = "#2C2C2F"
INK = "#1F1F22"
SOFT = "#55555B"
LIGHT_INK = "#ECE8DC"
ACCENT = "#C23B22"

# ---- LAYOUT (same numbers in src/Greybox.cpp) ----
CX = W / 2
COL = [10.0, 28.35, 46.7, 65.06, 83.41, 101.76]
ROW1_Y, ROW2_Y, ROW3_Y = 31.0, 54.0, 76.0
ROW3_X = [8.0, 19.5, 31.0, 45.0, 60.0, 75.0, 89.0, 102.0]
JACK_A_Y, JACK_B_Y = 97.0, 114.0

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
add(f'<rect x="0" y="0" width="{W}" height="2.2" fill="{BG_DARK}"/>')
add(text("GREYBOX", CX, 11.0, 4.6, FONT_BOLD, INK))
add(text("12 BIT SAMPLER  ·  900 / 950 / 60", CX, 15.4, 2.4, FONT_MED, SOFT))

# row 1
for x, lab, big in zip(COL, ["TUNE", "DECAY", "FILTER", "ENV", "START", "END"], [1, 1, 1, 0, 0, 0]):
    add(text(lab, x, ROW1_Y - (9.4 if big else 7.6), CONTROL))

# row 2
add(text("MODEL", COL[0], ROW2_Y - 7.6, CONTROL))
for dy, lab in [(-3.3, "60"), (0.0, "950"), (3.3, "900")]:
    add(text(lab, COL[0] + 0.6, ROW2_Y + dy + 1.0, JACK, FONT_MED, SOFT, anchor="start"))
add(text("GAIN", COL[1], ROW2_Y - 7.6, CONTROL))
add(text("CLIP", COL[1] + 7.0, ROW2_Y + 8.4, 2.2, FONT_MED, SOFT))
add(text("RATE", COL[2], ROW2_Y - 7.6, CONTROL))
add(text("STRETCH", COL[3], ROW2_Y - 7.6, CONTROL))
add(text("D-TIME", COL[4], ROW2_Y - 7.6, CONTROL))
add(text("LOOP", COL[5], ROW2_Y - 7.6, CONTROL))
for dy, lab in [(-3.3, "ALT"), (0.0, "FWD"), (3.3, "OFF")]:
    add(text(lab, COL[5] - 0.6, ROW2_Y + dy + 1.0, JACK, FONT_MED, SOFT, anchor="end"))

# row 3
for x, lab in zip(ROW3_X, ["REC", "THRU", "PLAY", "LEVEL", "AGE", "MUTATE", "MUT", "LOCK"]):
    big = lab in ("LEVEL", "AGE", "MUTATE")
    add(text(lab, x, ROW3_Y - (7.6 if big else 6.5), CONTROL, fill=ACCENT if lab == "AGE" else INK))
add(f'<rect x="3.0" y="{ROW2_Y + 11.2}" width="{W - 6.0}" height="0.35" fill="{SOFT}"/>')

# jacks
add(f'<rect x="2.5" y="{JACK_A_Y - 9.5}" width="{W - 5.0}" height="{JACK_B_Y - JACK_A_Y + 15.0}" rx="1.4" fill="{BG_DARK}"/>')
for x, lab in zip(COL, ["TRIG", "V/OCT", "VEL", "FILTER", "DECAY", "START"]):
    add(text(lab, x, JACK_A_Y - 5.6, JACK, fill=LIGHT_INK))
for x, lab in zip(COL[:3], ["IN", "REC", "MUT"]):
    add(text(lab, x, JACK_B_Y - 5.6, JACK, fill=LIGHT_INK))
for x, lab in zip(COL[4:], ["END", "OUT"]):
    add(f'<rect x="{x - 6.5}" y="{JACK_B_Y - 9.0}" width="13.0" height="14.0" rx="1.4" fill="{BG}"/>')
    add(text(lab, x, JACK_B_Y - 5.4, JACK, fill=INK))

out = os.path.join(HERE, f"{NAME}.svg")
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(out, "w").write(svg)

# Haer maker logo on the bottom rail (required on every panel)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=CX, y=125.6, cap=3.0, bg=BG)
print("ok", out)
