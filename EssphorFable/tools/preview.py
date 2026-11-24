#!/usr/bin/env python3
# Picture of the three panels side by side, with simple drawn knobs and jacks (no Rack graphics needed).
#   python3 tools/preview.py docs/panel-preview.png      (needs node + playwright)
import os, sys, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "panel-preview.png")
L = json.load(open(os.path.join(HERE, "layout.json")))
R = {"knob": 4.2, "big": 5.6, "small": 3.2, "snap": 4.2, "bezel": 3.0, "jack": 3.1, "out": 3.1, "light": 1.0, "button": 2.2}
TEXT = {"RATE_MODE_DISP": "1/16", "SCAN_MODE_DISP": "STRETCH", "SPRAY_MODE_DISP": "RANDOM", "WET_MODE_DISP": "MIX", "FB_MODE_DISP": "GRAIN",
        "RP_MODE_DISP": "DETUNE", "SCALE_DISP": "CHROMATIC", "PATTERN_DISP": "PAT 3F2A", "DECAY_MODE_DISP": "SUSTAIN",
        "W_RATE_MODE_DISP": "FREE", "W_SHAPE_DISP": "SIN", "R_RATE_MODE_DISP": "STRAIGHT"}
GAP = 4.0
x0 = 0.0
svgs = []
for mod in ("Essphor", "EssphorRes", "EssphorMod"):
    m = L[mod]
    W, H = m["W"], m["H"]
    panel = open(os.path.join(HERE, "..", "res", mod + ".svg")).read()
    inner = panel[panel.index(">", panel.index("<svg")) + 1: panel.rindex("</svg>")]
    g = [inner]
    for name, (kind, x, y, label) in m["comps"].items():
        if kind == "display":
            w, h = m["disp"][name]
            t = TEXT.get(name, "")
            if name == "SCALE_DISP" and mod == "EssphorRes": t = "CHROMATIC 48"
            if name == "BUF_DISP":
                import math
                bars = "".join(f'<rect x="{x - w / 2 + 1 + i * (w - 2) / 80:.2f}" y="{y - abs(math.sin(i * 0.31) * math.sin(i * 0.07)) * 2.4 - 0.2:.2f}" width="0.35" height="{abs(math.sin(i * 0.31) * math.sin(i * 0.07)) * 4.8 + 0.4:.2f}" fill="#f2c060" opacity="0.6"/>' for i in range(80))
                g.append(bars + f'<line x1="{x - 8}" y1="{y - h / 2 + 0.5}" x2="{x - 8}" y2="{y + h / 2 - 0.5}" stroke="#ec5a46" stroke-width="0.3"/>')
                continue
            g.append(f'<text x="{x}" y="{y + 1.0}" font-family="monospace" font-size="{min(2.9, (w - 1.2) / (0.62 * max(1, len(t)))):.2f}" fill="#f2c060" text-anchor="middle">{t}</text>')
            continue
        r = R[kind]
        if kind in ("knob", "big", "small", "snap"):
            g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#2b2b2b" stroke="#111" stroke-width="0.3"/>'
                     f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - r * 0.85}" stroke="#eee" stroke-width="0.5"/>')
        elif kind in ("jack", "out"):
            g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#c9c9c9" stroke="#777" stroke-width="0.3"/><circle cx="{x}" cy="{y}" r="{r * 0.5}" fill="#111"/>')
        elif kind == "bezel":
            g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#3a3a3a" stroke="#999" stroke-width="0.4"/>')
        elif kind == "button":
            g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#ddd" stroke="#666" stroke-width="0.3"/>')
        elif kind == "light":
            g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#333"/>')
    svgs.append(f'<g transform="translate({x0} 0)">' + "".join(g) + "</g>")
    x0 += W + GAP
TW, TH = x0 - GAP, 128.5
svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {TW} {TH}" width="{TW * 5}" height="{TH * 5}"><rect width="{TW}" height="{TH}" fill="#111"/>' + "".join(svgs) + "</svg>"
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = int(TW * 5), int(TH * 5)
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}'); await p.waitForTimeout(300); await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp)
print("wrote", OUT)
