#!/usr/bin/env python3
# Draws res/Bench808.svg (808 Circuit Bench, 48 HP). The screen, the voice buttons, the
# panel buttons and the four edit-knob labels are drawn live by the module.
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
BG, LINE, INK, SOFT, ORA = "#1F2023", "#3A3B40", "#ECE6D8", "#9A968C", "#F08A24"

# ---- LAYOUT (same numbers in src/Bench808.cpp) ----
W, H = 243.84, 128.5
SCR = (8.0, 20.0, 227.84, 47.0)
SEL_X, KNOB_Y = 128.0, 84.5
VOICE_Y, TRIG_Y, OUT_Y = 99.0, 106.4, 118.4
PITCH = (W - 44.0) / 13.0
MIX_X = [W - 26.0, W - 14.0]
MIX_IN_Y = 104.6

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
a(text("808 CIRCUIT BENCH", 8.0, 13.2, 6.0, anchor="start", spacing=0.08))
a(text("the TR-808 voice board on one screen", W - 8.0, 12.6, 2.9, FONT_MED, SOFT, anchor="end"))
x, y, w, h = SCR
a(f'<rect x="{x-1.4}" y="{y-1.4}" width="{w+2.8}" height="{h+2.8}" rx="2.2" fill="#0A0A0A"/>')
a(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.2" fill="#0E1411" stroke="#22302A" stroke-width="0.3"/>')
a(text("SELECT", SEL_X, 76.4, 3.2))
a(text("SHIFT + voice = hit it", 178.0, 81.8, 2.3, FONT_MED, SOFT, anchor="start"))
a(text("SHIFT + STOCK = whole voice", 178.0, 85.4, 2.3, FONT_MED, SOFT, anchor="start"))
a(text("SHIFT + KITS = stock kit", 178.0, 89.0, 2.3, FONT_MED, SOFT, anchor="start"))
def section(name, x0, x1, yy):
    a(text(name, x0, yy, 3.0, fill=ORA, anchor="start", spacing=0.12))
    a(f'<line x1="{x0 + text.width + 2.5}" y1="{yy-1.0}" x2="{x1}" y2="{yy-1.0}" stroke="{LINE}" stroke-width="0.35"/>')
section("VOICES", 8.0, W - 34.0, 94.6)
section("MIX", W - 32.0, W - 8.0, 94.6)
a(text("TRIG", 3.0, TRIG_Y + 0.8, 2.3, FONT_MED, SOFT, anchor="start"))
a(text("OUT", 3.0, OUT_Y + 0.8, 2.3, FONT_MED, SOFT, anchor="start"))
for i in range(13):
    cx = 8.0 + PITCH * (i + 0.5)
    a(f'<rect x="{cx-4.2}" y="{OUT_Y-5.6}" width="8.4" height="10.4" rx="1.2" fill="#4A4B50"/>')
for cx, lab in zip(MIX_X, ["ACC", "POLY"]):
    a(text(lab, cx, MIX_IN_Y - 5.2, 2.9))
for cx, lab in zip(MIX_X, ["L", "R"]):
    a(f'<rect x="{cx-4.6}" y="{OUT_Y-8.8}" width="9.2" height="13.6" rx="1.2" fill="{ORA}"/>')
    a(text(lab, cx, OUT_Y - 5.2, 2.6, fill="#1B1B1B"))
a(haer_logo.logo_group(W, BG, y=125.9, cap=2.1))
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
       + "".join(p) + "</svg>\n")
open(os.path.join(HERE, "Bench808.svg"), "w").write(svg)
print("ok Bench808.svg")
