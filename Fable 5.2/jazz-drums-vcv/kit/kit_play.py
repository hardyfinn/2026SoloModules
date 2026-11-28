"""Play a list of drum notes through the recorded kit or through the fitted model, the same way, for comparison."""
import os, sys, json, glob
import numpy as np
from kit_fit import *
HERE = os.path.dirname(os.path.abspath(__file__))
FIT = json.load(open(os.path.join(HERE, 'kit_fit.json')))
PAT = {n: p for n, p, _ in JOBS}
#        note: (voice, pan -1..1, seconds rendered)
VOICE = {51: ('ride', 0.35, 5.0), 53: ('bell', 0.35, 4.0), 49: ('crash', -0.3, 6.0), 44: ('hat-foot', -0.45, 1.2), 42: ('hat-closed', -0.45, 1.0), 46: ('hat-open', -0.45, 3.0),
         38: ('snare', -0.1, 1.5), 37: ('cross-stick', -0.1, 1.0), 36: ('kick', 0.0, 2.0), 48: ('tom-high', 0.1, 2.0), 45: ('tom-high', 0.1, 2.0), 41: ('tom-low', 0.4, 2.0)}
BRUSH = {38: ('brush-tap', -0.1, 1.1), 39: ('brush-tap', -0.1, 1.1), 44: ('brush-hat-foot', -0.45, 1.0), 51: ('brush-ride', 0.35, 5.0)}
def vel_db(v): return 32.0 * np.log10(max(v, 1) / 96.0)
_real, _cym = {}, {}
def real_takes(name):
    if name not in _real:
        L = layers_of(PAT[name])
        if len(L) > 5: L = [L[i] for i in np.round(np.linspace(0, len(L) - 1, 5)).astype(int)]
        _real[name] = [[load(p) for p in ps] for ps in L]
    return _real[name]
def render(notes, total, model, brushes=False, seed=1, only=None):
    """notes: (t0, t1, midi note, velocity). Returns stereo."""
    rng = np.random.RandomState(seed); y = np.zeros((int(total * SR), 2)); rr = {}
    table = {**VOICE, **BRUSH} if brushes else VOICE
    if brushes and not only:
        # the sweeping hand: one legato circle per sweep note, never quite stopping
        d = FIT['brush-sweep']; n = len(y); amp = np.zeros(n); top = d['lx'][-1]
        for t0, t1, note, v in notes:
            if note != 40 or t0 < 0: continue
            a = int(t0 * SR); m = min(int((t1 - t0) * 1.08 * SR), n - a); u = np.arange(m) / m
            g = (0.42 + 0.58 * np.sin(np.pi * u ** 0.7) ** 2) * np.minimum(1, np.minimum(u, 1 - u) / 0.06) * 10 ** ((top - 3 + 0.5 * vel_db(v)) / 20.0)
            amp[a:a + m] = np.maximum(amp[a:a + m], g)
        drift = np.cumsum(rng.normal(0, 1, n // 480 + 2)); drift = np.interp(np.arange(n), np.arange(len(drift)) * 480, drift - np.linspace(drift[0], drift[-1], len(drift))); drift *= 1.2 / (np.std(drift) + 1e-9)
        lvl = 20 * np.log10(amp + 1e-6) + drift
        if model: s = Sweep(d).render(lvl, rng)
        else:
            src = load_whole(sorted(glob.glob(os.environ.get('BRUSHREF', '/tmp/drumref/swirly/Samples') + '/snare_stir/stir_dl3_rr1.wav'))[0]); src = src[SR // 2:]
            src = np.tile(src, n // len(src) + 1)[:n]; s = src * 10 ** ((lvl - 10 * np.log10(np.mean(src ** 2))) / 20.0)
        y[:, 0] += s * np.sqrt(0.55); y[:, 1] += s * np.sqrt(0.45)
    for t0, t1, note, v in notes:
        if note not in table or t0 < 0: continue
        name, pan, dur = table[note]
        if only and name not in only: continue
        if name == 'snare' and v < 42: name = 'snare-edge'
        if note == 39: v = min(127, v + 22)
        d = FIT[name]; x = np.array(d['tot_db']) - d['tot_db'][len(d['tot_db']) // 2 if len(d['tot_db']) > 1 else 0]
        db = float(np.clip(vel_db(v), min(x.min(), -12) - 6, max(x.max(), 4) + 3))
        if model:
            if name not in _cym: _cym[name] = Cymbal(d)
            s = _cym[name].stroke(level_db=db, dur=dur, rng=rng, vary=1.5)
        else:
            tk = real_takes(name); k = int(np.argmin(np.abs(x - db))); i = rr.get(name, 0); rr[name] = i + 1
            s = tk[k][i % len(tk[k])][:int(dur * SR)] * 10 ** ((db - x[k]) / 20.0)
            s = s.copy(); f = min(len(s), 2400); s[-f:] *= np.linspace(1, 0, f)
        a = int(t0 * SR); s = s[:len(y) - a]
        y[a:a + len(s), 0] += s * np.sqrt(0.5 * (1 - pan)); y[a:a + len(s), 1] += s * np.sqrt(0.5 * (1 + pan))
    return y
