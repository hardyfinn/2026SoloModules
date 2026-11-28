"""A/B: the recorded cymbal, then the model, playing the same strokes."""
import json, glob, os, numpy as np, soundfile as sf
from kit_fit import *
REF = '/tmp/drumref/virt/Samples/oh'; OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ab'); os.makedirs(OUT, exist_ok=True)
D = json.load(open('kit_fit.json'))
def real_layers(pat, ks): return [[load(p) for p in sorted(glob.glob(REF + pat % k))] for k in ks]
def play(hits, total, real=None, cym=None, tot=None, seed=1):
    """hits: (time s, level dB against the reference layer)"""
    rng = np.random.RandomState(seed); y = np.zeros(int(total * SR)); rr = 0
    lv = np.array(tot) - tot[len(tot) // 2 if len(tot) > 1 else 0]
    for t, db in hits:
        if real is not None:
            k = int(np.argmin(np.abs(lv - db))); s = real[k][rr % len(real[k])] * 10 ** ((db - lv[k]) / 20); rr += 1
        else: s = cym.stroke(level_db=db, rng=rng, vary=1.5)
        a = int(t * SR); s = s[:len(y) - a]; y[a:a + len(s)] += s
    return y
def ab(name, hits, total, key, pat, ks):
    real = real_layers(pat, ks); c = Cymbal(D[key]); tot = D[key]['tot_db']
    a = play(hits, total, real=real, tot=tot); b = play(hits, total, cym=c, tot=tot)
    g = 0.7 / max(np.abs(a).max(), np.abs(b).max()); gap = np.zeros(int(0.6 * SR))
    sf.write(os.path.join(OUT, name + '.wav'), np.concatenate([a, gap, b]) * g, SR, subtype='PCM_16')
    print(name, 'rms real %.1f dB model %.1f dB' % (20 * np.log10(np.std(a)), 20 * np.log10(np.std(b))))
bpm = 168; spb = 60 / bpm
sw = []
for bar in range(4):
    for beat, off, db in [(0, 0, -3), (1, 0, 1), (1, 0.64, -9), (2, 0, -2), (3, 0, 2), (3, 0.64, -8)]:
        sw.append(((bar * 4 + beat + off) * spb + 0.05, db + np.random.RandomState(bar * 7 + beat).uniform(-1.5, 1.5)))
ab('ride-1-swing-pattern_recording-then-model', sw, 16 * spb + 3.0, 'ride', '/ride/oh_ride_ride_vl%d_rr*', (1, 2, 3))
ab('ride-2-single-strokes-soft-to-loud_recording-then-model', [(0.05, -11), (2.0, -5), (4.0, 0), (6.0, 5)], 10.0, 'ride', '/ride/oh_ride_ride_vl%d_rr*', (1, 2, 3))
bl = [((b) * spb + 0.05, db) for b, db in [(0, 0), (1.64, -6), (2, 0), (3.64, -6), (4, 2), (5.64, -6), (6, 0), (7, 3)]]
ab('ride-3-bell_recording-then-model', bl, 8 * spb + 3.5, 'bell', '/ride/oh_ride_bell_vl%d_rr*', (1, 2, 3))
ab('ride-4-crash_recording-then-model', [(0.05, 0)], 7.0, 'crash', '/flatride/oh_flatride_crash_vl%d*', (1, 2, 3, 4))
