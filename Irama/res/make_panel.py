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


# =====================================================================  IRAMA
# ---- LAYOUT (same numbers in src/Irama.cpp) ----
W = 203.2   # 40 HP
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 14.5, 125.0, 18.5
COLX = [13.0, 35.0, 57.0, 79.0, 101.0, 123.0]
ROW1_Y, ROW2_Y, ROW3_Y = 46.0, 66.0, 82.0
JX0, JDX = 10.0, 16.5
IN_Y, COL_Y = 99.5, 116.0
OUT_VX, OUT_GX = 168.0, 189.0
OUT_Y0, OUT_DY = 31.0, 13.0

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<rect x="136" y="0" width="{W - 136}" height="{H}" fill="{BG_B}"/>')
add(f'<line x1="136" y1="4" x2="136" y2="{H - 4}" stroke="{LINE}" stroke-width="0.4"/>')
add(text("Irama", 6.0, 11.4, 5.8, FONT_BOLD, INK, anchor="start", spacing=0.02))
gong_motif(add, 30.5, 9.4, 2.2)
add(text("gamelan sequencer · Java · Bali", 131.0, 11.0, 2.6, FONT_MED, GOLD, anchor="end"))
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" rx="1.0" '
    f'fill="#0E0905" stroke="{GOLD}" stroke-opacity="0.6" stroke-width="0.35"/>')


def ranges(x, y, r, lo, hi):
    add(text(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start"))


def switch(x, y, title, up, down):
    add(text(title, x, y - 7.6, CONTROL))
    add(text(up, x + 3.1, y - 1.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(text(down, x + 3.1, y + 3.6, SMALL, FONT_BOLD, SOFT, anchor="start"))


# row 1
switch(COLX[0], ROW1_Y, "STYLE", "BALI", "JAVA")
for x, n in zip(COLX[1:], ["FORM", "PATHET", "ROOT", "IRAMA", "KOTEKAN"]):
    add(text(n, x, ROW1_Y - 7.6, CONTROL, fill=GOLD if n in ("IRAMA", "KOTEKAN") else INK))
ranges(COLX[3], ROW1_Y + 7.4, 4.4, "−12", "+12")
ranges(COLX[4], ROW1_Y + 7.4, 4.4, "1", "16")
# row 2
for x, n in zip(COLX[:4], ["MUTATE", "FOCUS", "OMBAK", "ANGSEL"]):
    add(text(n, x, ROW2_Y - 7.6, CONTROL, fill=GOLD if n == "OMBAK" else INK))
ranges(COLX[1], ROW2_Y + 7.4, 4.4, "FLAT", "3")
ranges(COLX[2], ROW2_Y + 7.4, 4.4, "0", "10Hz")
switch(COLX[4], ROW2_Y, "TEMPO", "FIXED", "TRAD")
switch(COLX[5], ROW2_Y, "LOCK", "ON", "OFF")
# row 3: buttons
for x, n in zip(COLX[:4], ["NEW", "ANGSEL", "KEBYAR", "SUWUK"]):
    add(text(n, x, ROW3_Y - 5.6, CONTROL, fill=RED if n == "KEBYAR" else INK))
add(text("CLOCK", COLX[5], ROW3_Y - 4.0, SMALL, FONT_BOLD, SOFT))

# inputs
add(f'<rect x="2.6" y="89.4" width="130.8" height="15.4" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["CLOCK", "RESET", "IRAMA", "MUTATE", "ANGSEL", "KEBYAR", "SUWUK", "NEW"]):
    add(text(n, JX0 + JDX * i, IN_Y - 5.4, JACK, FONT_BOLD, SOFT))
# colotomic outputs (gold strip, Bali names above)
add(f'<rect x="2.6" y="105.6" width="130.8" height="15.6" rx="1.4" fill="{GOLD}"/>')
names = ["GONG", "KENONG", "KEMPUL", "KETHUK", "KMPYNG", "GATRA", "KENDNG", "ANGSEL"]
bali = ["", "kempur", "kemong", "kempli", "kajar", "", "", "gate"]
for i, (n, b) in enumerate(zip(names, bali)):
    x = JX0 + JDX * i
    add(text(n, x, COL_Y - 5.3, JACK, FONT_BOLD, BG))
    if b:
        add(text(b, x, COL_Y - 8.2 + 0.0, 2.1, FONT_MED, BG_DARK))

# melodic outputs (right)
add(f'<rect x="138.4" y="20.0" width="62.4" height="{OUT_Y0 + OUT_DY * 6 + 6.0 - 20.0}" rx="1.6" fill="{GOLD}"/>')
add(text("V/OCT", OUT_VX, 24.0, JACK, FONT_BOLD, BG))
add(text("GATE", OUT_GX, 24.0, JACK, FONT_BOLD, BG))
rows = [("SARON", "calung"), ("PEKING", "kantilan"), ("BONANG", "reyong"), ("POLOS", "imbal A"),
        ("SANGSIH", "imbal B"), ("SELEH", "bass"), ("POLY", "all five")]
for i, (n, b) in enumerate(rows):
    y = OUT_Y0 + OUT_DY * i
    add(text(n, 140.6, y + 0.2, JACK, FONT_BOLD, BG, anchor="start"))
    add(text(b, 140.6, y + 3.4, 2.2, FONT_MED, BG_DARK, anchor="start"))
    if i in (5, 6):
        add(f'<line x1="140.6" y1="{y - 6.4}" x2="198.6" y2="{y - 6.4}" stroke="{BG}" stroke-width="0.3"/>')
add(text("ombak: 2 channels", 169.6, 119.6, SMALL, FONT_MED, SOFT, anchor="middle"))
add(text("umbang + isep", 169.6, 122.4, SMALL, FONT_MED, SOFT, anchor="middle"))

save("Irama.svg", W, parts)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Irama.svg"), x=169.6, y=126.0, cap=3.0, bg=BG_B)

# =====================================================================  GATRA
# ---- LAYOUT (same numbers in src/Gatra.cpp) ----
W = 121.92  # 24 HP
G_DISP_X, G_DISP_Y, G_DISP_W, G_DISP_H = 6.0, 14.5, 109.92, 17.0
G_COLX = [16.0, 46.0, 76.0, 106.0]
G_ROW1_Y, G_ROW2_Y, G_ROW3_Y = 46.0, 65.0, 79.5
G_SWX = [68.0, 87.0, 106.0]
G_JX0, G_JDX, G_IN_Y = 9.0, 14.85, 93.0
G_OX0, G_ODX, G_OA_Y, G_OB_Y = 14.0, 23.48, 106.0, 119.5

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Gatra", 6.0, 11.4, 5.8, FONT_BOLD, INK, anchor="start", spacing=0.02))
gong_motif(add, 29.6, 9.4, 2.2)
add(text("gamelan × house / techno", 88.0, 11.0, 2.6, FONT_MED, GOLD, anchor="end"))
add(f'<rect x="{G_DISP_X - 0.6}" y="{G_DISP_Y - 0.6}" width="{G_DISP_W + 1.2}" height="{G_DISP_H + 1.2}" rx="1.0" '
    f'fill="#0E0905" stroke="{GOLD}" stroke-opacity="0.6" stroke-width="0.35"/>')

for x, n in zip(G_COLX, ["CYCLE", "LAND", "DENSITY", "SWING"]):
    add(text(n, x, G_ROW1_Y - (9.6 if n == "LAND" else 7.6), CONTROL, fill=GOLD if n == "LAND" else INK))
ranges(G_COLX[0], G_ROW1_Y + 7.4, 4.4, "8", "32")
ranges(G_COLX[1], G_ROW1_Y + 6.2, 7.4, "END", "ONE")
add(text("and of 4", G_COLX[1], G_ROW1_Y - 6.6, SMALL, FONT_MED, SOFT))
ranges(G_COLX[3], G_ROW1_Y + 7.4, 4.4, "50", "75%")
for x, n in zip(G_COLX[:2], ["MUTATE", "ANGSEL"]):
    add(text(n, x, G_ROW2_Y - 7.6, CONTROL))
switch(G_SWX[0], G_ROW2_Y, "BEAT", "1/8", "1/4")
add(text("KOTEKAN", G_SWX[1], G_ROW2_Y - 8.6, CONTROL, fill=GOLD))
for yy, o in zip([G_ROW2_Y - 3.0, G_ROW2_Y + 1.0, G_ROW2_Y + 5.0], ["EMPAT", "TELU", "NYOG"]):
    add(text(o, G_SWX[1] + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start"))
switch(G_SWX[2], G_ROW2_Y, "KICK", "ON", "OFF")
add(text("DROP", G_COLX[0] + 6.0, G_ROW3_Y + 1.1, CONTROL, anchor="start", fill=RED))
add(text("FILL", G_COLX[1] + 6.0, G_ROW3_Y + 1.1, CONTROL, anchor="start", fill=GOLD))
add(text("hold", G_COLX[0] + 18.0, G_ROW3_Y + 1.1, SMALL, FONT_MED, SOFT, anchor="start"))
add(text("hold", G_COLX[1] + 14.6, G_ROW3_Y + 1.1, SMALL, FONT_MED, SOFT, anchor="start"))
add(text("CLOCK", G_COLX[3] - 4.0, G_ROW3_Y + 1.0, SMALL, FONT_BOLD, SOFT, anchor="end"))

add(f'<rect x="2.6" y="83.4" width="{W - 5.2}" height="14.4" rx="1.4" fill="{BG_DARK}"/>')
for i, n in enumerate(["CLOCK", "RESET", "DROP", "FILL", "LAND", "DENS", "MUTATE", "CYCLE"]):
    add(text(n, G_JX0 + G_JDX * i, G_IN_Y - 5.2, JACK, FONT_BOLD, SOFT))
add(f'<rect x="2.6" y="98.4" width="{W - 5.2}" height="26.4" rx="1.4" fill="{GOLD}"/>')
rowA = [("KICK", "4 on floor"), ("GONG", "crash"), ("KENONG", "clap"), ("KEMPUL", "open hat"), ("KETHUK", "perc")]
rowB = [("KMPYNG", "rim"), ("POLOS", "hat"), ("SANGSIH", "shaker"), ("ACCENT", ""), ("ANGSEL", "drop")]
for row, y in ((rowA, G_OA_Y), (rowB, G_OB_Y)):
    for i, (n, h) in enumerate(row):
        x = G_OX0 + G_ODX * i
        add(text(n, x, y - 5.0, JACK, FONT_BOLD, BG))
        if h:
            add(text(h, x + 4.6, y + 0.9, 2.1, FONT_MED, BG_DARK, anchor="start"))

save("Gatra.svg", W, parts)
haer_logo.add_to_file(os.path.join(HERE, "Gatra.svg"), x=106.0, y=9.9, cap=3.0, bg=BG)
print("ok")
