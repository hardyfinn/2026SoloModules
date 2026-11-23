#!/usr/bin/env python3
# Draws res/Datamatic.svg (black) and res/TestPattern.svg (white).
# Rack can't draw SVG <text>, so labels become paths. Positions are in mm;
# the LAYOUT numbers are shared with src/Datamatic.cpp and src/TestPattern.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 101.6, 128.5  # 20 HP each

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5     # range marks

# ---- LAYOUT: Datamatic
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 15.0, 89.6, 22.0
KX = [14.0, 38.5, 63.0, 87.5]
ROW1_Y = 49.0
ROW2_Y = 71.0
JX0, JDX = 9.3, 16.6
JA_Y, JB_Y, JC_Y = 88.0, 103.0, 118.0
# ---- LAYOUT: Test Pattern
VX = [13.4, 38.4, 63.4, 88.4]
HEAD_Y = 19.0
ZONE_Y = 24.8
K1_Y, K2_Y, K3_Y, LEV_Y = 38.5, 53.5, 68.5, 81.5
GATE_Y, OUT_Y, IO_Y = 94.0, 107.0, 120.0

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill="#000", anchor="middle", spacing=0.04):
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


def save(name, parts, bg):
    path = os.path.join(HERE, name + ".svg")
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(p for p in parts if p) + "\n</svg>\n")
    open(path, "w").write(svg)
    # Haer maker logo (res/haer_logo.py) on the bottom rail
    sys.path.insert(0, HERE)
    import haer_logo
    haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=bg)


def barcode(add, x0, y0, w, h, bits, ink):
    # a strip of data bars, Ikeda style: one bar per bit
    n = len(bits)
    cw = w / n
    for i, b in enumerate(bits):
        if b == "1":
            add(f'<rect x="{x0 + i * cw:.3f}" y="{y0}" width="{cw:.3f}" height="{h}" fill="{ink}"/>')


# ======================================================================= Datamatic
BG, INK, SOFT, DARK = "#0B0B0B", "#F2F2F2", "#9A9A9A", "#1E1E1E"
parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Datamatic", 6.0, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("data → rhythm", W - 6.0, 11.0, 2.6, FONT_MED, SOFT, anchor="end"))
# pi in BCD, 3 1 4 1 5 9 2 6
pi_bits = "".join(format(int(d), "04b") for d in "31415926")
barcode(add, 44.0, 7.6, 22.0, 3.6, pi_bits, "#5A5A5A")
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="0.6" fill="#000000" stroke="{INK}" stroke-opacity="0.5" stroke-width="0.3"/>')
# lane names beside the screen rows
lane_top = DISP_Y + 1.2 + 4.2
row_h = (DISP_H - 1.2 - 4.2 - 1.2) / 4
for i, n in enumerate(["S", "T", "C", "N"]):
    add(text(n, DISP_X - 1.6, lane_top + row_h * (i + 0.5) + 0.8, 2.2, FONT_BOLD, SOFT))


def ranges(x, y, r, lo, hi):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


for x, n in zip(KX, ["SOURCE", "WORD", "LENGTH", "RATE"]):
    add(text(n, x, ROW1_Y - 9.4, CONTROL, fill=INK))
ranges(KX[1], ROW1_Y + 8.8, 4.6, "0", "255")
ranges(KX[2], ROW1_Y + 8.8, 4.6, "4", "64")
ranges(KX[3], ROW1_Y + 8.8, 4.6, "1x", "16x")
for x, n in zip(KX, ["MUTATE", "ROLL", "CUT", "BPM"]):
    add(text(n, x, ROW2_Y - 7.8, CONTROL, fill=INK))

# jack area
add(f'<rect x="2.4" y="79.9" width="{W - 4.8}" height="28.3" rx="1.0" fill="{DARK}"/>')
for i, n in enumerate(["SOURCE", "WORD", "LENGTH", "RATE", "MUTATE", "ROLL"]):
    add(text(n, JX0 + JDX * i, JA_Y - 5.2, JACK, FONT_BOLD, SOFT))
for i, n in enumerate(["CUT", "CLOCK", "RESET", "DATA"]):
    add(text(n, JX0 + JDX * i, JB_Y - 5.2, JACK, FONT_BOLD, INK if n in ("CLOCK", "RESET") else SOFT))
# outputs: white blocks with black type
ox = JX0 + JDX * 4
add(f'<rect x="{ox - 7.6}" y="{JB_Y - 8.6}" width="{JDX + 15.2}" height="13.2" rx="1.0" fill="{INK}"/>')
for i, n in enumerate(["CUT", "BIT"]):
    add(text(n, ox + JDX * i, JB_Y - 5.2, JACK, FONT_BOLD, BG))
add(f'<rect x="2.4" y="{JC_Y - 8.6}" width="{W - 4.8}" height="13.2" rx="1.0" fill="{INK}"/>')
for i, n in enumerate(["SUB", "TONE", "CLICK", "NOISE", "DATA", "LINK"]):
    add(text(n, JX0 + JDX * i, JC_Y - 5.2, JACK, FONT_BOLD, BG))
save("Datamatic", parts, BG)

# ======================================================================= Test Pattern
BG, INK, SOFT, DARK = "#EDEDED", "#0B0B0B", "#5C5C5C", "#D9D9D9"
parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Test Pattern", 6.0, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("sine · click · noise", W - 6.0, 11.0, 2.6, FONT_MED, SOFT, anchor="end"))
# thin column rules
for x in (25.9, 50.9, 75.9):
    add(f'<line x1="{x}" y1="15.5" x2="{x}" y2="85.5" stroke="{INK}" stroke-opacity="0.25" stroke-width="0.25"/>')

cols = [
    ("SUB", ["PITCH", "SHAPE", "LEN"]),
    ("TONE", ["PITCH", "BEAT", "SCAN"]),
    ("CLICK", ["WIDTH", "COLOUR", "SPREAD"]),
    ("NOISE", ["LEN", "COLOUR", "SPREAD"]),
]
for x, (head, knobs) in zip(VX, cols):
    add(text(head, x, HEAD_Y, 4.0, FONT_BOLD, INK, spacing=0.06))
    for y, n in zip((K1_Y, K2_Y, K3_Y), knobs):
        add(text(n, x, y - 6.9, CONTROL, FONT_BOLD, INK))
    add(text("LEVEL", x, LEV_Y - 5.6, CONTROL, FONT_BOLD, INK))
# TONE zone switch marks
add(text("LO", VX[1] - 6.0, ZONE_Y + 0.9, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("HI", VX[1] + 6.0, ZONE_Y + 0.9, SMALL, FONT_BOLD, SOFT, anchor="start"))
# small range marks
def r2(x, y, lo, hi):
    add(text(lo, x - 5.0, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + 5.0, y, SMALL, FONT_BOLD, SOFT, anchor="start"))
r2(VX[0], K1_Y + 4.6, "25", "160")
r2(VX[0], K3_Y + 4.6, "2ms", "GATE")
r2(VX[1], K2_Y + 4.6, "0", "20Hz")
r2(VX[2], K1_Y + 4.6, "1", "32")
r2(VX[3], K1_Y + 4.6, "1ms", "300")

# jack area
add(f'<rect x="2.4" y="86.8" width="{W - 4.8}" height="24.6" rx="1.0" fill="{DARK}"/>')
for x in VX:
    add(text("GATE", x, GATE_Y - 5.2, JACK, FONT_BOLD, SOFT))
for x in VX:
    add(f'<rect x="{x - 7.0}" y="{OUT_Y - 8.4}" width="14.0" height="12.8" rx="1.0" fill="{INK}"/>')
    add(text("OUT", x, OUT_Y - 5.2, JACK, FONT_BOLD, BG))
for i, n in enumerate(["LINK", "SUB V/O", "TONE V/O", "CUT"]):
    add(text(n, JX0 + JDX * i, IO_Y - 5.2, JACK, FONT_BOLD, INK if n == "LINK" else SOFT))
ox = JX0 + JDX * 4
add(f'<rect x="{ox - 7.6}" y="{IO_Y - 8.6}" width="{JDX + 15.2}" height="13.0" rx="1.0" fill="{INK}"/>')
for i, n in enumerate(["L", "R"]):
    add(text(n, ox + JDX * i, IO_Y - 5.2, JACK, FONT_BOLD, BG))
save("TestPattern", parts, BG)
print("ok")
