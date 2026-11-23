#!/usr/bin/env python3
"""Builds docs/gamelan-sequencing.html (10 A4 pages, inline SVG diagrams).
   python3 docs/make_theory_pdf.py && node tools/pdf.js $PWD/docs/gamelan-sequencing.html out.pdf"""
import os, math
HERE = os.path.dirname(os.path.abspath(__file__))

INK, SOFT, GOLD, RED, TEAK, PAPER, LINE = "#2A1D14", "#7A6650", "#C98F1E", "#B23A22", "#3B2A1E", "#FBF7EE", "#D9CDB6"
BLUE = "#2F6F8F"

def svg(w, h, body):
    return f'<svg viewBox="0 0 {w} {h}" width="100%" xmlns="http://www.w3.org/2000/svg" font-family="Inter, sans-serif">{body}</svg>'

def t(x, y, s, size=11, fill=INK, anchor="middle", weight=500, style=""):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}" {style}>{s}</text>'

def dot(x, y, r, fill=INK, stroke=None):
    s = f' stroke="{stroke}" stroke-width="1.2"' if stroke else ""
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"{s}/>'

def line(x1, y1, x2, y2, c=LINE, w=1, dash=""):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{c}" stroke-width="{w}"{d}/>'

def rect(x, y, w, h, fill, rx=0, stroke=None):
    s = f' stroke="{stroke}" stroke-width="1"' if stroke else ""
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"{s}/>'

# ------------------------------------------------------------ D1 layered time (lancaran, irama tanggung)
def d_layers():
    W, H, x0, x1 = 680, 300, 120, 665
    N = 16; cw = (x1 - x0) / N
    rows = [("gong", 1), ("kenong", 2), ("kempul", 3), ("kethuk", 4), ("saron (balungan)", 5), ("bonang", 6), ("peking", 7), ("kotekan / imbal", 8)]
    b = []
    for i in range(N + 1):
        x = x0 + i * cw
        b.append(line(x, 18, x, H - 30, LINE if i % 4 else "#BBA98A", 1 if i % 4 else 1.4))
    for i in range(N):
        b.append(t(x0 + (i + .5) * cw, H - 14, str(i + 1), 10, SOFT))
    for name, r in rows:
        y = 18 + r * 30
        b.append(t(x0 - 8, y + 4, name, 10.5, INK, "end"))
        for i in range(N):
            beat = i + 1; cx = x0 + (i + .5) * cw
            if r == 1 and beat == 16: b.append(dot(cx, y, 11, "none", GOLD) + dot(cx, y, 5, GOLD))
            if r == 2 and beat % 4 == 0: b.append(dot(cx, y, 7, TEAK))
            if r == 3 and beat in (6, 10, 14): b.append(dot(cx, y, 6, "none", TEAK))
            if r == 4 and beat % 2 == 1: b.append(dot(cx, y, 3, TEAK))
            if r == 5: b.append(dot(cx, y, 4, INK))
            if r == 6: b.append(dot(cx, y, 3, BLUE))
            if r == 7:
                for k in range(2): b.append(dot(x0 + (i + k / 2 + .25) * cw, y, 2.6, BLUE))
            if r == 8:
                for k in range(4): b.append(dot(x0 + (i + k / 4 + .125) * cw, y, 2.2, RED if k % 2 else BLUE))
    b.append(t(x0, H - 0, "beats of one lancaran gong cycle (16 balungan beats); vertical bold lines = gatra (4 beats)", 10, SOFT, "start"))
    return svg(W, H + 6, "".join(b))

# ------------------------------------------------------------ D2 gong cycle clock + end weighting
def d_clock():
    W, H = 680, 300
    cx, cy, R = 150, 150, 112
    b = [f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" stroke="{LINE}" stroke-width="1.4"/>']
    for i in range(16):
        beat = i + 1
        a = -math.pi / 2 + 2 * math.pi * beat / 16
        x, y = cx + R * math.cos(a), cy + R * math.sin(a)
        xl, yl = cx + (R + 18) * math.cos(a), cy + (R + 18) * math.sin(a)
        b.append(t(xl, yl + 4, str(beat), 10, SOFT))
        if beat == 16: b.append(dot(x, y, 14, PAPER, GOLD) + dot(x, y, 6, GOLD))
        elif beat % 4 == 0: b.append(dot(x, y, 8, TEAK))
        elif beat in (6, 10, 14): b.append(dot(x, y, 7, PAPER, TEAK))
        else: b.append(dot(x, y, 3.2, TEAK))
    b.append(t(cx, cy - 8, "LANCARAN", 13, INK, weight=700))
    b.append(t(cx, cy + 10, "the gong closes", 10.5, SOFT))
    b.append(t(cx, cy + 24, "and opens the cycle", 10.5, SOFT))
    # legend
    lx = 30; ly = 292
    # right: Western bar vs gatra
    x0 = 330
    b.append(t(x0, 40, "Where is the strong beat?", 13, INK, "start", 700))
    def strip(y, label, strong, col):
        out = [t(x0, y - 12, label, 11, INK, "start", 600)]
        for k in range(4):
            xx = x0 + 20 + k * 70
            r = 15 if k == strong else 6
            out.append(dot(xx, y + 12, r, col if k == strong else "#CDBFA6"))
            out.append(t(xx, y + 44, str(k + 1), 10.5, SOFT))
        return "".join(out)
    b.append(strip(80, "Western bar (and techno): weight on 1", 0, BLUE))
    b.append(strip(170, "Javanese gatra: weight on 4, the seleh", 3, GOLD))
    b.append(t(x0, 262, "The balungan is heard as moving towards beat 4.", 11, INK, "start"))
    b.append(t(x0, 278, "Big gongs mark the ends of units, not their starts.", 11, INK, "start"))
    return svg(W, H, "".join(b))

# ------------------------------------------------------------ D3 Javanese forms grid
def form_rows():
    def mask(form, b):
        m = set()
        if form == "lancaran":
            N = 16
            if b % 2 == 1: m.add("t")
            if b in (6, 10, 14): m.add("P")
            if b % 4 == 0: m.add("N")
        elif form == "ketawang":
            N = 16
            if b % 2 == 1: m.add("p")
            if b % 4 == 2: m.add("t")
            if b == 12: m.add("P")
            if b % 8 == 0: m.add("N")
        elif form == "ladrang":
            N = 32
            if b % 2 == 1: m.add("p")
            if b % 4 == 2: m.add("t")
            if b in (12, 20, 28): m.add("P")
            if b % 8 == 0: m.add("N")
        else:
            N = 64
            if b % 16 in (4, 12): m.add("t")
            if b % 16 == 0: m.add("N")
        if b == N: m.add("G")
        return m
    return mask

def d_forms():
    mask = form_rows()
    W = 680
    specs = [("lancaran", 16, 1), ("ketawang", 16, 1), ("ladrang", 32, 2), ("gendhing ketuk 2 kerep", 64, 4)]
    b = []; y = 20; x0 = 150; cw = 32
    for name, N, rows in specs:
        b.append(t(x0 - 14, y + 18, name, 11.5, INK, "end", 700))
        b.append(t(x0 - 14, y + 32, f"{N} beats", 10, SOFT, "end"))
        for r in range(rows):
            for i in range(16):
                beat = r * 16 + i + 1
                x = x0 + i * cw; yy = y + r * 30
                b.append(rect(x, yy + 4, cw - 3, 24, "#F2EADB", 3))
                m = mask(name.split()[0], beat)
                s = []
                if "G" in m: b.append(dot(x + cw / 2 - 1.5, yy + 16, 10, PAPER, GOLD)); s.append("G")
                if "N" in m: s.append("N")
                if "P" in m: s.append("P")
                if "t" in m: s.append("t")
                if "p" in m: s.append("p")
                col = GOLD if "G" in m else (TEAK if ("N" in m or "P" in m) else SOFT)
                b.append(t(x + cw / 2 - 1.5, yy + 20, "".join(x for x in s if x != "G") or ("" if "G" not in m else ""), 10.5, col, weight=700))
                if "G" in m: b.append(t(x + cw / 2 - 1.5, yy + 20, "G" if not ("N" in m) else "N", 10, GOLD, weight=800))
        y += rows * 30 + 18
    b.append(t(x0, y + 4, "G gong · N kenong · P kempul · t kethuk · p kempyang   (each row = 16 balungan beats)", 10, SOFT, "start"))
    return svg(W, y + 12, "".join(b))

# ------------------------------------------------------------ D4 balungan walk towards seleh
def d_walk():
    W, H = 680, 186
    names = ["1", "2", "3", "5", "6", "1̇", "2̇"]
    x0, x1, y0 = 70, 660, 150
    bal = [3, 5, 3, 2, 6, 5, 3, 2, 5, 6, 1, 6, 3, 2, 1, 6]
    pos = {"1": 0, "2": 1, "3": 2, "5": 3, "6": 4}
    ys = lambda p: y0 - p * 26
    b = []
    for k, n in enumerate(names[:5]):
        b.append(line(x0, ys(k), x1, ys(k), LINE, 1, "3 3"))
        b.append(t(x0 - 12, ys(k) + 4, n, 11, SOFT, "end"))
    cw = (x1 - x0) / 16
    pts = []
    for i, n in enumerate(bal):
        x = x0 + (i + .5) * cw; y = ys(pos[str(n)])
        pts.append((x, y))
    b.append('<polyline fill="none" stroke="#B9A78A" stroke-width="1.6" points="' + " ".join(f"{x},{y}" for x, y in pts) + '"/>')
    for i, (x, y) in enumerate(pts):
        seleh = i % 4 == 3
        b.append(dot(x, y, 8 if seleh else 4.5, GOLD if seleh else INK))
        if seleh: b.append(t(x, y - 13, "seleh", 9.5, GOLD, weight=700))
    for g in range(4):
        x = x0 + g * 4 * cw
        b.append(rect(x + 2, y0 + 14, 4 * cw - 4, 18, "#F2EADB", 3))
        b.append(t(x + 2 * cw, y0 + 27, f"gatra {g + 1}", 10, SOFT))
    b.append(t(x1, 14, "slendro manyura: stressed tone 6, strong tone 2, avoided 5", 10.5, SOFT, "end"))
    return svg(W, H, "".join(b))

# ------------------------------------------------------------ D5 tunings
def d_tunings():
    W, H = 680, 170
    x0, x1 = 120, 660
    sx = lambda c: x0 + (x1 - x0) * c / 1200
    rows = [("12-TET", [i * 100 for i in range(12)], ["C", "", "D", "", "E", "F", "", "G", "", "A", "", "B"], "#A99", ),
            ("slendro", [0, 240, 475, 720, 955], ["1", "2", "3", "5", "6"], TEAK),
            ("pelog", [0, 120, 258, 539, 675, 785, 943], ["1", "2", "3", "4", "5", "6", "7"], BLUE),
            ("selisir (Bali)", [0, 125, 270, 680, 795], ["i", "o", "e", "u", "a"], GOLD)]
    b = []
    for r, (name, cents, lab, col) in enumerate(rows):
        y = 22 + r * 34
        b.append(line(x0, y, x1, y, LINE, 1.2))
        b.append(t(x0 - 10, y + 4, name, 11, INK, "end", 600))
        for c, l in zip(cents, lab):
            rr = 3 if name == "12-TET" else 6
            fill = col
            if name == "pelog" and l == "4": fill = "#B8C8D2"
            b.append(dot(sx(c), y, rr, fill))
            if l: b.append(t(sx(c), y - 9, l, 10, SOFT))
        b.append(dot(sx(1200), y, 3 if name == "12-TET" else 6, "none", col))
    for c in range(0, 1201, 200):
        b.append(t(sx(c), H - 8, f"{c}", 9.5, SOFT))
    b.append(t(x0 - 10, H - 8, "cents", 9.5, SOFT, "end"))
    return svg(W, H, "".join(b))

# ------------------------------------------------------------ D6 irama ladder + tempo modes
def d_irama():
    W, H = 680, 300
    b = []
    names = ["lancar", "tanggung", "dadi", "wilet", "rangkep"]; D = [1, 2, 4, 8, 16]
    x0 = 110; y = 24
    b.append(t(x0, 12, "Traditional: the fastest part keeps its speed, the balungan beat stretches", 11, INK, "start", 700))
    unit = 8.6
    for k, (n, d) in enumerate(zip(names, D)):
        yy = y + k * 26
        b.append(t(x0 - 10, yy + 12, n, 11, INK, "end", 600))
        beatw = max(d, 2) * unit * 2
        for i in range(int(560 // beatw) + 1):
            xb = x0 + i * beatw
            if xb > x0 + 560: break
            b.append(rect(xb, yy + 2, min(beatw - 2, x0 + 560 - xb), 16, "#F2EADB", 3))
            b.append(dot(xb + 5, yy + 10, 4.2, INK))
            step = beatw / d
            for s in range(d):
                xs = xb + s * step + 5
                if xs < x0 + 560 and s > 0: b.append(dot(xs, yy + 10, 1.8, BLUE))
    y2 = y + 5 * 26 + 24
    b.append(t(x0, y2 - 6, "Fixed (Irama TEMPO switch up): the beat stays put, only the density doubles", 11, INK, "start", 700))
    for k, (n, d) in enumerate(zip(names, D)):
        yy = y2 + k * 22
        b.append(t(x0 - 10, yy + 11, n, 10.5, INK, "end", 600))
        beatw = 70
        for i in range(8):
            xb = x0 + i * beatw
            b.append(rect(xb, yy + 2, beatw - 2, 14, "#EDE3D0", 3))
            b.append(dot(xb + 4, yy + 9, 3.6, INK))
            for s in range(1, d):
                b.append(line(xb + 4 + s * (beatw - 6) / d, yy + 5, xb + 4 + s * (beatw - 6) / d, yy + 13, BLUE, 1))
    return svg(W, y2 + 5 * 22 + 6, "".join(b))

# ------------------------------------------------------------ D7 Java elaboration over 2 gatra beats
def d_elab():
    W, H = 680, 250
    x0, x1 = 140, 660
    steps = 16; cw = (x1 - x0) / steps
    b = []
    for i in range(steps + 1):
        b.append(line(x0 + i * cw, 16, x0 + i * cw, H - 20, LINE if i % 4 else "#BBA98A", 1 if i % 4 else 1.4))
    rows = [
        ("balungan (saron)", {0: "3", 4: "2", 8: "6", 12: "5"}, INK),
        ("peking  x x y y", {0: "3", 2: "3", 4: "2", 6: "2", 8: "6", 10: "6", 12: "5", 14: "5"}, BLUE),
        ("bonang mipil", {0: "3", 2: "2", 4: "3", 6: "2", 8: "6", 10: "5", 12: "6", 14: "5"}, TEAK),
        ("imbal A", {1: "5", 3: "6", 5: "5", 7: "6", 9: "1", 11: "2", 13: "1", 15: "2"}, BLUE),
        ("imbal B", {0: "2", 2: "3", 4: "2", 6: "3", 8: "6", 10: "5", 12: "6", 14: "5"}, RED),
        ("A + B together", {i: "•" for i in range(16)}, INK),
    ]
    for r, (name, notes, col) in enumerate(rows):
        y = 34 + r * 34
        b.append(t(x0 - 10, y + 4, name, 10.5, INK, "end", 600))
        for s, n in notes.items():
            cx = x0 + (s + .5) * cw
            if name.startswith("A +"):
                b.append(dot(cx, y, 4, BLUE if s % 2 else RED))
            else:
                b.append(dot(cx, y, 9, "#F2EADB" if col != INK else "#E8DCC4"))
                b.append(t(cx, y + 4, n, 11, col, weight=700))
    b.append(t(x0, H - 4, "four balungan beats (dadi feel, 4 steps per beat); illustrative patterns, real garap depends on the piece", 10, SOFT, "start"))
    return svg(W, H + 4, "".join(b))

# ------------------------------------------------------------ D8 angsel + ombak
def d_angsel_ombak():
    W, H = 680, 230
    b = []
    x0, x1 = 30, 330; steps = 16; cw = (x1 - x0) / steps
    b.append(t(x0, 14, "Angsel over one gatra (16 steps)", 11.5, INK, "start", 700))
    for i in range(steps):
        x = x0 + i * cw
        inbrk = i >= 8
        b.append(rect(x + 1, 30, cw - 2, 30, "#EDE3D0" if not inbrk else "#F7EFE2", 2))
        if not inbrk: b.append(dot(x + cw / 2, 45, 3, BLUE if i % 2 else RED))
        if i in (8, 11, 14): b.append(dot(x + cw / 2, 45, 7.5, RED))
    b.append(t(x0 + 4 * cw, 78, "kotekan runs", 10, SOFT))
    b.append(t(x0 + 12 * cw, 78, "3 + 3 + 2 unison accents, then breath", 10, RED))
    b.append(line(x0 + 7.5 * cw, 22, x0 + 7.5 * cw, 66, GOLD, 1.4, "3 2"))
    b.append(t(x0 + 7.4 * cw, 98, "kendang cue", 10, GOLD, "end"))
    # ombak
    X0, X1 = 370, 665
    b.append(t(X0, 14, "Ombak: umbang + isep = beating", 11.5, INK, "start", 700))
    pts1, pts2, env = [], [], []
    for i in range(0, 296):
        tt = i / 295 * 0.5
        y1 = 50 + 16 * math.sin(2 * math.pi * 40 * tt)
        y2 = 50 + 16 * math.sin(2 * math.pi * 46 * tt)
        pts1.append(f"{X0 + i},{y1:.1f}"); pts2.append(f"{X0 + i},{y2:.1f}")
        e = abs(math.cos(math.pi * 6 * tt))
        env.append((X0 + i, 130 - 30 * e, 130 + 30 * e))
    b.append(f'<polyline fill="none" stroke="{TEAK}" stroke-width="1" points="{" ".join(pts1)}"/>')
    b.append(f'<polyline fill="none" stroke="{GOLD}" stroke-width="1" points="{" ".join(pts2)}"/>')
    b.append('<polygon fill="#E9D3A6" points="' + " ".join(f"{x},{a:.1f}" for x, a, _ in env) + " " + " ".join(f"{x},{c:.1f}" for x, _, c in reversed(env)) + '"/>')
    b.append(t(X0, 85, "pengumbang (lower) and pengisep (higher)", 10, SOFT, "start"))
    b.append(t(X0, 178, "sum: loudness pulses at the difference frequency", 10, SOFT, "start"))
    b.append(t(X0, 200, "Irama OMBAK keeps that difference in Hz, so high and", 10, INK, "start"))
    b.append(t(X0, 214, "low instruments shimmer at the same speed.", 10, INK, "start"))
    return svg(W, H, "".join(b))

# ------------------------------------------------------------ D9 kotekan styles
def d_kotekan():
    W = 680
    styles = [
        ("norot", "polos and sangsih alternate the target and its upper neighbour", [(0, "P"), (1, "S"), (0, "P"), (1, "S"), (0, "P"), (1, "S"), (0, "P"), (1, "S")]),
        ("nyog cag", "one line split note by note between the two players", [(1, "P"), (2, "S"), (3, "P"), (2, "S"), (1, "P"), (2, "S"), (1, "P"), (0, "S")]),
        ("telu", "three tones: polos the lower two, sangsih the upper two, sharing the middle", [(1, "B"), (0, "P"), (2, "S"), (1, "B"), (0, "P"), (2, "S"), (1, "B"), (0, "P")]),
        ("empat", "four tones: polos the lower two, sangsih the upper two, plus kempyung (both)", [(0, "K"), (2, "S"), (1, "P"), (3, "S"), (0, "K"), (2, "S"), (1, "P"), (3, "S")]),
    ]
    b = []; y = 10
    x0 = 140; cw = 62
    for name, desc, seq in styles:
        b.append(t(x0 - 12, y + 34, name, 12, INK, "end", 700))
        b.append(t(x0, y + 10, desc, 10, SOFT, "start"))
        for lvl in range(4):
            b.append(line(x0, y + 62 - lvl * 11, x0 + 8 * cw, y + 62 - lvl * 11, LINE, .8, "2 3"))
        for i, (tone, who) in enumerate(seq):
            cx = x0 + (i + .5) * cw
            yy = y + 62 - tone * 11
            if who == "P": b.append(dot(cx, yy, 6, BLUE))
            elif who == "S": b.append(dot(cx, yy, 6, RED))
            elif who == "B":
                b.append(f'<path d="M{cx - 6},{yy} a6,6 0 0 1 12,0 z" fill="{BLUE}"/><path d="M{cx - 6},{yy} a6,6 0 0 0 12,0 z" fill="{RED}"/>')
            elif who == "K":
                b.append(dot(cx, yy, 6, BLUE)); b.append(dot(cx, yy - 33, 6, RED)); b.append(line(cx, yy - 6, cx, yy - 27, "#C9B79A", 1))
        y += 76
    b.append(dot(x0, y + 4, 5, BLUE) + t(x0 + 10, y + 8, "polos (on the beat side)", 10, INK, "start"))
    b.append(dot(x0 + 180, y + 4, 5, RED) + t(x0 + 190, y + 8, "sangsih (off the beat side)", 10, INK, "start"))
    b.append(t(x0 + 360, y + 8, "rows = tones above the target", 10, SOFT, "start"))
    return svg(W, y + 16, "".join(b))

# ------------------------------------------------------------ D10 Gatra land + polymeter
def d_gatra():
    W, H = 680, 210
    b = []
    x0, x1 = 120, 660; steps = 16; cw = (x1 - x0) / steps
    b.append(t(x0, 12, "LAND: where the gong falls in the last bar of a 16-beat cycle (16ths)", 11, INK, "start", 700))
    for r, (name, pos) in enumerate([("LAND end", 12), ("LAND middle", 14), ("LAND one", 16)]):
        y = 26 + r * 30
        b.append(t(x0 - 10, y + 13, name, 10.5, INK, "end", 600))
        for i in range(steps + 1):
            x = x0 + i * cw
            if i < steps: b.append(rect(x + 1, y, cw - 2, 20, "#F2EADB" if i % 4 else "#E6D8BE", 2))
        for k in range(0, 17, 4):
            if k < 16: b.append(dot(x0 + (k + .5) * cw, y + 10, 3, INK))
        gx = x0 + (pos + .5) * cw if pos < 16 else x1 + 10
        b.append(dot(gx, y + 10, 8, PAPER, GOLD) + dot(gx, y + 10, 3.5, GOLD))
    b.append(t(x1 + 10, 120, "next bar's 1", 9.5, SOFT, "middle"))
    y = 142
    b.append(t(x0, y, "CYCLE 20 against a 16-beat loop: the gong comes back to the bar's one every 80 beats", 11, INK, "start", 700))
    for k in range(80):
        x = x0 + k * (x1 - x0) / 80
        if k % 4 == 0: b.append(line(x, y + 12, x, y + 26, "#BBA98A", 1))
        if (k + 1) % 20 == 0: b.append(dot(x + 3, y + 40, 6, GOLD))
        if k % 16 == 0: b.append(dot(x + 3, y + 54, 4, BLUE))
    b.append(t(x0 - 10, y + 44, "gong", 10, INK, "end"))
    b.append(t(x0 - 10, y + 58, "loop start", 10, INK, "end"))
    return svg(W, y + 66, "".join(b))

# ------------------------------------------------------------ pages
P = []
def page(html): P.append(f'<section class="page">{html}<div class="foot">Gamelan sequencing · Irama &amp; Gatra · {len(P) + 1}</div></section>')

page(f"""
<div class="kicker">Theory notes for the Irama plugin</div>
<h1>Gamelan Sequencing</h1>
<p class="lead">How Javanese and Balinese gamelan organise time, melody and interlocking parts, and how the two new modules, <b>Irama</b> and <b>Gatra</b>, borrow those ideas.</p>
<div class="toc">
<div><span>1</span>Layered time: a whole orchestra as one sequencer</div>
<div><span>2</span>The gong cycle and end-weighting</div>
<div><span>3</span>Javanese forms: lancaran, ketawang, ladrang, gendhing</div>
<div><span>4</span>Balungan, gatra and seleh, laras and pathet</div>
<div><span>5</span>Irama: density levels and tempo</div>
<div><span>6</span>Javanese elaboration: peking, bonang, imbal</div>
<div><span>7</span>Bali: gong kebyar, angsel and ombak</div>
<div><span>8</span>Kotekan: four ways to interlock</div>
<div><span>9</span>From the gong to the dance floor, and further reading</div>
</div>
<h2>Why gamelan is a sequencing tradition</h2>
<p>A gamelan is an ensemble of mostly bronze percussion: gongs, kettle gongs and metallophones, with drums and, in Java, voice, flute and bowed string. Its music is built from layers that move at fixed ratios to one another. The largest gong marks the end of a long cycle; smaller gongs divide that cycle; a core melody moves at a steady pulse; and faster instruments fill each pulse with patterns derived from that melody. Once you know the form and the melody, most parts can be worked out by rule. That is what makes it such a good model for a sequencer: one clock and one skeleton generate a whole texture.</p>
<p>Two traditions are covered here. <b>Central Javanese</b> gamelan is calm and layered, with long cycles and gradual changes of density called irama. <b>Balinese gong kebyar</b>, a twentieth-century style, is fast and explosive, built on interlocking pairs (kotekan), sudden unison breaks (angsel) and paired instruments tuned apart so they shimmer (ombak).</p>
<div class="note"><b>A word on accuracy.</b> Practice varies between regions, schools, ensembles and pieces, and many terms have several uses. These pages describe the common textbook picture, simplified where the modules simplify it. Where the modules make their own choices (for example the Balinese cycle layouts and the pathet weightings) the text says so. The further reading on the last page goes deeper.</div>
<p class="small">Notation: Javanese cipher notation (kepatihan) numbers the tones 1 to 7; a dot above or below a number means the octave above or below. Balinese tones are named ding, dong, deng, dung, dang, written here as i, o, e, u, a.</p>
""")

page(f"""
<h2><span class="num">1</span>Layered time: a whole orchestra as one sequencer</h2>
<p>The clearest way to picture gamelan is as stratified time. Each layer has a fixed relationship to the layer below. In a lancaran, a short and lively Javanese form, a full cycle is sixteen balungan beats. The diagram shows one cycle: the gong once, the kenong four times, the kempul three times, the kethuk on every other beat, the saron playing the balungan on every beat, and faster instruments doubling and quadrupling that pulse.</p>
<div class="fig">{d_layers()}</div>
<p>Three families do the work:</p>
<ul>
<li><b>Colotomic instruments</b> (gong ageng, kempul, kenong, kethuk, kempyang) mark positions in the cycle. They are the structure, like the bar lines, phrase marks and section changes of a score. The word colotomy (from the Greek for "cutting into parts") was coined by the Dutch musicologist Jaap Kunst for this role.</li>
<li><b>Balungan instruments</b> (saron, demung, slenthem) play the skeleton melody, one note per beat.</li>
<li><b>Elaborating instruments</b> (bonang, peking, gendèr, gambang, voice, rebab and others) play faster lines worked out from the balungan, each with its own idiom. In Bali the gangsa pairs play kotekan, and the reyong plays its own figurations.</li>
</ul>
<p>The drummer (kendhang in Java, kendang in Bali) leads: they set the tempo, cue changes and endings, and in Bali signal the breaks.</p>
<h3>In the modules</h3>
<p><b>Irama</b> reproduces this stack from one clock: colotomic triggers (GONG, KENONG, KEMPUL, KETHUK, KEMPYANG), the balungan (SARON), and three elaborating lines (PEKING, BONANG, and the POLOS / SANGSIH pair). The clock is the pulse of the fastest line, so everything stays in ratio. <b>Gatra</b> keeps only the structural layers and puts them on drum roles over a straight kick.</p>
""")

page(f"""
<h2><span class="num">2</span>The gong cycle and end-weighting</h2>
<p>A unit of music closed by the big gong is a <b>gongan</b>. Musicians think of it as a circle rather than a line: the gong stroke ends one cycle and begins the next, and a piece is a gongan, or a few, repeated many times with variations in density and tempo. The circular picture below is how many teachers draw a lancaran.</p>
<div class="fig">{d_clock()}</div>
<p>The important part for a techno producer is the right-hand side. In Western music, and very strongly in four-on-the-floor dance music, the first beat of the bar is heavy and things begin there. In Javanese gamelan the weight falls at the <b>end</b>: the balungan is grouped into four-beat units called <b>gatra</b>, and the fourth beat, the <b>seleh</b>, is the goal of the phrase. The kenong and the gong fall on these final beats. A gatra is heard as moving towards its last note.</p>
<p>That is why gamelan can feel "back to front" when you first hear it alongside a drum machine: the big events seem to arrive a beat early, or late, depending on which way you count. Many musicians who fuse the two either rotate the gamelan cycle so its gong lands on the downbeat, or keep the gamelan orientation and let the kick sit across it.</p>
<h3>Two ways of reading the same cycle</h3>
<table>
<tr><th></th><th>Gamelan reading</th><th>Dance-floor reading</th></tr>
<tr><td>Where the cycle "is"</td><td>Arriving at the gong</td><td>Starting from the one</td></tr>
<tr><td>Strongest beat of four</td><td>4 (seleh)</td><td>1 (downbeat)</td></tr>
<tr><td>Phrase shape</td><td>Approach and land</td><td>Hit and decay</td></tr>
<tr><td>In Gatra</td><td>LAND at END</td><td>LAND at ONE</td></tr>
</table>
<p>Gatra's LAND knob is the bridge between these two readings: it slides every colotomic stroke from the end of the cycle (gamelan) through the "and" of the last beat (a pickup) to the downbeat of the next bar (techno). Irama always keeps the gamelan orientation.</p>
""")

page(f"""
<h2><span class="num">3</span>Javanese forms</h2>
<p>A <b>form</b> (bentuk) is a fixed pattern of colotomic strokes for one gongan. The four forms in Irama cover the short, lively end of the repertoire (lancaran), two medium forms used for many well-known pieces (ketawang, ladrang), and a long, stately form (gendhing). Each square below is one balungan beat.</p>
<div class="fig">{d_forms()}</div>
<ul>
<li><b>Lancaran</b>: 16 beats; kenong every 4 beats, kempul in between (but not right after the gong, a gap called <i>wela</i>), kethuk on the off-beats. Often used for marches and lively pieces, frequently in irama lancar or tanggung.</li>
<li><b>Ketawang</b>: 16 beats with two kenong strokes, one kempul, and kethuk and kempyang alternating to divide the time more finely. Many ketawang are sung, and move into slower irama.</li>
<li><b>Ladrang</b>: 32 beats with four kenongan (kenong phrases). Kempul falls in the middle of the second, third and fourth kenongan, never right after the gong.</li>
<li><b>Gendhing</b>: long forms named by how many kethuk strokes fall in each kenongan and how densely. Irama implements one, <i>ketuk 2 kerep</i>: four kenongan of 16 beats, kethuk on beats 4 and 12 of each. Real gendhing have further sections (merong, inggah), which Irama leaves out.</li>
</ul>
<h3>Listening tip</h3>
<p>When you listen to a recording, find the gong, then count back four beats to find the last kenong. Once you hear those two, the kempul and kethuk fall into place. The same habit makes Irama's display easy to read: the gong beat is circled, kenong beats have a bar above them, and the current beat is highlighted.</p>
<div class="note">Kempul and kenong are tuned to the melody: they usually match the balungan tone of the beat they fall on, which is how the structure "agrees" with the tune. Irama's demo voices follow this, and Bell Foundry or a tuned drum can do the same from the SARON pitch.</div>
""")

page(f"""
<h2><span class="num">4</span>Balungan, gatra and seleh; laras and pathet</h2>
<p>The <b>balungan</b> ("skeleton") is the core melody notated in cipher numbers, usually grouped in gatra of four: <span class="mono">3 5 3 2 · 6 5 3 2 · 5 6 1 6 · 3 2 1 6</span>. The last tone of each gatra is its seleh. Melodies tend to move by step and to approach their seleh from above; typical cadences are stepwise descents.</p>
<div class="fig">{d_walk()}</div>
<p>Irama builds its balungan the way this suggests: it picks each gatra's seleh first, then walks <b>backwards</b> from it with mostly small steps, so each gatra arrives at its landing tone. The gong gatra lands on the stressed tone of the pathet; kenong gatras favour the strong tones (FOCUS sets how strongly). MUTATE rewrites a gatra at each gong, so the tune slowly evolves while the form stays fixed.</p>
<h3>Laras: the two tuning systems</h3>
<div class="fig">{d_tunings()}</div>
<p><b>Slendro</b> has five roughly equal steps per octave (about 240 cents each). <b>Pelog</b> has seven unequal steps, but most pieces use a five-tone subset; tone 4 (shown pale) is rare. Balinese <b>selisir</b> is a five-tone pelog-type scale and the default mode of gong kebyar. Every set is tuned individually, so these numbers are only rough guides; Irama uses them by default and can snap to 12-TET (right-click menu) for Western gear.</p>
<h3>Pathet: which tones matter</h3>
<table>
<tr><th>Pathet</th><th>Stressed</th><th>Strong</th><th>Avoided / rare</th><th>Typical cadence</th></tr>
<tr><td>slendro nem</td><td>2</td><td>5</td><td>1</td><td>6 5 3 2</td></tr>
<tr><td>slendro sanga</td><td>5</td><td>1</td><td>3</td><td>2 1 6 5</td></tr>
<tr><td>slendro manyura</td><td>6</td><td>2</td><td>5</td><td>3 2 1 6</td></tr>
<tr><td>pelog lima / nem</td><td>1 / 5</td><td>5 / 1</td><td>4, 7</td><td>5 3 2 1 / 2 1 6 5</td></tr>
<tr><td>pelog barang</td><td>6</td><td>2</td><td>4, 1</td><td>3 2 7 6</td></tr>
</table>
<p class="small">Rule-of-thumb tone roles (after Mantle Hood); scholars dispute them. Irama's weights are designed from them, not measured.</p>
""")

page(f"""
<h2><span class="num">5</span>Irama: density levels and tempo</h2>
<p><b>Irama</b> (the word the plugin is named after) is the relationship between the balungan pulse and the fastest elaborating parts. Each level doubles the number of elaborating notes per balungan beat: <b>lancar</b> (1), <b>tanggung</b> (2), <b>dadi</b> (4), <b>wilet</b> (8) and <b>rangkep</b> (16). The standard Javanese naming counts the peking or similar parts; the module counts its fastest line.</p>
<div class="fig">{d_irama()}</div>
<p>In Java, irama and tempo are linked. When the drummer moves the ensemble into a higher irama, the fast instruments keep roughly the same speed while the balungan beat slows down to make room: a gatra that took two seconds might take four, then eight. A piece can travel through several irama levels and back, and these transitions, slowing and filling in or speeding and thinning out, are among the most expressive moments in the music. They happen gradually, usually across a gatra or more, led by the kendhang.</p>
<p>This is very different from the dance-floor idea of a fixed BPM. Irama therefore has a <b>TEMPO</b> switch:</p>
<ul>
<li><b>TRAD</b>: the clock is the fast pulse. Raising IRAMA stretches each balungan beat, so the gongs get further apart while the decoration stays at clock speed. This is the Javanese way, and gives long, slowly breathing structures.</li>
<li><b>FIXED</b>: each balungan beat is always four clock pulses (a quarter note with 16th clocks). Raising IRAMA only doubles the density of the decoration, up to 64ths at rangkep. This keeps the gong cycle locked to bars, which is what you want against Gatra and a kick.</li>
</ul>
<p>Level changes are latched at the next gatra, as in practice, where the change is cued and completed at a structural point. <b>SUWUK</b>, the Javanese word for the ending, drops to lancar and stops on the next gong, the way a piece slows and closes on the final gong stroke.</p>
<div class="note"><b>Arrangement idea.</b> Irama levels are a ready-made energy map: lancar for an intro, tanggung and dadi as the track builds, an angsel or a drop at a gong, then back down. Patching a slow envelope or Long Form into the IRAMA CV input (2 V per level) turns that into a whole track.</div>
""")

page(f"""
<h2><span class="num">6</span>Javanese elaboration: peking, bonang and imbal</h2>
<p>Javanese elaborating parts are derived from the balungan by <b>garap</b>, a body of shared conventions. Each instrument has stock ways of treating a pair or a gatra of balungan tones. Three of the simplest are built into Irama.</p>
<div class="fig">{d_elab()}</div>
<ul>
<li><b>Peking</b>, the smallest saron, plays each balungan tone twice, typically pairing tones as "x x y y". In many styles it anticipates the balungan, playing a tone just before the saron reaches it. Irama's PEKING plays this pattern an octave up, at a density set by irama.</li>
<li><b>Bonang</b>, a double row of kettle gongs, has several techniques. <b>Mipil</b> ("picking out one by one") alternates the two tones of a balungan pair, landing on the second. Irama's BONANG uses this in Java mode.</li>
<li><b>Imbal</b> is interlocking between two players: the bonang barung and bonang panerus, or two saron. Each plays a fixed alternating figure in the gaps of the other, so neither part makes sense alone but together they make a continuous fast line. Imbal usually ends with a short flourish (sekaran) that runs into the seleh. Irama's IMBAL setting gives this as POLOS (part A) and SANGSIH (part B).</li>
</ul>
<h3>Why interlocking matters for sequencing</h3>
<p>Interlocking parts are a superb source of rhythmic life: two simple, repetitive parts combine into a complex line, and if you send them to two different voices, pan them apart or give them different filters, the line jumps between places. Mute one and you get a syncopated part with gaps; bring it back and the line fills in. That on/off gesture is one of the most useful tools in the plugin for working against loop fatigue.</p>
<h3>What Irama does not attempt</h3>
<p>The soft-playing instruments (gendèr, gambang, siter, suling, rebab and voice) carry much of the expressive and modal content of Javanese music, and their garap depends on the piece, the pathet and the musician. Irama leaves these out; the SELEH output (the landing tone, an octave down) is a good starting point for a sustained or vocal-like voice of your own.</p>
""")

page(f"""
<h2><span class="num">7</span>Bali: gong kebyar, angsel and ombak</h2>
<p><b>Gong kebyar</b> emerged in north Bali in the early twentieth century (the first reported performances are usually dated around 1915) and quickly became the island's most widespread ensemble. <i>Kebyar</i> means something like "to flare up" or "burst open". Compared with Javanese gamelan it is fast, loud and changeable: pieces switch suddenly between tempos, textures and sections, and the whole group often plays tight unison rhythms at high speed.</p>
<p>The ensemble includes paired <b>gangsa</b> metallophones (pemade and the higher kantilan), which play kotekan; the <b>ugal</b>, which leads the melody; the <b>reyong</b>, a row of kettle gongs played by four players; <b>calung</b> (or jublag) and <b>jegogan</b> for the core melody (pokok); a set of gongs (<b>gong</b>, <b>kempur</b>, <b>kemong</b>) and the <b>kempli</b> keeping the beat; cymbals (ceng-ceng); and two drums, lanang and wadon, whose interlocking patterns lead the ensemble.</p>
<div class="fig">{d_angsel_ombak()}</div>
<h3>Angsel</h3>
<p>An <b>angsel</b> is a rhythmic break: the kotekan stops, the ensemble plays a short unison accent pattern, and there is a held breath before the music resumes. Angsel are cued by the drummers and are closely tied to dance, marking a dancer's sudden movements and freezes. Irama's ANGSEL plays a 3 + 3 + 2 accent figure over the last two beats of a gatra (a knob sets its chance; the button or trigger forces one at the next gatra), and outputs a gate while it happens, which Gatra can use as a drop.</p>
<h3>Kebyar</h3>
<p>Many kebyar pieces open with a <i>kebyar</i> section: loud, irregular unison strikes by the whole ensemble, often with gongs, in rhythms that sound free but are tightly rehearsed. Irama's KEBYAR gate produces irregular unison bursts with chords built on the current tones, with a gong at the start and at the end.</p>
<h3>Ombak</h3>
<p>Balinese instruments are made in pairs, tuned slightly apart: the lower <b>pengumbang</b> and the higher <b>pengisep</b>. Played together, the pairs beat against each other, producing the shimmer called <b>ombak</b> ("wave"). Ensembles aim for a fairly consistent beating speed across the range, which means the higher pairs are detuned by fewer cents than the lower ones. Irama's OMBAK knob sets the beat in Hz (0 to 10); each pitch output becomes a two-channel umbang / isep pair, so a polyphonic voice plays both and the shimmer appears.</p>
<h3>Balinese forms in Irama</h3>
<p>Irama's BALI forms are named after common Balinese structures: <b>gilak</b> (an 8-beat ostinato associated with processional and baris music), <b>bapang</b>, and the <b>tabuh</b> forms of the lelambatan repertoire. The stroke layouts in the module are simplified (gong at the end, kempur halfway, kemong on the quarters, kempli on every beat), not exact transcriptions.</p>
""")

page(f"""
<h2><span class="num">8</span>Kotekan: four ways to interlock</h2>
<p><b>Kotekan</b> is the Balinese art of interlocking figuration. Two parts, <b>polos</b> (generally the part on or near the beat) and <b>sangsih</b> (generally the part that fills between), are each simple and repetitive, but together form a fast composite line, often at speeds no single player could manage. The figure is built to lead into the next tone of the core melody, so the kotekan always anticipates where the music is going.</p>
<div class="fig">{d_kotekan()}</div>
<ul>
<li><b>Norot</b>: each part repeats a tone, the target and its upper neighbour, in alternation, then moves to the next target a little before it arrives. It is the simplest, steadiest kotekan, and common on reyong.</li>
<li><b>Nyog cag</b>: a single melodic line divided note by note between the two players, like two people completing each other's sentences. Neither part has two consecutive notes.</li>
<li><b>Telu</b> ("three"): the figure uses three neighbouring tones. Polos takes the lower two, sangsih the upper two, and both play the middle tone, giving occasional unison hits within a constant stream.</li>
<li><b>Empat</b> ("four"): four tones; polos the lower two, sangsih the upper two. Where polos plays the lowest tone, sangsih often adds the tone a fourth step above at the same moment, the interval called <b>kempyung</b>, so empat has a chordal ring.</li>
</ul>
<p>The patterns drawn above are illustrative; real kotekan are composed for each piece and change with the melody. Irama's KOTEKAN knob applies these four principles (plus Javanese imbal) to whatever balungan it is playing. Gatra applies three of them (nyog cag, telu, empat) to rhythm alone: hat triggers split between POLOS and SANGSIH.</p>
<h3>What each style does to a groove</h3>
<table>
<tr><th>Style</th><th>As melody (Irama)</th><th>As hats (Gatra)</th></tr>
<tr><td>nyog cag</td><td>Running stepwise line</td><td>Straight 16ths split on / off: classic</td></tr>
<tr><td>telu</td><td>Three-tone ripple with unisons</td><td>Repeats every three 16ths: a 3-against-4 cross rhythm, accents on the shared hits</td></tr>
<tr><td>empat</td><td>Four tones with kempyung chords</td><td>Steady with doubled accents</td></tr>
</table>
""")

page(f"""
<h2><span class="num">9</span>From the gong to the dance floor</h2>
<p>Gatra takes the structural ideas above and places them over a four-on-the-floor kick. Its two main controls answer the two biggest differences between the traditions.</p>
<div class="fig">{d_gatra()}</div>
<ul>
<li><b>LAND</b> resolves end-weighting versus downbeat-weighting (page 3). Leave it at END for gamelan-style anticipation, put it in the middle for a pickup into the bar, or at ONE for a gong that behaves like a crash on the downbeat.</li>
<li><b>CYCLE</b> sets the gongan length. 8, 16, 24 and 32 beats line up with bars. 12, 20 and 28 do not, so the gong and its kenong and kempul strokes drift across the bar and only realign after several cycles. This is polymeter, the source of slow, self-changing patterns without any randomness.</li>
</ul>
<h3>A starting patch</h3>
<ol>
<li>One clock at 16ths (for example 124 BPM) into both modules' CLOCK, one reset into both RESETs.</li>
<li>Irama: STYLE JAVA, FORM lancaran, PATHET pelog nem, TEMPO FIXED, IRAMA tanggung, KOTEKAN nyog cag. POLY outputs into Bell Foundry (gamelan profile); SELEH into a bass voice.</li>
<li>Gatra: CYCLE 16, LAND END, KOTEKAN telu. KICK, KENONG, POLOS and SANGSIH into a 909 (kick, clap, closed hat, shaker); GONG into a crash or a long tom.</li>
<li>Irama ANGSEL out into Gatra DROP: every angsel in the melody becomes a drop in the drums.</li>
<li>Over a few minutes: raise IRAMA to dadi, turn LAND to ONE, switch CYCLE to 20, and press SUWUK to end on a gong.</li>
</ol>
<h3>Further reading</h3>
<p class="small">Jennifer Lindsay, <i>Javanese Gamelan</i> (Oxford, 1979 / 1992): a short, clear introduction. · Sumarsam, <i>Gamelan: Cultural Interaction and Musical Development in Central Java</i> (Chicago, 1995). · Judith Becker, <i>Traditional Music in Modern Java</i> (Hawaii, 1980). · R. Anderson Sutton, <i>Variation in Central Javanese Gamelan Music</i> (1993). · Benjamin Brinner, <i>Knowing Music, Making Music: Javanese Gamelan and the Theory of Musical Competence and Interaction</i> (Chicago, 1995). · Mantle Hood, <i>The Nuclear Theme as a Determinant of Patet in Javanese Music</i> (1954). · Michael Tenzer, <i>Gamelan Gong Kebyar: The Art of Twentieth-Century Balinese Music</i> (Chicago, 2000), the standard work on kebyar, kotekan and ombak; and his shorter <i>Balinese Gamelan Music</i> (Periplus). · Colin McPhee, <i>Music in Bali</i> (Yale, 1966). · Wikipedia articles "Gamelan", "Colotomy", "Irama", "Pathet", "Gamelan gong kebyar", "Kotekan". · Companion note in this project: <i>Gamelan Weights and the Seleh Quantizer</i>.</p>
<p class="small">These pages were written from general knowledge of the literature above rather than fresh research; details (especially the Balinese form layouts and the illustrative patterns) are simplified to match the modules.</p>
""")

CSS = f"""
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-Regular.otf); font-weight: 400; }}
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-Medium.otf); font-weight: 500; }}
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-SemiBold.otf); font-weight: 600; }}
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-Bold.otf); font-weight: 700; }}
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-ExtraBold.otf); font-weight: 800; }}
@font-face {{ font-family: Inter; src: url(file:///usr/share/fonts/opentype/inter/Inter-Italic.otf); font-style: italic; }}
@page {{ size: A4; margin: 0; }}
* {{ box-sizing: border-box; }}
body {{ margin: 0; font-family: Inter, sans-serif; color: {INK}; background: {PAPER}; font-size: 9.6pt; line-height: 1.45; }}
.page {{ width: 210mm; height: 297mm; padding: 15mm 17mm 14mm; position: relative; overflow: hidden; page-break-after: always; background: {PAPER}; }}
.page:first-child {{ border-top: 9mm solid {TEAK}; padding-top: 12mm; }}
.foot {{ position: absolute; bottom: 7mm; left: 17mm; right: 17mm; font-size: 8pt; color: {SOFT}; border-top: 0.6pt solid {LINE}; padding-top: 2mm; }}
h1 {{ font-size: 34pt; margin: 2mm 0 3mm; letter-spacing: -0.5pt; }}
h2 {{ font-size: 16pt; margin: 0 0 3mm; }}
h3 {{ font-size: 11pt; margin: 4mm 0 1.5mm; color: {TEAK}; }}
.num {{ display: inline-block; width: 8.5mm; height: 8.5mm; border-radius: 50%; background: {GOLD}; color: #fff; text-align: center; line-height: 8.5mm; font-size: 12pt; margin-right: 3mm; }}
.kicker {{ color: {GOLD}; font-weight: 700; text-transform: uppercase; letter-spacing: 1.5pt; font-size: 9pt; }}
.lead {{ font-size: 12.5pt; line-height: 1.4; color: {TEAK}; }}
p {{ margin: 0 0 2.4mm; }}
ul, ol {{ margin: 0 0 2.4mm; padding-left: 5mm; }} li {{ margin-bottom: 1.2mm; }}
.fig {{ margin: 3mm 0 3mm; padding: 3mm; background: #fff; border: 0.6pt solid {LINE}; border-radius: 2mm; }}
.note {{ background: #F3E8D2; border-left: 1.2mm solid {GOLD}; padding: 2.5mm 3.5mm; margin: 3mm 0; font-size: 9.2pt; }}
.small {{ font-size: 8.4pt; color: #5A4A3A; }}
.mono {{ font-family: monospace; font-size: 10pt; }}
table {{ border-collapse: collapse; width: 100%; margin: 2mm 0 3mm; font-size: 9pt; }}
th, td {{ border-bottom: 0.6pt solid {LINE}; padding: 1.4mm 2mm; text-align: left; vertical-align: top; }}
th {{ background: #EFE4CE; }}
.toc {{ margin: 6mm 0 7mm; border-top: 0.6pt solid {LINE}; }}
.toc div {{ padding: 2.2mm 0; border-bottom: 0.6pt solid {LINE}; font-size: 11pt; }}
.toc span {{ display: inline-block; width: 9mm; color: {GOLD}; font-weight: 800; }}
"""
html = f"<!doctype html><html><head><meta charset='utf-8'><title>Gamelan Sequencing</title><style>{CSS}</style></head><body>{''.join(P)}</body></html>"
open(os.path.join(HERE, "gamelan-sequencing.html"), "w").write(html)
print(len(P), "pages")
