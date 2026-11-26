#!/usr/bin/env python3
# Draws res/Tapeizer.svg and writes src/layout.hpp (every component position, shared with src/Tapeizer.cpp).
# Rack can't draw SVG <text>, so labels become paths.
#   pip install fonttools ; python3 res/make_panel.py
import os, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 254.0, 128.5  # 50 HP

BG = "#2B2824"
BG_DARK = "#1B1916"
LINE = "#4C463F"
INK = "#F0E7D8"
SOFT = "#B7AC9C"
ACCENT = "#D9A441"   # tape-box amber

CONTROL = 3.2   # house standard: control labels 3.2 mm
JACK = 2.9      # jack labels 2.9 mm
SMALL = 2.1
HEAD = 2.4

# ---- LAYOUT ----
HEAD_Y = 14.0
DISP_Y = 21.5
R1, R2, R3, R4 = 37.0, 55.0, 73.0, 91.0
J1, J2 = 105.0, 118.5

# name: (kind, x, y, label)   kinds: knob, big, snap, bezel, jack, out, light, display
C = {}
def comp(name, kind, x, y, label=None): C[name] = (kind, x, y, label)

# DESK  (3 .. 38)
comp("TRIM", "knob", 20.5, R1, "TRIM")
comp("LOW", "knob", 12.0, R2, "LOW"); comp("LOW_FREQ", "knob", 29.0, R2, "LO FREQ")
comp("HIGH", "knob", 12.0, R3, "HIGH"); comp("HIGH_FREQ", "knob", 29.0, R3, "HI FREQ")
comp("DESK_ON", "bezel", 20.5, R4, "ON")
# MULTITRACK  (40 .. 112): three rows a little further apart than the rest of the panel
# (deck, record, noise reduction), display and ON along the bottom, VU beside the deck row.
MA, MB, MC = 33.0, 53.5, 74.0
comp("MULTI_MACHINE", "snap", 50.0, MA, "MACHINE"); comp("TAPE", "snap", 67.0, MA, "TAPE"); comp("BOUNCE", "snap", 84.0, MA, "BOUNCE")
comp("REC", "big", 50.0, MB, "REC"); comp("BIAS", "knob", 67.0, MB, "BIAS"); comp("CAL", "knob", 84.0, MB, "NR CAL")
comp("NR_TYPE", "snap", 50.0, MC, "NR TYPE"); comp("NR_ENC", "bezel", 67.0, MC, "NR ENC"); comp("NR_DEC", "bezel", 84.0, MC, "NR DEC")
comp("MULTI_DISP", "display", 60.5, R4)
comp("MULTI_ON", "bezel", 84.0, R4, "ON")
VU_M = 102.5
for i, n in enumerate(["PK", "+3", "0", "-5", "-10"]):
    comp("MVU%d" % i, "light", VU_M, 31.0 + i * 4.6, None)
# TAPE PATH  (114 .. 146)
comp("WOW", "knob", 122.0, R1, "WOW"); comp("FLUTTER", "knob", 138.5, R1, "FLUTTER")
comp("DROPOUT", "knob", 122.0, R2, "DROPOUT"); comp("HISS", "knob", 138.5, R2, "HISS")
comp("AZIMUTH", "knob", 122.0, R3, "AZIMUTH"); comp("WEAR", "knob", 138.5, R3, "WEAR")
# BUS  (148 .. 168)
comp("THRESH", "knob", 158.0, R1, "THRESH"); comp("RATIO", "knob", 158.0, R2, "RATIO")
comp("GR", "light", 158.0, R3 - 1.0, None)
comp("BUS_ON", "bezel", 158.0, R4, "ON")
# MASTER  (170 .. 212)
comp("MASTER_MACHINE", "snap", 179.0, R1, "MACHINE"); comp("MASTER_TAPE", "snap", 195.5, R1, "TAPE")
comp("MASTER_REC", "knob", 179.0, 58.0, "REC"); comp("MASTER_NR", "bezel", 195.5, 58.0, "NR B")
comp("MASTER_ON", "bezel", 179.0, R4, "ON")
VU_S, VU_S_Y = 205.0, 68.0   # master meter sits where the old name display was
for i, n in enumerate(["PK", "+3", "0", "-5", "-10"]):
    comp("SVU%d" % i, "light", VU_S, VU_S_Y + i * 4.6, None)
# GLOBAL  (214 .. 252)
comp("STRENGTH", "big", 233.0, R1, "STRENGTH"); comp("AGE", "big", 233.0, R2, "AGE")
comp("MIX", "knob", 224.0, R3, "MIX"); comp("OUTPUT", "knob", 242.0, R3, "OUTPUT")
comp("BYPASS", "bezel", 233.0, R4, "BYPASS")
# PRESET selector in the header: < [name screen] >
PRE_Y = 7.6
comp("PRESET_PREV", "button", 93.0, PRE_Y); comp("PRESET_NEXT", "button", 167.0, PRE_Y)
comp("PRESET_DISP", "display", 130.0, PRE_Y)
# jacks: inputs
for name, x, lab in [("IN_L", 10.0, "IN L"), ("IN_R", 21.0, "IN R"), ("TRACKS", 32.0, "TRACKS"), ("REC_CV", 50.0, "REC"),
                     ("WOW_CV", 122.0, "WOW"), ("AZIMUTH_CV", 138.5, "AZIM"), ("MASTER_IN_L", 172.0, "MST L"), ("MASTER_IN_R", 184.0, "MST R"),
                     ("MASTER_REC_CV", 196.0, "M REC"), ("STRENGTH_CV", 224.0, "STRENGTH"), ("AGE_CV", 242.0, "AGE")]:
    comp(name, "jack", x, J1, lab)
for name, x, lab in [("TRACKS_OUT", 67.0, "TRACKS"), ("MIX_L", 84.0, "MIX L"), ("MIX_R", 101.0, "MIX R"), ("OUT_L", 224.0, "OUT L"), ("OUT_R", 242.0, "OUT R")]:
    comp(name, "out", x, J2, lab)
DISP_W = {"MULTI_DISP": 35.0, "PRESET_DISP": 66.0}
BTN_W, BTN_H = 6.0, 6.0
DISP_H = 6.0
RADIUS = {"button": 3.0, "knob": 4.9, "big": 6.2, "snap": 4.9, "bezel": 3.7, "jack": 4.0, "out": 4.0, "light": 1.1}

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
add(text("Tapeizer", 5.0, 9.6, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02))
add(text("cheap tape, 80s and 90s: desk, multitrack, bus, master", W - 4.5, 9.2, 2.4, FONT_MED, ACCENT, anchor="end"))


def heading(s, x0, x1, y=HEAD_Y):
    add(text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12))
    tx = boxes[-1][3] + 1.4
    add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')


heading("DESK", 3.5, 38.0)
heading("MULTITRACK", 40.5, 111.5)
heading("TAPE PATH", 114.0, 146.0)
heading("BUS", 148.5, 167.5)
heading("MASTER", 170.0, 212.0)
heading("CHARACTER", 214.5, W - 3.5)

# section dividers
for x in (39.2, 112.8, 147.2, 168.8, 213.2):
    add(f'<line x1="{x}" y1="{HEAD_Y + 3}" x2="{x}" y2="{R4 + 6}" stroke="{LINE}" stroke-width="0.3"/>')

# position labels round snap knobs: name -> (labels, radius)
AROUND = {"TAPE": (["I", "II", "IV"], 7.4), "BOUNCE": (["1", "2", "3", "4"], 7.4), "NR_TYPE": (["OFF", "B", "C", "2:1"], 8.0),
          "MASTER_TAPE": (["I", "II", "IV"], 7.4), "MASTER_MACHINE": (["DECK", "BOOM", "1/4", "DAT"], 8.4)}
# Control names sit 1.7 mm above the knob (or above its top position labels), so each name is clearly
# closer to its own control than to the one above it. A row shares one baseline (the highest needed).
TOP = {"knob": 5.1, "snap": 5.1, "big": 6.4, "bezel": 3.9}
def name_base(name):
    kind, x, y, _ = C[name]
    top = y - TOP[kind]
    if name in AROUND:
        labs, r = AROUND[name]
        n = len(labs)
        for i in range(n):
            a = math.radians(-150.0 + 300.0 * i / (n - 1))
            if math.cos(a) > 0.3: top = min(top, y - r * math.cos(a) + SMALL * 0.36 - SMALL * 0.75)
    return top - 1.7
rows = {}
for name, (kind, x, y, label) in C.items():
    if label and kind in ("knob", "big", "snap", "bezel"):
        sec = sum(1 for d in (39.2, 112.8, 147.2, 168.8, 213.2) if x > d)
        rows.setdefault((sec, y), []).append(name)
for key, names in rows.items():
    base = min(name_base(n) for n in names)
    for n in names:
        _, x, y, label = C[n]
        add(text(label, x, base, CONTROL))

# displays: dark windows
for n in ("MULTI_DISP", "PRESET_DISP"):
    _, x, y, _ = C[n]; w = DISP_W[n]
    add(f'<rect x="{x - w / 2:.2f}" y="{y - DISP_H / 2:.2f}" width="{w}" height="{DISP_H}" rx="0.8" fill="{BG_DARK}" stroke="{LINE}" stroke-width="0.3"/>')

# snap knob position labels
def around(name, labels, r=7.4, size=SMALL, a0=-150.0, a1=150.0):
    _, x, y, _ = C[name]
    n = len(labels)
    for i, lab in enumerate(labels):
        a = math.radians(a0 + (a1 - a0) * i / (n - 1))
        tx, ty = x + r * math.sin(a), y - r * math.cos(a) + size * 0.36
        add(text(lab, tx, ty, size, FONT_BOLD, SOFT, anchor="middle", spacing=0.02))

for n, (labs, r) in AROUND.items():
    around(n, labs, r=r, size=1.9 if n == "MASTER_MACHINE" else SMALL)
add(text("PRESET", C["PRESET_PREV"][1] - BTN_W / 2 - 1.6, PRE_Y + 0.8, SMALL, FONT_BOLD, SOFT, anchor="end"))
# range labels under some knobs
def ends(name, lo, hi, dx=4.4, dy=6.6):
    _, x, y, _ = C[name]
    add(text(lo, x - dx, y + dy, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(text(hi, x + dx, y + dy, SMALL, FONT_BOLD, SOFT, anchor="start"))
ends("STRENGTH", "CLEAN", "FULL", 7.4, 4.2)
ends("AGE", "NEW", "TIRED", 7.4, 4.2)
ends("BIAS", "UNDER", "OVER", 2.6, 7.4)
ends("MIX", "DRY", "WET")

# VU captions
for xm, y0 in ((VU_M, 31.0), (VU_S, VU_S_Y)):
    add(text("REC VU", xm, y0 - 3.8, SMALL, FONT_BOLD, SOFT))
    for i, n in enumerate(["PK", "+3", "0", "-5", "-10"]):
        add(text(n, xm - 2.2, y0 + i * 4.6 + 0.75, 1.9, FONT_BOLD, SOFT, anchor="end", spacing=0.02))
add(text("GR", C["GR"][1], C["GR"][2] - 2.4, SMALL, FONT_BOLD, SOFT))

# jack rows
add(f'<rect x="2.0" y="{J1 - 8.4}" width="{W - 4.0}" height="{J2 - J1 + 13.0}" rx="1.4" fill="{BG_DARK}"/>')
for name, (kind, x, y, label) in C.items():
    if kind == "jack":
        cv = name.endswith("_CV")
        add(text(label, x, y - 5.4, JACK, FONT_BOLD, SOFT if cv else INK))
add(text("CV", 61.0, J1 + 1.0, SMALL, FONT_BOLD, SOFT, anchor="start"))
add(text("CV", 147.0, J1 + 1.0, SMALL, FONT_BOLD, SOFT, anchor="middle"))
for x0, x1 in ((58.5, 109.5), (215.0, 251.0)):
    add(f'<rect x="{x0}" y="{J2 - 8.2}" width="{x1 - x0}" height="12.4" rx="1.2" fill="{ACCENT}"/>')
for name, (kind, x, y, label) in C.items():
    if kind == "out":
        add(text(label, x, y - 5.2, JACK, FONT_BOLD, BG_DARK))
add(text("poly, up to 8", 32.0, J1 + 6.9, 1.8, FONT_MED, SOFT))
add(text("MASTER IN replaces the mix", 184.0, J1 + 6.9, 1.8, FONT_MED, SOFT))
add(text("TRACKS OUT patched:", 30.0, J2 - 0.4, 1.9, FONT_MED, SOFT))
add(text("tracks leave the mix", 30.0, J2 + 2.4, 1.9, FONT_MED, SOFT))


def comp_boxes():
    b = []
    for name, (kind, x, y, label) in C.items():
        if kind == "button":
            b.append((name, x - BTN_W / 2, y - BTN_H / 2, x + BTN_W / 2, y + BTN_H / 2)); continue
        if kind == "display":
            w = DISP_W[name]; b.append((name, x - w / 2, y - DISP_H / 2, x + w / 2, y + DISP_H / 2)); continue
        r = RADIUS[kind]
        b.append((name, x - r, y - r, x + r, y + r))
    return b


allb = boxes + comp_boxes()
bad = []
for i in range(len(allb)):
    for j in range(i + 1, len(allb)):
        a, b = allb[i], allb[j]
        if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
            bad.append((a[0], b[0]))
for a in allb:
    if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
if bad:
    print("OVERLAPS:", bad)

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Tapeizer.svg"), "w").write(svg)

# src/layout.hpp
lines = ["// Generated by res/make_panel.py. Do not edit.", "#pragma once", "namespace layout {"]
for name, (kind, x, y, label) in C.items():
    lines.append(f"static const float {name}_X = {x:.3f}f, {name}_Y = {y:.3f}f;")
for n, w in DISP_W.items():
    lines.append(f"static const float {n}_W = {w:.3f}f, {n}_H = {DISP_H:.3f}f;")
lines.append("}")
open(os.path.join(HERE, "..", "src", "layout.hpp"), "w").write("\n".join(lines) + "\n")
import json
json.dump({"W": W, "H": H, "comps": C, "disp_w": DISP_W, "disp_h": DISP_H}, open(os.path.join(HERE, "..", "tools", "layout.json"), "w"), indent=0)
print("ok")

import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Tapeizer.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
