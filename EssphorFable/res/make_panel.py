#!/usr/bin/env python3
# Draws res/Essphor.svg, res/EssphorRes.svg and res/EssphorMod.svg, and writes src/layout.hpp
# (every component position, shared with the module sources). Rack can't draw SVG <text>, so labels become paths.
#   pip install fonttools ; python3 res/make_panel.py
import os, json, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
H = 128.5
HP = 5.08

BG = "#1E2226"        # slate
BG_DARK = "#14171A"
LINE = "#3A4249"
INK = "#ECE6D8"
SOFT = "#9FA8AE"
ACCENT = "#E0B04A"    # tile gold
ACCENT2 = "#5FB3A1"   # verdigris

RADIUS = {"knob": 4.9, "big": 6.4, "small": 4.0, "snap": 4.9, "bezel": 3.7, "jack": 4.0, "out": 4.0, "light": 1.1, "button": 2.6, "display": 0}

fonts = {}


def text(boxes, s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, check=True):
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


class Panel:
    def __init__(self, name, hp, title, subtitle):
        self.name, self.W = name, hp * HP
        self.C = {}          # name -> (kind, x, y, label)
        self.disp = {}       # display name -> (w, h)
        self.parts, self.boxes = [], []
        self.add(f'<rect width="{self.W:.2f}" height="{H}" fill="{BG}"/>')
        self.add(self.t(title, 5.0, 9.4, 5.2, FONT_BOLD, INK, anchor="start", spacing=0.02))
        self.title_end = self.boxes[-1][3]
        if subtitle:
            self.add(self.t(subtitle, self.title_end + 2.5, 9.0, 2.2, FONT_MED, ACCENT, anchor="start"))

    def add(self, p): self.parts.append(p)
    def t(self, *a, **k): return text(self.boxes, *a, **k)
    def comp(self, name, kind, x, y, label=None): self.C[name] = (kind, x, y, label)

    def display(self, name, x, y, w, h=4.6):
        self.C[name] = ("display", x, y, None); self.disp[name] = (w, h)
        self.add(f'<rect x="{x - w / 2:.2f}" y="{y - h / 2:.2f}" width="{w}" height="{h}" rx="0.7" fill="{BG_DARK}" stroke="{LINE}" stroke-width="0.25"/>')

    def heading(self, s, x0, x1, y, color=ACCENT):
        self.add(self.t(s, x0, y, 2.3, FONT_BOLD, color, anchor="start", spacing=0.12))
        tx = self.boxes[-1][3] + 1.2
        if x1 > tx:
            self.add(f'<line x1="{tx:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

    def labels(self, size=2.6, skip=()):
        for name, (kind, x, y, label) in self.C.items():
            if not label or name in skip: continue
            if kind in ("knob", "big", "snap", "small", "bezel"):
                dy = {"big": 8.8, "bezel": 6.0, "small": 6.2}.get(kind, 7.4)
                self.add(self.t(label, x, y - dy, size if kind != "small" else 2.1))
            elif kind in ("jack", "out"):
                self.add(self.t(label, x, y - 5.3, 2.3, FONT_BOLD, BG_DARK if kind == "out" else (SOFT if name.endswith("_CV") else INK)))

    def finish(self, logo_y=125.6):
        allb = list(self.boxes)
        for name, (kind, x, y, label) in self.C.items():
            if kind == "display":
                w, h = self.disp[name]; allb.append((name, x - w / 2, y - h / 2, x + w / 2, y + h / 2)); continue
            r = RADIUS[kind]; allb.append((name, x - r, y - r, x + r, y + r))
        bad = []
        for i in range(len(allb)):
            for j in range(i + 1, len(allb)):
                a, b = allb[i], allb[j]
                if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
                    bad.append((a[0], b[0]))
        for a in allb:
            if a[1] < 0.8 or a[3] > self.W - 0.8: bad.append((a[0], "panel edge"))
        if bad: print(self.name, "OVERLAPS:", bad)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.W:.2f}mm" height="{H}mm" viewBox="0 0 {self.W:.2f} {H}">\n'
               + "\n".join(p for p in self.parts if p) + "\n</svg>\n")
        path = os.path.join(HERE, self.name + ".svg")
        open(path, "w").write(svg)
        sys.path.insert(0, HERE)
        import haer_logo
        haer_logo.add_to_file(path, x=self.W / 2, y=logo_y, cap=2.8, bg=BG)
        return bad


def jack_well(p, x0, y0, x1, y1, fill=BG_DARK):
    p.add(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.2" fill="{fill}"/>')


layout = {}

# ------------------------------------------------------------------------- Essphor (32 HP)
p = Panel("Essphor", 32, "Essphor", "live rhythmic grains")
COLS = [11.0 + 20.0 * i for i in range(8)]
R1, M1, R2, M2, R3 = 28.0, 37.4, 53.4, 62.4, 72.5
JA, JB, JC = 88.0, 102.5, 117.0
top = [("PITCH", "PITCH"), ("RATE", "RATE"), ("SIZE", "SIZE"), ("CONTOUR", "CONTOUR"), ("SCAN", "SCAN"), ("SPRAY", "SPRAY"), ("PATTERN", "PATTERN"), ("WET", "WET")]
bot = [("FEEDBACK", "FEEDBACK"), ("RAND_PITCH", "R PITCH"), ("RAND_RATE", "R RATE"), ("RAND_SIZE", "R SIZE"), ("RAND_AMP", "R AMP"), ("RAND_REV", "R REV"), ("AGE", "AGE")]
p.heading("GRAINS", 4.0, p.W - 4.0, 15.2)
for (n, lab), x in zip(top, COLS): p.comp(n, "big", x, R1, lab)
p.heading("RANDOM AND CHARACTER", 4.0, p.W - 4.0, 42.3, ACCENT2)
for (n, lab), x in zip(bot, COLS): p.comp(n, "knob", x, R2, lab)
p.comp("SCALE", "snap", COLS[7], R2, "SCALE")
# mode buttons + text displays
modes = {"RATE_MODE": COLS[1], "SCAN_MODE": COLS[4], "SPRAY_MODE": COLS[5], "WET_MODE": COLS[7]}
for n, x in modes.items():
    p.comp(n, "button", x - 7.0, M1); p.display(n + "_DISP", x + 2.6, M1, 12.6)
for n, x in {"FB_MODE": COLS[0], "RP_MODE": COLS[1]}.items():
    p.comp(n, "button", x - 7.0, M2); p.display(n + "_DISP", x + 2.6, M2, 12.6)
p.display("SCALE_DISP", COLS[7] - 1.5, M2, 17.0)
p.display("PATTERN_DISP", COLS[6], M1, 14.0)
# row 3: tempo, lock, lights, buffer display
p.comp("TEMPO", "small", COLS[0], R3 + 1.0, "TEMPO")
p.comp("LOCK", "bezel", COLS[1], R3 + 1.0, "LOCK")
p.comp("CLOCK_LIGHT", "light", COLS[2] - 3.0, R3 + 1.0); p.comp("GRAIN_LIGHT", "light", COLS[2] + 5.0, R3 + 1.0)
p.add(p.t("CLOCK", COLS[2] - 3.0, R3 - 2.2, 2.0, FONT_BOLD, SOFT)); p.add(p.t("GRAIN", COLS[2] + 5.0, R3 - 2.2, 2.0, FONT_BOLD, SOFT))
p.display("BUF_DISP", (COLS[3] + COLS[6]) / 2, R3 + 1.0, 58.0, 6.0)
p.add(p.t("BUFFER", COLS[3] - 9.0, R3 - 3.4, 2.0, FONT_BOLD, SOFT, anchor="start"))
# jacks
jack_well(p, 2.0, JA - 8.0, p.W - 2.0, JC + 4.6)
for (n, lab), x in zip(top, COLS): p.comp(n + "_CV", "jack", x, JA, lab)
for (n, lab), x in zip(bot, COLS): p.comp(n + "_CV", "jack", x, JB, lab)
p.comp("VOCT", "jack", COLS[7], JB, "V/OCT")
for n, lab, x in [("IN_L", "IN L", COLS[0]), ("IN_R", "IN R", COLS[1]), ("CLOCK", "CLOCK", COLS[2]), ("RESET", "RESET", COLS[3]), ("LOCK_IN", "LOCK", COLS[4])]:
    p.comp(n, "jack", x, JC, lab)
p.add(f'<rect x="{COLS[5] - 8.5:.2f}" y="{JC - 8.0:.2f}" width="{COLS[7] - COLS[5] + 17.0:.2f}" height="11.6" rx="1.1" fill="{ACCENT}"/>')
for n, lab, x in [("GRAIN_OUT", "GRAIN", COLS[5]), ("OUT_L", "OUT L", COLS[6]), ("OUT_R", "OUT R", COLS[7])]:
    p.comp(n, "out", x, JC, lab)
p.labels()
p.add(p.t("CV", 3.2, JA + 1.0, 2.0, FONT_BOLD, SOFT, anchor="start", check=False))
bad = p.finish()
layout["Essphor"] = p

# ------------------------------------------------------------------------- Essphor Res (16 HP)
p = Panel("EssphorRes", 16, "Essphor Res", None)
X3 = [13.5, 40.64, 67.8]
p.add(p.t("48-band tuned resonator", 5.0, 14.0, 2.2, FONT_MED, ACCENT, anchor="start"))
p.comp("CUTOFF", "big", X3[0], 28.0, "CUTOFF"); p.comp("RES", "knob", X3[1], 28.0, "RESONANCE"); p.comp("DECAY", "knob", X3[2], 28.0, "DECAY")
p.comp("DECAY_MODE", "button", X3[2] - 7.0, 38.0); p.display("DECAY_MODE_DISP", X3[2] + 2.4, 38.0, 12.6)
p.comp("PITCH", "knob", X3[0], 52.0, "PITCH"); p.comp("SLOPE", "knob", X3[1], 52.0, "SLOPE"); p.comp("TONE", "knob", X3[2], 52.0, "TONE")
p.add(p.t("LP", X3[1] - 5.6, 59.0, 1.9, FONT_BOLD, SOFT)); p.add(p.t("HP", X3[1] + 5.6, 59.0, 1.9, FONT_BOLD, SOFT))
p.add(p.t("DARK", X3[2] - 6.0, 59.0, 1.9, FONT_BOLD, SOFT)); p.add(p.t("BRIGHT", X3[2] + 6.6, 59.0, 1.9, FONT_BOLD, SOFT))
p.comp("SCALE", "snap", X3[0], 71.0, "SCALE"); p.display("SCALE_DISP", X3[0] + 2.0, 79.6, 21.0)
p.comp("WET", "knob", X3[1], 71.0, "WET"); p.comp("WET_MODE", "button", X3[1] - 7.0, 79.6); p.display("WET_MODE_DISP", X3[1] + 2.4, 79.6, 12.6)
p.comp("AGE", "knob", X3[2], 71.0, "AGE")
J5 = [8.6, 24.6, 40.64, 56.7, 72.7]
JA, JB, JC = 93.0, 105.5, 118.0
jack_well(p, 2.0, JA - 7.8, p.W - 2.0, JC + 4.4)
for n, lab, x in zip(["CUTOFF_CV", "RES_CV", "DECAY_CV", "PITCH_CV", "SLOPE_CV"], ["CUTOFF", "RES", "DECAY", "PITCH", "SLOPE"], J5): p.comp(n, "jack", x, JA, lab)
for n, lab, x in zip(["TONE_CV", "WET_CV", "AGE_CV", "VOCT"], ["TONE", "WET", "AGE", "V/OCT"], J5): p.comp(n, "jack", x, JB, lab)
p.comp("IN_L", "jack", J5[0], JC, "IN L"); p.comp("IN_R", "jack", J5[1], JC, "IN R")
p.add(f'<rect x="{J5[3] - 8.2:.2f}" y="{JC - 7.8:.2f}" width="{J5[4] - J5[3] + 16.4:.2f}" height="11.4" rx="1.1" fill="{ACCENT}"/>')
p.comp("OUT_L", "out", J5[3], JC, "OUT L"); p.comp("OUT_R", "out", J5[4], JC, "OUT R")
p.labels()
bad += p.finish()
layout["EssphorRes"] = p

# ------------------------------------------------------------------------- Essphor Mod (26 HP)
p = Panel("EssphorMod", 26, "Essphor Mod", None)
KX = [8.5 + 11.4 * i for i in range(8)]     # 8.5 .. 88.3
MX, OA, OB = 101.5, 117.0, 126.5
p.comp("CLOCK", "jack", 101.0, 9.5); p.comp("RESET", "jack", 122.0, 9.5)
p.add(p.t("CLOCK", 95.2, 10.4, 2.2, FONT_BOLD, INK, anchor="end")); p.add(p.t("RESET", 116.2, 10.4, 2.2, FONT_BOLD, INK, anchor="end"))
SECS = [
    ("WAVE", "W", ["RATE", "AMOUNT", "PHASE", "OFFSET", "SKEW", "FOLD", "CURVE", "SPREAD"]),
    ("RANDOM", "R", ["RATE", "AMOUNT", "PHASE", "OFFSET", "LENGTH", "VARIATION", "SMOOTH", "SPREAD"]),
    ("FOLLOW", "F", ["DELAY", "AMOUNT", "RISE", "FALL", "INPUT", "FILTER", "HOLD", "SPREAD"]),
    ("ADSR", "A", ["ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ATT CRV", "DEC CRV", "REL CRV", "SPREAD"]),
]
Y0 = [17.5, 44.5, 71.5, 98.5]
for (title, pre, knobs), y0 in zip(SECS, Y0):
    p.heading(title, 4.0, 110.0, y0 + 1.8, ACCENT if pre in "WF" else ACCENT2)
    ky = y0 + 14.0
    for i, (lab, x) in enumerate(zip(knobs, KX)):
        nm = pre + "_" + lab.replace(" CRV", "_CURVE").replace(" ", "_")
        p.comp(nm, "small", x, ky, None)
        p.add(p.t(lab, x, ky - 6.0, 2.0 if len(lab) < 8 else 1.75))
    p.add(f'<rect x="{OA - 5.6:.2f}" y="{y0 - 0.6:.2f}" width="{OB - OA + 11.2:.2f}" height="{21.6:.2f}" rx="1.1" fill="{ACCENT}"/>')
    p.comp(pre + "_OUT_A", "out", OA, ky, None); p.comp(pre + "_OUT_B", "out", OB, ky, None)
    p.add(p.t("A", OA, ky - 5.6, 2.4, FONT_BOLD, BG_DARK)); p.add(p.t("B", OB, ky - 5.6, 2.4, FONT_BOLD, BG_DARK))
    p.add(p.t("OUT", (OA + OB) / 2, y0 + 3.0, 2.0, FONT_BOLD, BG_DARK))
    if pre in "WR":
        p.comp(pre + "_RATE_MODE", "button", MX - 6.3, y0 + 9.0); p.display(pre + "_RATE_MODE_DISP", MX + 3.2, y0 + 9.0, 10.5)
    if pre == "W":
        p.comp("W_SHAPE", "button", MX - 6.3, y0 + 17.0); p.display("W_SHAPE_DISP", MX + 3.2, y0 + 17.0, 10.5)
    if pre == "F":
        p.comp("F_IN_L", "jack", MX - 4.0, ky, None); p.comp("F_IN_R", "jack", MX + 5.0, ky, None)
        p.add(p.t("IN L", MX - 4.0, ky - 5.6, 2.0, FONT_BOLD, INK)); p.add(p.t("IN R", MX + 5.0, ky - 5.6, 2.0, FONT_BOLD, INK))
    if pre == "A":
        p.comp("A_GATE", "jack", MX - 4.0, ky, None); p.comp("A_TRIG", "bezel", MX + 5.4, ky, None)
        p.add(p.t("GATE", MX - 4.0, ky - 5.6, 2.0, FONT_BOLD, INK)); p.add(p.t("TRIG", MX + 5.4, ky - 5.6, 2.0, FONT_BOLD, INK))
bad += p.finish()
layout["EssphorMod"] = p

# ------------------------------------------------------------------------- layout.hpp
lines = ["// Generated by res/make_panel.py. Do not edit.", "#pragma once", "namespace layout {"]
for mod, pn in layout.items():
    lines.append(f"namespace {mod} {{")
    for name, (kind, x, y, label) in pn.C.items():
        lines.append(f"static const float {name}_X = {x:.3f}f, {name}_Y = {y:.3f}f;")
    for n, (w, h) in pn.disp.items():
        lines.append(f"static const float {n}_W = {w:.3f}f, {n}_H = {h:.3f}f;")
    lines.append("}")
lines.append("}")
open(os.path.join(HERE, "..", "src", "layout.hpp"), "w").write("\n".join(lines) + "\n")
json.dump({m: {"W": pn.W, "H": H, "comps": pn.C, "disp": pn.disp} for m, pn in layout.items()},
          open(os.path.join(HERE, "..", "tools", "layout.json"), "w"), indent=0)
print("ok" if not bad else "ok, with overlaps")
