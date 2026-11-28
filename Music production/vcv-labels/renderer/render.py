#!/usr/bin/env python3
"""Render a VCV panel mockup: panel SVG from (patched) make_panel.py + components.
usage: render.py <repo> <Widget> <svg> <script> <preview.png> <out.png> [--big] [--layout layout.py]
"""
import argparse, base64, json, math, os, re, shutil, subprocess, sys, tempfile, io
from PIL import Image

W = '/tmp/claude-0/w'
LIB = W + '/rack/res/ComponentLibrary'
PX = 75 / 25.4

ap = argparse.ArgumentParser()
ap.add_argument('repo'); ap.add_argument('widget'); ap.add_argument('svg'); ap.add_argument('script'); ap.add_argument('preview'); ap.add_argument('out')
ap.add_argument('--big', action='store_true'); ap.add_argument('--scale', type=float, default=10.0)
ap.add_argument('--layout'); ap.add_argument('--labels-json')
a = ap.parse_args()

# ---- panel svg
tmp = tempfile.mkdtemp(dir=W)
shutil.copytree(a.repo, tmp + '/r', ignore=shutil.ignore_patterns('.git', '*.png', 'docs'))
script = tmp + '/r/' + a.script
src = open(script).read()
pre = '''
import json as _json, os as _os
_LABELS = []
_BIG = %r
def _scale(size):
    if not _BIG or size >= 3.5: return size
    return min(2 * size, 3.2)
def text(s, x, y, size, *args, **kw):
    ns = _scale(size)
    _LABELS.append([s, x, y, size, ns])
    return _text0(s, x, y, ns, *args, **kw)
import atexit
atexit.register(lambda: open(%r, 'w').write(_json.dumps(_LABELS)))
''' % (a.big, tmp + '/labels.json')
src = src.replace('def text(', 'def _text0(', 1)
# put prelude after the module docstring / comments but before code: simplest is at top
lines = src.split('\n')
src = pre + '\n' + src
if a.layout:
    src = src.replace('\n', '\n', 1)
open(script, 'w').write(src)
env = dict(os.environ)
if a.layout:
    env['PANEL_LAYOUT'] = a.layout
r = subprocess.run([sys.executable, script], cwd=os.path.dirname(script), env=env, capture_output=True, text=True)
if r.returncode:
    print(r.stdout, r.stderr); sys.exit(1)
svgpath = tmp + '/r/' + a.svg
svg = open(svgpath).read()
pw = float(re.search(r'width="([\d.]+)', svg).group(1)); ph = float(re.search(r'height="([\d.]+)', svg).group(1))
if a.labels_json: shutil.copy(tmp + '/labels.json', a.labels_json)

# ---- components (from possibly modified source in tmp copy)
comp = json.loads(subprocess.check_output(['node', W + '/tools/extract.js', tmp + '/r', a.widget, str(pw)], text=True))
OR = W + '/orig/' + os.path.basename(os.path.abspath(a.repo))
orig = json.loads(subprocess.check_output(['node', W + '/tools/extract.js', OR, a.widget, str(float(re.search(r'width="([\d.]+)', open(os.path.join(OR, a.svg)).read()).group(1)))], text=True))

prev = Image.open(a.preview).convert('RGBA')
pscale = prev.size[1] / 128.5

S = a.scale  # output px per mm

def uri_file(p):
    return 'data:image/svg+xml;base64,' + base64.b64encode(open(p, 'rb').read()).decode()

def svg_size(p):
    s = open(p).read(3000)
    f = lambda m: float(m.group(1)) * (PX if m.group(2) == 'mm' else 1) / PX  # -> mm
    return f(re.search(r'width="([\d.]+)(mm|px)?"', s)), f(re.search(r'height="([\d.]+)(mm|px)?"', s))

base_cache = {}
allsrc = ''
for root, _, fs in os.walk(tmp + '/r/src'):
    for f in fs: allsrc += open(os.path.join(root, f), errors='ignore').read()

def resolve(T):
    T = T.strip()
    m = re.match(r'(\w+)<(.*)>$', T)
    outer, inner = (m.group(1), m.group(2)) if m else (T, None)
    known = {
        'PJ301MPort': ['PJ301M'], 'ThemedPJ301MPort': ['PJ301M'], 'DarkPJ301MPort': ['PJ301M-dark'],
        'CKSS': ['CKSS_0'], 'CKSSThree': ['CKSSThree_0'], 'CKSSThreeHorizontal': ['CKSSThreeHorizontal_0'],
        'VCVLightBezel': ['VCVBezel'], 'VCVBezel': ['VCVBezel'], 'VCVLightButton': ['VCVButton_0'], 'VCVButton': ['VCVButton_0'],
        'VCVLightLatch': ['VCVButton_0'], 'VCVLightBezelLatch': ['VCVBezel'], 'VCVBezelLatch': ['VCVBezel'], 'VCVLatch': ['VCVButton_0'], 'TL1105': ['TL1105_0'], 'CKD6': ['CKD6_0'],
        'BefacoTinyKnob': ['BefacoTinyKnobWhite_bg', 'BefacoTinyPointBlack'], 'ScrewSilver': ['ScrewSilver'], 'ScrewBlack': ['ScrewBlack'],
        'BefacoPush': ['BefacoPush_0'], 'NKK': ['NKK_0'], 'VCVSlider': ['VCVSlider', 'VCVSliderHandle'],
    }
    if outer in known: return ('svg', known[outer], inner)
    if os.path.exists(f'{LIB}/{outer}_bg.svg'): return ('svg', [outer + '_bg', outer], inner)
    if os.path.exists(f'{LIB}/{outer}.svg'): return ('svg', [outer], inner)
    if re.search(r'Light$', outer) and outer != 'Light':
        return ('light', outer, inner)
    m2 = re.search(r'struct\s+%s\s*:\s*(?:public\s+)?([\w:<>, ]+?)\s*\{' % outer, allsrc)
    if m2:
        b = m2.group(1).replace('app::', '').replace('rack::', '').replace('componentlibrary::', '')
        if b not in ('Widget', 'OpaqueWidget', 'TransparentWidget', 'LedDisplay', 'app::LedDisplay', 'FramebufferWidget', 'ModuleLightWidget', 'SvgSlider', 'app::SvgSlider', 'SliderKnob', 'Knob', 'SvgKnob', 'SvgSwitch', 'SvgPort', 'ParamWidget', 'app::ParamWidget'):
            return resolve(b)
    return ('crop', outer, inner)

LIGHT_MM = {'TinyLight': 1.088, 'SmallLight': 2.176, 'MediumLight': 3.17, 'LargeLight': 5.179, 'TinySimpleLight': 1.088, 'SmallSimpleLight': 2.176, 'MediumSimpleLight': 3.17, 'LargeSimpleLight': 5.179}
COLORS = {'Red': '#ff3030', 'Green': '#30e040', 'Blue': '#3080ff', 'Yellow': '#ffd030', 'White': '#ffffff', 'GreenRed': '#30e040', 'RedGreenBlue': '#ff60a0', 'Orange': '#ff9020', 'Purple': '#b050ff', 'Cyan': '#40e0ff'}
def light_html(cx, cy, d, color, bezel=False):
    c = '#888'
    for k, v in COLORS.items():
        if color and k in color: c = v
    return f'<div style="position:absolute;left:{(cx-d/2)*S}px;top:{(cy-d/2)*S}px;width:{d*S}px;height:{d*S}px;border-radius:50%;background:{c};opacity:.85"></div>'

items = []
crops = 0
for i, c in enumerate(comp):
    o = orig[i] if i < len(orig) and orig[i]['type'] == c['type'] else None
    kind, what, inner = resolve(c['type'])
    x, y = c['x'], c['y']
    if x is None or y is None: continue
    if kind == 'svg':
        for j, name in enumerate(what):
            p = f'{LIB}/{name}.svg'
            w, h = svg_size(p)
            if c['centered']: lx, ly = x - w / 2, y - h / 2
            else: lx, ly = x, y
            rot = 'transform:rotate(-0.5rad);' if (j == len(what) - 1 and len(what) > 1) else ''
            items.append(f'<img src="{uri_file(p)}" style="position:absolute;left:{lx*S}px;top:{ly*S}px;width:{w*S}px;height:{h*S}px;{rot}">')
        if inner and 'Light' in inner:
            w, h = svg_size(f'{LIB}/{what[0]}.svg')
            cx, cy = (x, y) if c['centered'] else (x + w / 2, y + h / 2)
            items.append(light_html(cx, cy, min(w, h) * 0.55, inner))
    elif kind == 'light':
        d = LIGHT_MM.get(what, 2.176)
        cx, cy = (x, y) if c['centered'] else (x + d / 2, y + d / 2)
        items.append(light_html(cx, cy, d, inner))
    else:
        # crop from preview at original position
        if o is None: continue
        w, h = (c['w'] or 0), (c['h'] or 0)
        ow, oh = (o['w'] or 0), (o['h'] or 0)
        if ow == 0 or oh == 0:
            ow = oh = w = h = 9.0
            ox, oy = o['x'] - ow / 2, o['y'] - oh / 2
            x, y = x - w / 2, y - h / 2
        else:
            ox, oy = o['x'], o['y']
            if c['centered']: x, y = x - w / 2, y - h / 2
            if o['centered']: ox, oy = ox - ow / 2, oy - oh / 2
        box = (round(ox * pscale), round(oy * pscale), round((ox + ow) * pscale), round((oy + oh) * pscale))
        im = prev.crop(box).resize((max(1, round(w * S)), max(1, round(h * S))), Image.LANCZOS)
        b = io.BytesIO(); im.save(b, 'PNG')
        items.append(f'<img src="data:image/png;base64,{base64.b64encode(b.getvalue()).decode()}" style="position:absolute;left:{x*S}px;top:{y*S}px;width:{w*S}px;height:{h*S}px">')
        crops += 1

html = f'''<!doctype html><html><body style="margin:0;background:#222">
<div id="p" style="position:relative;width:{pw*S}px;height:{ph*S}px;background:url({uri_file(svgpath)});background-size:100% 100%">
{''.join(items)}</div></body></html>'''
hp = tmp + '/p.html'
open(hp, 'w').write(html)
js = f"""const {{ chromium }} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{ viewport: {{ width: {int(pw*S)+2}, height: {int(ph*S)+2} }} }});
await p.goto('file://{hp}'); await p.waitForTimeout(150); await p.locator('#p').screenshot({{ path: '{a.out}' }}); await b.close(); }})();"""
subprocess.run(['node', '-e', js], check=True, cwd='/tmp')
shutil.rmtree(tmp)
print('ok', a.out, f'{pw:.2f}mm = {round(pw/5.08)}HP', 'crops', crops)
