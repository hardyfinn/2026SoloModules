#!/usr/bin/env python3
# Panel preview: the three panel SVGs side by side with knobs, buttons and jacks drawn on, as PNG (needs playwright).
import os, sys, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "res"))
L = {"__file__": os.path.join(ROOT, "res", "make_panels.py")}
src = open(os.path.join(ROOT, "res", "make_panels.py")).read().split("fonts = {}")[0]
exec(src, L)
def comps(kind):
    c = []
    if kind == "Strata":
        cols, rows = L["S_COLS"], L["S_ROWS"]
        for r in range(2):
            for x in cols: c.append(("knob", x, rows[r]))
        c += [("knob", cols[0], rows[2]), ("large", cols[1], rows[2]), ("bezel", cols[2], rows[2]), ("bezel", cols[3], rows[2])]
        c += [("jack", x, L["Y_IN"]) for x in cols] + [("jack", x, L["Y_OUT"]) for x in cols]
        scr = ["[Fm9] Dbmaj7 Eb6/9 Bbm9", "DETROIT - F minor - Belleville", "bar 1/4 - offbeat - close", "DRIFT 1/4 chords moved"]
    else:
        cols, rows = L["T_COLS"], L["T_ROWS"]
        for r in range(2):
            for x in cols: c.append(("knob", x, rows[r]))
        c += [("large", cols[0], rows[2]), ("knob" if kind == "Undertow" else "bezel", cols[1], rows[2]), ("bezel", cols[2], rows[2])]
        c += [("jack", x, L["Y_IN"]) for x in cols] + [("jack", x, L["Y_OUT"]) for x in L["T_JCOLS"]]
        scr = ["ROLLING - Fm9 - F1", "chord tones - kick 4/4"] if kind == "Undertow" else ["LEAD - 16-step motif", "gen 7 - Ab5"]
    return c, scr
out = []
x0 = 0
for name in ["Strata", "Undertow", "Signal"]:
    svg = open(os.path.join(ROOT, "res", name + ".svg")).read()
    W = float(re.search(r'viewBox="0 0 ([\d.]+)', svg).group(1))
    inner = svg.split(">", 1)[1].rsplit("</svg>", 1)[0]
    g = [f'<g transform="translate({x0},0)">{inner}']
    cs, scr = comps(name)
    for k, x, y in cs:
        if k == "knob": g.append(f'<circle cx="{x}" cy="{y}" r="4.8" fill="#222"/><line x1="{x}" y1="{y}" x2="{x}" y2="{y-4}" stroke="#eee" stroke-width="0.6"/>')
        if k == "large": g.append(f'<circle cx="{x}" cy="{y}" r="6.1" fill="#222"/><line x1="{x}" y1="{y}" x2="{x}" y2="{y-5.2}" stroke="#eee" stroke-width="0.7"/>')
        if k == "bezel": g.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="#ccc" stroke="#555" stroke-width="0.4"/><circle cx="{x}" cy="{y}" r="2.4" fill="#eee"/>')
        if k == "jack": g.append(f'<circle cx="{x}" cy="{y}" r="3.9" fill="#aaa" stroke="#555" stroke-width="0.4"/><circle cx="{x}" cy="{y}" r="1.8" fill="#111"/>')
    for i, t in enumerate(scr):
        g.append(f'<text x="7" y="{14.5 + 4.6 + i * 4.6}" font-family="monospace" font-size="3.4" fill="#7EF0C0">{t}</text>')
    g.append(f'<circle cx="{W - 6.3}" cy="12.2" r="0.9" fill="#3CD08C"/>')
    g.append("</g>")
    out.append("".join(g)); x0 += W
svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{x0*6}" height="{128.5*6}" viewBox="0 0 {x0} 128.5">' + "".join(out) + "</svg>"
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "panel-preview.png")
html = os.path.join(HERE, "_preview.html"); open(html, "w").write(f'<html><body style="margin:0">{svg}</body></html>')
js = f"const {{chromium}}=require('playwright');(async()=>{{const b=await chromium.launch();const p=await b.newPage({{viewport:{{width:{int(x0*6)},height:{int(128.5*6)}}}}});await p.goto('file://{html}');await p.screenshot({{path:'{path}'}});await b.close()}})()"
subprocess.run(["node", "-e", js], check=True)
os.remove(html)
print("wrote", path)
