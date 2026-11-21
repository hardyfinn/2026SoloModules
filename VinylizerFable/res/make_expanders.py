#!/usr/bin/env python3
# Draws res/VinylMaster.svg (12 HP) and res/VinylPressing.svg (14 HP), the two expanders.
# Positions are in mm and match the widget code in src/VinylizerFable.cpp.
#   python3 res/make_expanders.py
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(HERE, "make_panel.py")).read()
exec(_src.split("def colx")[0])          # palette, sizes and text() from the main panel script
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import haer_logo
H = 128.5


def panel(fname, W, title, sub, accent, side, draw):
    parts = []
    add = parts.append
    add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
    add(text(title, 5.0, 10.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.01))
    add(text("LINK", W - 8.0, 9.8, SMALL, FONT_BOLD, SOFT, anchor="end"))
    add(f'<rect x="3.6" y="13.6" width="{W - 7.2}" height="0.6" fill="{accent}"/>')
    add(text(sub, 5.0, 17.2, 2.2, FONT_MED, accent, anchor="start"))
    draw(add, W)
    add(text(side, 5.0, 126.9, 1.9, FONT_MED, SOFT, anchor="start"))
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(parts) + "\n</svg>\n")
    path = os.path.join(HERE, fname)
    open(path, "w").write(svg)
    haer_logo.add_to_file(path, x=W - 22.0, y=126.0, cap=3.0, bg=BG)


def knob(add, name, x, y, lo=None, hi=None):
    add(text(name, x, y - 6.6, CONTROL if len(name) < 8 else CONTROL - 0.3))
    if lo is not None:
        add(text(lo, x - 4.4, y + 6.4, SMALL, FONT_BOLD, SOFT, anchor="end"))
        add(text(hi, x + 4.4, y + 6.4, SMALL, FONT_BOLD, SOFT, anchor="start"))


def selector(add, W, name, opts, accent):
    add(f'<rect x="3.6" y="19.0" width="{W - 7.2}" height="21.6" rx="1.4" fill="{COL_BG}"/>')
    add(text(name, 12.0, 23.0, CONTROL, FONT_BOLD, accent))
    for i, o in enumerate(opts):
        add(text(o, 27.6, 22.0 + 4.0 * i + 0.85, 2.4, FONT_BOLD, INK, anchor="start"))


def master(add, W):
    X = [11.0, 30.48, 49.96]
    selector(add, W, "SOURCE", ['1/2" TAPE  30 ips', '1/4" TAPE  15 ips', "1630  early digital", "DAT", "CD MASTER"], COPPER)
    add(f'<rect x="3.6" y="42.6" width="{W - 7.2}" height="57.6" rx="1.4" fill="{COL_BG}"/>')
    knob(add, "DRIVE", X[0], 54, "0", "1"); knob(add, "PRINT", X[1], 54, "0", "1"); knob(add, "NR", X[2], 54, "−", "+")
    knob(add, "HISS", X[0], 72, "0", "1"); knob(add, "CONVERT", X[1], 72, "OLD", "NEW"); knob(add, "LOUD", X[2], 72, "0", "10")
    for x, n in zip(X, ["BITS", "EMPHASIS", "DELAY"]):
        add(text(n, x, 84.6, CONTROL - 0.3))
    for yy, o in zip([93.6, 88.6], ["14", "16"]):
        add(text(o, X[0] - 0.9, yy + 0.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    for yy, o in zip([96.0, 92.0, 88.0], ["OK", "BRIGHT", "DULL"]):
        add(text(o, X[1] - 1.9, yy - 0.2, SMALL, FONT_BOLD, SOFT, anchor="start"))
    for yy, o in zip([93.6, 88.6], ["OFF", "ON"]):
        add(text(o, X[2] - 0.9, yy + 0.6, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(f'<rect x="3.6" y="104.6" width="{W - 7.2}" height="17.4" rx="1.4" fill="{STRIP}"/>')
    for x, n in zip(X, ["ON", "DRIVE", "LOUD"]):
        add(text(n, x, 110.4, JACK, FONT_BOLD, SOFT))


def pressing(add, W):
    X = [12.0, 35.56, 59.12]
    JX = [9.0, 22.28, 35.56, 48.84, 62.12]
    selector(add, W, "STOCK", ["VIRGIN", "REGRIND  recycled", 'STYRENE  7" single', "FLEXI", "PICTURE DISC"], GOLD)
    add(f'<rect x="3.6" y="42.6" width="{W - 7.2}" height="54.4" rx="1.4" fill="{COL_BG}"/>')
    knob(add, "NOISE", X[0], 54, "0", "2"); knob(add, "NON-FILL", X[1], 54, "0", "1"); knob(add, "STAMPER", X[2], 54, "NEW", "3000")
    knob(add, "WEIGHT", X[0], 72, "90", "180g"); knob(add, "DUST", X[1], 72, "0", "1"); knob(add, "SCRATCH", X[2], 72, "0", "1")
    knob(add, "CUE BURN", X[0], 90, "0", "1")
    add(text("CLEAN", X[1], 84.0, CONTROL - 0.3)); add(text("ADD", X[2], 84.0, CONTROL - 0.3))
    add(f'<rect x="3.6" y="97.6" width="{W - 7.2}" height="25.4" rx="1.4" fill="{STRIP}"/>')
    for x, n in zip(JX, ["NOISE", "DUST", "STAMP", "CLEAN", "ADD"]):
        add(text(n, x, 100.2, SMALL, FONT_BOLD, SOFT))
    add(text("ON", JX[0] + 4.6, 118.8, SMALL, FONT_BOLD, SOFT, anchor="start"))
    add(f'<rect x="{JX[3] - 6.0}" y="110.6" width="{JX[4] - JX[3] + 12.0}" height="12.0" rx="1.4" fill="{COPPER}"/>')
    for x, n in zip(JX[3:], ["TURN", "SKIP"]):
        add(text(n, x, 113.2, SMALL + 0.1, fill=BG))


panel("VinylMaster.svg", 60.96, "Master", "what the cutting engineer was handed", COPPER, "left of Vinylizer", master)
panel("VinylPressing.svg", 71.12, "Pressing", "the plant, the plastic, the life of the disc", GOLD, "right of Vinylizer", pressing)
print("ok")
