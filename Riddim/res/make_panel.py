#!/usr/bin/env python3
# Draws res/Riddim.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Riddim.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 152.4, 128.5  # 30 HP

BG = "#16171A"
BG_DARK = "#0E0F11"
LINE = "#34363B"
INK = "#F3EEDC"
SOFT = "#B4B0A0"
GOLD = "#F2B731"
GREEN = "#2FA253"
RED = "#D8362B"

# ---- LAYOUT (same numbers in src/Riddim.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 5.0, 14.0, 142.4, 21.0
COLX = [13.0, 37.5, 62.0, 86.5, 111.0, 135.5]
ROW1_Y = 51.0
ROW2_Y = 69.5
VX = [17.0, 47.0, 77.0, 107.0, 137.0]
VKNOB_DX, VJACK_DX = -6.5, 6.5
VA_Y, VB_Y = 88.6, 105.0
IO_Y = 122.6
IO_X0, IO_DX = 17.0, 19.6

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5

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
# rasta stripe along the very top
third = W / 3
add(f'<rect x="0" y="0" width="{third:.3f}" height="1.3" fill="{RED}"/>')
add(f'<rect x="{third:.3f}" y="0" width="{third:.3f}" height="1.3" fill="{GOLD}"/>')
add(f'<rect x="{2 * third:.3f}" y="0" width="{third:.3f}" height="1.3" fill="{GREEN}"/>')

# ---- title
add(text("Riddim", 6.0, 11.2, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("dancehall drum machine", 126.0, 10.8, 2.6, FONT_MED, GOLD, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#05060A" stroke="{GOLD}" stroke-opacity="0.55" stroke-width="0.35"/>')

# ---- row 1
names1 = ["RIDDIM", "KIT", "TUNE", "DECAY", "TONE", "LEVEL"]
for x, n in zip(COLX, names1):
    add(text(n, x, ROW1_Y - 9.4, CONTROL, fill=GOLD if n in ("RIDDIM", "LEVEL") else INK))
# ---- row 2
for x, n in zip(COLX[:4], ["SWING", "MUTATE", "CRUSH", "DRIVE"]):
    add(text(n, x, ROW2_Y - 8.0, CONTROL))
# VAR switch (3-way) and FILL
add(text("VAR", COLX[4] - 3.0, ROW2_Y - 9.0, CONTROL))
for dy, o in zip([-3.0, 1.0, 5.0], ["A", "AUTO", "B"]):
    add(text(o, COLX[4] - 3.0 + 2.9, ROW2_Y + dy, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("FILL", COLX[5], ROW2_Y - 8.0, CONTROL, fill=RED))

# ---- voices band
add(f'<rect x="2.4" y="77.2" width="{W - 4.8}" height="34.2" rx="1.4" fill="{BG_DARK}"/>')
vnames = ["KICK", "SNARE", "RIM", "CLAP", "TOM", "C HAT", "O HAT", "COW", "PERC", "SHAKE"]
for i, n in enumerate(vnames):
    cx = VX[i % 5]
    y = (VA_Y if i < 5 else VB_Y) - 6.0
    add(text(n, cx, y, CONTROL))

# ---- I/O row
for i, n in enumerate(["CLOCK", "RESET", "FILL", "VAR B", "RIDDIM"]):
    add(text(n, IO_X0 + IO_DX * i, IO_Y - 6.6, JACK))
ox0 = IO_X0 + IO_DX * 5
add(f'<rect x="{ox0 - 7.6}" y="{IO_Y - 10.6}" width="{IO_DX + 15.2}" height="{H - (IO_Y - 10.6) - 1.8}" rx="1.4" fill="{GOLD}"/>')
for i, n in enumerate(["MIX L", "MIX R"]):
    add(text(n, ox0 + IO_DX * i, IO_Y - 6.6, JACK, fill=BG))

svgtxt = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
          + "\n".join(parts) + "\n</svg>\n")
out = os.path.join(HERE, "Riddim.svg")
open(out, "w").write(svgtxt)

# Haer maker logo, in the title bar (the bottom rail is full of jacks)
import sys
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=137.0, y=7.0, cap=3.0, bg=BG)
print("wrote", out)
