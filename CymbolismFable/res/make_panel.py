#!/usr/bin/env python3
# Draws res/Cymbolism.svg (40 HP). Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Cymbolism.cpp.
#   pip install fonttools ; python3 res/make_panel.py [lite]
# "lite" draws res/CymbolismLite.svg: the same panel with the Lite title and ranges.
import os
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 203.2, 128.5  # 40 HP
LITE = len(sys.argv) > 1 and sys.argv[1] == "lite"
OUT_NAME = "CymbolismLite.svg" if LITE else "Cymbolism.svg"

BG = "#2A2418"        # dark patinated bronze
BG_ROW = "#221D13"
BG_IO = "#1C1810"
LINE = "#4E4330"
GOLD = "#E2C27A"      # warm gold ink
CREAM = "#F1E7CF"
BRONZE = "#B98E52"    # lighter bronze for headers / play
PATINA = "#7FA58F"    # verdigris accent
SOFT = "#B7A888"

# ---- LAYOUT (same numbers in src/Cymbolism.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.0, 98.0, 42.0
TYPE_X, TYPE_Y = 116.0, 27.0
NEW_X, NEW_Y = 116.0, 48.0
POS_X, FORCE_X, BIG_Y = 142.0, 168.0, 27.0
STRIKE_X, STRIKE_Y, CHOKE_Y = 192.0, 23.0, 43.0
ROWA_Y, ROWA_X0, ROWA_DX = 69.0, 13.0, 22.0
ROWB_Y, ROWB_X0, ROWB_DX = 89.0, 14.0, 19.6
JACK1_Y, JACK2_Y, JACK_X0, JACK_DX = 106.5, 119.0, 10.0, 16.6

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4     # range marks

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=CREAM, anchor="middle", spacing=0.04):
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
# lathe rings: faint concentric arcs, like the tonal grooves on a cymbal
for i in range(26):
    r = 40 + i * 5.5
    add(f'<circle cx="{W + 10}" cy="-30" r="{r:.1f}" fill="none" stroke="#000" stroke-opacity="0.07" stroke-width="0.6"/>')

# ---- title + screen
add(text("Cymbolism Lite" if LITE else "Cymbolism Fable", 5.0, 10.6, 5.4, FONT_BOLD, GOLD, anchor="start", spacing=0.02))
add(text("a physical cymbal on a CPU budget" if LITE else "a physical cymbal · nothing sampled", W - 5.0, 10.4, 2.7, FONT_MED, BRONZE, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.2" fill="#0D0B07" stroke="{GOLD}" stroke-opacity="0.5" stroke-width="0.35"/>')


def ranges(x, y, r, lo, hi, col=SOFT):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


# ---- top right: type, new cymbal, the two big playing knobs, strike and choke
add(text("TYPE", TYPE_X, TYPE_Y - 9.4, CONTROL, fill=GOLD))
add(text("NEW", NEW_X, NEW_Y - 5.0, CONTROL, fill=GOLD))
add(text("CYMBAL", NEW_X, NEW_Y + 7.0, SMALL, fill=SOFT))
add(text("POSITION", POS_X, BIG_Y - 9.4, CONTROL, fill=BRONZE))
ranges(POS_X, BIG_Y + 10.0, 2.0, "BELL", "EDGE")
add(text("FORCE", FORCE_X, BIG_Y - 9.4, CONTROL, fill=BRONZE))
ranges(FORCE_X, BIG_Y + 10.0, 2.0, "SOFT", "HARD")
add(text("STRIKE", STRIKE_X, STRIKE_Y - 5.4, CONTROL, fill=BRONZE))
add(text("CHOKE", STRIKE_X, CHOKE_Y - 5.4, CONTROL, fill=BRONZE))
add(text("hold", STRIKE_X, CHOKE_Y + 7.4, SMALL, fill=SOFT))

# ---- row A: the cymbal (geometry; changing these rebuilds the modes)
add(f'<rect x="2.6" y="{ROWA_Y - 11.4}" width="{W - 5.2}" height="20.6" rx="1.4" fill="{BG_ROW}"/>')
add(f'<rect x="2.6" y="{ROWA_Y - 11.4}" width="{W - 5.2}" height="0.5" fill="{GOLD}" fill-opacity="0.6"/>')
namesA = ["SIZE", "WEIGHT", "TAPER", "PROFILE", "BELL", "FLANGE", "ALLOY", "HAMMER", "TENSION"]
marksA = [("6\"", "24\""), ("THIN", "HEAVY"), None, ("FLAT", "HIGH"), None,
          ("DOWN", "UP"), None, ("SPUN", "HAND"), ("-", "+")]
if LITE:
    marksA[0] = ("0.6x", "1.2x")
for i, n in enumerate(namesA):
    x = ROWA_X0 + ROWA_DX * i
    add(text(n, x, ROWA_Y - 7.0, CONTROL, fill=GOLD))
    if marksA[i]:
        ranges(x, ROWA_Y + 7.6, 2.2, *marksA[i])

# ---- row B: sound (no rebuild) and playing
add(f'<rect x="2.6" y="{ROWB_Y - 10.4}" width="{W - 5.2}" height="19.8" rx="1.4" fill="{BG_ROW}"/>')
namesB = ["DECAY", "BLOOM", "DAMP", "MOUNT", "SIZZLE", "MIC", "PLAY", "HARDNESS", "RATE", "EXC"]
marksB = [("SHORT", "LONG"), ("0", "2"), None, ("TIGHT", "LOOSE"), ("0", "12"), ("BELL", "ROOM"),
          None, ("SOFT", "HARD"), ("2", "30"), ("0", "1")]
if LITE:
    marksB[4] = ("0", "6")
for i, n in enumerate(namesB):
    x = ROWB_X0 + ROWB_DX * i
    col = BRONZE if i >= 6 else CREAM
    add(text(n, x, ROWB_Y - 7.0, CONTROL if len(n) < 8 else 2.9, fill=col))
    if marksB[i]:
        ranges(x, ROWB_Y + 7.6, 2.2, *marksB[i])
add(f'<rect x="{ROWB_X0 + ROWB_DX * 5.5 - 0.2}" y="{ROWB_Y - 9.4}" width="0.4" height="17.8" fill="{LINE}"/>')

# ---- jacks
add(f'<rect x="2.6" y="{JACK1_Y - 7.8}" width="{W - 5.2}" height="25.8" rx="1.4" fill="{BG_IO}"/>')
jack1 = ["STRIKE", "VEL", "CHOKE", "ROLL", "EXCITE", "PLAY", "SIZE", "WEIGHT", "TAPER", "PROFILE", "BELL", "HAMMER"]
jack2 = ["TENSION", "DECAY", "BLOOM", "DAMP", "MOUNT", "SIZZLE", "MIC", "POS", "FORCE", "HARD", "RATE"]
for i, n in enumerate(jack1):
    col = BRONZE if i < 5 else (GOLD if i >= 6 else BRONZE)
    add(text(n, JACK_X0 + JACK_DX * i, JACK1_Y - 5.0, JACK if len(n) < 7 else 2.6, FONT_BOLD, col))
for i, n in enumerate(jack2):
    col = GOLD if i == 0 else (CREAM if i < 7 else BRONZE)
    add(text(n, JACK_X0 + JACK_DX * i, JACK2_Y - 5.0, JACK if len(n) < 7 else 2.6, FONT_BOLD, col))
# OUT
ox = JACK_X0 + JACK_DX * 11
add(f'<rect x="{ox - 6.6}" y="{JACK2_Y - 7.6}" width="13.2" height="12.4" rx="1.4" fill="{GOLD}"/>')
add(text("OUT", ox, JACK2_Y - 5.0, JACK, FONT_BOLD, BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, OUT_NAME), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, OUT_NAME), x=W / 2, y=126.0, cap=3.0, bg=BG)
