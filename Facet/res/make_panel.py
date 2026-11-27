#!/usr/bin/env python3
# Draws res/Facet.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/Facet.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 284.48, 128.5  # 56 HP

BG = "#2B2D31"
BAND = "#35383E"
INK = "#ECEAE4"
SOFT = "#A3A6AC"
ACC = "#7FD3C8"     # section names
IOBG = "#24262A"
TONE_COL = ["#E0533D", "#F3B04A", "#4FB3A9", "#7B8CDE"]

# ---- LAYOUT (same numbers in src/Facet.cpp) ----
TSEL_X = [12.0, 25.0, 38.0, 51.0]; TSEL_Y, TON_Y = 25.0, 34.0
LINK_X, SOLO_X, LINKSOLO_Y = 22.0, 50.0, 45.0
SCR_X, SCR_Y, SCR_W, SCR_H = 62.0, 12.5, 100.0, 31.0
NAV_Y, PREV_X, NEXT_X, PAGE_X, DATA_X = 49.0, 68.0, 80.0, 98.0, 152.0
PRESET_X = [171.0, 181.0]
MEM_X0, MEM_DX, MEM_Y, SAVE_X = 193.0, 10.6, 25.0, 277.8
MOV_Y, RAMT_X, RAND_X, MUT_X, LOCK_X, SEL_X = 40.0, 176.0, 194.0, 216.0, 234.0, 252.0
SL_Y = 78.0
WAVE_X, WAVE_Y, COARSE_X, FINE_X = 14.0, 74.0, 30.0, 43.0
FM_X, FM_Y = 64.0, [69.0, 77.0, 85.0]
CUT_X, RES_X, FENV_X = 77.0, 91.0, 105.0
LEV_X, VELO_X = 127.0, 141.0
ES_X, ENV_X = 166.0, [180.0, 192.0, 204.0, 216.0, 228.0]
LS_X, LS_Y, LRATE_X, LRATE_Y = [244.0, 252.0], 70.0, 248.0, 84.0
LDEPTH_X, LDEPTH_Y = 274.0, [69.0, 80.0, 91.0]
FX_X0, FX_DX, FX_Y, FX_LATCH_DX, FX_KNOB_DX = 5.0, 20.2, 114.0, 4.0, 13.0
JACK_X0, JACK_DX, JACK_Y = 152.0, 10.5, 117.0
LEARN_Y, DAC_X, DAC_Y = 103.5, 262.0, 103.5
OUTL_X, OUTR_X, OLEV_X = 252.0, 263.0, 275.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.3
# half sizes of the Rack parts
R_KNOB, R_SKNOB, R_JACK, R_BEZEL, R_BTN = 4.8, 3.85, 4.0, 3.6, 3.05
SL_HALF = 12.95  # VCVSlider is 25.9 mm tall

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


def tri(x, y, s, left, fill=SOFT):
    if left:
        pts = f"{x + s},{y - s} {x - s},{y} {x + s},{y + s}"
    else:
        pts = f"{x - s},{y - s} {x + s},{y} {x - s},{y + s}"
    return f'<polygon points="{pts}" fill="{fill}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# ---- header: name, tones
add(text("FACET", 5.0, 11.5, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.14))
add(text("layered digital synth", 31.0, 11.4, 2.3, FONT_MED, SOFT, anchor="start"))
add(f'<rect x="2.0" y="14.5" width="56.5" height="36.5" rx="1.4" fill="{BAND}"/>')
for t, x in enumerate(TSEL_X):
    add(text("ABCD"[t], x, TSEL_Y - R_BEZEL - 1.4, CONTROL, fill=TONE_COL[t]))
add(text("ON", TSEL_X[0] - R_BTN - 0.9, TON_Y + 1.15, CONTROL, anchor="end"))
add(text("LINK", LINK_X - R_BTN - 0.9, LINKSOLO_Y + 1.15, CONTROL, anchor="end"))
add(text("SOLO", SOLO_X - R_BTN - 0.9, LINKSOLO_Y + 1.15, CONTROL, anchor="end"))

# ---- screen + navigation
add(f'<rect x="{SCR_X - 0.8}" y="{SCR_Y - 0.8}" width="{SCR_W + 1.6}" height="{SCR_H + 1.6}" rx="1.2" fill="#0B0D0C"/>')
add(tri(PREV_X + R_BTN + 2.3, NAV_Y, 1.3, True))
add(tri(NEXT_X + R_BTN + 2.3, NAV_Y, 1.3, False))
add(text("PAGE", PAGE_X + R_BTN + 1.0, NAV_Y + 1.15, CONTROL, anchor="start"))
add(text("DATA", DATA_X - R_SKNOB - 1.0, NAV_Y + 1.15, CONTROL, anchor="end"))

# ---- presets, memories, movement
add(f'<rect x="165.0" y="14.5" width="{W - 167.0}" height="36.5" rx="1.4" fill="{BAND}"/>')
add(text("PRESET", (PRESET_X[0] + PRESET_X[1]) / 2, MEM_Y - R_BTN - 1.6, CONTROL))
add(tri(PRESET_X[0] - R_BTN - 1.9, MEM_Y, 1.2, True))
add(tri(PRESET_X[1] + R_BTN + 1.9, MEM_Y, 1.2, False))
for m in range(8):
    add(text(str(m + 1), MEM_X0 + m * MEM_DX, MEM_Y - R_BEZEL - 1.4, CONTROL))
add(text("MEMORIES", MEM_X0 + 3.5 * MEM_DX, 13.0, SMALL, FONT_BOLD, ACC, spacing=0.1))
add(text("SAVE", SAVE_X, MEM_Y - R_BEZEL - 1.4, CONTROL))
for x, n, r in [(RAMT_X, "AMOUNT", R_KNOB), (RAND_X, "RAND", R_BEZEL), (MUT_X, "MUTATE", R_KNOB),
                (LOCK_X, "LOCK", R_BTN), (SEL_X, "SEL", R_BTN)]:
    add(text(n, x, MOV_Y - r - 1.4, CONTROL))
add(text("ALL  WAVES  FILTER  ENVS  LFOS  FX", (RAMT_X + SEL_X) / 2 + 12, MOV_Y + 8.6, 1.9, FONT_MED, SOFT, spacing=0.02))

# ---- tone edit band
y0, y1 = 57.0, 98.0
add(f'<rect x="2.0" y="{y0}" width="{W - 4.0}" height="{y1 - y0}" rx="1.4" fill="{BAND}"/>')
add(text("EDITS THE SELECTED TONE (LINK = ALL FOUR)", W / 2, y1 - 0.9, 1.8, FONT_MED, SOFT, spacing=0.06))
HEAD_Y = 61.6
for x, n in [(28.5, "WAVE"), (84.0, "FILTER"), (134.0, "AMP"), (197.0, "ENVELOPE"), (259.0, "LFO")]:
    add(text(n, x, HEAD_Y, 2.6, FONT_BOLD, ACC, spacing=0.12))
below = SL_Y + SL_HALF + 1.2 + 2.35  # label baseline under sliders
for x, n in [(COARSE_X, "COARSE"), (FINE_X, "FINE"), (CUT_X, "CUTOFF"), (RES_X, "RES"), (FENV_X, "ENV"),
             (LEV_X, "LEVEL"), (VELO_X, "VELO"), (ENV_X[0], "T1"), (ENV_X[1], "T2"), (ENV_X[2], "T3"),
             (ENV_X[3], "SUS"), (ENV_X[4], "T4")]:
    add(text(n, x, below, CONTROL))
add(text("WAVE", WAVE_X, WAVE_Y + R_KNOB + 1.2 + 2.35, CONTROL))
add(text("pitch: L1", ENV_X[3] - 1.0, SL_Y - SL_HALF - 0.9, 1.8, FONT_MED, SOFT))
add(text("L0", ENV_X[4], SL_Y - SL_HALF - 0.9, 1.8, FONT_MED, SOFT))
for k, n in enumerate(["LPF", "BPF", "HPF"]):
    add(text(n, FM_X - R_BTN - 0.9, FM_Y[k] + 1.15, CONTROL, anchor="end"))
for k, n in enumerate(["PITCH", "TVF", "TVA"]):
    add(text(n, ES_X - R_BTN - 0.9, FM_Y[k] + 1.15, CONTROL, anchor="end"))
for k in range(2):
    add(text(str(k + 1), LS_X[k], LS_Y - R_BTN - 1.4, CONTROL))
add(text("RATE", LRATE_X, LRATE_Y + R_KNOB + 1.2 + 2.35, CONTROL))
for k, n in enumerate(["PITCH", "TVF", "TVA"]):
    add(text(n, LDEPTH_X - R_SKNOB - 0.9, LDEPTH_Y[k] + 1.15, CONTROL, anchor="end"))

# ---- effects
add(f'<rect x="2.0" y="100.5" width="143.0" height="21.0" rx="1.4" fill="{BAND}"/>')
for k, n in enumerate(["DIST", "PHASER", "SPECTRUM", "ENHANCE", "CHORUS", "DELAY", "REVERB"]):
    x0 = FX_X0 + k * FX_DX
    add(text(n, x0 + 8.5, FX_Y - R_SKNOB - 1.6, CONTROL, spacing=0.01))
add(text("ON + AMOUNT, THE REST ON THE SCREEN FX PAGES", 73.5, 120.4, 1.8, FONT_MED, SOFT, spacing=0.06))

# ---- jacks
add(f'<rect x="146.5" y="100.5" width="{W - 148.5}" height="21.0" rx="1.4" fill="{IOBG}"/>')
names = ["V/OCT", "GATE", "VEL", "PRESS", "MOD", "BEND", "CV1", "CV2", "LOCK"]
for i, n in enumerate(names):
    add(text(n, JACK_X0 + i * JACK_DX, JACK_Y - R_JACK - 1.3, JACK, fill=ACC if n in ("V/OCT", "GATE") else INK, spacing=0.0))
add(text("LEARN", JACK_X0 + 6 * JACK_DX - R_BTN - 0.9, LEARN_Y + 1.15, CONTROL, anchor="end"))
add(text("90s DAC", DAC_X - R_BTN - 0.9, DAC_Y + 1.15, CONTROL, anchor="end"))
add(text("L", OUTL_X, JACK_Y - R_JACK - 1.3, JACK, fill=ACC))
add(text("R", OUTR_X, JACK_Y - R_JACK - 1.3, JACK, fill=ACC))
add(text("LEVEL", OLEV_X, JACK_Y - R_SKNOB - 1.3, JACK))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "Facet.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Facet.svg"), x=25.0, y=125.6, cap=2.6, bg=BG)
