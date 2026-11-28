#!/usr/bin/env python3
# Three mockups of a one-module 808 kit with a screen. Writes SVG + PNG.
import sys, os, subprocess
sys.path.insert(0, "/home/user/2026solomodules/Circuit808/res")
from haer_logo import logo_group

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)
H = 128.5
BG, INK, DIM, LINE = "#1E1F22", "#ECE6D8", "#8C8A84", "#3A3B40"
RED, ORA, YEL, CRM = "#E0533D", "#F08A24", "#E8C547", "#ECE6D8"
SCR, SCRLINE, AMB = "#0E1411", "#22302A", "#F3B04A"
V = [("BD", RED), ("SD", RED), ("LT", RED), ("MT", RED), ("HT", ORA), ("RS", ORA), ("CL", ORA), ("CP", ORA),
     ("MA", YEL), ("CB", YEL), ("CY", YEL), ("OH", CRM), ("CH", CRM)]
FONT = "Inter, sans-serif"
MONO = "DejaVu Sans Mono, monospace"


class P:
    def __init__(s, hp, title, sub, logo_y=125.3, logo_cap=2.6):
        s.w = hp * 5.08; s.g = []; s.hp = hp
        s.g.append(f'<rect x="0" y="0" width="{s.w}" height="{H}" fill="{BG}"/>')
        s.t(title, 6, 13.2, 7.2, anchor="start", weight=800, spacing=0.6)
        s.t(sub, s.w - 6, 12.6, 2.9, anchor="end", fill=DIM, weight=500)
        for x in (7.5, s.w - 7.5):
            for y in (3, H - 3): s.screw(x, y)
        s.g.append(logo_group(s.w, BG, y=logo_y, cap=logo_cap))

    def t(s, txt, x, y, size, anchor="middle", fill=INK, weight=700, spacing=0.15, font=FONT):
        txt = txt.replace("&", "&amp;").replace("<", "&lt;")
        s.g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{weight}" '
                   f'letter-spacing="{spacing}" text-anchor="{anchor}" fill="{fill}">{txt}</text>')

    def screw(s, x, y):
        s.g.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#9a9a9a"/><path d="M{x-1} {y}h2M{x} {y-1}v2" stroke="#555" stroke-width="0.35"/>')

    def jack(s, x, y, label=None, fill=INK, out=False):
        if out:
            s.g.append(f'<rect x="{x-4.2}" y="{y-8.2}" width="8.4" height="13.2" rx="1.2" fill="{fill if fill != INK else "#4a4b50"}"/>')
        s.g.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#d8d8d8" stroke="#9c9c9c" stroke-width="0.3"/>'
                   f'<circle cx="{x}" cy="{y}" r="2.15" fill="#b9b9b9"/><circle cx="{x}" cy="{y}" r="1.55" fill="#111"/>')
        if label: s.t(label, x, y - 4.6, 2.6, fill=("#1B1B1B" if out and fill != INK else INK))

    def knob(s, x, y, r=4.6, label=None, a=0.5, dot=None):
        import math
        s.g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#161616" stroke="#000" stroke-width="0.4"/>'
                   f'<circle cx="{x}" cy="{y}" r="{r*0.78}" fill="#232323"/>')
        ang = math.radians(-150 + 300 * a)
        s.g.append(f'<line x1="{x+math.sin(ang)*r*0.15}" y1="{y-math.cos(ang)*r*0.15}" x2="{x+math.sin(ang)*r*0.85}" y2="{y-math.cos(ang)*r*0.85}" stroke="{INK}" stroke-width="0.7" stroke-linecap="round"/>')
        if label: s.t(label, x, y - r - 2.2, 2.9)

    def btn(s, x, y, w, h, fill, label=None, lit=False, lab_fill="#1B1B1B", size=2.6):
        s.g.append(f'<rect x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}" rx="1.1" fill="{fill}" opacity="{1 if lit else 0.82}"/>')
        if lit: s.g.append(f'<rect x="{x-w/2-0.6}" y="{y-h/2-0.6}" width="{w+1.2}" height="{h+1.2}" rx="1.6" fill="none" stroke="{INK}" stroke-width="0.45"/>')
        if label: s.t(label, x, y + size * 0.36, size, fill=lab_fill, weight=800)

    def led(s, x, y, c="#ff3b30", on=True):
        s.g.append(f'<circle cx="{x}" cy="{y}" r="1.05" fill="{c if on else "#3a2a28"}"/>')

    def screen(s, x, y, w, h):
        s.g.append(f'<rect x="{x-1.4}" y="{y-1.4}" width="{w+2.8}" height="{h+2.8}" rx="2.2" fill="#0a0a0a"/>'
                   f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.2" fill="{SCR}" stroke="{SCRLINE}" stroke-width="0.3"/>')

    def section(s, name, x0, x1, y):
        s.t(name, x0, y, 2.7, anchor="start", fill=ORA, weight=800, spacing=0.35)
        s.g.append(f'<line x1="{x0 + len(name)*2.05 + 2}" y1="{y-0.9}" x2="{x1}" y2="{y-0.9}" stroke="{LINE}" stroke-width="0.35"/>')

    def raw(s, x): s.g.append(x)

    def save(s, name):
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{s.w}mm" height="{H}mm" viewBox="0 0 {s.w} {H}">'
               + "".join(s.g) + "</svg>")
        p = os.path.join(OUT, name + ".svg")
        open(p, "w").write(svg)
        return p


def bar(p, x, y, w, v, c=AMB, h=1.3):
    p.raw(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#1d2a24"/><rect x="{x}" y="{y}" width="{w*v}" height="{h}" fill="{c}"/>')


def stockmark(p, x, y, w, v, h=1.3):
    p.raw(f'<line x1="{x+w*v}" y1="{y-0.5}" x2="{x+w*v}" y2="{y+h+0.5}" stroke="{RED}" stroke-width="0.4"/>')


# ------------------------------------------------------------- 1. STUDIO (pages + soft knobs)
def studio():
    p = P(40, "808 CIRCUITS KIT", "13 TR-808 voices · one screen")
    W = p.w
    sx, sy, sw, sh = 8, 22, W - 16, 46
    p.screen(sx, sy, sw, sh)
    # screen: header tabs
    for i, (n, c) in enumerate(V):
        tx = sx + 2 + i * (sw - 4) / 13
        on = n == "SD"
        p.raw(f'<rect x="{tx+0.3}" y="{sy+1.5}" width="{(sw-4)/13-0.6}" height="4.2" rx="0.6" fill="{c if on else "#17201c"}" stroke="{c}" stroke-width="0.25"/>')
        p.t(n, tx + (sw - 4) / 26, sy + 4.6, 2.4, fill="#111" if on else c, weight=800, font=MONO, spacing=0)
    p.t("SNARE", sx + 3, sy + 11.5, 4.0, anchor="start", fill=INK, weight=800, font=MONO, spacing=0.2)
    p.t("PAGE 1/3  SHELLS", sx + sw - 3, sy + 11.2, 2.6, anchor="end", fill=DIM, font=MONO, spacing=0)
    # waveform preview
    import math
    pts = []
    for i in range(160):
        t = i / 160
        v = math.exp(-t * 5) * (0.6 * math.sin(t * 60) + 0.4 * math.sin(t * 110)) + 0.35 * math.exp(-t * 3) * math.sin(i * 7.7) * math.sin(i * 3.1)
        pts.append(f"{sx+3+t*(sw*0.44):.2f},{sy+22-v*7:.2f}")
    p.raw(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{AMB}" stroke-width="0.35"/>')
    p.t("ENV", sx + 3, sy + 32, 2.2, anchor="start", fill=DIM, font=MONO, spacing=0)
    p.raw(f'<path d="M{sx+9} {sy+33} L{sx+10} {sy+29} L{sx+30} {sy+33}" fill="none" stroke="{CRM}" stroke-width="0.35"/>')
    # 6 params mapped to soft knobs
    params = [("TUNE", "180 Hz", .5, .5), ("TONE", "50 %", .5, .5), ("SNAPPY", "62 %", .62, .5),
              ("RATIO", "1.83x", .35, .35), ("DECAY", "220 ms", .43, .43), ("STRIKE", "1.0 ms", .5, .5)]
    cw = sw / 6
    for i, (n, val, v, st) in enumerate(params):
        cx = sx + cw * i + cw / 2
        p.t(n, cx, sy + 38.5, 2.5, fill=INK, font=MONO, spacing=0)
        p.t(val, cx, sy + 42.2, 2.4, fill=AMB, font=MONO, spacing=0)
        bar(p, cx - cw * 0.38, sy + 43.4, cw * 0.76, v)
        stockmark(p, cx - cw * 0.38, sy + 43.4, cw * 0.76, st)
    p.raw(f'<line x1="{sx+sw*0.5}" y1="{sy+14}" x2="{sx+sw*0.5}" y2="{sy+33}" stroke="{SCRLINE}" stroke-width="0.3"/>')
    # right half of screen: mini mixer
    p.t("KIT MIX", sx + sw * 0.53, sy + 16, 2.3, anchor="start", fill=DIM, font=MONO, spacing=0)
    for i, (n, c) in enumerate(V):
        bx = sx + sw * 0.53 + i * (sw * 0.44 / 13)
        lv = [0.9, .7, .5, .55, .5, .35, .3, .65, .25, .4, .5, .45, .6][i]
        p.raw(f'<rect x="{bx}" y="{sy+18}" width="{sw*0.44/13-0.8}" height="13" fill="#17201c"/>'
              f'<rect x="{bx}" y="{sy+18+13*(1-lv)}" width="{sw*0.44/13-0.8}" height="{13*lv}" fill="{c}" opacity="0.85"/>')
        p.t(n, bx + (sw * 0.44 / 13 - 0.8) / 2, sy + 33.5, 1.9, fill=c, font=MONO, spacing=0)
    # soft knobs under the screen
    for i in range(6):
        p.knob(sx + cw * i + cw / 2, 76.5, r=4.6, a=params[i][2])
    # nav buttons
    p.btn(sx + 6, 85.5, 9, 4.2, "#3a3b40", "PAGE", lab_fill=INK, size=2.2)
    p.btn(sx + 17, 85.5, 9, 4.2, "#3a3b40", "STOCK", lab_fill=INK, size=2.2)
    p.btn(sx + 28, 85.5, 9, 4.2, "#3a3b40", "SHIFT", lab_fill=INK, size=2.2)
    p.t("hold a voice + turn = edit it · STOCK = back to the 808 · SHIFT + voice = hit", sx + 36, 86.3, 2.2, anchor="start", fill=DIM, weight=500)
    # voice buttons = hit + select
    p.section("VOICES", 8, W - 8, 92.3)
    for i, (n, c) in enumerate(V):
        x = 8 + (W - 16) / 13 * (i + 0.5)
        p.btn(x, 96.8, (W - 16) / 13 - 2.2, 4.8, c, n, lit=(n == "SD"))
    # jacks: trig row + out row
    for i, (n, c) in enumerate(V):
        x = 8 + (W - 16) / 13 * (i + 0.5)
        p.jack(x, 105.2)
    p.t("TRIG", 3.2, 106, 2.1, anchor="start", fill=DIM)
    # bottom row
    xs = [14, 28, 42]
    for x, l in zip(xs, ["ACCENT", "POLY", "RESET"]): p.jack(x, 119.3, l)
    p.t("TRIG IN", 28, 125.6, 2.0, fill=DIM, weight=500)
    for x, l in zip([W - 62, W - 50, W - 38], ["POLY", "ENV", "AUX"]): p.jack(x, 119.3, l, out=True)
    p.jack(W - 24, 119.3, "L", fill=ORA, out=True); p.jack(W - 14, 119.3, "R", fill=ORA, out=True)
    return p.save("1-studio-40hp")


# ------------------------------------------------------------- 2. COMPACT (all voices at once, one encoder)
def compact():
    p = P(22, "808 KIT", "compact")
    W = p.w
    sx, sy, sw, sh = 6, 19, W - 12, 47
    p.screen(sx, sy, sw, sh)
    p.t("KIT  ·  STOCK 808  ·  124.0", sx + 2, sy + 3.8, 2.3, anchor="start", fill=DIM, font=MONO, spacing=0)
    rows = [("BD", "TUNE", "49.5Hz", .4), ("SD", "SNAPPY", "62%", .62), ("LT", "DECAY", "1.0x", .5), ("MT", "DECAY", "1.0x", .5),
            ("HT", "TUNE", "0.0oct", .5), ("RS", "RING", "40ms", .44), ("CL", "TUNE", "2.5k", .67), ("CP", "TAIL", "250ms", .52),
            ("MA", "LENGTH", "60ms", .47), ("CB", "DECAY", "400ms", .48), ("CY", "DECAY", "1.2s", .5), ("OH", "DECAY", "450ms", .4), ("CH", "DECAY", "60ms", .3)]
    for i, (n, prm, val, v) in enumerate(rows):
        y = sy + 6.2 + i * 3.06
        c = dict(V)[n]
        sel = n == "CP"
        if sel: p.raw(f'<rect x="{sx+0.8}" y="{y-0.5}" width="{sw-1.6}" height="3.3" rx="0.5" fill="#24342c"/>')
        p.raw(f'<rect x="{sx+1.6}" y="{y+0.15}" width="2.6" height="2.3" rx="0.3" fill="{c}"/>')
        p.t(n, sx + 5.3, y + 2.25, 2.3, anchor="start", fill=INK if sel else c, font=MONO, spacing=0, weight=800)
        p.t(prm, sx + 11.5, y + 2.25, 2.1, anchor="start", fill=DIM if not sel else INK, font=MONO, spacing=0, weight=500)
        p.t(val, sx + 31, y + 2.25, 2.1, anchor="end", fill=AMB, font=MONO, spacing=0, weight=500)
        bar(p, sx + 32.5, y + 0.6, sw - 42, v, c=AMB if sel else "#a07a35", h=1.4)
        lv = [0.9, .7, .1, 0, .3, 0, 0, .8, .2, 0, 0, .4, .6][i]
        p.raw(f'<rect x="{sx+sw-8}" y="{y+0.6}" width="6.5" height="1.4" fill="#17201c"/><rect x="{sx+sw-8}" y="{y+0.6}" width="{6.5*lv}" height="1.4" fill="#5fd38a"/>')
    # encoder + buttons
    p.knob(W / 2, 77.5, r=5.6, a=0.52)
    p.t("EDIT · push = next", W / 2, 86.6, 2.1, fill=DIM, weight=500)
    p.btn(W / 2 - 21, 74.5, 9, 4.2, "#3a3b40", "VOICE", lab_fill=INK, size=2.2)
    p.btn(W / 2 - 21, 81, 9, 4.2, "#3a3b40", "BACK", lab_fill=INK, size=2.2)
    p.btn(W / 2 + 21, 74.5, 9, 4.2, "#3a3b40", "STOCK", lab_fill=INK, size=2.2)
    p.btn(W / 2 + 21, 81, 9, 4.2, RED, "HIT", size=2.2)
    # trig grid: 13 + accent, 7 x 2
    p.section("TRIG", 6, W - 6, 90.5)
    cols = 7
    for i, (n, c) in enumerate(V + [("ACC", INK)]):
        x = 6 + (W - 12) / cols * (i % cols + 0.5)
        y = 96.8 + (i // cols) * 9.6
        p.jack(x, y)
        p.t(n, x, y - 3.8, 2.2, fill=c)
    p.jack(14, 119.6, "POLY T"); p.jack(W - 36, 119.6, "POLY", out=True)
    p.jack(W - 24, 119.6, "L", fill=ORA, out=True); p.jack(W - 13, 119.6, "R", fill=ORA, out=True)
    return p.save("2-compact-22hp")


# ------------------------------------------------------------- 3. CIRCUIT BENCH (circuit diagram you edit + per-voice strips)
def bench():
    p = P(48, "808 CIRCUIT BENCH", "the 808 voice board, one screen", logo_y=125.9, logo_cap=2.1)
    W = p.w
    sx, sy, sw, sh = 8, 20, W - 16, 47
    p.screen(sx, sy, sw, sh)
    p.t("SD  SNARE", sx + 3, sy + 5.5, 3.6, anchor="start", fill=RED, font=MONO, weight=800, spacing=0.2)
    p.t("tap a block to edit · ● stock 808", sx + sw - 3, sy + 5.2, 2.3, anchor="end", fill=DIM, font=MONO, spacing=0)
    # circuit blocks
    blocks = [("TRIG", ["ACCENT 60%", "STRIKE 1.0ms"], False), ("2x BRIDGED-T", ["TUNE 180Hz", "RATIO 1.83x", "DECAY 220ms", "TONE 50%"], True),
              ("NOISE", ["COLOUR 1.0k", "HISS 3.5k"], False), ("VCA", ["SNAPPY 62%", "SNAP DEC 250ms"], False), ("OUT", ["DRIVE 0%", "LEVEL 100%", "AGE 0%"], False)]
    bw, gap = 38, 12
    tot = bw * 5 + gap * 4
    x0 = sx + (sw - 188) / 2
    ys = sy + 9
    pos = []
    for i, (n, ps, sel) in enumerate(blocks):
        bx = x0 + i * (bw + gap)
        by = ys + (8 if n == "NOISE" else 0)
        if n == "NOISE": bx = x0 + 1 * (bw + gap); by = ys + 0
        pos.append((bx, by))
    # layout manually: TRIG -> BRIDGED-T -> VCA... noise feeds VCA from below
    lay = {"TRIG": (x0, ys + 6), "2x BRIDGED-T": (x0 + 46, ys), "NOISE": (x0 + 46, ys + 25), "VCA": (x0 + 104, ys + 25), "OUT": (x0 + 150, ys + 6)}
    hts = {"TRIG": 15, "2x BRIDGED-T": 21, "NOISE": 12, "VCA": 12, "OUT": 17}
    for n, ps, sel in blocks:
        bx, by = lay[n]; hh = hts[n]
        p.raw(f'<rect x="{bx}" y="{by}" width="{bw}" height="{hh}" rx="1" fill="{"#2a3a31" if sel else "#151d19"}" stroke="{AMB if sel else "#4a5a52"}" stroke-width="{0.5 if sel else 0.3}"/>')
        p.t(n, bx + 2, by + 3.6, 2.5, anchor="start", fill=AMB if sel else INK, font=MONO, weight=800, spacing=0)
        for j, s in enumerate(ps):
            k, v = s.rsplit(" ", 1)
            yy = by + 7 + j * 3.4
            p.t(k, bx + 2, yy, 2.1, anchor="start", fill=DIM if not sel else INK, font=MONO, spacing=0, weight=500)
            p.t(v, bx + bw - 6, yy, 2.1, anchor="end", fill=AMB, font=MONO, spacing=0, weight=500)
            p.raw(f'<circle cx="{bx+bw-3.2}" cy="{yy-0.75}" r="0.65" fill="{RED}"/>')
    def arrow(x1, y1, x2, y2):
        p.raw(f'<path d="M{x1} {y1} L{x2} {y2}" stroke="{DIM}" stroke-width="0.35"/><path d="M{x2} {y2} l-1.4 -0.8 v1.6 z" fill="{DIM}"/>')
    tx, ty = lay["TRIG"]; bx, by = lay["2x BRIDGED-T"]; nx, ny = lay["NOISE"]; vx, vy = lay["VCA"]; ox, oy = lay["OUT"]
    arrow(tx + bw, ty + 6, bx, ty + 6)
    p.raw(f'<path d="M{tx+bw+3} {ty+6} V{ny+6} H{nx}" fill="none" stroke="{DIM}" stroke-width="0.35"/><path d="M{nx} {ny+6} l-1.4 -0.8 v1.6 z" fill="{DIM}"/>')
    arrow(nx + bw, ny + 6, vx, ny + 6)
    p.raw(f'<path d="M{bx+bw} {by+8} H{ox}" fill="none" stroke="{DIM}" stroke-width="0.35"/><path d="M{ox} {by+8} l-1.4 -0.8 v1.6 z" fill="{DIM}"/>')
    p.raw(f'<path d="M{vx+bw} {vy+6} H{vx+bw+4} V{oy+12} H{ox}" fill="none" stroke="{DIM}" stroke-width="0.35"/><path d="M{ox} {oy+12} l-1.4 -0.8 v1.6 z" fill="{DIM}"/>')
    # encoders under screen
    names = ["TUNE", "RATIO", "DECAY", "TONE"]
    for i, n in enumerate(names):
        p.knob(sx + 20 + i * 22, 83.5, r=4.8, label=n, a=[.5, .35, .43, .5][i])
    p.knob(sx + 120, 83.5, r=5.6, label="SELECT", a=0.4)
    p.btn(sx + 140, 80.5, 11, 4.2, "#3a3b40", "STOCK", lab_fill=INK, size=2.2)
    p.btn(sx + 140, 87, 11, 4.2, "#3a3b40", "COPY", lab_fill=INK, size=2.2)
    p.btn(sx + 156, 80.5, 11, 4.2, "#3a3b40", "KITS", lab_fill=INK, size=2.2)
    p.btn(sx + 156, 87, 11, 4.2, "#3a3b40", "SHIFT", lab_fill=INK, size=2.2)
    p.t("the 4 knobs follow the block you tap", sx + 188, 84.3, 2.1, anchor="start", fill=DIM, weight=500)
    # 13 voice strips: button, trig, out
    p.section("VOICES", 8, W - 34, 94.5)
    sw2 = (W - 44) / 13
    for i, (n, c) in enumerate(V):
        x = 8 + sw2 * (i + 0.5)
        p.btn(x, 98.8, sw2 - 2.4, 4.4, c, n, lit=(n == "SD"))
        p.jack(x, 106.2)
        p.jack(x, 117.6, out=True)
    p.t("TRIG", 3.2, 107, 2.0, anchor="start", fill=DIM)
    p.t("OUT", 3.2, 118.4, 2.0, anchor="start", fill=DIM)
    p.section("MIX", W - 32, W - 8, 94.5)
    p.jack(W - 26, 104.5, "ACC"); p.jack(W - 14, 104.5, "POLY")
    p.jack(W - 26, 117.6, "L", fill=ORA, out=True); p.jack(W - 14, 117.6, "R", fill=ORA, out=True)
    return p.save("3-circuit-bench-48hp")


paths = [studio(), compact(), bench()]
js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({deviceScaleFactor:2});
for (const f of process.argv.slice(2)){const fs=require('fs');const svg=fs.readFileSync(f,'utf8');
const m=svg.match(/width="([0-9.]+)mm" height="([0-9.]+)mm"/);const w=Math.round(m[1]*6),h=Math.round(m[2]*6);
await p.setViewportSize({width:w,height:h});
await p.setContent(`<html><body style="margin:0;background:#111">${svg.replace(/width="[0-9.]+mm" height="[0-9.]+mm"/,`width="${w}" height="${h}"`)}</body></html>`);
await p.screenshot({path:f.replace('.svg','.png'),clip:{x:0,y:0,width:w,height:h}});}
await b.close()})()"""
open(os.path.join(OUT, "_r.js"), "w").write(js)
subprocess.run(["node", os.path.join(OUT, "_r.js")] + paths, check=True)
os.remove(os.path.join(OUT, "_r.js"))
print("\n".join(p.replace(".svg", ".png") for p in paths))
