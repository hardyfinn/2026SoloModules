#!/usr/bin/env python3
# Draws res/Sprout.svg, res/Graft.svg, res/Bloom.svg and writes src/Layout.hpp
# (every position the C++ needs). Rack can't draw SVG <text>, so labels are paths.
#   pip install fonttools ; python3 res/make_panels.py
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
BG = "#1E3B2A"; BG_DARK = "#142A1D"; INK = "#F2ECD9"; SOFT = "#B5C9A8"; LEAF = "#9BD770"
CONTROL, JACK, SMALL = 3.2, 2.9, 2.5
H = 128.5
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

layout = []

def panel(name, hp, sub, disp, knobs, switches, ins, outs, buttons=()):
    W = round(hp * 5.08, 2)
    p = [f'<rect width="{W}" height="{H}" fill="{BG}"/>']
    p.append(text(name, 4.0, 10.6, 5.8, FONT_BOLD, INK, anchor="start", spacing=0.02))
    p.append(text(sub, W - 4.0, 10.4, 2.5, FONT_MED, LEAF, anchor="end"))
    dx, dy, dw, dh = disp
    p.append(f'<rect x="{dx - 0.6}" y="{dy - 0.6}" width="{dw + 1.2}" height="{dh + 1.2}" rx="1" fill="#07110B" stroke="{LEAF}" stroke-opacity="0.5" stroke-width="0.35"/>')
    for lab, x, y, big, lo, hi in knobs:
        p.append(text(lab, x, y - (7.8 if big else 6.6), CONTROL))
        if lo:
            r = 6.0 if big else 5.0
            p.append(text(lo, x - r, y + 6.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
            p.append(text(hi, x + r, y + 6.4, SMALL, FONT_BOLD, SOFT, anchor="start"))
    for lab, x, y, up, down in switches:
        p.append(text(lab, x, y - 6.4, CONTROL))
        p.append(text(up, x + 2.8, y - 1.2, SMALL, FONT_BOLD, SOFT, anchor="start"))
        p.append(text(down, x + 2.8, y + 3.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    for lab, x, y in buttons:
        p.append(text(lab, x, y - 5.4, CONTROL))
    p.append(f'<rect x="2.5" y="{ins[0][2] - 8.6}" width="{W - 5}" height="13.4" rx="1.4" fill="{BG_DARK}"/>')
    for lab, x, y in ins:
        p.append(text(lab, x, y - 5.2, JACK, FONT_BOLD, SOFT))
    oy = outs[0][2]
    p.append(f'<rect x="2.5" y="{oy - 8.6}" width="{W - 5}" height="13.4" rx="1.4" fill="{LEAF}"/>')
    for lab, x, y in outs:
        p.append(text(lab, x, y - 5.2, JACK, FONT_BOLD, BG))
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(p) + "\n</svg>\n")
    path = os.path.join(HERE, name + ".svg")
    open(path, "w").write(svg)
    sys.path.insert(0, HERE)
    import haer_logo
    haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=BG)
    # layout for the C++
    ns = name.lower()
    layout.append(f"namespace {ns} {{")
    layout.append(f"static const float DISP_X = {dx}f, DISP_Y = {dy}f, DISP_W = {dw}f, DISP_H = {dh}f;")
    for kind, items in (("K", knobs), ("S", switches), ("B", buttons), ("I", ins), ("O", outs)):
        for it in items:
            lab = it[0].replace(" ", "_").replace("7TH", "SEVENTH").upper()
            layout.append(f"static const float {kind}_{lab}_X = {it[1]}f, {kind}_{lab}_Y = {it[2]}f;")
    layout.append("}")

def cols(n, W, m=9.0):
    return [round(m + (W - 2 * m) * i / (n - 1), 3) for i in range(n)]

# ---- Sprout 12 HP
W = 60.96; c2 = cols(2, W, 16.0); c3 = cols(3, W, 11.0); c4 = cols(4, W, 8.5)
panel("Sprout", 12, "growing rhythm", (4.0, 14.0, round(W - 8.0, 3), 20.0),
      knobs=[("RULE", c2[0], 49.0, True, "", ""), ("GEN", c2[1], 49.0, True, "0", "MAX"),
             ("GROW", c2[0], 70.0, False, "HOLD", "8"), ("LENGTH", c2[1], 70.0, False, "4", "32")],
      switches=[("MODE", c2[0] - 2.5, 88.5, "STREAM", "BAR"), ("QUANT", c2[1] - 2.5, 88.5, "ON", "OFF")],
      ins=[("CLOCK", c3[0], 105.0), ("RESET", c3[1], 105.0), ("GEN CV", c3[2], 105.0)],
      outs=[("A", c4[0], 119.0), ("B", c4[1], 119.0), ("C", c4[2], 119.0), ("CYCLE", c4[3], 119.0)])

# ---- Graft 16 HP
W = 81.28; c3 = cols(3, W, 15.0); c4 = cols(4, W, 10.5)
panel("Graft", 16, "drum rules that rewrite themselves", (4.0, 14.0, round(W - 8.0, 3), 22.0),
      knobs=[("GROOVE", c3[0], 50.0, True, "", ""), ("EVERY", c3[1], 50.0, True, "OFF", "8"),
             ("RATCHET", c3[2], 50.0, True, "0", "1"),
             ("DROP", c3[0], 71.0, False, "0", "1"), ("SPROUT", c3[1], 71.0, False, "0", "1"),
             ("HOME", c3[2], 71.0, False, "0", "1")],
      switches=[],
      buttons=[("MUTATE", c3[0] + 8.0, 89.5), ("SEED", c3[2] - 8.0, 89.5)],
      ins=[("CLOCK", c4[0], 105.0), ("RESET", c4[1], 105.0), ("MUTATE", c4[2], 105.0), ("SEED", c4[3], 105.0)],
      outs=[("KICK", c4[0], 119.0), ("SNARE", c4[1], 119.0), ("HAT", c4[2], 119.0), ("PERC", c4[3], 119.0)])

# ---- Bloom 14 HP
W = 71.12; c3 = cols(3, W, 13.0); c4 = cols(4, W, 11.0)
panel("Bloom", 14, "chords grown by rules", (4.0, 14.0, round(W - 8.0, 3), 22.0),
      knobs=[("RULE", c3[0], 50.0, True, "", ""), ("MODE", c3[1], 50.0, True, "", ""), ("KEY", c3[2], 50.0, True, "C", "B"),
             ("LENGTH", c3[0], 71.0, False, "1", "32"), ("GEN", c3[1], 71.0, False, "0", "MAX")],
      switches=[("GROW", c3[2] - 2.5, 70.0, "ON", "OFF"), ("7TH", c3[1] - 2.5, 89.0, "ON", "OFF")],
      ins=[("CLOCK", c3[0], 105.0), ("RESET", c3[1], 105.0), ("ROOT CV", c3[2], 105.0)],
      outs=[("POLY", c4[0], 119.0), ("BASS", c4[1], 119.0), ("GATE", c4[2], 119.0), ("CHANGE", c4[3], 119.0)])

open(os.path.join(HERE, "..", "src", "Layout.hpp"), "w").write(
    "// Generated by res/make_panels.py - do not edit by hand.\n#pragma once\n" + "\n".join(layout) + "\n")
print("ok")
