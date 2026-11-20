#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess, math
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "Ectoplasm.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W, H = 121.92, 128.5
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")): comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

def led(x, y, col, r=0.75):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{col}"/>')

knob("RoundHugeBlackKnob", L["BREAK_X"], L["BREAK_Y"], 0.6)
cols = ["#e8a33a", "#e86a3a", "#d94c7a", "#7fe0c4"]
for i in range(16):
    a = -2.356 + i * 4.712 / 15
    led(L["BREAK_X"] + math.sin(a) * L["RING_R"], L["BREAK_Y"] - math.cos(a) * L["RING_R"], cols[i % 4] if i < 10 else "#333a55", 1.4)
knob("RoundLargeBlackKnob", L["GRIM_X"], L["TOP_Y"], 0.45)
knob("RoundLargeBlackKnob", L["AMEN_X"], L["TOP_Y"], 0.25)
comp("VCVButton_0", L["BTN_X"][0], L["BTN_Y"]); comp("VCVButton_0", L["BTN_X"][1], L["BTN_Y"])
comp("VCVBezel", L["BTN_X"][2], L["BTN_Y"]); comp("VCVBezel", L["BTN_X"][3], L["BTN_Y"])
led(L["BTN_X"][2], L["BTN_Y"], "#f0d040", 2.0); led(L["BTN_X"][3], L["BTN_Y"], "#50d070", 2.0)
for i in range(4): led(4.6, L["BTN_Y"] - 3 + i * 2, "#50d070" if i == 0 else "#2a3050", 0.5)
for x, v in zip(L["ROW2_X"], [0.1, 0.0, 0.5, 0.33]): knob("RoundBlackKnob", x, L["ROW2_Y"], v)
for x, v in zip(L["LAYER_X"], [0.5, 0.5, 0.0, 0.2, 0.5]): knob("RoundSmallBlackKnob", x, L["TAP_Y"], v)
for x, v in zip(L["LAYER_X"], [0.0, 0.3, 0.0, 0.1, 0.0]): knob("RoundSmallBlackKnob", x, L["MODE_Y"], v)
for i in range(9): comp("PJ301M", L["JACK_X0"] + i * L["JACK_DX"], L["JACK_Y1"])
for i in range(9): comp("PJ301M", L["JACK_X0"] + i * L["JACK_DX"], L["JACK_Y2"])
for x in (7.5, W - 7.5):
    for y in (1.5, H - 1.5): comp("ScrewSilver", x, y)
dx, dy = L["DISP_X"], L["DISP_Y"]
g.append(f'<text x="{dx + 2}" y="{dy + 5.0}" font-family="Share Tech Mono, DejaVu Sans Mono, monospace" font-size="4.2" fill="#7fe0c4">B01 S03  SL 05  172 BPM  RUNE 4  KICK</text>')
g.append(f'<text x="{dx + 2}" y="{dy + 8.9}" font-family="Share Tech Mono, DejaVu Sans Mono, monospace" font-size="3.3" fill="#6f9e92">amen break 172bpm</text>')
body = re.sub(r'^<svg[^>]*>', '', svg.strip()).replace('</svg>', '')
page = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*6}" height="{H*6}">{body}{"".join(g)}</svg>'
html = os.path.join(os.path.dirname(OUT), "preview.html")
open(html, "w").write(f'<html><body style="margin:0;background:#777">{page}</body></html>')
js = "const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d},deviceScaleFactor:2});await p.goto('file://%s');await p.screenshot({path:'%s'});await b.close();})();" % (int(W*6), int(H*6), html, OUT)
subprocess.run(["node", "-e", js], check=True)
print(OUT)
