"""Renders the reference cases for every Werkbank engine. Writes WAVs (16-bit, 48 kHz) and ref_bands.json:
third-octave band levels (dB re 1 unit RMS) for each case, 50 Hz to 16 kHz. The C++ tests render the same cases
and must match the bands within the tolerance given in BUILD-INSTRUCTIONS.md.
Run: python3 make_reference.py <output folder>"""
import numpy as np, sys, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scipy.io import wavfile
from wb_common import *
import zap_blech_ref as ZB, dynamo_orch_ref as DO, vokoder_ref as VK, sprecher_ref as SP
fs = SR
OUT = sys.argv[1] if len(sys.argv) > 1 else 'ref_out'
os.makedirs(OUT, exist_ok=True)
CENT = 1000.0 * 2.0 ** (np.arange(-13, 13) / 3.0)        # 50 Hz ... 16 kHz
def bands(x):
    X = np.abs(np.fft.rfft(x)) ** 2; f = np.fft.rfftfreq(len(x), 1 / fs); out = []
    for c in CENT:
        m = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(round(float(10 * np.log10(2 * X[m].sum() / len(x) ** 2 + 1e-14)), 2))
    return out
REF = {}
def keep(name, x, params):
    x = np.asarray(x, float)
    REF[name] = dict(params=params, seconds=len(x) / fs, rms_db=round(float(10 * np.log10((x ** 2).mean() + 1e-14)), 2), bands=bands(x))
    wavfile.write(os.path.join(OUT, name + '.wav'), fs, np.clip(x * 0.5 * 32767, -32767, 32767).astype(np.int16))
    return x
S16 = 0.125                                              # a sixteenth at 120 BPM
def hits(pattern, bars, step=S16):
    t = np.zeros(int(len(pattern) * bars * step * fs))
    for b in range(bars):
        for i, c in enumerate(pattern):
            if c != '.': t[int((b * len(pattern) + i) * step * fs)] = 1.6 if c == 'X' else 1.0
    return t

# ---- ZAP: eight hits, 250 ms apart
ZAPS = {'zap_kick': (400., 50., .03, .80, .40, 0., 1., 0.), 'zap_pew_snare': (6000., 180., .02, .60, .15, .5, 1., .2),
        'zap_tom': (800., 200., .05, .95, .60, 0., 1., 0.), 'zap_tick': (9000., 3000., .004, .30, .03, .2, 1., 0.),
        'zap_drone': (1000., 1000., .03, 1.0, 8.0, 0., 1., 0.)}
tz = hits('x...', 4, 0.0625); pz = np.zeros(len(tz))
for k, p in ZAPS.items():
    keep(k, ZB.zap_render(tz, pz, *p, 0.0, unit(1, 4), 12345, float(fs)), dict(zip('start end sweep res decay noise click drive'.split(), p)))
# ---- BLECH: two bars per box, six voices summed
PAT = ['x.....x.x.......', '....x.......x...', 'x.x.x.x.x.x.x.X.', '........x.......', '.......x..x.....', '...x.....x....x.']
tr = np.stack([hits(p, 2) for p in PAT])
for b in range(3):
    o = ZB.blech_render(tr, ZB.BOX[b], np.zeros(6), np.ones(6), 0.7, 0.0, 1.0, 0.0, unit(2, 24), 999, float(fs))
    keep('blech_box%d' % (b + 1), o.sum(0), dict(box=b + 1, pattern=PAT, dyn=0.7))
    if b == 0:
        for v, nm in enumerate('bass snare hat cymbal tom clave'.split()): keep('blech_box1_' + nm, o[v], dict(voice=v))
# ---- DYNAMO
STEPS = [0, 12, 0, 7, 0, 12, 3, 10, 0, 12, 0, 7, 5, 12, 3, 15]; GATES = 'X.xxx.xxX.xxxx.x'
def line(octv, bars=2, glen=0.5):
    n = int(16 * bars * S16 * fs); p = np.zeros(n); g = np.zeros(n); a = np.zeros(n)
    for b in range(bars):
        for i, st in enumerate(STEPS):
            i0 = int((b * 16 + i) * S16 * fs); i1 = int((b * 16 + i + 1) * S16 * fs)
            p[i0:i1] = octv + st / 12.0
            if GATES[i] != '.': g[i0:i0 + int(glen * S16 * fs)] = 1; a[i0:i1] = 1.0 if GATES[i] == 'X' else 0.0
    return p, g, a
#          w1 w2 det  pw  mix sub noi cut  res  env  key   fA    fD   fS   aA    aD   aS  glide drv age
DYN = {'dynamo_bass':  [0, 0, .10, .5, .5, .4, 0, .30, .25, .45, .3, .002, .18, .0, .002, .30, .6, 0, .3, 0],
       'dynamo_pluck': [0, 1, 12., .3, .3, 0, 0, .35, .70, .60, .5, .002, .08, .0, .002, .12, .0, 0, .2, 0],
       'dynamo_lead':  [1, 1, .15, .35, .5, 0, 0, .55, .20, .25, .5, .020, .40, .6, .010, .30, .9, .04, .3, 0]}
for k, P in DYN.items():
    p, g, a = line(-2.0 if k != 'dynamo_lead' else 0.0); z = np.zeros(len(p))
    keep(k, DO.dynamo_render(p, g, a, z, P), dict(P=P))
# ---- ORCHESTER: one chord per disc
for d, nm in enumerate('choir strings flute organ cello horn vox reed'.split()):
    keep('orch_' + nm, DO.orch_render([(-3, 0, 2.6), (0, 0, 2.6), (4, 0, 2.6), (9, 0, 2.6)], d, 3.0, age=0.0), dict(disc=d, notes=[-3, 0, 4, 9]))
# ---- SPRECHER
W = SP.load_words()
SPK = {'spr_en_numbers_formant': ('1 2 3 4 5 6 7 8', 'en', 'formant', {}), 'spr_de_numbers_formant': ('1 2 3 4 5 6 7 8', 'de', 'formant', {}),
       'spr_en_numbers_chip': ('1 2 3 4 5 6 7 8', 'en', 'chip', {}), 'spr_de_words_chip': ('wir sind die roboter musik maschine', 'de', 'chip', {}),
       'spr_en_words_formant': ('we are the music machine computer world', 'en', 'formant', {}),
       'spr_small': ('hello computer', 'en', 'formant', dict(size=0.75, base_hz=220.)), 'spr_giant': ('hello computer', 'en', 'formant', dict(size=1.35, base_hz=65.)),
       'spr_whisper': ('1 2 3 4', 'en', 'formant', dict(whisper=1.0)), 'spr_ja': ('1 2 3 4 5 6 7 8', 'ja', 'formant', {}),
       'spr_fr_es_it_ru': None}
for k, v in SPK.items():
    if v is None:
        x = np.concatenate([SP.say('1 2 3 4', l, 'formant', words=W, inflect=0.0)[0] for l in ('fr', 'es', 'it', 'ru')])
        keep(k, x, dict(text='1 2 3 4', langs=['fr', 'es', 'it', 'ru'])); continue
    kw = dict(inflect=0.0); kw.update(v[3])
    keep(k, SP.say(v[0], v[1], v[2], words=W, **kw)[0], dict(text=v[0], lang=v[1], engine=v[2], **kw))
# sung: four words held for 400 ms each on a melody
wl = [p for _, p in SP.parse('music machine computer world', 'en', W)]
gates = [(0.1 + 0.6 * i, 0.4) for i in range(4)]; T = SP.schedule(wl, gates, fs, 1.0, hold=True)
N = int(2.8 * fs); f0 = np.zeros(N)
for i, st in enumerate([0, 3, 7, 5]): f0[int(0.6 * i * fs):] = 110.0 * 2 ** (st / 12)
sung = keep('spr_sung', SP.formant_engine(T[:int(2.8 / SP.CTRL)], f0, 1.0, 0.0, 0.0, unit(7, 4), 4242, float(fs))[0], dict(text='music machine computer world', gates=gates, semis=[0, 3, 7, 5]))
# ---- VOKODER: the formant voice onto the internal carrier
mod = SP.say('we are the music machine', 'en', 'formant', words=W, inflect=0.0, rate=0.85)[0]
N = len(mod); z = np.zeros(N); sl = np.ones(VK.NB)
for k, (chord, shape, shift) in {'vok_saw_major': (3, 0.0, 0.0), 'vok_pulse_single': (0, 1.0, 0.0), 'vok_shift_up': (3, 0.0, 2.0), 'vok_shift_down': (3, 0.0, -2.0)}.items():
    car = VK.carrier(np.full(N, -1.0), shape, 0.3, chord, float(fs))
    o, v, s = VK.vok_render(mod, car, VK.CENTRES, sl, 0.004, 0.04, shift, 0.7, 0.5, z, 0.0, unit(3, 60), 77, float(fs))
    keep(k, o, dict(chord=chord, shape=shape, shift=shift, text='we are the music machine', rate=0.85))
json.dump(dict(centres=[round(float(c), 1) for c in CENT], cases=REF), open(os.path.join(OUT, 'ref_bands.json'), 'w'), indent=1)
print(len(REF), 'cases'); 
for k, v in REF.items(): print('%-28s %5.2f s  %6.1f dB' % (k, v['seconds'], v['rms_db']))

# ---- a gallery for listening (not a test case): four bars of everything together
p, g, a = line(-2.0, 4); n = len(p); z = np.zeros(n)
bass = DO.dynamo_render(p, g, a, z, DYN['dynamo_bass'])
tr4 = np.stack([hits(q, 4) for q in PAT]); tr4[1] = 0; tr4[4] = 0
drums = ZB.blech_render(tr4, ZB.BOX[2], np.zeros(6), np.ones(6), 0.7, 0.0, 1.0, 0.25, unit(2, 24), 999, float(fs)).sum(0)
zs = ZB.zap_render(hits('....x.......x...', 4), z, 6000., 180., .02, .60, .15, .5, 1., .2, 0.25, unit(1, 4), 12345, float(fs))
zt = ZB.zap_render(hits('..x....x.x....x.', 4), p + 2.0, 1200., 300., .03, .92, .25, 0., 1., 0., 0.25, unit(4, 4), 5, float(fs))
choir = DO.orch_render([(-12, 0, 7.6), (-9, 0, 7.6), (-5, 0, 7.6), (0, 0, 7.6)], 0, 8.0, age=0.25)
cnt = SP.say('1 2 3 4 5 6 7 8', 'de', 'formant', words=W, inflect=0.0, gap=0.0)[0]
wl = [q for _, q in SP.parse('1 2 3 4 5 6 7 8', 'de', W)]
T = SP.schedule(wl, [(4.0 + 0.5 * i, 0.3) for i in range(8)], fs, 1.2, hold=True)
voice = SP.formant_engine(T[:int(8.0 / SP.CTRL)], np.full(n, 110.0), 1.0, 0.0, 0.0, unit(7, 4), 4242, float(fs))[0]
car = VK.carrier(np.full(n, -1.0 - 9 / 12.0), 0.0, 0.3, 2, float(fs))
vo = VK.vok_render(voice, car, VK.CENTRES, np.ones(VK.NB), 0.004, 0.04, 0.0, 0.7, 0.5, z, 0.25, unit(3, 60), 77, float(fs))[0]
mix = 0.8 * bass + 0.8 * drums + 0.5 * zs + 0.35 * zt + 0.25 * choir[:n] + 0.5 * vo
wavfile.write(os.path.join(OUT, '00-gallery.wav'), fs, np.clip(np.tanh(mix * 0.7) * 0.9 * 32767, -32767, 32767).astype(np.int16))
