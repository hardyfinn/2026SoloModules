#!/usr/bin/env python3
# Draws res/Comper.svg (38 HP, 2.3.0) and writes src/ComperLayout.hpp with the same numbers.
# Rack can't draw SVG <text>, so labels become paths. Positions are in mm.
# Labels follow the house standard: controls 3.2 mm, jacks 2.9 mm.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 193.04, 128.5  # 38 HP

BG = "#23262B"
BG_DARK = "#16181C"
LINE = "#3D424A"
INK = "#EFE9DD"
SOFT = "#A8A397"
ACCENT = "#D9A441"

CONTROL, JACK, SMALL, HEAD = 3.2, 2.9, 2.2, 3.2

# ---- LAYOUT (exported to src/ComperLayout.hpp) ----
DISP = (4.0, 12.2, W - 8.0, 26.0)        # x y w h
KX0, KDX = 9.6, (W - 19.2) / 12          # 13 knob columns
Y_HEAD = 45.0
RY = [55.6, 71.6, 86.0]                  # three knob rows
LOOSE_DY = 1.0                           # the big knob sits a little low
JX0, JDX = 8.4, (W - 17.6) / 12          # 13 jack columns
Y_IN1, Y_IN2 = 104.8, 119.2

def kc(i): return KX0 + i * KDX
def jc(i): return JX0 + i * JDX

# control kinds: knob (RoundBlackKnob), small (RoundSmallBlackKnob), large, trim, switch3, button, latch, light
GRID = [
    ("STEPS", 0, 0, "small"), ("BEATS", 0, 1, "small"), ("KEY", 0, 2, "small"),
    ("EDIT_ROOT", 1, 0, "small"), ("EDIT_QUAL", 1, 1, "small"),
    ("STYLE", 2, 0, "knob"), ("SPREAD", 3, 0, "knob"), ("VOICES", 4, 0, "knob"),
    ("LEAD", 2, 1, "knob"), ("REGISTER", 3, 1, "knob"), ("OUT", 4, 1, "knob"),
    ("ROOTS", 2, 2, "switch3"), ("OCTAVE", 3, 2, "knob"),
    ("GROOVE", 5, 0, "knob"), ("SWING", 6, 0, "knob"), ("DENSITY", 7, 0, "knob"),
    ("REPEAT", 5, 1, "knob"), ("PUSH", 6, 1, "knob"), ("LENGTH", 7, 1, "knob"),
    ("SEED", 5, 2, "knob"), ("NEW", 6, 2, "button"),
    ("LOOSE", 8, 0, "large"), ("TIMING", 9, 0, "trim"), ("ROLL", 10, 0, "trim"),
    ("PEDAL", 8, 1, "knob"), ("WEIGHT", 9, 1, "trim"), ("SLOP", 10, 1, "trim"),
    ("LISTEN", 11, 0, "knob"), ("TEMPO", 12, 0, "knob"), ("RUN", 11, 1, "latch"),
    ("HIT", 11, 2, "light"), ("SOLO", 12, 2, "light"),
]
LABEL = {"EDIT_ROOT": "ROOT", "EDIT_QUAL": "QUAL", "OUT": "AUTRE"}
RADIUS = {"knob": 4.6, "small": 4.0, "large": 6.4, "trim": 3.1, "button": 3.0, "latch": 3.2, "light": 1.5}

def pos(name):
    for n, c, r, k in GRID:
        if n == name:
            return kc(c), RY[r] + (LOOSE_DY if k == "large" else 0.0), k
    raise KeyError(name)

IN1 = ["CLOCK", "RESET", "RUN", "KEY", "ROOT", "QUAL", "CHORD_VOCT", "CHORD_GATE"]
IN2 = ["SOLO", "SOLO_VOCT", "DENSITY", "OUT", "LOOSE", "LISTEN", "REGISTER"]
OUT1 = ["VOCT", "GATE", "VEL", "PEDAL", "SCALE"]
OUT2 = ["BASS_VOCT", "BASS_GATE", "CHANGE", "EOC"]
JLABEL = {"CHORD_VOCT": "CHORD\nV/OCT", "CHORD_GATE": "CHORD\nGATE", "SOLO_VOCT": "SOLO\nV/OCT", "OUT": "AUTRE",
          "VOCT": "V/OCT", "BASS_VOCT": "BASS\nV/OCT", "BASS_GATE": "BASS\nGATE"}
OUT_COL0 = 8

fonts = {}
boxes = []

def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.02, check=True):
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
        boxes.append((s, x0, y - size * 0.73, x0 + width * scale, y + size * 0.02))
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(text("Comper", 5.0, 9.4, 5.4, FONT_BOLD, INK, anchor="start"))
add(text("jazz piano comping player · notes out", W - 5.0, 9.0, 2.6, FONT_MED, ACCENT, anchor="end", spacing=0.0))

dx, dy, dw, dh = DISP
add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="1.2" fill="#0E1013" stroke="{LINE}" stroke-width="0.3"/>')

def heading(s, x0, x1, y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.1))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 1.1:.2f}" x2="{x1:.2f}" y2="{y - 1.1:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

heading("CHART", kc(0) - 5.0, kc(1) + 5.4, Y_HEAD)
heading("VOICING", kc(2) - 5.4, kc(4) + 5.4, Y_HEAD)
heading("RHYTHM", kc(5) - 5.4, kc(7) + 5.4, Y_HEAD)
heading("HANDS", kc(9) - 5.0, kc(10) + 5.4, Y_HEAD)
heading("BAND", kc(11) - 5.4, kc(12) + 5.0, Y_HEAD)

for n, c, r, k in GRID:
    x, y, _ = pos(n)
    lab = LABEL.get(n, n)
    rad = RADIUS.get(k, 4.6)
    if k == "large":
        add(text(lab, x, Y_HEAD, CONTROL))
    elif k == "switch3":
        add(text(lab, x, y - 6.0, CONTROL))
    elif k == "light":
        add(text(lab, x, y - 3.2, CONTROL))
    else:
        add(text(lab, x, y - rad - 1.5, CONTROL))

x, y, _ = pos("ROOTS"); add(text("OFF·SOME·ALL", x, y + 6.6, 1.9, FONT_BOLD, SOFT, check=False))
x, y, _ = pos("OCTAVE"); add(text("−3", x - 5.4, y + 5.6, SMALL, FONT_BOLD, SOFT, check=False)); add(text("+3", x + 5.4, y + 5.6, SMALL, FONT_BOLD, SOFT, check=False))
x, y, _ = pos("LENGTH"); add(text("SHORT", x - 2.4, y + 6.6, 1.9, FONT_BOLD, SOFT, anchor="end", check=False)); add(text("LEGATO", x + 2.4, y + 6.6, 1.9, FONT_BOLD, SOFT, anchor="start", check=False))
x, y, _ = pos("KEY"); add(text("± 6 ST", x, y + 6.6, 1.9, FONT_BOLD, SOFT, check=False))
x, y, _ = pos("TEMPO"); add(text("BPM", x, y + 7.0, 1.9, FONT_BOLD, SOFT, check=False))
add(text("EDIT", kc(1), RY[2] - 1.2, CONTROL, FONT_BOLD, ACCENT))
add(text("TAPPED STEP", kc(1), RY[2] + 2.4, 1.9, FONT_BOLD, SOFT, check=False))
add(text("TRIMS × LOOSE", (kc(9) + kc(10)) / 2, RY[2] - 0.4, SMALL, FONT_BOLD, SOFT))

# jacks
top = Y_IN1 - 10.2
add(f'<rect x="2.0" y="{top:.2f}" width="{W - 4.0:.2f}" height="{Y_IN2 - top + 3.9:.2f}" rx="1.4" fill="{BG_DARK}"/>')
ox0 = jc(OUT_COL0) - 6.6
add(f'<rect x="{ox0:.2f}" y="{top + 0.6:.2f}" width="{W - 3.2 - ox0:.2f}" height="{Y_IN2 - top + 2.7:.2f}" rx="1.4" fill="{ACCENT}"/>')
def jlabel(n, x, y, fill):
    lines = JLABEL.get(n, n).split("\n")
    if len(lines) == 1: add(text(lines[0], x, y - 5.0, JACK, FONT_BOLD, fill))
    else:
        add(text(lines[0], x, y - 7.7, JACK, FONT_BOLD, fill)); add(text(lines[1], x, y - 4.8, JACK, FONT_BOLD, fill))
for i, n in enumerate(IN1): jlabel(n, jc(i), Y_IN1, INK)
for i, n in enumerate(IN2): jlabel(n, jc(i), Y_IN2, INK if i < 2 else SOFT)
for i, n in enumerate(OUT1): jlabel(n, jc(OUT_COL0 + i), Y_IN1, BG_DARK)
for i, n in enumerate(OUT2): jlabel(n, jc(OUT_COL0 + i), Y_IN2, BG_DARK)

def comp_boxes():
    b = []
    def c(x, y, r, name): b.append((name, x - r, y - r, x + r, y + r))
    for n, col, row, k in GRID:
        x, y, _ = pos(n)
        if k == "switch3": b.append((n, x - 2.0, y - 4.6, x + 2.0, y + 4.6))
        else: c(x, y, RADIUS.get(k, 4.6), n)
    for i in range(len(IN1)): c(jc(i), Y_IN1, 3.8, "in1")
    for i in range(len(IN2)): c(jc(i), Y_IN2, 3.8, "in2")
    for i in range(len(OUT1)): c(jc(OUT_COL0 + i), Y_IN1, 3.8, "out1")
    for i in range(len(OUT2)): c(jc(OUT_COL0 + i), Y_IN2, 3.8, "out2")
    b.append(("display", dx, dy, dx + dw, dy + dh))
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

# ---- layout header for src/Comper.cpp
hdr = ["// Generated by res/make_panel.py. Positions in mm.", "#pragma once", "namespace comperlayout {",
       f"static const float DISP_X = {dx}f, DISP_Y = {dy}f, DISP_W = {dw:.3f}f, DISP_H = {dh}f;"]
for n, col, row, k in GRID:
    x, y, _ = pos(n)
    hdr.append(f"static const float {n}_X = {x:.3f}f, {n}_Y = {y:.3f}f;")
for row, names, y in [(1, IN1, Y_IN1), (2, IN2, Y_IN2)]:
    for i, n in enumerate(names):
        hdr.append(f"static const float IN_{n}_X = {jc(i):.3f}f, IN_{n}_Y = {y:.3f}f;")
for names, y in [(OUT1, Y_IN1), (OUT2, Y_IN2)]:
    for i, n in enumerate(names):
        hdr.append(f"static const float OUT_{n}_X = {jc(OUT_COL0 + i):.3f}f, OUT_{n}_Y = {y:.3f}f;")
hdr.append("}")
open(os.path.join(HERE, "..", "src", "ComperLayout.hpp"), "w").write("\n".join(hdr) + "\n")
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Comper.svg"), x=W / 2, y=126.6, cap=2.8, bg=BG)
