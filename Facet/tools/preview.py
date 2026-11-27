#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Facet.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 284.48, 128.5
PX = 25.4 / 75.0
g = []


def comp(name, x, y, angle=0.0, dy=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2 + dy}" width="{w}" height="{h}"/></g>')


def knob(x, y, v, name="RoundBlackKnob"):
    if os.path.exists(os.path.join(LIB, name + "_bg.svg")):
        comp(name + "_bg", x, y)
    comp(name, x, y, -135 + 270 * v)


def lit(x, y, col="#ffcc00", r=1.6):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}" fill-opacity="0.85"/>')


def slider(x, v):
    comp("VCVSlider", x, L["SL_Y"])
    travel = 25.9 - 4.0 - 2.0
    comp("VCVSliderHandle", x, L["SL_Y"] + travel / 2 - travel * v)


TC = ["#E0533D", "#F3B04A", "#4FB3A9", "#7B8CDE"]
for t, x in enumerate(L["TSEL_X"]):
    comp("VCVBezel", x, L["TSEL_Y"]); lit(x, L["TSEL_Y"], TC[t], 2.3 if t == 0 else 1.0)
    comp("VCVButton_0", x, L["TON_Y"]); lit(x, L["TON_Y"]) if t < 3 else None
comp("VCVButton_0", L["LINK_X"], L["LINKSOLO_Y"]); comp("VCVButton_0", L["SOLO_X"], L["LINKSOLO_Y"])
for x in (L["PREV_X"], L["NEXT_X"], L["PAGE_X"]): comp("VCVButton_0", x, L["NAV_Y"])
knob(L["DATA_X"], L["NAV_Y"], 0.5, "RoundSmallBlackKnob")
for x in L["PRESET_X"]: comp("VCVButton_0", x, L["MEM_Y"])
for m in range(8):
    x = L["MEM_X0"] + m * L["MEM_DX"]; comp("VCVBezel", x, L["MEM_Y"])
    if m == 0: lit(x, L["MEM_Y"], r=2.3)
comp("VCVBezel", L["SAVE_X"], L["MEM_Y"])
knob(L["RAMT_X"], L["MOV_Y"], 0.25); comp("VCVBezel", L["RAND_X"], L["MOV_Y"]); knob(L["MUT_X"], L["MOV_Y"], 0.0)
comp("VCVButton_0", L["LOCK_X"], L["MOV_Y"]); comp("VCVButton_0", L["SEL_X"], L["MOV_Y"])
knob(L["WAVE_X"], L["WAVE_Y"], 0.3)
for x, v in [("COARSE_X", 0.5), ("FINE_X", 0.5), ("CUT_X", 0.62), ("RES_X", 0.2), ("FENV_X", 0.55), ("LEV_X", 0.8), ("VELO_X", 0.6)]:
    slider(L[x], v)
for k, v in enumerate([0.2, 0.5, 0.4, 0.7, 0.45]): slider(L["ENV_X"][k], v)
for k in range(3):
    comp("VCVButton_0", L["FM_X"], L["FM_Y"][k]); comp("VCVButton_0", L["ES_X"], L["FM_Y"][k])
lit(L["FM_X"], L["FM_Y"][0]); lit(L["ES_X"], L["FM_Y"][2])
for k in range(2): comp("VCVButton_0", L["LS_X"][k], L["LS_Y"])
lit(L["LS_X"][0], L["LS_Y"])
knob(L["LRATE_X"], L["LRATE_Y"], 0.4)
for k in range(3): knob(L["LDEPTH_X"], L["LDEPTH_Y"][k], 0.5 + 0.1 * k, "RoundSmallBlackKnob")
for k in range(7):
    x0 = L["FX_X0"] + k * L["FX_DX"]
    comp("VCVButton_0", x0 + L["FX_LATCH_DX"], L["FX_Y"])
    if k in (4, 6): lit(x0 + L["FX_LATCH_DX"], L["FX_Y"])
    knob(x0 + L["FX_KNOB_DX"], L["FX_Y"], 0.4, "RoundSmallBlackKnob")
for i in range(9): comp("PJ301M", L["JACK_X0"] + i * L["JACK_DX"], L["JACK_Y"])
for k in range(2): comp("VCVButton_0", L["JACK_X0"] + (6 + k) * L["JACK_DX"], L["LEARN_Y"])
comp("VCVButton_0", L["DAC_X"], L["DAC_Y"])
comp("PJ301M", L["OUTL_X"], L["JACK_Y"]); comp("PJ301M", L["OUTR_X"], L["JACK_Y"])
knob(L["OLEV_X"], L["JACK_Y"], 0.63, "RoundSmallBlackKnob")
sx, sy, sw, sh = L["SCR_X"], L["SCR_Y"], L["SCR_W"], L["SCR_H"]
g.append(f'<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="1" fill="#10201c"/>')
lines = ["P01 GLASS CATHEDRAL          TONE A", "TVF ENV  (page 4/18)",
         "&gt;VELO      +40     T3       60", " T-VELO    +12     SUS      72",
         " T-KF        0     T4       55", " T1        18     L4        0", " L1       100", "RAND 25%  SEL ALL   MUTATE 0"]
for i, t in enumerate(lines):
    g.append(f'<text x="{sx + 2}" y="{sy + 4.2 + i * 3.55}" font-family="monospace" font-size="2.7" fill="#9ff0de" xml:space="preserve">{t}</text>')
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
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
