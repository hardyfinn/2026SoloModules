#!/usr/bin/env python3
# Panel preview: res/Breaker.svg + stand-in knobs/jacks/buttons + the two screens
# filled from a real analysis (JSON from the engine). Usage:
#   python3 tools/preview_panel.py dump.json out.svg
import json, math, re, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(sys.argv[1]))
svg = open(os.path.join(HERE, "..", "res", "Breaker.svg")).read()
lay = open(os.path.join(HERE, "..", "src", "Layout.hpp")).read()
pos = {m[0]: (float(m[1]), float(m[3])) for m in re.findall(r"static const float (\w+)_X = ([\d.]+)f, (\w+)_Y = ([\d.]+)f;", lay)}
COL = ["#FF4D4D", "#FFD23F", "#46D3FF", "#B98CFF"]; LET = "KSHO"
o = []
def knob(x, y, r, a):
    o.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#1b1b1b" stroke="#3a3a3a" stroke-width="0.4"/>')
    o.append(f'<circle cx="{x}" cy="{y}" r="{r*0.78}" fill="#262626"/>')
    ang = math.radians(-135 + 270 * a)
    o.append(f'<line x1="{x}" y1="{y}" x2="{x+math.sin(ang)*r*0.8:.2f}" y2="{y-math.cos(ang)*r*0.8:.2f}" stroke="#eee" stroke-width="0.6" stroke-linecap="round"/>')
vals = {"STYLE": 0, "DENSITY": .5, "MUTATE": .3, "GHOSTS": .55, "FILLS": .25, "SWING": .2, "SENS": .5, "BARS": .5, "PITCH": .5, "DECAY": 1, "VARIETY": .4, "LEVEL": .5}
for k, (x, y) in pos.items():
    n = k[2:]
    if k.startswith("K_"): knob(x, y, 5.0 if y < 80 else 4.0, vals.get(n, .5))
    elif k.startswith("I_") or k.startswith("O_"):
        o.append(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#b8b8b8"/><circle cx="{x}" cy="{y}" r="2.2" fill="#555"/><circle cx="{x}" cy="{y}" r="1.6" fill="#0a0a0a"/>')
    elif k.startswith("B_"):
        on = n in ("LOCK", "RUN")
        o.append(f'<rect x="{x-3.2}" y="{y-3.2}" width="6.4" height="6.4" rx="1" fill="#2a2a2a" stroke="#555" stroke-width="0.3"/>')
        o.append(f'<rect x="{x-2.2}" y="{y-2.2}" width="4.4" height="4.4" rx="0.6" fill="{"#FF7A1A" if on else "#3a2a20"}"/>')
for n in ("STYLE",): pass
# wave screen
wx, wy, ww, wh = 5.0, 14.0, 172.88, 24.0
x0, x1 = wx + 2.5, wx + ww - 2.5
mono = "font-family:'DejaVu Sans Mono',monospace;font-weight:700"
o.append(f'<text x="{x0}" y="{wy+4.2}" style="{mono};font-size:2.9px;fill:#F2EFE6">{"made-up-break.wav"}   {d["bars"]} BARS   {d["bpm"]} BPM</text>')
cnt = [0] * 4
for s, t, g in d["hits"]: cnt[t] += 1
o.append(f'<text x="{x1}" y="{wy+4.2}" text-anchor="end" style="{mono};font-size:2.9px">' + "  ".join(f'<tspan fill="{COL[t]}">{LET[t]} {cnt[t]}</tspan>' for t in range(4)) + '</text>')
ty, th = wy + 5.4, 3.3
y0, y1 = ty + th + 0.4, wy + wh - 1.2
mid, amp = (y0 + y1) / 2, (y1 - y0) / 2
pk = d["peaks"]
path = "".join(f"M{x0 + (x1-x0)*i/len(pk):.2f},{mid - v*amp:.2f}L{x0 + (x1-x0)*i/len(pk):.2f},{mid + v*amp:.2f}" for i, v in enumerate(pk))
o.append(f'<path d="{path}" stroke="#6F7A72" stroke-width="0.2"/>')
for i, (s, t, g) in enumerate(d["hits"]):
    x = x0 + (x1 - x0) * s / d["n"]; play = i == 7
    o.append(f'<line x1="{x:.2f}" y1="{ty+th}" x2="{x:.2f}" y2="{y1}" stroke="{COL[t]}" stroke-opacity="{0.45 if g else 0.8}" stroke-width="{0.5 if play else 0.3}"/>')
    o.append(f'<rect x="{x-2:.2f}" y="{ty}" width="4" height="{th}" rx="0.5" fill="{"#fff" if play else COL[t]}"/>')
    o.append(f'<text x="{x:.2f}" y="{ty+2.5}" text-anchor="middle" style="{mono};font-size:2.5px;fill:#111">{LET[t]}</text>')
# grid screen
gx, gy, gw = 5.0, 40.0, 172.88
o.append(f'<text x="{gx+2.5}" y="{gy+4.2}" style="{mono};font-size:2.8px;fill:#F2EFE6">JUNGLE   BAR 1/2   MUTATE 30%</text>')
o.append(f'<text x="{gx+gw-2.5}" y="{gy+4.2}" text-anchor="end" style="{mono};font-size:2.8px;fill:#FF7A1A">LOCKED: K S</text>')
cw = (gw - 14) / 16
for l in range(4):
    y = gy + 6.2 + l * 4.5
    o.append(f'<text x="{gx+2.6}" y="{y+3}" style="{mono};font-size:3px;fill:{COL[l]}">{LET[l]}</text>')
    if l < 2: o.append(f'<rect x="{gx+6.6}" y="{y+0.8}" width="2" height="2" rx="0.3" fill="#FF7A1A"/>')
    for s in range(16):
        st = d["cells"][l][s]; x = gx + 11 + s * cw
        fill = ("#22252B" if s % 4 else "#30343D") if st == 0 else COL[l]
        op = 0.45 if st == 1 else 1
        o.append(f'<rect x="{x+0.25:.2f}" y="{y}" width="{cw-0.5:.2f}" height="3.6" rx="0.35" fill="{fill}" fill-opacity="{op}"/>')
        if st == 3: o.append(f'<rect x="{x+0.5:.2f}" y="{y+0.25}" width="{cw-1:.2f}" height="0.25" fill="#fff" fill-opacity=".8"/>')
x = gx + 11 + 6 * cw
o.append(f'<rect x="{x+0.1:.2f}" y="{gy+5.6}" width="{cw-0.2:.2f}" height="{4.5*3+3.6+1.2}" rx="0.45" fill="none" stroke="#fff" stroke-width="0.25"/>')
# screws
W = 182.88
for sx, sy in ((7.62, 2.9), (W - 7.62, 2.9), (7.62, 125.6), (W - 7.62, 125.6)):
    o.append(f'<circle cx="{sx}" cy="{sy}" r="1.9" fill="#c8c8c8"/><line x1="{sx-1.2}" y1="{sy}" x2="{sx+1.2}" y2="{sy}" stroke="#777" stroke-width="0.4"/>')
i = svg.rfind("</svg>")
open(sys.argv[2], "w").write(svg[:i] + "\n".join(o) + svg[i:])
