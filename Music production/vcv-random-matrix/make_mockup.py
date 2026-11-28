"""Panel mock-up for the random 32x32 matrix mixer (house style)."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "vcv-long-form-mute-theory"))
from make_panels import Panel, DISP, ORANGE, CYAN, INK, QUIET, ACC, H, CTL, BG

p = Panel(36, "DRIFTMATRIX", "32 x 32, unseen", "")
w = p.w
# display
dx, dy, dw, dh = 8, 18.5, w - 16, 10.5
p.add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="1.2" fill="{DISP}"/>')
p.text(dx + 3, dy + 4.6, "VCA  ·  chance 35%  ·  358 of 1024 live", 3.0, "#f3efe8", anchor="start", weight=600)
p.text(dx + 3, dy + 8.6, "seed 4F21  ·  avg 11 ins per out", 2.7, "#b9b3a9", anchor="start", weight=500)
p.text(dx + dw - 3, dy + 4.6, "A 3  →  B 6", 3.0, ORANGE, anchor="end", weight=700)
p.text(dx + dw - 3, dy + 8.6, "mix 40%", 2.7, CYAN, anchor="end", weight=600)

# row 1
y1 = 42
# mode toggle
tx = 14
p.text(tx, y1 - 7.6, "MODE", CTL)
p.add(f'<rect x="{tx-2.2}" y="{y1-4.2}" width="4.4" height="8.4" rx="1.0" fill="#1b1b1d"/>')
p.add(f'<rect x="{tx-1.5}" y="{y1+0.4}" width="3.0" height="3.2" rx="0.7" fill="#e8e8e8"/>')
p.text(tx + 3.4, y1 - 1.6, "ON/OFF", 2.5, QUIET, anchor="start", weight=500)
p.text(tx + 3.4, y1 + 3.6, "0-100%", 2.5, INK, anchor="start", weight=700)
p.knob(42, y1, 6.0, "CHANCE", angle=-40)
p.jack(56, y1 + 1.5, "CV")
p.knob(74, y1 + 0.5, 4.2, "AMOUNT", angle=135)
p.button(92, y1, "RESEED", "o", r=3.6)
p.jack(105, y1 + 1.5, "TRIG")
p.knob(124, y1 + 0.5, 4.2, "SLEW", angle=-70)
p.jack(137, y1 + 1.5, "CV")
# auto gain toggle
gx = 158
p.text(gx, y1 - 7.6, "GAIN", CTL)
p.add(f'<rect x="{gx-2.2}" y="{y1-4.2}" width="4.4" height="8.4" rx="1.0" fill="#1b1b1d"/>')
p.add(f'<rect x="{gx-1.5}" y="{y1-3.6}" width="3.0" height="3.2" rx="0.7" fill="#e8e8e8"/>')
p.text(gx + 3.4, y1 - 1.6, "AUTO", 2.5, INK, anchor="start", weight=700)
p.text(gx + 3.4, y1 + 3.6, "SUM", 2.5, QUIET, anchor="start", weight=500)

p.rule(8, w - 8, 51.5, col="#cfc6b6", wdt=0.35)
# row 2: snapshots
y2 = 61
p.button(14, y2, "STORE", None, r=3.0)
p.text(54, 55.2, "SNAPSHOTS", CTL)
lit = {3: "o", 6: "c", 1: "w", 2: "w", 4: "w"}
for i in range(8):
    x = 25 + i * 8.6
    p.button(x, y2 + 1.5, None, lit.get(i + 1), r=2.6, square=True)
for i in range(8):
    p.text(25 + i * 8.6, y2 + 1.5 + 1.0, str(i + 1), 2.6, "#1d1e22" if (i + 1) in lit else "#bdb4a4", weight=700)
p.knob(104, y2 + 1, 4.2, "A", angle=-60, lab_fill=ACC)
p.knob(118, y2 + 1, 4.2, "B", angle=20, lab_fill="#1f8fa0")
p.knob(137, y2 + 1, 5.0, "MIX A-B", angle=-25)
p.jack(152, y2 + 1.5, "CV")
p.jack(166, y2 + 1.5, "A/B CV")

# jacks
py0 = 70.5
pitch = 9.6
cols = 8
ix0 = 9.5 + 4.6
ox0 = w / 2 + 4.0 + 4.6
p.text(ix0 + 3.5 * pitch, py0 - 0.6, "", CTL)
p.plate(w / 2 + 1.0, py0 - 2.5, w / 2 - 7.5, 124.0 - py0)
rows = [79, 90.5, 102, 113.5]
for r, y in enumerate(rows):
    for c in range(cols):
        n = r * cols + c + 1
        p.jack(ix0 + c * pitch, y, str(n))
        p.jack(ox0 + c * pitch, y, str(n), True)
p.text(9, py0 + 0.2, "IN", CTL, ACC, anchor="start", weight=700)
p.text(w / 2 + 3.5, py0 + 0.6, "OUT", CTL, ORANGE, anchor="start", weight=700)

svg = p.svg()
# Haer logo, bottom rail, cap 3.0 mm
logo = open(os.path.join(HERE, "..", "vcv-haer-logo", "Haer-ClimateCrisis-light.svg")).read()
paths = "".join(re.findall(r"<path[^>]*/>", logo))
s = 3.0 / 750
lx = w / 2 - (3070 * s) / 2
svg = svg.replace("</svg>", f'<rect x="{lx-1.5:.2f}" y="122.6" width="{3070*s+3:.2f}" height="4.6" fill="{BG}"/>'
                  f'<g transform="translate({lx - 20*s:.3f} 126.0) scale({s})">{paths}</g></svg>')
open(os.path.join(HERE, "DriftMatrix-mockup.svg"), "w").write(svg)
print(w)
