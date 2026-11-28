#!/usr/bin/env python3
# Draws res/Breaker.svg and writes src/Layout.hpp (every position the C++ needs).
# Rack can't draw SVG <text>, so labels are paths.
#   pip install fonttools ; python3 res/make_panel.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_BLACK = os.environ.get("PANEL_FONT_BLACK", "/usr/share/fonts/opentype/inter/Inter-Black.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
BG = "#16171B"; PLATE = "#24201C"; INK = "#F2EFE6"; DIM = "#9A9A9A"; ACC = "#FF7A1A"
LANE = {"K": "#FF4D4D", "S": "#FFD23F", "H": "#46D3FF", "O": "#B98CFF"}
CONTROL, JACK, SMALL = 3.2, 2.9, 2.4
H = 128.5; HP = 36; W = round(HP * 5.08, 2)
fonts = {}

def text(s, x, y, size, f=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    if f not in fonts:
        t = TTFont(f); fonts[f] = (t.getGlyphSet(), t.getBestCmap(), t["head"].unitsPerEm)
    gs, cmap, upm = fonts[f]
    sc = size / upm; tr = spacing * upm
    names = [cmap[ord(c)] for c in s]
    w = sum(gs[n].width + tr for n in names) - tr
    x0 = {"middle": x - w * sc / 2, "end": x - w * sc}.get(anchor, x)
    pen = SVGPathPen(gs); cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (sc, 0, 0, -sc, x0 + cx * sc, y))); cx += gs[n].width + tr
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

L = []   # layout lines
def put(name, x, y): L.append(f"static const float {name}_X = {x}f, {name}_Y = {y}f;")

p = [f'<rect width="{W}" height="{H}" fill="{BG}"/>', f'<rect width="{W}" height="1.6" fill="{ACC}"/>']
p.append(text("BREAKER", 5.0, 11.2, 7.0, FONT_BLACK, INK, anchor="start", spacing=0.08))
p.append(text("break analyser + drum pattern generator", 52.0, 10.6, 2.6, FONT_MED, DIM, anchor="start", spacing=0.02))

# displays (drawn by the module; the panel gives them a frame)
WAVE = (5.0, 14.0, round(W - 10, 2), 24.0)
GRID = (5.0, 40.0, round(W - 10, 2), 23.8)
for x, y, w, h in (WAVE, GRID):
    p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.3" fill="#0B0C0F" stroke="#333" stroke-width="0.25"/>')
L.append(f"static const float WAVE_X = {WAVE[0]}f, WAVE_Y = {WAVE[1]}f, WAVE_W = {WAVE[2]}f, WAVE_H = {WAVE[3]}f;")
L.append(f"static const float GRID_X = {GRID[0]}f, GRID_Y = {GRID[1]}f, GRID_W = {GRID[2]}f, GRID_H = {GRID[3]}f;")

cols = [round(16 + (W - 32) * i / 5, 3) for i in range(6)]
big = ["STYLE", "DENSITY", "MUTATE", "GHOSTS", "FILLS", "SWING"]
small = ["SENS", "BARS", "PITCH", "DECAY", "VARIETY", "LEVEL"]
for x, lab in zip(cols, big):
    p.append(f'<circle cx="{x}" cy="70.5" r="5.9" fill="none" stroke="{ACC}" stroke-width="0.45"/>')
    p.append(text(lab, x, 70.5 + 9.4, CONTROL)); put("K_" + lab, x, 70.5)
for x, lab in zip(cols, small):
    p.append(text(lab, x, 86.0 + 8.3, CONTROL)); put("K_" + lab, x, 86.0)
# scale marks for STYLE (5 positions) and BARS (3)
import math
def ticks(x, y, r, n, labels=None):
    out = []
    for i in range(n):
        a = math.radians(-135 + 270 * i / (n - 1))
        x1, y1 = x + math.sin(a) * (r + 0.4), y - math.cos(a) * (r + 0.4)
        x2, y2 = x + math.sin(a) * (r + 1.4), y - math.cos(a) * (r + 1.4)
        out.append(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{DIM}" stroke-width="0.35"/>')
        if labels and labels[i]:
            lx, ly = x + math.sin(a) * (r + 3.2), y - math.cos(a) * (r + 3.2) + 0.8
            out.append(text(labels[i], lx, ly, 2.0, FONT_BOLD, DIM))
    return out
p += ticks(cols[0], 70.5, 6.3, 5)
p += ticks(cols[1], 86.0, 4.6, 3, ["1", "", "4"])

# inputs
ins = [("CLOCK", "RESET", "NEW", "LOCK"), ("MUTATE", "STYLE", "DENS", "V/OCT")]
for r, row in enumerate(ins):
    y = 105.0 + r * 14.0
    for i, lab in enumerate(row):
        x = 11.0 + i * 14.0
        p.append(text(lab, x, y - 5.6, JACK, FONT_BOLD, INK)); put("I_" + lab.replace("/", ""), x, y)
# buttons
for lab, x, y in (("NEW", 68.5, 105.0), ("LOCK", 68.5, 119.0), ("LOAD", 81.5, 105.0), ("RUN", 81.5, 119.0)):
    p.append(text(lab, x, y - 5.0, CONTROL)); put("B_" + lab, x, y)
# outputs on a plate
p.append(f'<rect x="88.5" y="96.2" width="{W - 92.0}" height="28.4" rx="1.6" fill="{PLATE}" stroke="{ACC}" stroke-width="0.3"/>')
outs = [("KICK", "K"), ("SNARE", "S"), ("HAT", "H"), ("OTHER", "O")]
for i, (lab, t) in enumerate(outs):
    x = round(95.5 + i * 14.2, 2)
    for y, l2 in ((105.0, lab), (119.0, t + " GATE")):
        p.append(f'<rect x="{x - 4.4}" y="{y - 4.4}" width="8.8" height="8.8" rx="1.3" fill="{LANE[t]}"/>')
        p.append(text(l2, x, y - 5.6, JACK, FONT_BOLD, INK))
    put("O_" + lab, x, 105.0); put("O_" + t + "GATE", x, 119.0)
for lab, x, y, c in (("L", 155.5, 105.0, ACC), ("R", 170.0, 105.0, ACC), ("ACCENT", 155.5, 119.0, "#CCCCCC"), ("CLK", 170.0, 119.0, "#CCCCCC")):
    p.append(f'<rect x="{x - 4.4}" y="{y - 4.4}" width="8.8" height="8.8" rx="1.3" fill="{c}"/>')
    p.append(text(lab, x, y - 5.6, JACK, FONT_BOLD, INK)); put("O_" + lab, x, y)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p) + "\n</svg>\n")
path = os.path.join(HERE, "Breaker.svg")
open(path, "w").write(svg)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(path, x=W - 17.0, y=9.4, cap=3.6, bg=BG)
open(os.path.join(HERE, "..", "src", "Layout.hpp"), "w").write(
    "// Generated by res/make_panel.py - do not edit by hand.\n#pragma once\nnamespace layout {\n"
    + f"static const float PANEL_W = {W}f;\n" + "\n".join(L) + "\n}\n")
print("ok", W)
