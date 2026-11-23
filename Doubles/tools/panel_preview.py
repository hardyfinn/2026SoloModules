"""Draws an approximate picture of the device view from the presentation layout in Doubles.maxpat."""
import json, os, sys, wave
import numpy as np
from PIL import Image, ImageDraw, ImageFont
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
S = 2
dev = json.load(open(os.path.join(ROOT, 'Doubles.maxpat')))['patcher']
W, H = int(dev['devicewidth']), 169
img = Image.new('RGB', (W * S, H * S), (36, 36, 38))
d = ImageDraw.Draw(img)
F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 7 * S)
FS = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 6 * S)
def R(r): return [r[0] * S, r[1] * S, (r[0] + r[2]) * S, (r[1] + r[3]) * S]
wavefile = sys.argv[1] if len(sys.argv) > 1 else None
for b in dev['boxes']:
    b = b['box']
    if not b.get('presentation'):
        continue
    r = b['presentation_rect']; mc = b['maxclass']
    vo = b.get('saved_attribute_attributes', {}).get('valueof', {})
    if mc == 'panel':
        d.rounded_rectangle(R(r), radius=4 * S, fill=(44, 44, 46))
    elif mc == 'comment':
        txt = b.get('text', '')
        words, linesout, cur = txt.split(), [], ''
        for w_ in words:
            t = (cur + ' ' + w_).strip()
            if d.textlength(t, font=FS if len(txt) > 20 else F) > r[2] * S and cur:
                linesout.append(cur); cur = w_
            else:
                cur = t
        linesout.append(cur)
        col = tuple(int(255 * c) for c in b.get('textcolor', [0.8, 0.8, 0.8])[:3])
        for i, ln in enumerate(linesout):
            d.text((r[0] * S, (r[1] + i * 8) * S), ln, font=FS if len(txt) > 20 else F, fill=col)
    elif mc == 'live.dial':
        cx, cy, rad = (r[0] + r[2] / 2) * S, (r[1] + 26) * S, 13 * S
        d.text((r[0] * S + 2, r[1] * S), vo.get('parameter_shortname', ''), font=FS, fill=(200, 200, 205))
        d.arc([cx - rad, cy - rad, cx + rad, cy + rad], 135, 405, fill=(90, 90, 95), width=3 * S)
        lo, hi, ini = vo['parameter_mmin'], vo['parameter_mmax'], vo['parameter_initial'][0]
        f = (ini - lo) / (hi - lo)
        d.arc([cx - rad, cy - rad, cx + rad, cy + rad], 135, 135 + 270 * f, fill=(255, 160, 40), width=3 * S)
        unit = vo.get('parameter_unitstyle')
        val = '%d %%' % ini if unit == 5 else ('%.1f dB' % ini if unit == 4 else str(ini))
        d.text((cx - d.textlength(val, font=FS) / 2, (r[1] + 40) * S), val, font=FS, fill=(200, 200, 205))
    elif mc in ('live.text', 'live.button'):
        on = vo.get('parameter_initial', [0])[0] and b.get('mode', 1) == 1
        fill = (255, 160, 40) if on else (70, 70, 74)
        if mc == 'live.button':
            d.ellipse(R(r), fill=(70, 70, 74), outline=(120, 120, 125))
        else:
            d.rectangle(R(r), fill=fill, outline=(100, 100, 105))
            t = b.get('text', '')
            d.text(((r[0] + r[2] / 2) * S - d.textlength(t, font=FS) / 2, (r[1] + r[3] / 2 - 4) * S), t, font=FS, fill=(20, 20, 20) if on else (220, 220, 225))
    elif mc in ('live.menu', 'live.numbox'):
        d.rectangle(R(r), fill=(30, 30, 32), outline=(90, 90, 95))
        if mc == 'live.menu':
            t = vo['parameter_enum'][vo['parameter_initial'][0]]
        else:
            ini = vo['parameter_initial'][0]
            t = 'AUTO' if (ini == 0 and 'Fill' in vo['parameter_longname'] and 'Pat' not in vo['parameter_longname']) else ('%d %%' % ini if vo.get('parameter_unitstyle') == 5 else str(ini))
        d.text(((r[0] + 3) * S, (r[1] + 3) * S), t, font=FS, fill=(230, 230, 235))
    elif mc == 'live.tab':
        items = vo['parameter_enum']
        h = r[3] / len(items)
        for i, t in enumerate(items):
            rr = [r[0], r[1] + i * h, r[2], h - 1]
            on = i == vo['parameter_initial'][0]
            d.rectangle(R(rr), fill=(255, 160, 40) if on else (70, 70, 74))
            d.text(((r[0] + 4) * S, (rr[1] + 4) * S), t, font=FS, fill=(20, 20, 20) if on else (220, 220, 225))
    elif mc == 'live.drop':
        d.rectangle(R(r), outline=(120, 120, 125), fill=(30, 30, 32))
        d.text(((r[0] + 8) * S, (r[1] + r[3] / 2 - 4) * S), 'DROP AUDIO', font=FS, fill=(160, 160, 165))
    elif mc == 'jsui':
        d.rectangle(R(r), fill=(28, 28, 31))
        if wavefile:
            w_ = wave.open(wavefile); x = np.frombuffer(w_.readframes(w_.getnframes()), '<i2').reshape(-1, w_.getnchannels()).mean(1) / 32768
            n = int(r[2]); per = len(x) / n; top = r[1] + 2; wh = r[3] - 16; mid = top + wh / 2
            for k in range(8):
                xx = r[0] + r[2] * k / 8
                d.line([xx * S, top * S, xx * S, (top + wh) * S], fill=(77, 77, 82), width=1)
            for k in range(n):
                v = np.abs(x[int(k * per):int((k + 1) * per)]).max() * wh * 0.48
                xx = (r[0] + k + 0.5) * S
                d.line([xx, (mid - v) * S, xx, (mid + v) * S], fill=(140, 145, 153), width=S)
            for k in range(32):
                xx = (r[0] + r[2] * (k + 0.02) / 32) * S
                d.line([xx, top * S, xx, (top + 5) * S], fill=(217, 217, 217), width=1)
            d.rectangle([(r[0] + r[2] * 0.02 - 3) * S, top * S, (r[0] + r[2] * 0.02 + 3) * S, (top + 6) * S], fill=(255, 217, 51))
            d.rectangle([(r[0] + r[2] * 0.033 - 3) * S, top * S, (r[0] + r[2] * 0.033 + 3) * S, (top + 6) * S], fill=(242, 89, 115))
            for pos, col in ((0.31, (255, 140, 25)), (0.62, (64, 166, 255))):
                xx = (r[0] + r[2] * pos) * S
                d.line([xx, top * S, xx, (top + wh) * S], fill=col, width=2 * S)
            fy = r[1] + r[3] - 12
            d.rectangle([r[0] * S, fy * S, (r[0] + r[2]) * S, (fy + 10) * S], fill=(77, 77, 82))
            d.rectangle([r[0] * S, fy * S, (r[0] + r[2] / 2) * S, (fy + 10) * S], fill=(255, 140, 25))
            d.text(((r[0] + 3) * S, (fy + 1) * S), 'A', font=FS, fill=(20, 20, 20))
            d.text(((r[0] + r[2] - 9) * S, (fy + 1) * S), 'B', font=FS, fill=(220, 220, 225))
img.save(os.path.join(ROOT, 'docs', 'panel-preview.png'))
print('docs/panel-preview.png', img.size)
