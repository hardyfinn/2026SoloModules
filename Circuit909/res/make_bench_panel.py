#!/usr/bin/env python3
# Draws res/Bench909.svg (909 Circuit Bench, 36 HP). The knob names, values and stage
# headings, the voice title, the buttons and the kit lights are drawn live by the module.
#   pip install fonttools ; python3 res/make_bench_panel.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import haer_logo
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
BG, LINE, INK, SOFT, ORA = "#D9D8D3", "#B3B1AA", "#262626", "#5F5D57", "#E5631F"

# ---- LAYOUT (same numbers in src/Bench909.cpp) ----
W, H = 182.88, 128.5                       # 36 HP
BTN_Y, LED_X0, LED_DX = 79.0, 67.0, 6.0
RAND_IN_X = 113.5
NV = 11
VOICE_X0, VOICE_P = 13.0, 12.4
VOICE_Y, TRIG_Y, OUT_Y = 92.4, 102.4, 115.4
MIX_X = [W - 23.5, W - 10.5]

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
    text.width = width * scale
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

p = []
a = p.append
a(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
a(f'<rect width="{W}" height="1.6" fill="{ORA}"/>')
a(text("909 CIRCUIT BENCH", 8.0, 11.5, 5.4, anchor="start", spacing=0.08))
# knob names, values and stage headings are drawn live by the module (they follow the voice)
for i, L in enumerate("ABCD"):
    a(text(L, LED_X0 + i * LED_DX, BTN_Y + 5.4, 2.6, FONT_MED, SOFT))
a(text("IN", RAND_IN_X, BTN_Y - 4.9, 2.9))
for j, line in enumerate(["SHIFT + voice = hit  |  + STOCK = loaded kit",
                          "SHIFT + KITS = stock kit  |  + RAND = wild",
                          "double-click knob = reset  |  CH chokes OH"]):
    a(text(line, W - 8.0, BTN_Y - 2.4 + j * 3.0, 2.2, FONT_MED, SOFT, anchor="end", spacing=0.02))
def section(name, x0, x1, yy):
    a(text(name, x0, yy, 3.0, fill=ORA, anchor="start", spacing=0.12))
    a(f'<line x1="{x0 + text.width + 2.5}" y1="{yy-1.0}" x2="{x1}" y2="{yy-1.0}" stroke="{LINE}" stroke-width="0.35"/>')
section("VOICES", 4.0, W - 34.0, 87.4)
section("MIX", W - 30.0, W - 4.0, 87.4)
a(text("TRIG", 3.0, TRIG_Y + 0.8, 2.3, FONT_MED, SOFT, anchor="start"))
a(text("OUT", 3.0, OUT_Y + 0.8, 2.3, FONT_MED, SOFT, anchor="start"))
for i in range(NV):
    cx = VOICE_X0 + VOICE_P * (i + 0.5)
    a(f'<rect x="{cx-4.2}" y="{OUT_Y-5.4}" width="8.4" height="10.4" rx="1.2" fill="#B9B7B0"/>')
for cx, lab in zip(MIX_X, ["ACC", "POLY"]):
    a(text(lab, cx, VOICE_Y + 1.1, 2.9))
for cx, lab in zip(MIX_X, ["L", "R"]):
    a(f'<rect x="{cx-4.6}" y="{OUT_Y-8.6}" width="9.2" height="13.6" rx="1.2" fill="{ORA}"/>')
    a(text(lab, cx, OUT_Y - 5.0, 2.6, fill="#FBF8F2"))
a(haer_logo.logo_group(W, BG, y=125.9, cap=2.1))
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
       + "".join(p) + "</svg>\n")
open(os.path.join(HERE, "Bench909.svg"), "w").write(svg)
print("ok Bench909.svg")
