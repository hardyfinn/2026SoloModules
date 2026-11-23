#!/usr/bin/env python3
# Draws res/Peal.svg and res/BellFoundry.svg. Rack can't draw SVG <text>, so
# labels become paths. Positions in mm; LAYOUT numbers match src/*.cpp.
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

INK = "#F0E6D2"
SOFT = "#BFB3A0"
BRONZE = "#E8B262"
RED = "#FF5A50"

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5     # range marks

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


def write(name, W, H, parts, bg, logo_x):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(parts) + "\n</svg>\n")
    path = os.path.join(HERE, name)
    open(path, "w").write(svg)
    haer_logo.add_to_file(path, x=logo_x, y=125.6, cap=3.0, bg=bg)
    print("wrote", name)


def bell_icon(x, y, s, fill):
    # a small bell silhouette for the title
    return (f'<path fill="{fill}" transform="translate({x},{y}) scale({s})" '
            'd="M0 -4.2 C-1.9 -4.2 -2.6 -2.6 -2.6 -0.6 C-2.6 1.2 -3.0 2.0 -3.8 2.8 L3.8 2.8 C3.0 2.0 2.6 1.2 2.6 -0.6 '
            'C2.6 -2.6 1.9 -4.2 0 -4.2 Z M-0.9 3.3 A0.9 0.9 0 0 0 0.9 3.3 Z"/>')


# =========================== PEAL (36 HP) ===========================
# ---- LAYOUT (same numbers in src/Peal.cpp) ----
W, H = 182.88, 128.5
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 14.5, 116.0, 28.5
BTN_X = [137.0, 165.0]
BTN_Y = [23.0, 38.5]
KNOB_X = [16.0, 46.0, 76.0, 106.0, 136.0, 166.0]
KNOB_Y = 58.0
J_X0, J_DX = 10.0, 14.808
ROWA_Y, ROWB_Y, ROWC_Y = 84.0, 100.0, 116.0

BG = "#231E1A"
BG_DARK = "#16120F"
LINE = "#4A3F33"


def jx(i):
    return J_X0 + J_DX * i


P = []
add = P.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(bell_icon(9.0, 8.6, 0.85, BRONZE))
add(text("Peal", 13.6, 11.4, 5.6, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("change ringing sequencer", DISP_X + DISP_W, 11.0, 2.6, FONT_MED, BRONZE, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="#0B0907" stroke="{BRONZE}" stroke-opacity="0.55" stroke-width="0.35"/>')

# buttons
for (x, y, n, col) in [(BTN_X[0], BTN_Y[0], "GO", BRONZE), (BTN_X[1], BTN_Y[0], "RESET", INK),
                       (BTN_X[0], BTN_Y[1], "BOB", RED), (BTN_X[1], BTN_Y[1], "SINGLE", RED)]:
    add(text(n, x, y - 5.6, CONTROL, fill=col))

# knobs
names = ["BELLS", "METHOD", "CALLS", "GAP", "GATE", "SCALE"]
for x, n in zip(KNOB_X, names):
    add(text(n, x, KNOB_Y - 9.4, CONTROL, fill=BRONZE if n in ("METHOD", "CALLS") else INK))


def ranges(x, y, r, lo, hi):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


ranges(KNOB_X[0], KNOB_Y + 8.6, 5.0, "4", "12")
ranges(KNOB_X[2], KNOB_Y + 8.6, 5.0, "NONE", "ALL")
ranges(KNOB_X[3], KNOB_Y + 8.6, 5.0, "0", "4")
ranges(KNOB_X[4], KNOB_Y + 8.6, 5.0, "SHORT", "TIE")
ranges(KNOB_X[5], KNOB_Y + 8.6, 5.0, "MAJ", "CHROM")

# jack strips: inputs dark, outputs bronze
add(f'<rect x="2.6" y="{ROWA_Y - 9.0}" width="{jx(6) - 2.6 + 7.2}" height="13.6" rx="1.4" fill="{BG_DARK}"/>')
add(f'<rect x="2.6" y="{ROWB_Y - 9.0}" width="{jx(5) - 2.6 + 7.2}" height="13.6" rx="1.4" fill="{BG_DARK}"/>')
add(f'<rect x="{jx(7) - 7.2}" y="{ROWA_Y - 9.0}" width="{jx(11) - jx(7) + 14.4}" height="13.6" rx="1.4" fill="{BRONZE}"/>')
add(f'<rect x="{jx(6) - 7.2}" y="{ROWB_Y - 9.0}" width="{jx(11) - jx(6) + 14.4}" height="13.6" rx="1.4" fill="{BRONZE}"/>')
add(f'<rect x="2.6" y="{ROWC_Y - 9.0}" width="{W - 5.2}" height="13.6" rx="1.4" fill="{BRONZE}"/>')
for i, n in enumerate(["CLOCK", "RESET", "GO", "BOB", "SINGLE", "TUNE", "V/OCT"]):
    add(text(n, jx(i), ROWA_Y - 5.4, JACK, fill=INK))
for i, n in enumerate(["PITCH", "PLACE", "ROW", "GATES", "TUNES"]):
    add(text(n, jx(7 + i), ROWA_Y - 5.4, JACK, fill=BG))
for i, n in enumerate(["BELLS", "METHOD", "CALLS", "GAP", "GATE", "SCALE"]):
    add(text(n, jx(i), ROWB_Y - 5.4, JACK, fill=SOFT))
for i, n in enumerate(["STRIKE", "HAND", "CHANGE", "LEAD", "ROUNDS", "CALL"]):
    add(text(n, jx(6 + i), ROWB_Y - 5.4, JACK, fill=BG))
for i in range(12):
    lab = str(i + 1)
    add(text(lab, jx(i), ROWC_Y - 5.4, JACK, fill=RED if i == 0 else BG))
write("Peal.svg", W, H, P, BG, W / 2)

# ======================= BELL FOUNDRY (20 HP) =======================
# ---- LAYOUT (same numbers in src/BellFoundry.cpp) ----
FW = 121.92
F_DISP_X, F_DISP_Y, F_DISP_W, F_DISP_H = 6.0, 14.5, 109.92, 17.5
F_SW_Y = 44.0
F_SW_X = [17.5, 55.0]
F_RECAST_X = 104.5
F_KX = [17.5, 46.5, 75.5, 104.5]
F_K1_Y, F_K2_Y = 61.5, 82.0
F_J_X0, F_J_DX = 12.0, 19.584
F_CV_Y, F_IO_Y = 101.0, 117.0

FBG = "#1E2522"
FBG_DARK = "#121715"
VERD = "#7CC7AE"

P = []
add = P.append
add(f'<rect width="{FW}" height="{H}" fill="{FBG}"/>')
add(bell_icon(9.0, 8.6, 0.85, BRONZE))
add(text("Bell Foundry", 13.6, 11.4, 5.2, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("12 cast bells", F_DISP_X + F_DISP_W, 11.0, 2.6, FONT_MED, VERD, anchor="end"))
add(f'<rect x="{F_DISP_X - 0.6}" y="{F_DISP_Y - 0.6}" width="{F_DISP_W + 1.2}" height="{F_DISP_H + 1.2}" '
    f'rx="1.0" fill="#0A0D0C" stroke="{VERD}" stroke-opacity="0.5" stroke-width="0.35"/>')


def switch3(x, y, title, opts):
    add(text(title, x, y - 7.4, CONTROL))
    for yy, o in zip([y - 3.0, y + 1.0, y + 5.0], opts):
        add(text(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))


switch3(F_SW_X[0], F_SW_Y, "TUNING", ["TRUE", "OLD", "OMBAK"])
switch3(F_SW_X[1], F_SW_Y, "LISTEN", ["CHAMBER", "CHURCHYARD", "TOWN"])
add(text("RECAST", F_RECAST_X, F_SW_Y - 7.4, CONTROL, fill=BRONZE))

for x, n in zip(F_KX, ["PROFILE", "METAL", "THICK", "SIZE"]):
    add(text(n, x, F_K1_Y - 8.0, CONTROL))
for x, n in zip(F_KX, ["CRACK", "CLAPPER", "RING", "SPREAD"]):
    add(text(n, x, F_K2_Y - 8.0, CONTROL, fill=RED if n == "CRACK" else INK))


def franges(x, y, lo, hi):
    add(text(lo, x - 4.6, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + 4.6, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


franges(F_KX[0], F_K1_Y + 7.6, "CHURCH", "GONG")
franges(F_KX[1], F_K1_Y + 7.6, "BRASS", "STEEL")
franges(F_KX[2], F_K1_Y + 7.6, "THIN", "THICK")
franges(F_KX[3], F_K1_Y + 7.6, "−2", "+2")
franges(F_KX[0], F_K2_Y + 7.6, "NONE", "MAX")
franges(F_KX[1], F_K2_Y + 7.6, "SOFT", "IRON")
franges(F_KX[2], F_K2_Y + 7.6, "SHORT", "LONG")
franges(F_KX[3], F_K2_Y + 7.6, "MONO", "WIDE")

fj = lambda i: F_J_X0 + F_J_DX * i
add(f'<rect x="2.6" y="{F_CV_Y - 9.0}" width="{FW - 5.2}" height="13.6" rx="1.4" fill="{FBG_DARK}"/>')
add(f'<rect x="2.6" y="{F_IO_Y - 9.0}" width="{fj(2) - 2.6 + 7.4}" height="13.6" rx="1.4" fill="{FBG_DARK}"/>')
add(f'<rect x="{fj(4) - 7.4}" y="{F_IO_Y - 9.0}" width="{fj(5) - fj(4) + 14.8}" height="13.6" rx="1.4" fill="{BRONZE}"/>')
for i, n in enumerate(["PROFILE", "METAL", "THICK", "CRACK", "CLAPPER", "RING"]):
    add(text(n, fj(i), F_CV_Y - 5.4, JACK, fill=SOFT))
for i, n in enumerate(["GATES", "V/OCT", "VEL"]):
    add(text(n, fj(i), F_IO_Y - 5.4, JACK, fill=INK))
add(text("from Peal", fj(3), F_IO_Y - 1.4, SMALL, FONT_MED, VERD))
add(text("GATES + TUNES", fj(3), F_IO_Y + 2.2, SMALL, FONT_MED, VERD))
for i, n in enumerate(["OUT L", "OUT R"]):
    add(text(n, fj(4 + i), F_IO_Y - 5.4, JACK, fill=FBG))
write("BellFoundry.svg", FW, H, P, FBG, fj(3))
