#!/usr/bin/env python3
# Four Dub Siren panel prototypes (2.1.0: HARDWARE / EXTRAS switch, bigger knobs).
# python3 gen.py -> option-A..D.png + panel-options.png
import math, os, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
H = 128.5
RED, YEL, GRN = "#C8282A", "#EFD84A", "#3E8A44"
INK, SOFT, STRIP, CREAM = "#1B1B1B", "#3A3C3B", "#232425", "#F1EEE4"

class P:
    def __init__(s, hp, title):
        s.W = hp * 5.08; s.hp = hp; s.o = []; s.title = title
        W = s.W
        s.o.append(f'<defs><linearGradient id="al" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="{W}" y2="{H}"><stop offset="0" stop-color="#D2D4D1"/><stop offset="0.5" stop-color="#B9BBB8"/><stop offset="1" stop-color="#CACCC9"/></linearGradient></defs>')
        s.o.append(f'<rect width="{W}" height="{H}" fill="url(#al)"/>')
        for i in range(60):
            s.o.append(f'<rect x="0" y="{2+i*2.1:.2f}" width="{W}" height="0.18" fill="#fff" fill-opacity="{0.05+0.04*((i*7)%5)/4:.3f}"/>')
        for x, y in ((7.5, 3), (W-7.5, 3), (7.5, H-3), (W-7.5, H-3)):
            s.o.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="#9a9c99" stroke="#6d6f6c" stroke-width="0.3"/><path d="M{x-1.4} {y}h2.8M{x} {y-1.4}v2.8" stroke="#555" stroke-width="0.5"/>')
        s.t(13.5 if hp < 30 else 14, 11.2, "DUB SIREN", 6.4, anchor="start", w=900)
        s.t(W/2, 126.8, "Hær", 3.4, w=900, fam="Climate Crisis")
    def t(s, x, y, txt, size=3.2, anchor="middle", fill=INK, w=700, fam="Inter"):
        s.o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{fam}" font-weight="{w}" font-size="{size}" text-anchor="{anchor}" fill="{fill}">{txt}</text>')
    def knob(s, x, y, d, cap, label, scale=None, ang=-30, below=False):
        r = d/2
        s.o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#121212"/>')
        n = 32 if d > 12 else 22
        for i in range(n):
            a = 2*math.pi*i/n
            s.o.append(f'<line x1="{x+(r-.05)*math.cos(a):.2f}" y1="{y+(r-.05)*math.sin(a):.2f}" x2="{x+(r-1.0)*math.cos(a):.2f}" y2="{y+(r-1.0)*math.sin(a):.2f}" stroke="#3a3a3a" stroke-width="0.35"/>')
        rc = r*0.74
        s.o.append(f'<circle cx="{x}" cy="{y}" r="{rc:.2f}" fill="{cap}" stroke="#000" stroke-opacity=".35" stroke-width=".25"/><circle cx="{x-rc*.25:.2f}" cy="{y-rc*.3:.2f}" r="{rc*.5:.2f}" fill="#fff" fill-opacity=".12"/>')
        a = math.radians(ang)
        s.o.append(f'<line x1="{x+rc*.25*math.sin(a):.2f}" y1="{y-rc*.25*math.cos(a):.2f}" x2="{x+rc*.92*math.sin(a):.2f}" y2="{y-rc*.92*math.cos(a):.2f}" stroke="#fff" stroke-width="{1.0 if d>12 else .8}" stroke-linecap="round"/>')
        s.t(x, (y + r + 4.6) if below else (y - r - 2.4), label, 3.2)
        if scale:
            R = r + 3.0; k = len(scale)
            for i, l in enumerate(scale):
                a = math.radians(-150 + 300*i/(k-1))
                sx, sy = x + R*math.sin(a), y - R*math.cos(a) + 1.0
                anc = "middle"
                if abs(math.sin(a)) > .4: anc = "start" if math.sin(a) > 0 else "end"
                s.t(sx + (-.8 if anc == "start" else .8 if anc == "end" else 0), sy, l, 2.7, anchor=anc, fill=SOFT)
    def switch(s, x, y, label, up="ON", down="OFF", hl=False):
        if hl: s.o.append(f'<rect x="{x-4.2}" y="{y-11.2}" width="{8.4+len(max(up,down,key=len))*2.0+1}" height="17" rx="1.2" fill="{RED}" fill-opacity=".14" stroke="{RED}" stroke-width=".35"/>')
        s.o.append(f'<rect x="{x-1.9}" y="{y-4}" width="3.8" height="8" rx=".5" fill="#1a1a1a"/><rect x="{x-1.5}" y="{y-3.6}" width="3" height="3.6" rx=".3" fill="#bdbdbd"/>')
        s.t(x, y-6.6, label, 3.2)
        s.t(x+3, y-1.2, up, 2.7, anchor="start", fill=SOFT); s.t(x+3, y+3.7, down, 2.7, anchor="start", fill=SOFT)
    def button(s, x, y, label, col):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="4.6" fill="#b5b5b5" stroke="#666" stroke-width=".4"/><circle cx="{x}" cy="{y}" r="3.3" fill="{col}"/>')
        s.t(x, y-6.4, label, 3.2)
    def led(s, x, y, label="ACTIVE"):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="2.3" fill="#E8302A" stroke="#5a1010" stroke-width=".35"/>')
        s.t(x, y-4.0, label, 2.9, fill=SOFT)
    def jack(s, x, y, label, out=False, dark=True):
        s.o.append(f'<circle cx="{x}" cy="{y}" r="3.4" fill="#d8d8d8" stroke="#777" stroke-width=".4"/><circle cx="{x}" cy="{y}" r="2.0" fill="#111"/>')
        s.t(x, y-5.0, label, 2.9, fill=CREAM if dark or out else INK)
    def strip(s, x, y, w, h, col=STRIP):
        s.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.4" fill="{col}"/>')
    def box(s, x, y, w, h, label=None):
        s.o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="1.6" fill="none" stroke="{SOFT}" stroke-width=".35" stroke-dasharray="1.2 0.8"/>')
        if label:
            s.o.append(f'<rect x="{x+3}" y="{y-1.9}" width="{len(label)*2.15+2}" height="3.6" fill="#C6C8C5"/>')
            s.t(x+4, y+1.0, label, 2.7, anchor="start", fill=SOFT)
    def svg(s):
        return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {s.W} {H}" width="{s.W*7}" height="{H*7}">' + "".join(s.o) + '</svg>'

HW = dict(speed=("SPEED", RED), pitch=("PITCH", RED))
P14 = ["1", "2", "3", "4"]; PB = ["1", "2", "3", "OFF"]
JACKS = ["V/OCT", "SPEED", "FDBK", "CLOCK", "TRIG", "SIREN", "TONE", "IN"]
designs = []

# ---- A: the box, bigger. Right column PITCH/MODE/BEAT/VOL in one line, jacks along the bottom
a = P(28, "A"); W = a.W
L, R = 32.0, 110.0
a.knob(L, 29, 18, RED, "SPEED", ang=0); a.knob(R, 27, 16, RED, "PITCH", P14, ang=-50)
a.led(71, 29)
a.knob(L, 53, 16, YEL, "FEEDBACK", ang=-40); a.knob(R, 53.5, 15, YEL, "MODE", P14, ang=150)
a.switch(14, 77, "TRIG"); a.button(34, 77, "SIREN", RED); a.button(53, 77, "TONE", "#222")
a.knob(R, 79.5, 15, YEL, "BEAT", PB, ang=-150)
a.knob(R, 103, 14, GRN, "SIREN VOL", ang=-60, below=False)
a.box(4, 93, 92, 15.5, "EXTRAS")
a.switch(13, 103, "", "ON", "OFF", hl=False)
for x, l in ((36, "DEPTH"), (55, "RELEASE"), (73, "TIME")): a.knob(x, 103.5, 8.5, YEL, l, ang=10)
a.switch(84, 103, "ERA", "80s", "90s")
a.strip(2.5, 111, W-5, 13.0)
st = (W-16)/8
for i, l in enumerate(JACKS): a.jack(8+i*st, 119.5, l)
a.strip(8+8*st-5.6, 111.8, 11.2, 11.4, RED); a.jack(8+8*st, 119.5, "OUT", True)
designs.append(("A", "The box, bigger", "28 HP. Same layout as your box with bigger knobs. PITCH, MODE, BEAT and SIREN VOL sit in one straight column on the right. Extras sit in their own dashed box with the EXTRAS switch at its left. Jacks along the bottom.", a))

# ---- B: jacks down the left side
b = P(30, "B"); W = b.W
b.strip(2.5, 15, 27, 106)
JL = 9.5; JR = 22.5
for i, l in enumerate(JACKS):
    b.jack(JL if i % 2 == 0 else JR, 25 + (i//2)*20 + (0 if i % 2 == 0 else 10) - 0, l)
b.strip(JR-5.6, 99.5, 11.2, 12.2, RED); b.jack(JR, 107.8, "OUT", True)
L, R = 56.0, 120.0
b.knob(L, 31, 20, RED, "SPEED", ang=0); b.knob(R, 31, 20, RED, "PITCH", P14, ang=-50)
b.led(88, 31)
b.knob(L, 59, 17, YEL, "FEEDBACK", ang=-40); b.knob(R, 59, 17, YEL, "MODE", P14, ang=150)
b.switch(40, 84, "TRIG"); b.button(59, 84, "SIREN", RED); b.button(77, 84, "TONE", "#222")
b.knob(R, 84.5, 17, YEL, "BEAT", PB, ang=-150)
b.box(33, 96, 70, 26, "EXTRAS")
b.switch(40, 110, "", "ON", "OFF")
for x, l in ((53, "DEPTH"), (67, "RELEASE"), (81, "TIME")): b.knob(x, 108, 8.5, YEL, l, ang=10)
b.switch(92, 110, "ERA", "80s", "90s")
b.switch(56, 120.4, "", "", "") if False else None
b.t(70, 119.4, "", 2)
b.knob(R, 110, 15, GRN, "SIREN VOL", ang=-60)
designs.append(("B", "Jacks down the left", "30 HP. All jacks in a dark column on the left, so the controls get the whole right side and the biggest knobs (20 mm reds). Right column PITCH, MODE, BEAT, SIREN VOL lined up. Extras sit in a dashed box under the buttons.", b))

# ---- C: jacks down the right side, extras panel on the left
c = P(30, "C"); W = c.W
c.strip(W-29.5, 15, 27, 106)
JL = W-22.5; JR = W-9.5
for i, l in enumerate(JACKS):
    c.jack(JL if i % 2 == 0 else JR, 25 + (i//2)*20 + (0 if i % 2 == 0 else 10), l)
c.strip(JL-5.6, 99.5, 11.2, 12.2, RED); c.jack(JL, 107.8, "OUT", True)
L, R = 50.0, 94.0
c.knob(L, 31, 20, RED, "SPEED", ang=0); c.knob(R, 31, 20, RED, "PITCH", P14, ang=-50)
c.knob(L, 59, 17, YEL, "FEEDBACK", ang=-40); c.knob(R, 59, 17, YEL, "MODE", P14, ang=150)
c.knob(R, 85, 17, YEL, "BEAT", PB, ang=-150)
c.knob(R, 110, 15, GRN, "SIREN VOL", ang=-60)
c.switch(36, 84, "TRIG"); c.button(54, 84, "SIREN", RED); c.button(70, 84, "TONE", "#222")
c.led(70, 104)
c.box(3.5, 18, 22, 104, "EXTRAS")
c.switch(11, 30, "", "ON", "OFF")
for y, l in ((49, "DEPTH"), (67, "RELEASE"), (85, "TIME")): c.knob(14.5, y, 9, YEL, l, ang=10)
c.switch(11, 107, "ERA", "80s", "90s")
designs.append(("C", "Jacks down the right", "30 HP. Jacks in a column on the right, extras in a slim strip down the left that you can ignore in hardware mode. The six box knobs and buttons sit in the middle as on your box, SIREN VOL lined up under BEAT.", c))

# ---- D: hardware on top, extras + jacks below a line
d = P(26, "D"); W = d.W
xs = [22, 66, 110]
d.knob(xs[0], 30, 19, RED, "SPEED", ang=0); d.knob(xs[2], 30, 19, RED, "PITCH", P14, ang=-50)
d.knob(xs[1], 30, 16, YEL, "FEEDBACK", ang=-40)
d.knob(xs[0], 59, 16, YEL, "MODE", P14, ang=150); d.knob(xs[1], 59, 16, YEL, "BEAT", PB, ang=-150)
d.knob(xs[2], 59, 16, GRN, "SIREN VOL", ang=-60)
d.switch(16, 82, "TRIG"); d.button(40, 82, "SIREN", RED); d.button(60, 82, "TONE", "#222"); d.led(80, 82)
d.o.append(f'<line x1="4" y1="90.5" x2="{W-4}" y2="90.5" stroke="{SOFT}" stroke-width=".4"/>')
d.switch(11, 101, "EXTRAS", "ON", "OFF")
for x, l in ((33, "DEPTH"), (50, "RELEASE"), (67, "TIME")): d.knob(x, 101.5, 8.5, YEL, l, ang=10)
d.switch(79, 101, "ERA", "80s", "90s")
d.jack(101, 101.5, "IN", dark=False); d.jack(117, 101.5, "CLOCK", dark=False)
d.strip(2.5, 110.5, W-5, 13.5)
st = (W-16)/7
for i, l in enumerate(["V/OCT", "SPEED", "FDBK", "TRIG", "SIREN", "TONE"]): d.jack(8+i*st, 119, l)
d.strip(8+7*st-5.6-0.0 - (st), 111.3, 0, 0)
d.strip(W-8-5.6, 111.3, 11.2, 11.9, RED); d.jack(W-8, 119, "OUT", True)
designs.append(("D", "Hardware top, extras below", "26 HP. The top two-thirds is just the box: three big knobs, then MODE, BEAT and SIREN VOL in one straight row, with TRIG, SIREN, TONE and the LED below. A line separates the extras row and jacks underneath.", d))

# render
html = '<html><head><link href="https://fonts.googleapis.com/css2?family=Inter:wght@700;900&family=Climate+Crisis&display=swap" rel="stylesheet"><style>body{margin:0;background:#2b2d31;font-family:Inter;color:#eee} .w{display:inline-block;vertical-align:top;margin:18px;width:%dpx} h2{margin:6px 0;font-size:22px} p{font-size:14px;line-height:1.35;color:#ccc}</style></head><body>'
blocks = []
for key, name, desc, p in designs:
    svg = p.svg()
    one = f'<html><head><link href="https://fonts.googleapis.com/css2?family=Inter:wght@700;900&family=Climate+Crisis&display=swap" rel="stylesheet"></head><body style="margin:0;background:#2b2d31">{svg}</body></html>'
    open(os.path.join(HERE, f"_{key}.html"), "w").write(one)
    blocks.append(f'<div class="w" style="width:{int(p.W*5.2)}px"><h2>{key}. {name}</h2>{svg.replace(f"width=\"{p.W*7}\" height=\"{H*7}\"", f"width=\"{p.W*5.2}\" height=\"{H*5.2}\"")}<p>{desc}</p></div>')
open(os.path.join(HERE, "_all.html"), "w").write(html.replace("%d", "300") + "".join(blocks) + "</body></html>")
js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();
for (const k of ['A','B','C','D']){const p=await b.newPage({viewport:{width:%W%,height:%H%}});await p.goto('file://%DIR%/_'+k+'.html');await p.waitForTimeout(900);await p.screenshot({path:'%DIR%/option-'+k+'.png',fullPage:true});await p.close();}
const p=await b.newPage({viewport:{width:2900,height:900}});await p.goto('file://%DIR%/_all.html');await p.waitForTimeout(900);await p.screenshot({path:'%DIR%/panel-options.png',fullPage:true});await b.close();})();"""
js = js.replace("%W%", str(int(max(p.W for *_, p in designs)*7))).replace("%H%", str(int(H*7))).replace("%DIR%", HERE)
subprocess.run(["node", "-e", js], check=True)
for f in os.listdir(HERE):
    if f.startswith("_") and f.endswith(".html"): os.remove(os.path.join(HERE, f))
print("ok")
