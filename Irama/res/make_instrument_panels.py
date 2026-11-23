#!/usr/bin/env python3
# Draws res/Irama.svg (40 HP) and res/Gatra.svg (24 HP). Rack can't draw SVG
# <text>, so labels become paths. Positions in mm; the LAYOUT numbers are shared
# with src/Irama.cpp and src/Gatra.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

H = 128.5
BG = "#2A1D14"        # dark teak
BG_B = "#33241A"
BG_DARK = "#1C130D"
LINE = "#5A4330"
INK = "#F3E6C8"
SOFT = "#C9B79A"
GOLD = "#E8B040"      # bronze / gold leaf
RED = "#C8442C"       # lacquer red

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4     # hints and range marks

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


def save(name, W, parts):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(p for p in parts if p) + "\n</svg>\n")
    open(os.path.join(HERE, name), "w").write(svg)


def gong_motif(add, x, y, r):
    # a small gong (boss in a circle) used as a title ornament
    add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="none" stroke="{GOLD}" stroke-width="0.45"/>')
    add(f'<circle cx="{x}" cy="{y}" r="{r * 0.42}" fill="{GOLD}"/>')



# Panels for the instrument modules: Ageng (18 HP), Gangsa (20 HP), Reyong (22 HP).
# Shares the look and helpers of make_panel.py. LAYOUT numbers match src/Ageng.cpp,
# src/Gangsa.cpp, src/Reyong.cpp.   python3 res/make_instrument_panels.py
sys.path.insert(0, HERE)
import haer_logo


def frame(W, title, sub):
    parts = []
    add = parts.append
    add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
    add(text(title, 6.0, 11.4, 5.8, FONT_BOLD, INK, anchor="start", spacing=0.02))
    add(text(sub, 6.0, 16.6, 2.6, FONT_MED, GOLD, anchor="start"))
    return parts, add


def knob_label(add, x, y, name, big=False, col=None):
    add(text(name, x, y - (9.6 if big else 7.6), CONTROL, fill=col or INK))


def marks(add, x, y, lo, hi, r=4.4, dy=7.4):
    add(text(lo, x - r, y + dy, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y + dy, SMALL, FONT_BOLD, SOFT, anchor="start"))


def sw(add, x, y, title, up, down, side=False):
    if side:   # title to the left of the switch (one word per line), for tight rows
        words = title.split()
        y0 = y + 1.2 - 1.8 * (len(words) - 1)
        for i, w in enumerate(words):
            add(text(w, x - 3.4, y0 + 3.6 * i, CONTROL, anchor="end"))
    else:
        add(text(title, x, y - 7.6, CONTROL))
    add(text(up, x + 3.1, y - 1.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(down, x + 3.1, y + 3.6, SMALL, FONT_BOLD, SOFT, anchor="start"))


def jacks(add, xs, y, names, out=False):
    for x, n in zip(xs, names):
        add(text(n, x, y - 5.3, JACK, FONT_BOLD, BG if out else SOFT))


def strip(add, x0, x1, y, out=False):
    add(f'<rect x="{x0}" y="{y - 8.8}" width="{x1 - x0}" height="14.6" rx="1.4" fill="{GOLD if out else BG_DARK}"/>')


# ================================================================ AGENG
# ---- LAYOUT (same numbers in src/Ageng.cpp) ----
W = 91.44
A_COLX = [16.0, 45.72, 75.44]
A_ROW_Y = [33.0, 54.0, 75.0]
A_ROW4_Y = 89.0
A_IN_Y, A_IN_X0, A_IN_DX = 102.0, 11.0, 23.15
A_BOT_Y, A_BOT_X0, A_BOT_DX = 117.0, 9.0, 14.69

parts, add = frame(W, "Ageng", "gong pair + kempul")
names = ["TUNE", "INTERVAL", "SIZE", "BLOOM", "OMBAK", "MALLET", "BRIGHT", "K TUNE", "K RING"]
for i, n in enumerate(names):
    x, y = A_COLX[i % 3], A_ROW_Y[i // 3]
    knob_label(add, x, y, n, big=(i == 0), col=GOLD if n in ("BLOOM", "OMBAK") else None)
marks(add, A_COLX[0], A_ROW_Y[0], "32", "98Hz", 5.4, 8.6)
marks(add, A_COLX[1], A_ROW_Y[0], "0", "12")
marks(add, A_COLX[2], A_ROW_Y[0], "6s", "30s")
marks(add, A_COLX[1], A_ROW_Y[1], "0", "6Hz")
marks(add, A_COLX[2], A_ROW_Y[1], "SOFT", "HARD")
sw(add, A_COLX[0], A_ROW4_Y, "ALT", "ON", "OFF", side=True)
add(text("GONG LVL", A_COLX[1], A_ROW4_Y - 5.4, CONTROL)); add(text("KEMP LVL", A_COLX[2], A_ROW4_Y - 5.4, CONTROL))
strip(add, 2.6, W - 2.6, A_IN_Y)
jacks(add, [A_IN_X0 + A_IN_DX * i for i in range(4)], A_IN_Y, ["GONG", "GONG 2", "KEMPUL", "K V/OCT"])
strip(add, 2.6, A_BOT_X0 + A_BOT_DX * 2.5, A_BOT_Y)
strip(add, A_BOT_X0 + A_BOT_DX * 2.5, W - 2.6, A_BOT_Y, out=True)
jacks(add, [A_BOT_X0 + A_BOT_DX * i for i in range(3)], A_BOT_Y, ["VEL", "DAMP", "TUNE"])
jacks(add, [A_BOT_X0 + A_BOT_DX * (3 + i) for i in range(3)], A_BOT_Y, ["L", "R", "SUB"], out=True)
save("Ageng.svg", W, parts)
haer_logo.add_to_file(os.path.join(HERE, "Ageng.svg"), x=W - 14.0, y=9.9, cap=3.0, bg=BG)

# ================================================================ GANGSA
# ---- LAYOUT (same numbers in src/Gangsa.cpp) ----
W = 101.6
G2_COLX = [19.0, 50.8, 82.6]
G2_ROW_Y = [33.0, 56.0, 76.0]
G2_ROW4_Y = 89.5
G2_IN_Y, G2_IN_X0, G2_IN_DX = 102.0, 11.0, 19.9
G2_BOT_Y, G2_BOT_X0, G2_BOT_DX = 117.0, 10.0, 16.32

parts, add = frame(W, "Gangsa", "polos + sangsih · umbang + isep")
names = ["OMBAK", "KANTILAN", "MALLET", "DAMP", "RING", "BRIGHT", "TUBE", "SET", "WIDTH"]
for i, n in enumerate(names):
    x, y = G2_COLX[i % 3], G2_ROW_Y[i // 3]
    knob_label(add, x, y, n, big=(i in (0, 3)), col=GOLD if n in ("OMBAK", "DAMP") else None)
marks(add, G2_COLX[0], G2_ROW_Y[0], "0", "12Hz", 5.4, 8.6)
marks(add, G2_COLX[0], G2_ROW_Y[1], "RING", "CHOKE", 5.4, 8.6)
marks(add, G2_COLX[1], G2_ROW_Y[1], ".3", "6s")
sw(add, G2_COLX[0], G2_ROW4_Y, "DAMP ON", "GATE", "NEXT", side=True)
add(text("POLOS", G2_COLX[1], G2_ROW4_Y - 3.6, SMALL, FONT_BOLD, SOFT))
add(text("SANGSIH", G2_COLX[2], G2_ROW4_Y - 3.6, SMALL, FONT_BOLD, SOFT))
strip(add, 2.6, W - 2.6, G2_IN_Y)
jacks(add, [G2_IN_X0 + G2_IN_DX * i for i in range(5)], G2_IN_Y, ["P V/OCT", "P GATE", "S V/OCT", "S GATE", "OMBAK"])
strip(add, 2.6, G2_BOT_X0 + G2_BOT_DX * 1.5, G2_BOT_Y)
strip(add, G2_BOT_X0 + G2_BOT_DX * 1.5, W - 2.6, G2_BOT_Y, out=True)
jacks(add, [G2_BOT_X0 + G2_BOT_DX * i for i in range(2)], G2_BOT_Y, ["VEL", "DAMP"])
jacks(add, [G2_BOT_X0 + G2_BOT_DX * (2 + i) for i in range(4)], G2_BOT_Y, ["L", "R", "POLOS", "SANGSIH"], out=True)
save("Gangsa.svg", W, parts)
haer_logo.add_to_file(os.path.join(HERE, "Gangsa.svg"), x=W - 14.0, y=9.9, cap=3.0, bg=BG)

# ================================================================ REYONG
# ---- LAYOUT (same numbers in src/Reyong.cpp) ----
W = 111.76
R_COLX = [15.0, 41.5, 68.0, 94.5]
R_ROW_Y = [33.0, 53.0, 73.0]
R_IN_Y, R_IN_X0, R_IN_DX = 95.0, 9.5, 15.46
R_BOT_Y, R_BOT_X0, R_BOT_DX = 116.0, 11.0, 17.95

parts, add = frame(W, "Reyong", "bonang · reyong · kenong · kethuk")
names = [None, "STRIKE", "MALLET", "RING", "DAMP", "BYOT", "CEK", "SYMPATHY", None, "SET", "WIDTH", "K RING"]
for i, n in enumerate(names):
    if n:
        knob_label(add, R_COLX[i % 4], R_ROW_Y[i // 4], n, col=GOLD if n in ("BYOT", "CEK") else None)
sw(add, R_COLX[0], R_ROW_Y[0], "STYLE", "BALI", "JAVA")
sw(add, R_COLX[0], R_ROW_Y[2], "DAMP ON", "GATE", "NEXT")
marks(add, R_COLX[1], R_ROW_Y[0], "BOSS", "RIM")
marks(add, R_COLX[3], R_ROW_Y[0], ".2", "5s")
strip(add, 2.6, W - 2.6, R_IN_Y)
jacks(add, [R_IN_X0 + R_IN_DX * i for i in range(7)], R_IN_Y, ["A V/OCT", "A GATE", "B V/OCT", "B GATE", "CEK", "BYOT", "VEL"])
strip(add, 2.6, R_BOT_X0 + R_BOT_DX * 2.5, R_BOT_Y)
strip(add, R_BOT_X0 + R_BOT_DX * 2.5, W - 2.6, R_BOT_Y, out=True)
jacks(add, [R_BOT_X0 + R_BOT_DX * i for i in range(3)], R_BOT_Y, ["KENONG", "K V/OCT", "KETHUK"])
jacks(add, [R_BOT_X0 + R_BOT_DX * (3 + i) for i in range(3)], R_BOT_Y, ["L", "R", "COLO"], out=True)
save("Reyong.svg", W, parts)
haer_logo.add_to_file(os.path.join(HERE, "Reyong.svg"), x=W - 14.0, y=9.9, cap=3.0, bg=BG)
print("ok")
