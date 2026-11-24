"""One-off: trim each voice's band gains so the real-time engine (rider_kit_ref) matches the recordings' band-by-time energies.
Needs the fit made from the recordings (kit_fit.json, with ref_bands and ref_early) and kit_fit.py; neither is in this repository.
   KITFIT=/path/to/kit python3 tools/proto/rider_kit_calibrate.py"""
import os, sys, json, math, importlib
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
KF = os.environ.get('KITFIT', '/mnt/project-files/jazz-drums-vcv/kit'); sys.path.insert(0, KF)
import kit_fit as K
F = json.load(open(os.path.join(KF, 'kit_fit.json')))
P = os.path.join(HERE, 'rider_kit.json')
def one(R, name, dur=4.5):
    k = R.Kit(R.KitParams(width=0.0), seed=3); v = R.VOICES.index(name)
    d = R.D['voices'][name]; k.strokes.append((0, v if name != 'snare-edge' else R.SNARE, 96 if name != 'snare-edge' else 41))
    return k
iters = int(sys.argv[1]) if len(sys.argv) > 1 else 3
for it in range(iters + 1):
    import rider_kit_ref as R; importlib.reload(R)
    D = R.D; rows = []
    for name, d in D['voices'].items():
        ref = np.array(F[name]['ref_bands']); refE = np.array(F[name]['ref_early'])
        k = R.Kit(R.KitParams(width=0.0), seed=3)
        # a stroke at exactly the reference level (0 dB): velocity 96, but the edge voice is reached through a soft snare, so call it directly
        k.strokes.append((0, R.VOICES.index(name), 96))
        if name == 'snare-edge':
            sv = R.vel_db; R.vel_db = lambda v: 0.0; y = k.render(4.5)[:, 0] * math.sqrt(2.0); R.vel_db = sv
        else: y = k.render(4.5)[:, 0] * math.sqrt(2.0)
        y = np.concatenate([np.zeros(24), y])                    # the recordings were cut 24 samples before the onset
        M = K.bands(y); ME = K.early(y); g = np.array(d['gain']); cg = np.array(d['cgain'])
        ok = (ref > ref.max() - 45) & (g > 0)[:, None] & (M > -150); dd = (M - ref)[ok]
        rows.append((name, float(np.mean(np.abs(dd))), float(np.max(np.abs(dd))), float(np.mean(dd))))
        if it == iters: continue
        dur = np.array([w[1] - w[0] for w in K.TW])
        for i in range(K.NB):
            o = (ref[i] > -150) & (M[i] > -150); o[:2] = False
            if o.any() and g[i] > 0:
                er = np.sum((10 ** (ref[i] / 10) * dur)[o]); em = np.sum((10 ** (M[i] / 10) * dur)[o])
                g[i] = float(np.clip(g[i] * math.sqrt(er / em), 0.05, 20.0))
        lo = K.EDGES[:-1] < 1000; dl = 10 * np.log10(np.sum(10 ** (refE[lo] / 10)) / np.sum(10 ** (ME[lo] / 10)))
        for i in range(K.NB): cg[i] = float(np.clip(cg[i] * 10 ** (np.clip(dl if lo[i] else refE[i] - ME[i], -6, 6) / 20.0), 0.03, 30.0))
        d['gain'] = [round(float(x), 5) for x in g]; d['cgain'] = [round(float(x), 5) for x in cg]
    print('pass %d: ' % it + '  '.join('%s %.1f/%.0f/%+.1f' % (n[:10], a, b, c) for n, a, b, c in rows))
    if it < iters: json.dump(D, open(P, 'w'), separators=(',', ':'))
