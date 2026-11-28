#!/usr/bin/env python3
# 20 reggae / Jamaican / Rastafari panel themes for Dub Siren, all on layout D (26 HP).
# python3 gen.py  ->  theme-01.png .. theme-20.png, dub-siren-20-themes.png
import math, os, subprocess, random
HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 132.08, 128.5
RED, GOLD, GRN, BLK = "#D21F26", "#F7C400", "#119A45", "#111111"
JGRN, JGOLD = "#009B3A", "#FED100"

FONTS = "Inter:wght@700;900&family=Climate+Crisis&family=Bungee&family=Bungee+Shade&family=Rubik+Mono+One&family=Shrikhand&family=Monoton&family=Permanent+Marker&family=Black+Ops+One&family=Bowlby+One&family=Rye&family=Fredericka+the+Great&family=Bungee+Inline&family=Righteous"

def esc(s): return s.replace("&", "&amp;")

class T:
    """One theme: background + decorations + colours. Layout is fixed (option D)."""
    def __init__(s, n, name, blurb, **kw):
        s.n, s.name, s.blurb = n, name, blurb
        s.ink = kw.get("ink", BLK); s.soft = kw.get("soft", s.ink)
        s.plate = kw.get("plate")            # (fill, opacity) behind labels on busy backgrounds
        s.caps = kw.get("caps", [RED, GOLD, RED, GOLD, GOLD, GRN, GOLD])  # SPEED FDBK PITCH MODE BEAT VOL trims
        s.skirt = kw.get("skirt", "#121212"); s.ridge = kw.get("ridge", "#3a3a3a")
        s.strip = kw.get("strip", "#1A1A1A"); s.jink = kw.get("jink", "#F4EEDF")
        s.out = kw.get("out", RED); s.line = kw.get("line", s.ink)
        s.title = kw.get("title", ("Bungee", 7.0, s.ink)); s.title_fx = kw.get("title_fx")
        s.bg = kw.get("bg"); s.deco = kw.get("deco"); s.logo = kw.get("logo", s.ink)
        s.btn = kw.get("btn", (RED, "#222")); s.bezel = kw.get("bezel", "#b5b5b5")
        s.sub = kw.get("sub", "sound system siren")
        s.o = []

    # ---- primitives
    def t(s, x, y, txt, size=3.2, anchor="middle", fill=None, font="Inter", w=800, extra=""):
        s.o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-weight="{w}" font-size="{size}" text-anchor="{anchor}" fill="{fill or s.ink}" {extra}>{esc(txt)}</text>')
    def label(s, x, y, txt, size=3.2, fill=None, anchor="middle"):
        if s.plate:
            wd = len(txt) * size * 0.68 + 2.0
            x0 = x - wd / 2 if anchor == "middle" else (x - 1.0 if anchor == "start" else x - wd + 1.0)
            s.o.append(f'<rect x="{x0:.2f}" y="{y - size * 0.86:.2f}" width="{wd:.2f}" height="{size * 1.15:.2f}" rx="{size * 0.3:.2f}" fill="{s.plate[0]}" fill-opacity="{s.plate[1]}"/>')
        s.t(x, y, txt, size, anchor, fill)
    def knob(s, x, y, d, cap, lab, scale=None, ang=-30):
        r = d / 2
        s.o.append(f'<circle cx="{x}" cy="{y}" r="{r + 0.25}" fill="#000" fill-opacity=".25"/><circle cx="{x}" cy="{y}" r="{r}" fill="{s.skirt}"/>')
        n = 32 if d > 12 else 22
        for i in range(n):
            a = 2 * math.pi * i / n
            s.o.append(f'<line x1="{x + (r - .05) * math.cos(a):.2f}" y1="{y + (r - .05) * math.sin(a):.2f}" x2="{x + (r - 1) * math.cos(a):.2f}" y2="{y + (r - 1) * math.sin(a):.2f}" stroke="{s.ridge}" stroke-width=".35"/>')
        rc = r * .74
        s.o.append(f'<circle cx="{x}" cy="{y}" r="{rc:.2f}" fill="{cap}" stroke="#000" stroke-opacity=".35" stroke-width=".25"/><circle cx="{x - rc * .25:.2f}" cy="{y - rc * .3:.2f}" r="{rc * .5:.2f}" fill="#fff" fill-opacity=".14"/>')
        a = math.radians(ang)
        pc = "#111" if cap in (GOLD, JGOLD, "#FFFFFF", "#F4EEDF", "#FFE34D") else "#fff"
        s.o.append(f'<line x1="{x + rc * .25 * math.sin(a):.2f}" y1="{y - rc * .25 * math.cos(a):.2f}" x2="{x + rc * .92 * math.sin(a):.2f}" y2="{y - rc * .92 * math.cos(a):.2f}" stroke="{pc}" stroke-width="{1.0 if d > 12 else .8}" stroke-linecap="round"/>')
        s.label(x, y - r - 2.4, lab)
        if scale:
            R = r + 3.0; k = len(scale)
            for i, l in enumerate(scale):
                a = math.radians(-150 + 300 * i / (k - 1))
                sx, sy = x + R * math.sin(a), y - R * math.cos(a) + 1.0
                anc = "middle"
                if abs(math.sin(a)) > .4: anc = "start" if math.sin(a) > 0 else "end"
                s.label(sx + (-.8 if anc == "start" else .8 if anc == "end" else 0), sy, l, 2.7, s.soft, anc)
    def switch(s, x, y, lab, up="ON", down="OFF"):
        s.o.append(f'<rect x="{x - 1.9}" y="{y - 4}" width="3.8" height="8" rx=".5" fill="#1a1a1a"/><rect x="{x - 1.5}" y="{y - 3.6}" width="3" height="3.6" rx=".3" fill="#bdbdbd"/>')
        s.label(x, y - 6.6, lab)
        s.label(x + 3, y - 1.2, up, 2.7, s.soft, "start"); s.label(x + 3, y + 3.7, down, 2.7, s.soft, "start")
    def button(s, x, y, lab, col):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="4.6" fill="{s.bezel}" stroke="#555" stroke-width=".4"/><circle cx="{x}" cy="{y}" r="3.3" fill="{col}"/>')
        s.label(x, y - 6.4, lab)
    def jack(s, x, y, lab, col=None):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="3.4" fill="#d8d8d8" stroke="#777" stroke-width=".4"/><circle cx="{x}" cy="{y}" r="2.0" fill="#111"/>')
        s.t(x, y - 5.0, lab, 2.9, fill=col)

    def render(s):
        s.o = []
        if s.bg: s.bg(s)
        # title
        f, sz, col = s.title
        if s.title_fx: s.title_fx(s)
        else:
            s.t(13.5, 12.2, "DUB SIREN", sz, "start", col, f, 400 if f != "Inter" else 900)
        if s.sub: s.label(W - 13.5, 11.4, s.sub, 2.7, s.soft, "end")
        if s.deco: s.deco(s)
        c = s.caps
        XS = [22.0, 66.0, 110.0]
        s.knob(XS[0], 30, 19, c[0], "SPEED", ang=0); s.knob(XS[1], 30, 16, c[1], "FEEDBACK", ang=-40)
        s.knob(XS[2], 30, 19, c[2], "PITCH", ["1", "2", "3", "4"], ang=-50)
        s.knob(XS[0], 59, 16, c[3], "MODE", ["1", "2", "3", "4"], ang=150)
        s.knob(XS[1], 59, 16, c[4], "BEAT", ["1", "2", "3", "OFF"], ang=-150)
        s.knob(XS[2], 59, 16, c[5], "SIREN VOL", ang=-60)
        s.switch(16, 83, "TRIG"); s.button(40, 83, "SIREN", s.btn[0]); s.button(60, 83, "TONE", s.btn[1])
        s.o.append(f'<circle cx="80" cy="83" r="2.3" fill="#FF2A1F" stroke="#5a1010" stroke-width=".35"/><circle cx="80" cy="83" r="4" fill="#FF2A1F" fill-opacity=".18"/>')
        s.label(80, 79, "ACTIVE", 2.9, s.soft)
        s.o.append(f'<rect x="4" y="90.3" width="{W - 8}" height="0.45" fill="{s.line}"/>')
        s.switch(11, 101, "EXTRAS")
        for x, l in ((33, "DEPTH"), (50, "RELEASE"), (67, "TIME")): s.knob(x, 101.5, 8.5, c[6], l, ang=10)
        s.switch(79, 101, "ERA", "80s", "90s")
        for x, l in ((101, "IN"), (117, "CLOCK")):
            s.jack(x, 101.5, "", None); s.label(x, 96.5, l, 2.9)
        s.o.append(f'<rect x="2.5" y="110.5" width="{W - 5}" height="13.5" rx="1.4" fill="{s.strip}"/>')
        st = (W - 16) / 7
        for i, l in enumerate(["V/OCT", "SPEED", "FDBK", "TRIG", "SIREN", "TONE"]): s.jack(8 + i * st, 119, l, s.jink)
        s.o.append(f'<rect x="{W - 13.6}" y="111.3" width="11.2" height="11.9" rx="1.2" fill="{s.out}"/>')
        s.jack(W - 8, 119, "OUT", "#FFFFFF")
        for x, y in ((7.5, 3), (W - 7.5, 3), (7.5, H - 3), (W - 7.5, H - 3)):
            s.o.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#a9aba8" stroke="#555" stroke-width=".3"/><path d="M{x - 1.4} {y}h2.8M{x} {y - 1.4}v2.8" stroke="#444" stroke-width=".5"/>')
        s.t(W / 2, 127.3, "Hær", 3.3, fill=s.logo, font="Climate Crisis", w=400)
        return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W * 7:.0f}" height="{H * 7:.0f}"><defs>{DEFS}</defs><clipPath id="clip"><rect width="{W}" height="{H}"/></clipPath><g clip-path="url(#clip)">' + "".join(s.o) + '</g></svg>'

DEFS = f'''
<linearGradient id="sunset" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2B0B3F"/><stop offset=".45" stop-color="#E2412E"/><stop offset=".75" stop-color="#F7A21B"/><stop offset="1" stop-color="#FFD56B"/></linearGradient>
<radialGradient id="glowR"><stop offset="0" stop-color="{GOLD}" stop-opacity=".9"/><stop offset="1" stop-color="{GOLD}" stop-opacity="0"/></radialGradient>
<linearGradient id="desk" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3E5B4A"/><stop offset="1" stop-color="#223328"/></linearGradient>
<pattern id="brick" width="16" height="8" patternUnits="userSpaceOnUse"><rect width="16" height="8" fill="#7A3B2E"/><path d="M0 0H16M0 4H16M4 0V4M12 4V8" stroke="#4A2119" stroke-width=".6"/></pattern>
<pattern id="check" width="6" height="6" patternUnits="userSpaceOnUse"><rect width="6" height="6" fill="#111"/><rect width="3" height="3" fill="#F4EEDF"/><rect x="3" y="3" width="3" height="3" fill="#F4EEDF"/></pattern>
<pattern id="hazard" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="8" height="8" fill="{GOLD}"/><rect width="4" height="8" fill="#111"/></pattern>
<pattern id="knit" width="3" height="2.4" patternUnits="userSpaceOnUse"><path d="M0 0 L1.5 2.4 L3 0" fill="none" stroke="#000" stroke-opacity=".18" stroke-width=".35"/></pattern>
<pattern id="paper" width="5" height="5" patternUnits="userSpaceOnUse"><rect width="5" height="5" fill="#EDE6D3"/><circle cx="1" cy="2" r=".25" fill="#000" fill-opacity=".06"/><circle cx="3.5" cy="4" r=".2" fill="#000" fill-opacity=".05"/></pattern>
<pattern id="dots" width="4" height="4" patternUnits="userSpaceOnUse"><circle cx="2" cy="2" r=".7" fill="#000" fill-opacity=".2"/></pattern>
<filter id="neon" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation=".9" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
'''

# ---------------------------------------------------------------- background helpers
def rect(s, x, y, w, h, fill, extra=""): s.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" {extra}/>')
def hbands(s, cols, y0=0, y1=H):
    h = (y1 - y0) / len(cols)
    for i, c in enumerate(cols): rect(s, 0, y0 + i * h, W, h + .05, c)
def rays(s, cx, cy, cols, n=36, R=260, op=1):
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        s.o.append(f'<path d="M{cx} {cy}L{cx + R * math.cos(a0):.1f} {cy + R * math.sin(a0):.1f}L{cx + R * math.cos(a1):.1f} {cy + R * math.sin(a1):.1f}Z" fill="{cols[i % len(cols)]}" fill-opacity="{op}"/>')
def star(s, cx, cy, r, fill, pts=5, rot=-90, inner=.45, extra=""):
    p = []
    for i in range(pts * 2):
        rr = r if i % 2 == 0 else r * inner
        a = math.radians(rot + 180 * i / pts)
        p.append(f"{cx + rr * math.cos(a):.2f},{cy + rr * math.sin(a):.2f}")
    s.o.append(f'<polygon points="{" ".join(p)}" fill="{fill}" {extra}/>')
def speaker(s, cx, cy, r, cone="#2a2a2a", rim="#555"):
    s.o.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{rim}"/><circle cx="{cx}" cy="{cy}" r="{r * .9}" fill="#0d0d0d"/>')
    for k in range(4): s.o.append(f'<circle cx="{cx}" cy="{cy}" r="{r * (.82 - k * .16):.2f}" fill="none" stroke="{cone}" stroke-width="{r * .05:.2f}"/>')
    s.o.append(f'<circle cx="{cx}" cy="{cy}" r="{r * .22:.2f}" fill="#222" stroke="#444" stroke-width=".4"/>')
def lion(s, cx, cy, r, mane, face, ink):
    # stylised crowned lion head: spiky mane, round face, simple features
    pts = []
    for i in range(28):
        rr = r if i % 2 == 0 else r * .78
        a = 2 * math.pi * i / 28
        pts.append(f"{cx + rr * math.cos(a):.2f},{cy + rr * math.sin(a):.2f}")
    s.o.append(f'<polygon points="{" ".join(pts)}" fill="{mane}"/>')
    s.o.append(f'<ellipse cx="{cx}" cy="{cy + r * .05}" rx="{r * .55}" ry="{r * .6}" fill="{face}"/>')
    s.o.append(f'<path d="M{cx - r * .2} {cy - r * .1}h{r * .1}M{cx + r * .1} {cy - r * .1}h{r * .1}" stroke="{ink}" stroke-width="{r * .07}" stroke-linecap="round"/>')
    s.o.append(f'<path d="M{cx - r * .12} {cy + r * .15}L{cx} {cy + r * .28}L{cx + r * .12} {cy + r * .15}Z" fill="{ink}"/>')
    s.o.append(f'<path d="M{cx - r * .18} {cy + r * .4}Q{cx} {cy + r * .52} {cx + r * .18} {cy + r * .4}" fill="none" stroke="{ink}" stroke-width="{r * .05}"/>')
    # crown
    cyc = cy - r * 1.05
    s.o.append(f'<path d="M{cx - r * .45} {cyc + r * .3}L{cx - r * .45} {cyc - r * .05}L{cx - r * .22} {cyc + r * .15}L{cx} {cyc - r * .2}L{cx + r * .22} {cyc + r * .15}L{cx + r * .45} {cyc - r * .05}L{cx + r * .45} {cyc + r * .3}Z" fill="{GOLD}" stroke="{ink}" stroke-width=".3"/>')
def palm(s, x, y, sc, col, flip=1):
    s.o.append(f'<g transform="translate({x} {y}) scale({sc * flip} {sc})" fill="{col}">'
               '<path d="M0 0 C2 -20 4 -40 8 -58 L10 -58 C7 -40 5 -20 3 0Z"/>'
               '<path d="M9 -58 C0 -66 -14 -66 -24 -58 C-14 -62 -2 -60 9 -56Z"/>'
               '<path d="M9 -58 C2 -72 -8 -78 -18 -78 C-8 -74 0 -68 9 -57Z"/>'
               '<path d="M9 -58 C14 -72 24 -78 34 -76 C24 -72 16 -66 10 -57Z"/>'
               '<path d="M9 -58 C20 -64 32 -62 40 -54 C30 -58 20 -58 10 -56Z"/>'
               '<path d="M9 -58 C6 -70 8 -82 14 -88 C10 -78 10 -68 10 -58Z"/></g>')
def drips(s, y, cols, seed=3):
    rnd = random.Random(seed)
    x = 0
    i = 0
    while x < W:
        w = rnd.uniform(4, 9); c = cols[i % len(cols)]
        rect(s, x, y, w, 3, c)
        for k in range(rnd.randint(1, 2)):
            dx = x + rnd.uniform(.5, w - 1.2); L = rnd.uniform(3, 14)
            s.o.append(f'<path d="M{dx:.1f} {y + 2}v{L:.1f}a.7 .7 0 0 0 1.4 0v-{L:.1f}z" fill="{c}"/>')
        x += w; i += 1
def waves(s, y0, cols, amp=2.5, period=18, th=3.2):
    for i, c in enumerate(cols):
        yy = y0 + i * th
        d = f"M-5 {yy}"
        for x in range(-5, int(W) + 20, 2):
            d += f" L{x} {yy + amp * math.sin(2 * math.pi * x / period + i * .8):.2f}"
        d += f" L{W + 20} {yy + th} L-5 {yy + th}Z"
        d2 = f"M-5 {yy}"
        pts = [(x, yy + amp * math.sin(2 * math.pi * x / period + i * .8)) for x in range(-5, int(W) + 20, 2)]
        top = " ".join(f"L{x} {y:.2f}" for x, y in pts)
        bot = " ".join(f"L{x} {y + th:.2f}" for x, y in reversed(pts))
        s.o.append(f'<path d="M-5 {pts[0][1]:.2f} {top} {bot} Z" fill="{c}"/>')

themes = []
def add(*a, **k): themes.append(T(len(themes) + 1, *a, **k))

# 1 Rasta stripes
def bg1(s): hbands(s, [RED, GOLD, GRN], 0, H)
add("Rasta stripes", "The classic: red, gold and green bands, black type on cream plates so it stays readable.",
    bg=bg1, plate=("#F4EEDF", .92), title=("Bungee", 7.2, BLK), strip="#111", line=BLK,
    caps=[BLK, BLK, BLK, BLK, BLK, BLK, BLK], skirt="#222", ridge="#555")
# 2 Jamaica flag
def bg2(s):
    rect(s, 0, 0, W, H, JGRN)
    s.o.append(f'<path d="M0 0L{W / 2} {H / 2}L0 {H}Z" fill="{BLK}"/><path d="M{W} 0L{W / 2} {H / 2}L{W} {H}Z" fill="{BLK}"/>')
    s.o.append(f'<path d="M0 0L{W} {H}M{W} 0L0 {H}" stroke="{JGOLD}" stroke-width="15"/>')
add("Jamaican flag", "Black, green and gold saltire across the whole panel, labels on dark plates.",
    bg=bg2, plate=("#111", .85), ink="#FFFFFF", soft=JGOLD, title=("Rubik Mono One", 6.4, "#FFFFFF"),
    caps=[JGOLD, JGOLD, JGOLD, "#FFFFFF", "#FFFFFF", JGOLD, JGOLD], strip="#000", line=JGOLD, logo="#FFFFFF", out=JGRN)
# 3 Speaker stack
def bg3(s):
    rect(s, 0, 0, W, H, "#1b1b1b"); rect(s, 0, 0, W, H, "url(#dots)")
    for cx, cy, r in ((22, 30, 16), (66, 30, 13), (110, 30, 16), (22, 59, 13), (66, 59, 13), (110, 59, 13)): speaker(s, cx, cy, r)
    for x in (3, W - 3):
        for y in range(16, 90, 6): s.o.append(f'<circle cx="{x}" cy="{y}" r=".7" fill="#777"/>')
add("Speaker stack", "The panel is a sound-system box: every big knob sits in the middle of its own speaker cone.",
    bg=bg3, ink="#F4EEDF", soft=GOLD, title=("Black Ops One", 7.4, GOLD), plate=("#000", .6),
    caps=[RED, GOLD, RED, GRN, GOLD, GRN, GOLD], strip="#000", line=GOLD, logo="#F4EEDF")
# 4 Dance poster
def bg4(s):
    rect(s, 0, 0, W, H, "#FFE34D")
    rect(s, 0, 0, W, 17, "#FF2E88"); rect(s, 0, 91, W, 2.5, "#00B04F")
    s.t(W / 2, 47, "INNA DI DANCE", 15, fill="#FF2E88", font="Bowlby One", w=400, extra='fill-opacity=".18"')
    s.t(W / 2, 75, "TONIGHT!", 18, fill="#00B04F", font="Bowlby One", w=400, extra='fill-opacity=".18"')
def tfx4(s): s.t(W / 2, 12.6, "DUB SIREN", 9.5, fill="#FFE34D", font="Bowlby One", w=400)
add("Dance poster", "Hand-painted Jamaican sound-clash poster: fluorescent pink and yellow, giant faded slogans behind the knobs.",
    bg=bg4, title_fx=tfx4, sub=None, ink="#111", soft="#C4005A",
    caps=["#FF2E88", "#00B04F", "#FF2E88", "#00B04F", "#00B04F", "#111", "#FF2E88"], strip="#111", out="#FF2E88", line="#111")
# 5 Mixing desk
def bg5(s):
    rect(s, 0, 0, W, H, "url(#desk)")
    for x in range(8, int(W), 11): rect(s, x, 14, .3, 76, "#000", 'fill-opacity=".25"')
    s.o.append(f'<rect x="84" y="73.5" width="40" height="14" rx="1.5" fill="#F2E7C4" stroke="#111" stroke-width=".5"/>')
    s.o.append('<path d="M88 85 A16 16 0 0 1 120 85" fill="none" stroke="#111" stroke-width=".35"/><path d="M112 80 A16 16 0 0 1 120 85" fill="none" stroke="#C8282A" stroke-width="1"/><line x1="104" y1="86.5" x2="98" y2="76" stroke="#111" stroke-width=".5"/>')
    s.t(104, 86.6, "VU", 2.2, fill="#111")
add("Mixing desk", "Dub studio console: dark green desk paint, fader slots, cream VU meter by the buttons. Old-school engineer feel.",
    bg=bg5, ink="#F2E7C4", soft="#E9C46A", title=("Righteous", 7.2, "#F2E7C4"),
    caps=["#E9E4D4", "#E9E4D4", "#E9E4D4", RED, GOLD, GRN, "#E9E4D4"], skirt="#1d1d1d", strip="#111", line="#F2E7C4", logo="#F2E7C4")
# 6 Sunburst
def bg6(s):
    rays(s, 66, 59, [RED, GOLD, GRN], 30)
    s.o.append('<circle cx="66" cy="59" r="22" fill="#111" fill-opacity=".85"/>')
add("Sunburst", "Red, gold and green rays exploding out from behind BEAT. Loud but the layout stays calm.",
    bg=bg6, plate=("#111", .82), ink="#FFFFFF", soft=GOLD, title=("Shrikhand", 7.4, "#FFFFFF"),
    caps=[BLK, BLK, BLK, BLK, GOLD, BLK, BLK], skirt="#000", ridge="#444", strip="#111", line="#111", logo="#111")
# 7 Lion of Judah
def bg7(s):
    rect(s, 0, 0, W, H, "#0E5A2B")
    rect(s, 0, 0, W, 3.5, RED); rect(s, 0, 3.5, W, 2, GOLD); rect(s, 0, H - 5.5, W, 2, GOLD); rect(s, 0, H - 3.5, W, 3.5, RED)
    lion(s, 66, 32, 12, GOLD, "#E8B21F", "#4A2A00")
add("Lion of Judah", "Deep green with a gold crowned lion behind FEEDBACK, red and gold rails top and bottom.",
    bg=bg7, ink="#F7E7B0", soft=GOLD, title=("Rye", 6.6, GOLD), plate=("#0E5A2B", .8),
    caps=[RED, GOLD, RED, GOLD, GOLD, RED, GOLD], strip="#062D15", line=GOLD, logo=GOLD)
# 8 Tibeb weave
def bg8(s):
    rect(s, 0, 0, W, H, "#F6F0E2")
    for y in (0, 88):
        for i, c in enumerate([RED, GOLD, GRN, GOLD, RED]): rect(s, 0, y + i * 1.1, W, 1.1, c)
        for x in range(0, int(W) + 6, 6):
            star(s, x + 3, y + 2.75, 1.6, "#111", 4, -90, .35)
    for x in (0, W - 4):
        for i, c in enumerate([RED, GOLD, GRN]): rect(s, x + i * 1.33, 20, 1.33, 68, c)
add("Woven border", "Ethiopian tibeb-style woven borders in Rasta colours on undyed cotton, a nod to Rastafari's Ethiopian roots.",
    bg=bg8, ink="#2A1A10", soft="#8B2A1A", title=("Fredericka the Great", 7.6, "#2A1A10"),
    caps=[RED, GOLD, RED, GRN, GRN, GRN, GOLD], strip="#2A1A10", line="#2A1A10")
# 9 Dub plate
def bg9(s):
    rect(s, 0, 0, W, H, "#0A0A0A")
    for r in range(8, 150, 1): s.o.append(f'<circle cx="66" cy="59" r="{r * .9:.1f}" fill="none" stroke="#fff" stroke-opacity="{.04 + .03 * (r % 3)}" stroke-width=".18"/>')
    s.o.append(f'<circle cx="66" cy="59" r="17" fill="{RED}"/><path d="M49 59a17 17 0 0 0 34 0z" fill="{GRN}"/><rect x="49" y="55.5" width="34" height="7" fill="{GOLD}"/><circle cx="66" cy="59" r="1.3" fill="#000"/>')
    s.t(66, 46, "ONE-OFF ACETATE", 2.3, fill="#888", w=700)
add("Dub plate", "A spinning acetate: black grooves everywhere, the BEAT knob sits on a Rasta-coloured record label.",
    bg=bg9, ink="#EEE", soft=GOLD, title=("Monoton", 7.4, "#EEE"), plate=("#000", .55),
    caps=["#EEE", "#EEE", "#EEE", "#EEE", BLK, "#EEE", "#EEE"], skirt="#000", ridge="#444", strip="#000", line="#444", logo="#EEE")
# 10 Tropical sunset
def bg10(s):
    rect(s, 0, 0, W, H, "url(#sunset)")
    s.o.append('<circle cx="66" cy="78" r="26" fill="#FFD56B" fill-opacity=".55"/>')
    palm(s, 2, 92, .95, "#1A0B1E"); palm(s, W - 2, 92, 1.0, "#1A0B1E", -1)
    rect(s, 0, 90, W, 38, "#1A0B1E")
add("Tropical sunset", "Purple to orange Caribbean dusk with palm silhouettes; the extras sit on the dark ground.",
    bg=bg10, ink="#FFF3D6", soft="#FFD56B", title=("Shrikhand", 7.4, "#FFF3D6"), plate=("#1A0B1E", .55),
    caps=[GOLD, GRN, GOLD, GRN, RED, GRN, GOLD], strip="#0B0410", line="#FFD56B", logo="#FFD56B")
# 11 Black Ark chaos
def bg11(s):
    rect(s, 0, 0, W, H, "#1A1410")
    rnd = random.Random(11)
    for i in range(70):
        c = rnd.choice([RED, GOLD, GRN, "#2C7BE5", "#FF2E88", "#F4EEDF"])
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        k = rnd.random()
        if k < .3: star(s, x, y, rnd.uniform(1.5, 5), c, 5, rnd.uniform(0, 90), .45, f'fill-opacity="{rnd.uniform(.3, .8):.2f}"')
        elif k < .6: s.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rnd.uniform(.5, 3):.1f}" fill="{c}" fill-opacity="{rnd.uniform(.3, .9):.2f}"/>')
        else:
            d = f"M{x:.1f} {y:.1f}" + "".join(f" q{rnd.uniform(-8, 8):.1f} {rnd.uniform(-8, 8):.1f} {rnd.uniform(-10, 10):.1f} {rnd.uniform(-10, 10):.1f}" for _ in range(3))
            s.o.append(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="{rnd.uniform(.4, 1.4):.1f}" stroke-opacity=".7" stroke-linecap="round"/>')
    for txt, x, y, r in (("DUB", 40, 75, -12), ("ECHO", 95, 45, 8), ("BASS", 20, 47, 6)):
        s.t(x, y, txt, 7, fill="#F4EEDF", font="Permanent Marker", w=400, extra=f'fill-opacity=".22" transform="rotate({r} {x} {y})"')
def tfx11(s):
    for i, c in enumerate([GRN, GOLD, RED]): s.t(13.5 + i * .5, 12.6 - i * .5, "DUB SIREN", 8.4, "start", c, "Permanent Marker", 400)
add("Black Ark chaos", "Studio-wall graffiti madness: scribbles, stars and scrawled words in every colour. The wildest of the hand-drawn ones.",
    bg=bg11, title_fx=tfx11, ink="#F4EEDF", soft=GOLD, plate=("#1A1410", .85),
    caps=[RED, "#2C7BE5", GOLD, "#FF2E88", GRN, GRN, "#F4EEDF"], strip="#000", line=GOLD, logo="#F4EEDF")
# 12 Crochet tam
def bg12(s):
    cols = [RED, GOLD, GRN, BLK] * 6
    waves(s, -3, cols, amp=1.8, period=12, th=5.6)
    rect(s, 0, 0, W, H, "url(#knit)")
def tfx12(s):
    s.o.append(f'<rect x="11" y="4.6" width="58" height="9.6" rx="1.5" fill="#F4EEDF"/>')
    s.t(13.5, 12.4, "DUB SIREN", 7.4, "start", BLK, "Bungee Shade", 400)
add("Crochet tam", "Knitted-tam stripes with a wavy stitch texture all the way down. Cosy and loud at once.",
    bg=bg12, title_fx=tfx12, plate=("#F4EEDF", .92), ink=BLK, title=("Bungee Shade", 7.4, BLK),
    caps=[BLK, BLK, BLK, BLK, BLK, BLK, BLK], skirt="#000", ridge="#555", strip="#111", line=BLK)
# 13 Route taxi
def bg13(s):
    rect(s, 0, 0, W, H, "#F4EEDF")
    rect(s, 0, 0, W, 4, "url(#check)"); rect(s, 0, 86.5, W, 4, "url(#check)")
    s.o.append(f'<path d="M0 14.2 Q{W / 2} 11.5 {W} 14.2 L{W} 15.6 Q{W / 2} 12.9 0 15.6Z" fill="{GRN}"/>')
    s.t(W / 2, 43.0, "JAH GUIDE", 4.2, fill=GOLD, font="Rye", w=400, extra='stroke="#111" stroke-width=".25"')
def tfx13(s): s.t(W / 2, 12.4, "Dub Siren", 9, fill=RED, font="Shrikhand", w=400, extra='stroke="#111" stroke-width=".35"')
add("Route taxi", "Hand-painted Jamaican route-taxi signwriting: chequer trim, swoosh stripes, a little 'Jah Guide' blessing in the middle.",
    bg=bg13, title_fx=tfx13, sub=None, ink="#111", soft=RED,
    caps=[RED, GOLD, RED, GRN, GOLD, GRN, GOLD], strip="#111", line="#111")
# 14 Rubber-stamp zine
def bg14(s):
    rect(s, 0, 0, W, H, "url(#paper)")
    for txt, x, y, r, c in (("DUB", 92, 50, -14, RED), ("WICKED!", 30, 76, 9, GRN), ("SOUND", 76, 22, 4, GOLD)):
        s.o.append(f'<g transform="rotate({r} {x} {y})" opacity=".28"><rect x="{x - len(txt) * 3.2}" y="{y - 7}" width="{len(txt) * 6.4}" height="9" fill="none" stroke="{c}" stroke-width="1"/>')
        s.t(x, y, txt, 7.2, fill=c, font="Black Ops One", w=400); s.o.append('</g>')
add("Rubber-stamp zine", "Photocopied sound-system flyer: off-white paper, faded red, green and gold rubber stamps, stencil title.",
    bg=bg14, ink="#1A1A1A", soft="#8B2A1A", title=("Black Ops One", 7.6, "#1A1A1A"),
    caps=[RED, GRN, RED, "#1A1A1A", "#1A1A1A", GRN, GOLD], strip="#1A1A1A", line="#1A1A1A")
# 15 Op-art bass
def bg15(s):
    for i in range(46, 0, -1):
        s.o.append(f'<circle cx="66" cy="59" r="{i * 3.2}" fill="{[RED, GOLD, GRN, BLK][i % 4]}"/>')
add("Op-art bass", "Concentric red, gold, green and black rings: the panel as a pumping speaker cone. Dizzying on purpose.",
    bg=bg15, plate=("#F4EEDF", .95), ink=BLK, title=("Bungee", 7.2, "#F4EEDF"),
    caps=["#F4EEDF"] * 7, skirt="#000", ridge="#555", strip="#000", line="#F4EEDF", logo="#F4EEDF")
# 16 Neon night
def bg16(s):
    rect(s, 0, 0, W, H, "#07070C")
    for (x, y, w, h, c) in ((4, 15.5, W - 8, 73, RED), (6, 17.5, W - 12, 69, GOLD), (8, 19.5, W - 16, 65, GRN)):
        s.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="none" stroke="{c}" stroke-width=".55" filter="url(#neon)"/>')
def tfx16(s):
    s.t(13.5, 12.4, "DUB SIREN", 7.6, "start", "#FF4D4D", "Monoton", 400, 'filter="url(#neon)"')
add("Neon night", "Night-time dance: black panel, three glowing neon tubes in red, gold and green, Monoton title lit up.",
    bg=bg16, title_fx=tfx16, ink="#F4EEDF", soft="#FFD84D",
    caps=["#FF4D4D", "#FFD84D", "#FF4D4D", "#3DFF7A", "#FFD84D", "#3DFF7A", "#FFD84D"], skirt="#000", ridge="#333", strip="#000", line="#3DFF7A", logo="#F4EEDF", out="#FF4D4D")
# 17 Graffiti wall
def bg17(s):
    rect(s, 0, 0, W, H, "url(#brick)")
    drips(s, 14, [RED, GOLD, GRN])
    s.t(98, 82, "ONE LOVE", 6, fill="#F4EEDF", font="Permanent Marker", w=400, extra='fill-opacity=".35" transform="rotate(-6 98 82)"')
add("Graffiti wall", "Brick wall with a band of dripping Rasta spray paint under the title, 'One Love' tagged faintly beside the LED.",
    bg=bg17, ink="#F4EEDF", soft=GOLD, plate=("#2A100B", .78), title=("Permanent Marker", 8.0, "#F4EEDF"),
    caps=[RED, GOLD, RED, GRN, GOLD, GRN, GOLD], strip="#1A0A06", line=GOLD, logo="#F4EEDF")
# 18 Minimal
def bg18(s):
    rect(s, 0, 0, W, H, "#141414")
    for i, c in enumerate([RED, GOLD, GRN]): rect(s, 0, 15.2 + i * .7, W, .7, c)
add("Minimal", "The restrained one: matte black, one thin red-gold-green pinstripe under the title, black knobs with coloured pointers.",
    bg=bg18, ink="#EDEDED", soft="#9A9A9A", title=("Inter", 6.6, "#EDEDED"),
    caps=["#2A2A2A"] * 7, skirt="#0A0A0A", ridge="#2c2c2c", strip="#0A0A0A", line="#333", logo="#EDEDED")
# 19 Siren hazard
def bg19(s):
    rect(s, 0, 0, W, H, "#F4EEDF")
    rect(s, 0, 0, W, 4, "url(#hazard)"); rect(s, 0, 86, W, 4.5, "url(#hazard)")
    for i, c in enumerate([RED, GOLD, GRN]):
        s.o.append(f'<path d="M{W - 34 + i * 9} 14.5 l9 0 l-6 9 l-9 0z" fill="{c}"/>')
    s.o.append(f'<circle cx="80" cy="83" r="9" fill="url(#glowR)"/>')
add("Siren hazard", "Emergency-vehicle look: hazard chevrons, a beacon glow round the ACTIVE LED, Rasta flashes by the title.",
    bg=bg19, ink="#111", soft=RED, title=("Black Ops One", 7.6, "#111"),
    caps=[RED, GOLD, RED, "#111", "#111", GRN, GOLD], strip="#111", line="#111")
# 20 Everything
def bg20(s):
    rays(s, 66, 30, [RED, GOLD, GRN, BLK], 40)
    s.o.append(f'<path d="M0 0L{W / 2} {H / 2}L0 {H}Z" fill="{BLK}" fill-opacity=".35"/><path d="M{W} 0L{W / 2} {H / 2}L{W} {H}Z" fill="{BLK}" fill-opacity=".35"/>')
    rect(s, 0, 0, W, 4, "url(#check)")
    lion(s, 66, 59, 13, GOLD, "#E8B21F", "#4A2A00")
    rnd = random.Random(20)
    for i in range(26): star(s, rnd.uniform(0, W), rnd.uniform(16, 88), rnd.uniform(1.2, 3), rnd.choice([GOLD, "#FFFFFF", RED]), 5, rnd.uniform(0, 70))
    waves(s, 90.3, [RED, GOLD, GRN, BLK, RED, GOLD, GRN], amp=1.2, period=10, th=3.0)
def tfx20(s):
    s.o.append(f'<rect x="10" y="4.5" width="{W - 20}" height="10.5" rx="2" fill="#111" stroke="{GOLD}" stroke-width=".6"/>')
    s.t(W / 2, 12.8, "DUB SIREN", 8.6, fill=GOLD, font="Bungee Inline", w=400)
add("Everything", "Maximum: sunburst, flag saltire, chequer trim, crowned lion behind BEAT, stars, wavy stripes under the extras. No restraint at all.",
    bg=bg20, title_fx=tfx20, sub=None, plate=("#111", .85), ink="#FFFFFF", soft=GOLD,
    caps=[RED, GRN, RED, GRN, GOLD, GRN, GOLD], strip="#111", line=GOLD, logo="#FFFFFF")

# ---------------------------------------------------------------- render
head = f'<link href="https://fonts.googleapis.com/css2?family={FONTS}&display=swap" rel="stylesheet">'
cards = []
for th in themes:
    svg = th.render()
    open(os.path.join(HERE, f"_t{th.n:02d}.html"), "w").write(f'<html><head>{head}</head><body style="margin:0;background:#222">{svg}</body></html>')
    small = svg.replace(f'width="{W * 7:.0f}" height="{H * 7:.0f}"', f'width="{W * 3.2:.0f}" height="{H * 3.2:.0f}"')
    cards.append(f'<div class="c"><div class="h">{th.n}. {esc(th.name)}</div>{small}<div class="p">{esc(th.blurb)}</div></div>')
sheet = (f'<html><head>{head}<style>body{{margin:0;padding:14px;background:#1d1f22;font-family:Inter;color:#eee}}'
         f'.g{{display:grid;grid-template-columns:repeat(5,{W * 3.2:.0f}px);gap:18px 16px}} .h{{font-weight:900;font-size:17px;margin:0 0 6px}} .p{{font-size:12.5px;line-height:1.3;color:#bbb;margin-top:5px}}'
         f'h1{{font-size:26px;margin:4px 0 14px}}</style></head><body><h1>Dub Siren: 20 panel themes (layout D, 26 HP)</h1><div class="g">' + "".join(cards) + '</div></body></html>')
open(os.path.join(HERE, "_sheet.html"), "w").write(sheet)
js = f"""const {{chromium}}=require('playwright');(async()=>{{const b=await chromium.launch();
for(let i=1;i<=20;i++){{const k=String(i).padStart(2,'0');const p=await b.newPage({{viewport:{{width:{W * 7:.0f},height:{H * 7:.0f}}}}});await p.goto('file://{HERE}/_t'+k+'.html');await p.waitForTimeout(1200);await p.screenshot({{path:'{HERE}/theme-'+k+'.png'}});await p.close();}}
const p=await b.newPage({{viewport:{{width:{int(W * 3.2 * 5 + 16 * 4 + 28)},height:1000}}}});await p.goto('file://{HERE}/_sheet.html');await p.waitForTimeout(1500);await p.screenshot({{path:'{HERE}/dub-siren-20-themes.png',fullPage:true}});await b.close();}})();"""
subprocess.run(["node", "-e", js], check=True)
for f in os.listdir(HERE):
    if f.startswith("_") and f.endswith(".html"): os.remove(os.path.join(HERE, f))
print("ok")
