#!/usr/bin/env python3
# Picture of both panels with Rack's component graphics and sample screens.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
PX = 25.4 / 75.0
mono = "font-family:'DejaVu Sans Mono',monospace"


def layout(tag):
    part = src.split(tag)[1].split("# ---- LAYOUT")[1]
    code = part.split("\nBG")[0] if "PEAL" in tag else part.split("\nFBG")[0]
    L = {}
    exec(code.split("\n", 1)[1], L)
    return L


def comp(g, name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')


def knob(g, base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(g, base + "_bg", x, y)
    comp(g, base, x, y, -135 + 270 * v)


def body(path):
    s = open(path).read()
    return re.search(r'<svg[^>]*>(.*)</svg>', s, re.S).group(1)


# ---- Peal
L = layout("PEAL (36 HP)")
g = [body(os.path.join(HERE, "..", "res", "Peal.svg"))]
for x, v in zip(L["KNOB_X"], [0.5, 1 / 6, 0.25, 0.25, 0.5, 0.0]):
    knob(g, "RoundLargeBlackKnob", x, L["KNOB_Y"], v)
for x in L["BTN_X"]:
    for y in L["BTN_Y"]:
        comp(g, "VCVBezel", x, y)
jx = lambda i: L["J_X0"] + L["J_DX"] * i
for i in range(12):
    for y in (L["ROWA_Y"], L["ROWB_Y"], L["ROWC_Y"]):
        comp(g, "PJ301M", jx(i), y)
# sample screen
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
rows = ["12345678", "21436587", "24163857", "42618375", "46281735", "64827153", "68472513", "86745231"]
gx, gy, cw = dx + 2.5, dy + 3.0, 4.4
rh = (dh - 3.0 - 1.4) / 8
for bell, col in (("2", "#6EAAFF"), ("1", "#FF5A50")):
    pts = " ".join(f"{gx + cw * (r.index(bell) + 0.5):.2f},{gy + rh * (i + 0.5):.2f}" for i, r in enumerate(rows))
    g.append(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-opacity="0.7" stroke-width="0.5"/>')
for i, r in enumerate(rows):
    for k, c in enumerate(r):
        col = "#FF5A50" if c == "1" else ("#F0E6D2" if i == 7 and k < 5 else "rgba(232,178,98,0.45)")
        if i == 7 and k == 4:
            g.append(f'<rect x="{gx + cw * k + cw * 0.05}" y="{gy + rh * i}" width="{cw * 0.9}" height="{rh}" rx="0.6" fill="rgba(232,178,98,0.5)"/>')
        g.append(f'<text x="{gx + cw * (k + 0.5)}" y="{gy + rh * (i + 0.5) + 1.1}" font-size="3.4" text-anchor="middle" fill="{col}" style="{mono}">{c}</text>')
tx = gx + cw * 12 + 5.5
g.append(f'<text x="{tx}" y="{gy + 3.4}" font-size="4.0" fill="#E8B262" style="{mono}">PLAIN BOB</text>')
g.append(f'<text x="{tx}" y="{gy + 8.4}" font-size="3.2" fill="#F0E6D2" style="{mono}">MAJOR</text>')
g.append(f'<text x="{tx}" y="{gy + 13.6}" font-size="2.6" fill="rgba(232,178,98,0.6)" style="{mono}">LEAD 2  8/16</text>')
g.append(f'<text x="{tx}" y="{gy + 17.4}" font-size="2.6" fill="rgba(232,178,98,0.6)" style="{mono}">GAP 1  MAJOR</text>')
g.append(f'<text x="{tx}" y="{dy + dh - 2.0}" font-size="4.6" fill="#FF5A50" style="{mono}">BOB!</text>')
W1 = 182.88
peal = "\n".join(g)

# ---- Bell Foundry
F = layout("BELL FOUNDRY (20 HP)")
g = [body(os.path.join(HERE, "..", "res", "BellFoundry.svg"))]
comp(g, "CKSSThree_2", F["F_SW_X"][0], F["F_SW_Y"]); comp(g, "CKSSThree_1", F["F_SW_X"][1], F["F_SW_Y"])
comp(g, "VCVBezel", F["F_RECAST_X"], F["F_SW_Y"])
for x, v in zip(F["F_KX"], [0.0, 0.5, 0.5, 0.5]): knob(g, "RoundBlackKnob", x, F["F_K1_Y"], v)
for x, v in zip(F["F_KX"], [0.1, 0.55, 0.3, 0.7]): knob(g, "RoundBlackKnob", x, F["F_K2_Y"], v)
fj = lambda i: F["F_J_X0"] + F["F_J_DX"] * i
for i in range(6): comp(g, "PJ301M", fj(i), F["F_CV_Y"])
for i in (0, 1, 2, 4, 5): comp(g, "PJ301M", fj(i), F["F_IO_Y"])
dx, dy, dw, dh = F["F_DISP_X"], F["F_DISP_Y"], F["F_DISP_W"], F["F_DISP_H"]
sw = dw * 0.62
xo = lambda r: dx + 1.5 + (sw - 3) * math.log2(r / 0.4) / math.log2(10 / 0.4)
for r, l in zip([0.5, 1.0, 1.2, 1.5, 2.0, 2.5, 3.0, 4.0], [0.45, 0.40, 0.45, 0.20, 0.90, 0.35, 0.30, 0.22]):
    y0 = dy + dh - 1.5; y1 = y0 - (dh - 3) * l / 1.2
    g.append(f'<line x1="{xo(r)}" y1="{y0}" x2="{xo(r)}" y2="{y1}" stroke="{"#F0E6D2" if r == 1.0 else "#E8B262"}" stroke-width="0.7"/>')
bx0 = dx + sw + 1.5; bw = (dw - sw - 3) / 12
for i, v in enumerate([0.9, 0.2, 0.5, 0.1, 0.7, 0.3, 0.05, 0.4, 0, 0, 0, 0]):
    hh = (dh - 3) * v
    g.append(f'<rect x="{dx + 0 + bx0 - dx + bw * i + bw * 0.15}" y="{dy + dh - 1.5 - hh}" width="{bw * 0.7}" height="{hh}" fill="{"#FF5A50" if i == 0 else "#E8B262"}"/>')
foundry = "\n".join(g)

W, H = W1 + 121.92 + 4, 128.5
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">'
       f'<rect width="{W}" height="{H}" fill="#111"/>{peal}<g transform="translate({W1 + 4},0)">{foundry}</g></svg>')
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(W * 5), int(H * 5)
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
