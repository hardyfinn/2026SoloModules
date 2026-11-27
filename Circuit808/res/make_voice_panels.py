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

W, H = 142.24, 128.5  # 28 HP
BG, BG_DARK, LINE, INK, SOFT = "#1F2023", "#16171A", "#3A3B40", "#ECE6D8", "#9A968C"

# ---- LAYOUT (same numbers in src/Voice808.hpp) ----
COL = [11.0, 31.04, 51.08, 71.12, 91.16, 111.2, 131.24]
ROWS = [30.0, 52.0, 74.0]
ROW_CV, ROW_IO = 96.0, 115.0

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
    add(text(v["title"], 6.0, 11.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
    tw = text.width
    # a small ring glyph after the title
    x0 = 6.0 + tw + 4.0
    pts = []
    for i in range(81):
        t = i / 80.0
        pts.append((x0 + t * 26.0, 8.6 - 2.4 * math.exp(-t * 3.0) * math.sin(2 * math.pi * t * 6.0)))
    add('<path d="M' + " L".join(f"{x:.2f} {y:.2f}" for x, y in pts) + f'" stroke="{acc}" stroke-width="0.45" fill="none"/>')
    add(text(v["sub"], W - 6.0, 11.2, SMALL, FONT_MED, SOFT, anchor="end"))

    def section(name, y, c0, c1):
        hy = y - 12.6
        xa = 4.0 if c0 == 0 else COL[c0] - 8.0
        xb = W - 4.0 if c1 == 6 else COL[c1] + 8.0
        add(text(name, xa, hy, SMALL, FONT_BOLD, acc, anchor="start", spacing=0.1))
        if xa + text.width + 2.0 < xb:
            add(f'<line x1="{xa + text.width + 2.0:.2f}" y1="{hy - 0.85}" x2="{xb:.2f}" y2="{hy - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')

    used = {0: set(), 1: set(), 2: set()}
    for r, secs in enumerate(v["sections"]):
        for name, c0, c1 in secs:
            section(name, ROWS[r], c0, c1)
    for k in map(knob_t, v["knobs"]):
        kid, label, tip, stock, disp, lo, hi, unit, row, col, kind, norand = k
        x, y = COL[col], ROWS[row]
        used[row].add(col)
        add(text(label, x, y - LBL, CONTROL))
        if kind == "knob" and kid != "LEVEL":
            a = (-0.83 + 1.66 * stock) * math.pi
            add(f'<circle cx="{x + 6.3 * math.sin(a):.2f}" cy="{y - 6.3 * math.cos(a):.2f}" r="0.62" fill="{acc}"/>')
        if kind in ("sw3", "sw2"):
            names = unit.split("|")
            ys = [3.4, 0.0, -3.4] if kind == "sw3" else [2.4, -2.4]
            for i, n in enumerate(names):
                stock_i = int(round(stock))
                add(text(n, x + 3.6, y + ys[i] + 0.8, 2.1, FONT_BOLD if i == stock_i else FONT_MED, acc if i == stock_i else SOFT, anchor="start"))
        if kid == "AGE":
            add(text("NEW", x - 6.4, y + 6.6, 2.0, FONT_MED, SOFT, anchor="end"))
            add(text("OLD", x + 6.4, y + 6.6, 2.0, FONT_MED, SOFT, anchor="start"))
    add(text("HIT", COL[6], ROWS[0] - LBL, CONTROL))
    used[0].add(6)

    # flow box in the free space of row 3 (or the whole row when it is empty)
    free = [c for c in range(7) if c not in used[2]]
    c0 = free[0]
    bx = 4.0 if c0 == 0 else COL[c0] - 9.0
    by = ROWS[2] - 14.2
    add(f'<rect x="{bx:.2f}" y="{by:.2f}" width="{W - 4.0 - bx:.2f}" height="19.6" rx="1.4" fill="{BG_DARK}"/>')
    fy = ROWS[2] - 4.0
    flow = v["flow"]
    n = len(flow)
    xs = [bx + 6.0 + i * (W - 10.0 - bx - 6.0) / (n - 1) for i in range(n)]
    for i, s in enumerate(flow):
        add(text(s, xs[i], fy, 2.4, FONT_BOLD, INK))
        if i < n - 1:
            add(text("→", (xs[i] + xs[i + 1]) / 2, fy, 2.4, FONT_MED, SOFT))
    cx = (bx + W - 4.0) / 2
    add(text(v["note"], cx, fy + 4.6, 2.1, FONT_MED, SOFT))
    add(f'<circle cx="{bx + 4.0:.2f}" cy="{fy + 8.5}" r="0.62" fill="{acc}"/>')
    add(text("= stock 808 setting   ·   right-click: reset to stock 808", bx + 5.4, fy + 9.3, 2.1, FONT_MED, SOFT, anchor="start"))

    add(text("CV IN", 4.0, ROW_CV - 12.6, SMALL, FONT_BOLD, acc, anchor="start", spacing=0.1))
    add(f'<line x1="{4.0 + text.width + 2.0:.2f}" y1="{ROW_CV - 13.45}" x2="{W - 4.0}" y2="{ROW_CV - 13.45}" stroke="{LINE}" stroke-width="0.3"/>')
    for i, (lab, _) in enumerate(v["cv"]):
        add(text(lab, COL[i], ROW_CV - LBL + 0.6, JACK))

    add(f'<line x1="4.0" y1="{ROW_IO - 13.45}" x2="{W - 4.0}" y2="{ROW_IO - 13.45}" stroke="{LINE}" stroke-width="0.3"/>')
    add(text("TRIG", COL[0], ROW_IO - LBL + 0.6, JACK))
    add(text("ACCENT", COL[1], ROW_IO - LBL + 0.6, JACK))
    add(text("ENV", COL[5], ROW_IO - LBL + 0.6, JACK))
    add(f'<rect x="{COL[6] - 8.0}" y="{ROW_IO - 11.0}" width="16.0" height="17.0" rx="1.4" fill="{acc}"/>')
    add(text("OUT", COL[6], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))
    add(text("ACCENT: gate = accented, 0-5 V = how much", (COL[2] + COL[4]) / 2 + 1.0, ROW_IO + 0.8, 2.1, FONT_MED, SOFT))

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
           "#pragma once", "#include \"VoiceSpec.hpp\"", "", "namespace c808 {", ""]
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
        out.append("\tstatic const CvDef cvs[] = {")
        idx = {k[0]: i for i, k in enumerate(ks)}
        for lab, tgt in v["cv"]:
            out.append(f"\t\t{{{cstr(lab)}, {-1 if tgt == 'VOCT' else idx[tgt]}}},")
        out.append("\t};")
        out.append(f"\tstatic const VoiceSpec s = {{{cstr(v['slug'])}, {cstr(v['title'].title())}, NUM_KNOBS, knobs, {len(v['cv'])}, cvs}};")
        out.append("\treturn s;")
        out.append("}")
        out.append(f"}}  // namespace {ns}")
        out.append("")
    out.append("}  // namespace c808")
    open(os.path.join(HERE, "..", "src", "VoiceSpecs.hpp"), "w").write("\n".join(out) + "\n")


for v in VOICES:
    panel(v)
header()
print("ok", len(VOICES))
