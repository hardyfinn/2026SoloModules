"""Panel mock-up for Changes (house style, 3.2 mm control / 2.9 mm jack labels).
Reuses the Panel class from ../vcv-long-form-mute-theory/make_panels.py."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "vcv-long-form-mute-theory"))
from make_panels import Panel, DISP, ORANGE, CYAN, INK, QUIET, ACC, H, CTL  # noqa: E402

def changes():
    p = Panel(28, "CHANGES", "chords the way the style plays them", "style chords  ·  voice-led  ·  HARM to Weighted  ·  Haer")
    w = p.w
    # display
    dx, dy, dw, dh = 8, 18.5, w - 16, 21
    p.add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="1.2" fill="{DISP}"/>')
    p.text(dx + 3, dy + 10.5, "Fm9", 9.0, "#f3efe8", anchor="start", weight=700, ls=0)
    p.text(dx + 31, dy + 6.2, "i9  ·  F minor", 3.0, "#b9b3a9", anchor="start", weight=500)
    p.text(dx + 31, dy + 10.4, "HOUSE", 3.0, ORANGE, anchor="start", weight=700)
    p.text(dx + dw - 3, dy + 6.2, "next", 2.6, "#8a857d", anchor="end", weight=500)
    p.text(dx + dw - 3, dy + 10.4, "Dbmaj9", 3.4, "#8a857d", anchor="end", weight=600)
    loop = ["Fm9", "Dbmaj9", "Ebsus2", "Cm7"]
    bw = (dw - 6) / 4
    for i, c in enumerate(loop):
        x = dx + 3 + i * bw
        cur = i == 0
        p.add(f'<rect x="{x+0.4:.2f}" y="{dy+13.5}" width="{bw-0.8:.2f}" height="5.6" rx="0.6" fill="{ORANGE if cur else "#2c2c2f"}"/>')
        if i == 2: p.add(f'<circle cx="{x+bw-2:.2f}" cy="{dy+14.8}" r="0.6" fill="#f0b030"/>')
        p.text(x + bw / 2, dy + 17.4, c, 2.9, "#1d1e22" if cur else "#d8d2c8", weight=700)
    # row 1: setting knobs
    y1 = 50
    for i, (nm, ang) in enumerate((("STYLE", 40), ("ROOT", -60), ("MODE", 120), ("RATE", -20), ("LOOP", 30))):
        p.knob(15 + i * (w - 30) / 4, y1, 4.2, nm, angle=ang)
    # row 2: big knobs, each with its CV jack beside it
    y2 = 68
    pair = (w - 16) / 4
    xs = [8 + pair * i + pair / 2 - 5 for i in range(4)]
    for x, (nm, ang) in zip(xs, (("FOCUS", 0), ("COLOUR", 60), ("SPREAD", -40), ("DRIFT", -90))):
        p.knob(x, y2, 6.0, nm, angle=ang)
        p.jack(x + 13, y2 + 1.5, "CV")
    # row 3: buttons + tension meter
    y3 = 87
    bx = [15 + i * (w - 30) / 4 for i in range(5)]
    for x, nm in ((bx[0], "HOLD"), (bx[1], "NEW"), (bx[2], "LOCK")):
        p.button(x, y3, nm, None, r=3.0)
    tx = (bx[3] + bx[4]) / 2
    p.text(tx, y3 - 4.8, "TENSION", 2.9, QUIET)
    for k in range(6):
        p.led(tx - 6.25 + k * 2.5, y3, ORANGE if k < 2 else "#3a3a3a", r=0.95)
    # inputs on the panel, outputs on the plate
    yi = 103
    ins = ["CLOCK", "RESET", "CHANGE", "HOLD", "TRANSP", "LINK"]
    for i, nm in enumerate(ins):
        p.jack(14 + i * (w - 28) / 5, yi, nm, lab_fill="#1f8fa0" if nm == "LINK" else None)
    py0 = 108.6
    p.plate(6.5, py0, w - 13, 122.3 - py0)
    outs = ["CHORD", "BASS", "ROOT", "TOP", "GATE", "CHANGE", "TENSION", "HARM"]
    for i, nm in enumerate(outs):
        p.jack(14 + i * (w - 28) / 7, py0 + 8.9, nm, True, lab_fill=CYAN if nm == "HARM" else None)
    return p, outs, py0

if __name__ == "__main__":
    p, outs, py0 = changes()
    # output labels sit beside the plate's jacks: drawn small above them inside the plate
    svg = p.svg()
    open(os.path.join(HERE, "Changes.svg"), "w").write(svg)
