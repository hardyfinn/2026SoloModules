#!/usr/bin/env python3
# Draws res/VinylizerFable.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/VinylizerFable.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 254.0, 128.5  # 50 HP

BG = "#16151A"
COL_BG = "#1E1C23"
RIGHT_BG = "#211E25"
STRIP = "#0F0E12"
LINE = "#34303C"
INK = "#F2EADA"
SOFT = "#A9A2B2"
COPPER = "#D98B4A"
GOLD = "#CDB47A"
TEAL = "#8CC8BE"

# ---- LAYOUT (same numbers in src/VinylizerFable.cpp) ----
COLX0, COLDX = 13.0, 19.0
SW_Y, K1_Y, K2_Y, K3_Y, ON_Y, CV_Y = 33.0, 50.0, 66.0, 82.0, 95.5, 111.0
DISP_X, DISP_Y, DISP_W, DISP_H = 197.0, 13.5, 53.0, 25.0
FMT_X, DISC_X, SPEED_X, SW2_Y = 205.0, 222.0, 241.0, 49.0
NEEDLE_X, NEEDLE_Y, PLAY_X, PLAY_Y = 214.0, 70.0, 241.0, 70.0
MIX_X, OUT_X, MIXOUT_Y = 208.0, 238.0, 88.5
JR1_Y, NCV_X, PTRIG_X = 104.0, 211.0, 237.0
IO_Y = 117.0
IOX = [205.0, 218.0, 231.0, 244.0]

CONTROL = 2.8
JACK = 2.5
SMALL = 2.1

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


def colx(i):
    return COLX0 + COLDX * i


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<rect x="194.0" y="0" width="{W - 194.0}" height="{H}" fill="{RIGHT_BG}"/>')
add(f'<line x1="194.0" y1="4" x2="194.0" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')

# title
add(text("Vinylizer Fable", 5.0, 10.6, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.01))
add(text("the 90s record chain, cut · press · play", 189.0, 10.2, 2.6, FONT_MED, COPPER, anchor="end"))

# process columns
COLS = [
    # name, group colour, switch options (value order) or light label, knobs, cv label
    ("ELLIPTIC", COPPER, ("sw3", ["6dB", "12dB", "24dB"]), ["FREQ", "AMOUNT", None], "AMOUNT"),
    ("HF LIMIT", COPPER, ("light", "GR"), ["THRESH", "FREQ", "RELEASE"], "THRESH"),
    ("CUT", COPPER, ("light", "EXC"), ["LEVEL", "SIDE", "HEAD"], "LEVEL"),
    ("MEDIUM", GOLD, ("sw3", ["LACQ", "DMM", "DUB"]), ["ECHO", "WEAR", None], "WEAR"),
    ("TRACING", TEAL, ("sw3", ["SPH", "ELL", "FINE"]), ["AMOUNT", None, None], "AMOUNT"),
    ("TRACKING", TEAL, ("sw2", ["BAER", "STEV"]), ["FORCE", "BALANCE", None], "FORCE"),
    ("CARTRIDGE", TEAL, ("sw3", ["DJ", "HIFI", "MC"]), ["LOAD", "STEREO", "AZIMUTH"], "LOAD"),
    ("ARM", TEAL, None, ["RES", "DAMP", "WARP"], "WARP"),
    ("PLATTER", TEAL, ("sw3", ["DIRECT", "BELT", "CHEAP"]), ["ECCENTRIC", "WOW", "FLUTTER"], "WOW"),
    ("PHONO", TEAL, ("sw3", ["TUBE", "OPAMP", "DJ"]), ["GAIN", "TOLERANCE", "SUBSONIC"], "GAIN"),
]
RANGES = {
    (0, 0): ("40", "300"), (0, 1): ("0", "1"),
    (1, 0): ("−24", "+12"), (1, 1): ("3k", "12k"), (1, 2): ("5", "300"),
    (2, 0): ("−12", "+9"), (2, 1): ("4", "25"), (2, 2): ("0", "1"),
    (3, 0): ("0", "1"), (3, 1): ("0", "100"),
    (4, 0): ("0", "2"),
    (5, 0): ("¾g", "5g"), (5, 1): ("L", "R"),
    (6, 0): ("50", "600"), (6, 1): ("10", "40"), (6, 2): ("−", "+"),
    (7, 0): ("5", "20"), (7, 1): ("LOW", "HIGH"), (7, 2): ("0", "1.5"),
    (8, 0): ("0", "1mm"), (8, 1): ("0", "1"), (8, 2): ("0", "1"),
    (9, 0): ("−6", "+24"), (9, 1): ("−", "+"),
}

# group bands
def band(i0, i1, label, col):
    x0, x1 = colx(i0) - 8.6, colx(i1) + 8.6
    add(f'<rect x="{x0}" y="14.2" width="{x1 - x0}" height="0.6" fill="{col}"/>')
    add(f'<rect x="{x0 + 1.2}" y="13.0" width="{len(label) * 1.9 + 2.2}" height="3.0" fill="{BG}"/>')
    add(text(label, x0 + 2.2, 15.4, 2.4, FONT_BOLD, col, anchor="start", spacing=0.12))

band(0, 2, "CUT", COPPER)
band(3, 3, "PRESS", GOLD)
band(4, 9, "PLAY", TEAL)

# CV strip (drawn first so the labels sit on it)
add(f'<rect x="3.6" y="102.2" width="{colx(9) + 9.4 - 3.6}" height="15.0" rx="1.4" fill="{STRIP}"/>')

for i, (name, col, sw, knobs, cvl) in enumerate(COLS):
    x = colx(i)
    add(f'<rect x="{x - 8.9}" y="17.0" width="17.8" height="83.2" rx="1.4" fill="{COL_BG}"/>')
    add(text(str(i + 1), x, 20.4, 2.3, FONT_BOLD, col))
    add(text(name, x, 24.6, 2.75, FONT_BOLD, INK, spacing=0.02))
    if sw:
        kind, opts = sw
        if kind == "sw3":
            for yy, o in zip([SW_Y + 5.0, SW_Y + 1.0, SW_Y - 3.0], opts):
                add(text(o, x - 0.9, yy + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
        elif kind == "sw2":
            for yy, o in zip([SW_Y + 3.6, SW_Y - 1.4], opts):
                add(text(o, x - 0.9, yy + 0.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
        else:
            add(text(opts, x, SW_Y + 6.0, SMALL, FONT_BOLD, SOFT))
    for k, (kn, ky) in enumerate(zip(knobs, [K1_Y, K2_Y, K3_Y])):
        if not kn:
            continue
        if i == 9 and k == 2:  # subsonic switch
            add(text(kn, x, ky - 6.4, CONTROL - 0.3))
            for yy, o in zip([ky + 1.0 + 5.0, ky + 1.0 + 1.0, ky + 1.0 - 3.0], ["OFF", "IEC", "18Hz"]):
                add(text(o, x - 0.9, yy + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
            continue
        add(text(kn, x, ky - 6.6, CONTROL if len(kn) < 9 else CONTROL - 0.3))
        r = RANGES.get((i, k))
        if r:
            add(text(r[0], x - 4.4, ky + 6.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
            add(text(r[1], x + 4.4, ky + 6.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text("ON", x - 4.6, ON_Y + 0.8, SMALL, FONT_BOLD, SOFT, anchor="end"))
    # CV strip label
    add(text(cvl, x, CV_Y - 5.6, JACK - 0.2, FONT_BOLD, SOFT))

# tracking / phono lights labels
add(text("SKIP", colx(5), K3_Y + 5.6, SMALL, FONT_BOLD, SOFT))
add(text("OVL", colx(9) + 6.4, K3_Y + 3.2, SMALL, FONT_BOLD, SOFT))

add(text("CV  0-10 V = full range of the named control", 5.0, 121.3, 2.1, FONT_MED, SOFT, anchor="start"))

# ---- right side
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.2" fill="#050507" stroke="{COPPER}" stroke-opacity="0.6" stroke-width="0.35"/>')
add(text("FORMAT", FMT_X, SW2_Y - 6.0, CONTROL))
add(text("DISC", DISC_X, SW2_Y - 6.0, CONTROL))
for yy, o in zip([SW2_Y + 5.0, SW2_Y + 1.0, SW2_Y - 3.0], ['7"', '10"', '12"']):
    add(text(o, DISC_X + 3.1, yy + 0.8, SMALL + 0.2, FONT_BOLD, SOFT, anchor="start"))
add(text("RPM", SPEED_X, SW2_Y - 6.0, CONTROL))
for yy, o in zip([SW2_Y + 3.6, SW2_Y - 1.4], ["33", "45"]):
    add(text(o, SPEED_X + 3.1, yy + 0.6, SMALL + 0.2, FONT_BOLD, SOFT, anchor="start"))
add(text("NEEDLE", NEEDLE_X, NEEDLE_Y - 10.6, CONTROL + 0.4, FONT_BOLD, COPPER))
add(text("EDGE", NEEDLE_X - 9.6, NEEDLE_Y + 8.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
add(text("END", NEEDLE_X + 9.6, NEEDLE_Y + 8.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("PLAY", PLAY_X, PLAY_Y - 7.6, CONTROL))
add(text("SIDE", PLAY_X, PLAY_Y - 4.7, CONTROL))
add(text("MIX", MIX_X, MIXOUT_Y - 6.6, CONTROL))
add(text("OUTPUT", OUT_X, MIXOUT_Y - 6.6, CONTROL))
add(f'<rect x="197.6" y="97.6" width="{W - 3.0 - 197.6}" height="25.6" rx="1.4" fill="{STRIP}"/>')
add(text("NEEDLE", NCV_X, JR1_Y - 5.0, JACK, FONT_BOLD, SOFT))
add(text("PLAY", PTRIG_X, JR1_Y - 5.0, JACK, FONT_BOLD, SOFT))
for x, n in zip(IOX[:2], ["IN L", "IN R"]):
    add(text(n, x, IO_Y - 5.0, JACK))
add(f'<rect x="{IOX[2] - 6.0}" y="{IO_Y - 8.4}" width="{IOX[3] - IOX[2] + 12.0}" height="13.4" rx="1.4" fill="{COPPER}"/>')
for x, n in zip(IOX[2:], ["OUT L", "OUT R"]):
    add(text(n, x, IO_Y - 5.0, JACK, fill=BG))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "VinylizerFable.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "VinylizerFable.svg"), x=150.0, y=126.0, cap=3.0, bg=BG)
