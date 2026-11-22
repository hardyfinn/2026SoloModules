#!/usr/bin/env python3
# Draws res/Scanner.svg and res/BoreRoom.svg. Rack can't draw SVG <text>, so
# labels become paths. Positions are in mm; the LAYOUT numbers are shared with
# src/Scanner.cpp and src/BoreRoom.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import haer_logo

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
H = 128.5

# clinical white with scanner blue
BG = "#ECEEF1"
BG_B = "#E1E5EA"
STRIP = "#D5DBE2"
LINE = "#B8C1CC"
INK = "#1B2430"
SOFT = "#5A6778"
BLUE = "#2D6BD3"
WHITE = "#FFFFFF"

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


def write(name, W, parts):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(p for p in parts if p) + "\n</svg>\n")
    path = os.path.join(HERE, name)
    open(path, "w").write(svg)
    haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=BG)


def bore_rings(add, cx, cy, r):
    # the scanner seen end-on: magnet, coil, bore
    add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{BLUE}" stroke-width="0.5"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r * 0.72}" fill="none" stroke="{BLUE}" stroke-width="0.35"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r * 0.46}" fill="{BLUE}"/>')


# =========================================================== SCANNER (36 HP)
W = 182.88
# ---- LAYOUT (same numbers in src/Scanner.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 15.0, 150.0, 19.0
COLX = [19.0, 49.0, 79.0, 109.0, 139.0]
ROW1_Y = 49.0
ROW2_Y = 70.0
ROW3_Y = 90.0
X3 = [12.0, 30.5, 49.0, 67.5, 86.0, 104.5, 125.0, 145.0]
JX0, JDX = 10.5, 17.5
IN_Y = 104.0
OUT_Y = 118.0
RX = 171.5
RY = [52.0, 66.0, 80.0]

P = []
add = P.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<rect x="161.0" y="0" width="{W - 161.0}" height="{H}" fill="{BG_B}"/>')
add(f'<line x1="161.0" y1="4" x2="161.0" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')
add(text("Scanner", 6.0, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("MRI pulse sequences · coils · bore", 156.0, 11.0, 2.6, FONT_MED, BLUE, anchor="end"))
bore_rings(add, 171.5, 22.0, 7.5)
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0B1220" stroke="{BLUE}" stroke-width="0.4"/>')


def ranges(x, y, r, lo, hi, col=SOFT):
    if lo: add(text(lo, x - r, y, SMALL, FONT_BOLD, col, anchor="end"))
    if hi: add(text(hi, x + r, y, SMALL, FONT_BOLD, col, anchor="start"))


for x, n in zip(COLX, ["SEQUENCE", "TR", "SPACING", "SLICES", "RAMP"]):
    add(text(n, x, ROW1_Y - 9.4, CONTROL, fill=BLUE if n == "SEQUENCE" else INK))
ranges(COLX[1], ROW1_Y + 8.8, 5.4, "¼×", "4×")
ranges(COLX[2], ROW1_Y + 8.8, 5.4, "LOW", "HIGH")
ranges(COLX[3], ROW1_Y + 8.8, 5.4, "1", "32")
ranges(COLX[4], ROW1_Y + 8.8, 5.4, "HARD", "SOFT")
for x, n in zip(COLX, ["FIELD", "HUMAN", "DRIFT", "PLANT", "SESSION"]):
    add(text(n, x, ROW2_Y - 7.8, CONTROL, fill=BLUE if n == "SESSION" else INK))
ranges(COLX[0], ROW2_Y + 7.6, 4.8, "1.5T", "7T")
ranges(COLX[1], ROW2_Y + 7.6, 4.8, "EXACT", "GATED")
ranges(COLX[2], ROW2_Y + 7.6, 4.8, "0", "WILD")
ranges(COLX[3], ROW2_Y + 7.6, 4.8, "OFF", "LOUD")
ranges(COLX[4], ROW2_Y + 7.6, 4.8, "OFF", "40 MIN")
for x, n in zip(X3, ["X", "Y", "Z", "BORE", "POSITION", "BODY"]):
    add(text(n, x, ROW3_Y - 6.4, CONTROL))
# EARS 3-way: lever down = 0 = OPEN
add(text("EARS", X3[6], ROW3_Y - 9.4, CONTROL))
for yy, o in zip([ROW3_Y - 5.0, ROW3_Y - 1.0, ROW3_Y + 3.0], ["PHONES", "PLUGS", "OPEN"]):
    add(text(o, X3[6] + 3.0, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("SLEW", X3[7] + 2.0, ROW3_Y - 6.4, CONTROL))

# jack strips
add(f'<rect x="2.2" y="{IN_Y - 9.0}" width="156.6" height="13.4" rx="1.4" fill="{STRIP}"/>')
for i, n in enumerate(["CLOCK", "RESET", "V/OCT", "SEQ", "TR", "SPACING", "RAMP", "DRIFT", "AUDIO"]):
    add(text(n, JX0 + JDX * i, IN_Y - 5.4, JACK, FONT_BOLD, INK))
add(f'<rect x="2.2" y="{OUT_Y - 9.0}" width="{JDX * 3 + 1.6}" height="13.4" rx="1.4" fill="{BLUE}"/>')
add(f'<rect x="{JX0 + JDX * 3 - 8.3}" y="{OUT_Y - 9.0}" width="{JDX * 5 - 1.0}" height="13.4" rx="1.4" fill="{STRIP}"/>')
for i, n in enumerate(["GX", "GY", "GZ"]):
    add(text(n, JX0 + JDX * i, OUT_Y - 5.4, JACK, FONT_BOLD, WHITE))
for i, n in enumerate(["TR", "SLICE", "PUMP", "SCAN", "END"]):
    add(text(n, JX0 + JDX * (3 + i), OUT_Y - 5.4, JACK, FONT_BOLD, INK))
add(text("gradient CV", JX0 + JDX * 8, OUT_Y - 2.2, SMALL, FONT_MED, SOFT))
add(text("±5 V out", JX0 + JDX * 8, OUT_Y + 1.4, SMALL, FONT_MED, SOFT))

# right column: coil outs and the main out
add(text("COILS", RX, RY[0] - 9.6, CONTROL, FONT_BOLD, BLUE))
for y, n in zip(RY, ["X", "Y", "Z"]):
    add(text(n, RX - 6.4, y + 1.1, JACK, FONT_BOLD, INK, anchor="end"))
add(f'<rect x="{RX - 8.2}" y="{IN_Y - 9.0}" width="16.4" height="{OUT_Y - IN_Y + 13.4}" rx="1.4" fill="{BLUE}"/>')
add(text("L", RX, IN_Y - 5.4, JACK, FONT_BOLD, WHITE))
add(text("R", RX, OUT_Y - 5.4, JACK, FONT_BOLD, WHITE))
add(text("OUT", RX, IN_Y - 11.2, CONTROL, FONT_BOLD, BLUE))
write("Scanner.svg", W, P)

# =========================================================== BORE ROOM (12 HP)
W = 60.96
# ---- LAYOUT (same numbers in src/BoreRoom.cpp) ----
BX = [17.0, 44.0]
BR1_Y = 29.0
BR2_Y = 52.0
BR3_Y = 73.0
EARS_Y = 88.0
BJX = [9.0, 23.3, 37.6, 52.0]
BCV_Y = 103.0
BIO_Y = 117.0

P = []
add = P.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Bore Room", 4.0, 11.4, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("the scanner tube as a reverb", 4.0, 15.6, 2.4, FONT_MED, BLUE, anchor="start"))
bore_rings(add, 53.5, 9.6, 3.8)
for x, n in zip(BX, ["SIZE", "DECAY"]):
    add(text(n, x, BR1_Y - 9.4, CONTROL, fill=BLUE if n == "SIZE" else INK))
ranges(BX[0], BR1_Y + 8.8, 5.4, "HEAD", "WIDE")
ranges(BX[1], BR1_Y + 8.8, 5.4, "0.1s", "1.5s")
for x, n in zip(BX, ["DAMP", "POSITION"]):
    add(text(n, x, BR2_Y - 7.8, CONTROL))
ranges(BX[0], BR2_Y + 7.6, 4.8, "HARD", "PAD")
ranges(BX[1], BR2_Y + 7.6, 4.8, "MID", "END")
for x, n in zip(BX, ["BODY", "MIX"]):
    add(text(n, x, BR3_Y - 7.8, CONTROL, fill=BLUE if n == "MIX" else INK))
ranges(BX[1], BR3_Y + 7.6, 4.8, "DRY", "WET")
add(text("EARS", 17.0, EARS_Y + 1.1, CONTROL, anchor="middle"))
for yy, o in zip([EARS_Y - 3.0, EARS_Y + 1.0, EARS_Y + 5.0], ["PHONES", "PLUGS", "OPEN"]):
    add(text(o, 34.0, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(f'<rect x="2.2" y="{BCV_Y - 9.0}" width="{W - 4.4}" height="13.4" rx="1.4" fill="{STRIP}"/>')
for x, n in zip(BJX, ["SIZE", "DECAY", "POS", "MIX"]):
    add(text(n, x, BCV_Y - 5.4, JACK, FONT_BOLD, INK))
add(f'<rect x="2.2" y="{BIO_Y - 9.0}" width="27.6" height="13.4" rx="1.4" fill="{STRIP}"/>')
add(f'<rect x="{BJX[2] - 7.8}" y="{BIO_Y - 9.0}" width="{W - 2.2 - (BJX[2] - 7.8)}" height="13.4" rx="1.4" fill="{BLUE}"/>')
for x, n in zip(BJX[:2], ["IN L", "IN R"]):
    add(text(n, x, BIO_Y - 5.4, JACK, FONT_BOLD, INK))
for x, n in zip(BJX[2:], ["OUT L", "OUT R"]):
    add(text(n, x, BIO_Y - 5.4, JACK, FONT_BOLD, WHITE))
write("BoreRoom.svg", W, P)
print("ok")
