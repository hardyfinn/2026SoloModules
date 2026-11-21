#!/usr/bin/env python3
# Draws res/HouseKeys.svg and writes src/PanelLayout.hpp (the positions the module uses).
# Rack can't draw SVG <text>, so labels become paths. Positions are in mm.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 132.08, 128.5  # 26 HP

BG = "#ECE9E1"      # light warm grey, like Hardy's other light panels
BG_BLOCK = "#E2DED4"
LINE = "#B9B3A6"
INK = "#1B1B1B"
SOFT = "#5E5A52"
ACCENT = "#B8432F"  # brick red for headings
DARK = "#24221F"
CREAM = "#F1EBDD"

# ---- LAYOUT (exported to src/PanelLayout.hpp) ----
LAYOUT = dict(
    DISP_X=5.0, DISP_Y=15.0, DISP_W=56.0, DISP_H=14.0,
    SOUND_X=33.0, SOUND_Y=47.0,
    PREV_X=11.0, NEXT_X=55.0, PN_Y=47.0,
    ENG_X0=9.0, ENG_DX=11.5, ENG_Y=72.0,
    COL_PLAY=74.0, COL_SHAPE=91.0, COL_M1=108.0, COL_M2=123.0,
    ROW1=26.0, ROW2=46.0, ROW3=66.0, ROW4=86.0,
    JACK_X0=9.0, JACK_DX=12.6, JACK_Y=116.0,
)
globals().update(LAYOUT)

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4     # range marks

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

# ---- title
add(text("HOUSE KEYS", 5.0, 10.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.03))
add(text("100 keyboard sounds · five machines", W - 5.0, 10.2, 2.5, FONT_MED, SOFT, anchor="end"))

# ---- SOUND block
add(f'<rect x="2.5" y="12.8" width="61.0" height="80.6" rx="1.4" fill="{BG_BLOCK}"/>')
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#12110F" stroke="{LINE}" stroke-width="0.35"/>')
add(text("SOUND", SOUND_X, SOUND_Y - 12.0, CONTROL, fill=ACCENT))
add(text("1", SOUND_X - 9.6, SOUND_Y + 10.2, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("100", SOUND_X + 9.6, SOUND_Y + 10.2, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("PREV", PREV_X, PN_Y - 5.6, CONTROL))
add(text("NEXT", NEXT_X, PN_Y - 5.6, CONTROL))
for i, n in enumerate(["FM", "POLY", "PD", "WAVE", "SMPL"]):
    add(text(n, ENG_X0 + ENG_DX * i, ENG_Y - 6.0, CONTROL))
add(text("jump to the first sound of an engine", 33.0, ENG_Y + 8.6, SMALL, FONT_MED, SOFT))
add(text("SOUND CV: 0 to 10 V spans 1 to 100", 33.0, ENG_Y + 13.2, SMALL, FONT_MED, SOFT))
add(text("OCTAVE adds to the sound's own", 33.0, ENG_Y + 17.8, SMALL, FONT_MED, SOFT))

# ---- PLAY / SHAPE / MACHINE headings
def heading(s, x0, x1):
    add(text(s, (x0 + x1) / 2, 16.2, 2.6, FONT_BOLD, ACCENT, spacing=0.12))
    add(f'<line x1="{x0}" y1="17.6" x2="{x1}" y2="17.6" stroke="{LINE}" stroke-width="0.35"/>')

heading("PLAY", COL_PLAY - 7.0, COL_PLAY + 7.0)
heading("SHAPE", COL_SHAPE - 7.0, COL_SHAPE + 7.0)
heading("MACHINE", COL_M1 - 7.0, COL_M2 + 7.0)


def knob_label(name, x, y, lo=None, hi=None, r=6.6):
    add(text(name, x, y - 7.6, CONTROL))
    if lo is not None:
        add(text(lo, x - r + 1.2, y + 7.8, SMALL, FONT_BOLD, SOFT, anchor="end"))
        add(text(hi, x + r - 1.2, y + 7.8, SMALL, FONT_BOLD, SOFT, anchor="start"))


knob_label("OCTAVE", COL_PLAY, ROW1, "−2", "+2")
knob_label("STAB", COL_PLAY, ROW2, "OFF", "MIN9")
knob_label("VELOCITY", COL_PLAY, ROW3, "0", "200%")
knob_label("MOD", COL_PLAY, ROW4, "0", "1")
knob_label("TONE", COL_SHAPE, ROW1, "−", "+")
knob_label("ATTACK", COL_SHAPE, ROW2, "÷8", "×8")
knob_label("DECAY", COL_SHAPE, ROW3, "÷8", "×8")
knob_label("TWIN", COL_M1, ROW1, "OFF", "1")
knob_label("CHORUS", COL_M2, ROW1)
add(text("SND", COL_M2 - 4.6, ROW1 + 7.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("OFF", COL_M2 - 4.0, ROW1 - 3.6, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("I", COL_M2 + 4.0, ROW1 - 3.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("II", COL_M2 + 4.6, ROW1 + 7.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
knob_label("GRIT", COL_M1, ROW2, "CLEAN", "WORN")
knob_label("TAPE", COL_M2, ROW2, "OFF", "1")
knob_label("LEVEL", (COL_M1 + COL_M2) / 2, ROW3)
add(text("STAB: OFF 5TH OCT MIN MAJ", COL_SHAPE + 0.0, ROW4 - 1.2, SMALL, FONT_MED, SOFT, anchor="middle"))
add(text("MIN7 MAJ7 DOM7 SUS4 MIN9", COL_SHAPE + 0.0, ROW4 + 2.6, SMALL, FONT_MED, SOFT, anchor="middle"))
add(text("GRIT 0.5 is the real machine", (COL_M1 + COL_M2) / 2 + 4.0, ROW4 + 6.4, SMALL, FONT_MED, SOFT))

# ---- jacks
add(f'<line x1="3.0" y1="104.4" x2="{W - 3.0}" y2="104.4" stroke="{LINE}" stroke-width="0.35"/>')
ins = ["V/OCT", "GATE", "VEL", "SOUND", "TONE", "DECAY", "MOD", "TWIN"]
for i, n in enumerate(ins):
    add(text(n, JACK_X0 + JACK_DX * i, JACK_Y - 5.6, JACK, fill=INK if i < 3 else SOFT))
add(text("CV", JACK_X0 + JACK_DX * 5.5, JACK_Y - 9.6, SMALL, FONT_BOLD, SOFT))
ox = JACK_X0 + JACK_DX * 8
add(f'<rect x="{ox - 6.2}" y="{JACK_Y - 9.0}" width="{JACK_DX + 12.4}" height="14.4" rx="1.4" fill="{DARK}"/>')
for i, n in enumerate(["L", "R"]):
    add(text(n, ox + JACK_DX * i, JACK_Y - 5.6, JACK, fill=CREAM))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "HouseKeys.svg"), "w").write(svg)

# layout header for the module
with open(os.path.join(HERE, "..", "src", "PanelLayout.hpp"), "w") as f:
    f.write("// Generated by res/make_panel.py. Positions in mm.\n#pragma once\n\n")
    for k, v in LAYOUT.items():
        f.write(f"static constexpr float {k} = {v}f;\n")
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "HouseKeys.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
