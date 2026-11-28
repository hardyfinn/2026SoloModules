import subprocess, os
C = 13.0   # cell size
COL = {"act": "#6FE08A", "muted": "#5A4E33", "actm": "#6E6672", "ch": "#E6B450", "fn": "#B9A9C9", "time": "#7FD1C1", "scene": "#E08A6B", "edit": "#8FB3E8", "off": "#2A262D"}
def layout_a():
    k = {}
    for c in range(4):
        for r in range(6):
            k[(c, r)] = ("ch", str(c * 6 + r + 1))
    for r, s in enumerate(["CUE", "LOCK", "SOLO", "A/B", "PAIR", "INV"]):
        k[(5, r)] = ("fn", s)
    for r, s in enumerate(["ALL+", "ALL-", "CLR", "SAVE", "DEL", "COPY"]):
        k[(6, r)] = ("edit", s)
    for i in range(16):
        k[(12 + i % 4, i // 4)] = ("scene", "S%d" % (i + 1))
    for i, s in enumerate(["NOW", "BEAT", "BAR", "2BAR"]):
        k[(12 + i, 5)] = ("time", s)
    return k
import random
def layout_a2():
    k = layout_a()
    random.seed(4)
    muted = {3, 8, 15, 20, 22}
    for c in range(4):
        for r in range(6):
            ch = c * 6 + r + 1
            if ch in muted: k[(c, r)] = ("muted", str(ch))
            lvl = random.choice([0, 0.2, 0.45, 0.7, 1.0, 1.0])
            if ch in muted: k[(7 + c, r)] = ("actm", "", 0.25 + 0.5 * lvl)
            else: k[(7 + c, r)] = ("act", "", 0.08 + 0.92 * lvl)
    return k
def layout_b():
    k = {}
    for i in range(16):
        k[(i % 8, i // 8)] = ("scene", "S%d" % (i + 1))
    for i, s in enumerate(["NOW", "BEAT", "BAR", "2BAR"]):
        k[(12 + i, 0)] = ("time", s)
    for c in range(4):
        for r in range(6):
            k[(c, r + 2)] = ("ch", str(c * 6 + r + 1))
    fns = [("fn", "CUE"), ("fn", "LOCK"), ("fn", "SOLO"), ("fn", "A/B"),
           ("fn", "PAIR"), ("fn", "INV"), ("edit", "ALL+"), ("edit", "ALL-"),
           ("edit", "CLR"), ("edit", "SAVE"), ("edit", "DEL"), ("edit", "COPY")]
    for i, f in enumerate(fns):
        k[(12 + i % 4, 5 + i // 4)] = f
    return k
def svg(keys, title, sub, legend):
    W = 16 * C + 30; H = 8 * C + 90
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W*4}" height="{H*4}">',
         f'<rect width="{W}" height="{H}" fill="#1B181D"/>',
         f'<text x="15" y="12" font-family="Inter,sans-serif" font-weight="700" font-size="7" fill="#EEE8EF">{title}</text>',
         f'<text x="15" y="20" font-family="Inter,sans-serif" font-size="4" fill="#9A919E">{sub}</text>']
    for c in range(16):
        o.append(f'<text x="{15 + c*C + C/2}" y="28" font-family="Inter,sans-serif" font-size="3" text-anchor="middle" fill="#6E6672">{c+1}</text>')
    for r in range(8):
        o.append(f'<text x="10" y="{32 + r*C + C/2 + 1}" font-family="Inter,sans-serif" font-size="3" text-anchor="middle" fill="#6E6672">{r+1}</text>')
        for c in range(16):
            x = 15 + c * C; y = 32 + r * C
            v = keys.get((c, r), ("off", ""))
            kind, lab = v[0], v[1]
            col = COL[kind]
            op = v[2] if len(v) > 2 else (0.9 if kind != "off" else 1)
            if kind in ("act", "actm"):
                o.append(f'<rect x="{x+0.8}" y="{y+0.8}" width="{C-1.6}" height="{C-1.6}" rx="1.6" fill="#2A262D"/>')
            o.append(f'<rect x="{x+0.8}" y="{y+0.8}" width="{C-1.6}" height="{C-1.6}" rx="1.6" fill="{col}" fill-opacity="{op}"/>')
            if lab:
                fs = 3.2 if len(lab) > 3 else 3.8
                o.append(f'<text x="{x + C/2}" y="{y + C/2 + 1.3}" font-family="Inter,sans-serif" font-weight="700" font-size="{fs}" text-anchor="middle" fill="{'#EEE8EF' if kind == 'muted' else '#141215'}">{lab}</text>')
    ly = 32 + 8 * C + 10
    lx = 15
    for kind, name in legend:
        o.append(f'<rect x="{lx}" y="{ly-3.6}" width="4.5" height="4.5" rx="0.8" fill="{COL[kind]}"/>')
        o.append(f'<text x="{lx+6.5}" y="{ly}" font-family="Inter,sans-serif" font-size="3.8" fill="#EEE8EF">{name}</text>')
        lx += 6.5 + len(name) * 2.1 + 8
    o.append(f'<text x="15" y="{ly+11}" font-family="Inter,sans-serif" font-size="3.4" fill="#9A919E">ALL+ / ALL- = all channels on / off · CLR = forget cued changes · SAVE, DEL, COPY: tap the key, then tap a scene</text>')
    o.append('</svg>')
    return "\n".join(o)
legend = [("ch", "channels 1-24"), ("fn", "performance"), ("edit", "all / scene editing"), ("scene", "16 scenes"), ("time", "timing")]
out = os.path.dirname(os.path.abspath(__file__))
legend2 = [("ch", "channel on"), ("muted", "channel muted"), ("act", "signal level"), ("actm", "signal, muted channel")]
for name, keys, title, sub in [
    ("A2", layout_a2(), "Layout A with signal activity", "Columns 8-11 mirror the channel block: each key glows with that channel's level, grey when the channel is muted"),
]:
    legend_use = legend2
    p = os.path.join(out, f"GridMutes-layout-{name}.svg")
    open(p, "w").write(svg(keys, title, sub, legend_use))
    html = p + ".html"
    open(html, "w").write(f'<html><body style="margin:0;background:#1B181D"><img src="{os.path.basename(p)}" style="width:100%"></body></html>')
    js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const pg = await b.newPage({{viewport: {{width: 950, height: 820}}, deviceScaleFactor: 2}});
await pg.goto('file://{html}'); await pg.waitForTimeout(300);
const el = await pg.$('img'); await el.screenshot({{path: '{out}/GridMutes-layout-{name}.png'}}); await b.close(); }})();"""
    open(p + ".js", "w").write(js)
    subprocess.run(["node", p + ".js"], check=True, cwd="/tmp")
for name, keys, title, sub in [
    ("A", layout_a(), "Layout A: channels left, scenes as a 4x4 block", "Channels in columns 1-4, function keys in columns 6-7, scenes and timing on the right"),
    ("B", layout_b(), "Layout B: scenes across the top", "Scenes on rows 1-2, channels below them, timing top right, function keys bottom right")]:
    p = os.path.join(out, f"GridMutes-layout-{name}.svg")
    open(p, "w").write(svg(keys, title, sub, legend))
    html = p + ".html"
    open(html, "w").write(f'<html><body style="margin:0;background:#1B181D"><img src="{os.path.basename(p)}" style="width:100%"></body></html>')
    js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const pg = await b.newPage({{viewport: {{width: 950, height: 820}}, deviceScaleFactor: 2}});
await pg.goto('file://{html}'); await pg.waitForTimeout(300);
const el = await pg.$('img'); await el.screenshot({{path: '{out}/GridMutes-layout-{name}.png'}}); await b.close(); }})();"""
    open(p + ".js", "w").write(js)
    subprocess.run(["node", p + ".js"], check=True, cwd="/tmp")
print("ok")
