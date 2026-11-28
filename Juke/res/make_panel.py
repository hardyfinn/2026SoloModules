#!/usr/bin/env python3
# Draws res/Juke.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Juke.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 162.56, 128.5  # 32 HP

BG = "#17141C"
BG_DARK = "#0E0C12"
LINE = "#34303D"
INK = "#F1ECF5"
SOFT = "#A9A2B5"
PINK = "#FF4F9A"
CYAN = "#59C3E8"

# ---- LAYOUT (same numbers in src/Juke.cpp) ----
KNOB_Y, CV_Y = 19.5, 30.0
KX = [10.5, 24.0, 39.5, 54.0, 67.5, 81.5]   # TEMPO PRESET DENSITY VARY SWING LEVEL
BX = [95.0, 108.0, 121.0]                   # GEN MUTATE FILL
FEEL_X, EVOLVE_X = 134.0, 154.0
ROW_Y0, ROW_DY = 38.5, 9.0
LABEL_X, MUTE_X = 3.6, 20.5
GRID_X0, CELL, GAP = 26.0, 6.0, 1.5
TRIG_X, AUDIO_X = 134.5, 148.5
IO_Y, IO_LABEL_Y = 123.0, 117.4
IOX = [15.0, 27.5, 40.0, 52.5, 65.0, 78.0, 90.0]  # RUN CLOCK RESET CLK ACCENT MIX L MIX R
DISP_X, DISP_Y, DISP_W, DISP_H = 99.0, 115.6, 29.0, 11.4

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3

GRID_W = 16 * CELL + 3 * GAP
fonts = {}


def measure(s, size, font=FONT_BOLD, spacing=0.04):
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    names = [cmap[ord(c)] for c in s]
    return (sum(gs[n].width + spacing * upm for n in names) - spacing * upm) * size / upm


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    measure(s, size, font, spacing)
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
add(f'<rect x="0" y="0" width="{W}" height="1.3" fill="{PINK}"/>')

# ---- title
add(text("Juke", 13.0, 9.0, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("footwork drum machine", 148.0, 8.6, 2.6, FONT_MED, PINK, anchor="end"))

# ---- top controls
for x, n in zip(KX, ["TEMPO", "PRESET", "DENSITY", "VARY", "SWING", "LEVEL"]):
    add(text(n, x, KNOB_Y - 6.6, CONTROL, fill=PINK if n in ("PRESET",) else INK))
for x, n in zip(BX, ["GEN", "MUTATE", "FILL"]):
    add(text(n, x, KNOB_Y - 6.6, CONTROL, fill=CYAN if n != "FILL" else PINK))
add(text("FEEL", FEEL_X, KNOB_Y - 6.6, CONTROL))
for dy, o in zip([-3.2, 0.9, 5.0], ["HALF", "AUTO", "DBL"]):
    add(text(o, FEEL_X + 2.9, KNOB_Y + dy, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("EVOLVE", EVOLVE_X, KNOB_Y - 6.6, CONTROL, fill=CYAN))
add(text("ON", EVOLVE_X + 2.9, KNOB_Y - 2.3, SMALL, FONT_BOLD, SOFT, anchor="start"))
# little ticks from knob to its CV jack
for x in KX[1:5] + BX:
    add(f'<rect x="{x - 0.15:.2f}" y="{KNOB_Y + 5.4:.2f}" width="0.3" height="0.9" fill="{SOFT}"/>')

# ---- grid band
gy0 = ROW_Y0 - ROW_DY / 2
add(f'<rect x="2.0" y="{gy0 - 0.6:.2f}" width="{W - 4.0:.2f}" height="{9 * ROW_DY + 1.2:.2f}" rx="1.4" fill="{BG_DARK}"/>')
rows = ["KICK", "CLAP", "SNARE", "C HAT", "O HAT", "TOM HI", "TOM LO", "BELL", "CHOP"]
for i, n in enumerate(rows):
    y = ROW_Y0 + i * ROW_DY
    add(text(n, LABEL_X, y + 1.05, JACK, anchor="start"))
    if i:
        add(f'<rect x="3.0" y="{y - ROW_DY / 2 - 0.08:.2f}" width="{W - 6.0:.2f}" height="0.16" fill="{LINE}"/>')
# beat numbers are drawn by the grid widget; outputs plate on the right
add(f'<rect x="{TRIG_X - 6.0:.2f}" y="{gy0 - 0.6:.2f}" width="{AUDIO_X - TRIG_X + 12.0:.2f}" height="{9 * ROW_DY + 1.2:.2f}" rx="1.4" fill="#231E2B"/>')

# ---- bottom row
add(f'<rect x="{IOX[3] - 6.0:.2f}" y="{IO_LABEL_Y - 3.6:.2f}" width="{IOX[6] - IOX[3] + 12.0:.2f}" height="{H - IO_LABEL_Y + 1.6:.2f}" rx="1.2" fill="#231E2B"/>')
for x, n in zip(IOX, ["RUN", "CLOCK", "RESET", "CLK", "ACCENT", "MIX L", "MIX R"]):
    add(text(n, x, IO_LABEL_Y, JACK, fill=PINK if n.startswith("MIX") else INK))
add(f'<rect x="{DISP_X - 0.5}" y="{DISP_Y - 0.5}" width="{DISP_W + 1.0}" height="{DISP_H + 1.0}" rx="0.9" '
    f'fill="#05040A" stroke="{PINK}" stroke-opacity="0.5" stroke-width="0.3"/>')
add(text("TRIG", TRIG_X, IO_LABEL_Y, JACK, fill=CYAN))
add(text("AUDIO", AUDIO_X, IO_LABEL_Y, JACK, fill=PINK))

# label collision check (top row)
labels = list(zip(KX + BX, ["TEMPO", "PRESET", "DENSITY", "VARY", "SWING", "LEVEL", "GEN", "MUTATE", "FILL"]))
for (x1, a), (x2, b) in zip(labels, labels[1:]):
    gap = (x2 - x1) - (measure(a, CONTROL) + measure(b, CONTROL)) / 2
    if gap < 0.8:
        print(f"warning: {a}/{b} labels only {gap:.2f} mm apart", file=sys.stderr)

svgtxt = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
          + "\n".join(parts) + "\n</svg>\n")
out = os.path.join(HERE, "Juke.svg")
open(out, "w").write(svgtxt)

sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=141.5, y=124.4, cap=2.8, bg=BG)
print("wrote", out)
