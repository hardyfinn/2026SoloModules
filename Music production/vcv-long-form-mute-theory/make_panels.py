"""Panel mock-ups for Long Form and Mute Theory (house style, 3.2 mm control / 2.9 mm jack labels)."""
import math, sys

HP = 5.08
H = 128.5
BG = "#ece5d8"; INK = "#26262a"; QUIET = "#6b6660"; ACC = "#cf5a2e"
PLATE = "#2b2c31"; DISP = "#1d1e22"; ORANGE = "#ef7a37"; CYAN = "#3fd0e0"
CTL = 3.2; JCK = 2.9; FONT = "Inter, Helvetica, Arial, sans-serif"


class Panel:
    def __init__(self, hp, title, tagline, footer):
        self.w = hp * HP; self.el = []; self.top = []
        self.title, self.tagline, self.footer = title, tagline, footer

    def add(self, s): self.el.append(s)

    def text(self, x, y, s, size=CTL, fill=INK, anchor="middle", weight=600, ls=0.15):
        self.top.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{FONT}" font-size="{size}" '
                        f'font-weight="{weight}" fill="{fill}" text-anchor="{anchor}" letter-spacing="{ls}">{s}</text>')

    def knob(self, x, y, r=4.6, label=None, angle=-40, lab_fill=INK):
        self.add(f'<circle cx="{x}" cy="{y}" r="{r+0.5}" fill="#151515" opacity="0.18"/>')
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#1b1b1d"/>')
        self.add(f'<circle cx="{x}" cy="{y}" r="{r*0.78}" fill="#2a2a2d"/>')
        a = math.radians(angle - 90)
        self.add(f'<line x1="{x+math.cos(a)*r*0.25:.2f}" y1="{y+math.sin(a)*r*0.25:.2f}" '
                 f'x2="{x+math.cos(a)*r*0.92:.2f}" y2="{y+math.sin(a)*r*0.92:.2f}" stroke="#f2f2f2" stroke-width="{max(0.6,r*0.16):.2f}" stroke-linecap="round"/>')
        if label: self.text(x, y - r - 1.6, label, CTL, lab_fill)

    def jack(self, x, y, label=None, on_plate=False, lab_fill=None):
        self.add(f'<circle cx="{x}" cy="{y}" r="4.1" fill="#c9c9c9" stroke="#8d8d8d" stroke-width="0.3"/>')
        self.add(f'<circle cx="{x}" cy="{y}" r="3.2" fill="#eeeeee"/>')
        self.add(f'<circle cx="{x}" cy="{y}" r="2.2" fill="#111"/>')
        if label: self.text(x, y - 5.4, label, JCK, lab_fill or ("#f3efe8" if on_plate else INK))

    def button(self, x, y, label=None, lit=None, r=3.0, lab_fill=INK, square=False):
        col = {None: "#2c2c2f", "o": ORANGE, "c": CYAN, "a": "#f0b030", "w": "#f4f1ea", "r": "#e2483b", "g": "#4fc46a"}[lit]
        if square:
            self.add(f'<rect x="{x-r}" y="{y-r}" width="{2*r}" height="{2*r}" rx="0.8" fill="#18181a"/>')
            self.add(f'<rect x="{x-r+0.6}" y="{y-r+0.6}" width="{2*r-1.2}" height="{2*r-1.2}" rx="0.5" fill="{col}"/>')
        else:
            self.add(f'<circle cx="{x}" cy="{y}" r="{r+0.7}" fill="#18181a"/>')
            self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}"/>')
        if label: self.text(x, y - r - 1.8, label, CTL, lab_fill)

    def led(self, x, y, col, r=0.9):
        self.add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}"/>')

    def plate(self, x, y, w, h, label=None):
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.6" fill="{PLATE}"/>')

    def rule(self, x1, x2, y, col=ACC, wdt=0.45):
        self.add(f'<line x1="{x1}" y1="{y}" x2="{x2}" y2="{y}" stroke="{col}" stroke-width="{wdt}"/>')

    def vrule(self, x, y1, y2):
        self.add(f'<line x1="{x}" y1="{y1}" x2="{x}" y2="{y2}" stroke="#cfc6b6" stroke-width="0.35"/>')

    def svg(self):
        w = self.w
        screws = "".join(f'<g><circle cx="{sx}" cy="{sy}" r="2.4" fill="#d6d6d6" stroke="#9a9a9a" stroke-width="0.3"/>'
                         f'<path d="M{sx-1.3} {sy}H{sx+1.3}M{sx} {sy-1.3}V{sy+1.3}" stroke="#777" stroke-width="0.45"/></g>'
                         for sx, sy in [(7.5, 3.6), (w - 7.5, 3.6), (7.5, H - 3.6), (w - 7.5, H - 3.6)])
        head = (f'<text x="8" y="13.2" font-family="{FONT}" font-size="6.2" font-weight="700" fill="{INK}" letter-spacing="0.5">{self.title}</text>'
                f'<text x="{w-8}" y="13.2" font-family="{FONT}" font-size="3.4" font-weight="500" fill="{QUIET}" text-anchor="end" letter-spacing="0.15">{self.tagline}</text>'
                f'<line x1="8" y1="15.6" x2="{w-8}" y2="15.6" stroke="{ACC}" stroke-width="0.5"/>'
                f'<text x="{w/2}" y="{H-2.4}" font-family="{FONT}" font-size="2.9" font-weight="500" fill="{QUIET}" text-anchor="middle">{self.footer}</text>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}mm" height="{H}mm" viewBox="0 0 {w:.2f} {H}">'
                f'<rect width="{w:.2f}" height="{H}" fill="{BG}"/>{screws}{head}{"".join(self.el)}{"".join(self.top)}</svg>')


# ---------------------------------------------------------------- LONG FORM
def long_form():
    p = Panel(52, "LONG FORM", "an arrangement that steers and drifts", "sections down, lanes across  ·  Haer")
    w = p.w
    # display strip
    dx, dy, dw, dh = 8, 18.5, w - 16, 15
    p.add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="1.2" fill="{DISP}"/>')
    form = [("INTRO", 16, 2), ("GROOVE", 32, 4), ("BUILD", 8, 6), ("PEAK", 32, 8), ("BREAK", 16, 3), ("DROP", 8, 7), ("PEAK 2", 32, 9), ("OUTRO", 16, 1)]
    tot = sum(f[1] for f in form); bx = dx + 2; bw = dw - 4; cur = 0
    for i, (nm, ln, en) in enumerate(form):
        ww = bw * ln / tot
        op = 0.25 + en * 0.075
        p.add(f'<rect x="{bx+0.25:.2f}" y="{dy+2:.2f}" width="{ww-0.5:.2f}" height="5" rx="0.5" fill="{ORANGE}" opacity="{op:.2f}"/>')
        if i == 4: p.add(f'<rect x="{bx+0.25:.2f}" y="{dy+2:.2f}" width="{ww-0.5:.2f}" height="5" rx="0.5" fill="none" stroke="{CYAN}" stroke-width="0.45"/>')
        if i == 3: cur = bx + ww * 0.6
        if ww > 14: p.text(bx + ww / 2, dy + 5.6, nm, 2.3, "#1d1e22", weight=700)
        bx += ww
    p.add(f'<line x1="{cur}" y1="{dy+1.3}" x2="{cur}" y2="{dy+7.7}" stroke="#fff" stroke-width="0.5"/>')
    p.text(dx + 2.5, dy + 12, "PEAK  ·  bar 20 of 32", 3.0, "#f3efe8", anchor="start", weight=600)
    p.text(dx + dw / 2 + 6, dy + 12, "next: BREAK (cued)", 3.0, CYAN, weight=500)
    p.text(dx + dw - 2.5, dy + 12, "drift: Build, hats out", 3.0, "#b9b3a9", anchor="end", weight=500)

    # grid
    gx0, gy0, pitch, rp = 22, 47, 8.6, 8.4
    p.text(10, gy0 - 6.5, "CUE", CTL)
    for c in range(8):
        p.text(gx0 + c * pitch, gy0 - 6.5, str(c + 1), CTL, ACC, weight=700)
    lenx, enx = gx0 + 8 * pitch + 4.5, gx0 + 8 * pitch + 18.5
    p.text(lenx, gy0 - 6.5, "LEN", CTL); p.text(enx, gy0 - 6.5, "ENERGY", CTL)
    masks = ["11000000", "11110010", "11111110", "11111111", "00001101", "10000011", "11111111", "01001000"]
    angs = [-90, -10, -130, -10, -90, -130, -10, -90]
    eangs = [-110, -50, 10, 70, -80, 40, 100, -135]
    for r in range(8):
        y = gy0 + r * rp
        if r % 2 == 0: p.add(f'<rect x="4" y="{y-rp/2:.2f}" width="{enx+7-4:.2f}" height="{rp:.2f}" fill="#e2d9c8" opacity="0.6"/>')
        p.button(10, y, None, "c" if r == 4 else ("w" if r == 3 else None), r=2.4)
        for c in range(8):
            lit = "o" if masks[r][c] == "1" else None
            if r == 1 and c == 6: lit = "a"
            p.button(gx0 + c * pitch, y, None, lit, r=2.9, square=True)
        p.knob(lenx, y, r=3.2, angle=angs[r]); p.knob(enx, y, r=3.2, angle=eangs[r])
    p.text(4, gy0 + 8 * rp - 0.8, "cell = lane plays in that section  ·  CUE = jump at next phrase", 2.6, QUIET, anchor="start", weight=500)

    # right column
    x0 = enx + 11
    p.vrule(x0 - 4, 38, H - 8)
    rw = w - 9 - x0
    y1 = 50
    for i, (nm, ang) in enumerate((("STEER", 30), ("FOLLOW", 110), ("DRIFT", -60))):
        bx = x0 + 8 + i * rw / 3
        p.knob(bx, y1, 6.0, nm, angle=ang)
        p.jack(bx + 13.5, y1 + 1.5, "CV")
    y2 = 71
    items = [("b", "HOLD"), ("b", "NEXT"), ("b", "LOCK"), ("b", "RESTORE"), ("k", "GLIDE"), ("k", "FILL LEN")]
    for i, (kind, nm) in enumerate(items):
        bx = x0 + 6 + i * (rw - 8) / 5
        if kind == "b": p.button(bx, y2, nm, None, r=3.0)
        else: p.knob(bx, y2 + 0.5, 4.2, nm, angle=-60 if nm == "GLIDE" else -20)
    y3 = 87.5
    for i, nm in enumerate(["CLOCK", "RESET", "RUN", "HOLD", "NEXT", "CUE"]):
        p.jack(x0 + 6 + i * (rw - 8) / 5, y3, nm)
    py0 = 94.5
    p.plate(x0 - 3, py0, w - 11.5 - (x0 - 3), 122 - py0)
    pitch_o = (w - 11.5 - x0 - 10) / 8
    oc = [x0 + 5 + i * pitch_o for i in range(9)]
    yo1, yo2 = py0 + 9.3, py0 + 22.2
    for i in range(8):
        p.jack(oc[i], yo1, f"L{i+1}", True)
    p.jack(oc[8], yo1, "ACT", True, lab_fill="#7fd08f")
    for i, nm in enumerate(["ENERGY", "RAMP", "FILL", "CHANGE", "BAR", "PHRASE", "END"]):
        p.jack(oc[i], yo2, nm, True)
    p.jack(oc[8], yo2, "LINK", True, lab_fill=CYAN)
    return p


# ---------------------------------------------------------------- MUTE THEORY
def mute_theory():
    p = Panel(58, "MIXXA", "mutes land on the next beat", "8 channels  ·  random mutes  ·  kick ducking  ·  glue  ·  Haer")
    w = p.w
    sw, sx0 = 22, 6
    states = ["play", "play", "armed", "muted", "play", "override", "muted", "play"]
    fades = [0.55, 0.4, 0.5, 0.35, 0.45, 0.3, 0.38, 0.42]
    for i in range(8):
        cx = sx0 + sw * i + sw / 2
        if i % 2 == 0: p.add(f'<rect x="{sx0+sw*i:.2f}" y="18" width="{sw:.2f}" height="{H-26}" fill="#e2d9c8" opacity="0.55"/>')
        p.text(cx, 23.2, str(i + 1) + (" KICK" if i == 0 else ""), CTL, ACC, weight=700)
        p.jack(cx - 4.9, 32.5, "IN L"); p.jack(cx + 4.9, 32.5, "R")
        p.knob(cx, 46, 3.6, "PAN", angle=[0, -30, 25, 0, -60, 60, 10, -20][i])
        p.knob(cx, 59, 3.6, "DUCK", angle=-135 if i == 0 else [-135, 40, 40, 20, 60, -20, 0, 30][i], lab_fill=QUIET if i == 0 else INK)
        # slider
        sy0, sy1 = 67, 101
        p.add(f'<rect x="{cx-3.6:.2f}" y="{sy0}" width="1.6" height="{sy1-sy0}" rx="0.8" fill="#bdb4a4"/>')
        ky = sy1 - (sy1 - sy0) * fades[i] - 6
        p.add(f'<rect x="{cx-6.4:.2f}" y="{ky:.2f}" width="7.2" height="4.4" rx="0.8" fill="#1b1b1d"/>')
        p.add(f'<line x1="{cx-6.0:.2f}" y1="{ky+2.2:.2f}" x2="{cx+0.4:.2f}" y2="{ky+2.2:.2f}" stroke="#f2f2f2" stroke-width="0.5"/>')
        # meter
        lvl = 0 if states[i] == "muted" else [5, 3, 4, 0, 3, 0, 0, 4][i]
        for k in range(6):
            col = "#e2483b" if k == 5 else ("#f0b030" if k == 4 else "#4fc46a")
            p.led(cx + 4.6, sy1 - 2 - k * 5.8, col if k < lvl else "#3a3a3a", r=1.0)
        p.text(cx - 2.8, sy1 + 4.2, "LEVEL", 2.9, INK)
        # mute / solo
        st = states[i]
        mlit = {"play": None, "armed": "o", "muted": "r", "override": "a"}[st]
        p.button(cx - 5.6, 115, "MUTE", mlit, r=3.0)
        p.button(cx + 5.6, 115, "SOLO", None, r=2.5)
    # strip dividers
    for i in range(1, 8): p.vrule(sx0 + sw * i, 20, H - 8)
    rx0 = sx0 + sw * 8 + 3
    p.vrule(rx0 - 1.5, 20, H - 8)
    rc = rx0 + 13
    p.text(rc, 23.2, "RANDOM", CTL, ACC, weight=700)
    p.button(rc, 33, "DICE", "o", r=3.6)
    p.jack(rc, 48, "TRIG")
    p.knob(rc, 63.5, 4.8, "COUNT", angle=-30)
    p.text(rc - 7.0, 71.6, "0", 2.6, QUIET, weight=500); p.text(rc + 7.0, 71.6, "8", 2.6, QUIET, weight=500)
    p.jack(rc, 81, "CV")
    p.button(rc, 96, "BACK", None, r=3.0)
    # KICK in/out toggle switch (shown IN = up)
    ty = 113
    p.text(rc, ty - 6.6, "KICK", CTL)
    p.add(f'<rect x="{rc-2.2}" y="{ty-4.2}" width="4.4" height="8.4" rx="1.0" fill="#1b1b1d"/>')
    p.add(f'<rect x="{rc-1.5}" y="{ty-3.6}" width="3.0" height="3.8" rx="0.7" fill="#e8e8e8"/>')
    p.text(rc + 3.6, ty - 1.4, "IN", 2.6, INK, anchor="start", weight=600)
    p.text(rc + 3.6, ty + 3.6, "OUT", 2.6, QUIET, anchor="start", weight=500)
    mx0 = rx0 + 29
    p.vrule(mx0 - 1.5, 20, H - 8)
    c = [mx0 + 6 + i * 12.3 for i in range(5)]
    # scenes
    p.text(mx0 + 3, 23.2, "SCENES", CTL, ACC, anchor="start", weight=700)
    for j, s in enumerate("ABCD"):
        p.button(c[j], 33, s, "w" if s == "B" else None, r=3.0)
    p.jack(c[4] - 0.5, 33, "SCENE")
    p.text(mx0 + 3, 40.5, "tap: recall on next beat  ·  hold: store", 2.6, QUIET, anchor="start", weight=500)
    # ducking
    p.rule(mx0 + 2, w - 8, 44, "#cfc6b6", 0.35)
    p.text(mx0 + 3, 49.6, "KICK DUCK", CTL, ACC, anchor="start", weight=700)
    p.jack(c[0], 60, "KICK")
    p.knob(c[1] + 3, 60, 4.4, "RELEASE", angle=-20)
    p.knob(c[3] - 1, 60, 4.4, "SHAPE", angle=30)
    p.jack(c[4], 60, "CLOCK")
    # master
    p.rule(mx0 + 2, w - 8, 68, "#cfc6b6", 0.35)
    p.text(mx0 + 3, 73.6, "MASTER", CTL, ACC, anchor="start", weight=700)
    p.knob(c[0] + 3, 87, 6.0, "GLUE", angle=-30)
    # GR meter
    for k in range(6):
        p.led(c[1] + 3.5, 80 + k * 3.0, "#f0b030" if k < 2 else "#3a3a3a", r=0.95)
    p.text(c[1] + 3.5, 76.6, "GR", 2.9, QUIET)
    p.knob(c[2] + 7, 87, 6.0, "MASTER", angle=0)
    for side, xx in (("L", c[4] - 1.8), ("R", c[4] + 1.8)):
        for k in range(8):
            col = "#e2483b" if k == 7 else ("#f0b030" if k >= 5 else "#4fc46a")
            p.led(xx, 96 - k * 2.5, col if k < (5 if side == "L" else 4) else "#3a3a3a", r=0.95)
    p.text(c[4], 75.6, "OUT", 2.9, QUIET)
    # inputs row
    p.jack(c[0], 111, "ACT"); p.jack(c[1], 111, "LINK", lab_fill="#1f8fa0")
    # outputs plate
    px0 = c[2] - 6.5
    p.plate(px0, 100, w - 11.5 - px0, 122 - 100)
    p.jack(c[2] + 0.5, 111, "OUT L", True); p.jack(c[3] + 0.5, 111, "OUT R", True)
    p.jack(c[4] + 0.5, 111, "STEMS", True)
    return p


if __name__ == "__main__":
    out = sys.argv[1]
    for name, fn in (("LongForm", long_form), ("Mixxa", mute_theory)):
        with open(f"{out}/{name}.svg", "w") as f: f.write(fn().svg())
