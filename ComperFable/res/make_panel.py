#!/usr/bin/env python3
# Draws res/Comper.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Comper.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 182.88, 128.5  # 36 HP

BG = "#23262B"
BG_DARK = "#16181C"
LINE = "#3D424A"
INK = "#EFE9DD"
SOFT = "#A8A397"
ACCENT = "#D9A441"

# ---- LAYOUT (same numbers in src/Comper.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 4.0, 13.0, 118.0, 16.0
SMALLK_X = [134.0, 153.0, 172.0]   # STEPS BEATS KEY
SMALLK_Y = 23.0
STEP_X0, STEP_DX = 17.5, 10.25
Y_STEPLIGHT = 34.8
Y_ROOT = 40.6
Y_QUAL = 48.4
Y_STEPNAME = 54.6
COL_X0, COL_DX = 9.0, 11.06       # 16 columns for knobs and jacks
Y_ROWA = 73.0
Y_ROWB = 89.0
Y_IN1 = 104.6
Y_IN2 = 117.6
OUT_COL0 = 11                      # outputs in columns 11..15

CONTROL = 2.2
JACK = 2.15
SMALL = 1.9
HEAD = 2.4

def col(i): return COL_X0 + i * COL_DX
def step(i): return STEP_X0 + i * STEP_DX

fonts = {}
boxes = []

def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, check=True):
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
    if check:
        boxes.append((s, x0, y - size * 0.75, x0 + width * scale, y + size * 0.05))
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Comper", 5.0, 9.6, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("jazz piano comping player · notes out", W - 5.0, 9.2, 2.4, FONT_MED, ACCENT, anchor="end"))

def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

# display window
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')
for x, n in zip(SMALLK_X, ["STEPS", "BEATS", "KEY"]):
    add(text(n, x, SMALLK_Y - 6.0, CONTROL))
add(text("PER STEP", SMALLK_X[1], SMALLK_Y + 7.6, SMALL, FONT_BOLD, SOFT))
add(text("± 6 ST", SMALLK_X[2], SMALLK_Y + 7.6, SMALL, FONT_BOLD, SOFT))

# chart
add(text("CHART", 3.2, Y_STEPLIGHT + 0.8, SMALL, FONT_BOLD, ACCENT, anchor="start", spacing=0.1))
add(text("ROOT", 3.2, Y_ROOT + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("QUAL", 3.2, Y_QUAL + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
for i in range(16):
    x = step(i)
    if i % 4 == 0 and i:
        add(f'<line x1="{x - STEP_DX / 2:.2f}" y1="{Y_STEPLIGHT - 2.5:.2f}" x2="{x - STEP_DX / 2:.2f}" y2="{Y_STEPNAME + 1.0:.2f}" stroke="{LINE}" stroke-width="0.3"/>')
    add(text(str(i + 1), x + 2.6, Y_STEPLIGHT + 0.8, 1.7, FONT_BOLD, SOFT, anchor="start", check=False))
add(f'<line x1="3.0" y1="{Y_STEPNAME + 2.6:.2f}" x2="{W - 3.0:.2f}" y2="{Y_STEPNAME + 2.6:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

# knob band
heading("VOICING", col(0) - 5.0, col(3) + 4.6, 60.6)
heading("RHYTHM", col(4) - 4.6, col(7) + 4.6, 60.6)
heading("HANDS", col(8) - 4.6, col(11) + 4.6, 60.6)
heading("BAND", col(12) - 4.6, col(15) + 4.0, 60.6)
ROWA = ["STYLE", "SPREAD", "REGISTER", "VOICES", "GROOVE", "DENSITY", "SWING", "REPEAT", "LOOSE", "TIMING", "ROLL", "WEIGHT", "LISTEN", "TEMPO", "RUN", ""]
ROWB = ["LEAD", "OUT", "ROOTS", "", "PUSH", "SEED", "NEW", "", "SLOP", "PEDAL", "", "", "HIT", "SOLO", "", ""]
for i, n in enumerate(ROWA):
    if n: add(text(n, col(i), Y_ROWA - (8.6 if n == "LOOSE" else 6.2), CONTROL))
for i, n in enumerate(ROWB):
    if n: add(text(n, col(i), Y_ROWB - 6.2, CONTROL))
# ranges under some knobs
for i, lo, hi in [(1, "CLOSE", "OPEN"), (8, "MACHINE", "LOOSE")]:
    pass
add(text("OFF·SOME·ALL", col(2), Y_ROWB + 7.4, 1.7, FONT_BOLD, SOFT))
add(text("TRIMS", col(10) + 0.5, Y_ROWB - 1.2, 1.7, FONT_BOLD, SOFT, anchor="middle"))
add(text("× LOOSE", col(10) + 0.5, Y_ROWB + 1.2, 1.7, FONT_BOLD, SOFT, anchor="middle"))
add(text("BPM", col(13) + 5.4, Y_ROWA + 5.6, 1.7, FONT_BOLD, SOFT, anchor="start"))

# jacks
add(f'<rect x="2.0" y="{Y_IN1 - 9.6}" width="{W - 4.0}" height="{Y_IN2 - Y_IN1 + 14.6}" rx="1.4" fill="{BG_DARK}"/>')
IN1 = ["CLOCK", "RESET", "RUN", "KEY", "ROOT", "QUAL", "CHORD\nV/OCT", "CHORD\nGATE", "SOLO", "SOLO\nV/OCT"]
IN2 = ["DENSITY", "OUT", "LOOSE", "LISTEN", "REGISTER"]
def jlabel(n, x, y, fill):
    lines = n.split("\n")
    if len(lines) == 1: add(text(n, x, y - 5.2, JACK if len(n) < 7 else SMALL, FONT_BOLD, fill))
    else:
        add(text(lines[0], x, y - 7.3, SMALL, FONT_BOLD, fill)); add(text(lines[1], x, y - 5.1, SMALL, FONT_BOLD, fill))
for i, n in enumerate(IN1): jlabel(n, col(i), Y_IN1, INK)
for i, n in enumerate(IN2): jlabel(n, col(i), Y_IN2, SOFT)
add(text("CV IN", col(5) - 3.6, Y_IN2 + 0.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("POLY = CHORD JACKS, OUTPUTS V/OCT GATE VEL SCALE", col(6) - 5.0, Y_IN2 + 5.6, 1.6, FONT_MED, SOFT, anchor="start", check=False))
ox0 = col(OUT_COL0) - 5.2
add(f'<rect x="{ox0:.2f}" y="{Y_IN1 - 9.0}" width="{W - 3.2 - ox0:.2f}" height="{Y_IN2 - Y_IN1 + 13.4}" rx="1.4" fill="{ACCENT}"/>')
OUT1 = ["V/OCT", "GATE", "VEL", "PEDAL", "SCALE"]
OUT2 = ["BASS\nV/OCT", "BASS\nGATE", "CHANGE", "EOC", ""]
for i, n in enumerate(OUT1): jlabel(n, col(OUT_COL0 + i), Y_IN1, BG_DARK)
for i, n in enumerate(OUT2):
    if n: jlabel(n, col(OUT_COL0 + i), Y_IN2, BG_DARK)

def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for x in SMALLK_X: c(x, SMALLK_Y, 4.0, "small knob")
    for i in range(16):
        c(step(i), Y_STEPLIGHT, 1.1, "step light"); c(step(i), Y_ROOT, 3.1, "root"); c(step(i), Y_QUAL, 3.1, "qual")
    for i, n in enumerate(ROWA):
        if not n: continue
        r = 6.4 if n == "LOOSE" else (4.0 if n == "RUN" else 3.1 if n in ("TIMING", "ROLL", "WEIGHT") else 4.6)
        c(col(i), Y_ROWA, r, n)
    for i, n in enumerate(ROWB):
        if not n: continue
        r = {"ROOTS": 3.4, "NEW": 3.0, "HIT": 1.2, "SOLO": 1.2, "SLOP": 3.1}.get(n, 4.6)
        if n == "ROOTS": b.append(("ROOTS switch", col(i) - 2.0, Y_ROWB - 4.6, col(i) + 2.0, Y_ROWB + 4.6))
        else: c(col(i), Y_ROWB, r, n)
    for i in range(10): c(col(i), Y_IN1, 4.0, "in")
    for i in range(5): c(col(i), Y_IN2, 4.0, "cv in")
    for i in range(5): c(col(OUT_COL0 + i), Y_IN1, 4.0, "out")
    for i in range(4): c(col(OUT_COL0 + i), Y_IN2, 4.0, "out2")
    b.append(("display", DISP_X, DISP_Y, DISP_X + DISP_W, DISP_Y + DISP_H))
    return b

allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in boxes:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Comper.svg"), "w").write(svg)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Comper.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
