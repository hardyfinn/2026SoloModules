"""Renders the tape sketches. Run from any folder: python3 make_sketches.py <outdir>"""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, soundfile as sf
from tape_proto import *
from songs import house, hiphop, techno, mix
OUT = sys.argv[1] if len(sys.argv) > 1 else 'sketches'
os.makedirs(OUT, exist_ok=True)
stats = {}
def rms(x): return np.sqrt(np.mean(x ** 2))
def save(name, x, ref):
    x = x * rms(ref) / rms(x); g = 0.89 / max(np.abs(ref).max(), 1e-9)
    y = x * g; pk = np.abs(y).max()
    if pk > 0.99: y = np.tanh(y / pk * 1.2) / np.tanh(1.2) * 0.99 if pk > 1.6 else y * 0.99 / pk
    sf.write(f'{OUT}/{name}.wav', y, SR, subtype='PCM_16')
    X = (np.abs(np.fft.rfft(y * np.hanning(len(y))[:, None], axis=0)) ** 2).sum(1); f = np.fft.rfftfreq(len(y), 1 / SR)
    band = lambda a, b: 10 * np.log10(X[(f >= a) & (f < b)].sum() + 1e-20)
    stats[name] = dict(crest=round(20 * np.log10(np.abs(y).max() / rms(y)), 1), hf=round(band(8000, 16000) - band(200, 2000), 1),
                       lf=round(band(30, 120) - band(200, 2000), 1), side=round(20 * np.log10(rms(y[:, 0] - y[:, 1]) / rms(y[:, 0] + y[:, 1]) + 1e-9), 1))
    return y

def four_track(st, name='porta', rec=4.0, drive=0.0, **kw):
    """drums on 1, bass on 2, keys on 3 and 4, like a real cassette session."""
    k = st['keys'] + st['extra']
    tr = np.stack([st['drums'], st['bass'], k[:, 0], k[:, 1]], 1)
    if drive: tr = desk(tr, drive)
    y = machine(tr, name, rec=rec, **kw)
    return np.stack([y[:, 0] + y[:, 1] + y[:, 2], y[:, 0] + y[:, 1] + y[:, 3]], 1)
def bars(x, st, a, b):
    L = 16 * 60 / st['bpm'] / 4; return x[int(a * L * SR):int(b * L * SR)]
def xcat(parts, gap=0.35):
    out = []
    for p in parts:
        p = p.copy(); n = int(0.01 * SR); p[:n] *= np.linspace(0, 1, n)[:, None]; p[-n:] *= np.linspace(1, 0, n)[:, None]
        out += [p, np.zeros((int(gap * SR), 2))]
    return np.concatenate(out[:-1])
def match(x, ref): return x * rms(ref) / rms(x)

H, P, Tn = house(), hiphop(), techno()
dH, dP, dT = mix(H), mix(P), mix(Tn)
G = {}
G['01'] = save('01-house-dry', dH, dH)
y = four_track(H, 'porta', 0, age=0.15, seed=11); y = machine(y, 'deck2', rec=2, age=0.15, seed=12)
G['02'] = save('02-house-four-track-to-cassette-master', y, dH)
y = four_track(H, 'porta', 9, drive=8, age=0.2, seed=13); y = machine(y, 'deck2', rec=7, age=0.2, seed=14)
G['03'] = save('03-house-four-track-levels-in-the-red', y, dH)
# two cassette decks, as Larry Heard described: one pass on deck A, play it back and add parts live onto deck B
a = machine(np.stack([H['drums'] + H['bass']] * 2, 1), 'deck', rec=5, age=0.3, seed=15, nr=None)
y = machine(a + H['keys'] + H['extra'], 'deck', rec=5, age=0.3, seed=16, nr=None)
G['04'] = save('04-house-two-cassette-decks-no-noise-reduction', y, dH)
# bounce generations
gens = [dH]; cur = dH
for g in range(4):
    cur = machine(cur, 'porta', rec=1, age=0.2, seed=20 + g); gens.append(cur)
save('05-house-bounced-dry-then-1-2-3-4-generations', xcat([match(bars(x, H, 0, 4), bars(dH, H, 0, 4)) for x in gens]), dH)
G['06'] = save('06-hiphop-dry', dP, dP)
y = four_track(P, 'porta_hi', 2, drive=4, age=0.2, seed=31); y = machine(y, 'deck', rec=5, age=0.25, seed=32)
G['07'] = save('07-hiphop-four-track-high-speed-to-cassette-master', y, dP)
y = machine(machine(dP, 'boombox', rec=6, age=0.5, seed=33), 'boombox', rec=6, age=0.5, seed=34)
save('08-hiphop-boombox-dubbed-twice', y, dP)
# noise reduction misuse
src = dH; seg = lambda x: bars(x, H, 4, 8)
a = machine(src, 'deck2', rec=2, age=0.1, seed=41)
b = machine(src, 'deck2', rec=2, age=0.1, seed=41, nr='dolby', nr_dec=None)
c = machine(src, 'deck2', rec=2, age=0.1, seed=41, nr=None, nr_dec='dolby')
d = machine(src, 'porta', rec=2, age=0.1, seed=41, nr='dbx', nr_dec=None)
save('09-noise-reduction-right-then-three-ways-wrong', xcat([match(seg(x), seg(dH)) for x in (a, b, c, d)]), dH)
parts = []
for i, ag in enumerate((0.0, 0.33, 0.66, 1.0)):
    y = four_track(H, 'porta', 0, age=ag, seed=50 + i); y = machine(y, 'deck2', rec=2, age=ag, seed=60 + i)
    parts.append(match(bars(y, H, 0, 4), bars(dH, H, 0, 4)))
save('10-age-new-used-tired-neglected', xcat(parts), dH)
G['11'] = save('11-techno-dry', dT, dT)
k = Tn['keys']; tr = np.stack([Tn['drums'], Tn['bass'], k[:, 0], k[:, 1]], 1)
y = machine(tr, 'reel8', rec=9, age=0.15, seed=71); y = np.stack([y[:, 0] + y[:, 1] + y[:, 2], y[:, 0] + y[:, 1] + y[:, 3]], 1)
y = machine(y, 'reel2', rec=7, age=0.15, seed=72)
G['12'] = save('12-techno-budget-eight-track-reel-to-quarter-inch-master', y, dT)
y = four_track(Tn, 'porta', 8, drive=6, age=0.3, seed=73); y = machine(y, 'deck', rec=8, age=0.3, seed=74, nr=None)
save('13-techno-four-track-hot-to-normal-cassette', y, dT)
sp = dict(P); sp['keys'] = P['keys'] * 0; dS = mix(sp)
save('14-noise-reduction-breathing-drums-and-bass-only', xcat([match(x, dS)[:int(11 * SR)] for x in (machine(dS, 'porta', rec=0, age=0.3, wear=0.5, seed=81, nr=None), machine(dS, 'porta', rec=0, age=0.3, wear=0.5, seed=81))]), dS)
y = four_track(H, 'porta', 1, age=0.5, wear=0.8, seed=91); y = machine(y, 'deck', rec=4, age=0.5, wear=0.6, seed=92)
save('15-house-worn-tape-and-dirty-heads', y, dH)
y = four_track(H, 'porta', 4, drive=4, age=0.2, seed=95); y = bus_comp(y, -8, 4, 0.004, 0.15); y = machine(y, 'deck2', rec=6, age=0.2, seed=96)
save('16-house-full-chain-with-budget-compressor', y, dH)
gal = []
for st, dry, a, b in ((H, '01', '02', '03'), (P, '06', '07', None), (Tn, '11', '12', None)):
    for key in (dry, a, b):
        if key: gal.append(bars(G[key], st, 0, 4))
sf.write(f'{OUT}/00-gallery.wav', xcat(gal), SR, subtype='PCM_16')
for k_, v in stats.items(): print(k_, v)
