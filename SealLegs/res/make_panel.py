#!/usr/bin/env python3
# Draws res/SealLegs.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/SealLegs.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import math, os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "SealLegs"
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

HP = 40                       # widen HP rather than cram
W, H = HP * 5.08, 128.5

BG = "#BCC9C6"        # sea-glass grey
BG_DARK = "#1F2F36"
INK = "#17242A"
SOFT = "#4B5E66"
LIGHT_INK = "#E4EDEA"
ACCENT = "#1F7A83"

# ---- LAYOUT (same numbers in src/SealLegs.cpp) ----
SX = [9.5, 21.0, 32.5, 44.0, 55.5, 67.0, 78.5, 90.0]
SLIDER_Y, TRIM_Y, CV_Y = 43.0, 63.0, 73.5
J1_Y, J2_Y = 96.0, 114.0
RX = [107.5, 123.0, 138.5, 154.0, 169.5, 185.0]
R1_Y, R2_Y, R3_Y, R4_Y, R5_Y = 28.0, 47.5, 66.0, 84.5, 103.0
SW_DX = -2.6

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
OPT = 2.4       # switch position labels

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


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<rect x="0" y="0" width="{W}" height="2.2" fill="{BG_DARK}"/>')
add(text("SEAL LEGS", 52.0, 11.5, 5.2, FONT_BOLD, INK, spacing=0.08))
add(text("STEREO CHARACTER DELAY  ·  TAPE / BBD / DIGI", 52.0, 16.0, 2.4, FONT_MED, SOFT))

# --- left block: sliders ---
for x0, x1, lab in [(SX[0], SX[2], "DELAY"), (SX[3], SX[4], "MOD"), (SX[5], SX[7], "TONE")]:
    add(f'<rect x="{x0 - 4.5}" y="22.0" width="{x1 - x0 + 9.0}" height="0.3" fill="{SOFT}"/>')
    add(text(lab, (x0 + x1) / 2, 21.0, 2.4, FONT_MED, SOFT, spacing=0.1))
for x, lab in zip(SX, ["TIME", "WIDTH", "FB", "RATE", "DEPTH", "COLOR", "HP", "LP"]):
    add(text(lab, x, 27.3, CONTROL, fill=ACCENT if lab == "COLOR" else INK))
add(text("CV", 3.6, TRIM_Y + 0.9, 2.2, FONT_MED, SOFT))
add(text("IN", 3.6, CV_Y + 0.9, 2.2, FONT_MED, SOFT))

# --- left block: jack field ---
add(f'<rect x="2.5" y="{J1_Y - 9.0}" width="{SX[7] - SX[0] + 14.0}" height="{J2_Y - J1_Y + 14.5}" rx="1.4" fill="{BG_DARK}"/>')
for x, lab in zip(SX, ["IN L", "IN R", "FREEZE", "REV", "CLK", "TAP", "CLEAR", "MUT"]):
    add(text(lab, x, J1_Y - 5.6, JACK, fill=LIGHT_INK))
add(text("AUX", SX[0], J2_Y - 5.6, JACK, fill=LIGHT_INK))
add(text("ATT", SX[1], J2_Y - 5.6, JACK, fill=LIGHT_INK))
dx = SX[2] + 5.0
KA = 0.83 * 180   # Rack knob travel is +-0.83 pi
add(text("AUX TO", dx, J2_Y - 8.4, JACK, fill=LIGHT_INK))
for i, lab in enumerate(["DRV", "NOISE", "SHAPE", "FOG", "MIX"]):
    a = math.radians(-KA + KA / 2 * i)
    r = 5.6
    x, y = dx + r * math.sin(a), J2_Y - r * math.cos(a) + 0.75
    anchor = "end" if i < 2 else ("start" if i > 2 else "middle")
    add(text(lab, x + (-0.3 if i < 2 else (0.3 if i > 2 else 0)), y + (0.6 if i in (0, 4) else 0), 2.0, FONT_MED, LIGHT_INK, anchor=anchor))
for x, lab in [(SX[5], "MOD"), (SX[6], "OUT L"), (SX[7], "OUT R")]:
    add(f'<rect x="{x - 5.4}" y="{J2_Y - 9.0}" width="10.8" height="14.0" rx="1.2" fill="{BG}"/>')
    add(text(lab, x, J2_Y - 5.4, JACK, fill=INK))

# --- right block ---
add(f'<rect x="{RX[0] - 8.5}" y="18.0" width="0.3" height="{J2_Y - 18.0 + 6.0}" fill="{SOFT}"/>')


def switch3(col, y, title, opts, dx=SW_DX):
    x = RX[col]
    add(text(title, x, y - 7.2, CONTROL))
    for dy, lab in zip([-3.3, 0.0, 3.3], opts):
        add(text(lab, x + dx + 2.9, y + dy + 0.85, OPT, FONT_MED, SOFT, anchor="start"))


def switch2(col, y, title, opts, dx=SW_DX, fill=INK, ty=6.0):
    x = RX[col]
    add(text(title, x, y - ty, CONTROL, fill=fill))
    for dy, lab in zip([-1.9, 1.9], opts):
        add(text(lab, x + dx + 2.9, y + dy + 0.85, OPT, FONT_MED, SOFT, anchor="start"))


switch3(0, R1_Y, "MODEL", ["TAPE", "BBD", "DIGI"])
switch3(1, R1_Y, "RANGE", ["LONG", "MED", "SHORT"])
switch3(2, R1_Y, "STEREO", ["PONG", "T:W", "L/R"])
switch3(3, R1_Y, "SPEED", ["×2", "×1", "×½"])
switch3(4, R1_Y, "REVERSE", ["OFF", "REV", "MIX"])
switch2(5, R1_Y, "CHANGE", ["GLIDE", "JUMP"], ty=7.2)

for col, lab in enumerate(["DRIVE", "NOISE", "N MODE", "FOG", "DECAY", "MIX"]):
    add(text(lab, RX[col], R2_Y - 7.6, CONTROL))
# noise-mode ends
add(text("PRE", RX[2] - 5.0, R2_Y + 6.6, 2.0, FONT_MED, SOFT, anchor="middle"))
add(text("POST", RX[2] + 5.0, R2_Y + 6.6, 2.0, FONT_MED, SOFT, anchor="middle"))

switch3(0, R3_Y, "DRV POS", ["IN", "DLY", "MIX"])
add(text("FOG POS", RX[1], R3_Y - 7.2, CONTROL))
for i, lab in enumerate(["IN", "DLY", "MIX", "LOOP"]):
    a = math.radians(-KA + 2 * KA / 3 * i)
    r = 5.4
    x, y = RX[1] + r * math.sin(a), R3_Y - r * math.cos(a) + 0.75
    anchor = "end" if i < 2 else "start"
    add(text(lab, x + (-0.3 if i < 2 else 0.3), y + (0.6 if i in (0, 3) else 0), 2.0, FONT_MED, SOFT, anchor=anchor))
add(text("DUCK", RX[2], R3_Y - 7.6, CONTROL))
add(text("SHAPE", RX[3], R3_Y - 7.6, CONTROL))
add(text("W&F", RX[3] - 4.2, R3_Y + 7.4, 2.0, FONT_MED, SOFT, anchor="middle"))
add(text("RAMP", RX[3] + 4.2, R3_Y + 7.4, 2.0, FONT_MED, SOFT, anchor="middle"))
switch2(4, R3_Y, "T-STOP", ["ON", "OFF"])
add(text("CLEAR", RX[5], R3_Y - 6.0, CONTROL))

for col, lab in enumerate(["FREEZE", "D SYNC", "M SYNC", "FB X", "M INV", "TAP"]):
    add(text(lab, RX[col], R4_Y - 6.4, CONTROL))

add(f'<rect x="{RX[1] + 1.0}" y="{R5_Y - 12.0}" width="{RX[5] - RX[1] - 2.0}" height="20.0" rx="1.4" fill="none" stroke="{ACCENT}" stroke-width="0.35"/>')
add(text("MUTATE", RX[2], R5_Y - 7.6, CONTROL, fill=ACCENT))
add(text("MUT", RX[3], R5_Y - 6.0, CONTROL, fill=ACCENT))
switch2(4, R5_Y, "LOCK", ["ON", "OFF"], fill=ACCENT)

out = os.path.join(HERE, f"{NAME}.svg")
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(out, "w").write(svg)

# Haer maker logo on the bottom rail (required on every panel)
sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(out, x=W / 2, y=125.6, cap=3.0, bg=BG)
print("ok", out)
