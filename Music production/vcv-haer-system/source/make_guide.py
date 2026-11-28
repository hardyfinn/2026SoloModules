#!/usr/bin/env python3
"""Builds the Haer System Mk1 user guide (HTML, then PDF with Playwright) from docs/MANUAL.md.
    python3 make_guide.py /path/to/repo out.pdf
"""
import html, os, re, sys, base64

REPO = sys.argv[1]
OUT = sys.argv[2]
PANELS = "/mnt/project-files/vcv-codex"
md = open(os.path.join(REPO, "docs/MANUAL.md")).read()

def inline(s):
    s = html.escape(s, quote=False)
    s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1", s)
    return s

def convert(block):
    out, lines, i = [], block.split("\n"), 0
    while i < len(lines):
        l = lines[i]
        if not l.strip():
            i += 1; continue
        if l.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")]); i += 1
            head, body = rows[0], [r for r in rows[2:]]
            t = "<table><thead><tr>" + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr></thead><tbody>"
            t += "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in body)
            out.append(t + "</tbody></table>"); continue
        if re.match(r"\d+\. ", l):
            items = []
            while i < len(lines) and re.match(r"\d+\. ", lines[i]):
                item = inline(re.sub(r"^\d+\. ", "", lines[i])); i += 1
                subs = []
                while i < len(lines) and re.match(r"\s+- ", lines[i]):
                    subs.append(inline(lines[i].strip()[2:])); i += 1
                if subs:
                    item += "<ul>" + "".join(f"<li>{x}</li>" for x in subs) + "</ul>"
                items.append(item)
            out.append("<ol>" + "".join(f"<li>{x}</li>" for x in items) + "</ol>"); continue
        if l.startswith("- "):
            items = []
            while i < len(lines) and lines[i].startswith("- "):
                items.append(lines[i][2:]); i += 1
            out.append("<ul>" + "".join(f"<li>{inline(x)}</li>" for x in items) + "</ul>"); continue
        if l.startswith("### "):
            out.append(f"<h3>{inline(l[4:])}</h3>"); i += 1; continue
        para = [l]; i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r"(\||\d+\. |- |#)", lines[i]):
            para.append(lines[i]); i += 1
        p = " ".join(para)
        m = re.match(r"\*\*(Three tips)\*\*$", p)
        if m:
            out.append(f'<h3>{m.group(1)}</h3>'); continue
        out.append(f"<p>{inline(p)}</p>")
    return "\n".join(out)

sections = re.split(r"^## ", md, flags=re.M)
signals = next(s for s in sections if s.startswith("Signals used"))
others = next(s for s in sections if s.startswith("Working with other"))
modules = [s for s in sections if re.match(r"[A-Z ]+: ", s)]

LAYER = {"WARP": "time", "CONTOUR": "time", "MUTATE": "time", "CHAIN": "time", "FLOW": "time", "TUNE MAP": "time",
         "STRIKE": "sound", "FMX": "sound", "SPLICE": "sound", "DEPTH": "sound",
         "VAULT": "system", "STEER": "system", "EAR": "system"}
LAYER_NAME = {"time": "Time and pattern", "sound": "Sound", "system": "Keep, perform, listen"}

TIGHT = {"MUTATE": ' style="max-height:36mm"', "CHAIN": ' style="max-height:44mm"'}

def img(name):
    path = os.path.join(PANELS, name.replace(" ", "") + "-panel-layout.png")
    data = base64.b64encode(open(path, "rb").read()).decode()
    return f"data:image/png;base64,{data}"

mod_html, toc = [], []
for s in modules:
    head, body = s.split("\n", 1)
    name, rest = head.split(": ", 1)
    m = re.match(r"(.*) \((\d+) HP\)", rest)
    tagline, hp = m.group(1), m.group(2)
    layer = LAYER[name]
    anchor = name.replace(" ", "").lower()
    toc.append(f'<li><span class="dot {layer}"></span><b>{name}</b> <span class="small">{html.escape(tagline)}</span></li>')
    mod_html.append(f'''<section class="page module {layer}" id="{anchor}">
  <div class="kicker {layer}">{LAYER_NAME[layer]} · {hp} HP</div>
  <h2>{name}</h2>
  <p class="tag">{html.escape(tagline[0].upper() + tagline[1:])}</p>
  <div class="fig"><img src="{img(name)}"{TIGHT.get(name, "")}></div>
  {convert(body)}
</section>''')

CSS = """
@page { size: A4; margin: 15mm 15mm 17mm 15mm; }
:root { --ink:#222226; --quiet:#6a6a72; --paper:#efede9; --plate:#1d1d1f; --time:#d9902a; --sound:#22a593; --system:#7d5fd6; }
* { box-sizing: border-box; }
body { font-family: Inter, Helvetica, Arial, sans-serif; color: var(--ink); font-size: 9.2pt; line-height: 1.38; margin: 0; }
h1 { font-size: 34pt; letter-spacing: .02em; margin: 0 0 4mm; line-height: 1.02; }
h2 { font-size: 20pt; margin: 0 0 1mm; letter-spacing: .04em; }
h3 { font-size: 11.5pt; margin: 4mm 0 1.5mm; }
p { margin: 0 0 1.8mm; }
.page { page-break-after: always; }
.page:last-child { page-break-after: auto; }
.cover { height: 262mm; display: flex; flex-direction: column; justify-content: space-between; }
.kicker { font-weight: 700; letter-spacing: .14em; font-size: 8.5pt; text-transform: uppercase; margin-bottom: 2mm; color: var(--quiet); }
.kicker.time, .module.time h2 { color: var(--time); }
.kicker.sound, .module.sound h2 { color: var(--sound); }
.kicker.system, .module.system h2 { color: var(--system); }
.module h2 { border-bottom: 1.2pt solid currentColor; padding-bottom: 1mm; }
.tag { font-size: 11.5pt; color: #3d3d44; margin: 1.5mm 0 3mm; }
.lead { font-size: 12.5pt; color: #3b3b40; max-width: 160mm; }
.strip { display: flex; gap: 2.5mm; align-items: flex-end; flex-wrap: nowrap; }
.strip img { height: 44mm; border-radius: 1mm; box-shadow: 0 .5mm 1.6mm rgba(0,0,0,.2); }
.fig { text-align: center; margin: 1mm 0 3mm; }
.fig img { max-width: 100%; max-height: 68mm; border-radius: 1.2mm; box-shadow: 0 .5mm 1.8mm rgba(0,0,0,.18); }
table { border-collapse: collapse; width: 100%; margin: 1.5mm 0 2.5mm; font-size: 8.3pt; page-break-inside: auto; }
tr { page-break-inside: avoid; }
th { text-align: left; background: var(--paper); font-weight: 700; padding: 1mm 2mm; border-bottom: .6pt solid #d3d0c9; }
td { padding: .9mm 2mm; border-bottom: .4pt solid #e4e1db; vertical-align: top; }
td:first-child { white-space: nowrap; font-weight: 600; }
code { font-family: Menlo, Consolas, monospace; font-size: 8.6pt; background: var(--paper); padding: 0 .8mm; border-radius: .6mm; }
ol, ul { margin: 0 0 2.2mm; padding-left: 5.5mm; }
li { margin-bottom: .9mm; }
.box { background: var(--paper); border-radius: 2mm; padding: 3mm 4mm; margin: 2mm 0 3mm; page-break-inside: avoid; }
.box h3 { margin-top: 0; }
.small { font-size: 8.6pt; color: var(--quiet); }
.dot { display: inline-block; width: 2.6mm; height: 2.6mm; border-radius: 50%; margin-right: 2mm; vertical-align: middle; }
.dot.time { background: var(--time); } .dot.sound { background: var(--sound); } .dot.system { background: var(--system); }
.toc { list-style: none; padding: 0; columns: 1; }
.toc li { margin-bottom: 1.6mm; font-size: 10.5pt; }
.recipe { margin-top: 7mm; page-break-inside: avoid; }
.recipes .recipe + .recipe { border-top: .3mm solid #ddd; padding-top: 5mm; }
.rtitle { font-size: 15pt; color: var(--plate); }
.mods { color: #3d3d44; }
.listen { margin-top: 2mm; }
.recipe ol ul { margin-top: 1mm; }
.recipe ol > li { margin-bottom: 1.3mm; }
.layers { display: flex; gap: 4mm; margin: 2mm 0 4mm; }
.layers > div { flex: 1; border-radius: 2mm; padding: 3mm; background: var(--paper); border-top: 2.5mm solid; }
.layers .time { border-color: var(--time); } .layers .sound { border-color: var(--sound); } .layers .system { border-color: var(--system); }
.layers h3 { margin-top: 0; }
.step { display: flex; gap: 2.5mm; margin-bottom: 2mm; page-break-inside: avoid; }
.num { flex: none; width: 5.5mm; height: 5.5mm; border-radius: 50%; background: var(--plate); color: #fff; font-size: 8pt; font-weight: 700; text-align: center; line-height: 5.5mm; }
"""

cover_strip = "".join(f'<img src="{img(n)}">' for n in ["WARP", "MUTATE", "STRIKE", "FLOW"])

intro = f'''
<section class="page cover">
  <div>
    <div class="kicker">Haer · VCV Rack 2 · user guide</div>
    <h1>Haer System Mk1</h1>
    <p class="lead">Thirteen modules for procedural music in the spirit of Autechre: time that bends, rhythms that rewrite themselves, struck and FM voices, space, and a way of working where the patch generates, you keep what is good, and then you perform with it.</p>
  </div>
  <div class="strip">{cover_strip}</div>
  <div class="small">Version 2.11.0 · Mac (Apple Silicon) build: HaerSystemMk1-2.11.0-mac-arm64.vcvplugin · Source: github.com/hardyfinn/HaerSystemMk1</div>
</section>

<section class="page">
  <h2>How the system fits together</h2>
  <p>Every module belongs to one of three layers, and its panel colour tells you which.</p>
  <div class="layers">
    <div class="time"><h3>Time and pattern</h3><p><b>WARP</b> is the clock. <b>CONTOUR</b>, <b>MUTATE</b> and <b>CHAIN</b> write rhythms and melodies. <b>FLOW</b> makes slow, connected modulation. <b>TUNE MAP</b> bends pitch into tunings that morph.</p></div>
    <div class="sound"><h3>Sound</h3><p><b>STRIKE</b> (struck objects) and <b>FMX</b> (FM) are the voices. <b>SPLICE</b> chops and smears. <b>DEPTH</b> places up to four sources near and far in one room.</p></div>
    <div class="system"><h3>Keep, perform, listen</h3><p><b>VAULT</b> remembers the last 64 bars and keeps the good ones as clips. <b>STEER</b> plays the whole rack with macros, scenes and saved versions. <b>EAR</b> listens to audio and learns its groove.</p></div>
  </div>

  <h3>Phase, not clock pulses</h3>
  <p>WARP does not send clock pulses. It sends <b>phase</b>: a 0 to 10 V ramp that rises once per bar. Every module that follows phase knows exactly where it is inside the bar, so when WARP bends time inside the bar, or its tempo drifts, everything bends together and still lands on the downbeat. Patch WARP's output 1 into the PHASE input of anything that has one.</p>

  <h3>Generate, keep, perform</h3>
  <ol>
    <li><b>Generate.</b> Let MUTATE, CONTOUR, CHAIN and FLOW play. Steer them with their AMOUNT, DRIFT and CHARACTER knobs rather than entering notes.</li>
    <li><b>Keep.</b> When something good goes by, press KEEP on the module (8 slots each) or on VAULT, which keeps the last few bars of everything it records as a clip.</li>
    <li><b>Perform.</b> Play the kept clips from VAULT, and ride the rack with STEER's macros, scenes and versions.</li>
  </ol>

  <h3>Signals</h3>
  {convert(signals.split(chr(10), 1)[1])}

  <div class="box">
    <h3>Every control explains itself</h3>
    <p>Hover over any knob, button or jack in Rack for a tooltip saying what it does and its range. Right-click a module's panel for its extra options. Modules with a SEED knob are repeatable: the same seed gives the same result after a reset.</p>
  </div>
</section>

<section class="page">
  <h2>A first patch</h2>
  <p>Ten minutes from an empty rack to a bending beat in a room.</p>
  <div class="step"><span class="num">1</span><div>Add <b>WARP</b>, <b>MUTATE</b>, two <b>STRIKE</b>s, <b>DEPTH</b> and an <b>Audio</b> module (VCV Core). Turn WARP's RUN on.</div></div>
  <div class="step"><span class="num">2</span><div>WARP <b>1</b> → MUTATE <b>PHASE</b>. Press MUTATE's <b>GEN</b> until the grid on its screen looks busy enough. DENS changes how busy the next GEN is.</div></div>
  <div class="step"><span class="num">3</span><div>MUTATE <b>1</b> → first STRIKE's <b>TRIG</b>, MATERIAL to the far left (wood), PITCH low. MUTATE <b>2</b> and <b>3</b> → a Merge → second STRIKE's TRIG, MATERIAL in the middle (metal).</div></div>
  <div class="step"><span class="num">4</span><div>Each STRIKE's <b>L</b> and <b>R</b> → DEPTH sources 1 and 2. DIST 1 at 10 %, DIST 2 at 60 %. DEPTH <b>OUT L</b>/<b>R</b> → Audio.</div></div>
  <div class="step"><span class="num">5</span><div>On MUTATE set <b>AMT</b> and <b>DRIFT</b> to about 40 %: the beat keeps changing but stays itself. Press <b>HOME</b> to snap back.</div></div>
  <div class="step"><span class="num">6</span><div>On WARP set SHAPE to 2 (S-curve) and raise <b>DEPTH</b>: the beats bunch and spread inside each bar and meet again on the downbeat.</div></div>
  <div class="step"><span class="num">7</span><div>When you like it, press MUTATE's <b>KEEP</b>. Change things, then set RECALL to 1 and send a trigger to the RECALL jack (any button or trigger module): the kept beat comes back at the next bar.</div></div>
  <p class="small">Five bigger starter patches (Bent electro, Interference, Communicating voices, Keep then play, The rack listens) are in the Patch recipes section at the end of this guide.</p>

  <h2 style="margin-top:8mm">Modules</h2>
  <ul class="toc">{"".join(toc)}</ul>
</section>
'''

pmd = open(os.path.join(REPO, "docs/PATCHES.md")).read()
psec = re.split(r"^## ", pmd, flags=re.M)
pintro = psec[0].split("\n", 1)[1].replace("---", "")
pintro = pintro.replace("Four starter patches (spec §17), plus one that uses the listening layer.", "Four starter patches, plus one that uses the listening layer.")
recipes_html = []
for r in psec[1:]:
    title, body = r.split("\n", 1)
    body = body.replace("---", "").strip()
    rhtml = convert(body)
    rhtml = re.sub(r"<p><b>Modules:</b>(.*?)</p>", r'<p class="mods"><b>Modules:</b>\1</p>', rhtml)
    rhtml = re.sub(r"<p><b>(Listen for:|Add TUNE MAP:)</b>(.*?)</p>", r'<div class="box listen"><b>\1</b>\2</div>', rhtml)
    recipes_html.append(f'<div class="recipe"><h2 class="rtitle">{inline(title)}</h2>{rhtml}</div>')
recipes = f'''<section class="page recipes">
  <div class="kicker">Patch recipes</div>
  <h2>Five starter patches</h2>
  {convert(pintro)}
  {"".join(recipes_html)}
</section>
'''

outro = f'''<section class="page">
  <h2>Working with other plugins</h2>
  {convert(re.sub(r" ?\(spec §[0-9A-Z]+\)", "", others.split(chr(10), 1)[1]))}
  <h2 style="margin-top:8mm">Installing</h2>
  <ol>
    <li>Download <code>HaerSystemMk1-2.11.0-mac-arm64.vcvplugin</code>.</li>
    <li>In Rack: Help → Open user folder, then open <code>plugins-mac-arm64</code>. Drop the file in. Delete any older <code>Codex</code> build there.</li>
    <li>Restart Rack. The modules are under the brand <b>Haer</b> in the module browser.</li>
  </ol>
</section>'''

doc = f'<!doctype html><html><head><meta charset="utf-8"><title>Haer System Mk1: user guide</title><style>{CSS}</style></head><body>{intro}{"".join(mod_html)}{recipes}{outro}</body></html>'
html_path = os.path.splitext(OUT)[0] + ".html"
open(html_path, "w").write(doc)
print("wrote", html_path)
