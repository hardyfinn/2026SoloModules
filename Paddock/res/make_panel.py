#!/usr/bin/env python3
# Draws res/Swerve.svg, res/Gallop.svg and res/Tumble.svg. Rack can't draw SVG <text>,
# so labels become paths. Positions are in mm; the LAYOUT numbers are shared with src/*.cpp.
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
H = 128.5

BG = "#2B2119"       # walnut
BG2 = "#352A20"
STRIP = "#1C1510"
INK = "#F2E6CF"
BRASS = "#D9A441"
SOFT = "#BFAF95"

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.5

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


class Panel:
    def __init__(self, name, w):
        self.name, self.w, self.p = name, w, []
        self.add(f'<rect width="{w}" height="{H}" fill="{BG}"/>')
        # brass rails top and bottom (a nod to the brass touch nodes)
        self.add(f'<rect x="0" y="0" width="{w}" height="1.2" fill="{BRASS}" fill-opacity="0.55"/>')
        self.add(f'<rect x="0" y="{H - 1.2}" width="{w}" height="1.2" fill="{BRASS}" fill-opacity="0.55"/>')

    def add(self, s):
        self.p.append(s)

    def t(self, *a, **k):
        self.add(text(*a, **k))

    def box(self, x, y, w, h, fill=STRIP, rx=1.4, op=1.0):
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" fill-opacity="{op}"/>')

    def title(self, name, sub):
        self.t(name, 4.6, 10.6, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.02)
        self.t(sub, self.w - 4.0, 10.2, 2.5, FONT_MED, BRASS, anchor="end")

    def ranges(self, x, y, r, lo, hi):
        self.t(lo, x - r, y, SMALL, FONT_BOLD, SOFT, anchor="end")
        self.t(hi, x + r, y, SMALL, FONT_BOLD, SOFT, anchor="start")

    def switch3(self, x, y, title, opts):   # opts top to bottom
        self.t(title, x, y - 7.6, CONTROL)
        for yy, o in zip([y - 3.0, y + 1.0, y + 5.0], opts):
            self.t(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start")

    def switch2(self, x, y, title, opts):
        self.t(title, x, y - 6.2, CONTROL)
        for yy, o in zip([y - 1.4, y + 3.6], opts):
            self.t(o, x + 2.9, yy, SMALL, FONT_BOLD, SOFT, anchor="start")

    def save(self):
        path = os.path.join(HERE, self.name + ".svg")
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}mm" height="{H}mm" viewBox="0 0 {self.w} {H}">\n'
               + "\n".join(self.p) + "\n</svg>\n")
        open(path, "w").write(svg)
        haer_logo.add_to_file(path, x=self.w / 2, y=125.9, cap=2.6, bg=BG)


# ================================================================ Swerve (12 HP)
# ---- LAYOUT Swerve (same numbers in src/Swerve.cpp) ----
SW_W = 60.96
SW_KX = [16.0, 44.96]
SW_K1_Y = 29.0
SW_K2_Y = 48.5
SW_SW_Y = 69.0
SW_RANGE_X, SW_CYCLE_X, SW_LIGHT_X = 9.0, 31.0, 52.0
SW_TX = [11.0, 30.48, 49.96]
SW_TRIM_Y = 84.0
SW_CVJ_Y = 92.6
SW_JX = [8.5, 23.2, 37.8, 52.46]
SW_IN_Y = 105.6
SW_OUT_Y = 117.6

p = Panel("Swerve", SW_W)
p.title("Swerve", "bounce generator")
for x, n in zip(SW_KX, ["UP", "DOWN"]):
    p.t(n, x, SW_K1_Y - 9.4, CONTROL, fill=BRASS)
    p.ranges(x, SW_K1_Y + 8.8, 5.0, "SLOW", "FAST")
for x, n in zip(SW_KX, ["HIGH", "LOW"]):
    p.t(n, x, SW_K2_Y - 7.8, CONTROL)
    p.ranges(x, SW_K2_Y + 7.6, 4.6, "−5", "+5")
p.switch3(SW_RANGE_X, SW_SW_Y, "RANGE", ["AUDIO", "LOW", "CV"])
p.switch2(SW_CYCLE_X, SW_SW_Y, "MODE", ["CYCLE", "ONCE"])
p.t("DIR", SW_LIGHT_X, SW_SW_Y - 6.2, CONTROL)
p.box(2.4, SW_TRIM_Y - 7.6, SW_W - 4.8, 19.4)
for x, n in zip(SW_TX, ["UP CV", "BOTH CV", "DOWN CV"]):
    p.t(n, x, SW_TRIM_Y - 4.2, JACK, fill=SOFT)
for x, n in zip(SW_JX, ["HIGH", "LOW", "▲ TRIG", "▼ TRIG"]):
    p.t(n, x, SW_IN_Y - 5.0, JACK)
p.box(2.4, SW_OUT_Y - 8.6, SW_W - 4.8, 13.4, fill=BRASS)
for x, n in zip(SW_JX, ["TRI", "SQR", "TOP", "BTM"]):
    p.t(n, x, SW_OUT_Y - 5.0, JACK, fill=BG)
p.save()

# ================================================================ Gallop (24 HP)
# ---- LAYOUT Gallop (same numbers in src/Gallop.cpp) ----
GA_W = 121.92
GA_DISP = (5.0, 13.6, 111.92, 14.4)
GA_NUM_X = 5.6
GA_RATE_X, GA_TILT_X, GA_TRIM_X, GA_CV_X = 17.0, 32.0, 44.5, 55.5
GA_HI_X, GA_LO_X = 68.5, 80.5
GA_TRI_X, GA_SQR_X = 96.0, 110.0
GA_ROWS = [42.0, 58.0, 74.0, 90.0]
GA_HEAD_Y = 33.4
GA_BOT_Y = 113.0
GA_RANGE_X, GA_RAIL_X, GA_FM_X, GA_FMATT_X = 10.0, 32.0, 55.5, 44.5
GA_MIX_X, GA_POLY_X = 96.0, 110.0

p = Panel("Gallop", GA_W)
p.title("Gallop", "four horses, each bounded by its neighbours")
dx, dy, dw, dh = GA_DISP
p.add(f'<rect x="{dx - 0.5}" y="{dy - 0.5}" width="{dw + 1}" height="{dh + 1}" rx="1.0" fill="#0B0806" '
      f'stroke="{BRASS}" stroke-opacity="0.6" stroke-width="0.35"/>')
p.box(88.6, GA_HEAD_Y - 4.0, 28.8, 64.6, fill=BRASS)
for x, n in [(GA_RATE_X, "RATE"), (GA_TILT_X, "TILT"), (GA_TRIM_X, "CV"), (GA_CV_X, "IN"),
             (GA_HI_X, "HIGH"), (GA_LO_X, "LOW")]:
    p.t(n, x, GA_HEAD_Y, CONTROL, fill=BRASS if n in ("RATE",) else INK)
p.t("TRI", GA_TRI_X, GA_HEAD_Y, JACK, fill=BG)
p.t("SQR", GA_SQR_X, GA_HEAD_Y, JACK, fill=BG)
p.t("bounds: patch to override", (GA_HI_X + GA_LO_X) / 2, GA_HEAD_Y + 2.9, 1.9, FONT_MED, SOFT)
for i, y in enumerate(GA_ROWS):
    p.t(str(i + 1), GA_NUM_X, y + 1.2, 4.2, FONT_BOLD, BRASS)
    if i < 3:
        p.add(f'<line x1="9" y1="{y + 8.0}" x2="86" y2="{y + 8.0}" stroke="#4A3B2C" stroke-width="0.3"/>')
p.box(62.0, GA_ROWS[0] - 5.0, 25.0, GA_ROWS[3] - GA_ROWS[0] + 10.0, op=0.7)
# bottom row
p.add(f'<line x1="3" y1="100.4" x2="{GA_W - 3}" y2="100.4" stroke="#4A3B2C" stroke-width="0.4"/>')
p.switch3(GA_RANGE_X - 2.0, GA_BOT_Y, "RANGE", ["AUDIO", "LOW", "CV"])
p.t("RAIL", GA_RAIL_X, GA_BOT_Y - 7.6, CONTROL)
p.ranges(GA_RAIL_X, GA_BOT_Y + 6.6, 4.4, "1V", "8V")
p.t("FM", GA_FMATT_X, GA_BOT_Y - 7.6, JACK, fill=SOFT)
p.t("V/OCT", GA_FM_X, GA_BOT_Y - 7.6, JACK)
p.t("all four", (GA_FMATT_X + GA_FM_X) / 2, GA_BOT_Y + 7.6, SMALL, FONT_MED, SOFT)
p.t("PARADOX", 72.0, GA_BOT_Y - 7.6, JACK, fill=SOFT)
p.box(88.6, GA_BOT_Y - 11.4, 28.8, 16.0, fill=BRASS)
p.t("MIX", GA_MIX_X, GA_BOT_Y - 7.6, JACK, fill=BG)
p.t("POLY", GA_POLY_X, GA_BOT_Y - 7.6, JACK, fill=BG)
p.save()

# ================================================================ Tumble (20 HP)
# ---- LAYOUT Tumble (same numbers in src/Tumble.cpp) ----
TU_W = 101.6
TU_X = [11.6, 31.2, 50.8, 70.4, 90.0]
TU_RATE_Y = 28.5
TU_TOUCH_Y = 49.0
TU_TCV_Y = 62.5
TU_GATE_Y = 78.0
TU_KNOB_Y = 96.0
TU_JACK_Y = 115.0

p = Panel("Tumble", TU_W)
p.title("Tumble", "five rolling rhythm cores + gongs")
for i, x in enumerate(TU_X):
    p.t(f"RATE {i + 1}", x, TU_RATE_Y - 8.4, CONTROL, fill=BRASS)
p.t("TOUCH: a finger across two rolz. Right = pull together, left = push apart", TU_W / 2, TU_TOUCH_Y - 11.6,
    1.95, FONT_MED, SOFT)
for i, x in enumerate(TU_X):
    p.t(f"{i + 1}↔{(i + 1) % 5 + 1}", x, TU_TOUCH_Y - 7.4, CONTROL)
p.box(2.4, TU_TCV_Y - 4.8, TU_W - 4.8, 9.6)
p.t("TOUCH CV (0–10 V, or a gate as a finger)", TU_W / 2, TU_TCV_Y - 5.6, 1.95, FONT_MED, SOFT)
p.box(2.4, TU_GATE_Y - 8.6, TU_W - 4.8, 13.2, fill=BRASS)
for i, x in enumerate(TU_X):
    p.t(f"GATE {i + 1}", x, TU_GATE_Y - 5.0, JACK, fill=BG)
p.add(f'<line x1="3" y1="{TU_KNOB_Y - 12.4}" x2="{TU_W - 3}" y2="{TU_KNOB_Y - 12.4}" stroke="#4A3B2C" stroke-width="0.4"/>')
for x, n in zip(TU_X, ["TEMPO", "PITCH", "SPREAD", "TONE", "DECAY"]):
    p.t(n, x, TU_KNOB_Y - 7.8, CONTROL, fill=BRASS if n == "TEMPO" else INK)
p.ranges(TU_X[0], TU_KNOB_Y + 7.6, 4.6, "÷4", "×4")
p.ranges(TU_X[3], TU_KNOB_Y + 7.6, 3.6, "WOOD", "METAL")
for x, n in zip(TU_X[:3], ["SYNC", "TEMPO", "V/OCT"]):
    p.t(n, x, TU_JACK_Y - 5.0, JACK)
p.box(TU_X[3] - 8.0, TU_JACK_Y - 8.6, TU_X[4] - TU_X[3] + 16.0, 13.2, fill=BRASS)
for x, n in zip(TU_X[3:], ["OUT L", "OUT R"]):
    p.t(n, x, TU_JACK_Y - 5.0, JACK, fill=BG)
p.save()
print("ok")
