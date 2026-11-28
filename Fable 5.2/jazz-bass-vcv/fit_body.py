"""Fit the body: every partial of every measured note says how loud the instrument is at that frequency,
once the string model's own level is taken off. Solve for one body curve (1/6-octave bins) shared by all notes."""
import json, numpy as np, itertools
import upright as U
M = json.load(open('ref_measurements.json'))
VL = {'vl1': 0.3, 'vl2': 0.6, 'vl4': 0.95}
NAMEMIDI = {'c': 0, 'db': 1, 'd': 2, 'eb': 3, 'e': 4, 'f': 5, 'gb': 6, 'g': 7, 'ab': 8, 'a': 9, 'bb': 10, 'b': 11}
EDGES = 30.0 * 2 ** (np.arange(0, 37) / 6.0)
def string_db(f0, vel, p, nh=10):
    lo, hi, ex, key = p
    k = np.arange(1, nh + 1); fk = k * f0
    fc = lo * (hi / lo) ** (vel ** ex) * (f0 / 65.0) ** key
    A = np.abs(np.sin(k * np.pi * 0.17 * 1.04)) / k * np.exp(-fk / fc)
    sig = 5.0 + fk * (0.03 + 0.05 * min(1.0, max(0.0, (f0 - 40.0) / 110.0)))
    return 20 * np.log10(A + 1e-9) - sig * 0.18
def rows(p):
    R = []
    for i, (name, m) in enumerate(sorted(M.items())):
        parts = name.split('_')
        if len(parts) != 3 or parts[1] not in VL or 'amps_db' not in m: continue
        f0 = m['f0']
        if not (28 < f0 < 280): continue
        real = np.array(m['amps_db'][:10]); s = string_db(f0, VL[parts[1]], p, len(real))
        for k in range(len(real)):
            f = (k + 1) * f0
            if real[k] > -45 and f < EDGES[-1]: R.append((i, int(np.searchsorted(EDGES, f)) - 1, real[k] - s[k], f))
    return R
def solve(p):
    R = rows(p); ids = sorted(set(r[0] for r in R)); c = {i: 0.0 for i in ids}; B = np.zeros(len(EDGES))
    for _ in range(30):
        for b in range(len(EDGES)):
            v = [r[2] - c[r[0]] for r in R if r[1] == b]
            if len(v) >= 2: B[b] = np.median(v)
        Bs = np.convolve(np.pad(B, 1, mode='edge'), [0.2, 0.6, 0.2], 'valid')
        for i in ids: c[i] = np.mean([r[2] - Bs[r[1]] for r in R if r[0] == i])
    e = np.mean([abs(r[2] - c[r[0]] - Bs[r[1]]) for r in R])
    return e, Bs, len(R)
if __name__ == '__main__':
    best = None
    for p in itertools.product((70, 110, 160, 240), (600, 900, 1400, 2200), (0.7, 1.0, 1.4), (0.0, 0.3, 0.6, 0.9)):
        e, B, n = solve(p)
        if best is None or e < best[0]: best = (e, p, B, n)
    e, p, B, n = best
    print('best string parameters', p, 'mean error %.1f dB over %d partials' % (e, n))
    e0, _, _ = solve((110, 900, 0.9, 0.5)); print('before', round(e0, 1))
    B = B - B.max()
    for f, b in zip(EDGES, B): print('%7.1f Hz %6.1f dB %s' % (f * 2 ** (1 / 12.0), b, '#' * int(max(0, 40 + b))))
    json.dump(dict(params=p, centres=list(EDGES * 2 ** (1 / 12.0)), body_db=list(B), err=e), open('body_fit.json', 'w'))
