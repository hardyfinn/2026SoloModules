#!/usr/bin/env python3
# Draws res/<Module>.svg for the seven Werkbank modules and writes src/layout.hpp with every
# component position, so the C++ places components from the same numbers. Rack can't draw SVG
# <text>, so labels become paths.
#   pip install fonttools ; python3 res/make_panels.py
import os, sys, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import haer_logo
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

H = 128.5
HP = 5.08
BG = "#2E2D2B"
BG_DARK = "#1E1D1C"
LINE = "#57534D"
INK = "#EFE9DD"
SOFT = "#ABA597"
ACCENT = "#D6402F"
OUTINK = "#F7F1E6"

CONTROL = 3.0
JACK = 2.6
SMALL = 2.1
HEAD = 2.3

# component half-sizes (mm) for the collision check
SIZE = {"big": (6.1, 6.1), "knob": (4.8, 4.8), "small": (3.85, 3.85), "trim": (3.0, 3.0), "jack": (4.0, 4.0), "out": (4.0, 4.0),
        "button": (3.05, 3.05), "bezel": (3.6, 3.6), "pad": (5.6, 5.6), "sw2": (2.35, 3.5), "sw3": (2.3, 4.8),
        "slider": (3.35, 12.95), "light": (1.0, 1.0), "mlight": (1.5, 1.5)}
LABEL_GAP = {"big": 1.7, "knob": 1.6, "small": 1.4, "trim": 1.2, "jack": 1.4, "out": 1.4, "button": 1.3, "bezel": 1.3, "pad": 1.6,
             "sw2": 1.3, "sw3": 1.3, "slider": 1.3, "light": 0.8, "mlight": 0.8}

fonts = {}


def glyphs(s, size, font):
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    return fonts[font]


class Panel:
    def __init__(self, name, hp, title, subtitle):
        self.name, self.W, self.title, self.subtitle = name, hp * HP, title, subtitle
        self.parts, self.boxes, self.comps, self.ids = [], [], [], []
        self.add(f'<rect width="{self.W:.2f}" height="{H}" fill="{BG}"/>')
        self.text(title, 5.0, 10.4, 5.4, FONT_BOLD, INK, anchor="start", spacing=0.03)
        self.text(subtitle, self.W - 4.5, 10.0, 2.2, FONT_MED, ACCENT, anchor="end", spacing=0.03, check=False)

    def add(self, s):
        self.parts.append(s)

    def text(self, s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04, check=True):
        f, gs, cmap, upm = glyphs(s, size, font)
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
            self.boxes.append((s, x0, y - size * 0.75, x0 + width * scale, y + size * 0.05))
        self.add(f'<path fill="{fill}" d="{pen.getCommands()}"/>')
        return x0, x0 + width * scale

    def heading(self, s, x0, x1, y):
        a, b = self.text(s, x0, y, HEAD, FONT_BOLD, ACCENT, anchor="start", spacing=0.12)
        if x1 > b + 2:
            self.add(f'<line x1="{b + 1.4:.2f}" y1="{y - 0.8:.2f}" x2="{x1:.2f}" y2="{y - 0.8:.2f}" stroke="{LINE}" stroke-width="0.3"/>')

    def plate(self, x0, y0, x1, y1, fill=BG_DARK):
        self.add(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.4" fill="{fill}"/>')

    def comp(self, kind, cid, x, y, label=None, size=None, where="above", ink=None, font=FONT_BOLD):
        hw, hh = SIZE[kind]
        self.comps.append((cid or kind, x - hw, y - hh, x + hw, y + hh))
        if cid:
            self.ids.append((cid, kind, x, y))
        if label:
            if size is None:
                size = JACK if kind in ("jack", "out") else (SMALL if kind in ("light", "mlight", "sw2", "sw3", "button") else CONTROL)
            if ink is None:
                ink = OUTINK if kind == "out" else (SOFT if kind in ("trim",) else INK)
            if where == "above":
                self.text(label, x, y - hh - LABEL_GAP[kind], size, font, ink)
            elif where == "below":
                self.text(label, x, y + hh + LABEL_GAP[kind] + size * 0.75, size, font, ink)
            elif where == "right":
                self.text(label, x + hw + 1.0, y + size * 0.35, size, font, ink, anchor="start")
            elif where == "left":
                self.text(label, x - hw - 1.0, y + size * 0.35, size, font, ink, anchor="end")

    def arc(self, x, y, r, labels, size=SMALL, span=270.0, ink=SOFT):
        n = len(labels)
        for i, s in enumerate(labels):
            a = math.radians(-span / 2 + span * i / max(1, n - 1))
            lx, ly = x + r * math.sin(a), y - r * math.cos(a)
            anchor = "middle" if abs(math.sin(a)) < 0.3 else ("start" if math.sin(a) > 0 else "end")
            self.text(s, lx, ly + size * 0.35, size, FONT_BOLD, ink, anchor=anchor)

    def side(self, x, y, labels, kind="sw3", size=SMALL, where="right", ink=SOFT):
        """labels beside a vertical switch: top, (middle), bottom"""
        hw, hh = SIZE[kind]
        ys = [y - hh + 1.4, y + hh - 1.4] if len(labels) == 2 else [y - hh + 1.4, y, y + hh - 1.4]
        for s, yy in zip(labels, ys):
            if where == "right":
                self.text(s, x + hw + 0.9, yy + size * 0.35, size, FONT_BOLD, ink, anchor="start")
            else:
                self.text(s, x - hw - 0.9, yy + size * 0.35, size, FONT_BOLD, ink, anchor="end")

    def outplate(self, x0, y0, x1, y1):
        self.plate(x0, y0, x1, y1, ACCENT)

    def finish(self):
        allb = self.boxes + self.comps
        bad = []
        for i in range(len(allb)):
            for j in range(i + 1, len(allb)):
                a, b = allb[i], allb[j]
                if a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4]:
                    bad.append((a[0], b[0]))
        for a in allb:
            if a[1] < 0.6 or a[3] > self.W - 0.6 or a[2] < 0.5 or a[4] > H - 5.5:
                bad.append((a[0], "panel edge"))
        if bad:
            print(self.name, "OVERLAPS:", bad)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.W:.2f}mm" height="{H}mm" viewBox="0 0 {self.W:.2f} {H}">\n'
               + "\n".join(self.parts) + "\n</svg>\n")
        p = os.path.join(HERE, self.name + ".svg")
        open(p, "w").write(svg)
        haer_logo.add_to_file(p, x=self.W / 2, y=126.0, cap=2.6, bg=BG)
        return self.ids


layouts = {}

# ------------------------------------------------------------------------------------------- ZAP (12 HP)
def zap_full():
    P = Panel("Zap", 12, "ZAP", "synth percussion")
    C = [10.5, 30.48, 50.46]
    for x, (cid, n) in zip(C, [("START", "START"), ("END", "END"), ("SWEEP", "SWEEP")]): P.comp("knob", cid, x, 23.5, n)
    for x, (cid, n) in zip(C, [("RES", "RES"), ("DECAY", "DECAY"), ("NOISE", "NOISE")]): P.comp("knob", cid, x, 39.0, n)
    for x, (cid, n) in zip(C, [("CLICK", "CLICK"), ("DRIVE", "DRIVE"), ("AGE", "AGE")]): P.comp("knob", cid, x, 54.5, n)
    P.comp("bezel", "HIT", C[0], 68.5, "HIT", size=SMALL, where="right")
    P.comp("knob", "LEVEL", C[2], 68.5, "LEVEL", where="left", size=SMALL)
    for x, (cid, n) in zip(C, [("TRIG_IN", "TRIG"), ("VOCT_IN", "V/OCT"), ("ACCENT_IN", "ACCENT")]): P.comp("jack", cid, x, 84.5, n)
    for x, (cid, n) in zip(C, [("START_IN", "START"), ("END_IN", "END"), ("SWEEP_IN", "SWEEP")]): P.comp("jack", cid, x, 99.0, n)
    P.outplate(C[2] - 6.4, 103.9, C[2] + 6.4, 119.6)
    for x, (cid, n) in zip(C, [("RES_IN", "RES"), ("DECAY_IN", "DECAY"), ("OUT", "OUT")]):
        P.comp("out" if cid == "OUT" else "jack", cid, x, 113.5, n)
    return P.finish()

# ------------------------------------------------------------------------------------------- BLECH (26 HP)
def blech():
    P = Panel("Blech", 26, "BLECH", "six rhythm-box drum voices")
    L = [11.0, 29.0]
    VX = [50.0, 64.5, 79.0, 93.5, 108.0, 122.5]
    P.heading("BOX", 4.0, 20.0, 16.0)
    P.comp("sw3", "BOX", L[0], 25.0)
    P.side(L[0], 25.0, ["1", "2", "3"])
    P.comp("knob", "AGE", L[1], 25.5, "AGE")
    P.heading("PEDALS", 4.0, 38.5, 36.5)
    for i, (cid, n) in enumerate([("DYN", "DYNAMICS"), ("PITCH", "PITCH"), ("TONE", "TONE")]):
        y = 49.0 + 17.0 * i
        P.comp("knob", cid, L[0], y, n, size=2.7)
        P.comp("jack", cid + "_IN", L[1], y, "CV")
    P.comp("jack", "ACCENT_IN", L[0], 97.5, "ACCENT")
    P.comp("knob", "LEVEL", L[1], 97.5, "LEVEL", size=2.7)
    for x, n in zip(VX, ["BASS", "SNARE", "HAT", "CYMBAL", "TOM", "CLAVE"]):
        cid = n
        P.comp("pad", cid + "_PAD", x, 27.5, n, size=2.6)
        P.comp("light", cid + "_LIGHT", x + 7.0, 22.5)
        P.comp("small", cid + "_TUNE", x, 47.0, "TUNE", size=2.3, ink=SOFT)
        P.comp("small", cid + "_DECAY", x, 62.5, "DECAY", size=2.3, ink=SOFT)
        P.comp("jack", cid + "_TRIG", x, 80.0, "TRIG")
    P.add(f'<line x1="42.5" y1="16" x2="42.5" y2="104" stroke="{LINE}" stroke-width="0.3"/>')
    P.outplate(3.0, 104.4, P.W - 3.0, 120.0)
    P.comp("out", "MIX", L[0] + 9.0, 114.0, "MIX")
    for x, n in zip(VX, ["BASS", "SNARE", "HAT", "CYMBAL", "TOM", "CLAVE"]):
        P.comp("out", n + "_OUT", x, 114.0, n, size=2.3)
    return P.finish()

# ------------------------------------------------------------------------------------------- VOKODER (28 HP)
def vokoder():
    P = Panel("Vokoder", 28, "VOKODER", "20-band vocoder")
    xs = [7.2 + 6.75 * i for i in range(20)]
    P.heading("BANDS", 4.0, P.W - 4.0, 16.0)
    for i, x in enumerate(xs):
        P.comp("slider", "BAND%d" % i, x, 32.5)
    for i in (0, 5, 10, 15, 19):
        c = 100.0 * 80.0 ** ((i + 0.5) / 20.0)
        s = "%d" % round(c, -1 if c < 1000 else 0) if c < 1000 else ("%.1fk" % (c / 1000.0)).replace(".0k", "k")
        P.text(s, xs[i], 49.6, SMALL, FONT_BOLD, SOFT)
    K = [11.0, 28.0, 45.0, 62.0, 79.0, 96.0, 113.0, 130.0]
    for x, (cid, n) in zip(K, [("MODGAIN", "MOD GAIN"), ("ATTACK", "ATTACK"), ("RELEASE", "RELEASE"), ("FORMANT", "FORMANT"),
                               ("SIB", "SIBILANCE"), ("VOICING", "VOICING"), ("AGE", "AGE"), ("LEVEL", "LEVEL")]):
        P.comp("small", cid, x, 62.0, n, size=2.4)
    P.heading("CARRIER", 4.0, 70.0, 73.5)
    for x, (cid, n) in zip(K, [("SHAPE", "SHAPE"), ("WIDTH", "WIDTH"), ("TUNE", "TUNE")]):
        P.comp("small", cid, x, 84.0, n, size=2.4)
    P.comp("small", "CHORD", K[3], 84.0)
    P.arc(K[3], 84.0, 6.3, ["OFF", "5TH", "MIN", "MAJ"], size=1.8, span=180.0)
    P.comp("bezel", "FREEZE", K[5], 84.0, "FREEZE", size=2.4)
    P.comp("mlight", "VOICED_LIGHT", K[6], 84.0, "VOICED", size=SMALL, where="above")
    P.outplate(K[5] - 6.4, 101.6, K[7] + 6.4, 120.0)
    for x, (cid, n) in zip(K, [("MOD_IN", "MOD"), ("CAR_IN", "CARRIER"), ("VOCT_IN", "V/OCT"), ("FORMANT_IN", "FORMANT"),
                               ("FREEZE_IN", "FREEZE"), ("OUT", "OUT"), ("VOICED_OUT", "VOICED"), ("ENV_OUT", "ENV")]):
        P.comp("out" if x >= K[5] else "jack", cid, x, 112.5, n)
    return P.finish()

# ------------------------------------------------------------------------------------------- SPRECHER (28 HP)
def sprecher():
    P = Panel("Sprecher", 28, "SPRECHER", "speech synthesiser")
    P.plate(4.0, 15.0, 88.0, 47.0, "#141413")
    P.comps.append(("TEXT", 4.0, 15.0, 88.0, 47.0)); P.ids.append(("TEXT", "rect", 4.5, 15.5)); P.ids.append(("TEXT_SIZE", "rect", 83.0, 31.0))
    P.plate(4.0, 48.5, 88.0, 55.5, "#141413")
    P.comps.append(("WORD", 4.0, 48.5, 88.0, 55.5)); P.ids.append(("WORD", "rect", 4.5, 48.5)); P.ids.append(("WORD_SIZE", "rect", 83.0, 7.0))
    P.comp("knob", "LANG", 115.0, 30.0)
    P.arc(115.0, 30.0, 9.6, ["EN", "DE", "FR", "ES", "IT", "JA", "RU"], size=2.2)
    P.text("LANGUAGE", 115.0, 45.4, 2.4, FONT_BOLD, INK)
    P.comp("sw2", "ENGINE", 98.0, 54.0)
    P.side(98.0, 54.0, ["FORMANT", "CHIP"], kind="sw2", size=1.8)
    P.comp("sw2", "HOLD", 124.0, 54.0)
    P.side(124.0, 54.0, ["HOLD", "OFF"], kind="sw2", size=1.8)
    K = [13.0, 32.0, 51.0, 70.0, 89.0, 108.0, 127.0]
    for x, (cid, n) in zip(K, [("PITCH", "PITCH"), ("INFLECT", "INFLECT"), ("RATE", "RATE"), ("SIZE", "SIZE"),
                               ("WHISPER", "WHISPER"), ("AGE", "AGE"), ("LEVEL", "LEVEL")]):
        P.comp("knob", cid, x, 68.5, n, size=2.6)
    P.comp("bezel", "SPEAK", K[0], 81.5, "SPEAK", size=SMALL, where="right")
    P.comp("bezel", "RESET", K[1] + 6.0, 81.5, "RESET", size=SMALL, where="right")
    P.comp("mlight", "BUSY_LIGHT", K[3], 81.5, "BUSY", size=SMALL, where="right")
    for x, (cid, n) in zip(K, [("STEP_IN", "STEP"), ("RESET_IN", "RESET"), ("WORD_IN", "WORD"), ("VOCT_IN", "V/OCT"),
                               ("RATE_IN", "RATE"), ("SIZE_IN", "SIZE")]):
        P.comp("jack", cid, x, 95.0, n)
    P.outplate(K[3] - 6.4, 101.8, K[6] + 6.4, 120.4)
    for x, (cid, n) in zip(K[3:], [("OUT", "OUT"), ("F1_OUT", "F1"), ("BUSY_OUT", "BUSY"), ("END_OUT", "END")]):
        P.comp("out", cid, x, 113.5, n)
    return P.finish()

# ------------------------------------------------------------------------------------------- ORCHESTER (16 HP)
def orchester():
    P = Panel("Orchester", 16, "ORCHESTER", "optical disc")
    cx = P.W / 2
    P.comp("big", "DISC", cx, 31.0)
    P.arc(cx, 31.0, 11.0, ["CHOIR", "STRINGS", "FLUTE", "ORGAN", "CELLO", "HORN", "VOX", "REED"], size=2.0)
    P.text("DISC", cx, 47.2, 2.6, FONT_BOLD, INK)
    P.comp("knob", "WOBBLE", 14.0, 60.0, "WOBBLE", size=2.7)
    P.comp("sw3", "OCTAVE", cx, 60.0, "OCT", size=SMALL)
    P.side(cx, 60.0, ["+1", "0", "-1"])
    P.comp("knob", "TONE", P.W - 14.0, 60.0, "TONE", size=2.7)
    C = [12.0, 31.0, 50.3, 69.3]
    for x, (cid, n) in zip(C, [("ATTACK", "ATTACK"), ("RELEASE", "RELEASE"), ("AGE", "AGE"), ("LEVEL", "LEVEL")]):
        P.comp("small", cid, x, 78.0, n, size=2.3)
    for x, (cid, n) in zip(C, [("VOCT_IN", "V/OCT"), ("GATE_IN", "GATE"), ("DISC_IN", "DISC"), ("WOBBLE_IN", "WOBBLE")]):
        P.comp("jack", cid, x, 98.0, n)
    P.outplate(C[3] - 6.4, 105.2, C[3] + 6.4, 120.4)
    P.comp("jack", "TONE_IN", C[0], 114.0, "TONE")
    P.comp("out", "OUT", C[3], 114.0, "OUT")
    return P.finish()

# ------------------------------------------------------------------------------------------- DYNAMO (22 HP)
def dynamo():
    P = Panel("Dynamo", 22, "DYNAMO", "bass and lead voice")
    P.heading("OSCILLATORS", 4.0, P.W - 4.0, 15.6)
    P.comp("sw3", "OCTAVE", 8.0, 28.0, "OCT", size=SMALL)
    P.side(8.0, 28.0, ["0", "-1", "-2"])
    P.comp("sw2", "WAVE1", 21.0, 28.0, "OSC 1", size=SMALL)
    P.side(21.0, 28.0, ["SAW", "PULSE"], kind="sw2", size=1.7)
    P.comp("sw2", "WAVE2", 35.0, 28.0, "OSC 2", size=SMALL)
    P.side(35.0, 28.0, ["SAW", "PULSE"], kind="sw2", size=1.7)
    K5 = [52.0, 64.5, 77.0, 89.5, 102.0]
    for x, (cid, n) in zip(K5, [("DETUNE", "DETUNE"), ("WIDTH", "WIDTH"), ("MIX", "MIX"), ("SUB", "SUB"), ("NOISE", "NOISE")]):
        P.comp("small", cid, x, 28.0, n, size=2.3)
    P.heading("FILTER", 4.0, 44.0, 39.6)
    P.comp("big", "CUTOFF", 12.5, 52.0, "CUTOFF", size=2.6)
    P.comp("big", "RES", 32.5, 52.0, "RESONANCE", size=2.6)
    P.heading("FILTER ENVELOPE", 47.0, P.W - 4.0, 39.6)
    for x, (cid, n) in zip(K5, [("ENV", "AMOUNT"), ("KEY", "KEY"), ("FA", "ATTACK"), ("FD", "DECAY"), ("FS", "SUSTAIN")]):
        P.comp("small", cid, x, 52.0, n, size=2.1)
    P.heading("AMP ENVELOPE", 4.0, 44.0, 64.6)
    P.heading("CHARACTER", 47.0, P.W - 4.0, 64.6)
    K7 = [10.0, 23.5, 37.0, 52.0, 66.0, 80.0, 102.0]
    for x, (cid, n) in zip(K7, [("AA", "ATTACK"), ("AD", "DECAY"), ("AS", "SUSTAIN"), ("GLIDE", "GLIDE"), ("DRIVE", "DRIVE"), ("AGE", "AGE"), ("LEVEL", "LEVEL")]):
        P.comp("small", cid, x, 77.0, n, size=2.1)
    J = [10.0, 26.0, 42.0, 58.0, 74.0]
    P.comp("trim", "CUT_ATT", J[3], 89.5)
    P.text("CV AMT", J[3] + 4.4, 90.3, 1.8, FONT_BOLD, SOFT, anchor="start")
    for x, (cid, n) in zip(J, [("VOCT_IN", "V/OCT"), ("GATE_IN", "GATE"), ("ACCENT_IN", "ACCENT"), ("CUT_IN", "CUTOFF"), ("WIDTH_IN", "WIDTH")]):
        P.comp("jack", cid, x, 107.0, n)
    P.outplate(95.6, 101.4, 108.4, 120.0)
    P.comp("out", "OUT", 102.0, 111.0, "OUT")
    return P.finish()

# ------------------------------------------------------------------------------------------- TAKT (40 HP)
def takt():
    P = Panel("Takt", 40, "TAKT", "two-row step sequencer")
    X0, DX = 77.0, 8.0
    xs = [X0 + DX * i for i in range(16)]
    rows = {"A": dict(light=17.0, slider=33.0, sw=53.0, acc=62.5), "B": dict(light=70.5, slider=86.5, sw=106.5, acc=116.0)}
    for r, Y in rows.items():
        P.text(r, 69.0, Y["slider"] + 2.4, 6.0, FONT_BOLD, ACCENT)
        P.text("PLAY", 72.0, Y["sw"] - 2.6, 1.7, FONT_BOLD, SOFT, anchor="end")
        P.text("TIE", 72.0, Y["sw"] + 0.6, 1.7, FONT_BOLD, SOFT, anchor="end")
        P.text("REST", 72.0, Y["sw"] + 3.8, 1.7, FONT_BOLD, SOFT, anchor="end")
        P.text("ACC", 72.0, Y["acc"] + 0.6, 1.7, FONT_BOLD, SOFT, anchor="end")
        for i, x in enumerate(xs):
            P.text(str(i + 1), x - 2.0, Y["light"] + 0.7, 1.9, FONT_BOLD, SOFT, anchor="end")
            P.comp("light", "%s_LIGHT%d" % (r, i), x + 0.6, Y["light"])
            P.comp("slider", "%s_SLIDER%d" % (r, i), x, Y["slider"])
            P.comp("sw3", "%s_MODE%d" % (r, i), x, Y["sw"])
            P.comp("button", "%s_ACC%d" % (r, i), x, Y["acc"])
    P.add(f'<line x1="74.5" y1="66.5" x2="{P.W - 3.0:.2f}" y2="66.5" stroke="{LINE}" stroke-width="0.3"/>')
    P.add(f'<line x1="66.0" y1="14" x2="66.0" y2="122" stroke="{LINE}" stroke-width="0.3"/>')
    K = [10.0, 25.0, 40.0, 55.0]
    for x, (cid, n) in zip(K, [("PATTERN", "PATTERN"), ("GATE", "GATE"), ("LAST_A", "LAST A"), ("LAST_B", "LAST B")]):
        P.comp("small", cid, x, 24.0, n, size=2.4)
    P.comp("button", "RUN", 8.0, 41.0, "RUN", size=SMALL, where="above")
    P.comp("button", "RESET", 19.0, 41.0, "RESET", size=SMALL, where="above")
    P.comp("sw3", "RANGE", 30.5, 41.0, "OCT", size=SMALL)
    P.side(30.5, 41.0, ["4", "2", "1"])
    P.comp("sw2", "CHAIN", 46.0, 41.0, "CHAIN", size=SMALL)
    P.side(46.0, 41.0, ["SER", "PAR"], kind="sw2", size=1.7)
    P.comp("sw2", "WHEN", 7.5, 52.5, "WHEN", size=SMALL)
    P.side(7.5, 52.5, ["BAR", "NOW"], kind="sw2", size=1.7)
    P.comp("sw2", "KEYS", 7.5, 65.0, "KEYS", size=SMALL)
    P.side(7.5, 65.0, ["TRNS", "SCALE"], kind="sw2", size=1.7)
    # keyboard of light buttons: white keys C D E F G A B on the lower row, black keys between them above
    white = [0, 2, 4, 5, 7, 9, 11]; black = {1: 0.5, 3: 1.5, 6: 3.5, 8: 4.5, 10: 5.5}
    kx0, kdx = 22.5, 6.5
    for i, n in enumerate(white): P.comp("button", "KEY%d" % n, kx0 + kdx * i, 64.0)
    for n, pos in black.items(): P.comp("button", "KEY%d" % n, kx0 + kdx * pos, 56.5)
    P.text("KEYBOARD", kx0 + kdx * 3, 51.6, SMALL, FONT_BOLD, SOFT)
    J = [8.5, 21.0, 33.5, 46.0, 58.5]
    for x, (cid, n) in zip(J, [("CLOCK_IN", "CLOCK"), ("RESET_IN", "RESET"), ("RUN_IN", "RUN"), ("TRANS_IN", "TRANSP"), ("GATECV_IN", "GATE CV")]):
        P.comp("jack", cid, x, 80.5, n, size=2.2)
    P.comp("jack", "PATCV_IN", J[0], 97.0, "PATTERN", size=2.2)
    P.outplate(J[1] - 6.25, 88.2, J[4] + 6.25, 120.6)
    for x, (cid, n) in zip(J[1:], [("A_VOCT", "A V/OCT"), ("A_GATE", "A GATE"), ("A_ACC", "A ACC"), ("END_OUT", "END")]):
        P.comp("out", cid, x, 97.0, n, size=2.2)
    for x, (cid, n) in zip(J[1:], [("B_VOCT", "B V/OCT"), ("B_GATE", "B GATE"), ("B_ACC", "B ACC")]):
        P.comp("out", cid, x, 113.0, n, size=2.2)
    return P.finish()

if __name__ == "__main__":
    out = ["// Generated by res/make_panels.py. Component centres in mm. Do not edit by hand.", "#pragma once", "namespace layout {",
           "struct P { float x, y; };"]
    for name, f in [("Zap", zap_full), ("Blech", blech), ("Vokoder", vokoder), ("Sprecher", sprecher), ("Orchester", orchester), ("Dynamo", dynamo), ("Takt", takt)]:
        ids = f()
        out.append("namespace %s {" % name)
        for cid, kind, x, y in ids:
            out.append("static const P %s = {%.3ff, %.3ff};   // %s" % (cid, x, y, kind))
        out.append("}")
    out.append("}  // namespace layout")
    open(os.path.join(HERE, "..", "src", "layout.hpp"), "w").write("\n".join(out) + "\n")
    print("ok")
