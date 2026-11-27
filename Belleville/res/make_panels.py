#!/usr/bin/env python3
# Draws res/Strata.svg, res/Undertow.svg and res/Signal.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm and shared with src/Layout.hpp.   pip install fonttools ; python3 res/make_panels.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
import haer_logo

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_BLACK = os.environ.get("PANEL_FONT_BLACK", "/usr/share/fonts/opentype/inter/Inter-Black.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
H = 128.5
CONTROL, JACK = 3.2, 2.9          # project standard label sizes (2026-11-10)
INK, PLATE, PLATE_INK = "#1D1D1D", "#1D1D1D", "#F4F1EA"
R_KNOB, R_LARGE, R_BEZEL, R_JACK = 4.8, 6.1, 3.6, 4.0

# ---- LAYOUT (same numbers in src/Layout.hpp) ----
DISP_X, DISP_Y = 5.0, 14.5
S_W = 24 * 5.08
S_COLS = [S_W / 2 - 40.65, S_W / 2 - 13.55, S_W / 2 + 13.55, S_W / 2 + 40.65]
S_DISP_H = 22.0
S_ROWS = [45.0, 62.5, 80.0]
T_W = 20 * 5.08
T_COLS = [T_W / 2 - 28.8, T_W / 2, T_W / 2 + 28.8]
T_JCOLS = [T_W / 2 - 35.4, T_W / 2 - 11.8, T_W / 2 + 11.8, T_W / 2 + 35.4]
T_DISP_H = 18.0
T_ROWS = [42.0, 59.5, 77.5]
Y_IN, Y_OUT = 102.0, 116.5

fonts = {}
def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, boxes=None):
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
    if boxes is not None:
        boxes.append((s, x0, y - size * 0.75, x0 + width * scale, y + size * 0.05))
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'

def panel(fname, W, name, sub, bg, accent, disp_h, rows, cols, jcols, controls, inputs, outputs):
    """controls: list of (row, col, label, kind) with kind knob/large/bezel"""
    parts, boxes, comp = [], [], []
    add = parts.append
    add(f'<rect width="{W:.2f}" height="{H}" fill="{bg}"/>')
    add(f'<rect x="0" y="0" width="{W:.2f}" height="2.2" fill="{accent}"/>')
    add(text(name, 5.0, 10.2, 6.4, FONT_BLACK, INK, anchor="start", spacing=0.02, boxes=boxes))
    add(text(sub, W - 5.0, 10.0, 2.9, FONT_MED, INK, anchor="end", boxes=boxes))
    add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{W - 2 * DISP_X:.2f}" height="{disp_h}" rx="1.4" fill="#0D1A17" stroke="{INK}" stroke-width="0.4"/>')
    comp.append(("display", DISP_X, DISP_Y, W - DISP_X, DISP_Y + disp_h))
    for row, col, label, kind in controls:
        x, y = cols[col], rows[row]
        r = {"knob": R_KNOB, "large": R_LARGE, "bezel": R_BEZEL}[kind]
        comp.append((label + " ctl", x - r, y - r, x + r, y + r))
        add(text(label, x, y + R_LARGE + 3.9 if row == 2 else y + r + 3.9, CONTROL, boxes=boxes))
    # inputs: labels above the jacks
    ic = cols if len(inputs) == len(cols) else jcols
    for i, label in enumerate(inputs):
        x = ic[i]
        comp.append(("in", x - R_JACK, Y_IN - R_JACK, x + R_JACK, Y_IN + R_JACK))
        add(text(label, x, Y_IN - R_JACK - 1.4, JACK, boxes=boxes))
    # outputs on a dark plate
    add(f'<rect x="3.0" y="107.4" width="{W - 6.0:.2f}" height="14.8" rx="1.6" fill="{PLATE}"/>')
    for i, label in enumerate(outputs):
        x = jcols[i]
        comp.append(("out", x - R_JACK, Y_OUT - R_JACK, x + R_JACK, Y_OUT + R_JACK))
        add(text(label, x, Y_OUT - R_JACK - 1.4, JACK, fill=PLATE_INK, boxes=boxes))
    # small caption: LINK light, top right under the subtitle
    add(text("LINK", W - 8.6, 13.0 - 0.3, 2.2, FONT_BOLD, INK, anchor="end", boxes=boxes))
    comp.append(("link light", W - 7.6, 11.0, W - 5.0, 13.4))
    allb = boxes + comp
    bad = []
    for i in range(len(allb)):
        for j in range(i + 1, len(allb)):
            a, b = allb[i], allb[j]
            if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
                bad.append((a[0], b[0]))
    for a in allb:
        if a[1] < 0.8 or a[3] > W - 0.8: bad.append((a[0], "panel edge"))
    if bad: print(fname, "OVERLAPS:", bad)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">\n'
           + "\n".join(parts) + "\n</svg>\n")
    path = os.path.join(HERE, fname)
    open(path, "w").write(svg)
    haer_logo.add_to_file(path, x=W / 2, y=125.6, cap=3.0, bg=bg)
    print("wrote", fname)

panel("Strata.svg", S_W, "Strata", "chords · harmony leader", "#E8E2D4", "#B8862E", S_DISP_H, S_ROWS, S_COLS, S_COLS,
      [(0, 0, "STYLE", "knob"), (0, 1, "KEY", "knob"), (0, 2, "MODE", "knob"), (0, 3, "PROG", "knob"),
       (1, 0, "RHYTHM", "knob"), (1, 1, "VOICING", "knob"), (1, 2, "COLOUR", "knob"), (1, 3, "SPREAD", "knob"),
       (2, 0, "LENGTH", "knob"), (2, 1, "DRIFT", "large"), (2, 2, "HOLD", "bezel"), (2, 3, "NEXT", "bezel")],
      ["CLOCK", "RESET", "CHORDS", "ENERGY"], ["V/OCT", "GATE", "VEL", "CHANGE"])
panel("Undertow.svg", T_W, "Undertow", "bass line", "#D9E4E8", "#3E7C91", T_DISP_H, T_ROWS, T_COLS, T_JCOLS,
      [(0, 0, "STYLE", "knob"), (0, 1, "DENSITY", "knob"), (0, 2, "FOLLOW", "knob"),
       (1, 0, "OCTAVE", "knob"), (1, 1, "SLIDE", "knob"), (1, 2, "ACCENT", "knob"),
       (2, 0, "DRIFT", "large"), (2, 1, "SPACE", "knob"), (2, 2, "NEW", "bezel")],
      ["KICK", "DRIFT", "OCT"], ["V/OCT", "GATE", "ACC", "SLIDE"])
panel("Signal.svg", T_W, "Signal", "melody · hook · arp", "#ECE0E4", "#A04A6A", T_DISP_H, T_ROWS, T_COLS, T_JCOLS,
      [(0, 0, "STYLE", "knob"), (0, 1, "RANGE", "knob"), (0, 2, "DENSITY", "knob"),
       (1, 0, "MOTIF", "knob"), (1, 1, "ANSWER", "knob"), (1, 2, "OUTSIDE", "knob"),
       (2, 0, "MUTATE", "large"), (2, 1, "FREEZE", "bezel"), (2, 2, "NEW", "bezel")],
      ["MUTATE", "FREEZE", "RANGE"], ["V/OCT", "GATE", "VEL", "MOD"])
