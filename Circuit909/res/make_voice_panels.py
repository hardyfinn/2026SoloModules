#!/usr/bin/env python3
# Draws res/<Slug>.svg for every voice in res/voices.py and writes src/VoiceSpecs.hpp
# (the same knob table for the module code). Rack can't draw SVG <text>, so labels are paths.
#   pip install fonttools ; python3 res/make_voice_panels.py
import os, sys, math
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from voices import VOICES
import haer_logo

FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 81.28, 128.5  # 16 HP (2.3.0)
BG, BG_DARK, LINE, INK, SOFT = "#D9D8D3", "#C9C7C0", "#B3B1AA", "#262626", "#5F5D57"
OUT_INK = "#FBF8F2"

# ---- LAYOUT (same numbers in src/Voice909.hpp) ----
BIG_X = [W / 6 + i * W / 3 for i in range(3)]          # big knobs, rows 0-1
SMALL_X = [W / 8 + i * W / 4 for i in range(4)]        # small knobs, rows 2-4
ROW_Y = [27.0, 44.5, 63.0, 76.5, 90.0]
JACK_Y = 112.0
def jack_x(n): return [7.5 + i * (W - 15.0) / (n - 1) for i in range(n)]

CONTROL, JACK, SMALL, LBL = 3.2, 2.9, 2.3, 7.8
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


def knob_t(k):
    k = list(k)
    while len(k) < 12:
        k.append("knob" if len(k) == 10 else False)
    return k


def panel(v):
    acc = v["color"]
    parts = []
    add = parts.append
    add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
    add(f'<rect width="{W}" height="1.6" fill="{acc}"/>')
    add(text(v["title"], 5.0, 11.0, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
    # the big knobs sit on a darker band
    add(f'<rect x="3.0" y="{ROW_Y[0] - 11.5}" width="{W - 6.0}" height="{ROW_Y[1] - ROW_Y[0] + 20.5}" rx="1.4" fill="{BG_DARK}"/>')
    add(text(v["band"], W - 5.0, 13.8, SMALL, FONT_BOLD, acc, anchor="end", spacing=0.1))
    for k in map(knob_t, v["knobs"]):
        kid, label, tip, stock, disp, lo, hi, unit, row, col, kind, norand = k
        big = row < 2
        x, y = (BIG_X[col] if big else SMALL_X[col]), ROW_Y[row]
        r = 5.0 if big else 3.6          # knob radius (RoundBlackKnob / RoundSmallBlackKnob)
        add(text(label, x, y - r - 2.2, CONTROL))
        if kind == "knob" and kid != "LEVEL":
            a = (-0.83 + 1.66 * stock) * math.pi
            rr = r + 1.3
            add(f'<circle cx="{x + rr * math.sin(a):.2f}" cy="{y - rr * math.cos(a):.2f}" r="0.6" fill="{acc}"/>')
        if kind in ("sw3", "sw2"):
            names = unit.split("|")
            ys = [3.4, 0.0, -3.4] if kind == "sw3" else [2.4, -2.4]
            stock_i = int(round(stock))
            for i, n in enumerate(names):
                add(text(n, x + 3.6, y + ys[i] + 0.8, 2.1, FONT_BOLD if i == stock_i else FONT_MED, acc if i == stock_i else SOFT, anchor="start"))
    add(text("HIT", BIG_X[2], ROW_Y[1] - 7.2, CONTROL))
    ry = ROW_Y[v["rand_row"]]
    add(text("RAND", SMALL_X[2], ry - 5.0, CONTROL))
    add(text("NEAR", SMALL_X[3], ry - 5.0, CONTROL))

    # one row of jacks: RAND, NEAR (triggers for the buttons), TRIG, ACC, (OPEN), (V/OCT), OUT
    labs = ["RAND", "NEAR", "TRIG", "ACC"] + ([v["extra_trig"]] if v.get("extra_trig") else []) + [c[0] for c in v["cv"]] + ["OUT"]
    xs = jack_x(len(labs))
    add(f'<line x1="4.0" y1="{JACK_Y - 11.0}" x2="{W - 4.0}" y2="{JACK_Y - 11.0}" stroke="{LINE}" stroke-width="0.3"/>')
    bw = min(12.0, xs[1] - xs[0] - 1.2)
    add(f'<rect x="{xs[-1] - bw / 2:.2f}" y="{JACK_Y - 9.6}" width="{bw:.2f}" height="15.0" rx="1.3" fill="{acc}"/>')
    for i, lab in enumerate(labs):
        add(text(lab, xs[i], JACK_Y - 5.6, JACK, fill=OUT_INK if i == len(labs) - 1 else INK))

    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
           + "\n".join(parts) + "\n</svg>\n")
    path = os.path.join(HERE, v["slug"] + ".svg")
    open(path, "w").write(svg)
    haer_logo.add_to_file(path, x=W / 2, y=126.0, cap=3.0, bg=BG)


DISP = {"pct": "DISP_PCT", "exp": "DISP_EXP", "lin": "DISP_LIN", "int": "DISP_INT", "sw": "DISP_SW"}
KIND = {"knob": 0, "sw3": 1, "sw2": 2}


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def header():
    out = ["// Generated by res/make_voice_panels.py from res/voices.py. Do not edit by hand.",
           "#pragma once", "#include \"VoiceSpec.hpp\"", "", "namespace c909 {", ""]
    for v in VOICES:
        ns = v["cls"].lower()
        ks = list(map(knob_t, v["knobs"]))
        out.append(f"namespace {ns} {{")
        out.append("enum Knob { " + ", ".join(k[0] for k in ks) + ", NUM_KNOBS };")
        out.append("inline const VoiceSpec& spec() {")
        out.append("\tstatic const KnobDef knobs[] = {")
        for k in ks:
            kid, label, tip, stock, disp, lo, hi, unit, row, col, kind, norand = k
            out.append(f"\t\t{{{cstr(kid)}, {cstr(label)}, {cstr(tip)}, {stock:.6f}f, {DISP[disp]}, {float(lo)!r}f, {float(hi)!r}f, {cstr(unit)}, {row}, {col}, {KIND[kind]}, {'true' if norand else 'false'}}},")
        out.append("\t};")
        idx = {k[0]: i for i, k in enumerate(ks)}
        if v["cv"]:
            out.append("\tstatic const CvDef cvs[] = {")
            for lab, tgt in v["cv"]:
                out.append(f"\t\t{{{cstr(lab)}, {-1 if tgt == 'VOCT' else idx[tgt]}}},")
            out.append("\t};")
        else:
            out.append("\tstatic const CvDef* cvs = nullptr;")
        out.append(f"\tstatic const VoiceSpec s = {{{cstr(v['slug'])}, {cstr(v['title'].title())}, NUM_KNOBS, knobs, {len(v['cv'])}, cvs, {v['rand_row']}}};")
        out.append("\treturn s;")
        out.append("}")
        out.append(f"}}  // namespace {ns}")
        out.append("")
    out.append("}  // namespace c909")
    open(os.path.join(HERE, "..", "src", "VoiceSpecs.hpp"), "w").write("\n".join(out) + "\n")


for v in VOICES:
    panel(v)
header()
print("ok", len(VOICES))
