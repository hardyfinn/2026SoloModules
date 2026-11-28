#!/usr/bin/env python3
# Three knob-panel alternatives for the 808 Circuit Bench (no screen).
#   python3 make_knob_mockups.py bench2.json OUTDIR CODE
# Knob names/values that change with the selected voice are drawn live by the module.
import os, sys, re, base64, json, subprocess, tempfile, math
sys.path.insert(0, "/home/user/2026solomodules/Circuit808/res"); import haer_logo
LIB = os.path.expanduser("~/Rack-src/res/ComponentLibrary")
data = json.load(open(sys.argv[1])); OUT = sys.argv[2]; WANT = sys.argv[3]
V = next(i for i, v in enumerate(data) if v["code"] == WANT); VO = data[V]
BG, LINE, INK, SOFT, ORA, AMB, RED = "#1F2023", "#3A3B40", "#ECE6D8", "#9A968C", "#F08A24", "#F3B04A", "#E0533D"
PX = 25.4 / 75.0
g = []
def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    d = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{d}" x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}"/></g>')
    return w / 2
def t(s, x, y, size, fill=INK, anchor="middle", font="Inter", weight=700, ls=0.0):
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    g.append(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" font-weight="{weight}" letter-spacing="{ls}" text-anchor="{anchor}" fill="{fill}">{s}</text>')
def section(name, x0, x1, yy):
    t(name, x0, yy, 3.0, fill=ORA, anchor="start", ls=0.35)
    w = len(name) * 2.45 + 2.5
    g.append(f'<line x1="{x0+w}" y1="{yy-1}" x2="{x1}" y2="{yy-1}" stroke="{LINE}" stroke-width="0.35"/>')
def knob(x, y, k, name=None, val=None, big=True, lab_dy=9.4, dot=True):
    ang = -149.4 + 298.8 * k
    r = comp("RoundBlackKnob" if big else "RoundSmallBlackKnob", x, y, ang)
    if dot:
        a = math.radians(ang - 90); rr = r + 0.9
        g.append(f'<circle cx="{x+rr*math.cos(a):.2f}" cy="{y+rr*math.sin(a):.2f}" r="0.55" fill="{RED}"/>')
    if name: t(name, x, y + lab_dy, 3.2)
    if val: t(val, x, y + lab_dy + 4.0, 2.9, fill=AMB, font="ShareTech", weight=400)
def blank(x, y):
    comp("RoundBlackKnob", x, y, 0); g.append(f'<circle cx="{x}" cy="{y}" r="4.9" fill="{BG}" opacity="0.65"/>')
def btn(x, y, lab, w=13.0, fill="#3A3B40", ink=INK, h=5.2, size=3.0):
    g.append(f'<rect x="{x-w/2}" y="{y-h/2}" width="{w}" height="{h}" rx="1" fill="{fill}"/>')
    t(lab, x, y + size * 0.36, size, fill=ink)
def leds(x0, y, labels, on=0, dx=6.0):
    for i, L in enumerate(labels):
        x = x0 + i * dx
        g.append(f'<circle cx="{x}" cy="{y}" r="1.3" fill="{AMB if i == on else "#3A3020"}"/>')
        t(L, x, y + 5.0, 2.6, fill=SOFT, weight=600)
def voicebtn(x, y, d, w, sel):
    g.append(f'<rect x="{x-w/2}" y="{y-2.4}" width="{w}" height="4.8" rx="1" fill="{d["colour"]}" opacity="{1 if sel else 0.8}"/>')
    if sel: g.append(f'<rect x="{x-w/2-0.55}" y="{y-2.95}" width="{w+1.1}" height="5.9" rx="1.4" fill="none" stroke="{INK}" stroke-width="0.45"/>')
    t(d["code"], x, y + 1.15, 3.2, fill="#1B1B1B")
def outbox(x, y): g.append(f'<rect x="{x-4.2}" y="{y-5.4}" width="8.4" height="10.4" rx="1.2" fill="#4A4B50"/>')
def mixbox(x, y, lab):
    g.append(f'<rect x="{x-4.6}" y="{y-8.6}" width="9.2" height="13.6" rx="1.2" fill="{ORA}"/>')
    t(lab, x, y - 5.0, 2.6, fill="#1B1B1B"); comp("PJ301M", x, y)
def params(v):
    return [(b["name"], p) for b in sorted(v["blocks"], key=lambda b: (b["col"], b["row"])) for p in b["p"]]
def grid(slots, x0, x1, ys, cols, groups=True, values=True, lab_dy=9.4):
    pit = (x1 - x0) / cols
    for r, ky in enumerate(ys):
        row = slots[r*cols:(r+1)*cols]
        if groups:
            i = 0
            while i < len(row):
                j = i
                while j + 1 < len(row) and row[j+1][0] == row[i][0]: j += 1
                xa, xb, hy = x0 + pit*i + 1.2, x0 + pit*(j+1) - 1.2, ky - 10.0
                g.append(f'<path d="M{xa} {hy+1.6} V{hy} H{xb} V{hy+1.6}" fill="none" stroke="#5A5B60" stroke-width="0.35"/>')
                nm = row[i][0]; tw = len(nm) * 2.0 + 2.4
                g.append(f'<rect x="{(xa+xb)/2-tw/2}" y="{hy-1.8}" width="{tw}" height="3.6" fill="{BG}"/>')
                t(nm, (xa+xb)/2, hy+1.05, 2.9, fill=SOFT, weight=600, ls=0.15)
                i = j + 1
        for c in range(cols):
            x = x0 + pit*(c+0.5)
            if r*cols + c >= len(slots): blank(x, ky); continue
            _, (name, val, k) = slots[r*cols + c]
            knob(x, ky, k, name, val if values else None, lab_dy=lab_dy)
def logo(W): g.append(haer_logo.logo_group(W, BG, y=125.9, cap=2.1))

# ---------------- A: FOLLOW 16 (36 HP) ----------------
def panel_A():
    W = 182.88
    g.append(f'<rect width="{W}" height="128.5" fill="{BG}"/>')
    t("808 CIRCUIT BENCH", 8, 11.5, 5.4, anchor="start", ls=0.4)
    t(VO["code"] + "  " + VO["name"], W - 8, 11.3, 4.4, fill=VO["colour"], anchor="end", weight=800)
    grid(params(VO), 8.0, W - 8.0, [29.0, 58.5], 8)
    BY = 79.0
    btn(16, BY, "STOCK"); btn(32, BY, "COPY"); btn(48, BY, "SHIFT"); btn(66, BY, "KITS")
    leds(77, BY - 0.2, "ABCD")
    btn(112, BY, "RAND", fill="#5A3A1A", ink=AMB)
    t("SHIFT + voice = hit  |  + STOCK = whole voice", W - 8, BY - 1.6, 2.3, fill=SOFT, anchor="end", weight=500)
    t("SHIFT + KITS = stock kit  |  + RAND = wild", W - 8, BY + 2.0, 2.3, fill=SOFT, anchor="end", weight=500)
    MIXW = 26.0; VP = (W - 12.0 - MIXW - 4.0) / 13.0
    VY, TY, OY = 92.4, 102.4, 115.4
    section("VOICES", 4.0, W - MIXW - 8.0, 87.4); section("MIX", W - MIXW - 4.0, W - 4.0, 87.4)
    for i, d in enumerate(data):
        x = 8.0 + VP*(i+0.5)
        voicebtn(x, VY, d, VP - 1.6, i == V); outbox(x, OY); comp("PJ301M", x, TY); comp("PJ301M", x, OY)
    t("TRIG", 3.4, TY + 0.8, 2.2, fill=SOFT, anchor="start", weight=500)
    t("OUT", 3.4, OY + 0.8, 2.2, fill=SOFT, anchor="start", weight=500)
    MX = [W - MIXW - 4.0 + 7.0, W - 10.0]
    for x, lab in zip(MX, ["ACC", "POLY"]): t(lab, x, VY + 1.1, 2.9); comp("PJ301M", x, TY)
    for x, lab in zip(MX, ["L", "R"]): mixbox(x, OY, lab)
    logo(W); return W

# ---------------- B: 808 FRONT + CIRCUIT (40 HP) ----------------
FRONT = {"BD": ["TONE", "DECAY"], "SD": ["TONE", "SNAPPY"], "LT": ["TUNE"], "MT": ["TUNE"], "HT": ["TUNE"],
         "CY": ["TONE", "DECAY"], "OH": ["DECAY"]}
def panel_B():
    W = 203.2
    g.append(f'<rect width="{W}" height="128.5" fill="{BG}"/>')
    t("808 CIRCUIT BENCH", 8, 11.5, 5.4, anchor="start", ls=0.4)
    t("CIRCUIT: " + VO["code"] + "  " + VO["name"], W - 8, 11.3, 4.0, fill=VO["colour"], anchor="end", weight=800)
    fixed = set(FRONT.get(VO["code"], [])) | {"LEVEL"}
    deep = [s for s in params(VO) if s[1][0] not in fixed]
    grid(deep, 8.0, W - 8.0, [26.5, 49.0], 8, values=False, lab_dy=8.6)
    BY = 63.6
    btn(16, BY, "STOCK"); btn(32, BY, "COPY"); btn(48, BY, "SHIFT"); btn(66, BY, "KITS")
    leds(77, BY - 0.2, "ABCD")
    t("big knobs follow the selected voice  |  front row is always every voice", W - 8, BY + 1.0, 2.4, fill=SOFT, anchor="end", weight=500)
    MIXW = 28.0; X0 = 16.0; VP = (W - X0 - 4.0 - MIXW - 4.0) / 13.0
    section("808 FRONT PANEL", 4.0, W - MIXW - 8.0, 73.0); section("MIX", W - MIXW - 4.0, W - 4.0, 73.0)
    R1, R2, R3, VY, TY, OY = 78.6, 86.4, 93.2, 100.0, 108.4, 119.0
    t("LEVEL", 3.4, R1 + 0.9, 2.3, fill=SOFT, anchor="start", weight=600)
    t("TUNE/TONE", 3.4, R2 + 0.9, 2.1, fill=SOFT, anchor="start", weight=600)
    t("DECAY/SNAP", 3.4, R3 + 0.9, 2.1, fill=SOFT, anchor="start", weight=600)
    ROW = {"TUNE": R2, "TONE": R2, "DECAY": R3, "SNAPPY": R3}
    for i, d in enumerate(data):
        x = X0 + VP*(i+0.5)
        ps = {p[0]: p for _, p in params(d)}
        knob(x, R1, ps["LEVEL"][2], big=False)
        for nm in FRONT.get(d["code"], []):
            comp("Trimpot", x, ROW[nm], -149.4 + 298.8 * ps[nm][2])
        voicebtn(x, VY, d, VP - 1.4, i == V); outbox(x, OY); comp("PJ301M", x, TY); comp("PJ301M", x, OY)
    t("TRIG", 3.4, TY + 0.8, 2.2, fill=SOFT, anchor="start", weight=500)
    t("OUT", 3.4, OY + 0.8, 2.2, fill=SOFT, anchor="start", weight=500)
    MX = [W - MIXW - 4.0 + 8.0, W - 10.0]
    t("ACC", MX[0], R1 - 2.2, 2.9); comp("PJ301M", MX[0], R1 + 3.6)
    t("POLY", MX[1], R1 - 2.2, 2.9); comp("PJ301M", MX[1], R1 + 3.6)
    for x, lab in zip(MX, ["L", "R"]): mixbox(x, OY, lab)
    logo(W); return W

# ---------------- C: COMPACT PAGES (26 HP) ----------------
FIRST = ["TUNE", "DECAY", "TONE", "SNAPPY", "ACCENT", "DRIVE", "LEVEL", "PAN"]
def panel_C():
    W = 132.08
    g.append(f'<rect width="{W}" height="128.5" fill="{BG}"/>')
    t("808 CIRCUIT BENCH", 6, 10.6, 4.6, anchor="start", ls=0.3)
    t(VO["code"], W - 6, 10.6, 4.6, fill=VO["colour"], anchor="end", weight=800)
    ps = params(VO)
    p1 = [s for n in FIRST for s in ps if s[1][0] == n]
    p1 += [s for s in ps if s not in p1][:8 - len(p1)]
    grid([("", s[1]) for s in p1], 6.0, W - 6.0, [21.5, 44.5], 4, groups=False)
    BY = 66.4
    btn(15, BY, "STOCK", w=12); btn(29.5, BY, "COPY", w=12); btn(44, BY, "SHIFT", w=12)
    btn(60, BY, "KITS", w=12); leds(70, BY - 0.2, "ABCD", dx=5.0)
    btn(103, BY, "PAGE", w=12); leds(113, BY - 0.2, ["1", "2"], dx=6.0)
    section("VOICES", 4.0, W - 4.0, 75.8)
    VP = (W - 8.0) / 7.0
    rows = [(data[:7], 81.0, 89.4), (data[7:] + [None], 97.6, 106.0)]
    for r, (vs, VY, TY) in enumerate(rows):
        for c, d in enumerate(vs):
            x = 4.0 + VP*(c+0.5)
            if d is None:
                t("ACC", x, VY + 1.1, 2.9); comp("PJ301M", x, TY); continue
            voicebtn(x, VY, d, VP - 2.2, data.index(d) == V); comp("PJ301M", x, TY)
    OY = 119.0
    section("OUT", 4.0, W - 4.0, 112.0) if False else None
    for x, lab in [(14, "POLY IN"), (38, "POLY OUT")]:
        t(lab, x + 9.5, OY + 0.9, 2.6, fill=SOFT, anchor="start", weight=600) if False else None
    for x, lab, box in [(12.0, "POLY", False), (30.0, "POLY OUT", True), (W - 30.0, "L", True), (W - 14.0, "R", True)]:
        if box:
            g.append(f'<rect x="{x-6}" y="{OY-8.2}" width="12" height="13.2" rx="1.2" fill="{ORA if len(lab) == 1 else "#4A4B50"}"/>')
            t(lab, x, OY - 4.8, 2.5 if len(lab) > 1 else 2.9, fill="#1B1B1B" if len(lab) == 1 else INK)
        else:
            t(lab + " IN", x, OY - 4.8, 2.5)
        comp("PJ301M", x, OY)
    t("hits", 56, OY - 1.0, 2.3, fill=SOFT, anchor="start", weight=500) if False else None
    logo(W); return W

def render(fn, out):
    g.clear(); W = fn(); H = 128.5; S = 8
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(W*S)}" height="{int(H*S)}" viewBox="0 0 {W} {H}">' + "".join(g) + "</svg>"
    F = "/usr/share/fonts/opentype/inter"
    html = f"""<html><head><meta charset="utf-8"><style>
@font-face{{font-family:Inter;font-weight:500;src:url(file://{F}/Inter-Medium.otf)}}
@font-face{{font-family:Inter;font-weight:600;src:url(file://{F}/Inter-SemiBold.otf)}}
@font-face{{font-family:Inter;font-weight:700;src:url(file://{F}/Inter-Bold.otf)}}
@font-face{{font-family:Inter;font-weight:800;src:url(file://{F}/Inter-ExtraBold.otf)}}
@font-face{{font-family:ShareTech;src:url(file://{os.path.expanduser('~/Rack-src/res/fonts')}/ShareTechMono-Regular.ttf)}}
body{{margin:0;background:#111}}</style></head><body>{svg}</body></html>"""
    tmp = tempfile.NamedTemporaryFile("w", suffix=".html", delete=False); tmp.write(html); tmp.close()
    js = """const {chromium}=require('playwright');(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:%d,height:%d}});
await p.goto('file://%s');await p.waitForTimeout(400);await p.screenshot({path:'%s'});await b.close()})()""" % (int(W*S), int(H*S), tmp.name, out)
    jf = tmp.name + ".js"; open(jf, "w").write(js)
    subprocess.run(["node", jf], check=True); print("wrote", out)
for tag, fn in [("A-follow16", panel_A), ("B-808front", panel_B), ("C-compact", panel_C)]:
    render(fn, os.path.join(OUT, f"bench-knobs-{tag}-{WANT}.png"))
